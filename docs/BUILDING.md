# Building RootFlow from Source

## Requirements
- **CMake** (3.22 or newer)
- **JUCE** (8.0.10)
- **C++17** compatible compiler (Xcode, MSVC, or GCC)

## Build Options

### 1. Using a Global JUCE Install
If you have JUCE installed in a standard location, pass the path during configuration:

```bash
cmake -B build -DJUCE_DIR=/path/to/JUCE/lib/cmake/JUCE-8.0.10
cmake --build build --parallel
```

### 2. The Self-Contained Way (Recommended)
You can clone JUCE directly into the project's `external/` directory:

```bash
git clone --recursive https://github.com/blubass/funky-moose-rootflow-synth.git
cd funky-moose-rootflow-synth

# Clone JUCE 8.0.10
git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git external/JUCE

# Configure and Build
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
```

## Platform Specifics

### macOS
- The build will generate a Universal Binary (arm64 and x86_64).
- Deployment target is set to macOS 12.0 (Monterey).

### Windows
- Use a Developer PowerShell for VS 2022 or similar.
- The build will generate VST3 and Standalone formats.

## Continuous Integration
Automated builds are triggered on every push to the `main` and `develop` branches via GitHub Actions. You can check the "Actions" tab on GitHub to see the status of recent builds.
