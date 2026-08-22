#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace tapemxa
{

// Coeur DSP du magnetophone. Le principe : la bande ne defile jamais a
// vitesse parfaitement constante — le WOW (ondulation lente du cabestan) et
// le FLUTTER (tremblement rapide des guides) desaccordent legerement le son,
// simules par une ligne a retard modulee. S'y ajoutent la saturation douce de
// la bande (tanh), la bosse de tete dans le grave, la perte d'aigus et le
// souffle — tous dependants de la vitesse de defilement (7.5 / 15 / 30 IPS) :
// plus la bande est rapide, plus le son est propre.
class TapeEngine
{
public:
    static constexpr float kCenterMs   = 5.0f;   // retard central du transport
    static constexpr float kWowHz      = 0.8f;   // ondulation lente
    static constexpr float kFlutterHz  = 9.3f;   // tremblement rapide

    TapeEngine();

    void prepare (double sampleRate, int blockSize, int numChannels);
    void reset();

    void setWow (float amount01);
    void setFlutter (float amount01);
    void setSaturation (float amount01);
    void setSpeedIndex (int idx);          // 0 = 7.5, 1 = 15, 2 = 30 IPS
    void setHiss (float amount01);

    // Traite le buffer en place ; le remplace par le signal "bande" (wet).
    // Le mix sec/traite est fait dans le processeur, comme pour les freres MXA.
    void process (juce::AudioBuffer<float>& buffer);

    // --- Verites partagees avec l'UI (FIG. 1 / FIG. 2) ----------------------
    // Une bande lente bouge plus : facteur applique aux profondeurs wow/flutter.
    static float speedScaleFor (int idx) noexcept
    {
        static constexpr float s[3] = { 1.4f, 1.0f, 0.7f };
        return s[juce::jlimit (0, 2, idx)];
    }

    static float hfCutoffFor (int idx) noexcept
    {
        static constexpr float f[3] = { 9000.0f, 14000.0f, 19000.0f };
        return f[juce::jlimit (0, 2, idx)];
    }

    static float headBumpHzFor (int idx) noexcept
    {
        static constexpr float f[3] = { 45.0f, 60.0f, 90.0f };
        return f[juce::jlimit (0, 2, idx)];
    }

    // Vitesse visuelle des bobines (tours/s) ; la receptrice tourne x1.35.
    static float reelHzFor (int idx) noexcept
    {
        static constexpr float f[3] = { 0.45f, 0.9f, 1.8f };
        return f[juce::jlimit (0, 2, idx)];
    }

    static float wowDepthMsFor (float wow01, int idx) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, wow01) * 1.2f * speedScaleFor (idx);
    }

    static float flutterDepthMsFor (float fl01, int idx) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, fl01) * 0.15f * speedScaleFor (idx);
    }

    // Niveau de souffle en dB (-80 .. -40).
    static float hissDbFor (float hiss01) noexcept
    {
        return -80.0f + 40.0f * juce::jlimit (0.0f, 1.0f, hiss01);
    }

    // Deviation de vitesse instantanee (cents) : rapport de hauteur =
    // 1 - d(retard)/dt, wow et flutter combines. La courbe de FIG. 1.
    static float deviationCents (float wow01, float fl01, int idx,
                                 float phaseWow01, float phaseFl01) noexcept
    {
        const float tw = juce::MathConstants<float>::twoPi;
        const float slope =
              wowDepthMsFor (wow01, idx) * 0.001f * tw * kWowHz
                * std::cos ((phaseWow01 - std::floor (phaseWow01)) * tw)
            + flutterDepthMsFor (fl01, idx) * 0.001f * tw * kFlutterHz
                * std::cos ((phaseFl01 - std::floor (phaseFl01)) * tw);
        return 1200.0f * std::log2 (juce::jmax (1.0f - slope, 0.05f));
    }

    // Phases, angles de bobines et compteur reels, publies pour l'UI.
    float getWowPhase01() const noexcept     { return uiWowPhase.load (std::memory_order_relaxed); }
    float getFlutterPhase01() const noexcept { return uiFlPhase.load (std::memory_order_relaxed); }
    float getSupplyPhase01() const noexcept  { return uiSupply.load (std::memory_order_relaxed); }
    float getTakeupPhase01() const noexcept  { return uiTakeup.load (std::memory_order_relaxed); }
    float getCounterSeconds() const noexcept { return uiCounter.load (std::memory_order_relaxed); }

private:
    double sampleRate = 44100.0;
    int    numCh = 2;

    float wow      = 0.35f;
    float flutter  = 0.35f;
    float sat      = 0.4f;
    int   speedIdx = 1;
    float hiss     = 0.2f;

    float wowPhase = 0.0f, flPhase = 0.0f;
    float supplyPhase = 0.0f, takeupPhase = 0.0f;
    float counterSec = 0.0f;

    // Bosse de tete (biquad RBJ peaking) et perte d'aigus, par canal.
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        inline float process (float x) noexcept
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }
    };

    void updateBump();

    Biquad bump[2];
    float  lpState[2] { 0.0f, 0.0f };
    int    bumpSpeedIdx = -1;

    // Generateur de souffle : LCG rapide, un par canal, sans verrou.
    uint32_t noiseState[2] { 0x12345678u, 0x87654321u };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine { 1 << 15 };

    std::atomic<float> uiWowPhase { 0.0f };
    std::atomic<float> uiFlPhase  { 0.0f };
    std::atomic<float> uiSupply   { 0.0f };
    std::atomic<float> uiTakeup   { 0.0f };
    std::atomic<float> uiCounter  { 0.0f };
};

}
