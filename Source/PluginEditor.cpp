#include "PluginEditor.h"

namespace tapemxa
{

namespace
{
    constexpr int kWidth  = 820;
    constexpr int kHeight = 470;
    constexpr int kMargin = 24;   // marge interieure de la feuille
}

TapeEditor::TapeEditor (TapeProcessor& proc)
    : juce::AudioProcessorEditor (&proc),
      processor (proc),
      plot (proc),
      schematic (proc)
{
    setLookAndFeel (&lookAndFeel);
    setSize (kWidth, kHeight);

    filmTexture = makeFilmTexture (kWidth, kHeight);

    addAndMakeVisible (plot);
    addAndMakeVisible (schematic);

    setupDial (wowDial,     "WOW",     "wow");
    setupDial (flutterDial, "FLUTTER", "flutter");
    setupDial (satDial,     "SAT",     "sat");
    setupDial (hissDial,    "HISS",    "hiss");
    setupDial (mixDial,     "MIX",     "mix");

    // Commutateur SPEED : 3 crans (7.5 / 15 / 30 IPS), dessine par le LookAndFeel.
    {
        auto& s = speedSwitch.slider;
        s.setComponentID ("switch");
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.65f,
                               juce::MathConstants<float>::pi * 2.35f, true);

        // Course de glisse courte : au reglage JUCE par defaut (250 px pour la
        // course complete), chaque cran demandait une longue glisse et le
        // commutateur semblait bloque. Le clic sec avance d'un cran (mouseUp).
        s.setMouseDragSensitivity (100);
        s.addMouseListener (this, false);
        addAndMakeVisible (s);

        auto& l = speedSwitch.name;
        l.setText ("SPEED", juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (fonts::lettering (11.5f));
        l.setColour (juce::Label::textColourId, palette::ink);
        addAndMakeVisible (l);

        speedSwitch.attachment = std::make_unique<SAttach> (processor.getAPVTS(), "speed", s);
    }

    startTimerHz (30);   // pilote FIG. 1, les bobines et le compteur

}

TapeEditor::~TapeEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TapeEditor::mouseUp (const juce::MouseEvent& e)
{
    // Un clic sec (sans glisse) avance le commutateur d'un cran, en boucle.
    auto& s = speedSwitch.slider;
    if (e.eventComponent == &s && ! e.mouseWasDraggedSinceMouseDown())
        s.setValue ((double) ((juce::roundToInt (s.getValue()) + 1) % 3),
                    juce::sendNotificationSync);
}

void TapeEditor::setupDial (Dial& d, const juce::String& labelText, const juce::String& paramID)
{
    auto& s = d.slider;
    // Drag vertical uniquement : en mode Horizontal+Vertical, JUCE somme
    // dx + (-dy), donc un geste en diagonale bas-droite / haut-gauche
    // s'annule et le cadran semble mort par intermittence.
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 16);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                           juce::MathConstants<float>::pi * 2.75f, true);

    // Le format des valeurs ("35 %") vient des parametres eux-memes.
    s.setColour (juce::Slider::textBoxTextColourId,       palette::ink);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);

    addAndMakeVisible (s);

    auto& l = d.name;
    l.setText (labelText, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (fonts::lettering (11.5f));
    l.setColour (juce::Label::textColourId, palette::ink);
    addAndMakeVisible (l);

    d.attachment = std::make_unique<SAttach> (processor.getAPVTS(), paramID, s);
}

void TapeEditor::timerCallback()
{
    plot.repaint();
    schematic.repaint();
}

//==============================================================================

void TapeEditor::drawSheetFrame (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();

    // Double filet de feuille technique : trait fort dehors, filet dedans.
    g.setColour (palette::ink);
    g.drawRect (full.reduced (8.0f), 1.6f);
    g.setColour (palette::inkFaint);
    g.drawRect (full.reduced (14.0f), 0.7f);

    // Reperes de zone (A..E / 1..3), comme sur un plan.
    g.setFont (fonts::mono (8.0f));
    g.setColour (palette::inkMid.withAlpha (0.7f));

    static const char* cols[] = { "A", "B", "C", "D", "E" };
    for (int i = 0; i < 5; ++i)
    {
        const float cx = full.getX() + full.getWidth() * (0.5f + (float) i) / 5.0f;
        g.drawText (cols[i], juce::Rectangle<float> (12.0f, 8.0f).withCentre ({ cx, 5.0f }),
                    juce::Justification::centred);
        g.drawText (cols[i], juce::Rectangle<float> (12.0f, 8.0f).withCentre ({ cx, full.getBottom() - 5.0f }),
                    juce::Justification::centred);
    }
    static const char* rows[] = { "1", "2", "3" };
    for (int i = 0; i < 3; ++i)
    {
        const float cy = full.getY() + full.getHeight() * (0.5f + (float) i) / 3.0f;
        g.drawText (rows[i], juce::Rectangle<float> (8.0f, 10.0f).withCentre ({ 4.5f, cy }),
                    juce::Justification::centred);
        g.drawText (rows[i], juce::Rectangle<float> (8.0f, 10.0f).withCentre ({ full.getRight() - 4.5f, cy }),
                    juce::Justification::centred);
    }
}

