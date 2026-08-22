#include "SchematicDiagram.h"
#include "ManualStyle.h"
#include "TapeEngine.h"

namespace tapemxa
{

namespace
{
    // Epaisseur de trait proportionnelle a une quantite 0..1 : la geometrie
    // porte la valeur, jamais la couleur seule.
    float weightFor (float amount01)
    {
        return 0.7f + 2.4f * juce::jlimit (0.0f, 1.0f, amount01);
    }

    void drawDashedLine (juce::Graphics& g, juce::Line<float> line, float thickness)
    {
        const float dashes[] = { 3.0f, 3.0f };
        g.drawDashedLine (line, dashes, 2, thickness);
    }

    // Etiquette imprimee qui interrompt le trait qu'elle chevauche.
    void drawLabelOverLine (juce::Graphics& g, const juce::String& text,
                            juce::Rectangle<float> area, juce::Justification just)
    {
        const float tw = juce::GlyphArrangement::getStringWidth (fonts::lettering (9.0f), text);
        auto knockout = area.withSizeKeepingCentre (tw + 10.0f, area.getHeight());
        g.setColour (palette::film);
        g.fillRect (knockout);
        g.setFont (fonts::lettering (9.0f));
        g.setColour (palette::inkMid);
        g.drawText (text, area, just);
    }

    // Croix de sommateur dans un cercle (jonction "+" du schema).
    void drawSummingNode (juce::Graphics& g, juce::Point<float> c, float r)
    {
        g.setColour (palette::film);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (palette::ink);
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.1f);
        g.drawLine (c.x - r * 0.5f, c.y, c.x + r * 0.5f, c.y, 1.0f);
        g.drawLine (c.x, c.y - r * 0.5f, c.x, c.y + r * 0.5f, 1.0f);
    }

    // Rail pondere : epaisseur = quantite ; a zero il degenere en tirete fin.
    void drawWeightedLine (juce::Graphics& g, juce::Line<float> line, float amount01)
    {
        if (amount01 < 0.005f)
            drawDashedLine (g, line, 0.7f);
        else
            g.drawLine (line, weightFor (amount01));
    }

    // Bobine vue de face : flasque, trois rayons a l'angle reel, moyeu.
    void drawReel (juce::Graphics& g, juce::Point<float> c, float r, float phase01)
    {
        g.setColour (palette::film);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (palette::ink);
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.2f);

        const float tw = juce::MathConstants<float>::twoPi;
        g.setColour (palette::spot);
        for (int i = 0; i < 3; ++i)
        {
            const float a = (phase01 + (float) i / 3.0f) * tw;
            const juce::Point<float> dir (std::sin (a), -std::cos (a));
            g.drawLine ({ c + dir * 3.0f, c + dir * (r - 2.5f) }, 1.4f);
        }
        g.setColour (palette::ink);
        g.fillEllipse (c.x - 2.2f, c.y - 2.2f, 4.4f, 4.4f);
    }
}

SchematicDiagram::SchematicDiagram (TapeProcessor& proc)
    : processor (proc)
{
    auto& apvts = processor.getAPVTS();
    sat   = apvts.getRawParameterValue ("sat");
    speed = apvts.getRawParameterValue ("speed");
    hiss  = apvts.getRawParameterValue ("hiss");
    mix   = apvts.getRawParameterValue ("mix");

    setInterceptsMouseClicks (false, false);
}

