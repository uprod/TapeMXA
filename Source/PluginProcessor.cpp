#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace tapemxa
{

namespace IDs
{
    constexpr auto wow     = "wow";
    constexpr auto flutter = "flutter";
    constexpr auto sat     = "sat";
    constexpr auto speed   = "speed";
    constexpr auto hiss    = "hiss";
    constexpr auto mix     = "mix";
}

juce::AudioProcessorValueTreeState::ParameterLayout TapeProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Affichages "tally" a la machine a ecrire, aussi bien dans l'editeur que
    // dans les lignes d'automation de l'hote.
    const auto pctAttr = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; });

    params.push_back (std::make_unique<P> (juce::ParameterID { IDs::wow, 1 },
        "Wow", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f, pctAttr));

    params.push_back (std::make_unique<P> (juce::ParameterID { IDs::flutter, 1 },
        "Flutter", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f, pctAttr));

    params.push_back (std::make_unique<P> (juce::ParameterID { IDs::sat, 1 },
        "Saturation", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f, pctAttr));

    // Vitesse de defilement : plus la bande est rapide, plus le son est propre.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { IDs::speed, 1 }, "Speed",
        juce::StringArray { "7.5 ips", "15 ips", "30 ips" }, 1));

    params.push_back (std::make_unique<P> (juce::ParameterID { IDs::hiss, 1 },
        "Hiss", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f, pctAttr));

    params.push_back (std::make_unique<P> (juce::ParameterID { IDs::mix, 1 },
        "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, pctAttr));

    return { params.begin(), params.end() };
}

TapeProcessor::TapeProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

TapeProcessor::~TapeProcessor() = default;

void TapeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    engine.reset();

    dryBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock);
}

void TapeProcessor::releaseResources()
{
    engine.reset();
}

bool TapeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == main;
}

void TapeProcessor::pushParameterUpdatesToEngine()
{
    engine.setWow        (apvts.getRawParameterValue (IDs::wow)->load());
    engine.setFlutter    (apvts.getRawParameterValue (IDs::flutter)->load());
    engine.setSaturation (apvts.getRawParameterValue (IDs::sat)->load());
    engine.setSpeedIndex (juce::roundToInt (apvts.getRawParameterValue (IDs::speed)->load()));
    engine.setHiss       (apvts.getRawParameterValue (IDs::hiss)->load());
}

void TapeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    pushParameterUpdatesToEngine();

    // On garde une copie du son sec pour le mix dry/wet.
    dryBuffer.makeCopyOf (buffer, true);

    engine.process (buffer);   // 'buffer' contient maintenant le signal "bande" (wet)

    const float wetAmt = apvts.getRawParameterValue (IDs::mix)->load();
    const float dryAmt = 1.0f - wetAmt;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dryIn = dryBuffer.getReadPointer (juce::jmin (ch, dryBuffer.getNumChannels() - 1));

        for (int n = 0; n < buffer.getNumSamples(); ++n)
            wet[n] = dryAmt * dryIn[n] + wetAmt * wet[n];
    }
}

juce::AudioProcessorEditor* TapeProcessor::createEditor()
{
    return new TapeEditor (*this);
}

void TapeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TapeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

}

// Point d'entree du plugin JUCE — doit etre au niveau global.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new tapemxa::TapeProcessor();
}
