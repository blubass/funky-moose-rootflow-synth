
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/Utils/DesignTokens.h"
#include <cmath>
#include <algorithm>

namespace
{
struct FactoryPreset
{
    const char* name;
    std::array<float, 16> values;
};

struct FactoryPresetMasterSettings
{
    float compressor = 0.0f;
    float mix = 1.0f;
    bool mono = false;
    float monoFreqHz = 80.0f;
};

struct FactoryPresetSectionDefinition
{
    const char* title;
    int startIndex = 0;
    int count = 0;
};

struct SeasonMorph
{
    float spring = 0.0f;
    float summer = 0.0f;
    float autumn = 0.0f;
    float winter = 0.0f;
};

float smoothSeasonBlend(float t) noexcept
{
    const float x = juce::jlimit(0.0f, 1.0f, t);
    return x * x * (3.0f - 2.0f * x);
}

SeasonMorph getSeasonMorph(float value) noexcept
{
    SeasonMorph morph;
    const float x = juce::jlimit(0.0f, 1.0f, value);
    constexpr float segment = 1.0f / 3.0f;

    if (x <= segment)
    {
        const float t = smoothSeasonBlend(x / segment);
        morph.spring = 1.0f - t;
        morph.summer = t;
        return morph;
    }

    if (x <= segment * 2.0f)
    {
        const float t = smoothSeasonBlend((x - segment) / segment);
        morph.summer = 1.0f - t;
        morph.autumn = t;
        return morph;
    }

    const float t = smoothSeasonBlend((x - segment * 2.0f) / segment);
    morph.autumn = 1.0f - t;
    morph.winter = t;
    return morph;
}

constexpr std::array<const char*, 16> factoryPresetValueParameterIDs {
    "rootDepth", "rootSoil", "rootAnchor",
    "sapFlow", "sapVitality", "sapTexture",
    "pulseRate", "pulseBreath", "pulseGrowth",
    "canopy", "atmosphere",
    "instability",
    "bloom", "rain", "sun",
    "ecoSystem"
};

constexpr std::array<const char*, 5> factoryPresetUtilityParameterIDs {
    "sequencerOn", "sequencerRate", "sequencerSteps", "sequencerGate",
    "oscWave"
};

constexpr std::array<const char*, 27> managedParameterIDs {
    "rootDepth", "rootSoil", "rootAnchor",
    "sapFlow", "sapVitality", "sapTexture",
    "pulseRate", "pulseBreath", "pulseGrowth",
    "canopy", "atmosphere",
    "instability",
    "bloom", "rain", "sun",
    "ecoSystem",
    "sequencerOn", "sequencerRate", "sequencerSteps", "sequencerGate",
    "oscWave",
    "masterVolume", "masterCompressor", "masterMix", "monoMakerToggle", "monoMakerFreq",
    "evolution"
};

constexpr std::array<const char*, 24> mutationParameterIDs {
    "rootDepth", "rootSoil", "rootAnchor",
    "sapFlow", "sapVitality", "sapTexture",
    "pulseRate", "pulseBreath", "pulseGrowth",
    "canopy", "atmosphere",
    "instability",
    "bloom", "rain", "sun",
    "ecoSystem",
    "sequencerOn", "sequencerRate", "sequencerSteps", "sequencerGate",
    "oscWave",
    "masterCompressor", "masterMix", "monoMakerFreq"
};

const std::array<FactoryPreset, 86> factoryPresets {{
    // --- BIO-SIGNATURE PRESETS ---
    { "NEURAL MYCELIUM",      { 0.30f, 0.45f, 0.20f, 0.85f, 0.60f, 0.40f, 0.25f, 0.50f, 0.35f, 0.70f, 0.80f, 0.15f, 0.20f, 0.05f, 0.10f, 0.50f } },
    { "SPORE BURST",         { 0.20f, 0.10f, 0.60f, 0.40f, 0.90f, 0.85f, 0.95f, 0.30f, 0.75f, 0.40f, 0.10f, 0.65f, 0.50f, 0.80f, 0.20f, 0.30f } },
    { "BIO-LUMINESCENT LEAD",{ 0.25f, 0.30f, 0.15f, 0.55f, 0.88f, 0.20f, 0.35f, 0.60f, 0.45f, 0.75f, 0.35f, 0.10f, 0.70f, 0.08f, 0.15f, 0.18f } },
    { "DEEP ROOT PULSE",     { 0.90f, 0.75f, 0.80f, 0.15f, 0.20f, 0.55f, 0.12f, 0.18f, 0.08f, 0.45f, 0.12f, 0.30f, 0.05f, 0.10f, 0.65f, 0.88f } },
    { "MEMBRANE DRIFT",      { 0.48f, 0.55f, 0.35f, 0.65f, 0.42f, 0.30f, 0.28f, 0.72f, 0.38f, 0.88f, 0.42f, 0.08f, 0.15f, 0.55f, 0.30f, 0.72f } },
    { "CELL DIVISION",       { 0.55f, 0.40f, 0.30f, 0.70f, 0.75f, 0.60f, 0.88f, 0.65f, 0.80f, 0.62f, 0.22f, 0.48f, 0.60f, 0.15f, 0.18f, 0.25f } },
    { "FOSSIL RESONANCE",    { 0.85f, 0.78f, 0.82f, 0.25f, 0.30f, 0.70f, 0.10f, 0.15f, 0.12f, 0.38f, 0.14f, 0.22f, 0.08f, 0.12f, 0.72f, 0.42f } },
    { "BIOLUMINESCENCE",     { 0.22f, 0.18f, 0.12f, 0.78f, 0.92f, 0.25f, 0.48f, 0.88f, 0.55f, 0.80f, 0.50f, 0.12f, 0.85f, 0.06f, 0.22f, 0.16f } },

    // Organic Stasis
    { "DEEP MYCELIUM",  { 0.36f, 0.28f, 0.16f, 0.62f, 0.78f, 0.40f, 0.30f, 0.34f, 0.28f, 0.68f, 0.26f, 0.14f, 0.42f, 0.20f, 0.10f, 0.08f } },

    // Motion
    { "FOREST DRIFT",  { 0.60f, 0.52f, 0.34f, 0.74f, 0.70f, 0.54f, 0.36f, 0.48f, 0.54f, 0.76f, 0.28f, 0.56f, 0.22f, 0.34f, 0.26f, 0.44f } },
    { "BROOK CANOPY",  { 0.50f, 0.42f, 0.26f, 0.56f, 0.50f, 0.52f, 0.26f, 0.36f, 0.28f, 0.92f, 0.30f, 0.18f, 0.12f, 0.70f, 0.12f, 0.66f } },
    { "CANOPY DRIP",   { 0.58f, 0.46f, 0.30f, 0.62f, 0.54f, 0.68f, 0.30f, 0.42f, 0.38f, 0.90f, 0.34f, 0.30f, 0.14f, 0.82f, 0.16f, 0.72f } },
    { "PULSE BLOOM",   { 0.46f, 0.36f, 0.18f, 0.58f, 0.70f, 0.36f, 0.84f, 0.70f, 0.82f, 0.72f, 0.20f, 0.40f, 0.66f, 0.08f, 0.22f, 0.18f } },
    { "AMBER RAIN",    { 0.78f, 0.72f, 0.62f, 0.42f, 0.46f, 0.56f, 0.22f, 0.40f, 0.34f, 0.68f, 0.30f, 0.44f, 0.10f, 0.72f, 0.34f, 0.78f } },
    { "RIVER VINES",   { 0.48f, 0.40f, 0.24f, 0.60f, 0.58f, 0.42f, 0.64f, 0.52f, 0.46f, 0.74f, 0.24f, 0.18f, 0.18f, 0.44f, 0.10f, 0.58f } },
    { "WIND LATTICE",  { 0.42f, 0.30f, 0.18f, 0.66f, 0.72f, 0.46f, 0.72f, 0.62f, 0.58f, 0.84f, 0.30f, 0.24f, 0.26f, 0.24f, 0.12f, 0.24f } },
    { "TRAIL SPORES",  { 0.44f, 0.34f, 0.20f, 0.62f, 0.76f, 0.50f, 0.78f, 0.66f, 0.70f, 0.70f, 0.26f, 0.32f, 0.58f, 0.14f, 0.12f, 0.28f } },

    // Deep
    { "ROOT CHOIR",    { 0.82f, 0.78f, 0.74f, 0.34f, 0.40f, 0.20f, 0.18f, 0.30f, 0.20f, 0.48f, 0.16f, 0.12f, 0.08f, 0.18f, 0.56f, 0.18f } },
    { "MYCEL DOME",    { 0.70f, 0.60f, 0.48f, 0.40f, 0.44f, 0.32f, 0.26f, 0.48f, 0.30f, 0.62f, 0.22f, 0.20f, 0.06f, 0.16f, 0.86f, 0.12f } },
    { "NIGHT ROOTS",   { 0.88f, 0.84f, 0.80f, 0.26f, 0.30f, 0.24f, 0.14f, 0.28f, 0.18f, 0.40f, 0.18f, 0.16f, 0.00f, 0.12f, 0.74f, 0.10f } },
    { "LOAM CATHEDRAL",{ 0.84f, 0.82f, 0.76f, 0.30f, 0.34f, 0.20f, 0.16f, 0.24f, 0.16f, 0.52f, 0.18f, 0.10f, 0.06f, 0.14f, 0.64f, 0.24f } },
    { "HUMUS VEIL",    { 0.76f, 0.70f, 0.60f, 0.36f, 0.38f, 0.26f, 0.20f, 0.30f, 0.20f, 0.58f, 0.22f, 0.16f, 0.04f, 0.12f, 0.56f, 0.52f } },
    { "CAVE BLOOM",    { 0.72f, 0.66f, 0.58f, 0.40f, 0.46f, 0.30f, 0.24f, 0.34f, 0.26f, 0.46f, 0.20f, 0.18f, 0.34f, 0.08f, 0.58f, 0.30f } },

    // Wild
    { "SPORE HALO",    { 0.42f, 0.34f, 0.18f, 0.70f, 0.84f, 0.66f, 0.44f, 0.38f, 0.46f, 0.74f, 0.40f, 0.34f, 0.78f, 0.10f, 0.18f, 0.16f } },
    { "PETAL GLASS",   { 0.34f, 0.20f, 0.12f, 0.74f, 0.88f, 0.56f, 0.40f, 0.34f, 0.42f, 0.70f, 0.30f, 0.18f, 0.88f, 0.06f, 0.14f, 0.18f } },
    { "EMBER SPORES",  { 0.62f, 0.52f, 0.36f, 0.50f, 0.62f, 0.46f, 0.36f, 0.42f, 0.40f, 0.68f, 0.22f, 0.24f, 0.54f, 0.22f, 0.44f, 0.74f } },
    { "LUNAR DRIP",    { 0.66f, 0.54f, 0.44f, 0.42f, 0.40f, 0.34f, 0.22f, 0.38f, 0.24f, 0.84f, 0.28f, 0.18f, 0.10f, 0.78f, 0.58f, 0.96f } },
    { "THORN STATIC",  { 0.52f, 0.44f, 0.28f, 0.68f, 0.80f, 0.62f, 0.56f, 0.46f, 0.52f, 0.72f, 0.34f, 0.74f, 0.82f, 0.08f, 0.08f, 0.20f } },
    { "FERAL CANOPY",  { 0.60f, 0.48f, 0.30f, 0.54f, 0.58f, 0.48f, 0.38f, 0.44f, 0.30f, 0.94f, 0.28f, 0.52f, 0.18f, 0.62f, 0.18f, 0.84f } },
    { "SOLAR SHED",    { 0.46f, 0.34f, 0.18f, 0.70f, 0.86f, 0.58f, 0.42f, 0.36f, 0.44f, 0.68f, 0.36f, 0.40f, 0.72f, 0.08f, 0.48f, 0.10f } },

    // --- BIO-PROFILES (Newly Integrated) ---
    { "Ancient Oak      ", { 0.94f, 0.28f, 0.88f, 0.12f, 0.10f, 0.18f, 0.08f, 0.14f, 0.06f, 0.65f, 0.18f, 0.28f, 0.22f, 0.12f, 0.76f, 0.68f } },
    { "Mimosa Pudica    ", { 0.18f, 0.42f, 0.12f, 0.48f, 0.62f, 0.12f, 0.92f, 0.84f, 0.78f, 0.65f, 0.18f, 0.28f, 0.72f, 0.38f, 0.32f, 0.82f } },
    { "Wild Ivy         ", { 0.38f, 0.82f, 0.44f, 0.82f, 0.72f, 0.94f, 0.42f, 0.34f, 0.52f, 0.65f, 0.18f, 0.28f, 0.32f, 0.88f, 0.42f, 0.72f } },
    { "Redwood Titan    ", { 1.00f, 0.20f, 0.90f, 0.10f, 0.00f, 0.40f, 0.00f, 0.10f, 0.20f, 0.65f, 0.18f, 0.28f, 0.20f, 0.20f, 0.90f, 0.90f } },
    { "Weeping Willow   ", { 0.60f, 0.40f, 0.70f, 0.70f, 0.20f, 0.50f, 0.30f, 0.40f, 0.30f, 0.65f, 0.18f, 0.28f, 0.50f, 0.60f, 0.60f, 0.80f } },
    { "Baobab Core      ", { 0.80f, 0.10f, 0.90f, 0.00f, 0.10f, 0.70f, 0.10f, 0.00f, 0.10f, 0.65f, 0.18f, 0.28f, 0.10f, 0.00f, 0.40f, 0.50f } },
    { "Banyan Network   ", { 0.90f, 0.70f, 0.80f, 0.40f, 0.30f, 0.60f, 0.20f, 0.30f, 0.20f, 0.65f, 0.18f, 0.28f, 0.40f, 0.50f, 0.70f, 0.80f } },
    { "Ironwood Solid   ", { 1.00f, 0.00f, 1.00f, 0.10f, 0.00f, 0.90f, 0.00f, 0.00f, 0.00f, 0.65f, 0.18f, 0.28f, 0.00f, 0.10f, 0.20f, 0.30f } },
    { "Petrified Log    ", { 0.90f, 0.00f, 1.00f, 0.00f, 0.00f, 1.00f, 0.00f, 0.00f, 0.00f, 0.65f, 0.18f, 0.28f, 0.10f, 0.10f, 0.80f, 0.40f } },
    { "Poison Ivy       ", { 0.30f, 0.70f, 0.40f, 0.80f, 0.80f, 0.90f, 0.70f, 0.60f, 0.80f, 0.65f, 0.18f, 0.28f, 0.50f, 0.60f, 0.30f, 0.70f } },
    { "Strangler Fig    ", { 0.70f, 0.90f, 0.80f, 0.60f, 0.70f, 0.80f, 0.40f, 0.50f, 0.60f, 0.65f, 0.18f, 0.28f, 0.30f, 0.70f, 0.40f, 0.80f } },
    { "Morning Glory    ", { 0.20f, 0.40f, 0.20f, 0.90f, 0.60f, 0.30f, 0.60f, 0.50f, 0.70f, 0.65f, 0.18f, 0.28f, 0.80f, 0.20f, 0.70f, 0.90f } },
    { "Kudzu Swarm      ", { 0.40f, 1.00f, 0.50f, 1.00f, 1.00f, 0.90f, 0.80f, 0.70f, 1.00f, 0.65f, 0.18f, 0.28f, 0.60f, 0.90f, 0.50f, 0.90f } },
    { "Thorny Bramble   ", { 0.50f, 0.60f, 0.60f, 0.40f, 0.80f, 1.00f, 0.70f, 0.30f, 0.40f, 0.65f, 0.18f, 0.28f, 0.20f, 0.40f, 0.30f, 0.60f } },
    { "Jungle Canopy    ", { 0.60f, 0.50f, 0.40f, 0.70f, 0.50f, 0.50f, 0.50f, 0.60f, 0.50f, 0.65f, 0.18f, 0.28f, 0.70f, 0.80f, 0.90f, 1.00f } },
    { "Neon Fern        ", { 0.20f, 0.30f, 0.20f, 0.60f, 0.50f, 0.20f, 0.60f, 0.80f, 0.70f, 0.65f, 0.18f, 0.28f, 0.90f, 0.30f, 0.80f, 0.90f } },
    { "Ghost Fungus     ", { 0.10f, 0.80f, 0.10f, 0.30f, 0.90f, 0.50f, 0.20f, 0.90f, 0.40f, 0.65f, 0.18f, 0.28f, 1.00f, 0.80f, 0.90f, 1.00f } },
    { "Glimmer Moss     ", { 0.00f, 0.20f, 0.30f, 0.40f, 0.40f, 0.10f, 0.50f, 0.70f, 0.30f, 0.65f, 0.18f, 0.28f, 0.80f, 0.20f, 0.60f, 0.80f } },
    { "Lunar Lotus      ", { 0.30f, 0.10f, 0.20f, 0.50f, 0.20f, 0.00f, 0.30f, 1.00f, 0.50f, 0.65f, 0.18f, 0.28f, 0.70f, 0.10f, 1.00f, 0.90f } },
    { "Foxfire Wood     ", { 0.40f, 0.60f, 0.50f, 0.70f, 0.80f, 0.40f, 0.40f, 0.60f, 0.60f, 0.65f, 0.18f, 0.28f, 0.80f, 0.50f, 0.70f, 0.80f } },
    { "Avatar Tree      ", { 0.80f, 0.30f, 0.70f, 0.80f, 0.50f, 0.30f, 0.50f, 0.80f, 0.80f, 0.65f, 0.18f, 0.28f, 0.90f, 0.60f, 1.00f, 1.00f } },
    { "Mycelium Web     ", { 0.70f, 0.90f, 0.30f, 0.90f, 1.00f, 0.80f, 0.20f, 0.40f, 0.10f, 0.65f, 0.18f, 0.28f, 0.40f, 0.90f, 0.50f, 0.80f } },
    { "Puffball Cloud   ", { 0.10f, 0.20f, 0.10f, 0.50f, 0.80f, 0.90f, 1.00f, 0.20f, 0.90f, 0.65f, 0.18f, 0.28f, 0.50f, 0.80f, 0.30f, 0.70f } },
    { "Slime Mold       ", { 0.20f, 1.00f, 0.20f, 1.00f, 0.50f, 0.60f, 0.10f, 0.10f, 0.30f, 0.65f, 0.18f, 0.28f, 0.80f, 0.50f, 0.20f, 0.60f } },
    { "Truffle Dark     ", { 0.90f, 0.80f, 0.90f, 0.20f, 0.30f, 0.80f, 0.10f, 0.20f, 0.00f, 0.65f, 0.18f, 0.28f, 0.10f, 0.30f, 0.10f, 0.40f } },
    { "Cordyceps Hive   ", { 0.50f, 0.90f, 0.60f, 0.80f, 1.00f, 0.90f, 0.90f, 0.80f, 0.80f, 0.65f, 0.18f, 0.28f, 0.60f, 0.90f, 0.40f, 0.90f } },
    { "Kelp Forest      ", { 0.60f, 0.80f, 0.40f, 0.90f, 0.60f, 0.20f, 0.40f, 0.70f, 0.50f, 0.65f, 0.18f, 0.28f, 0.70f, 0.40f, 0.90f, 0.90f } },
    { "Water Lily       ", { 0.20f, 0.30f, 0.10f, 0.60f, 0.30f, 0.00f, 0.50f, 0.80f, 0.40f, 0.65f, 0.18f, 0.28f, 0.50f, 0.20f, 0.80f, 0.80f } },
    { "Coral Polyps     ", { 0.40f, 0.50f, 0.80f, 0.70f, 0.90f, 0.50f, 0.80f, 0.60f, 0.70f, 0.65f, 0.18f, 0.28f, 0.80f, 0.70f, 0.60f, 0.90f } },
    { "Seaweed Drift    ", { 0.30f, 0.70f, 0.20f, 1.00f, 0.40f, 0.10f, 0.20f, 0.90f, 0.30f, 0.65f, 0.18f, 0.28f, 0.90f, 0.10f, 0.70f, 0.80f } },
    { "Abyssal Bloom    ", { 0.80f, 0.90f, 0.60f, 0.50f, 0.80f, 0.30f, 0.30f, 0.80f, 0.60f, 0.65f, 0.18f, 0.28f, 1.00f, 0.80f, 1.00f, 1.00f } },
    { "Saguaro Cactus   ", { 0.80f, 0.10f, 0.90f, 0.00f, 0.00f, 0.80f, 0.10f, 0.10f, 0.10f, 0.65f, 0.18f, 0.28f, 0.00f, 0.00f, 0.50f, 0.30f } },
    { "Tumbleweed       ", { 0.00f, 0.80f, 0.00f, 0.20f, 0.70f, 0.90f, 0.60f, 0.20f, 0.40f, 0.65f, 0.18f, 0.28f, 0.20f, 0.40f, 0.20f, 0.40f } },
    { "Resurrection     ", { 0.40f, 0.60f, 0.50f, 0.10f, 0.90f, 0.80f, 0.90f, 0.50f, 1.00f, 0.65f, 0.18f, 0.28f, 0.40f, 0.20f, 0.60f, 0.60f } },
    { "Aloe Vera        ", { 0.50f, 0.20f, 0.40f, 0.40f, 0.10f, 0.30f, 0.20f, 0.40f, 0.30f, 0.65f, 0.18f, 0.28f, 0.20f, 0.10f, 0.40f, 0.50f } },
    { "Scorched Earth   ", { 0.90f, 0.90f, 0.90f, 0.00f, 0.00f, 1.00f, 0.00f, 0.00f, 0.00f, 0.65f, 0.18f, 0.28f, 0.00f, 0.00f, 0.10f, 0.20f } },
    { "Venus Flytrap    ", { 0.30f, 0.40f, 0.50f, 0.20f, 0.80f, 0.40f, 1.00f, 0.10f, 1.00f, 0.65f, 0.18f, 0.28f, 0.20f, 0.10f, 0.30f, 0.60f } },
    { "Pitcher Plant    ", { 0.40f, 0.30f, 0.60f, 0.60f, 0.50f, 0.70f, 0.80f, 0.40f, 0.60f, 0.65f, 0.18f, 0.28f, 0.40f, 0.50f, 0.60f, 0.70f } },
    { "Sundew Drops     ", { 0.20f, 0.50f, 0.30f, 0.80f, 0.90f, 0.20f, 0.70f, 0.70f, 0.80f, 0.65f, 0.18f, 0.28f, 0.80f, 0.40f, 0.50f, 0.80f } },
    { "Cobra Lily       ", { 0.50f, 0.60f, 0.70f, 0.50f, 0.70f, 0.60f, 0.90f, 0.50f, 0.90f, 0.65f, 0.18f, 0.28f, 0.30f, 0.30f, 0.40f, 0.70f } },
    { "Xenoflora 1      ", { 0.80f, 1.00f, 0.20f, 1.00f, 1.00f, 1.00f, 1.00f, 0.10f, 0.80f, 0.65f, 0.18f, 0.28f, 0.90f, 1.00f, 0.20f, 1.00f } },
    { "Glass Orchid     ", { 0.10f, 0.00f, 0.10f, 0.20f, 0.10f, 0.00f, 0.40f, 0.90f, 0.20f, 0.65f, 0.18f, 0.28f, 1.00f, 0.20f, 1.00f, 0.90f } },
    { "Quantum Spore    ", { 0.50f, 0.50f, 0.50f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 0.65f, 0.18f, 0.28f, 1.00f, 1.00f, 1.00f, 1.00f } },
    { "Crystalline      ", { 0.20f, 0.10f, 0.30f, 0.10f, 0.00f, 0.10f, 0.20f, 0.80f, 0.30f, 0.65f, 0.18f, 0.28f, 0.90f, 0.50f, 0.90f, 0.80f } },
    { "Plasma Root      ", { 1.00f, 0.90f, 0.90f, 0.80f, 1.00f, 0.80f, 0.90f, 0.90f, 1.00f, 0.65f, 0.18f, 0.28f, 0.70f, 0.60f, 0.80f, 0.90f } },
    { "Cyber Bonsai     ", { 0.50f, 0.00f, 0.60f, 0.30f, 0.20f, 0.10f, 0.80f, 0.40f, 0.50f, 0.65f, 0.18f, 0.28f, 0.40f, 0.20f, 0.50f, 0.60f } },
    { "Fiber Optic      ", { 0.10f, 0.10f, 0.20f, 0.80f, 0.40f, 0.00f, 0.60f, 0.90f, 0.40f, 0.65f, 0.18f, 0.28f, 0.90f, 0.30f, 0.80f, 0.90f } },
    { "Wire Weed        ", { 0.30f, 0.80f, 0.40f, 0.90f, 0.90f, 1.00f, 0.70f, 0.20f, 0.60f, 0.65f, 0.18f, 0.28f, 0.50f, 0.80f, 0.30f, 0.70f } },
    { "Silicon Petal    ", { 0.20f, 0.00f, 0.30f, 0.40f, 0.10f, 0.00f, 0.40f, 0.70f, 0.30f, 0.65f, 0.18f, 0.28f, 0.70f, 0.20f, 0.70f, 0.80f } },
    { "Robo Sprout      ", { 0.40f, 0.20f, 0.40f, 0.50f, 0.30f, 0.20f, 1.00f, 0.50f, 0.80f, 0.65f, 0.18f, 0.28f, 0.30f, 0.30f, 0.40f, 0.50f } },

    // --- REAL BOTANICS (True Plant Acoustics) ---
    { "Xylem Cavitation", { 0.10f, 0.20f, 0.80f, 0.10f, 0.40f, 0.90f, 0.80f, 0.90f, 0.50f, 0.80f, 0.40f, 0.90f, 0.20f, 0.90f, 0.10f, 0.80f } },
    { "Stomata Breath",   { 0.20f, 0.30f, 0.10f, 0.40f, 0.20f, 0.10f, 0.10f, 1.00f, 0.20f, 1.00f, 0.90f, 0.20f, 0.80f, 0.10f, 0.50f, 0.40f } },
    { "Root Expansion",   { 1.00f, 0.90f, 1.00f, 0.20f, 0.30f, 0.60f, 0.05f, 0.40f, 0.80f, 0.00f, 0.30f, 0.60f, 0.10f, 0.30f, 0.10f, 0.50f } },
    { "Phloem Rush",      { 0.50f, 0.50f, 0.40f, 1.00f, 0.70f, 0.50f, 0.60f, 0.30f, 0.50f, 0.40f, 0.60f, 0.10f, 0.50f, 0.60f, 0.40f, 0.20f } },
    { "Osmotic Shift",    { 0.70f, 0.40f, 0.60f, 0.50f, 0.80f, 0.30f, 0.20f, 0.80f, 0.90f, 0.60f, 0.50f, 0.40f, 0.60f, 0.20f, 0.70f, 0.30f } },
    { "Photosynthesis",   { 0.10f, 0.10f, 0.20f, 0.80f, 1.00f, 0.20f, 0.90f, 0.50f, 0.40f, 0.90f, 0.80f, 0.30f, 0.70f, 0.00f, 1.00f, 0.50f } }
}};

constexpr std::array<FactoryPresetSectionDefinition, 15> factoryPresetSections {{
    { "BIO-SIGNATURE",  0, 8 },   // The new flagship presets
    { "ORGANIC STASIS", 8, 1 },   // Deep Mycelium (the old start)
    { "MOTION",        9, 8 },
    { "DEEP",         17, 6 },
    { "WILD",         23, 7 },
    { "BIO: LEGACY",  30, 3 },
    { "BIO: GIANTS",  33, 6 },
    { "BIO: CREEPERS",39, 6 },
    { "BIO: LUMINA",  45, 6 },
    { "BIO: SPORES",  51, 5 },
    { "BIO: AQUATIC", 56, 5 },
    { "BIO: DESERT",  61, 5 },
    { "BIO: HUNTERS", 66, 4 },
    { "BIO: XENO",    70, 10 },
    { "REAL BOTANICS", 80, 6 }
}};

constexpr auto midiBindingsTag = "MidiBindings";
constexpr auto midiMappingStateTag = "MidiMappingState";
constexpr auto midiBindingTag = "Binding";
constexpr auto userPresetsTag = "UserPresets";
constexpr auto userPresetTag = "UserPreset";
constexpr auto presetIndexProperty = "factoryPresetIndex";
constexpr auto hoverEffectsEnabledProperty = "hoverEffectsEnabled";
constexpr auto idleEffectsEnabledProperty = "idleEffectsEnabled";
constexpr auto popupOverlaysEnabledProperty = "popupOverlaysEnabled";
constexpr auto midiMappingVersionProperty = "version";
constexpr auto midiMappingModeProperty = "mode";
constexpr int midiMappingSchemaVersion = 1;
constexpr auto midiMappingModeDefault = "default";
constexpr auto midiMappingModeEmpty = "empty";
constexpr auto midiMappingModeCustom = "custom";
constexpr int silenceWatchdogTriggerBlocks = 48;
constexpr float silenceWatchdogPreFxThreshold = 0.0010f;
constexpr float silenceWatchdogPostFxThreshold = 0.00015f;
constexpr int runawayWatchdogTriggerBlocks = 8;
constexpr float runawayWatchdogPreFxThreshold = 0.010f;
constexpr float runawayWatchdogOutputThreshold = 0.30f;
constexpr float postFxDcBlockPole = 0.995f;
constexpr float outputDcBlockPole = 0.995f;
constexpr float outputSafetyLimit = 0.98f;
constexpr int postFxSafetyBypassDurationBlocks = 256;

constexpr auto nodeSystemTag = "NodeSystem";
constexpr auto nodeTag = "Node";
constexpr auto connectionTag = "Connection";

bool presetNameContainsAny(juce::String name, std::initializer_list<const char*> needles)
{
    name = name.trim().toUpperCase();
    for (const auto* needle : needles)
    {
        if (name.contains(juce::String(needle).trim().toUpperCase()))
            return true;
    }

    return false;
}

FactoryPresetMasterSettings getFactoryPresetMasterSettings(int index, const FactoryPreset& preset) noexcept
{
    const auto name = juce::String(preset.name).trim();
    const auto& values = preset.values;

    const float rootWeight = (values[0] + values[1] + values[2]) / 3.0f;
    const float motionWeight = (values[6] + values[7] + values[8]) / 3.0f;
    const float airWeight = (values[9] + values[10] + values[15]) / 3.0f;
    const float fxWeight = (values[12] + values[13] + values[14]) / 3.0f;
    const float instability = values[11];

    FactoryPresetMasterSettings settings;
    settings.compressor = juce::jlimit(0.05f, 0.58f,
                                       0.06f
                                       + rootWeight * 0.14f
                                       + motionWeight * 0.10f
                                       + instability * 0.20f
                                       + fxWeight * 0.05f);

    settings.mix = juce::jlimit(0.76f, 1.0f,
                                0.90f
                                + airWeight * 0.07f
                                + fxWeight * 0.06f
                                - rootWeight * 0.08f
                                - instability * 0.05f);

    settings.monoFreqHz = juce::jlimit(42.0f, 220.0f,
                                       52.0f
                                       + rootWeight * 90.0f
                                       + motionWeight * 28.0f
                                       + instability * 18.0f);

    settings.mono = rootWeight > 0.76f
                 || ((index >= 17 && index <= 22) && rootWeight > 0.56f)
                 || (index >= 61 && index <= 65)
                 || presetNameContainsAny(name, { "ROOT", "MYCEL", "CAVE", "LOAM", "HUMUS", "TRUFFLE", "SAGUARO", "CAVITATION" });

    if (presetNameContainsAny(name, { "LEAD", "BURST", "PULSE", "STATIC", "SWARM", "CORDYCEPS" }))
        settings.compressor = juce::jlimit(0.05f, 0.64f, settings.compressor + 0.08f);

    if (presetNameContainsAny(name, { "DRIFT", "WIND", "LOTUS", "LILY", "BROOK", "RIVER", "WILLOW", "LUNAR", "GLASS" }))
        settings.mix = juce::jlimit(0.76f, 1.0f, settings.mix + 0.04f);

    if (settings.mono)
    {
        settings.mix = juce::jlimit(0.72f, 0.94f, settings.mix - 0.05f);
        settings.compressor = juce::jlimit(0.08f, 0.68f, settings.compressor + 0.05f);
    }

    if (index >= 80)
    {
        settings.compressor = juce::jlimit(0.06f, 0.48f, settings.compressor + 0.04f);
        settings.mix = juce::jlimit(0.82f, 1.0f, settings.mix - 0.03f);
    }

    return settings;
}

float onePoleCoeffFromCutoff(float cutoffHz, float sampleRate) noexcept
{
    const float safeSampleRate = juce::jmax(1.0f, sampleRate);
    const float clampedCutoff = juce::jlimit(20.0f, safeSampleRate * 0.45f, cutoffHz);
    return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * clampedCutoff / safeSampleRate);
}

