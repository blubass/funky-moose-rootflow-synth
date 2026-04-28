# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.2] - 2026-04-28

### Added
- **DSP Parameter Smoothing**: Implemented per-sample interpolation for all critical effect parameters (Mix, Delay, Feedback, Resonance).
- **Musical ADSR Shaping**: Applied exponential power curves to the envelope output.
- **Filter Bite**: Added dynamic envelope modulation to the filter cutoff.
- **Audio Safety**: Added ScopedNoDenormals, safety clamps, and a transparent **softLimit** master stage.
- **PolyBLEP Waveforms**: Anti-aliased oscillators for saw and pulse waves.

### Changed
- **Roadmap**: Updated to a release-oriented roadmap.
- **Documentation**: Reformatted README files for better readability and added a prominent Download section.

### Fixed
- **Unison Stability**: Stabilized voice counts at note-on to prevent clicking.
- **Windows CI**: Resolved MSVC ambiguity in GitHub Actions.
- **Preset Switching**: `masterVolume` now persists across preset changes.
- **Standalone Startup**: Fixed null-deref crash on app startup.

## [1.3.1] - 2026-04-20

### Fixed
- **Preset Volume**: `masterVolume` persistence fix.
- **Standalone Crash**: Hardened editor overlay timer.
- **State Migration**: Legacy parameter ID migration.

## [1.3.0] - 2026-04-01

### Added
- **Audio Unit Build**: Re-enabled AU output in standard presets.
- **macOS Packaging**: Universal arm64/x86_64 binaries.

### Fixed
- **Audio Path**: Restored keyboard MIDI, modulation feedback, and master mix routing.

## [1.2.0] - 2026-03-30

### Added
- **32-Voice Polyphony**: Increased voice count for massive textures.
- **MUTATE System**: Organic mutation engine for patch "DNA".

### Changed
- **Sequencer Stability**: Completely rewritten gate logic and note-off handling.
