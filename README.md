# Funky Moose Rootflow Synth

![Funky Moose Rootflow Synth Banner](docs/rootflowsynthbanner.png)

![JUCE](https://img.shields.io/badge/Built%20with-JUCE-3A4E6A?style=for-the-badge)
![C++](https://img.shields.io/badge/C%2B%2B-17-3A4E6A?style=for-the-badge)
![VST3](https://img.shields.io/badge/VST3-supported-3A4E6A?style=for-the-badge)
![AudioUnit](https://img.shields.io/badge/AudioUnit-supported-3A4E6A?style=for-the-badge)
![Standalone](https://img.shields.io/badge/Standalone-supported-3A4E6A?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Evolving-5FD1C7?style=for-the-badge)
![Version](https://img.shields.io/badge/Version-1.3.2-blue?style=for-the-badge)

**Funky Moose Rootflow Synth** is an organic, visually reactive software instrument built with **JUCE** and **C++17**.

It blends ambient synthesis, bio-inspired sequencing, evolving modulation and a living interface into one playable instrument. Instead of feeling sterile or mechanical, Rootflow is designed to breathe, drift, pulse and grow.

## Features

- Organic synth engine for evolving textures, drones, pulses and ambient tones
- Bio-Sequencer for living rhythmic motion and mutation-driven pattern shaping
- Reactive center panel with node-based movement and organism-like visual feedback
- Root, Pulse and Ambient field layout for stable tone, animated motion and spatial shaping
- Patch workflow with save, delete and mutate actions directly in the main interface
- Playable keyboard and direct performance controls
- Available as **Audio Unit**, **VST3** and **Standalone** on macOS

## Interface

### Bio-Sequencer

Controls evolving step activity, rhythmic motion and the internal pulse of a patch.

### Center Panel

The living visual field. It reflects movement, modulation and interaction in real time.

### Root Field / Pulse Field / Ambient Field

These sections shape the body of the instrument:

- **Root Field**: depth, soil, anchor and tonal grounding
- **Pulse Field**: rate, breath, growth and movement
- **Ambient Field**: air, ground and space balance

### Version 1.3.2 (Latest)
- **DSP Parameter Smoothing**: Implemented per-sample interpolation for all critical effect parameters (Mix, Delay, Feedback, Resonance) to eliminate zipper noise and artifacts.
- **Musical ADSR Shaping**: Applied exponential power curves to the envelope output for a punchier, more analog response.
- **Filter Bite**: Added dynamic envelope modulation to the filter cutoff for more expressive performance.
- **Unison Stability**: Voice counts are now stabilized at note-on to prevent clicking during parameter modulation.
- **Windows CI Fixed**: Resolved MSVC ambiguity in GitHub Actions.
- **Preset Switching Stable**: `masterVolume` now persists across preset changes.
- **Standalone Startup Hardened**: Fixed null-deref crash on app startup.

## Screenshot

![Funky Moose Rootflow Synth Screenshot](docs/rootflow-screenshot-main.png)

## Installation

### macOS

- **Audio Unit**: copy the `.component` bundle to `~/Library/Audio/Plug-Ins/Components` or `/Library/Audio/Plug-Ins/Components`
- **VST3**: copy the `.vst3` bundle to `~/Library/Audio/Plug-Ins/VST3` or `/Library/Audio/Plug-Ins/VST3`
- **Standalone**: move the `.app` bundle to `Applications`

### Notes

- Unsigned builds may need to be opened once via Finder or allowed in `System Settings > Privacy & Security`
- If you are replacing an older `RootFlow` build, rescan your DAW after installing `Funky Moose Rootflow Synth`

## Build from Source

### Requirements

- CMake 3.22 or newer
- JUCE 8.0.10
- C++17 compatible compiler
- Xcode or Xcode Command Line Tools on macOS

### Quick Build

```bash
git clone https://github.com/blubass/funky-moose-rootflow-synth.git
cd funky-moose-rootflow-synth
cmake --preset default
cmake --build --preset default
```

The included preset expects JUCE here:

```text
$HOME/Developer/JUCE/install/lib/cmake/JUCE-8.0.10
```

If your JUCE install lives elsewhere, pass `-DJUCE_DIR=/path/to/JUCE/lib/cmake/JUCE-8.0.10` when configuring.

## Roadmap

- deeper modulation routing
- richer voice architecture
- expanded sequencer behaviors
- stronger patch and preset handling
- refined visual polish and release packaging

## Changelog

### [1.3.2] - 2026-04-28
- **Smooth Interaction**: Implemented robust per-sample parameter smoothing for all global effects to eliminate "holprige" behavior.
- **Analog ADSR**: Retuned envelope curves to exponential for punchier, more musical articulation.
- **VCF Drive**: Added envelope-to-filter modulation depth scaling.
- **Bugfixes**: Resolved Windows CI failures and stabilized unison voice management.

### [1.3.1] - 2026-04-20
- **Preset Volume**: `masterVolume` now persists across preset changes instead of collapsing to `-48 dB`.
- **Standalone Crash Fix**: Hardened the editor overlay timer against stale parameter access on startup.
- **State Migration**: Legacy parameter IDs for sessions and user presets are migrated into the current layout.
- **Prompt and Node Fixes**: Corrected node-slot mapping and prompt-memory read/write behavior.

### [1.3.0] - 2026-04-01
- **Audio Unit Build**: Re-enabled AU output in the standard CMake presets and validated the installed component with `auval`.
- **macOS Packaging**: Locked release builds to universal `arm64` and `x86_64` binaries with a Monterey deployment target.
- **Audio Path Fixes**: Restored keyboard MIDI injection, modulation feedback updates, master mix and mono maker routing, and oversampling-safe beat timing.
- **UI Sizing**: Improved the initial editor size selection for smaller MacBook displays.

### [1.2.0] - 2026-03-30
- **32-Voice Polyphony**: Increased voice count from 16 to 32 for massive chord textures.
- **Sequencer Stability**: Completely rewritten gate logic and note-off handling for rock-solid timing.
- **MUTATE System**: New organic mutation engine for evolving patch "DNA" with one click.
- **Mutation Visuals**: Integrated "Spore Burst" visual feedback for parameter mutations.
- **Performance**: Cached modulation indices to reduce CPU load in the audio thread.
- **Meta**: Added Release CMake presets and updated version badges.

## Author

Uwe Arthur Felchle

Musician, composer, producer and developer

https://uwefelchle.at

## License

This project is released under the MIT License. See [LICENSE](LICENSE).
