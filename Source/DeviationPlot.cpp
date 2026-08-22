#include "DeviationPlot.h"
#include "ManualStyle.h"
#include "TapeEngine.h"

namespace tapemxa
{

namespace
{
    constexpr int   kPoints  = 300;   // le flutter demande de la resolution
    constexpr float kWindowS = 2.5f;  // deux periodes de wow

    // Echelle verticale adaptative : le plus petit palier "propre" qui
    // contient la deviation crete reelle.
    float centsLimitFor (float peakCents)
    {
        static const float ladder[] = { 2.0f, 5.0f, 10.0f, 25.0f, 50.0f, 100.0f };
        for (const float l : ladder)
            if (peakCents <= l)
                return l;
        return 100.0f;
    }

    juce::String centsLabel (float v)
    {
        const juce::String sign = v > 0.0f ? "+" : "";
        if (std::abs (v - std::round (v)) < 0.01f)
            return sign + juce::String ((int) std::round (v));
        return sign + juce::String (v, 1);
    }
}

DeviationPlot::DeviationPlot (TapeProcessor& proc)
    : processor (proc)
{
    auto& apvts = processor.getAPVTS();
    wow     = apvts.getRawParameterValue ("wow");
    flutter = apvts.getRawParameterValue ("flutter");
    speed   = apvts.getRawParameterValue ("speed");

    setInterceptsMouseClicks (false, false);
}

void DeviationPlot::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    auto caption = full.removeFromBottom (16.0f);
    auto box = full;

    const float wowV = wow->load();
    const float flV  = flutter->load();
    const int   spdV = juce::roundToInt (speed->load());

    const float phW = processor.getWowPhase01();
    const float phF = processor.getFlutterPhase01();

    // Deviation crete reelle : les pentes max wow et flutter s'additionnent.
    const float tw = juce::MathConstants<float>::twoPi;
    const float kMax =
          TapeEngine::wowDepthMsFor (wowV, spdV) * 0.001f * tw * TapeEngine::kWowHz
        + TapeEngine::flutterDepthMsFor (flV, spdV) * 0.001f * tw * TapeEngine::kFlutterHz;
    const float peakCents = juce::jmax (
        std::abs (1200.0f * std::log2 (juce::jmax (1.0f - kMax, 0.05f))),
        1200.0f * std::log2 (1.0f + kMax));
    const float limit = centsLimitFor (juce::jmax (0.5f, peakCents * 1.05f));

    auto yForCents = [&] (float c)
    {
        return box.getCentreY() - c / limit * (box.getHeight() * 0.5f - 6.0f);
    };
    auto xForTimeBack = [&] (float tBack)
    {
        return box.getRight() - 1.0f - tBack / kWindowS * (box.getWidth() - 2.0f);
    };

    // --- Grille ------------------------------------------------------------
    for (int i = 1; i <= 4; ++i)   // verticales chaque demi-seconde
    {
        g.setColour (palette::inkFaint);
        g.drawVerticalLine ((int) xForTimeBack ((float) i * 0.5f),
                            box.getY() + 1.0f, box.getBottom() - 1.0f);
    }
    static const float levels[] = { 1.0f, 0.5f, 0.0f, -0.5f, -1.0f };
    for (const float lv : levels)
    {
        g.setColour (lv == 0.0f ? palette::inkMid.withAlpha (0.65f) : palette::inkFaint);
        g.drawHorizontalLine ((int) yForCents (lv * limit), box.getX() + 1.0f, box.getRight() - 1.0f);
    }

    // --- Trace : dev(t) reconstruite a rebours des phases reelles --------------
    {
        juce::Path p;
        for (int i = 0; i < kPoints; ++i)
        {
            const float tBack = kWindowS * (1.0f - (float) i / (float) (kPoints - 1));
            const float dev = TapeEngine::deviationCents (
                wowV, flV, spdV,
                phW - tBack * TapeEngine::kWowHz,
                phF - tBack * TapeEngine::kFlutterHz);
            const float px = xForTimeBack (tBack);
            const float py = juce::jlimit (box.getY() + 1.0f, box.getBottom() - 1.0f,
                                           yForCents (dev));
            if (i == 0) p.startNewSubPath (px, py);
            else        p.lineTo (px, py);
        }
        g.setColour (palette::spot);
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
    }

    // Repere de l'instant present : index vertical au bord droit du cadre.
    {
        const float sx = box.getRight() - 5.0f;
        g.setColour (palette::spot.withAlpha (0.30f));
        g.drawVerticalLine ((int) sx, box.getY() + 1.0f, box.getBottom() - 1.0f);

        juce::Path idx;   // petit index triangulaire en haut
        idx.addTriangle (sx - 3.5f, box.getY() + 1.0f, sx + 3.5f, box.getY() + 1.0f, sx, box.getY() + 7.0f);
        g.setColour (palette::spot);
        g.fillPath (idx);

        const float devNow = TapeEngine::deviationCents (wowV, flV, spdV, phW, phF);
        g.fillEllipse (sx - 2.2f, yForCents (devNow) - 2.2f, 4.4f, 4.4f);
    }