float clampStateVariableCutoff(float cutoffHz, double sampleRate) noexcept
{
    const auto safeSampleRate = juce::jmax(1.0, sampleRate);
    const auto maxCutoff = juce::jmax(10.0f, (float) safeSampleRate * 0.49f);
    const auto minCutoff = juce::jmin(20.0f, maxCutoff * 0.5f);
    return juce::jlimit(minCutoff, maxCutoff, cutoffHz);
}

float emphasizeMacroResponse(float value, float curve = 0.32f) noexcept
{
    const float x = juce::jlimit(0.0f, 1.0f, value);
    const float bipolar = x * 2.0f - 1.0f;
    const float shaped = std::copysign(std::pow(std::abs(bipolar), juce::jmax(0.1f, curve)), bipolar);
    return juce::jlimit(0.0f, 1.0f, 0.5f + 0.5f * shaped);
}

float emphasizeFxResponse(float value, float curve = 0.20f) noexcept
{
    return std::pow(juce::jlimit(0.0f, 1.0f, value), juce::jmax(0.01f, curve));
}

float shapeAtmosphereResponse(float value) noexcept
{
    return std::pow(emphasizeMacroResponse(value, 0.46f), 1.08f);
}

float applySeasonalFxScale(float base,
                           const SeasonMorph& seasonMorph,
                           float springScale,
                           float summerScale,
                           float autumnScale,
                           float winterScale,
                           float curve) noexcept
{
    const float shapedBase = emphasizeFxResponse(base, curve);
    const float seasonScale = juce::jlimit(0.35f, 1.85f,
                                           1.0f
                                           + seasonMorph.spring * springScale
                                           + seasonMorph.summer * summerScale
                                           + seasonMorph.autumn * autumnScale
                                           + seasonMorph.winter * winterScale);
    return juce::jlimit(0.0f, 1.0f, shapedBase * seasonScale);
}

