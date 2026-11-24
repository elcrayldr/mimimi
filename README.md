# CRAY LDR URBAN INSTRUMENTS

> **Professional hybrid synthesizer + sampler VST3 plugin for Windows x64**  
> Built with JUCE 8.0.6 · C++17 · CMake · Visual Studio 2022

---

## Overview

CRAY LDR URBAN INSTRUMENTS is a commercial-quality virtual instrument that combines:

- **Four-oscillator synthesizer** with PolyBLEP anti-aliasing, unison, and a full modulation matrix
- **Multisample engine** supporting WAV, AIFF, and FLAC with up to 4 layers per zone
- **Sample Editor** — non-destructive waveform editing with live preview
- **Instrument Builder** — build multi-sampled instruments from your own audio files
- **Effects rack** — EQ, Saturation/Distortion, Chorus, Delay, and Reverb with real DSP
- **Factory Library** — 8 categories, 8 factory presets included
- **User Library** — importable, searchable, organized by folder and favorites
- **Preset Manager** — `.craypreset` and portable `.craypack` formats

Runs in **FL Studio 26**, **Cubase 13**, and any VST3-compatible DAW.

---

## System Requirements

| Component | Minimum |
|-----------|---------|
| OS | Windows 10 (64-bit) or Windows 11 |
| CPU | Intel/AMD x64, 4+ cores recommended |
| RAM | 4 GB (8 GB recommended) |
| DAW | Any VST3-compatible host (FL Studio 26, Cubase 13 tested) |

### Build Requirements

| Tool | Version |
|------|---------|
| Visual Studio | 2022 (Community or Professional) — with **Desktop development with C++** workload |
| CMake | 3.22 or newer |
| JUCE | 8.0.6 (fetched automatically via CMake FetchContent) |
| Git | Any recent version |

---

## Quick Build (Windows)

### 1. Clone the repository

```bat
git clone https://github.com/crayldr/urban-instruments.git
cd urban-instruments
```

### 2. Configure

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCRAY_BUILD_TESTS=ON ^
      -DCRAY_BUILD_STANDALONE=ON
```

> **Local JUCE checkout (faster):** If you already have JUCE 8.0.6 on disk, skip the FetchContent download:
> ```bat
> cmake -B build -G "Visual Studio 17 2022" -A x64 ^
>       -DJUCE_DIR="C:\path\to\JUCE" ^
>       -DCMAKE_BUILD_TYPE=Release
> ```

### 3. Build

```bat
cmake --build build --config Release --parallel
```

### 4. Run unit tests

```bat
cd build
ctest -C Release --output-on-failure
```

### 5. Find the built VST3

```
build\CrayLdrUrbanInstruments_artefacts\Release\VST3\
  CRAY LDR URBAN INSTRUMENTS.vst3\
```

---

## Installation in Your DAW

### Manual install

1. Copy `CRAY LDR URBAN INSTRUMENTS.vst3` to:
   ```
   C:\Program Files\Common Files\VST3\
   ```
2. Copy the `Resources\` folder next to the `.vst3`, or use the automatic post-build copy (the CMake target stages them in the same output directory).
3. Re-scan plugins in your DAW.

### FL Studio 26

- Go to **Options → Manage Plugins → Find more plugins**
- Add `C:\Program Files\Common Files\VST3\` if not already listed
- Scan and verify CRAY LDR URBAN INSTRUMENTS appears in the browser

### Cubase 13

- **Studio → VST Plug-in Manager → Update**

---

## Factory Library Setup

After build, copy the contents of `Resources\` next to your VST3:

```
C:\Program Files\Common Files\VST3\
  CRAY LDR URBAN INSTRUMENTS.vst3\
  Resources\
    FactoryPresets\
      Plucks & Bells\
      Basses & 808\
      Keys & Pianos\
      ...
    FactorySamples\
    Graphics\
