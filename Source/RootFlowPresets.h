#pragma once
#include <JuceHeader.h>
#include <array>

namespace RootFlow
{
    struct FactoryPreset
    {
        const char* name;
        std::array<float, 16> values;
    };

    struct FactoryPresetSectionDefinition
    {
        const char* title;
        int startIndex = 0;
        int count = 0;
    };

    constexpr std::array<const char*, 16> factoryPresetValueParameterIDs {
        "sourceDepth", "sourceCore", "sourceAnchor",
        "flowRate", "flowEnergy", "flowTexture",
        "pulseFrequency", "pulseWidth", "pulseEnergy",
        "fieldComplexity", "fieldDepth",
        "instability",
        "radiance", "charge", "discharge",
        "systemMatrix"
    };

    constexpr std::array<float, 16> factoryPresetDefaultValues {
        0.5f, 0.5f, 0.5f,
        0.5f, 0.5f, 0.5f,
        0.5f, 0.5f, 0.5f,
        0.65f, 0.18f,
        0.28f,
        0.0f, 0.0f, 0.0f,
        0.34f
    };

    extern const std::array<FactoryPreset, 86> factoryPresets;
    extern const std::array<FactoryPresetSectionDefinition, 15> factoryPresetSections;
}