float sanitizeOutputSample(float sample, float limit = 1.25f) noexcept
{
    if (! std::isfinite(sample))
        return 0.0f;

    return juce::jlimit(-limit, limit, sample);
}
}

RootFlowAudioProcessor::RootFlowAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     ),
      tree(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (size_t i = 0; i < managedParameterIDs.size(); ++i)
    {
        mappedParameters[i] = tree.getParameter(managedParameterIDs[i]);
        pendingMappedParameterValues[i].store(mappedParameters[i] != nullptr ? mappedParameters[i]->getValue() : 0.0f,
                                              std::memory_order_relaxed);
        pendingMappedParameterDirty[i].store(0, std::memory_order_relaxed);
        mappedParameterGestureActive[i] = false;
        mappedParameterLastUpdateMs[i] = 0;
    }

    resetMidiExpressionState();
    resetToDefaultMpkMiniMappings();

    // Default Bio-Sequencer Pattern
    for (int i = 0; i < 16; ++i)
    {
        sequencerSteps[i].active = (i % 2 == 0); // Trigger every other step
        sequencerSteps[i].velocity = 0.6f + (i % 4 == 0 ? 0.3f : 0.0f);
    }

    resetSequencer();

    nodeSystem.initialise(tree);

    for (const auto* paramID : managedParameterIDs)
        tree.addParameterListener(paramID, this);

    startTimer(33);
}

RootFlowAudioProcessor::~RootFlowAudioProcessor()
{
    stopTimer();

    for (const auto* paramID : managedParameterIDs)
        tree.removeParameterListener(paramID, this);

    clearPendingMappedParameterChanges();
}

juce::AudioProcessorValueTreeState::ParameterLayout RootFlowAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addParam = [&](const juce::String& id, const juce::String& name, float defaultVal) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), name, 0.0f, 1.0f, defaultVal));
    };

    addParam("rootDepth", "Root Depth", 0.5f);
    addParam("rootSoil", "Root Soil", 0.5f);
    addParam("rootAnchor", "Root Anchor", 0.5f);
    addParam("sapFlow", "Sap Flow", 0.5f);
    addParam("sapVitality", "Sap Vitality", 0.5f);
    addParam("sapTexture", "Sap Texture", 0.5f);
    addParam("pulseRate", "Pulse Rate", 0.5f);
    addParam("pulseBreath", "Pulse Breath", 0.5f);
    addParam("pulseGrowth", "Pulse Growth", 0.5f);
    addParam("canopy", "Canopy", 0.65f);
    addParam("atmosphere", "Atmosphere", 0.18f);
    addParam("instability", "Instability", 0.28f);
    addParam("bloom", "Bloom Glow", 0.0f);
    addParam("rain", "Rain Density", 0.0f);
    addParam("sun", "Sun Flare", 0.0f);
    addParam("ecoSystem", "Seasons", 0.34f);

    // Bio-Sequencer
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("sequencerOn", 1), "Sequencer On", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("sequencerRate", 1), "Seq Rate",
                                                                  juce::StringArray { "1/4", "1/8", "1/16", "1/32" }, 2));
    params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("sequencerSteps", 1), "Seq Steps", 4, 16, 8));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("sequencerGate", 1), "Seq Gate", 0.1f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("sequencerProbOn", 1), "Seq Probability", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("sequencerJitterOn", 1), "Seq Jitter", true));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("oscWave", 1), "Waveform",
                                                                  juce::StringArray { "Sine", "Saw", "Pulse" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("masterVolume", 1), "Volume",
                                                                 juce::NormalisableRange<float>(-48.0f, +12.0f, 0.1f, 1.5f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("masterCompressor", 1), "Compressor",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("masterMix", 1), "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("monoMakerToggle", 1), "Mono Maker", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("monoMakerFreq", 1), "Mono Freq",
                                                                 juce::NormalisableRange<float>(20.0f, 400.0f, 1.0f, 0.5f), 80.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("evolution", 1), "Evolution", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("oversampling", 1), "Upsampling",
                                                                  juce::StringArray { "1x (Off)", "2x", "4x" }, 0));

    return { params.begin(), params.end() };
}

bool RootFlowAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    const bool outputIsMonoOrStereo = (output == juce::AudioChannelSet::mono()
                                    || output == juce::AudioChannelSet::stereo());

    if (! outputIsMonoOrStereo)
        return false;

    return input.isDisabled();
}

void RootFlowAudioProcessor::prepareToPlay(double sr, int bs)
{
    const auto safeSampleRateBase = sr > 0.0 ? sr : 44100.0;

    prepareOversampling(bs);

    const double safeSampleRate = safeSampleRateBase * (1 << currentOversamplingFactor);
    const int safeBlockSize = juce::jmax(bs, 1) * (1 << currentOversamplingFactor);

    prepareSynth(safeSampleRate, safeBlockSize);
    prepareEffects(safeSampleRate, safeBlockSize, juce::jmax(1, getTotalNumOutputChannels()));
    resetRuntimeState();
}

void RootFlowAudioProcessor::prepareOversampling(int bs)
{
    currentOversamplingFactor = (int)*tree.getRawParameterValue("oversampling");
    if (currentOversamplingFactor > 0)
    {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            juce::jmax(1, getTotalNumOutputChannels()), currentOversamplingFactor, 
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampling->reset();
        oversampling->initProcessing(juce::jmax(bs, 1));
    }
    else
    {
        oversampling.reset();
    }
}

void RootFlowAudioProcessor::prepareSynth(double safeSampleRate, int safeBlockSize)
{
    resetMidiExpressionState();
    synth.clearVoices();
    for (int i = 0; i < 32; ++i)
    {
        auto* voice = new RootFlowVoice();
        voice->setEngine(&modulation);
        voice->setMidiExpressionState(&midiExpressionState);
        voice->setSampleRate(safeSampleRate, safeBlockSize);
        synth.addVoice(voice);
    }

    synth.clearSounds();
    synth.addSound(new RootFlowSound());
    synth.setNoteStealingEnabled(true);
    synth.setCurrentPlaybackSampleRate(safeSampleRate);
    synth.setMinimumRenderingSubdivisionSize(4, false);
}

void RootFlowAudioProcessor::prepareEffects(double safeSampleRate, int safeBlockSize, int numChannels)
{
    drySafetyBuffer.setSize(numChannels, safeBlockSize, false, false, true);
    drySafetyBuffer.clear();

    modulation.prepare(safeSampleRate, safeBlockSize);

    juce::dsp::ProcessSpec fxSpec;
    fxSpec.sampleRate = safeSampleRate;
    fxSpec.maximumBlockSize = (juce::uint32) safeBlockSize;
    fxSpec.numChannels = (juce::uint32) numChannels;

    bloom.prepare(fxSpec);
    rain.prepare(fxSpec);
    sun.prepare(fxSpec);

    juce::dsp::ProcessSpec monoSpec { safeSampleRate, (juce::uint32) safeBlockSize, 1 };
    for (auto& filter : masterToneFilters)
    {
        filter.reset();
        filter.prepare(monoSpec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setResonance(0.52f);
        filter.setCutoffFrequency(clampStateVariableCutoff(7600.0f, safeSampleRate));
    }

    for (auto& filter : monoMakerFilters)
    {
        filter.reset();
        filter.prepare(monoSpec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency(clampStateVariableCutoff(80.0f, safeSampleRate));
    }

    masterDriveSmoothed.reset(safeSampleRate, 0.08);
    masterDriveSmoothed.setCurrentAndTargetValue(1.0f);
    masterToneCutoffSmoothed.reset(safeSampleRate, 0.10);
    masterToneCutoffSmoothed.setCurrentAndTargetValue(7600.0f);
    masterCrossfeedSmoothed.reset(safeSampleRate, 0.10);
    masterCrossfeedSmoothed.setCurrentAndTargetValue(0.0f);
    soilDriveSmoothed.reset(safeSampleRate, 0.12);
    soilDriveSmoothed.setCurrentAndTargetValue(1.0f);
    soilMixSmoothed.reset(safeSampleRate, 0.12);
    soilMixSmoothed.setCurrentAndTargetValue(0.0f);
    masterMixSmoothed.reset(safeSampleRate, 0.10);
    masterMixSmoothed.setCurrentAndTargetValue(tree.getRawParameterValue("masterMix") != nullptr
                                                   ? tree.getRawParameterValue("masterMix")->load()
                                                   : 1.0f);
    monoMakerFreqSmoothed.reset(safeSampleRate, 0.10);
    monoMakerFreqSmoothed.setCurrentAndTargetValue(tree.getRawParameterValue("monoMakerFreq") != nullptr
                                                       ? tree.getRawParameterValue("monoMakerFreq")->load()
                                                       : 80.0f);
    monoMakerEnabled.store(tree.getRawParameterValue("monoMakerToggle") != nullptr
                               && tree.getRawParameterValue("monoMakerToggle")->load() > 0.5f,
                           std::memory_order_relaxed);
    masterVolumeSmoothed.reset(safeSampleRate, 0.05);
    masterVolumeSmoothed.setCurrentAndTargetValue(tree.getRawParameterValue("masterVolume") != nullptr
                                                      ? juce::Decibels::decibelsToGain(tree.getRawParameterValue("masterVolume")->load())
                                                      : 1.0f);
    finalCompressor.prepare(fxSpec);
    soilBodyCutoffSmoothed.reset(safeSampleRate, 0.14);
    soilBodyCutoffSmoothed.setCurrentAndTargetValue(360.0f);
    soilToneCutoffSmoothed.reset(safeSampleRate, 0.14);
    soilToneCutoffSmoothed.setCurrentAndTargetValue(3600.0f);
    testToneLevelSmoothed.reset(safeSampleRate, 0.04);
    testToneLevelSmoothed.setCurrentAndTargetValue(isTestToneEnabled() ? 1.0f : 0.0f);
}

void RootFlowAudioProcessor::resetRuntimeState()
{
    soilBodyState.fill(0.0f);
    soilToneState.fill(0.0f);
    postFxDcBlockPrevInput.fill(0.0f);
    postFxDcBlockPrevOutput.fill(0.0f);
    outputDcBlockPrevInput.fill(0.0f);
    outputDcBlockPrevOutput.fill(0.0f);
    testTonePhase = 0.0;
    outputSilenceWatchdogBlocks = 0;
    outputRunawayWatchdogBlocks = 0;
    postFxSafetyBypassBlocksRemaining = 0;
}

void RootFlowAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    if (handleOversamplingChangeIfNeeded(buffer))
        return;

    clearUnusedOutputChannels(buffer);
    performPendingProcessingStateReset();
    const bool sequencerActive = *tree.getRawParameterValue("sequencerOn") > 0.5f;
    const bool sequencerStateChanged = sequencerActive != sequencerWasEnabled;
    handleIncomingMidiMessages(midiMessages);
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), !sequencerActive);
    updateSequencer(buffer.getNumSamples(), midiMessages);
    if (sequencerStateChanged)
        keyboardState.reset();

    // --- Oversampling Wrapper ---
    juce::dsp::AudioBlock<float> originalBlock(buffer);
    juce::dsp::AudioBlock<float> oversampledBlock;
    juce::dsp::AudioBlock<float>* activeBlockPtr = &originalBlock;

    if (oversampling != nullptr && currentOversamplingFactor > 0)
    {
        oversampledBlock = oversampling->processSamplesUp(originalBlock);
        activeBlockPtr = &oversampledBlock;
    }

    float* pointers[2] = { activeBlockPtr->getChannelPointer(0), activeBlockPtr->getNumChannels() > 1 ? activeBlockPtr->getChannelPointer(1) : nullptr };
    juce::AudioBuffer<float> procBuffer(pointers, (int)activeBlockPtr->getNumChannels(), (int)activeBlockPtr->getNumSamples());

    renderSynthAndVoices(procBuffer, midiMessages);

    applyGlobalFx(procBuffer);

    if (oversampling != nullptr && currentOversamplingFactor > 0)
    {
        juce::dsp::AudioBlock<float> outputBlock(buffer);
        oversampling->processSamplesDown(outputBlock);
    }

    applyOutputSafety(buffer);
    updateMetersAndFft(buffer);
}

