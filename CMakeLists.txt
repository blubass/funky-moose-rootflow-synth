
cmake_minimum_required(VERSION 3.22)
project(RootFlow VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization and Universal Binaries for macOS
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
endif()

find_package(JUCE CONFIG REQUIRED)

juce_add_plugin(RootFlow
    COMPANY_NAME "Funky Moose Audio"
    BUNDLE_ID at.uwefelchle.rootflow.synth
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE FMAA
    PLUGIN_CODE RTFL
    PLUGIN_NAME "Funky Moose Rootflow Synth"
    DESCRIPTION "Funky Moose Rootflow Synth"
    SUPPRESS_AU_PLIST_RESOURCE_USAGE TRUE
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Funky Moose Rootflow Synth"
    PLIST_TO_MERGE "${CMAKE_CURRENT_SOURCE_DIR}/Source/Info-App.plist"
    MACOS_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/RootFlow.entitlements"
)

target_compile_definitions(RootFlow PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

juce_generate_juce_header(RootFlow)

target_sources(RootFlow PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/RootFlowVoice.cpp
    Source/RootFlowLookAndFeel.cpp
    Source/UI/EnergyDisplay.cpp
    # NodeSystem and BioSequencerComponent are header-only
)

juce_add_binary_data(RootFlowAssets
    SOURCES
        Assets/wood_texture.png
        Assets/metal_texture.png
        Assets/roots_bg.png
        Assets/roots_overlay.png
        Assets/roots_overlay_soft.png
        Assets/roots_overlay_detail.png
        Assets/moose_logo.png
)

target_link_libraries(RootFlow PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    RootFlowAssets
)
