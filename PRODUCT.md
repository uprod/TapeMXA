# Product

<!-- impeccable:product-schema 1 -->

## Platform

desktop

Native JUCE audio plugin (AU/VST3/Standalone, macOS 11+, universal binary). Sibling of the MXA suite; family design authority: `../PhaserMXA/DESIGN.md`; family product context: `../PhaserMXA/PRODUCT.md`.

## Product Purpose

A reel-to-reel tape machine: wow and flutter (modulated delay), tape saturation (normalized tanh), speed-dependent head bump and HF loss, and hiss — all keyed to the 7.5/15/30 IPS transport speed.

## Capabilities and Constraints

- Exactly six parameters: `wow` (slow ±1.2 ms at 0.8 Hz), `flutter` (fast ±0.15 ms at 9.3 Hz), `sat` (tanh drive ×1..×6), `speed` (7.5/15/30 IPS choice: scales instability ×1.4/×1.0/×0.7, HF loss 9/14/19 kHz, head bump 45/60/90 Hz +2 dB), `hiss` (−80..−40 dB white noise, LCG per channel), `mix`.
- Both channels share one modulated delay (one tape transport); saturation is not oversampled (gentle drive by design — DriveMXA covers hard clipping).
- UI truth taps: atomic wow/flutter phases, reel angles (take-up ×1.35) and transport counter; static `deviationCents()` and per-speed constants — the single source of truth for FIG. 1's wow+flutter deviation trace and FIG. 2's live reels, running counter and printed playback figures.
- Editor: Service Manual family sheet, 820×470, spot ink magnetic-oxide brown #A9713C, DWG NO. MXA-TP-01.

## Brand Commitments

Inherits the family's: MXAudio, "BY MESCALINA" credit, one spot ink per sibling (Tape = magnetic-oxide brown).

## Evidence on Hand

Working DSP (`Source/TapeEngine.*`). No users/testimonials — nothing may be fabricated.