juce::StringArray RootFlowAudioProcessor::getFactoryPresetNames() const
{
    juce::StringArray names;
    for (const auto& preset : factoryPresets)
        names.add(preset.name);

    return names;
}

std::vector<RootFlowAudioProcessor::PresetMenuSection> RootFlowAudioProcessor::getFactoryPresetMenuSections() const
{
    std::vector<PresetMenuSection> sections;
    sections.reserve(factoryPresetSections.size());

    for (const auto& section : factoryPresetSections)
        sections.push_back({ juce::String(section.title), section.startIndex, section.count });

    return sections;
}

int RootFlowAudioProcessor::getFactoryPresetCount() const noexcept
{
    return (int) factoryPresets.size();
}

juce::StringArray RootFlowAudioProcessor::getCombinedPresetNames() const
{
    auto names = getFactoryPresetNames();

    const juce::ScopedLock lock(presetStateLock);
    for (const auto& preset : userPresets)
        names.add(preset.name);

    return names;
}

int RootFlowAudioProcessor::getCombinedPresetCount() const noexcept
{
    const juce::ScopedLock lock(presetStateLock);
    return (int) factoryPresets.size() + (int) userPresets.size();
}

int RootFlowAudioProcessor::getCurrentPresetMenuIndex() const noexcept
{
    const int factoryPresetIndex = currentFactoryPresetIndex.load(std::memory_order_relaxed);
    if (factoryPresetIndex >= 0)
        return factoryPresetIndex;

    const int userPresetIndex = currentUserPresetIndex.load(std::memory_order_relaxed);
    if (userPresetIndex >= 0)
        return (int) factoryPresets.size() + userPresetIndex;

    return -1;
}

void RootFlowAudioProcessor::applyFactoryPreset(int index)
{
    if (! juce::isPositiveAndBelow(index, (int) factoryPresets.size()))
        return;

    const auto& preset = factoryPresets[(size_t) index];
    const auto masterSettings = getFactoryPresetMasterSettings(index, preset);
    presetLoadInProgress.fetch_add(1, std::memory_order_acq_rel);

    // Bio-Jitter: each load is subtly unique, like a plant that never
    // grows exactly the same way twice (+/- 1% variation)
    juce::Random bioRng (juce::Time::getCurrentTime().toMilliseconds());
    const size_t numPresetValues = preset.values.size(); // 16

    for (size_t i = 0; i < factoryPresetValueParameterIDs.size(); ++i)
    {
        if (auto* parameter = tree.getParameter(factoryPresetValueParameterIDs[i]))
        {
            const float base = (i < numPresetValues) ? preset.values[i] : 0.0f;
            const float jitter = (i < numPresetValues) ? (bioRng.nextFloat() * 0.02f - 0.01f) : 0.0f;
            parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, base + jitter));
        }
    }

    for (const auto* paramID : factoryPresetUtilityParameterIDs)
    {
        if (auto* parameter = tree.getParameter(paramID))
            parameter->setValueNotifyingHost(0.0f);
    }

    if (auto* parameter = tree.getParameter("masterCompressor"))
        parameter->setValueNotifyingHost(masterSettings.compressor);

    if (auto* parameter = tree.getParameter("masterMix"))
        parameter->setValueNotifyingHost(masterSettings.mix);

    if (auto* parameter = tree.getParameter("monoMakerToggle"))
        parameter->setValueNotifyingHost(masterSettings.mono ? 1.0f : 0.0f);

    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(tree.getParameter("monoMakerFreq")))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(masterSettings.monoFreqHz));

    presetLoadInProgress.fetch_sub(1, std::memory_order_acq_rel);
    clearPendingMappedParameterChanges();
    currentFactoryPresetIndex.store(index, std::memory_order_relaxed);
    currentUserPresetIndex.store(-1, std::memory_order_relaxed);
    currentPresetDirty.store(0, std::memory_order_relaxed);
    requestProcessingStateReset();
}

void RootFlowAudioProcessor::applyCombinedPreset(int menuIndex)
{
    if (menuIndex < 0)
        return;

    const int factoryCount = (int) factoryPresets.size();
    if (menuIndex < factoryCount)
    {
        applyFactoryPreset(menuIndex);
        return;
    }

    const int userIndex = menuIndex - factoryCount;
    juce::ValueTree userPresetState;
    {
        const juce::ScopedLock lock(presetStateLock);
        if (! juce::isPositiveAndBelow(userIndex, (int) userPresets.size()))
            return;

        userPresetState = userPresets[(size_t) userIndex].state.createCopy();
    }

    presetLoadInProgress.fetch_add(1, std::memory_order_acq_rel);
    tree.replaceState(userPresetState);
    presetLoadInProgress.fetch_sub(1, std::memory_order_acq_rel);
    clearPendingMappedParameterChanges();
    currentFactoryPresetIndex.store(-1, std::memory_order_relaxed);
    currentUserPresetIndex.store(userIndex, std::memory_order_relaxed);
    currentPresetDirty.store(0, std::memory_order_relaxed);
    requestProcessingStateReset();
}

void RootFlowAudioProcessor::saveCurrentStateAsUserPreset()
{
    const auto capturedState = captureStateForUserPreset();

    const juce::ScopedLock lock(presetStateLock);
    const int userPresetIndex = currentUserPresetIndex.load(std::memory_order_relaxed);
    if (userPresetIndex >= 0 && juce::isPositiveAndBelow(userPresetIndex, (int) userPresets.size()))
    {
        userPresets[(size_t) userPresetIndex].state = capturedState;
        currentPresetDirty.store(0, std::memory_order_relaxed);
        return;
    }

    UserPreset preset;
    preset.name = "USER " + juce::String(userPresets.size() + 1);
    preset.state = capturedState;
    userPresets.push_back(preset);
    currentFactoryPresetIndex.store(-1, std::memory_order_relaxed);
    currentUserPresetIndex.store((int) userPresets.size() - 1, std::memory_order_relaxed);
    currentPresetDirty.store(0, std::memory_order_relaxed);
}

void RootFlowAudioProcessor::deleteCurrentUserPreset()
{
    const juce::ScopedLock lock(presetStateLock);
    const int userPresetIndex = currentUserPresetIndex.load(std::memory_order_relaxed);
    if (! juce::isPositiveAndBelow(userPresetIndex, (int) userPresets.size()))
        return;

    userPresets.erase(userPresets.begin() + userPresetIndex);
    currentUserPresetIndex.store(-1, std::memory_order_relaxed);
    currentPresetDirty.store(0, std::memory_order_relaxed);
}

void RootFlowAudioProcessor::setMidiLearnArmed(bool shouldArm)
{
    midiLearnArmed.store(shouldArm ? 1 : 0, std::memory_order_release);

    if (! shouldArm)
        midiLearnTargetParameterSlot.store(-1, std::memory_order_release);
}

bool RootFlowAudioProcessor::isMidiLearnArmed() const
{
    return midiLearnArmed.load(std::memory_order_acquire) != 0;
}

void RootFlowAudioProcessor::setMidiLearnTargetParameterID(const juce::String& paramID)
{
    midiLearnTargetParameterSlot.store(getMappedParameterSlotForID(paramID), std::memory_order_release);
    midiLearnArmed.store(1, std::memory_order_release);
}

juce::String RootFlowAudioProcessor::getMidiLearnTargetParameterID() const
{
    return getMappedParameterIDForSlot(midiLearnTargetParameterSlot.load(std::memory_order_acquire));
}

juce::String RootFlowAudioProcessor::getMappedControllerTextForParameter(const juce::String& paramID) const
{
    const int parameterSlot = getMappedParameterSlotForID(paramID);
    if (parameterSlot < 0)
        return {};

    auto bindingSnapshot = std::atomic_load_explicit(&midiBindingSnapshot, std::memory_order_acquire);
    if (bindingSnapshot == nullptr)
        return {};

    return bindingSnapshot->parameterControllerTexts[(size_t) parameterSlot];
}

juce::String RootFlowAudioProcessor::getParameterDisplayName(const juce::String& paramID) const
{
    if (auto* parameter = tree.getParameter(paramID))
        return parameter->getName(32);

    return paramID;
}

void RootFlowAudioProcessor::hiResTimerCallback()
{
    applyPendingMidiLearnBinding();
    flushPendingMappedParameterChanges();

    nodeSystem.update(getRMS(), (float)juce::Time::getMillisecondCounterHiRes() * 0.001f, beatPhase.load());

    if (processingStateResetPending.load(std::memory_order_acquire) != 0)
        performPendingProcessingStateReset();
}

juce::String RootFlowAudioProcessor::getMappedParameterIDForSlot(int parameterSlot) const
{
    if (! juce::isPositiveAndBelow(parameterSlot, (int) managedParameterIDs.size()))
        return {};

    return managedParameterIDs[(size_t) parameterSlot];
}

int RootFlowAudioProcessor::getMappedParameterSlotForID(const juce::String& paramID) const noexcept
{
    for (size_t i = 0; i < managedParameterIDs.size(); ++i)
    {
        if (paramID == managedParameterIDs[i])
            return (int) i;
    }

    return -1;
}

void RootFlowAudioProcessor::rebuildMidiBindingSnapshot()
{
    auto snapshot = std::make_shared<MidiBindingLookupSnapshot>();

    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);
        for (const auto& binding : midiBindings)
        {
            if (! juce::isPositiveAndBelow(binding.parameterSlot, mappedParameterSlotCount))
                continue;

            if (! juce::isPositiveAndBelow(binding.channel, 17)
                || ! juce::isPositiveAndBelow(binding.controllerNumber, 128))
                continue;

            snapshot->parameterSlots[(size_t) (binding.channel - 1) * 128u + (size_t) binding.controllerNumber] = binding.parameterSlot;
            auto text = "CC " + juce::String(binding.controllerNumber);
            if (binding.channel != 1)
                text << " CH" << binding.channel;
            snapshot->parameterControllerTexts[(size_t) binding.parameterSlot] = text;
        }
    }

    std::atomic_store_explicit(&midiBindingSnapshot,
                               std::shared_ptr<const MidiBindingLookupSnapshot>(std::move(snapshot)),
                               std::memory_order_release);
}

void RootFlowAudioProcessor::clearPendingMidiLearnBinding() noexcept
{
    pendingMidiLearnDirty.store(0, std::memory_order_release);
    pendingMidiLearnChannel.store(1, std::memory_order_relaxed);
    pendingMidiLearnControllerNumber.store(-1, std::memory_order_relaxed);
    pendingMidiLearnParameterSlot.store(-1, std::memory_order_relaxed);
}

void RootFlowAudioProcessor::queuePendingMidiLearnBinding(int channel, int controllerNumber, int parameterSlot) noexcept
{
    pendingMidiLearnChannel.store(channel, std::memory_order_relaxed);
    pendingMidiLearnControllerNumber.store(controllerNumber, std::memory_order_relaxed);
    pendingMidiLearnParameterSlot.store(parameterSlot, std::memory_order_relaxed);
    pendingMidiLearnDirty.store(1, std::memory_order_release);
}

void RootFlowAudioProcessor::applyPendingMidiLearnBinding()
{
    if (pendingMidiLearnDirty.exchange(0, std::memory_order_acq_rel) == 0)
        return;

    const int channel = pendingMidiLearnChannel.load(std::memory_order_relaxed);
    const int controllerNumber = pendingMidiLearnControllerNumber.load(std::memory_order_relaxed);
    const int parameterSlot = pendingMidiLearnParameterSlot.load(std::memory_order_relaxed);
    clearPendingMidiLearnBinding();

    const auto paramID = getMappedParameterIDForSlot(parameterSlot);
    if (paramID.isEmpty())
        return;

    addOrReplaceMidiBinding(channel, controllerNumber, paramID);
}

void RootFlowAudioProcessor::queueMappedParameterChange(int parameterSlot, float normalizedValue) noexcept
{
    if (! juce::isPositiveAndBelow(parameterSlot, (int) pendingMappedParameterValues.size()))
        return;

    pendingMappedParameterValues[(size_t) parameterSlot].store(juce::jlimit(0.0f, 1.0f, normalizedValue),
                                                               std::memory_order_relaxed);
    pendingMappedParameterDirty[(size_t) parameterSlot].store(1, std::memory_order_release);
}

