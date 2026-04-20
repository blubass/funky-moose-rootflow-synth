
cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment target" FORCE)
endif()

project(RootFlow VERSION 1.3.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(ROOTFLOW_COPY_PLUGIN_AFTER_BUILD "Install plugin artifacts after building." ON)
option(ROOTFLOW_BUILD_AU "Build AU on macOS" ON)

# Release only: LTO/IPO
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()

find_package(JUCE CONFIG QUIET)
if(NOT TARGET juce::juce_audio_utils)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
        add_subdirectory(external/JUCE EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR
            "JUCE was not found. Set JUCE_DIR to an installed JUCE package or clone JUCE into external/JUCE.")
    endif()
endif()

set(ROOTFLOW_REQUIRED_ASSETS
    Assets/rootflow_icon.png
    Assets/wood_texture.png
    Assets/metal_texture.png
    Assets/roots_bg.png
    Assets/roots_overlay.png
    Assets/roots_overlay_soft.png
    Assets/roots_overlay_detail.png
    Assets/moose_logo.png
)

foreach(asset IN LISTS ROOTFLOW_REQUIRED_ASSETS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${asset}")
        message(FATAL_ERROR "Missing required asset: ${asset}")
    endif()
endforeach()

set(ROOTFLOW_FORMATS VST3 Standalone)
set(ROOTFLOW_PLATFORM_PLUGIN_ARGS)

if(APPLE AND ROOTFLOW_BUILD_AU)
    list(APPEND ROOTFLOW_FORMATS AU)
    list(APPEND ROOTFLOW_PLATFORM_PLUGIN_ARGS
        PLIST_TO_MERGE "${CMAKE_CURRENT_SOURCE_DIR}/Source/Info-App.plist"
        MACOS_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/RootFlow.entitlements"
    )
endif()


juce_add_plugin(RootFlow
    COMPANY_NAME "Funky Moose Audio"
    BUNDLE_ID at.uwefelchle.rootflow.synth
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    COPY_PLUGIN_AFTER_BUILD ${ROOTFLOW_COPY_PLUGIN_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE FMAA
    PLUGIN_CODE RTFL
    PLUGIN_NAME "Funky Moose Rootflow Synth"
    DESCRIPTION "Funky Moose Rootflow Synth"
    ICON_BIG "Assets/rootflow_icon.png"
    SUPPRESS_AU_PLIST_RESOURCE_USAGE TRUE
    FORMATS ${ROOTFLOW_FORMATS}
    PRODUCT_NAME "Funky Moose Rootflow Synth"
    ${ROOTFLOW_PLATFORM_PLUGIN_ARGS}
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
    Source/RootFlowMutation.cpp
    Source/RootFlowPresets.cpp
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