void SchematicDiagram::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    auto caption  = getLocalBounds().toFloat().removeFromBottom (16.0f);

    const float satV  = sat->load();
    const int   spdV  = juce::roundToInt (speed->load());
    const float hissV = hiss->load();
    const float mixV  = mix->load();

    // Rangs horizontaux du schema : le rail dry passe au-dessus du transport.
    const float dryY  = 12.0f;
    const float railY = 40.0f;
    const float tapeY = 60.0f;   // trajet de la bande sur les tetes

    // Colonnes.
    const float inX     = 12.0f;
    const float branchX = 36.0f;
    const float supX    = 165.0f;   // bobine debitrice
    const float takX    = 355.0f;   // bobine receptrice
    const float reelR   = 15.0f;
    const float pbX0    = 450.0f, pbX1 = 590.0f;   // chaine de lecture
    const float mixX    = w * 0.86f;
    const float outX    = w - 16.0f;

    // --- Rail d'entree et derivation dry ------------------------------------
    g.setColour (palette::ink);
    g.drawEllipse (inX - 3.0f, railY - 3.0f, 6.0f, 6.0f, 1.1f);              // borne IN
    g.fillEllipse (branchX - 2.2f, railY - 2.2f, 4.4f, 4.4f);                // noeud de derivation

    g.setColour (palette::ink.withAlpha (0.9f));
    drawWeightedLine (g, { { branchX, railY }, { branchX, dryY } }, (1.0f - mixV) * 0.75f);
    drawWeightedLine (g, { { branchX, dryY }, { mixX, dryY } }, 1.0f - mixV);
    drawWeightedLine (g, { { mixX, dryY }, { mixX, railY - 9.0f } }, 1.0f - mixV);

    g.setFont (fonts::lettering (9.0f));
    g.setColour (palette::inkMid);
    g.drawText ("IN", juce::Rectangle<float> (24.0f, 10.0f).withPosition (inX - 8.0f, railY - 17.0f),
                juce::Justification::centredLeft);

    drawLabelOverLine (g, "DRY PATH",
                       juce::Rectangle<float> (80.0f, 10.0f).withCentre ({ (supX + takX) * 0.5f, dryY }),
                       juce::Justification::centred);

    // Le signal entre dans la tete d'enregistrement, sur le trajet de bande.
    g.setColour (palette::ink);
    g.drawLine (inX + 3.0f, railY, branchX + 40.0f, railY, 1.2f);
    g.drawLine (branchX + 40.0f, railY, branchX + 40.0f, tapeY, 1.1f);
    g.drawLine (branchX + 40.0f, tapeY, 235.0f, tapeY, 1.1f);

    // --- Le transport : bobines aux angles reels, bande, tetes ----------------
    drawReel (g, { supX, railY - 4.0f }, reelR, processor.getSupplyPhase01());
    drawReel (g, { takX, railY - 4.0f }, reelR, processor.getTakeupPhase01());

    // La bande descend des bobines et passe sur les tetes.
    g.setColour (palette::ink.withAlpha (0.85f));
    g.drawLine (supX - reelR + 3.0f, railY + 6.0f, supX - reelR + 3.0f, tapeY, 0.9f);
    g.drawLine (supX - reelR + 3.0f, tapeY, takX + reelR - 3.0f, tapeY, 0.9f);
    g.drawLine (takX + reelR - 3.0f, railY + 6.0f, takX + reelR - 3.0f, tapeY, 0.9f);

    // Tetes REC et PLAY sur le trajet.
    for (int i = 0; i < 2; ++i)
    {
        const float hx = i == 0 ? 240.0f : 272.0f;
        const juce::Rectangle<float> head (hx, tapeY - 8.0f, 14.0f, 10.0f);
        g.setColour (palette::film);
        g.fillRect (head);
        g.setColour (palette::ink);
        g.drawRect (head, 1.1f);
        g.setFont (fonts::mono (7.0f));
        g.drawText (i == 0 ? "R" : "P", head, juce::Justification::centred);
    }

    // Le compteur reel, entre les bobines.
    {
        const int count = ((int) processor.getCounterSeconds()) % 10000;
        juce::String digits (count);
        while (digits.length() < 4)
            digits = "0" + digits;

        const juce::Rectangle<float> counter (238.0f, 22.0f, 44.0f, 13.0f);
        g.setColour (palette::film);
        g.fillRect (counter);
        g.setColour (palette::inkMid);
        g.drawRect (counter, 0.8f);
        g.setFont (fonts::mono (10.0f));
        g.setColour (palette::ink);
        g.drawText (digits, counter, juce::Justification::centred);
    }

    // --- La chaine de lecture : les valeurs imprimees sont les vraies ---------
    g.setColour (palette::ink);
    g.drawLine (286.0f, tapeY, pbX0 - 40.0f, tapeY, 1.1f);
    g.drawLine (pbX0 - 40.0f, tapeY, pbX0 - 40.0f, railY, 1.1f);
    g.drawLine (pbX0 - 40.0f, railY, pbX0, railY, 1.2f);

    {
        const juce::Rectangle<float> block (pbX0, railY - 14.0f, pbX1 - pbX0, 28.0f);
        g.setColour (palette::film);
        g.fillRect (block);
        g.setColour (palette::ink);
        g.drawRect (block, 1.2f);

        g.setFont (fonts::lettering (10.0f));
        g.drawText ("PLAYBACK", block.withTrimmedBottom (10.0f), juce::Justification::centred);

        const juce::String sub = "SAT x" + juce::String (1.0f + 5.0f * satV, 1)
            + "  LP " + juce::String (TapeEngine::hfCutoffFor (spdV) / 1000.0f, 0) + "k"
            + "  HISS " + juce::String ((int) TapeEngine::hissDbFor (hissV)) + " dB";
        g.setFont (fonts::mono (8.0f));
        g.setColour (palette::inkMid);
        g.drawText (sub, block.withTrimmedTop (13.0f), juce::Justification::centred);
    }

    // --- Rail wet vers le sommateur de mix ------------------------------------
    g.setColour (palette::ink.withAlpha (0.9f));
    drawWeightedLine (g, { { pbX1, railY }, { mixX - 9.0f, railY } }, mixV);
    g.setFont (fonts::lettering (9.0f));
    g.setColour (palette::inkMid);
    g.drawText ("WET", juce::Rectangle<float> (30.0f, 10.0f).withPosition (mixX - 52.0f, railY - 14.0f),
                juce::Justification::centredRight);

    drawSummingNode (g, { mixX, railY }, 8.0f);
    g.drawText ("MIX", juce::Rectangle<float> (30.0f, 10.0f).withPosition (mixX + 12.0f, railY - 20.0f),
                juce::Justification::centredLeft);

    // --- Sortie ------------------------------------------------------------------
    g.setColour (palette::ink);
    g.drawLine (mixX + 8.0f, railY, outX - 3.0f, railY, 1.4f);
    g.fillEllipse (outX - 3.0f, railY - 3.0f, 6.0f, 6.0f);                   // borne OUT
    g.setColour (palette::inkMid);
    g.drawText ("OUT", juce::Rectangle<float> (28.0f, 10.0f).withPosition (outX - 24.0f, railY - 17.0f),
                juce::Justification::centredRight);

    // --- Legende de figure ----------------------------------------------------
    static const char* ipsNames[] = { "7.5", "15", "30" };
    const juce::String cap = juce::String ("FIG. 2 - TAPE TRANSPORT, ")
        + ipsNames[juce::jlimit (0, 2, spdV)] + " IPS, LIVE REELS";
    g.setFont (fonts::lettering (10.0f));
    g.setColour (palette::inkMid);
    g.drawText (cap, caption.withTrimmedTop (4.0f), juce::Justification::bottomLeft);

    const float capW = juce::GlyphArrangement::getStringWidth (fonts::lettering (10.0f), cap);
    g.setColour (palette::inkFaint);
    g.drawHorizontalLine ((int) (caption.getBottom() - 3.0f),
                          caption.getX() + capW + 10.0f, caption.getRight());
}

}