void RootFlowAudioProcessor::flushPendingMappedParameterChanges()
{
    const juce::ScopedLock automationLock(mappedParameterAutomationLock);
    const auto now = juce::Time::getMillisecondCounter();

    for (size_t i = 0; i < pendingMappedParameterDirty.size(); ++i)
    {
        if (pendingMappedParameterDirty[i].exchange(0, std::memory_order_acq_rel) != 0)
        {
            if (auto* parameter = mappedParameters[i])
            {
                if (! mappedParameterGestureActive[i])
                {
                    parameter->beginChangeGesture();
                    mappedParameterGestureActive[i] = true;
                }

                parameter->setValueNotifyingHost(pendingMappedParameterValues[i].load(std::memory_order_relaxed));
                mappedParameterLastUpdateMs[i] = now;
                currentPresetDirty.store(1, std::memory_order_relaxed);
            }
        }

        if (mappedParameterGestureActive[i]
            && now - mappedParameterLastUpdateMs[i] > mappedParameterGestureTimeoutMs)
        {
            if (auto* parameter = mappedParameters[i])
                parameter->endChangeGesture();

            mappedParameterGestureActive[i] = false;
        }
    }
}

void RootFlowAudioProcessor::clearPendingMappedParameterChanges()
{
    const juce::ScopedLock automationLock(mappedParameterAutomationLock);

    for (size_t i = 0; i < pendingMappedParameterDirty.size(); ++i)
    {
        if (mappedParameterGestureActive[i])
        {
            if (auto* parameter = mappedParameters[i])
                parameter->endChangeGesture();
        }

        pendingMappedParameterDirty[i].store(0, std::memory_order_relaxed);
        pendingMappedParameterValues[i].store(mappedParameters[i] != nullptr ? mappedParameters[i]->getValue() : 0.0f,
                                              std::memory_order_relaxed);
        mappedParameterLastUpdateMs[i] = 0;
        mappedParameterGestureActive[i] = false;
    }
}

void RootFlowAudioProcessor::resetToDefaultMpkMiniMappings()
{
    clearPendingMappedParameterChanges();
    clearPendingMidiLearnBinding();
    midiLearnTargetParameterSlot.store(-1, std::memory_order_release);
    midiLearnArmed.store(0, std::memory_order_release);

    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);
        midiBindings.clear();

        const std::array<std::pair<int, const char*>, 8> defaultBindings {{
            { 20, "rootDepth" },
            { 21, "sapFlow" },
            { 22, "sapVitality" },
            { 23, "pulseBreath" },
            { 24, "canopy" },
            { 25, "bloom" },
            { 26, "rain" },
            { 27, "sun" }
        }};

        for (const auto& binding : defaultBindings)
            midiBindings.push_back({ 1, binding.first, binding.second, getMappedParameterSlotForID(binding.second) });
    }

    midiMappingMode.store((int) MidiMappingMode::defaultMappings, std::memory_order_release);
    rebuildMidiBindingSnapshot();
}

void RootFlowAudioProcessor::clearMidiMappings()
{
    clearPendingMappedParameterChanges();
    clearPendingMidiLearnBinding();
    midiLearnTargetParameterSlot.store(-1, std::memory_order_release);
    midiLearnArmed.store(0, std::memory_order_release);

    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);
        midiBindings.clear();
    }

    midiMappingMode.store((int) MidiMappingMode::empty, std::memory_order_release);
    rebuildMidiBindingSnapshot();
}

RootFlowAudioProcessor::MidiActivitySnapshot RootFlowAudioProcessor::getMidiActivitySnapshot() const noexcept
{
    MidiActivitySnapshot snapshot;
    snapshot.eventCounter = midiEventCounter.load();
    snapshot.type = (MidiActivitySnapshot::Type) lastMidiActivityType.load();
    snapshot.controllerNumber = lastMidiControllerNumber.load();
    snapshot.noteNumber = lastMidiNoteNumber.load();
    snapshot.value = lastMidiValue.load();
    snapshot.bendCents = lastMidiBendCents.load();
    snapshot.channel = lastMidiChannel.load();
    snapshot.wasMapped = lastMidiWasMapped.load() != 0;
    return snapshot;
}

void RootFlowAudioProcessor::handleIncomingMidiMessages(juce::MidiBuffer& midiMessages)
{
    auto bindingSnapshot = std::atomic_load_explicit(&midiBindingSnapshot, std::memory_order_acquire);

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            const int note = message.getNoteNumber();
            {
                const juce::ScopedLock sl (noteLock);
                if (std::find(heldMidiNotes.begin(), heldMidiNotes.end(), note) == heldMidiNotes.end())
                    heldMidiNotes.push_back(note);
            }

            // --- BIO-IMPULSE: NoteOn injects energy into the node matrix ---
            const float velocity = message.getFloatVelocity();
            const juce::ScopedLock nodeLock(nodeSystem.getLock());
            auto& nodes = nodeSystem.getNodes();
            if (! nodes.empty())
            {
                int targetNode = note % (int)nodes.size();
                nodes[targetNode].energy = juce::jmin(1.0f, nodes[targetNode].energy + velocity * 0.55f);

                // Organic growth jitter – the node breathes from the hit
                auto& rng = juce::Random::getSystemRandom();
                nodes[targetNode].position.x = juce::jlimit(0.05f, 0.95f,
                    nodes[targetNode].position.x + (rng.nextFloat() - 0.5f) * 0.02f);
                nodes[targetNode].position.y = juce::jlimit(0.05f, 0.95f,
                    nodes[targetNode].position.y + (rng.nextFloat() - 0.5f) * 0.02f);
            }
            sequencerTriggered.store(true);
        }
        else if (message.isNoteOff())
        {
            const juce::ScopedLock sl (noteLock);
            int note = message.getNoteNumber();
            heldMidiNotes.erase(std::remove(heldMidiNotes.begin(), heldMidiNotes.end(), note), heldMidiNotes.end());
        }
        else if (message.isAllNotesOff())
        {
            const juce::ScopedLock sl (noteLock);
            heldMidiNotes.clear();
        }

        bool wasMapped = false;

        if (message.isController())
        {
            const int channel = message.getChannel();
            const int controllerNumber = message.getControllerNumber();
            const float normalizedValue = (float) message.getControllerValue() / 127.0f;

            handleMidiExpressionController(channel, controllerNumber, message.getControllerValue());

            int targetParameterSlot = bindingSnapshot != nullptr
                                    ? bindingSnapshot->getParameterSlot(channel, controllerNumber)
                                    : -1;

            if (midiLearnArmed.load(std::memory_order_acquire) != 0)
            {
                const int learnTargetSlot = midiLearnTargetParameterSlot.load(std::memory_order_acquire);
                if (learnTargetSlot >= 0)
                {
                    int expectedSlot = learnTargetSlot;
                    if (midiLearnTargetParameterSlot.compare_exchange_strong(expectedSlot, -1,
                                                                            std::memory_order_acq_rel,
                                                                            std::memory_order_relaxed))
                    {
                        midiLearnArmed.store(0, std::memory_order_release);
                        queuePendingMidiLearnBinding(channel, controllerNumber, learnTargetSlot);
                        targetParameterSlot = learnTargetSlot;
                    }
                }
            }

            if (targetParameterSlot >= 0)
            {
                queueMappedParameterChange(targetParameterSlot, normalizedValue);
                wasMapped = true;
            }
        }

        if (message.isController()
            || message.isNoteOnOrOff()
            || message.isPitchWheel()
            || message.isChannelPressure()
            || message.isAftertouch())
            recordMidiActivity(message, wasMapped);
    }
}

void RootFlowAudioProcessor::addOrReplaceMidiBinding(int channel, int controllerNumber, const juce::String& paramID)
{
    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);

        midiBindings.erase(std::remove_if(midiBindings.begin(),
                                          midiBindings.end(),
                                          [&](const MidiBinding& binding)
                                          {
                                              return binding.parameterID == paramID
                                                  || (binding.channel == channel && binding.controllerNumber == controllerNumber);
                                          }),
                           midiBindings.end());

        midiBindings.push_back({ channel, controllerNumber, paramID, getMappedParameterSlotForID(paramID) });
    }

    midiMappingMode.store((int) MidiMappingMode::custom, std::memory_order_release);
    rebuildMidiBindingSnapshot();
}

void RootFlowAudioProcessor::appendCustomState(juce::ValueTree& state) const
{
    state.setProperty(presetIndexProperty, currentFactoryPresetIndex.load(std::memory_order_relaxed), nullptr);
    state.setProperty(hoverEffectsEnabledProperty, RootFlow::areHoverEffectsEnabled(), nullptr);
    state.setProperty(idleEffectsEnabledProperty, RootFlow::areIdleEffectsEnabled(), nullptr);
    state.setProperty(popupOverlaysEnabledProperty, RootFlow::arePopupOverlaysEnabled(), nullptr);

    if (auto existing = state.getChildWithName(midiMappingStateTag); existing.isValid())
        state.removeChild(existing, nullptr);

    if (auto existing = state.getChildWithName(midiBindingsTag); existing.isValid())
        state.removeChild(existing, nullptr);

    if (auto existing = state.getChildWithName(userPresetsTag); existing.isValid())
        state.removeChild(existing, nullptr);

    std::vector<MidiBinding> bindingsCopy;
    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);
        bindingsCopy = midiBindings;
    }

    auto mappingMode = (MidiMappingMode) midiMappingMode.load(std::memory_order_acquire);
    if (bindingsCopy.empty() && mappingMode == MidiMappingMode::custom)
        mappingMode = MidiMappingMode::empty;

    juce::ValueTree midiMappingState(midiMappingStateTag);
    midiMappingState.setProperty(midiMappingVersionProperty, midiMappingSchemaVersion, nullptr);
    midiMappingState.setProperty(midiMappingModeProperty,
                                 mappingMode == MidiMappingMode::defaultMappings ? midiMappingModeDefault
                                 : mappingMode == MidiMappingMode::empty ? midiMappingModeEmpty
                                                                         : midiMappingModeCustom,
                                 nullptr);

    if (mappingMode == MidiMappingMode::custom)
    {
        juce::ValueTree bindingsState(midiBindingsTag);
        for (const auto& binding : bindingsCopy)
        {
            juce::ValueTree bindingState(midiBindingTag);
            bindingState.setProperty("channel", binding.channel, nullptr);
            bindingState.setProperty("cc", binding.controllerNumber, nullptr);
            bindingState.setProperty("paramID", binding.parameterID, nullptr);
            bindingsState.addChild(bindingState, -1, nullptr);
        }

        midiMappingState.addChild(bindingsState, -1, nullptr);
    }

    state.addChild(midiMappingState, -1, nullptr);

    juce::ValueTree userPresetsState(userPresetsTag);
    const juce::ScopedLock presetLock(presetStateLock);
    for (const auto& preset : userPresets)
    {
        juce::ValueTree presetState(userPresetTag);
        presetState.setProperty("name", preset.name, nullptr);
        presetState.addChild(preset.state.createCopy(), -1, nullptr);
        userPresetsState.addChild(presetState, -1, nullptr);
    }

    userPresetsState.setProperty("currentUserPresetIndex", currentUserPresetIndex.load(std::memory_order_relaxed), nullptr);
    state.addChild(userPresetsState, -1, nullptr);

    // Node System Serialization
    juce::ValueTree nodeSystemState(nodeSystemTag);
    {
        const juce::ScopedLock nodeLock(nodeSystem.getLock());
        auto& nodes = nodeSystem.getNodes();
        for (int i = 0; i < (int) nodes.size(); ++i)
        {
            juce::ValueTree nState(nodeTag);
            nState.setProperty("idx", i, nullptr);
            nState.setProperty("x", nodes[i].position.x, nullptr);
            nState.setProperty("y", nodes[i].position.y, nullptr);
            nodeSystemState.addChild(nState, -1, nullptr);
        }

        auto& connections = nodeSystem.getConnections();
        for (const auto& c : connections)
        {
            juce::ValueTree cState(connectionTag);
            cState.setProperty("src", c.source, nullptr);
            cState.setProperty("dst", c.target, nullptr);
            cState.setProperty("amt", c.amount, nullptr);
            nodeSystemState.addChild(cState, -1, nullptr);
        }
    }
    state.addChild(nodeSystemState, -1, nullptr);
}

