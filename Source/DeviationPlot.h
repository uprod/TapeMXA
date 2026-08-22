#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace tapemxa
{

// FIG. 1 - Deviation de vitesse de la bande, tracee comme la figure d'un
// manuel technique. Ce n'est pas une illustration : la courbe est la vraie
// deviation combinee wow + flutter (rapport de hauteur = 1 - d(retard)/dt,
// l'algebre de TapeEngine) evaluee aux phases reelles du moteur, en mode
// "defilement" : le bord droit du cadre est l'instant present — l'onde lente
// du wow porte l'ondulation rapide du flutter. Le repaint est pilote par le
// Timer de l'editeur (~30 Hz).
class DeviationPlot : public juce::Component
{
public:
    explicit DeviationPlot (TapeProcessor&);

    void paint (juce::Graphics&) override;

private:
    TapeProcessor& processor;

    std::atomic<float>* wow     = nullptr;
    std::atomic<float>* flutter = nullptr;
    std::atomic<float>* speed   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviationPlot)
};

}
