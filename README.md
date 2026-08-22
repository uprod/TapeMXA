# TapeMXA

A reel-to-reel tape machine: wow and flutter (modulated delay), tape saturation (normalized tanh), speed-dependent head bump and HF loss, and hiss — all keyed to the 7.5/15/30 IPS transport speed.

Audio plugin (AU / VST3 / Standalone) built with [JUCE](https://juce.com). Part of the MXA plugin suite. macOS 11+.

## Build

```sh
git clone --recurse-submodules https://github.com/uprod/TapeMXA.git
cd TapeMXA
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If you already have the MXA suite checked out with a shared `../JUCE` folder, the submodule is optional — the build falls back to the sibling folder automatically.
