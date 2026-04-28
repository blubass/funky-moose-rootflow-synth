# Installation Guide

## macOS

RootFlow is available as an Audio Unit (AU), VST3, and Standalone application.

### Automatic Installation
Currently, we do not provide a PKG installer. Please follow the manual steps below.

### Manual Installation
1. Download the latest release from the [Releases](https://github.com/blubass/funky-moose-rootflow-synth/releases) page.
2. Unzip the file.
3. **Audio Unit**: Copy `Funky Moose Rootflow Synth.component` to:
   - `~/Library/Audio/Plug-Ins/Components` (User only)
   - or `/Library/Audio/Plug-Ins/Components` (System wide)
4. **VST3**: Copy `Funky Moose Rootflow Synth.vst3` to:
   - `~/Library/Audio/Plug-Ins/VST3`
   - or `/Library/Audio/Plug-Ins/VST3`
5. **Standalone**: Move the `.app` bundle to your `Applications` folder.

### Security Note (Unsigned Builds)
Since these builds are currently unsigned, macOS will block them by default.
1. Right-click the plugin/app in Finder and select **Open**.
2. Alternatively, go to **System Settings > Privacy & Security** and click **Allow Anyway** at the bottom.

## Windows

### VST3
1. Download the Windows build from the [Releases](https://github.com/blubass/funky-moose-rootflow-synth/releases) page.
2. Copy the `.vst3` file to:
   - `C:\Program Files\Common Files\VST3`

### Standalone
Run the `.exe` directly or move it to a folder of your choice.