```

The plugin reads factory presets from this `Resources\` directory at startup. The user library defaults to `%USERPROFILE%\Documents\CRAY LDR\User Library\` and can be changed inside the plugin via **Library → Change User Library Location**.

---

## Project Structure

```
CrayLdrUrbanInstruments/
├── CMakeLists.txt           # Build system (JUCE 8.0.6, VST3, tests)
├── CMakePresets.json        # VS2022 x64 presets
├── README.md                # This file
├── docs/
│   └── ARCHITECTURE.md      # Module diagram, RT-safety guarantees, file formats
├── Source/
│   ├── Common/              # ProductInfo, Categories, ParameterIDs, LockFreeQueue
│   ├── DSP/
│   │   ├── Oscillators/     # PolyBlepOscillator, UnisonOscillatorBank
│   │   ├── Envelopes/       # AhdsrEnvelope
│   │   ├── Filters/         # ZdfSvfFilter (TPT / Zero-Delay Feedback)
│   │   ├── Modulation/      # LfoEngine, ModMatrix
│   │   ├── Effects/         # ParametricEq, SaturationDistortion, StereoChorus,
│   │   │                    #   StereoDelay, AlgorithmicReverb, EffectsChain
│   │   └── Utilities/       # DspMath, ParameterSmoother, DcBlocker
│   ├── Engine/
│   │   ├── SampleData       # SampleBuffer (RT-safe ref-counted)
│   │   ├── SynthEngine      # SynthVoice (4 osc + dual ADSR + filter)
│   │   ├── SampleEngine     # SampleVoice (multi-zone, cubic Hermite resampling)
│   │   ├── VoiceManager     # Poly/Mono/Legato, voice stealing, sustain pedal
│   │   └── AudioEngine      # Top-level RT engine: LFO, ModMatrix, Effects, MIDI
│   ├── Library/
│   │   ├── SampleDecoder    # WAV/AIFF/FLAC decoder with weak cache
│   │   ├── LibraryManager   # Factory + User library with JSON index
│   │   ├── LibraryItem      # LibraryItem, LibraryQuery structs
│   │   ├── InstrumentDefinition # Zone spec → CompiledSampleInstrument + JSON
│   │   ├── SampleEditorModel # Non-destructive editor model
│   │   ├── InstrumentBuilder # Note-name parser, automap, buildInstrument()
│   │   └── PresetManager    # .craypreset/.craypack save/load/export/import
│   ├── Plugin/
│   │   ├── PluginProcessor  # AudioProcessor: processBlock, state, APVTS
│   │   └── PluginEditor     # AudioProcessorEditor: 5 views, 30Hz timer
│   └── UI/
│       ├── Theme/           # ColorPalette, CrayLookAndFeel (custom JUCE L&F)
│       ├── Components/      # RotaryKnob, LevelMeter, EnvelopeDisplay,
│       │                    #   WaveformView, KeyZoneMapView, VirtualKeyboardBar,
│       │                    #   HeaderBar, BrowserSidebar
│       └── Views/           # MainView, EditView, SampleEditorView,
│                            #   InstrumentBuilderView, EffectsView
├── Resources/
│   ├── Graphics/            # logo.svg, icon.svg (embedded via juce_add_binary_data)
│   ├── FactoryPresets/      # 8 categories × factory presets (.craypreset)
│   └── FactorySamples/      # Factory sample audio (WAV/FLAC)
├── Tests/                   # JUCE unit tests (7 files)
├── scripts/
│   └── package_release.ps1  # PowerShell: build + package ZIP
└── .github/
    └── workflows/
        └── build.yml        # GitHub Actions CI: build, test, upload artifact
```

---

## File Formats

| Extension | Description |
|-----------|-------------|
| `.craypreset` | JSON preset: metadata + APVTS XML + optional instrument definition |
| `.crayinst` | JSON instrument definition: zones, layers, file paths |
| `.craypack` | ZIP archive: preset + referenced sample files (portable exchange) |

---

## Development Notes

- **Audio thread safety:** No heap allocations, no mutex locks, no disk I/O inside `processBlock`. All heavy work (sample loading, instrument compilation) occurs on the message thread and is published to the audio thread via `std::atomic<>` pointer swap.
- **Voice stealing:** Quiet-first (finds the voice with the lowest amplitude envelope value) with fallback to oldest-voice for equal amplitudes.
- **State versioning:** `getStateInformation` / `setStateInformation` wrap the APVTS XML in a JSON envelope with `"schemaVersion": 1`. Future schema migrations are handled by version inspection at load time.

---

## License

CRAY LDR URBAN INSTRUMENTS source code is proprietary software.  
© 2025 CRAY LDR. All rights reserved.

Third-party components:
- **JUCE** — licensed under the JUCE End User Licence Agreement (see [juce.com/juce/licensing](https://juce.com/juce/licensing/))
- **Freeverb** — public domain algorithm (Jezar at Dreampoint)
