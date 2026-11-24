# CRAY LDR URBAN INSTRUMENTS — Architecture

> Internal architecture reference for developers working on the codebase.

---

## Module Overview

```mermaid-like diagram (text representation)

  ┌─────────────────────────────────────────────────────────────┐
  │                      PluginProcessor                        │
  │  (AudioProcessor: prepareToPlay, processBlock, state I/O)  │
  └──────────────┬─────────────────────────────────────────────┘
                 │ owns
  ┌──────────────▼──────────────────────────────────┐
  │                   AudioEngine                   │
  │  • Dual LFO (LfoEngine ×2)                      │
  │  • ModMatrix (8 slots, 10 sources × 19 dests)   │
  │  • EffectsChain (EQ→Sat→Chorus→Delay→Reverb)   │
  │  • Lock-free instrument swap                    │
  │  • per-sample MIDI dispatch                     │
  └────┬──────────────────────────┬─────────────────┘
       │ owns                     │ owns
  ┌────▼────────┐           ┌────▼────────┐
  │ VoiceManager│           │ EffectsChain│
  │  128+8 slots│           └─────────────┘
  │  Poly/Mono/ │
  │  Legato     │
  └──┬───────┬──┘
     │       │ manages N voices
  ┌──▼──┐ ┌──▼──────┐
  │Synth│ │ Sample  │
  │Voice│ │  Voice  │
  └──┬──┘ └────┬────┘
     │          │
  ┌──▼──────┐ ┌▼─────────────┐
  │4× Unison│ │ CompiledSample│
  │OscBank  │ │ Instrument    │
  │AhdsrEnv │ │ (RT-safe ref) │
  │ZdfFilter│ └───────────────┘
  └─────────┘

  Message Thread:
  ┌─────────────────────────────────────┐
  │  LibraryManager  InstrumentBuilder  │
  │  SampleDecoder   SampleEditorModel  │
  │  PresetManager                      │
  └─────────────────────────────────────┘
  → publishes compiled instrument via atomic<> pointer
  → audio thread reads pointer, never touches files
```

---

## Real-Time Safety Contract

The audio callback (`AudioEngine::process`, called from `processBlock`) must never:

| Prohibited action | Mitigation |
|-------------------|-----------|
| Allocate heap memory | All buffers pre-allocated in `prepareToPlay` |
| Lock a mutex | Instrument swap uses `std::atomic<>` + ref-count |
| Read from disk | SampleBuffer pre-loaded on message thread |
| Throw exceptions | DSP code uses `noexcept`; filters have overflow guards |
| Call OS-blocking APIs | None used in audio path |

### Instrument Swap Protocol

1. **Message thread** builds a `CompiledSampleInstrument` (heap-allocated, refcount = 0).
2. Message thread atomically stores pointer in `AudioEngine::m_pendingInstrument`.
3. At the top of `AudioEngine::process`, if `m_pendingInstrument != nullptr`:
   - Atomically swap pending → active.
   - Increment `active->audioRefCount`.
   - Decrement old instrument's `audioRefCount`.
4. A 200ms timer on the message thread garbage-collects instruments with `audioRefCount == 0`.

---

## DSP Details

### Anti-Aliasing (Oscillators)

All band-limited waveforms use the **PolyBLEP** method (polynomial band-limited step). This reduces aliasing artifacts from discontinuities (Saw reset, Square transition) without requiring oversampling. Noise is a 32-bit xorshift PRNG, inherently band-limited.

### Zero-Delay Feedback Filter

The `ZdfSvfFilter` implements a **Trapezoidal-Integrator State-Variable Filter** (TPT/ZDF). This topology is unconditionally stable at all cutoff frequencies (0 Hz to Nyquist) and all resonance values up to self-oscillation. It supports LP12, LP24 (2×cascaded), HP12, and BP12.

### ADSR Envelope

`AhdsrEnvelope` uses **exponential curves** for attack (concave-up), decay and release (convex-down), giving musically natural shapes. A `FastKill` method immediately starts a very short release for voice stealing without audible clicks.

### Cubic Hermite Interpolation

`SampleVoice` uses 4-point cubic Hermite interpolation when the playback rate differs from 1:1. This achieves C1 continuity (smooth derivatives) at sample boundaries, superior to linear interpolation for resampling quality.

### Unison

`UnisonOscillatorBank` detuning is symmetric around the base pitch: for N voices, voice i is detuned by `detune × (2i/(N-1) - 1)` semitones. The summed output is multiplied by `1/sqrt(N)` to preserve perceived loudness.

---

## Modulation Matrix

8 slots, each connecting one source to one destination with a bipolar amount.

**Sources (10):**
LFO1, LFO2, AmpEnv, FilterEnv, Velocity, ModWheel, Aftertouch, PitchBend, KeyFollow, Random

