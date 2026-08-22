#include "TapeEngine.h"

namespace tapemxa
{

TapeEngine::TapeEngine() = default;

void TapeEngine::prepare (double newSampleRate, int blockSize, int numChannels)
{
    sampleRate = newSampleRate;
    numCh = juce::jlimit (1, 2, numChannels);

    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) juce::jmax (1, blockSize),
                                  (juce::uint32) numCh };
    delayLine.prepare (spec);

    // Retard maximal = centre + wow max + flutter max, plus une marge.
    const float maxDelaySamples = (kCenterMs + 1.2f * 1.4f + 0.15f * 1.4f)
                                    * 0.001f * (float) sampleRate + 4.0f;
    delayLine.setMaximumDelayInSamples ((int) maxDelaySamples);

    bumpSpeedIdx = -1;
    reset();
}

void TapeEngine::updateBump()
{
    if (bumpSpeedIdx == speedIdx)
        return;
    bumpSpeedIdx = speedIdx;

    // Bosse de tete : +2 dB, Q 0.8, a la frequence propre de la vitesse.
    const float A  = std::pow (10.0f, 2.0f / 40.0f);
    const float w  = juce::MathConstants<float>::twoPi * headBumpHzFor (speedIdx)
                       / (float) sampleRate;
    const float cw = std::cos (w), sw = std::sin (w);
    const float al = sw / (2.0f * 0.8f);
    const float a0 = 1.0f + al / A;

    for (auto& b : bump)
    {
        b.b0 = (1.0f + al * A) / a0;
        b.b1 = -2.0f * cw / a0;
        b.b2 = (1.0f - al * A) / a0;
        b.a1 = -2.0f * cw / a0;
        b.a2 = (1.0f - al / A) / a0;
    }
}

void TapeEngine::reset()
{
    delayLine.reset();
    wowPhase = flPhase = 0.0f;
    supplyPhase = takeupPhase = 0.0f;
    counterSec = 0.0f;
    for (auto& b : bump)
        b.reset();
    lpState[0] = lpState[1] = 0.0f;
    updateBump();
}

void TapeEngine::setWow (float a)         { wow     = juce::jlimit (0.0f, 1.0f, a); }
void TapeEngine::setFlutter (float a)     { flutter = juce::jlimit (0.0f, 1.0f, a); }
void TapeEngine::setSaturation (float a)  { sat     = juce::jlimit (0.0f, 1.0f, a); }
void TapeEngine::setSpeedIndex (int idx)  { speedIdx = juce::jlimit (0, 2, idx); }
void TapeEngine::setHiss (float a)        { hiss    = juce::jlimit (0.0f, 1.0f, a); }

void TapeEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int chs        = juce::jmin (numCh, buffer.getNumChannels());

    updateBump();

    const float wowInc = kWowHz / (float) sampleRate;
    const float flInc  = kFlutterHz / (float) sampleRate;
    const float wowD   = wowDepthMsFor (wow, speedIdx) * 0.001f * (float) sampleRate;
    const float flD    = flutterDepthMsFor (flutter, speedIdx) * 0.001f * (float) sampleRate;
    const float center = kCenterMs * 0.001f * (float) sampleRate;

    // Saturation de bande : tanh normalise pour garder la crete a 1.
    const float g     = 1.0f + 5.0f * sat;
    const float gNorm = 1.0f / std::tanh (g);

    const float kLp = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                       * hfCutoffFor (speedIdx) / (float) sampleRate);
    const float hissLin = juce::Decibels::decibelsToGain (hissDbFor (hiss))
                            * (hiss > 0.001f ? 1.0f : 0.0f);

    const float reelInc   = reelHzFor (speedIdx) / (float) sampleRate;
    const float takeupInc = reelInc * 1.35f;   // pack plus petit -> plus rapide

    for (int n = 0; n < numSamples; ++n)
    {
        // Les deux canaux voyagent sur la meme bande : meme retard.
        const float tw = juce::MathConstants<float>::twoPi;
        float delaySamples = center
            + wowD * std::sin (wowPhase * tw)
            + flD  * std::sin (flPhase * tw);
        if (delaySamples < 1.0f) delaySamples = 1.0f;

        for (int ch = 0; ch < chs; ++ch)
        {
            const float in = buffer.getSample (ch, n);
            delayLine.pushSample (ch, in);
            float v = delayLine.popSample (ch, delaySamples);

            // Saturation, bosse de tete, perte d'aigus, souffle.
            v = std::tanh (g * v) * gNorm;
            v = bump[ch].process (v);
            lpState[ch] += kLp * (v - lpState[ch]);
            v = lpState[ch];

            noiseState[ch] = noiseState[ch] * 1664525u + 1013904223u;
            const float noise = ((float) (noiseState[ch] >> 9)
                                 * (1.0f / 8388608.0f)) - 1.0f;
            buffer.setSample (ch, n, v + hissLin * noise);
        }

        wowPhase += wowInc;    if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flPhase  += flInc;     if (flPhase >= 1.0f)  flPhase -= 1.0f;
        supplyPhase += reelInc;   supplyPhase -= std::floor (supplyPhase);
        takeupPhase += takeupInc; takeupPhase -= std::floor (takeupPhase);
        counterSec  += 1.0f / (float) sampleRate;
    }

    uiWowPhase.store (wowPhase, std::memory_order_relaxed);   // pour FIG. 1 / FIG. 2
    uiFlPhase.store (flPhase, std::memory_order_relaxed);
    uiSupply.store (supplyPhase, std::memory_order_relaxed);
    uiTakeup.store (takeupPhase, std::memory_order_relaxed);
    uiCounter.store (counterSec, std::memory_order_relaxed);
}

}