    // --- Echelles ----------------------------------------------------------
    // Chaque chiffre est pose sur un cartouche film : ni le grain ni la grille
    // ne peuvent corrompre une valeur que le regard doit pouvoir croire.
    auto drawFigure = [&g] (const juce::String& text, juce::Point<float> anchor,
                            juce::Justification just)
    {
        const auto font = fonts::mono (9.0f);
        const float tw2 = juce::GlyphArrangement::getStringWidth (font, text);
        auto area = juce::Rectangle<float> (tw2 + 6.0f, 11.0f).withCentre (anchor);
        if (just.testFlags (juce::Justification::left))
            area.setX (anchor.x - 3.0f);
        else if (just.testFlags (juce::Justification::right))
            area.setX (anchor.x - tw2 - 3.0f);

        g.setColour (palette::film);
        g.fillRect (area);
        g.setFont (font);
        g.setColour (palette::inkMid);
        g.drawText (text, area, juce::Justification::centred);
    };

    for (int i = 1; i <= 4; ++i)
        drawFigure ("-" + juce::String ((float) i * 0.5f, 1),
                    { xForTimeBack ((float) i * 0.5f), box.getBottom() - 8.0f },
                    juce::Justification::centred);
    drawFigure ("s", { box.getRight() - 6.0f, box.getBottom() - 8.0f }, juce::Justification::right);

    drawFigure (centsLabel (limit) + " ct",
                { box.getX() + 6.0f, yForCents (limit) + 7.0f }, juce::Justification::left);
    drawFigure (centsLabel (0.5f * limit),
                { box.getX() + 6.0f, yForCents (0.5f * limit) - 6.0f }, juce::Justification::left);
    drawFigure ("0", { box.getX() + 6.0f, yForCents (0.0f) - 6.0f }, juce::Justification::left);
    drawFigure (centsLabel (-0.5f * limit),
                { box.getX() + 6.0f, yForCents (-0.5f * limit) - 6.0f }, juce::Justification::left);
    drawFigure (centsLabel (-limit),
                { box.getX() + 6.0f, yForCents (-limit) - 6.0f }, juce::Justification::left);

    // Tallies : deviation courante et vitesse de defilement.
    {
        const float devNow = TapeEngine::deviationCents (wowV, flV, spdV, phW, phF);
        auto tally = juce::Rectangle<float> (120.0f, 12.0f)
                         .withPosition (box.getRight() - 126.0f, box.getY() + 6.0f);
        g.setColour (palette::film);
        g.fillRect (tally.expanded (3.0f, 1.0f));
        g.setFont (fonts::mono (10.0f));
        g.setColour (palette::inkMid);
        g.drawText ("dev", tally.removeFromLeft (30.0f), juce::Justification::centredLeft);
        g.setColour (palette::ink);
        g.drawText ((devNow >= 0.0f ? "+" : "") + juce::String (devNow, 1) + " ct",
                    tally, juce::Justification::centredLeft);

        static const char* ipsNames[] = { "7.5 ips", "15 ips", "30 ips" };
        auto tally2 = juce::Rectangle<float> (120.0f, 12.0f)
                          .withPosition (box.getRight() - 126.0f, box.getY() + 20.0f);
        g.setColour (palette::film);
        g.fillRect (tally2.expanded (3.0f, 1.0f));
        g.setFont (fonts::mono (10.0f));
        g.setColour (palette::inkMid);
        g.drawText ("spd", tally2.removeFromLeft (30.0f), juce::Justification::centredLeft);
        g.setColour (palette::ink);
        g.drawText (ipsNames[juce::jlimit (0, 2, spdV)], tally2, juce::Justification::centredLeft);
    }

    // --- Cadre + legende de figure ------------------------------------------
    g.setColour (palette::ink);
    g.drawRect (box, 1.0f);

    const juce::String cap = "FIG. 1 - TAPE SPEED DEVIATION, WOW + FLUTTER";
    g.setFont (fonts::lettering (10.0f));
    g.setColour (palette::inkMid);
    g.drawText (cap, caption.withTrimmedTop (4.0f), juce::Justification::bottomLeft);

    const float capW = juce::GlyphArrangement::getStringWidth (fonts::lettering (10.0f), cap);
    g.setColour (palette::inkFaint);
    g.drawHorizontalLine ((int) (caption.getBottom() - 3.0f),
                          caption.getX() + capW + 10.0f, caption.getRight());
}

}