void RootFlowAudioProcessor::restoreCustomState(const juce::ValueTree& state)
{
    currentFactoryPresetIndex.store((int) state.getProperty(presetIndexProperty, 0), std::memory_order_relaxed);
    currentUserPresetIndex.store(-1, std::memory_order_relaxed);
    RootFlow::setHoverEffectsEnabled((bool) state.getProperty(hoverEffectsEnabledProperty, true));
    RootFlow::setIdleEffectsEnabled((bool) state.getProperty(idleEffectsEnabledProperty, false));
    RootFlow::setPopupOverlaysEnabled((bool) state.getProperty(popupOverlaysEnabledProperty, false));

    std::vector<MidiBinding> restoredBindings;
    std::vector<UserPreset> restoredUserPresets;
    MidiMappingMode restoredMappingMode = MidiMappingMode::defaultMappings;

    if (auto midiMappingState = state.getChildWithName(midiMappingStateTag); midiMappingState.isValid())
    {
        const auto mode = midiMappingState.getProperty(midiMappingModeProperty, midiMappingModeDefault).toString();

        if (mode == midiMappingModeCustom)
        {
            restoredMappingMode = MidiMappingMode::custom;

            if (auto bindingsState = midiMappingState.getChildWithName(midiBindingsTag); bindingsState.isValid())
            {
                for (int i = 0; i < bindingsState.getNumChildren(); ++i)
                {
                    auto bindingState = bindingsState.getChild(i);
                    if (! bindingState.hasType(midiBindingTag))
                        continue;

                    const auto paramID = bindingState.getProperty("paramID").toString();
                    const int parameterSlot = getMappedParameterSlotForID(paramID);
                    if (parameterSlot < 0)
                        continue;

                    restoredBindings.push_back({
                        (int) bindingState.getProperty("channel", 1),
                        (int) bindingState.getProperty("cc", 0),
                        paramID,
                        parameterSlot
                    });
                }
            }

            if (restoredBindings.empty())
                restoredMappingMode = MidiMappingMode::empty;
        }
        else if (mode == midiMappingModeEmpty)
        {
            restoredMappingMode = MidiMappingMode::empty;
        }
    }
    else
    {
        const auto bindingsState = state.getChildWithName(midiBindingsTag);
        if (bindingsState.isValid())
        {
            restoredMappingMode = MidiMappingMode::empty;

            for (int i = 0; i < bindingsState.getNumChildren(); ++i)
            {
                auto bindingState = bindingsState.getChild(i);
                if (! bindingState.hasType(midiBindingTag))
                    continue;

                const auto paramID = bindingState.getProperty("paramID").toString();
                const int parameterSlot = getMappedParameterSlotForID(paramID);
                if (parameterSlot < 0)
                    continue;

                restoredBindings.push_back({
                    (int) bindingState.getProperty("channel", 1),
                    (int) bindingState.getProperty("cc", 0),
                    paramID,
                    parameterSlot
                });
            }

            if (! restoredBindings.empty())
                restoredMappingMode = MidiMappingMode::custom;
        }
    }

    if (auto userPresetsState = state.getChildWithName(userPresetsTag); userPresetsState.isValid())
    {
        currentUserPresetIndex.store((int) userPresetsState.getProperty("currentUserPresetIndex", -1), std::memory_order_relaxed);

        for (int i = 0; i < userPresetsState.getNumChildren(); ++i)
        {
            auto presetState = userPresetsState.getChild(i);
            if (! presetState.hasType(userPresetTag) || presetState.getNumChildren() <= 0)
                continue;

            restoredUserPresets.push_back({
                presetState.getProperty("name").toString(),
                presetState.getChild(0).createCopy()
            });
        }
    }

    // Node System Deserialization
    if (auto nodeSystemState = state.getChildWithName(nodeSystemTag); nodeSystemState.isValid())
    {
        {
            const juce::ScopedLock nodeLock(nodeSystem.getLock());
            auto& nodes = nodeSystem.getNodes();
            auto& connections = nodeSystem.getConnections();
            connections.clear();

            for (int i = 0; i < nodeSystemState.getNumChildren(); ++i)
            {
                auto child = nodeSystemState.getChild(i);
                if (child.hasType(nodeTag))
                {
                    int idx = child.getProperty("idx");
                    if (idx >= 0 && idx < (int) nodes.size())
                    {
                        nodes[idx].position.x = child.getProperty("x");
                        nodes[idx].position.y = child.getProperty("y");
                    }
                }
                else if (child.hasType(connectionTag))
                {
                    connections.push_back({
                        (int) child.getProperty("src"),
                        (int) child.getProperty("dst"),
                        -1, // targetSlot (resolved below)
                        (float) child.getProperty("amt")
                    });
                }
            }
        }
    }

    clearPendingMappedParameterChanges();
    clearPendingMidiLearnBinding();
    midiLearnTargetParameterSlot.store(-1, std::memory_order_release);
    midiLearnArmed.store(0, std::memory_order_release);

    if (restoredMappingMode == MidiMappingMode::defaultMappings)
    {
        resetToDefaultMpkMiniMappings();
    }
    else if (restoredMappingMode == MidiMappingMode::empty)
    {
        clearMidiMappings();
    }
    else
    {
        {
            const juce::SpinLock::ScopedLockType lock(midiMappingLock);
            midiBindings = std::move(restoredBindings);
        }

        midiMappingMode.store((int) MidiMappingMode::custom, std::memory_order_release);
        rebuildMidiBindingSnapshot();
    }

    const juce::ScopedLock presetLock(presetStateLock);
    userPresets = std::move(restoredUserPresets);
    if (! juce::isPositiveAndBelow(currentUserPresetIndex.load(std::memory_order_relaxed), (int) userPresets.size()))
        currentUserPresetIndex.store(-1, std::memory_order_relaxed);
}

juce::ValueTree RootFlowAudioProcessor::captureStateForUserPreset()
{
    auto state = tree.copyState();
    if (auto child = state.getChildWithName(midiMappingStateTag); child.isValid())
        state.removeChild(child, nullptr);
    if (auto child = state.getChildWithName(midiBindingsTag); child.isValid())
        state.removeChild(child, nullptr);
    if (auto child = state.getChildWithName(userPresetsTag); child.isValid())
        state.removeChild(child, nullptr);
    state.removeProperty(presetIndexProperty, nullptr);
    state.removeProperty(hoverEffectsEnabledProperty, nullptr);
    state.removeProperty(idleEffectsEnabledProperty, nullptr);
    state.removeProperty(popupOverlaysEnabledProperty, nullptr);
    return state;
}

void RootFlowAudioProcessor::recordMidiActivity(const juce::MidiMessage& message, bool wasMapped) noexcept
{
    lastMidiChannel.store(message.getChannel());
    lastMidiWasMapped.store(wasMapped ? 1 : 0);
    lastMidiControllerNumber.store(-1);
    lastMidiNoteNumber.store(-1);
    lastMidiValue.store(-1);
    lastMidiBendCents.store(0);
    lastMidiActivityType.store((int) MidiActivitySnapshot::Type::none);

    if (message.isPitchWheel())
    {
        const auto channelIndex = (size_t) juce::jlimit(1, 16, message.getChannel()) - 1;
        const float normalized = juce::jlimit(-1.0f, 1.0f, ((float) message.getPitchWheelValue() - 8192.0f) / 8192.0f);
        const float bendSemitones = normalized * midiExpressionState.pitchBendRangeSemitones[channelIndex];
        lastMidiBendCents.store(juce::roundToInt(bendSemitones * 100.0f));
        lastMidiActivityType.store((int) MidiActivitySnapshot::Type::pitchBend);
    }
    else if (message.isChannelPressure())
    {
        lastMidiValue.store(message.getChannelPressureValue());
        lastMidiActivityType.store((int) MidiActivitySnapshot::Type::pressure);
    }
    else if (message.isAftertouch())
    {
        lastMidiNoteNumber.store(message.getNoteNumber());
        lastMidiValue.store(message.getAfterTouchValue());
        lastMidiActivityType.store((int) MidiActivitySnapshot::Type::pressure);
    }
    else if (message.isController())
    {
        lastMidiControllerNumber.store(message.getControllerNumber());
        lastMidiValue.store(message.getControllerValue());
        lastMidiActivityType.store((int) (message.getControllerNumber() == 74
                                              ? MidiActivitySnapshot::Type::timbre
                                              : MidiActivitySnapshot::Type::controller));
    }
    else if (message.isNoteOnOrOff())
    {
        lastMidiNoteNumber.store(message.getNoteNumber());
        lastMidiValue.store(message.isNoteOn() ? juce::roundToInt((float) message.getVelocity() * 127.0f) : 0);
        lastMidiActivityType.store((int) MidiActivitySnapshot::Type::note);
    }

    midiEventCounter.fetch_add(1);
}

void RootFlowAudioProcessor::resetMidiExpressionState() noexcept
{
    midiExpressionState.pitchBendRangeSemitones.fill(2.0f);
    midiExpressionState.modWheelNormalized.fill(0.0f);
    midiRpnMsb.fill(-1);
    midiRpnLsb.fill(-1);
    midiDataEntryMsb.fill(0);
    midiDataEntryLsb.fill(0);
}

void RootFlowAudioProcessor::handleMidiExpressionController(int channel, int controllerNumber, int controllerValue) noexcept
{
    const auto index = (size_t) juce::jlimit(1, 16, channel) - 1;

    switch (controllerNumber)
    {
        case 1:
            midiExpressionState.modWheelNormalized[index] = juce::jlimit(0.0f, 1.0f, (float) controllerValue / 127.0f);
            break;

        case 101:
            midiRpnMsb[index] = controllerValue;
            if (controllerValue == 127 && midiRpnLsb[index] == 127)
            {
                midiRpnMsb[index] = -1;
                midiRpnLsb[index] = -1;
            }
            break;

        case 100:
            midiRpnLsb[index] = controllerValue;
            if (controllerValue == 127 && midiRpnMsb[index] == 127)
            {
                midiRpnMsb[index] = -1;
                midiRpnLsb[index] = -1;
            }
            break;

        case 6:
            midiDataEntryMsb[index] = controllerValue;
            applyPitchbendRangeFromRpn(channel);
            applyMpeZoneLayoutFromRpn(channel, controllerValue);
            break;

        case 38:
            midiDataEntryLsb[index] = controllerValue;
            applyPitchbendRangeFromRpn(channel);
            break;

        default:
            break;
    }
}

void RootFlowAudioProcessor::applyPitchbendRangeFromRpn(int channel) noexcept
{
    const auto index = (size_t) juce::jlimit(1, 16, channel) - 1;

    if (midiRpnMsb[index] != 0 || midiRpnLsb[index] != 0)
        return;

    const float semitones = (float) midiDataEntryMsb[index] + (float) midiDataEntryLsb[index] / 100.0f;
    midiExpressionState.pitchBendRangeSemitones[index] = juce::jlimit(0.0f, 96.0f, semitones);
}


void RootFlowAudioProcessor::applyMpeZoneLayoutFromRpn(int masterChannel, int numMemberChannels) noexcept
{
    const auto index = (size_t) juce::jlimit(1, 16, masterChannel) - 1;

    if (midiRpnMsb[index] != 0 || midiRpnLsb[index] != juce::MPEMessages::zoneLayoutMessagesRpnNumber)
        return;

    const int members = juce::jlimit(0, 15, numMemberChannels);

    if (masterChannel == 1)
    {
        midiExpressionState.pitchBendRangeSemitones[0] = 2.0f;
        for (int channel = 2; channel <= 15; ++channel)
            midiExpressionState.pitchBendRangeSemitones[(size_t) channel - 1] = channel <= 1 + members ? 48.0f : 2.0f;
    }
    else if (masterChannel == 16)
    {
        midiExpressionState.pitchBendRangeSemitones[15] = 2.0f;
        for (int channel = 1; channel <= 15; ++channel)
            midiExpressionState.pitchBendRangeSemitones[(size_t) channel - 1] = channel >= 16 - members ? 48.0f : 2.0f;
    }
}

void RootFlowAudioProcessor::pushNextSampleIntoFifo(float sample) noexcept
{
    if (fifoIndex == fftSize)
    {
        std::fill(fftData.begin(), fftData.end(), 0.0f);
        std::copy(fifo.begin(), fifo.end(), fftData.begin());

        window.multiplyWithWindowingTable(fftData.data(), fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

        for (int i = 0; i < fftSize / 2; ++i)
        {
            const float mag = fftData[i] / (float)fftSize;
            scopeData[i] = juce::jlimit(0.0f, 1.0f, std::pow(mag, 0.45f) * 2.8f);
        }

        pushNextFFTBlockIntoQueue();
        fifoIndex = 0;
    }
    fifo[fifoIndex++] = sample;
}

float RootFlowAudioProcessor::getPlantEnergy() const
{
    return lastPlantEnergy.load(std::memory_order_relaxed);
}

void RootFlowAudioProcessor::setTestToneEnabled(bool shouldEnable) noexcept
{
    testToneEnabled.store(shouldEnable ? 1 : 0, std::memory_order_relaxed);
}

bool RootFlowAudioProcessor::getNextFFTBlock(float* destData)
{
    const int numReady = fftBlockFifo.getNumReady();

    if (numReady <= 0)
        return false;

    int startIndex1 = 0;
    int blockSize1 = 0;
    int startIndex2 = 0;
    int blockSize2 = 0;
    fftBlockFifo.prepareToRead(numReady, startIndex1, blockSize1, startIndex2, blockSize2);

    const auto copyLatestBlock = [&](int startIndex, int blockSize)
    {
        if (blockSize <= 0)
            return false;

        std::copy(fftBlockQueue[(size_t) (startIndex + blockSize - 1)].begin(),
                  fftBlockQueue[(size_t) (startIndex + blockSize - 1)].end(),
                  destData);
        return true;
    };

    const bool copied = blockSize2 > 0
                      ? copyLatestBlock(startIndex2, blockSize2)
                      : copyLatestBlock(startIndex1, blockSize1);

    fftBlockFifo.finishedRead(blockSize1 + blockSize2);
    if (! copied)
        return false;

    return true;
}

void RootFlowAudioProcessor::pushNextFFTBlockIntoQueue() noexcept
{
    int startIndex1 = 0;
    int blockSize1 = 0;
    int startIndex2 = 0;
    int blockSize2 = 0;
    fftBlockFifo.prepareToWrite(1, startIndex1, blockSize1, startIndex2, blockSize2);

    if (blockSize1 > 0)
    {
        fftBlockQueue[(size_t) startIndex1] = scopeData;
        fftBlockFifo.finishedWrite(1);
        return;
    }

    if (blockSize2 > 0)
    {
        fftBlockQueue[(size_t) startIndex2] = scopeData;
        fftBlockFifo.finishedWrite(1);
    }
}

void RootFlowAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(parameterID, newValue);

    if (presetLoadInProgress.load(std::memory_order_acquire) != 0)
        return;

    currentPresetDirty.store(1, std::memory_order_relaxed);
}

void RootFlowAudioProcessor::requestProcessingStateReset() noexcept
{
    processingStateResetPending.store(1, std::memory_order_release);
}

void RootFlowAudioProcessor::performPendingProcessingStateReset() noexcept
{
    if (processingStateResetPending.exchange(0, std::memory_order_acq_rel) == 0)
        return;

    const double safeSampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const int safeBlockSize = juce::jmax(getBlockSize(), 1);

    modulation.prepare(safeSampleRate, safeBlockSize);
    currentBioFeedback = {};
    lastPlantEnergy.store(0.0f, std::memory_order_relaxed);

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<RootFlowVoice*>(synth.getVoice(i)))
        {
            voice->setEngine(&modulation);
            voice->setMidiExpressionState(&midiExpressionState);
            voice->setSampleRate(safeSampleRate, safeBlockSize);
        }
    }

    bloom.reset();
    rain.reset();
    sun.reset();

    for (auto& filter : masterToneFilters)
    {
        filter.reset();
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setResonance(0.52f);
        filter.setCutoffFrequency(clampStateVariableCutoff(7600.0f, safeSampleRate));
    }

    for (auto& filter : monoMakerFilters)
    {
        filter.reset();
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency(clampStateVariableCutoff(tree.getRawParameterValue("monoMakerFreq") != nullptr
                                                               ? tree.getRawParameterValue("monoMakerFreq")->load()
                                                               : 80.0f,
                                                           safeSampleRate));
    }

    masterDriveSmoothed.setCurrentAndTargetValue(1.0f);
    masterToneCutoffSmoothed.setCurrentAndTargetValue(7600.0f);
    masterCrossfeedSmoothed.setCurrentAndTargetValue(0.0f);
    soilDriveSmoothed.setCurrentAndTargetValue(1.0f);
    soilMixSmoothed.setCurrentAndTargetValue(0.0f);
    masterMixSmoothed.setCurrentAndTargetValue(tree.getRawParameterValue("masterMix") != nullptr
                                                   ? tree.getRawParameterValue("masterMix")->load()
                                                   : 1.0f);
    monoMakerFreqSmoothed.setCurrentAndTargetValue(tree.getRawParameterValue("monoMakerFreq") != nullptr
                                                       ? tree.getRawParameterValue("monoMakerFreq")->load()
                                                       : 80.0f);
    monoMakerEnabled.store(tree.getRawParameterValue("monoMakerToggle") != nullptr
                               && tree.getRawParameterValue("monoMakerToggle")->load() > 0.5f,
                           std::memory_order_relaxed);
    soilBodyCutoffSmoothed.setCurrentAndTargetValue(360.0f);
    soilToneCutoffSmoothed.setCurrentAndTargetValue(3600.0f);
    masterVolumeSmoothed.setCurrentAndTargetValue(tree.getRawParameterValue("masterVolume") != nullptr
                                                      ? juce::Decibels::decibelsToGain(tree.getRawParameterValue("masterVolume")->load())
                                                      : 1.0f);
    testToneLevelSmoothed.setCurrentAndTargetValue(isTestToneEnabled() ? 1.0f : 0.0f);
    soilBodyState.fill(0.0f);
    soilToneState.fill(0.0f);
    postFxDcBlockPrevInput.fill(0.0f);
    postFxDcBlockPrevOutput.fill(0.0f);
    outputDcBlockPrevInput.fill(0.0f);
    outputDcBlockPrevOutput.fill(0.0f);
    testTonePhase = 0.0;
    outputSilenceWatchdogBlocks = 0;
    outputRunawayWatchdogBlocks = 0;
    postFxSafetyBypassBlocksRemaining = 0;

    {
        const juce::ScopedLock nodeLock(nodeSystem.getLock());
        auto& nodes = nodeSystem.getNodes();
        for (auto& n : nodes)
            n.slotIndex = getMappedParameterSlotForID(n.paramID);

        for (auto& c : nodeSystem.getConnections())
        {
            if (juce::isPositiveAndBelow(c.target, (int) nodes.size()))
                c.targetSlot = nodes[(size_t) c.target].slotIndex;
        }
    }
}

