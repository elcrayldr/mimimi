# CRAY LDR URBAN INSTRUMENTS

**Windows x64 VST3 instrument — development build 1.0.2**

CRAY LDR URBAN INSTRUMENTS is a JUCE-based hybrid synthesizer and sample instrument. This repository contains the C++ source, CMake project, Windows CI workflow and eight factory synth presets.

> **Release status:** development / pre-release. A successful CI build confirms that the Windows VST3 target compiles and packages. It does not establish DAW compatibility, real-time safety under stress, complete feature coverage or commercial-release readiness. FL Studio and Cubase have not been validated as part of this repository's automated workflow.

## Current implementation

- Four-oscillator synth architecture with PolyBLEP oscillators and unison.
- Polyphonic voice manager with mono/legato modes, sustain, pitch bend and modulation inputs.
- Dual LFOs and an eight-slot modulation matrix.
- Sample zones with key/velocity ranges, layers, pitch, reverse, loop, fades, envelopes and per-zone filtering.
- WAV, AIFF and FLAC sample decoding.
- Non-destructive sample editor model and instrument builder with filename-based MIDI root-note detection and key-range mapping.
- EQ, saturation/distortion, chorus, delay and algorithmic reverb DSP modules.
- JSON preset/instrument serialization and portable-pack import/export code.
- Custom JUCE interface with Main, Edit, Sample Editor, Instrument Builder and Effects views.

The eight checked-in `.craypreset` files are synth patches. They do **not** constitute a factory multisample/sample library.

## Build requirements

- Windows 10/11 x64
- Visual Studio 2022 with Desktop development with C++
- CMake 3.22+
- JUCE 8.0.6 (downloaded by FetchContent, or supplied with `JUCE_DIR`)

## Build (Windows)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCRAY_BUILD_TESTS=OFF -DCRAY_BUILD_STANDALONE=ON

cmake --build build --config Release --parallel
```

To use a local JUCE checkout:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DJUCE_DIR="C:/SDK/JUCE" -DCRAY_BUILD_TESTS=OFF
```

The automated Windows workflow currently builds the VST3 and Standalone targets with the legacy unit-test target disabled. The test sources still require porting and must not be considered passing.

## VST3 bundle

The expected output is:

```
CRAY LDR URBAN INSTRUMENTS.vst3/
  Contents/
    x86_64-win/
      CRAY LDR URBAN INSTRUMENTS.vst3
    Resources/
      FactoryPresets/
        *.craypreset
```

Install the complete `.vst3` bundle into:

```
C:\\Program Files\\Common Files\\VST3\\
```

Then rescan plugins in the host. The CI workflow checks the bundle, validates the factory preset JSON/APVTS XML and uploads the VST3 artifact. It does not launch a DAW.

## Preset and instrument formats

- `.craypreset`: preset metadata, APVTS XML and instrument definition.
- `.crayinst`: instrument metadata, layers and sample zones.
- `.craypack`: portable ZIP package containing a preset/instrument and copied samples.

Sample paths can be relative to the preset/instrument directory or absolute. Moving a project without its referenced samples can produce missing-sample warnings; use portable packs or relink samples after moving content.

## Known release blockers

1. Port and enable the unit-test suite; add tests for malformed presets, state restore, sample-zone mapping, resampling, loops and voice lifecycle.
2. Audit the sample-instrument ownership/hazard-pointer protocol with stress tests and sanitizers where available.
3. Verify that the audio callback performs no heap allocations, locks or file I/O under realistic host block sizes and automation.
4. Validate every UI control against the DSP parameter it is meant to control; remove or implement any decorative/nonfunctional controls.
5. Build and audition a genuine factory multisample library. The current eight presets are synth-only.
6. Test VST3 scanning, opening, MIDI, automation, preset changes, project save/reopen, sample relinking, CPU use and shutdown in FL Studio and Cubase.
7. Complete installer/package, user documentation, licensing review and release signing.

## Project layout

The current checkout stores implementation files at repository root. CMake explicitly lists sources and generates compatibility include paths for the original logical module names. See [ARCHITECTURE.md](ARCHITECTURE.md) for the current threading and DSP design notes.

## License

Project source and product assets are proprietary unless a file explicitly states otherwise. JUCE remains subject to its own licensing terms.
