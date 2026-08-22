#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace tapemxa
{

// FIG. 2 - Le transport dessine comme dans le manuel, vu de face : les deux
// bobines A LEURS ANGLES REELS (la receptrice tourne plus vite, elles
// accelerent avec la vitesse choisie), la bande passant sur les tetes REC et
// PLAY, le compteur qui defile, la chaine de lecture avec les vraies valeurs
// imprimees, les rails dry/wet ponderes par le mix. La quantite est dessinee
// en geometrie : le schema est la valeur.
class SchematicDiagram : public juce::Component
{
public:
    explicit SchematicDiagram (TapeProcessor&);

    void paint (juce::Graphics&) override;

private:
    TapeProcessor& processor;

    std::atomic<float>* sat   = nullptr;
    std::atomic<float>* speed = nullptr;
    std::atomic<float>* hiss  = nullptr;
    std::atomic<float>* mix   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SchematicDiagram)
};

}
