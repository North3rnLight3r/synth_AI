# FORGE Synth architecture

## Plugin and realtime data flow

CMake uses a pinned JUCE 8.0.15 source checkout unless `JUCE_ROOT` points to a local copy. The plugin configuration declares MIDI input, stereo output, no audio input, and no MIDI output. It builds VST3 and standalone targets, plus AU on macOS. The processor owns JUCE `AudioProcessorValueTreeState`, 16 synthesis voices, an immutable four-corner wavetable bank, step state, effects, and timestamped MIDI dispatch. The editor attaches its controls directly to host-automatable parameters.

```text
DAW MIDI / on-screen keys
        │ note, velocity, note-off, sustain, pitch bend
        ▼
JUCE Synthesiser · 16 pitch-bendable voices
        │
        ├── two detuned mipmapped tables · sub · noise
        ├── velocity × ADSR → resonant TPT low-pass
        ▼
host PPQ 16-step gate → soft drive → tempo delay → stereo room → gain / peak safety / meter
```

The wavetable bank holds sine, saw, triangle, and square corners at nine Fourier cutoffs, from 1 to 256 harmonics. MIDI pitch and host sample rate select a safe band; wavetable reads interpolate between adjacent samples. The audio callback does not request network inference, open files, or wait on wavetable locks. Imported cycles are decoded and nine band-limited tables prepared outside the audio callback, then transferred through an atomic three-buffer exchange.

MIDI is consumed by the instrument, never echoed as MIDI output. Pitch bend spans two semitones. Velocity controls amplitude. Sustain pedals preserve notes until pedal-up; panic clears voices, held on-screen keys, and effect buffers. The 16-step gate follows host PPQ and host tempo, or internal tempo with a free-running position if the host does not provide a usable play position. Gate steps mask held notes without emitting MIDI. Alternating gate steps respond to swing. The stereo delay processes release tails before a room reverb and master gain/output safety ceiling.

## Parameters and state

The persistent, automatable parameter IDs are `x`, `y`, `detune`, `sub`, `noise`, `attack`, `decay`, `sustain`, `release`, `cutoff`, `resonance`, `filterEnv`, `width`, `gain`, `drive`, `custom`, `delay`, `feedback`, `delayRate`, `reverb`, `gate`, `gateRate`, `bpm`, `swing`, and `step0` through `step15`. Keep IDs stable when changing or extending the plug-in to preserve host project automation.

Host state contains the APVTS parameter tree and, when present, the base64-encoded, normalised 2,048-sample custom cycle. `.forgepreset` files use the same XML format, schema version `1`, parameter set, patch name, and cycle. On restore, the plug-in checks schema, parameter count and IDs, finite parameter values and ranges, and finite valid wavetable samples before applying the change. The editor scales a 1120×780 design with resize limits from 896×624 to 1680×1170.

## Optional Python router

The FastAPI service is a separate development tool. It is never started or contacted by the VST. The local deterministic generator uses the same four-corner timbre mapping. `POST /wavetable.wav` returns an importable mono 2,048-frame, 48 kHz PCM single cycle. The included ONNX exporter makes a small **untrained interface reference**; there is no claimed learned model or supplied trained checkpoint.

In optional ONNX mode, the Python service loads and checks one model at startup. Its required signature is float32 `latent_input[1,2]` → float32 `wavetable_output[1,2048]`. It verifies the requested execution provider is actually available, bounds input coordinates, validates the output shape and sample values, and returns HTTP 503 on inference failure.
