#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TapeEngine.h"

namespace tapemxa
{

class TapeProcessor : public juce::AudioProcessor
{
public:
    TapeProcessor();
    ~TapeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TapeMXA"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    // Phases, bobines et compteur reels pour l'affichage temps reel.
    float getWowPhase01() const noexcept     { return engine.getWowPhase01(); }
    float getFlutterPhase01() const noexcept { return engine.getFlutterPhase01(); }
    float getSupplyPhase01() const noexcept  { return engine.getSupplyPhase01(); }
    float getTakeupPhase01() const noexcept  { return engine.getTakeupPhase01(); }
    float getCounterSeconds() const noexcept { return engine.getCounterSeconds(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushParameterUpdatesToEngine();

    juce::AudioProcessorValueTreeState apvts;
    TapeEngine engine;

    // Copie du signal sec, pre-allouee dans prepareToPlay : processBlock ne
    // doit jamais allouer sur le thread audio.
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeProcessor)
};

}
