# CRAY LDR URBAN INSTRUMENTS — Architecture Notes

These notes describe the current source layout and intended thread ownership. They are not a certification of real-time safety or a substitute for runtime stress tests.

## Build topology

- `CrayLdrCore`: DSP, voice engine, sample decoder, library, instrument builder and preset manager.
- `CrayLdrUrbanInstruments`: JUCE VST3 target and optional Standalone target.
- Source files currently live at repository root. CMake explicitly lists them and generates compatibility include paths for the original logical paths such as `Engine/...` and `DSP/...`.
- JUCE version: 8.0.6. Language level: C++17.

## Audio signal path

```
MIDI / UI audition queue
        |
        v
    VoiceManager <---- APVTS parameters
      |       |
      |       +---- SampleVoice -> compiled sample zones
      |
      +------------ SynthVoice -> oscillators / unison / envelopes / filter
        |
        v
     EffectsChain
 EQ -> Saturation/Distortion -> Chorus -> Delay -> Reverb
        |
        v
  master gain / output telemetry
```

The processor currently exposes stereo output and MIDI input. The audio engine clears the output buffer and renders the instrument; it is not an insert effect that mixes the incoming audio.

## Audio-thread responsibilities

The intended audio path is `PluginProcessor::processBlock` -> `AudioEngine::processBlock` -> voice rendering and effects.

- Sample decoding and instrument compilation occur outside the audio callback.
- UI audition events are passed through a bounded SPSC queue.
- Telemetry is communicated through atomics.
- Sample instruments are published through an atomic raw pointer while shared ownership is retained by the processor-side owner list.
- Each active sample voice pins the compiled instrument through its audio reference counter.
- A JUCE timer on the message thread calls `collectGarbage()` to reclaim retired snapshots.
- The audio callback does not acquire `instrumentOwnerMutex_`; publication and reclamation serialize on non-audio threads.

**Important:** this is the current ownership design, not a completed concurrency proof. The hazard-pointer publication/reclamation protocol and voice pin/unpin lifecycle still require stress testing, race detection where available, and host shutdown/reload tests. Do not advertise the plugin as formally lock-free or fully real-time safe until that work is complete.

## Synth and sampler

### Synth

The source includes four PolyBLEP oscillator paths, unison banks, amplitude/filter envelopes, a state-variable filter, dual LFOs and a modulation matrix. The voice manager supports polyphonic and monophonic/legato operation, voice stealing, sustain pedal, pitch bend, mod wheel and aftertouch.

### Sample engine

The instrument definition stores zones with:
- source file, root note, key range and velocity range;
- layer assignment and gain;
- sample start/end, reverse and loop points/crossfade;
- pitch/fine tune, pan and fades;
- per-zone amplitude envelope and filter.

WAV, AIFF and FLAC decoding is enabled. Samples are decoded and compiled before publication to the audio engine. The sample voice uses cubic Hermite interpolation.

The current factory presets do not include factory sample content. The sample engine and instrument builder need dedicated tests with short/long files, mono/stereo files, mismatched sample rates, invalid loop points, missing paths, velocity splits and overlapping zones.

## Effects

The effects chain contains four-band parametric EQ, saturation/distortion, stereo chorus, stereo delay and algorithmic reverb. Parameters are read from APVTS and applied to the DSP modules during audio processing. Verify parameter smoothing, coefficient-update cost and denormal/CPU behavior under automation before release.

## State and presets

The processor stores a JSON envelope containing:
- `schemaVersion`;
- preset metadata;
- APVTS state serialized as XML text;
- the active `InstrumentDefinition` JSON object.

Preset files contain the same parameter state plus metadata and an instrument definition. Instrument/sample paths may be absolute or relative to the file's directory. Portable-pack code copies referenced samples into the package.

Loading validates the JSON root, schema version, APVTS XML root/type and instrument JSON before replacing the live APVTS state. Further acceptance tests are required for malformed data, old schema versions, missing samples, relative paths, portable-pack traversal safety and repeated project restore.

## UI

The editor contains Main, Edit, Sample Editor, Instrument Builder and Effects views, plus shared header/browser components. UI components are JUCE controls and attachments; each control still needs a control-to-DSP audit. A visible control is not evidence that its intended behavior is complete.

## CI and release status

The Windows GitHub Actions workflow:
1. checks out the source;
2. obtains JUCE 8.0.6;
3. configures Visual Studio 2022 x64;
4. builds Release VST3 and Standalone targets;
5. verifies that the VST3 bundle contains at least eight factory preset files;
6. validates preset JSON and APVTS parameter IDs;
7. uploads the VST3 bundle.

The workflow currently sets `CRAY_BUILD_TESTS=OFF` because the legacy test sources need porting to the current API. A green build therefore means **compile/package success only**. It does not mean unit tests passed or that FL Studio/Cubase were launched.

## Release acceptance checklist

- [ ] Port and enable the complete JUCE unit-test suite.
- [ ] Add malformed preset/state and save/reopen tests.
- [ ] Stress-test instrument replacement, voice stealing, repeated note-on/off and editor close/reopen.
- [ ] Verify no allocation, lock or file access in the audio callback.
- [ ] Test sample mapping, velocity layers, loops, reverse, fades and relinking.
- [ ] Audit every UI control against its DSP destination.
- [ ] Create and audition the factory multisample library.
- [ ] Test FL Studio and Cubase VST3 scanning, MIDI, automation, preset loading, project restore and shutdown.
- [ ] Measure CPU at 44.1/48/96 kHz and 64/128/256/512 sample blocks.
- [ ] Finish installer, docs, license audit and release signing.