void TapeEditor::drawHeader (juce::Graphics& g)
{
    auto header = juce::Rectangle<float> ((float) kMargin, (float) kMargin - 2.0f,
                                          (float) (kWidth - 2 * kMargin), 32.0f);

    g.setFont (fonts::wide (24.0f));
    g.setColour (palette::ink);
    g.drawText ("TAPE MXA", header.withWidth (200.0f), juce::Justification::centredLeft);

    g.setFont (fonts::lettering (10.5f));
    g.setColour (palette::inkMid);
    g.drawText ("REEL-TO-REEL TAPE MACHINE",
                header.withX (header.getX() + 208.0f).withWidth (220.0f),
                juce::Justification::centredLeft);

    // Cartouche (title block) en haut a droite, comme sur la feuille de plan.
    const auto block = header.removeFromRight (252.0f);
    g.setColour (palette::inkMid);
    g.drawRect (block, 0.8f);
    g.drawHorizontalLine ((int) block.getCentreY(), block.getX(), block.getRight());
    const float divX = block.getRight() - 78.0f;
    g.drawVerticalLine ((int) divX, block.getY(), block.getBottom());

    auto row1 = block.withHeight (block.getHeight() * 0.5f);
    auto row2 = row1.withY (block.getCentreY());

    g.setFont (fonts::wide (10.0f));
    g.setColour (palette::ink);
    g.drawText ("MXAudio", row1.withTrimmedLeft (8.0f).withTrimmedRight (86.0f),
                juce::Justification::centredLeft);

    g.setFont (fonts::mono (9.0f));
    g.setColour (palette::inkMid);
    g.drawText ("REV A", row1.withX (divX).withWidth (78.0f), juce::Justification::centred);
    g.drawText ("DWG NO. MXA-TP-01", row2.withTrimmedLeft (8.0f).withTrimmedRight (86.0f),
                juce::Justification::centredLeft);
    g.drawText ("SHEET 1/1", row2.withX (divX).withWidth (78.0f), juce::Justification::centred);
}

void TapeEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (filmTexture, 0, 0);
    drawSheetFrame (g);
    drawHeader (g);

    // Credit de marque, comme la signature d'imprimeur au bas d'un plan.
    // En bas a droite, en echo au cartouche.
    g.setFont (fonts::lettering (8.0f));
    g.setColour (palette::inkMid);
    g.drawText ("BY MESCALINA",
                juce::Rectangle<float> ((float) (kWidth - kMargin - 200), 447.0f, 200.0f, 9.0f),
                juce::Justification::centredRight);
}

void TapeEditor::resized()
{
    auto area = getLocalBounds().reduced (kMargin);

    area.removeFromTop (32);   // bandeau titre (dessine dans paint)
    area.removeFromTop (6);

    plot.setBounds (area.removeFromTop (168));
    area.removeFromTop (8);
    schematic.setBounds (area.removeFromTop (94));
    area.removeFromTop (4);

    // Rangee des commandes : 6 cellules, ordre du circuit.
    Dial* dials[] = { &wowDial, &flutterDial, &satDial, &speedSwitch, &hissDial, &mixDial };
    const int cellW = area.getWidth() / 6;
    int cx = area.getX();

    for (auto* d : dials)
    {
        auto cell = juce::Rectangle<int> (cx, area.getY(), cellW, area.getHeight());
        d->name.setBounds (cell.removeFromTop (14));

        // Le commutateur n'a pas de zone de texte : on retire la meme hauteur
        // pour que son cercle reste a l'echelle des cadrans.
        if (d == &speedSwitch)
            cell.removeFromBottom (16);

        d->slider.setBounds (cell.reduced (6, 0));
        cx += cellW;
    }
}

}
