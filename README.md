<img width="360" height="360" alt="ForgeIcon-v2 5-1024 icon" src="https://github.com/user-attachments/assets/dce3d4b8-bec1-4df6-b53c-992aaad3a73e" />
# FORGE Synth

FORGE Synth is a polyphonic, MIDI-playable virtual instrument built with JUCE. Its local wavetable engine is self-contained; the optional Python API generates the same wavetable format and can use a compatible ONNX decoder. There is no bundled trained model, cloud service, telemetry, or network access in the plugin.

## Playable features

- 16 pitch-bendable stereo voices with nine-level harmonic-limited wavetable oscillators, sub oscillator, noise, stereo width, velocity-sensitive ADSR, and a resonant low-pass filter with bipolar envelope modulation.
- A draggable, audible four-shape timbre pad, plus drive, tempo-synced delay, stereo room, output meter, and safety ceiling.
- Ten factory patches for bass, keys, plucks, leads, pads, rhythms, and textures; timbre randomisation that preserves the rhythm, envelope, and effects.
- Host-restorable state and portable `.forgepreset` patches, including imported table cycles.
- Import and pitch-safe playback of a 2,048-frame mono or stereo WAV/AIFF single cycle; the local backend's WAV response is ready to import.
- A 16-step rhythm gate that follows DAW play position and tempo, with internal tempo fallback, step divisions, and adjustable swing.
- On-screen MIDI keyboard, resizable interface, resettable/typed knobs, contextual tooltips, and an immediate panic button.

## Build

The verified build uses macOS arm64, Apple Clang, CMake 4.4, Ninja, and JUCE 8.0.15. CMake fetches JUCE on the first build and reuses the pinned source afterwards.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 6
ctest --test-dir build --output-on-failure
```

The plugin output is `build/forge_synth/plugin_source/ForgeSynth_artefacts/Release/VST3/FORGE Synth.vst3`; standalone is beside it under `Standalone/FORGE Synth.app`; AU (macOS only) is under `AU/FORGE Synth.component`. Rescan VST3 in your DAW after copying it to your system VST3 folder. Launch standalone with `open 'build/forge_synth/plugin_source/ForgeSynth_artefacts/Release/Standalone/FORGE Synth.app'`.

The build emits VST3 and standalone on macOS, Windows, and Linux, and adds AU on macOS. To use a local JUCE source tree, add `-DJUCE_ROOT=/absolute/path/to/JUCE` to the configure command. JUCE's platform dependencies apply. Debian-based Linux may need `libasound2-dev libfreetype6-dev libfontconfig1-dev libx11-dev libxinerama-dev libxrandr-dev libxcursor-dev`; Windows requires the Visual Studio C++ toolchain and Windows SDK.

## Optional backend

The local C++ synth has no backend requirement. For offline wavetable generation, see [backend installation, routing, ONNX contract, and environment settings](forge_synth/docs/ai_api.md). By default, the Python service uses the same deterministic timbre map. ONNX mode requires a user-supplied trained model; the included exporter makes a plumbing reference, not a trained sound model. The service exports a one-cycle WAV that the plugin imports.

## Guides

- [Architecture, realtime data flow, parameters, and state](docs/architecture.md)
- [Build, install, and play guide](forge_synth/docs/usage.md)
- [Backend API, routing, environment setup, and model contract](forge_synth/docs/ai_api.md)
- [Backend JSON configuration schema](docs/ai_config_schema.md)
- [JUCE and VST format configuration](forge_synth/plugin_source/CMakeLists.txt)
- [Regression tests](tests/)

This is an actively developed instrument, not a claim of plug-in certification or a signed installer. Verify the plug-in in your target DAW before a production session.