juce::AudioProcessorEditor* RootFlowAudioProcessor::createEditor()
{
    return new RootFlowAudioProcessorEditor(*this);
}

void RootFlowAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = tree.copyState();
    
    // Save Node System connections into the XML
    state.addChild(nodeSystem.saveConnections(), -1, nullptr);
    
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RootFlowAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName (tree.state.getType()))
        {
            auto restoredState = juce::ValueTree::fromXml(*xmlState);
            restoreCustomState(restoredState);
            
            // Restore Node System connections
            nodeSystem.loadConnections(restoredState.getChildWithName("CONNECTIONS"));
            
            presetLoadInProgress.fetch_add(1, std::memory_order_acq_rel);
            tree.replaceState (restoredState);
            presetLoadInProgress.fetch_sub(1, std::memory_order_acq_rel);
            currentPresetDirty.store(0, std::memory_order_relaxed);
            requestProcessingStateReset();
        }
    }
}

void RootFlowAudioProcessor::resetSequencer()
{
    currentSequencerStep.store(0, std::memory_order_relaxed);
    sampleCounter = 0;
    lastSequencerNote = -1;
    sequencerNoteActive = false;
    samplesPerStep = 44100.0 * 0.125;
}

std::array<RootFlowAudioProcessor::SequencerStep, 16> RootFlowAudioProcessor::getSequencerStepSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    return sequencerSteps;
}

void RootFlowAudioProcessor::clearSequencerSteps()
{
    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    for (auto& step : sequencerSteps)
    {
        step.active = false;
        step.velocity = 0.8f;
    }
}

void RootFlowAudioProcessor::randomizeSequencerStepVelocity(int stepIndex)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    auto& step = sequencerSteps[(size_t) stepIndex];
    step.active = true;
    step.velocity = juce::Random::getSystemRandom().nextFloat() * 0.8f + 0.2f;
}

void RootFlowAudioProcessor::cycleSequencerStepState(int stepIndex)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    auto& step = sequencerSteps[(size_t) stepIndex];
    if (! step.active)
    {
        step.active = true;
        step.velocity = 0.35f;
    }
    else if (step.velocity < 0.55f)
    {
        step.velocity = 0.7f;
    }
    else if (step.velocity < 0.85f)
    {
        step.velocity = 1.0f;
    }
    else
    {
        step.active = false;
    }
}

void RootFlowAudioProcessor::toggleSequencerStepActive(int stepIndex)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    sequencerSteps[(size_t) stepIndex].active = ! sequencerSteps[(size_t) stepIndex].active;
}

void RootFlowAudioProcessor::adjustSequencerStepVelocity(int stepIndex, float delta)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    auto& step = sequencerSteps[(size_t) stepIndex];
    step.velocity = juce::jlimit(0.05f, 1.0f, step.velocity + delta);
}

void RootFlowAudioProcessor::adjustSequencerStepProbability(int stepIndex, float delta)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    auto& step = sequencerSteps[(size_t) stepIndex];
    step.probability = juce::jlimit(0.0f, 1.0f, step.probability + delta);
}

void RootFlowAudioProcessor::randomizeSequencerStepProbability(int stepIndex)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    auto& step = sequencerSteps[(size_t) stepIndex];
    step.probability = juce::Random::getSystemRandom().nextFloat();
}

void RootFlowAudioProcessor::previewSequencerStep(int stepIndex)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    currentSequencerStep.store(stepIndex, std::memory_order_relaxed);
    sequencerTriggered.store(true, std::memory_order_release);
}

void RootFlowAudioProcessor::updateSequencer(int numSamples, juce::MidiBuffer& midiMessages)
{
    const bool isEnabled = *tree.getRawParameterValue("sequencerOn") > 0.5f;
    
    // Transition Handling: Kill hanging manual notes when toggling sequencer
    if (isEnabled != sequencerWasEnabled)
    {
        // HARD KILL: Directly tell the synth engine to stop all voices across all channels
        for (int ch = 1; ch <= 16; ++ch)
            synth.allNotesOff(ch, false);
            
        // Also clear the MIDI buffer just in case
        midiMessages.clear();
        
        currentArpIndex = -1; // Reset arpeggiator sync
        sequencerWasEnabled = isEnabled;
        
        if (! isEnabled && sequencerNoteActive)
        {
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, lastSequencerNote), 0);
            sequencerNoteActive = false;
        }
    }

    if (! isEnabled)
        return;

    // Filter incoming NoteOn/NoteOff messages if the sequencer is active to prevent polyphonic bleed
    // and conflicts with sequenced notes.
    // IMPORTANT: Allow AllNotesOff/AllSoundOff to pass so the transition kill works!
    juce::MidiBuffer filteredMidi;
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (! msg.isNoteOn() && ! msg.isNoteOff() && ! msg.isAllNotesOff() && ! msg.isAllSoundOff())
            filteredMidi.addEvent(msg, metadata.samplePosition);
    }
    midiMessages.swapWith(filteredMidi);

    std::array<SequencerStep, 16> stepSnapshot;
    {
        const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
        stepSnapshot = sequencerSteps;
    }

    const int rateIndex = (int) *tree.getRawParameterValue("sequencerRate");
    const int stepsChoice = (int) *tree.getRawParameterValue("sequencerSteps");
    const int numSteps = juce::jlimit(1, (int) stepSnapshot.size(), stepsChoice);
    const float gateParam = *tree.getRawParameterValue("sequencerGate");
    const float energy = currentBioFeedback.plantEnergy;

    double speedMod = 0.95 + (energy * 0.1);

    if (isRealBotanicsPreset())
        speedMod *= (1.0 + (botanicsRng.nextFloat() - 0.5) * 0.035);

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpmOpt = position->getBpm())
                bpm = *bpmOpt;
        }
    }

    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    const double samplesPerBeat = (sr * 60.0) / bpm;

    double division = 0.25; // Default 1/16
    if      (rateIndex == 0) division = 1.0;   // 1/4
    else if (rateIndex == 1) division = 0.5;   // 1/8
    else if (rateIndex == 2) division = 0.25;  // 1/16
    else if (rateIndex == 3) division = 0.125; // 1/32

    double samplesPerStep = (samplesPerBeat * division) / speedMod;

    // Smooth Gate Calculation – 0.1 to 0.9 sustain
    const float modulatedGate = juce::jlimit(0.12f, 0.90f, gateParam * (0.8f + energy * 0.45f));
    const double sequencerGateSamples = samplesPerStep * modulatedGate;

    for (int i = 0; i < numSamples; ++i)
    {
        sampleCounter++;

        // --- GATE CHECK FIRST (Internal Note Off) ---
        if (sequencerNoteActive && sampleCounter >= sequencerGateSamples)
        {
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, lastSequencerNote), i);
            sequencerNoteActive = false;
        }

        // --- STEP TRIGGER ---
        if (sampleCounter >= samplesPerStep)
        {
            sampleCounter -= samplesPerStep;

            // Ensure previous note is off if Gate hadn't hit yet
            if (sequencerNoteActive)
            {
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, lastSequencerNote), i);
                sequencerNoteActive = false;
            }

            const int stepIndex = juce::jlimit(0, numSteps - 1, currentSequencerStep.load(std::memory_order_relaxed));
            const auto& step = stepSnapshot[(size_t) stepIndex];

            if (step.active)
            {
                // --- BIOLOGICAL EVOLUTION: Probability & Jitter ---
                auto& rng = juce::Random::getSystemRandom();
                
                // 1. Probability Check (Evolving Rhythms)
                const bool probOn = tree.getRawParameterValue("sequencerProbOn")->load() > 0.5f;
                if (!probOn || rng.nextFloat() <= step.probability)
                {
                    const juce::ScopedLock sl (noteLock);
                    if (!heldMidiNotes.empty())
                    {
                        currentArpIndex = (currentArpIndex + 1) % (int)heldMidiNotes.size();
                        lastSequencerNote = heldMidiNotes[(size_t)currentArpIndex];

                        const float dynamicVelocity = juce::jlimit(0.1f, 1.0f, step.velocity * (0.8f + energy * 0.4f));
                        
                        // 2. Jitter (Micro-Timing based on Instability)
                        const bool jitterOn = tree.getRawParameterValue("sequencerJitterOn")->load() > 0.5f;
                        const float instability = *tree.getRawParameterValue("instability");
                        const int jitterSamples = jitterOn ? juce::roundToInt((rng.nextFloat() - 0.5f) * instability * samplesPerStep * 0.15f) : 0;
                        const int triggerPos = juce::jlimit(0, numSamples - 1, i + jitterSamples);
                        
                        midiMessages.addEvent(juce::MidiMessage::noteOn(1, lastSequencerNote, dynamicVelocity), triggerPos);
                        sequencerNoteActive = true;
                        sequencerTriggered.store(true);
                    }
                }
            }

            currentSequencerStep.store((stepIndex + 1) % numSteps, std::memory_order_relaxed);
        }
    }
}

bool RootFlowAudioProcessor::isRealBotanicsPreset() const noexcept
{
    const int idx = currentFactoryPresetIndex.load(std::memory_order_relaxed);
    return (idx >= 80 && idx <= 85);
}

float RootFlowAudioProcessor::getModulatedValue(const juce::String& paramID) const
{
    float base = *tree.getRawParameterValue(paramID);
    float mod = 0.0f;

    const juce::ScopedLock sl (nodeSystem.getLock());

    auto& nodes = const_cast<NodeSystem&>(nodeSystem).getNodes();
    auto& connections = const_cast<NodeSystem&>(nodeSystem).getConnections();

    for (auto& c : connections)
    {
        if (c.source < 0 || c.target < 0) continue;

        auto& source = nodes[c.source];
        auto& target = nodes[c.target];

        if (target.paramID == paramID)
        {
            // Modulation: Source-Wert skaliert mit Connection-Amount und Bio-Health (Mycelium)
            mod += source.value * c.amount * c.health;
        }
    }

    return juce::jlimit(0.0f, 1.0f, base + mod);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RootFlowAudioProcessor();
}

void RootFlowAudioProcessor::mutatePlant()
{
    juce::Random r = juce::Random::getSystemRandom();

    // The user requested MUTATE to affect EVERYTHING: OSC, Ambient Field,
    // Root Field, Pulse Field, Center panel, and Sequencer parameters.
    for (const auto* id : mutationParameterIDs)
    {
        if (auto* param = tree.getParameter(id))
        {
            float currentVal = param->getValue();
            // Allow larger jumps for some parameters or mostly just +/- 20%
            float mutation = r.nextFloat() * 0.4f - 0.2f;

            // Für Sequencer und OSC vielleicht etwas größere Mutationen, damit sich Rhythmen spürbar ändern
            juce::String paramID (id);
            if (paramID.startsWith("sequencer") || paramID.startsWith("osc"))
                mutation *= 1.5f;
            else if (paramID.startsWith("master"))
                mutation *= 0.55f;
            else if (paramID == "monoMakerFreq")
                mutation *= 0.35f;

            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, currentVal + mutation));
        }
    }

    // Trigger a visual energy pulse through the neural mycelium
    sequencerTriggered.store(true);
}

// --- Modular Helpers for processBlock ---

bool RootFlowAudioProcessor::handleOversamplingChangeIfNeeded(juce::AudioBuffer<float>& buffer)
{
    if (auto* param = tree.getRawParameterValue("oversampling"))
    {
        const int targetOversamplingFactor = (int) param->load();

        if (targetOversamplingFactor != currentOversamplingFactor)
        {
            prepareToPlay(getSampleRate(), getBlockSize());
            buffer.clear();
            return true;
        }
    }

    return false;
}

void RootFlowAudioProcessor::clearUnusedOutputChannels(juce::AudioBuffer<float>& buffer)
{
    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
}

void RootFlowAudioProcessor::applyOutputSafety(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    float peak = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i];

            if (!std::isfinite(x))
                x = 0.0f;

            // Robust limiting at the very end of the chain (Safety First)
            x = juce::jlimit(-1.0f, 1.0f, x);
            data[i] = x;

            peak = juce::jmax(peak, std::abs(x));
        }
    }

    currentOutputPeak.store(peak, std::memory_order_relaxed);
}