**Destinations (19):**
Osc1Pitch, Osc2Pitch, Osc3Pitch, Osc4Pitch, Osc1Level, Osc2Level, FilterCutoff, FilterResonance, AmpLevel, PanPosition, ReverbWet, ChorusDepth, ChorusRate, DelayTime, DelayFeedback, SatDrive, LFO1Rate, LFO2Rate, Osc1FMAmount

Mod amounts are evaluated once per audio block (not per-sample) for efficiency. Destinations are applied as additive offsets to their corresponding AudioEngine parameters before voice rendering begins.

---

## Effects Chain

Signal path: `EQ → Saturation/Distortion → Chorus → Delay → Reverb → Safety Limiter`

| Module | Algorithm |
|--------|-----------|
| ParametricEq | 4-band stereo: Low Shelf + 2× Peak + High Shelf (RBJ biquads) |
| SaturationDistortion | Tape (soft clip tanh), Tube (asymmetric soft clip), Warm (mild fold) + Hard Clip, Foldback, Crunch distortion |
| StereoChorus | Dual-LFO quadrature modulated delay, pre-allocated ring buffers |
| StereoDelay | Ping-Pong with tempo sync, smooth delay slew, FB high-cut filter |
| AlgorithmicReverb | Freeverb-style: 8 comb filters + 4 allpass per side, pre-delay, stereo width |
| Safety Limiter | Per-sample soft-knee limiter at –0.1 dBFS |

---

## State Serialization

### Format

```json
{
  "schemaVersion": 1,
  "apvtsXml": "<CRAY_LDR_PARAMETERS>...</CRAY_LDR_PARAMETERS>",
  "instrument": { ... }   // null if synth-only preset
}
```

The `apvtsXml` field is the JUCE APVTS XML tree serialized as a string. The `instrument` field is an `InstrumentDefinition::toJson()` object.

### Migration Strategy

On load, `setStateInformation` reads `schemaVersion`. If the version is higher than the current code understands, all missing parameters default to their layout defaults. No crash on unknown schema versions.

---

## File Format Specifications

### `.craypreset`

```
{
  "schemaVersion": integer,
  "name": string,
  "author": string,
  "category": string,       // one of the 8 category names
  "bank": string,
  "tags": string,           // space-separated
  "isFactory": bool,
  "apvtsXml": string,       // JUCE ValueTree XML
  "instrument": object|null // InstrumentDefinition JSON, or null
}
```

### `.crayinst`

Identical to `InstrumentDefinition::toJson()`. Contains:
- `id`, `name`, `author`, `category`, `tags`
- `zones[]` — array of `SampleZoneSpec` objects with:
  - `filePath` (relative to the `.crayinst` file, or absolute)
  - `rootNote`, `keyLo`, `keyHi`, `velLo`, `velHi`, `layer`
  - `pitchOffsetSemitones`, `startFrame`, `endFrame`
  - `loopEnabled`, `loopStart`, `loopEnd`, `crossfadeFrames`
  - `reverse`, `gain`, `pan`
  - `adsrA`, `adsrD`, `adsrS`, `adsrR`
  - `filterEnabled`, `filterCutoff`, `filterResonance`

### `.craypack`

A standard ZIP archive with the following structure:
```
preset.craypreset
samples/
  <relative-path-to-sample-1.wav>
  <relative-path-to-sample-2.wav>
  ...
```
File paths inside `preset.craypreset` are rewritten to be relative to `samples/` on export, and restored on import.

---

## Threading Model

| Thread | Responsibility |
|--------|---------------|
| Audio thread | `AudioEngine::process`, `VoiceManager`, `SynthVoice`, `SampleVoice`, `EffectsChain` |
| Message thread | UI events, library browsing, sample loading, instrument building, preset save/load |
| Timer thread (200ms) | GC of old `CompiledSampleInstrument` objects, telemetry polling |
| JUCE audio thread | Calls `PluginProcessor::processBlock`, which delegates to `AudioEngine` |

Communication audio←→message uses only:
- `std::atomic<>` pointer swap (instrument swap)
- `LockFreeQueue<MidiEvent>` SPSC queue (UI→audio MIDI events)
- `AudioTelemetry` struct of atomics (peak meters, voice counts)

---

## Known Limitations (v1.0.0)

| Area | Note |
|------|------|
| Sample formats | OGG/Vorbis not included (JUCE_USE_OGGVORBIS=0); add it in CMakeLists to enable |
| ARA2 | Not supported |
| MPE | Not supported in v1; planned for v1.2 |
| macOS / Linux | Build system supports them in principle; not tested or packaged |
| VST2 | Not provided; VST2 SDK is deprecated |