void RootFlowAudioProcessor::renderSynthAndVoices(juce::AudioBuffer<float>& procBuffer, juce::MidiBuffer& midiMessages)
{
    const float evolution = *tree.getRawParameterValue("evolution");
    const float evolutionGrowth = juce::jlimit(0.0f, 1.0f, evolution / 0.65f);
    const float evolutionDecay  = juce::jlimit(0.0f, 1.0f, (evolution - 0.70f) / 0.30f);

    std::array<float, 16> baseValues {{
        *tree.getRawParameterValue("rootDepth"),
        *tree.getRawParameterValue("rootSoil"),
        *tree.getRawParameterValue("rootAnchor"),
        *tree.getRawParameterValue("sapFlow"),
        *tree.getRawParameterValue("sapVitality"),
        *tree.getRawParameterValue("sapTexture"),
        *tree.getRawParameterValue("pulseRate"),
        *tree.getRawParameterValue("pulseBreath"),
        *tree.getRawParameterValue("pulseGrowth"),
        *tree.getRawParameterValue("canopy"),
        *tree.getRawParameterValue("atmosphere"),
        *tree.getRawParameterValue("instability"),
        *tree.getRawParameterValue("bloom"),
        *tree.getRawParameterValue("rain"),
        *tree.getRawParameterValue("sun"),
        *tree.getRawParameterValue("ecoSystem")
    }};

    // --- EVOLUTION MACRO MORPHING ---
    baseValues[1]  = juce::jlimit(0.0f, 1.0f, baseValues[1] * (0.05f + evolutionGrowth * 0.95f + evolutionDecay * 0.6f));
    baseValues[3]  = juce::jlimit(0.0f, 1.0f, baseValues[3] * (0.2f + evolutionGrowth * 0.8f));
    baseValues[5]  = juce::jlimit(0.0f, 1.0f, baseValues[5] * (0.3f + evolutionGrowth * 0.7f) + evolutionDecay * 0.4f);
    baseValues[11] = juce::jlimit(0.0f, 1.0f, baseValues[11] + evolutionDecay * 0.75f + (evolutionGrowth * 0.08f));
    baseValues[12] = juce::jlimit(0.0f, 1.0f, baseValues[12] * (evolutionGrowth * 0.85f + evolutionDecay * 1.5f));
    baseValues[13] = juce::jlimit(0.0f, 1.0f, baseValues[13] * (evolutionGrowth * 0.80f + evolutionDecay * 0.9f));
    float canopyMorph = 1.0f - std::abs(evolution - 0.65f) * 1.3f;
    baseValues[9]  = juce::jlimit(0.0f, 1.0f, baseValues[9] * (0.4f + juce::jmax(0.0f, canopyMorph) * 0.6f));

    std::array<float, 16> modulationValues {};
    {
        const juce::ScopedLock nodeLock(nodeSystem.getLock());
        const auto& nodes = nodeSystem.getNodes();
        const auto& connections = nodeSystem.getConnections();

        for (auto& connection : connections)
        {
            if (!juce::isPositiveAndBelow(connection.source, (int)nodes.size()) || !juce::isPositiveAndBelow(connection.target, (int)nodes.size()))
                continue;

            const auto& source = nodes[(size_t)connection.source];
            const int targetSlot = connection.targetSlot;
            if (targetSlot >= 0 && juce::isPositiveAndBelow(targetSlot, (int)modulationValues.size()))
                modulationValues[(size_t)targetSlot] += source.value * connection.amount * connection.health;
        }
    }

    auto getBlockModulatedValue = [&](size_t index) { return juce::jlimit(0.0f, 1.0f, baseValues[index] + modulationValues[index]); };

    const float seasonMacro = getBlockModulatedValue(15);
    const auto seasonMorph = getSeasonMorph(seasonMacro);
    auto seasonBlend = [&](float base, float springOffset, float summerOffset, float autumnOffset, float winterOffset) {
        return juce::jlimit(0.0f, 1.0f, base + seasonMorph.spring * springOffset + seasonMorph.summer * summerOffset + seasonMorph.autumn * autumnOffset + seasonMorph.winter * winterOffset);
    };

    const float rootDepth = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(0), 0.22f, 0.12f, 0.18f, -0.32f), 0.015f);
    const float rootSoil = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(1), -0.12f, -0.04f, 0.44f, 0.18f), 0.018f);
    const float rootAnchor = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(2), -0.42f, -0.15f, 0.24f, 0.48f), 0.012f);
    const float sapFlow = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(3), 0.28f, 0.24f, 0.04f, -0.32f), 0.12f);
    const float sapVitality = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(4), 0.44f, 0.28f, -0.12f, -0.44f), 0.10f);
    const float sapTexture = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(5), -0.05f, 0.12f, 0.48f, 0.22f), 0.08f);
    const float pulseRate = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(6), 0.48f, 0.18f, -0.22f, -0.44f), 0.008f);
    const float pulseBreath = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(7), -0.24f, -0.08f, 0.36f, 0.24f), 0.012f);
    const float pulseGrowth = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(8), 0.48f, 0.14f, -0.12f, -0.44f), 0.008f);
    const float canopy = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(9), 0.12f, 0.22f, -0.04f, -0.32f), 0.09f);
    const float instabilityAmount = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(11), 0.05f, 0.12f, 0.22f, -0.14f), 0.025f);
    const float bloomAmount = applySeasonalFxScale(getBlockModulatedValue(12), seasonMorph, 0.22f, 0.14f, -0.10f, -0.22f, 0.65f);
    const float rainAmount = applySeasonalFxScale(getBlockModulatedValue(13), seasonMorph, 0.08f, -0.02f, 0.38f, 0.12f, 0.85f);
    const float sunAmount = applySeasonalFxScale(getBlockModulatedValue(14), seasonMorph, 0.40f, 0.16f, 0.00f, -0.16f, 1.05f);

    currentProcessingBlockState = {
        seasonMacro,
        rootDepth,
        rootAnchor,
        sapFlow,
        sapVitality,
        sapTexture,
        pulseBreath,
        canopy,
        bloomAmount,
        rainAmount,
        sunAmount
    };

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<RootFlowVoice*>(synth.getVoice(i)))
        {
            voice->setFlow(sapFlow); voice->setVitality(sapVitality); voice->setTexture(sapTexture);
            voice->setDepth(rootDepth); voice->setGrowth(pulseGrowth); voice->setPulseRate(pulseRate);
            voice->setPulseAmount(pulseBreath); voice->setCanopy(canopy); voice->setInstability(instabilityAmount);
            voice->setWaveform((int)(*tree.getRawParameterValue("oscWave")));
        }
    }

    // Internal voice unison handled via setUnison in the voice loop.
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<RootFlowVoice*>(synth.getVoice(i)))
        {
            int voiceVoices = 3; // Default internal unison
            if (seasonMacro > 0.45f) voiceVoices = 6;
            if (seasonMacro > 0.85f) voiceVoices = 8;
            voice->setUnison(voiceVoices);
        }
    }

    juce::MidiBuffer oversampledMidi;
    const int oversamplingMultiplier = 1 << currentOversamplingFactor;
    for (const auto meta : midiMessages)
        oversampledMidi.addEvent(meta.getMessage(), meta.samplePosition * oversamplingMultiplier);

    synth.renderNextBlock(procBuffer, oversampledMidi, 0, procBuffer.getNumSamples());
    
    const int numChannels = procBuffer.getNumChannels();
    const int numSamples = procBuffer.getNumSamples();
    if (drySafetyBuffer.getNumChannels() < numChannels
        || drySafetyBuffer.getNumSamples() < numSamples)
        drySafetyBuffer.setSize(numChannels, numSamples, false, false, true);

    for (int channel = 0; channel < numChannels; ++channel)
        drySafetyBuffer.copyFrom(channel, 0, procBuffer, channel, 0, numSamples);

    modulation.setParams(rootDepth, rootSoil, rootAnchor, sapFlow, sapVitality, sapTexture, pulseRate, pulseBreath, pulseGrowth);
    modulation.update(procBuffer);
    currentBioFeedback = modulation.getBioFeedbackSnapshot();
    lastPlantEnergy.store(currentBioFeedback.plantEnergy, std::memory_order_relaxed);
}

void RootFlowAudioProcessor::applyGlobalFx(juce::AudioBuffer<float>& procBuffer)
{
    const float plantEnergy = currentBioFeedback.plantEnergy;
    lastPlantEnergy.store(plantEnergy, std::memory_order_relaxed);

    // --- BPM & BEAT SYNC ---
    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpmOpt = position->getBpm(); bpmOpt && *bpmOpt > 0.0)
                bpm = *bpmOpt;
        }
    }

    const float processingSampleRate = (float) juce::jmax(1.0, getSampleRate()) * (float) (1 << currentOversamplingFactor);
    if (processingSampleRate > 0.0f)
    {
        const float beatPerSecond = (float) bpm / 60.0f;
        const float phaseDelta = beatPerSecond / processingSampleRate;
        float currentPhase = beatPhase.load() + phaseDelta * (float)procBuffer.getNumSamples();
        if (std::isnan(currentPhase) || std::isinf(currentPhase)) currentPhase = 0.0f;
        if (currentPhase >= 1.0f) currentPhase = std::fmod(currentPhase, 1.0f);
        beatPhase.store(currentPhase);
    }

    const auto blockState = currentProcessingBlockState;
    const float ecoRaw = RootFlowDSP::clamp01(*tree.getRawParameterValue("ecoSystem"));
    const float ecoMaster = std::pow(ecoRaw, 1.35f);
    const auto seasonMorph = getSeasonMorph(blockState.seasonMacro);
    const float fxEnergy = juce::jlimit(0.0f, 1.0f, plantEnergy * (0.46f + seasonMorph.spring * 0.06f + seasonMorph.summer * 0.12f + seasonMorph.autumn * 0.16f + seasonMorph.winter * 0.04f) + 0.01f) * ecoMaster;

    bloom.setParams(blockState.bloomAmount * ecoMaster, fxEnergy, blockState.sapVitality, blockState.pulseBreath, blockState.canopy, blockState.rootAnchor);
    bloom.process(procBuffer);

    rain.setParams(blockState.rainAmount * ecoMaster, fxEnergy, blockState.sapFlow, blockState.sapTexture, blockState.pulseBreath, blockState.canopy);
    rain.process(procBuffer);

    sun.setParams(blockState.sunAmount * ecoMaster, fxEnergy, blockState.rootDepth, blockState.sapVitality, blockState.pulseBreath, blockState.canopy);
    sun.process(procBuffer);

    if (auto* mixParam = tree.getRawParameterValue("masterMix"))
        masterMixSmoothed.setTargetValue(mixParam->load());

    if (auto* freqParam = tree.getRawParameterValue("monoMakerFreq"))
        monoMakerFreqSmoothed.setTargetValue(freqParam->load());

    if (auto* toggleParam = tree.getRawParameterValue("monoMakerToggle"))
        monoMakerEnabled.store(toggleParam->load() > 0.5f, std::memory_order_relaxed);

    if (auto* volParam = tree.getRawParameterValue("masterVolume"))
        masterVolumeSmoothed.setTargetValue(juce::Decibels::decibelsToGain(volParam->load()));

    // --- MASTER STAGE ---
    const float drive = tree.getRawParameterValue("masterDrive") != nullptr ? tree.getRawParameterValue("masterDrive")->load() : 1.0f;
    auto* leftData = procBuffer.getWritePointer(0);
    auto* rightData = procBuffer.getNumChannels() > 1 ? procBuffer.getWritePointer(1) : nullptr;

    auto processMasterSample = [this, drive](float x, int channelIndex)
    {
        const float xDrive = x * drive * 0.88f;
        const float saturated = std::tanh(xDrive) * 0.92f + (xDrive / (1.05f + std::abs(xDrive))) * 0.08f;
        const float y = saturated
                      - postFxDcBlockPrevInput[(size_t) channelIndex]
                      + 0.995f * postFxDcBlockPrevOutput[(size_t) channelIndex];
        postFxDcBlockPrevInput[(size_t) channelIndex] = saturated;
        postFxDcBlockPrevOutput[(size_t) channelIndex] = y;
        return y;
    };

    for (int i = 0; i < procBuffer.getNumSamples(); ++i)
    {
        leftData[i] = processMasterSample(leftData[i], 0);
        if (rightData != nullptr)
            rightData[i] = processMasterSample(rightData[i], 1);
    }

    if (auto* compParam = tree.getRawParameterValue("masterCompressor"))
    {
        float compVal = compParam->load();
        if (compVal > 0.01f)
        {
            finalCompressor.setThreshold(-compVal * 28.0f);
            finalCompressor.setRatio(1.0f + compVal * 7.0f);
            juce::dsp::AudioBlock<float> block(procBuffer);
            juce::dsp::ProcessContextReplacing<float> context(block);
            finalCompressor.process(context);
            procBuffer.applyGain(1.0f + compVal * 1.5f);
        }
    }

    const auto* dryLeft = drySafetyBuffer.getReadPointer(0);
    const auto* dryRight = drySafetyBuffer.getNumChannels() > 1 ? drySafetyBuffer.getReadPointer(1) : nullptr;
    const float fxSampleRate = processingSampleRate;
    const bool monoEnabled = rightData != nullptr && monoMakerEnabled.load(std::memory_order_relaxed);

    for (int i = 0; i < procBuffer.getNumSamples(); ++i)
    {
        float wetLeft = leftData[i];
        float wetRight = rightData != nullptr ? rightData[i] : wetLeft;
        const float monoFreq = monoMakerFreqSmoothed.getNextValue();

        if (monoEnabled)
        {
            monoMakerFilters[0].setCutoffFrequency(clampStateVariableCutoff(monoFreq, fxSampleRate));
            monoMakerFilters[1].setCutoffFrequency(clampStateVariableCutoff(monoFreq, fxSampleRate));

            const float lowLeft = monoMakerFilters[0].processSample(0, wetLeft);
            const float lowRight = monoMakerFilters[1].processSample(0, wetRight);
            const float monoLow = (lowLeft + lowRight) * 0.5f;
            wetLeft = monoLow + (wetLeft - lowLeft);
            wetRight = monoLow + (wetRight - lowRight);
        }

        const float mix = masterMixSmoothed.getNextValue();
        const float targetVolume = masterVolumeSmoothed.getNextValue();
        leftData[i] = (wetLeft * mix + dryLeft[i] * (1.0f - mix)) * targetVolume;

        if (rightData != nullptr)
        {
            const float dryRightSample = dryRight != nullptr ? dryRight[i] : dryLeft[i];
            rightData[i] = (wetRight * mix + dryRightSample * (1.0f - mix)) * targetVolume;
        }
    }
}

void RootFlowAudioProcessor::updateMetersAndFft(const juce::AudioBuffer<float>& buffer)
{
    float maxPeak = 0.0f;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        float sampleMix = 0.0f;
        for (int ch = 0; ch < juce::jmin(2, numChannels); ++ch)
        {
            const float s = buffer.getSample(ch, i);
            maxPeak = juce::jmax(maxPeak, std::abs(s));
            sampleMix += s;
        }

        float mixed = sampleMix * (numChannels > 1 ? 0.707f : 1.0f);

        // Update RMS with a simple smoothing alpha
        const float alpha = 0.005f;
        float currentRMS = rmsLevel.load(std::memory_order_relaxed);
        rmsLevel.store(currentRMS + (std::abs(mixed) - currentRMS) * alpha, std::memory_order_relaxed);

        pushNextSampleIntoFifo(mixed);
    }

    currentOutputPeak.store(maxPeak, std::memory_order_relaxed);
}
