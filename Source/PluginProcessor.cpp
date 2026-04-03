
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/Utils/DesignTokens.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace
{
using MutationMode = RootFlowAudioProcessor::MutationMode;

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

constexpr std::array<float, 16> factoryPresetDefaultValues {
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.65f, 0.18f,
    0.28f,
    0.0f, 0.0f, 0.0f,
    0.34f
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

constexpr std::array<const char*, 26> mutationParameterIDs {
    "rootDepth", "rootSoil", "rootAnchor",
    "sapFlow", "sapVitality", "sapTexture",
    "pulseRate", "pulseBreath", "pulseGrowth",
    "canopy", "atmosphere",
    "instability",
    "bloom", "rain", "sun",
    "ecoSystem",
    "sequencerOn", "sequencerRate", "sequencerSteps", "sequencerGate", "sequencerProbOn", "sequencerJitterOn",
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
constexpr auto mutationModeProperty = "mutationMode";
constexpr auto growLocksProperty = "growLocks";
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
constexpr auto sequencerStateTag = "SequencerState";
constexpr auto sequencerStepTag = "SequencerStep";
constexpr auto promptMemoryStateTag = "PromptMemoryState";
constexpr auto promptMemoryEntryTag = "PromptMemoryEntry";
constexpr auto promptMemoryVersionProperty = "version";
constexpr auto lastPromptSummaryProperty = "lastPromptSummary";
constexpr auto lastPromptSeedProperty = "lastPromptSeed";
constexpr auto lastPromptSeedValidProperty = "lastPromptSeedValid";
constexpr int promptMemorySchemaVersion = 1;
constexpr int maxPromptMemoryEntries = 24;

using GrowLockGroup = RootFlowAudioProcessor::GrowLockGroup;

enum class MutationLane
{
    root,
    sap,
    pulse,
    air,
    fx,
    sequencer,
    master
};

struct MutationContext
{
    float evolution = 0.5f;
    float rootWeight = 0.5f;
    float motionWeight = 0.5f;
    float airWeight = 0.5f;
    float fxWeight = 0.0f;
    float instability = 0.0f;
};

struct MutationProfile
{
    float continuousDepth = 0.08f;
    float discreteChance = 0.32f;
    float centerPull = 0.05f;
    float sequencerDepth = 0.12f;
    float masterScale = 0.55f;
    float wildness = 0.45f;
    bool sequencerOnly = false;
};

float getParameterValue(const juce::AudioProcessorValueTreeState& tree, const char* paramID, float fallback) noexcept
{
    if (auto* value = tree.getRawParameterValue(paramID))
        return value->load();

    return fallback;
}

MutationContext makeMutationContext(const juce::AudioProcessorValueTreeState& tree) noexcept
{
    MutationContext context;
    context.evolution = getParameterValue(tree, "evolution", 0.5f);
    context.rootWeight = (getParameterValue(tree, "rootDepth", 0.5f)
                          + getParameterValue(tree, "rootSoil", 0.5f)
                          + getParameterValue(tree, "rootAnchor", 0.5f)) / 3.0f;
    context.motionWeight = (getParameterValue(tree, "pulseRate", 0.5f)
                            + getParameterValue(tree, "pulseBreath", 0.5f)
                            + getParameterValue(tree, "pulseGrowth", 0.5f)) / 3.0f;
    context.airWeight = (getParameterValue(tree, "canopy", 0.65f)
                         + getParameterValue(tree, "atmosphere", 0.18f)
                         + getParameterValue(tree, "ecoSystem", 0.34f)) / 3.0f;
    context.fxWeight = (getParameterValue(tree, "bloom", 0.0f)
                        + getParameterValue(tree, "rain", 0.0f)
                        + getParameterValue(tree, "sun", 0.0f)) / 3.0f;
    context.instability = getParameterValue(tree, "instability", 0.28f);
    return context;
}

MutationProfile getMutationProfile(MutationMode mode, const MutationContext& context) noexcept
{
    MutationProfile profile;

    switch (mode)
    {
        case MutationMode::gentle:
            profile.continuousDepth = 0.030f + context.evolution * 0.040f;
            profile.discreteChance = 0.16f + context.evolution * 0.08f;
            profile.centerPull = 0.10f;
            profile.sequencerDepth = 0.08f + context.motionWeight * 0.05f;
            profile.masterScale = 0.42f;
            profile.wildness = 0.15f;
            break;

        case MutationMode::balanced:
            profile.continuousDepth = 0.055f + context.evolution * 0.070f;
            profile.discreteChance = 0.28f + context.evolution * 0.10f;
            profile.centerPull = 0.06f;
            profile.sequencerDepth = 0.12f + context.motionWeight * 0.06f;
            profile.masterScale = 0.56f;
            profile.wildness = 0.45f;
            break;

        case MutationMode::wild:
            profile.continuousDepth = 0.100f + context.evolution * 0.120f;
            profile.discreteChance = 0.48f + context.evolution * 0.14f;
            profile.centerPull = 0.02f;
            profile.sequencerDepth = 0.18f + context.motionWeight * 0.08f;
            profile.masterScale = 0.66f;
            profile.wildness = 1.0f;
            break;

        case MutationMode::sequencer:
            profile.continuousDepth = 0.0f;
            profile.discreteChance = 0.52f + context.evolution * 0.10f;
            profile.centerPull = 0.0f;
            profile.sequencerDepth = 0.18f + context.motionWeight * 0.10f + context.evolution * 0.08f;
            profile.masterScale = 0.0f;
            profile.wildness = 0.72f;
            profile.sequencerOnly = true;
            break;
    }

    return profile;
}

float randomBipolar(juce::Random& rng) noexcept
{
    return rng.nextFloat() * 2.0f - 1.0f;
}

MutationLane getMutationLane(const juce::String& paramID) noexcept
{
    if (paramID.startsWith("root"))
        return MutationLane::root;

    if (paramID.startsWith("sap"))
        return MutationLane::sap;

    if (paramID.startsWith("pulse"))
        return MutationLane::pulse;

    if (paramID == "canopy" || paramID == "atmosphere" || paramID == "ecoSystem")
        return MutationLane::air;

    if (paramID == "bloom" || paramID == "rain" || paramID == "sun" || paramID == "instability")
        return MutationLane::fx;

    if (paramID.startsWith("sequencer"))
        return MutationLane::sequencer;

    return MutationLane::master;
}

bool isDiscreteMutationParameter(const juce::String& paramID) noexcept
{
    return paramID == "sequencerOn"
        || paramID == "sequencerRate"
        || paramID == "sequencerSteps"
        || paramID == "sequencerProbOn"
        || paramID == "sequencerJitterOn"
        || paramID == "oscWave";
}

float getLaneScale(MutationLane lane,
                   MutationMode mode,
                   const MutationContext& context,
                   const MutationProfile& profile) noexcept
{
    float scale = 1.0f;

    switch (lane)
    {
        case MutationLane::root:
            scale = 0.82f + (1.0f - context.rootWeight) * 0.22f;
            break;

        case MutationLane::sap:
            scale = 0.92f + context.instability * 0.24f;
            break;

        case MutationLane::pulse:
            scale = 0.96f + context.motionWeight * 0.34f;
            break;

        case MutationLane::air:
            scale = 0.86f + context.airWeight * 0.28f;
            break;

        case MutationLane::fx:
            scale = 0.82f + context.fxWeight * 0.32f + context.instability * 0.10f;
            break;

        case MutationLane::sequencer:
            scale = 0.95f + context.motionWeight * 0.36f + context.evolution * 0.10f;
            break;

        case MutationLane::master:
            scale = 0.48f + context.fxWeight * 0.10f;
            break;
    }

    if (mode == MutationMode::gentle)
    {
        if (lane == MutationLane::root && context.rootWeight > 0.66f)
            scale *= 0.74f;
        else if (lane == MutationLane::fx)
            scale *= 0.84f;
    }
    else if (mode == MutationMode::wild)
    {
        if (lane == MutationLane::pulse || lane == MutationLane::sequencer)
            scale *= 1.24f;
        else if (lane == MutationLane::air && context.airWeight < 0.38f)
            scale *= 1.12f;
    }
    else if (mode == MutationMode::sequencer)
    {
        scale = (lane == MutationLane::sequencer) ? scale * 1.18f : 0.0f;
    }

    juce::ignoreUnused(profile);
    return juce::jlimit(0.0f, 2.0f, scale);
}

void mutateDiscreteParameter(juce::AudioProcessorParameter& parameter,
                             const juce::String& paramID,
                             juce::Random& rng,
                             const MutationProfile& profile,
                             float laneScale) noexcept
{
    const int numSteps = juce::jmax(2, parameter.getNumSteps());
    int currentIndex = juce::roundToInt(parameter.getValue() * (float) (numSteps - 1));
    currentIndex = juce::jlimit(0, numSteps - 1, currentIndex);

    int nextIndex = currentIndex;

    if (profile.sequencerOnly && paramID == "sequencerOn")
    {
        nextIndex = numSteps - 1;
    }
    else
    {
        const float chance = juce::jlimit(0.06f, 0.96f, profile.discreteChance * juce::jlimit(0.45f, 1.35f, laneScale));
        if (rng.nextFloat() > chance)
            return;

        if (numSteps <= 2)
        {
            nextIndex = 1 - currentIndex;
        }
        else
        {
            int maxLeap = 1;
            if (paramID == "sequencerSteps")
                maxLeap = profile.wildness > 0.80f ? 3 : 2;
            else if (paramID == "oscWave" && profile.wildness > 0.70f)
                maxLeap = 2;

            const int magnitude = 1 + (maxLeap > 1 ? rng.nextInt(maxLeap) : 0);
            const int direction = rng.nextBool() ? 1 : -1;
            nextIndex = juce::jlimit(0, numSteps - 1, currentIndex + direction * magnitude);

            if (nextIndex == currentIndex)
                nextIndex = currentIndex < numSteps - 1 ? currentIndex + 1 : currentIndex - 1;
        }
    }

    parameter.setValueNotifyingHost((float) nextIndex / (float) (numSteps - 1));
}

void mutateContinuousParameter(juce::AudioProcessorParameter& parameter,
                               const juce::String& paramID,
                               juce::Random& rng,
                               MutationLane lane,
                               const MutationContext& context,
                               const MutationProfile& profile,
                               float laneScale) noexcept
{
    float currentValue = parameter.getValue();
    float amount = profile.continuousDepth * laneScale;

    if (lane == MutationLane::sequencer)
        amount = juce::jmax(amount, profile.sequencerDepth * laneScale);
    else if (lane == MutationLane::master)
        amount *= profile.masterScale;

    float bias = (0.5f - currentValue) * profile.centerPull;

    if (lane == MutationLane::pulse)
        bias += (0.50f - context.motionWeight) * 0.05f * (0.35f + profile.wildness * 0.65f);
    else if (lane == MutationLane::air)
        bias += (0.54f - context.airWeight) * 0.04f * (0.45f + profile.wildness * 0.45f);
    else if (lane == MutationLane::fx)
        bias += (0.40f - context.fxWeight) * 0.04f * (0.35f + profile.wildness * 0.55f);
    else if (lane == MutationLane::root && context.rootWeight > 0.78f && profile.wildness < 0.5f)
        bias -= (currentValue - 0.5f) * 0.06f;

    const float nextValue = juce::jlimit(0.0f, 1.0f, currentValue + randomBipolar(rng) * amount + bias);
    parameter.setValueNotifyingHost(nextValue);
}

const char* mutationModeToStateString(MutationMode mode) noexcept
{
    switch (mode)
    {
        case MutationMode::gentle:    return "gentle";
        case MutationMode::wild:      return "wild";
        case MutationMode::sequencer: return "sequencer";
        case MutationMode::balanced:
        default:                      return "balanced";
    }
}

MutationMode mutationModeFromStateValue(const juce::var& value) noexcept
{
    const auto text = value.toString().trim().toLowerCase();

    if (text == "gentle" || text == "soft")
        return MutationMode::gentle;
    if (text == "wild")
        return MutationMode::wild;
    if (text == "sequencer" || text == "seq")
        return MutationMode::sequencer;
    if (text == "balanced")
        return MutationMode::balanced;

    const int numericValue = (int) value;
    if (numericValue >= (int) MutationMode::gentle && numericValue <= (int) MutationMode::sequencer)
        return (MutationMode) numericValue;

    return MutationMode::balanced;
}

juce::String getMutationModeShortLabel(MutationMode mode)
{
    switch (mode)
    {
        case MutationMode::gentle:    return "SOFT";
        case MutationMode::wild:      return "WILD";
        case MutationMode::sequencer: return "SEQ";
        case MutationMode::balanced:
        default:                      return "BAL";
    }
}

juce::String getMutationModeDisplayName(MutationMode mode)
{
    switch (mode)
    {
        case MutationMode::gentle:    return "Gentle";
        case MutationMode::wild:      return "Wild";
        case MutationMode::sequencer: return "Seq Only";
        case MutationMode::balanced:
        default:                      return "Balanced";
    }
}

constexpr int growLockBit(GrowLockGroup group) noexcept
{
    return 1 << (int) group;
}

bool hasGrowLock(int mask, GrowLockGroup group) noexcept
{
    return (mask & growLockBit(group)) != 0;
}

float circularDistance01(float a, float b) noexcept
{
    const float d = std::abs(a - b);
    return juce::jmin(d, 1.0f - d);
}

float rhythmicPulse(float position01,
                    std::initializer_list<float> centers,
                    float width) noexcept
{
    float best = 0.0f;
    const float safeWidth = juce::jmax(0.0001f, width);

    for (float center : centers)
        best = juce::jmax(best, 1.0f - circularDistance01(position01, center) / safeWidth);

    return juce::jlimit(0.0f, 1.0f, best);
}

int getSequencerLengthFromTree(const juce::AudioProcessorValueTreeState& tree) noexcept
{
    return juce::jlimit(4, 16, juce::roundToInt(getParameterValue(tree, "sequencerSteps", 8.0f)));
}

enum class PromptPatternArchetype : int
{
    organic = 0,
    straight,
    club,
    breakbeat,
    halftime,
    swing,
    triplet,
    latin,
    afro,
    cinematic
};

struct PromptRhythmProfile
{
    PromptPatternArchetype archetype = PromptPatternArchetype::organic;
    float densityBias = 0.0f;
    float anchorBias = 0.0f;
    float offbeatBias = 0.0f;
    float tripletBias = 0.0f;
    float swingAmount = 0.0f;
    float humanize = 0.0f;
    float tightness = 0.58f;
};

float clampSequencerTimingOffset(float amount) noexcept
{
    return juce::jlimit(-0.45f, 0.45f, amount);
}

int stepIndexForFraction(float fraction, int stepCount) noexcept
{
    if (stepCount <= 1)
        return 0;

    const float wrapped = fraction - std::floor(fraction);
    return juce::jlimit(0, stepCount - 1, juce::roundToInt(wrapped * (float) stepCount));
}

void emphasizePatternStep(std::array<RootFlowAudioProcessor::SequencerStep, 16>& steps,
                          int stepCount,
                          int stepIndex,
                          float velocityBoost,
                          float probabilityBoost,
                          float timingOffset = 0.0f,
                          bool forceActive = true) noexcept
{
    if (! juce::isPositiveAndBelow(stepIndex, stepCount))
        return;

    auto& step = steps[(size_t) stepIndex];
    if (forceActive)
        step.active = true;

    step.velocity = juce::jlimit(0.16f, 1.0f, juce::jmax(step.velocity, 0.42f + velocityBoost));
    step.probability = juce::jlimit(0.18f, 1.0f, juce::jmax(step.probability, 0.56f + probabilityBoost));
    step.timingOffset = clampSequencerTimingOffset(step.timingOffset + timingOffset);
}

void emphasizePatternFractions(std::array<RootFlowAudioProcessor::SequencerStep, 16>& steps,
                               int stepCount,
                               std::initializer_list<float> fractions,
                               float velocityBoost,
                               float probabilityBoost,
                               float timingOffset = 0.0f) noexcept
{
    for (const float fraction : fractions)
        emphasizePatternStep(steps, stepCount, stepIndexForFraction(fraction, stepCount),
                             velocityBoost, probabilityBoost, timingOffset);
}

void softenPatternFractions(std::array<RootFlowAudioProcessor::SequencerStep, 16>& steps,
                            int stepCount,
                            std::initializer_list<float> fractions,
                            float velocityScale,
                            float probabilityScale) noexcept
{
    for (const float fraction : fractions)
    {
        const int stepIndex = stepIndexForFraction(fraction, stepCount);
        if (! juce::isPositiveAndBelow(stepIndex, stepCount))
            continue;

        auto& step = steps[(size_t) stepIndex];
        if (! step.active)
            continue;

        step.velocity = juce::jlimit(0.12f, 1.0f, step.velocity * velocityScale);
        step.probability = juce::jlimit(0.08f, 1.0f, step.probability * probabilityScale);
    }
}

void applyPromptPatternTemplate(std::array<RootFlowAudioProcessor::SequencerStep, 16>& steps,
                                int stepCount,
                                const PromptRhythmProfile& promptRhythmProfile,
                                juce::Random& rng)
{
    const int limitedStepCount = juce::jlimit(1, (int) steps.size(), stepCount);

    for (int i = 0; i < limitedStepCount; ++i)
    {
        auto& step = steps[(size_t) i];
        step.timingOffset = 0.0f;

        if (promptRhythmProfile.swingAmount > 0.01f && (i % 2 == 1))
            step.timingOffset = clampSequencerTimingOffset(step.timingOffset + 0.05f + promptRhythmProfile.swingAmount * 0.24f);

        if (promptRhythmProfile.humanize > 0.01f)
        {
            const float randomShift = (rng.nextFloat() - 0.5f)
                                    * (0.04f + promptRhythmProfile.humanize * 0.12f)
                                    * (1.0f - promptRhythmProfile.tightness * 0.55f);
            step.timingOffset = clampSequencerTimingOffset(step.timingOffset + randomShift);
        }
    }

    switch (promptRhythmProfile.archetype)
    {
        case PromptPatternArchetype::straight:
            emphasizePatternFractions(steps, limitedStepCount, { 0.0f, 0.25f, 0.5f, 0.75f }, 0.30f, 0.28f);
            emphasizePatternFractions(steps, limitedStepCount, { 0.125f, 0.625f }, 0.10f, 0.08f);
            break;

        case PromptPatternArchetype::club:
            emphasizePatternFractions(steps, limitedStepCount, { 0.0f, 0.25f, 0.5f, 0.75f }, 0.24f, 0.24f);
            emphasizePatternFractions(steps, limitedStepCount, { 0.125f, 0.375f, 0.625f, 0.875f },
                                      0.10f + promptRhythmProfile.offbeatBias * 0.08f,
                                      0.06f + promptRhythmProfile.offbeatBias * 0.08f);
            break;

        case PromptPatternArchetype::breakbeat:
            emphasizePatternFractions(steps, limitedStepCount, { 0.0f, 0.3125f, 0.5f, 0.6875f, 0.875f }, 0.20f, 0.16f);
            emphasizePatternFractions(steps, limitedStepCount, { 0.125f, 0.5625f }, 0.08f, 0.04f, -0.06f);
            break;

        case PromptPatternArchetype::halftime:
            emphasizePatternFractions(steps, limitedStepCount, { 0.0f, 0.5f, 0.8125f }, 0.28f, 0.24f);
            softenPatternFractions(steps, limitedStepCount, { 0.125f, 0.25f, 0.375f, 0.625f, 0.75f, 0.875f }, 0.84f, 0.78f);
            break;

        case PromptPatternArchetype::swing:
            emphasizePatternFractions(steps, limitedStepCount, { 0.0f, 0.25f, 0.5f, 0.75f }, 0.22f, 0.18f);
            emphasizePatternFractions(steps, limitedStepCount, { 0.125f, 0.375f, 0.625f, 0.875f }, 0.10f, 0.06f,
                                      0.10f + promptRhythmProfile.swingAmount * 0.12f);
            break;

        case PromptPatternArchetype::triplet:
            emphasizePatternFractions(steps, limitedStepCount, { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f }, 0.26f, 0.22f);
            emphasizePatternFractions(steps, limitedStepCount, { 1.0f / 6.0f, 0.5f, 5.0f / 6.0f }, 0.12f, 0.08f);
            break;

        case PromptPatternArchetype::latin:
            emphasizePatternFractions(steps, limitedStepCount, { 0.0f, 0.1875f, 0.5f, 0.6875f, 0.875f }, 0.22f, 0.16f);
            emphasizePatternFractions(steps, limitedStepCount, { 0.3125f, 0.5625f }, 0.08f, 0.04f, 0.04f);
            break;

        case PromptPatternArchetype::afro:
            emphasizePatternFractions(steps, limitedStepCount, { 0.0f, 0.125f, 0.375f, 0.625f, 0.75f }, 0.20f, 0.16f);
            emphasizePatternFractions(steps, limitedStepCount, { 0.5f, 0.875f }, 0.08f, 0.04f,
                                      0.08f + promptRhythmProfile.swingAmount * 0.08f);
            break;

        case PromptPatternArchetype::cinematic:
            emphasizePatternFractions(steps, limitedStepCount, { 0.0f, 0.5f }, 0.24f, 0.24f);
            softenPatternFractions(steps, limitedStepCount, { 0.25f, 0.75f, 0.125f, 0.375f, 0.625f, 0.875f }, 0.82f, 0.74f);
            break;

        case PromptPatternArchetype::organic:
        default:
            break;
    }

    const float timingScale = juce::jlimit(0.34f, 1.0f, 1.04f - promptRhythmProfile.tightness * 0.54f);
    for (int i = 0; i < limitedStepCount; ++i)
        steps[(size_t) i].timingOffset = clampSequencerTimingOffset(steps[(size_t) i].timingOffset * timingScale);
}

void buildSmartSequencerSteps(std::array<RootFlowAudioProcessor::SequencerStep, 16>& steps,
                              int stepCount,
                              MutationMode mode,
                              const MutationContext& context,
                              juce::Random& rng,
                              const PromptRhythmProfile& promptRhythmProfile)
{
    const auto profile = getMutationProfile(mode, context);
    const int limitedStepCount = juce::jlimit(1, (int) steps.size(), stepCount);
    const float baseDensity = juce::jlimit(0.16f,
                                           0.82f,
                                           0.20f
                                           + context.motionWeight * 0.26f
                                           + context.evolution * 0.12f
                                           + promptRhythmProfile.densityBias
                                           + context.instability * 0.08f
                                           + context.fxWeight * 0.05f
                                           - context.rootWeight * 0.05f
                                           - context.airWeight * 0.03f
                                           + (mode == MutationMode::gentle ? -0.07f
                                              : mode == MutationMode::wild ? 0.08f
                                                                           : mode == MutationMode::sequencer ? 0.04f : 0.0f));
    const int targetActiveCount = juce::jlimit(mode == MutationMode::gentle ? 1 : 2,
                                               limitedStepCount,
                                               juce::roundToInt((float) limitedStepCount * baseDensity)
                                               + (mode == MutationMode::wild ? 1 : 0));
    const float threshold = juce::jlimit(0.46f,
                                         0.86f,
                                         0.66f
                                         - profile.wildness * 0.10f
                                         - promptRhythmProfile.densityBias * 0.08f
                                         - promptRhythmProfile.offbeatBias * 0.04f
                                         - promptRhythmProfile.tripletBias * 0.03f
                                         - context.motionWeight * 0.04f
                                         + context.rootWeight * 0.03f
                                         + promptRhythmProfile.anchorBias * 0.03f
                                         - context.airWeight * 0.02f);

    std::array<float, 16> activityScores {};
    std::array<float, 16> velocityTargets {};
    std::array<float, 16> probabilityTargets {};

    for (int i = 0; i < (int) steps.size(); ++i)
    {
        steps[(size_t) i].active = false;
        steps[(size_t) i].velocity = 0.72f;
        steps[(size_t) i].probability = 1.0f;
        steps[(size_t) i].timingOffset = 0.0f;
        activityScores[(size_t) i] = -100.0f;
        velocityTargets[(size_t) i] = 0.72f;
        probabilityTargets[(size_t) i] = 1.0f;
    }

    for (int i = 0; i < limitedStepCount; ++i)
    {
        const float position01 = (float) i / (float) juce::jmax(1, limitedStepCount);
        const float downbeat = rhythmicPulse(position01, { 0.0f, 0.25f, 0.5f, 0.75f }, 0.10f);
        const float offbeat = rhythmicPulse(position01, { 0.125f, 0.375f, 0.625f, 0.875f }, 0.085f);
        const float triplet = rhythmicPulse(position01, { 1.0f / 6.0f, 3.0f / 6.0f, 5.0f / 6.0f }, 0.075f);
        const float waveMotion = 0.5f + 0.5f * std::sin((position01 * juce::MathConstants<float>::twoPi * (1.75f + context.evolution * 1.25f))
                                                         + context.motionWeight * 1.7f
                                                         + context.airWeight * 0.6f);
        const float randomShape = rng.nextFloat();
        const float syncScore = offbeat * (0.16f + context.motionWeight * 0.24f + context.instability * 0.14f
                                           + promptRhythmProfile.offbeatBias * 0.36f)
                              + triplet * ((limitedStepCount == 12 ? 0.22f : 0.06f)
                                           + context.motionWeight * 0.10f
                                           + context.airWeight * 0.08f
                                           + promptRhythmProfile.tripletBias * 0.42f);
        const float anchorScore = downbeat * (0.26f + context.rootWeight * 0.28f
                                              + promptRhythmProfile.anchorBias * 0.24f);

        const float activity = baseDensity
                             + anchorScore
                             + syncScore
                             + (waveMotion - 0.5f) * 0.18f
                             + (randomShape - 0.5f) * (0.12f + promptRhythmProfile.humanize * 0.14f
                                                       + profile.wildness * 0.10f);
        const bool anchorLock = downbeat > 0.92f && (context.rootWeight > 0.72f || limitedStepCount <= 8);

        activityScores[(size_t) i] = activity;
        velocityTargets[(size_t) i] = juce::jlimit(0.18f,
                                                   1.0f,
                                                   0.28f
                                                   + anchorScore * 0.86f
                                                   + syncScore * 0.44f
                                                   + waveMotion * 0.12f
                                                   + (randomShape - 0.5f) * 0.24f);
        probabilityTargets[(size_t) i] = juce::jlimit(0.24f,
                                                      1.0f,
                                                      0.52f
                                                      + downbeat * (0.20f + promptRhythmProfile.tightness * 0.14f)
                                                      + (1.0f - context.instability) * 0.10f
                                                      - offbeat * (0.04f + promptRhythmProfile.tightness * 0.04f)
                                                      + promptRhythmProfile.offbeatBias * 0.04f
                                                      + (rng.nextFloat() - 0.5f) * (0.18f + context.instability * 0.18f)
                                                      - promptRhythmProfile.humanize * 0.08f
                                                      + (mode == MutationMode::gentle ? 0.08f
                                                         : mode == MutationMode::wild ? -0.04f : 0.0f));

        steps[(size_t) i].active = anchorLock || activity >= threshold;
        steps[(size_t) i].velocity = velocityTargets[(size_t) i];
        steps[(size_t) i].probability = steps[(size_t) i].active ? probabilityTargets[(size_t) i] : 1.0f;
    }

    auto countActiveSteps = [&steps, limitedStepCount]()
    {
        int count = 0;
        for (int i = 0; i < limitedStepCount; ++i)
            if (steps[(size_t) i].active)
                ++count;
        return count;
    };

    int activeCount = countActiveSteps();
    while (activeCount < targetActiveCount)
    {
        int bestIndex = -1;
        float bestScore = -1000.0f;

        for (int i = 0; i < limitedStepCount; ++i)
        {
            if (steps[(size_t) i].active)
                continue;

            if (activityScores[(size_t) i] > bestScore)
            {
                bestScore = activityScores[(size_t) i];
                bestIndex = i;
            }
        }

        if (bestIndex < 0)
            break;

        steps[(size_t) bestIndex].active = true;
        steps[(size_t) bestIndex].velocity = velocityTargets[(size_t) bestIndex];
        steps[(size_t) bestIndex].probability = probabilityTargets[(size_t) bestIndex];
        activityScores[(size_t) bestIndex] = -1000.0f;
        ++activeCount;
    }

    while (activeCount > targetActiveCount + (mode == MutationMode::wild ? 1 : 0))
    {
        int weakestIndex = -1;
        float weakestScore = 1000.0f;

        for (int i = 0; i < limitedStepCount; ++i)
        {
            if (! steps[(size_t) i].active)
                continue;

            const float position01 = (float) i / (float) juce::jmax(1, limitedStepCount);
            const float downbeat = rhythmicPulse(position01, { 0.0f, 0.25f, 0.5f, 0.75f }, 0.10f);
            if (downbeat > 0.95f && context.rootWeight > 0.68f)
                continue;

            if (activityScores[(size_t) i] < weakestScore)
            {
                weakestScore = activityScores[(size_t) i];
                weakestIndex = i;
            }
        }

        if (weakestIndex < 0)
            break;

        steps[(size_t) weakestIndex].active = false;
        steps[(size_t) weakestIndex].probability = 1.0f;
        --activeCount;
    }

    applyPromptPatternTemplate(steps, limitedStepCount, promptRhythmProfile, rng);
}

struct PromptIntent
{
    float darkness = 0.0f;
    float brightness = 0.0f;
    float warmth = 0.0f;
    float cold = 0.0f;
    float air = 0.0f;
    float wet = 0.0f;
    float bass = 0.0f;
    float motion = 0.0f;
    float rhythm = 0.0f;
    float aggression = 0.0f;
    float softness = 0.0f;
    float grit = 0.0f;
    float shimmer = 0.0f;
    float organic = 0.0f;
    float evolution = 0.0f;
    float weird = 0.0f;
    float drone = 0.0f;
    float pluck = 0.0f;
    float lead = 0.0f;
    float cinematic = 0.0f;
    float metallic = 0.0f;
    float lofi = 0.0f;
    float punch = 0.0f;
    float width = 0.0f;
    float digital = 0.0f;
    float vintage = 0.0f;
    float acid = 0.0f;
    float detune = 0.0f;
    float calm = 0.0f;
    float density = 0.0f;
    float house = 0.0f;
    float techno = 0.0f;
    float trance = 0.0f;
    float synthwave = 0.0f;
    float edm = 0.0f;
    float dubstep = 0.0f;
    float dnb = 0.0f;
    float breakbeat = 0.0f;
    float ukGarage = 0.0f;
    float trap = 0.0f;
    float hiphop = 0.0f;
    float drill = 0.0f;
    float reggaeton = 0.0f;
    float afrobeat = 0.0f;
    float amapiano = 0.0f;
    float latin = 0.0f;
    float jazz = 0.0f;
    float soul = 0.0f;
    float funk = 0.0f;
    float disco = 0.0f;
    float industrial = 0.0f;
    float orchestral = 0.0f;
    float idm = 0.0f;
    float swingFeel = 0.0f;
    float tripletFeel = 0.0f;
    float straightFeel = 0.0f;
    float humanFeel = 0.0f;
    float halftimeFeel = 0.0f;
    float polyrhythm = 0.0f;
    bool wantsSequencer = false;
    bool wantsMono = false;
    int explicitWaveform = -1;
};

struct PromptPatchTarget
{
    std::array<float, 16> values {};
    bool sequencerOn = false;
    int sequencerRateIndex = 1;
    int sequencerStepCount = 8;
    float sequencerGate = 0.5f;
    bool sequencerProbOn = false;
    bool sequencerJitterOn = false;
    int oscWaveIndex = 0;
    float masterCompressor = 0.12f;
    float masterMix = 0.88f;
    bool monoMaker = false;
    float monoMakerFreqHz = 80.0f;
    float evolution = 0.5f;
    PromptRhythmProfile rhythmProfile;
    juce::String summary;
};

struct PromptMemoryMatch
{
    juce::ValueTree entry;
    float score = 0.0f;
    float blendAmount = 0.0f;
};

juce::String normalizePromptText(juce::String text)
{
    text = text.toLowerCase();
    text = text.replace("\n", " ");
    text = text.replace("\t", " ");
    text = text.replace("ä", "ae");
    text = text.replace("ö", "oe");
    text = text.replace("ü", "ue");
    text = text.replace("ß", "ss");
    return text;
}

bool isPromptStopWord(const juce::String& token)
{
    static const std::array<const char*, 26> stopWords {{
        "a", "an", "and", "the", "mit", "und", "der", "die", "das",
        "ein", "eine", "mehr", "less", "weniger", "bitte", "please",
        "sound", "patch", "seed", "preset", "mach", "make", "more",
        "with", "fuer", "for"
    }};

    for (const auto* stopWord : stopWords)
    {
        if (token == stopWord)
            return true;
    }

    return false;
}

bool promptContainsAny(const juce::String& prompt, std::initializer_list<const char*> keywords)
{
    for (const auto* keyword : keywords)
    {
        if (prompt.contains(keyword))
            return true;
    }

    return false;
}

float promptMetric(const juce::String& prompt, std::initializer_list<const char*> keywords, float hitsForFullScale = 2.0f)
{
    int hits = 0;
    for (const auto* keyword : keywords)
    {
        if (prompt.contains(keyword))
            ++hits;
    }

    return juce::jlimit(0.0f, 1.0f, (float) hits / juce::jmax(0.5f, hitsForFullScale));
}

juce::StringArray tokenizePrompt(const juce::String& rawPrompt)
{
    auto normalized = normalizePromptText(rawPrompt);
    juce::StringArray rawTokens;
    rawTokens.addTokens(normalized, " ,.;:!?/\\|()[]{}<>+-_=*&^%$#@~`'\"", "");
    rawTokens.removeEmptyStrings();

    juce::StringArray tokens;
    for (auto token : rawTokens)
    {
        token = token.trim();

        if (token.length() < 3 || isPromptStopWord(token))
            continue;

        if (! tokens.contains(token))
            tokens.add(token);
    }

    return tokens;
}

float promptTokenSimilarity(const juce::StringArray& a, const juce::StringArray& b)
{
    if (a.isEmpty() || b.isEmpty())
        return 0.0f;

    int intersection = 0;
    for (const auto& token : a)
    {
        if (b.contains(token))
            ++intersection;
    }

    const int unionCount = a.size() + b.size() - intersection;
    return unionCount > 0 ? (float) intersection / (float) unionCount : 0.0f;
}

float mixFloat(float a, float b, float amount) noexcept
{
    return a + (b - a) * juce::jlimit(0.0f, 1.0f, amount);
}

int mixInt(int a, int b, float amount) noexcept
{
    return juce::roundToInt(mixFloat((float) a, (float) b, amount));
}

PromptIntent analysePromptIntent(const juce::String& rawPrompt)
{
    const auto prompt = normalizePromptText(rawPrompt);
    PromptIntent intent;

    intent.darkness = promptMetric(prompt, { "dark", "dunkel", "deep", "tief", "night", "nacht", "shadow", "schatten",
                                             "moody", "murky", "brooding", "cavern", "cave", "black" });
    intent.brightness = promptMetric(prompt, { "bright", "hell", "light", "glow", "glowing", "sonn", "shine", "shimmer",
                                               "klar", "crisp", "clear", "glossy", "radiant", "brilliant" });
    intent.warmth = promptMetric(prompt, { "warm", "warmth", "analog", "wood", "holz", "earth", "erd", "soil",
                                           "vintage", "mellow", "creamy", "velvet", "woody", "amber" });
    intent.cold = promptMetric(prompt, { "cold", "kalt", "icy", "eis", "glass", "glassy", "crystal", "crystalline",
                                         "frost", "sterile", "frozen" });
    intent.air = promptMetric(prompt, { "air", "airy", "luftig", "offen", "wide", "weit", "open", "float", "floating",
                                        "breath", "hazy", "misty" });
    intent.wet = promptMetric(prompt, { "wet", "reverb", "space", "spacious", "raum", "ambient", "hall", "echo",
                                        "wash", "cavernous", "distant", "halo" });
    intent.bass = promptMetric(prompt, { "bass", "sub", "lowend", "low end", "low-end", "bottom", "fundament",
                                         "druck", "rumble", "808", "thick low" });
    intent.motion = promptMetric(prompt, { "moving", "motion", "beweg", "flow", "drift", "alive", "lebendig",
                                           "wander", "swirl", "animated", "modulated", "wobble" });
    intent.rhythm = promptMetric(prompt, { "rhythm", "rhythmic", "groove", "beat", "pulse", "puls", "sequence",
                                           "sequenz", "arp", "arpeggio", "pattern", "syncopated", "stutter" });
    intent.aggression = promptMetric(prompt, { "aggressive", "hart", "hard", "cut", "bite", "edgy", "sharp", "push",
                                               "fierce", "snarl", "nasty", "attack" });
    intent.softness = promptMetric(prompt, { "soft", "weich", "smooth", "gentle", "sanft", "round", "silky",
                                             "calm", "mellow", "pillowy", "softly" });
    intent.grit = promptMetric(prompt, { "dirty", "dirt", "grit", "gritty", "rough", "raw", "noise", "noisy",
                                         "distort", "crunch", "fuzzy", "broken", "overdriven", "granular" });
    intent.shimmer = promptMetric(prompt, { "shimmer", "sparkle", "sparkling", "glitter", "shine", "bloom",
                                            "glow", "luminous", "angelic", "ethereal" });
    intent.organic = promptMetric(prompt, { "organic", "organisch", "bio", "plant", "root", "forest", "moos",
                                            "moss", "wood", "mycel", "natural", "living", "earthy" });
    intent.evolution = promptMetric(prompt, { "evolve", "evolving", "evolution", "grow", "growing", "growth",
                                              "develop", "mutation", "morph", "unfold", "transform" });
    intent.weird = promptMetric(prompt, { "weird", "strange", "alien", "xeno", "chaotic", "chaos", "unstable",
                                          "broken", "abstract", "glitch", "wonky", "surreal" });
    intent.drone = promptMetric(prompt, { "drone", "pad", "flaeche", "flaechig", "bed", "sustain", "long", "lang",
                                          "texture", "atmospheric", "endless" });
    intent.pluck = promptMetric(prompt, { "pluck", "plucky", "percussive", "short", "kurz", "staccato", "blip",
                                          "clicky", "hit", "mallet", "pluckish" });
    intent.lead = promptMetric(prompt, { "lead", "solo", "hook", "melody", "melodie", "topline", "anthem", "riff" });
    intent.cinematic = promptMetric(prompt, { "cinematic", "film", "score", "soundtrack", "trailer", "epic",
                                              "scene", "orchestral" });
    intent.metallic = promptMetric(prompt, { "metallic", "metal", "steel", "chrome", "bell", "clang", "bronze",
                                             "brass", "ringing" });
    intent.lofi = promptMetric(prompt, { "lofi", "lo-fi", "cassette", "dusty", "vhs", "aged", "worn", "degraded",
                                         "tape", "hazy vintage" });
    intent.punch = promptMetric(prompt, { "punchy", "punch", "tight", "snappy", "impact", "knock", "thump", "slam" });
    intent.width = promptMetric(prompt, { "wide", "stereo", "panoramic", "huge", "massive", "spread", "breit", "big" });
    intent.digital = promptMetric(prompt, { "digital", "synthetic", "neon", "cyber", "pixel", "computer", "hybrid", "clean" });
    intent.vintage = promptMetric(prompt, { "vintage", "retro", "analog", "tape", "tube", "classic", "oldschool", "old-school" });
    intent.acid = promptMetric(prompt, { "acid", "squelch", "squelchy", "resonant", "rez", "303", "tb" }, 1.5f);
    intent.detune = promptMetric(prompt, { "detune", "detuned", "chorus", "chorused", "swarm", "beating", "stacked", "ensemble" });
    intent.calm = promptMetric(prompt, { "calm", "peaceful", "meditative", "meditation", "serene", "still", "restful" });
    intent.density = promptMetric(prompt, { "dense", "thick", "layered", "wall", "full", "fat", "rich", "packed" });
    intent.house = promptMetric(prompt, { "house", "deep house", "club house", "slap house", "jackin", "nu house" }, 1.4f);
    intent.techno = promptMetric(prompt, { "techno", "warehouse", "peak time", "peak-time", "berghain", "hypnotic techno" }, 1.3f);
    intent.trance = promptMetric(prompt, { "trance", "uplifting", "euphoric", "supersaw", "anthemic", "hands up", "hands-up" }, 1.4f);
    intent.synthwave = promptMetric(prompt, { "synthwave", "retrowave", "retro wave", "outrun", "neon 80s", "80s" }, 1.4f);
    intent.edm = promptMetric(prompt, { "edm", "festival", "mainstage", "big room", "electro house" }, 1.3f);
    intent.dubstep = promptMetric(prompt, { "dubstep", "brostep", "tearout", "wobble", "growl bass", "riddim" }, 1.3f);
    intent.dnb = promptMetric(prompt, { "dnb", "drum and bass", "drum&bass", "liquid", "neurofunk", "jungle" }, 1.3f);
    intent.breakbeat = promptMetric(prompt, { "breakbeat", "breakbeats", "breaks", "amen", "broken beat" }, 1.3f);
    intent.ukGarage = promptMetric(prompt, { "garage", "uk garage", "ukg", "2-step", "2step", "shuffle garage" }, 1.3f);
    intent.trap = promptMetric(prompt, { "trap", "808 trap", "hat rolls", "hihat rolls", "trappy" }, 1.4f);
    intent.hiphop = promptMetric(prompt, { "hiphop", "hip-hop", "boom bap", "boombap", "rap beat" }, 1.4f);
    intent.drill = promptMetric(prompt, { "drill", "uk drill", "slide 808", "sliding 808", "ominous trap" }, 1.3f);
    intent.reggaeton = promptMetric(prompt, { "reggaeton", "dembow", "latin club" }, 1.2f);
    intent.afrobeat = promptMetric(prompt, { "afrobeat", "afrobeats", "afro groove", "afro swing" }, 1.3f);
    intent.amapiano = promptMetric(prompt, { "amapiano", "log drum", "piano house", "private school" }, 1.3f);
    intent.latin = promptMetric(prompt, { "latin", "salsa", "bachata", "bossa", "samba", "tango" }, 1.4f);
    intent.jazz = promptMetric(prompt, { "jazz", "bebop", "fusion", "blue note", "walking bass" }, 1.4f);
    intent.soul = promptMetric(prompt, { "soul", "neo soul", "neo-soul", "rnb", "r&b" }, 1.4f);
    intent.funk = promptMetric(prompt, { "funk", "funky", "slap", "groovy", "pocket" }, 1.4f);
    intent.disco = promptMetric(prompt, { "disco", "nu disco", "nudisco", "mirrorball" }, 1.3f);
    intent.industrial = promptMetric(prompt, { "industrial", "ebm", "factory", "mechanical", "machine" }, 1.4f);
    intent.orchestral = promptMetric(prompt, { "orchestral", "strings", "brass", "ensemble", "score" }, 1.6f);
    intent.idm = promptMetric(prompt, { "idm", "glitch", "microsound", "braindance", "leftfield" }, 1.4f);
    intent.swingFeel = promptMetric(prompt, { "swing", "swung", "shuffle", "shuffled", "laidback", "laid-back" }, 1.3f);
    intent.tripletFeel = promptMetric(prompt, { "triplet", "triplets", "12/8", "ternary", "triplet groove" }, 1.2f);
    intent.straightFeel = promptMetric(prompt, { "straight", "on grid", "on-grid", "4-on-the-floor", "four on the floor", "straight eighth" }, 1.2f);
    intent.humanFeel = promptMetric(prompt, { "human", "humanized", "humanised", "loose", "organic timing", "drunken" }, 1.4f);
    intent.halftimeFeel = promptMetric(prompt, { "halftime", "half-time", "slow groove", "slow bounce" }, 1.2f);
    intent.polyrhythm = promptMetric(prompt, { "polyrhythm", "polyrhythmic", "cross rhythm", "cross-rhythm" }, 1.2f);

    const float dry = promptMetric(prompt, { "dry", "trocken", "close", "nah", "tight", "direct" });
    const float clubEnergy = juce::jlimit(0.0f, 1.0f, intent.house * 0.54f + intent.techno * 0.70f + intent.trance * 0.44f
                                                       + intent.edm * 0.46f + intent.disco * 0.22f + intent.amapiano * 0.20f);
    const float brokenGroove = juce::jlimit(0.0f, 1.0f, intent.dnb * 0.74f + intent.breakbeat * 0.66f
                                                         + intent.ukGarage * 0.58f + intent.idm * 0.32f);
    const float urbanWeight = juce::jlimit(0.0f, 1.0f, intent.trap * 0.72f + intent.hiphop * 0.56f
                                                        + intent.drill * 0.74f + intent.reggaeton * 0.30f);
    const float afroLatinGroove = juce::jlimit(0.0f, 1.0f, intent.afrobeat * 0.70f + intent.amapiano * 0.56f
                                                            + intent.latin * 0.52f + intent.reggaeton * 0.48f
                                                            + intent.polyrhythm * 0.24f);
    const float soulfulWeight = juce::jlimit(0.0f, 1.0f, intent.jazz * 0.52f + intent.soul * 0.68f
                                                          + intent.funk * 0.62f + intent.disco * 0.36f);
    const float retroWeight = juce::jlimit(0.0f, 1.0f, intent.synthwave * 0.72f + intent.disco * 0.30f
                                                        + intent.vintage * 0.20f);
    const float heavyEdge = juce::jlimit(0.0f, 1.0f, intent.dubstep * 0.64f + intent.industrial * 0.72f
                                                      + intent.drill * 0.26f + intent.aggression * 0.16f);
    const float euphoricLift = juce::jlimit(0.0f, 1.0f, intent.trance * 0.74f + intent.edm * 0.42f
                                                         + intent.synthwave * 0.34f + intent.cinematic * 0.26f
                                                         + intent.orchestral * 0.18f);

    intent.wet = juce::jlimit(0.0f, 1.0f, intent.wet - dry * 0.75f + intent.drone * 0.12f + intent.cinematic * 0.10f);
    intent.air = juce::jlimit(0.0f, 1.0f, intent.air + intent.width * 0.22f + intent.cinematic * 0.08f);
    intent.rhythm = juce::jlimit(0.0f, 1.0f, intent.rhythm + intent.punch * 0.12f + intent.pluck * 0.10f);
    intent.motion = juce::jlimit(0.0f, 1.0f, intent.motion + intent.detune * 0.14f - intent.calm * 0.06f);
    intent.warmth = juce::jlimit(0.0f, 1.0f, intent.warmth + intent.vintage * 0.22f - intent.digital * 0.08f);
    intent.cold = juce::jlimit(0.0f, 1.0f, intent.cold + intent.digital * 0.18f + intent.metallic * 0.12f);
    intent.grit = juce::jlimit(0.0f, 1.0f, intent.grit + intent.lofi * 0.22f + intent.acid * 0.10f);
    intent.brightness = juce::jlimit(0.0f, 1.0f, intent.brightness + intent.cinematic * 0.10f);
    intent.softness = juce::jlimit(0.0f, 1.0f, intent.softness + intent.calm * 0.24f);
    intent.rhythm = juce::jlimit(0.0f, 1.0f, intent.rhythm + clubEnergy * 0.16f + brokenGroove * 0.24f
                                                  + urbanWeight * 0.16f + afroLatinGroove * 0.22f
                                                  + soulfulWeight * 0.12f + intent.swingFeel * 0.08f
                                                  + intent.tripletFeel * 0.08f + intent.halftimeFeel * 0.10f);
    intent.motion = juce::jlimit(0.0f, 1.0f, intent.motion + clubEnergy * 0.12f + brokenGroove * 0.14f
                                                  + afroLatinGroove * 0.16f + intent.idm * 0.12f
                                                  + intent.humanFeel * 0.08f);
    intent.bass = juce::jlimit(0.0f, 1.0f, intent.bass + urbanWeight * 0.20f + intent.dubstep * 0.22f
                                                + clubEnergy * 0.08f + intent.drill * 0.12f);
    intent.darkness = juce::jlimit(0.0f, 1.0f, intent.darkness + urbanWeight * 0.12f + heavyEdge * 0.16f
                                                     - euphoricLift * 0.10f);
    intent.brightness = juce::jlimit(0.0f, 1.0f, intent.brightness + euphoricLift * 0.16f + retroWeight * 0.12f
                                                       + soulfulWeight * 0.06f - heavyEdge * 0.08f);
    intent.width = juce::jlimit(0.0f, 1.0f, intent.width + euphoricLift * 0.16f + retroWeight * 0.12f
                                                 + soulfulWeight * 0.08f + intent.orchestral * 0.12f);
    intent.detune = juce::jlimit(0.0f, 1.0f, intent.detune + intent.trance * 0.22f + retroWeight * 0.12f
                                                  + intent.house * 0.06f);
    intent.warmth = juce::jlimit(0.0f, 1.0f, intent.warmth + soulfulWeight * 0.18f + intent.afrobeat * 0.08f
                                                  + intent.reggaeton * 0.06f);
    intent.cold = juce::jlimit(0.0f, 1.0f, intent.cold + intent.techno * 0.10f + intent.industrial * 0.22f);
    intent.grit = juce::jlimit(0.0f, 1.0f, intent.grit + heavyEdge * 0.18f + brokenGroove * 0.06f);
    intent.punch = juce::jlimit(0.0f, 1.0f, intent.punch + clubEnergy * 0.14f + urbanWeight * 0.16f
                                                 + brokenGroove * 0.10f + afroLatinGroove * 0.10f);
    intent.wet = juce::jlimit(0.0f, 1.0f, intent.wet + euphoricLift * 0.08f + intent.orchestral * 0.16f
                                               + soulfulWeight * 0.04f - urbanWeight * 0.06f);
    intent.shimmer = juce::jlimit(0.0f, 1.0f, intent.shimmer + euphoricLift * 0.14f + retroWeight * 0.10f);
    intent.cinematic = juce::jlimit(0.0f, 1.0f, intent.cinematic + intent.orchestral * 0.48f);
    intent.aggression = juce::jlimit(0.0f, 1.0f, intent.aggression + heavyEdge * 0.18f + intent.drill * 0.10f);
    intent.softness = juce::jlimit(0.0f, 1.0f, intent.softness + soulfulWeight * 0.10f - heavyEdge * 0.06f);

    intent.wantsSequencer = promptContainsAny(prompt, { "rhythm", "rhythmic", "groove", "beat", "pulse", "puls", "sequence",
                                                        "sequenz", "arp", "arpeggio", "step", "pattern", "stutter", "syncopated" })
                        || clubEnergy > 0.28f || brokenGroove > 0.24f || urbanWeight > 0.30f
                        || afroLatinGroove > 0.28f || intent.funk > 0.26f || intent.disco > 0.24f;
    intent.wantsMono = promptContainsAny(prompt, { "mono", "center", "zentral", "focused bass", "sub mono", "centered", "mono bass" })
                    || urbanWeight > 0.32f || intent.dubstep > 0.28f || intent.techno > 0.28f;

    if (promptContainsAny(prompt, { "sine", "sinus" }))
        intent.explicitWaveform = 0;
    else if (promptContainsAny(prompt, { "saw", "saegezahn" }))
        intent.explicitWaveform = 1;
    else if (promptContainsAny(prompt, { "pulse", "square", "rechteck" }))
        intent.explicitWaveform = 2;

    return intent;
}

juce::String buildPromptSummary(const PromptIntent& intent)
{
    juce::StringArray tags;
    const auto dominantStyleTag = [&intent]()
    {
        struct StyleCandidate
        {
            const char* tag;
            float score;
        };

        const std::array<StyleCandidate, 23> styleCandidates {{
            { "Techno", intent.techno },
            { "House", intent.house },
            { "Trance", intent.trance },
            { "Synthwave", intent.synthwave },
            { "EDM", intent.edm },
            { "Dubstep", intent.dubstep },
            { "DnB", intent.dnb },
            { "Breakbeat", intent.breakbeat },
            { "Garage", intent.ukGarage },
            { "Trap", intent.trap },
            { "Hip-Hop", intent.hiphop },
            { "Drill", intent.drill },
            { "Reggaeton", intent.reggaeton },
            { "Afro", intent.afrobeat },
            { "Amapiano", intent.amapiano },
            { "Latin", intent.latin },
            { "Jazz", intent.jazz },
            { "Soul", intent.soul },
            { "Funk", intent.funk },
            { "Disco", intent.disco },
            { "Industrial", intent.industrial },
            { "Orchestral", intent.orchestral },
            { "IDM", intent.idm }
        }};

        const auto bestIt = std::max_element(styleCandidates.begin(), styleCandidates.end(),
                                             [] (const auto& a, const auto& b) { return a.score < b.score; });
        return bestIt != styleCandidates.end() && bestIt->score > 0.34f
            ? juce::String(bestIt->tag)
            : juce::String();
    }();

    if (dominantStyleTag.isNotEmpty())
        tags.add(dominantStyleTag);

    if (intent.bass > 0.52f)                tags.add("Bass");
    else if (intent.lead > 0.50f)           tags.add("Lead");
    else if (intent.pluck > 0.56f)          tags.add("Pluck");
    else if (intent.drone > 0.52f)          tags.add("Drone");

    if (intent.trap > 0.48f || intent.drill > 0.48f || intent.halftimeFeel > 0.44f)           tags.add("Half-Time");
    else if (intent.dnb > 0.46f || intent.breakbeat > 0.44f || intent.ukGarage > 0.42f)      tags.add("Broken");
    else if (intent.afrobeat > 0.46f || intent.amapiano > 0.44f)                               tags.add("Afro");
    else if (intent.latin > 0.42f || intent.reggaeton > 0.42f)                                 tags.add("Latin");
    else if (intent.swingFeel > 0.42f || intent.jazz > 0.42f || intent.funk > 0.44f)          tags.add("Swing");
    else if (intent.rhythm > 0.42f)                                                             tags.add("Rhythmic");

    if (intent.cinematic > 0.48f)           tags.add("Cinematic");
    if (intent.lofi > 0.46f)                tags.add("Lo-Fi");
    if (intent.metallic > 0.50f)            tags.add("Metallic");
    if (intent.air > 0.48f || intent.width > 0.52f) tags.add("Airy");
    if (intent.wet > 0.48f)                 tags.add("Wet");

    if (intent.darkness > intent.brightness + 0.12f)            tags.add("Dark");
    else if (intent.brightness > intent.darkness + 0.12f)       tags.add("Bright");

    if (intent.acid > 0.52f)                 tags.add("Acid");
    if (intent.grit > 0.45f || intent.weird > 0.45f)             tags.add("Wild");
    else if (intent.softness > 0.45f || intent.calm > 0.45f)     tags.add("Soft");

    while (tags.size() > 4)
        tags.remove(tags.size() - 1);

    if (tags.isEmpty())
        tags.add("Custom Seed");

    return tags.joinIntoString(" / ");
}

PromptPatternArchetype promptPatternArchetypeFromValue(int value) noexcept
{
    switch (value)
    {
        case 1:  return PromptPatternArchetype::straight;
        case 2:  return PromptPatternArchetype::club;
        case 3:  return PromptPatternArchetype::breakbeat;
        case 4:  return PromptPatternArchetype::halftime;
        case 5:  return PromptPatternArchetype::swing;
        case 6:  return PromptPatternArchetype::triplet;
        case 7:  return PromptPatternArchetype::latin;
        case 8:  return PromptPatternArchetype::afro;
        case 9:  return PromptPatternArchetype::cinematic;
        case 0:
        default: return PromptPatternArchetype::organic;
    }
}

PromptRhythmProfile buildPromptRhythmProfile(const PromptIntent& intent)
{
    const float clubScore = juce::jlimit(0.0f, 1.0f, intent.house * 0.52f + intent.techno * 0.74f
                                                      + intent.trance * 0.44f + intent.edm * 0.42f
                                                      + intent.disco * 0.24f);
    const float straightScore = juce::jlimit(0.0f, 1.0f, intent.straightFeel * 0.88f + intent.house * 0.18f
                                                          + intent.techno * 0.12f);
    const float breakbeatScore = juce::jlimit(0.0f, 1.0f, intent.dnb * 0.78f + intent.breakbeat * 0.72f
                                                           + intent.ukGarage * 0.52f + intent.idm * 0.26f);
    const float halftimeScore = juce::jlimit(0.0f, 1.0f, intent.halftimeFeel * 0.84f + intent.trap * 0.70f
                                                          + intent.drill * 0.66f + intent.hiphop * 0.50f
                                                          + intent.dubstep * 0.42f);
    const float swingScore = juce::jlimit(0.0f, 1.0f, intent.swingFeel * 0.82f + intent.jazz * 0.60f
                                                       + intent.funk * 0.58f + intent.soul * 0.30f
                                                       + intent.hiphop * 0.18f);
    const float tripletScore = juce::jlimit(0.0f, 1.0f, intent.tripletFeel * 0.90f + intent.amapiano * 0.30f
                                                         + intent.polyrhythm * 0.16f + intent.trap * 0.14f);
    const float latinScore = juce::jlimit(0.0f, 1.0f, intent.latin * 0.68f + intent.reggaeton * 0.72f
                                                       + intent.polyrhythm * 0.16f);
    const float afroScore = juce::jlimit(0.0f, 1.0f, intent.afrobeat * 0.80f + intent.amapiano * 0.60f
                                                      + intent.polyrhythm * 0.24f + intent.swingFeel * 0.12f);
    const float cinematicScore = juce::jlimit(0.0f, 1.0f, intent.cinematic * 0.74f + intent.orchestral * 0.56f
                                                           + intent.drone * 0.24f + intent.calm * 0.08f);

    struct ArchetypeCandidate
    {
        PromptPatternArchetype archetype;
        float score;
    };

    const std::array<ArchetypeCandidate, 8> candidates {{
        { PromptPatternArchetype::straight, straightScore },
        { PromptPatternArchetype::club, clubScore },
        { PromptPatternArchetype::breakbeat, breakbeatScore },
        { PromptPatternArchetype::halftime, halftimeScore },
        { PromptPatternArchetype::swing, swingScore },
        { PromptPatternArchetype::triplet, tripletScore },
        { PromptPatternArchetype::latin, latinScore },
        { PromptPatternArchetype::afro, afroScore }
    }};

    PromptRhythmProfile profile;
    auto bestIt = std::max_element(candidates.begin(), candidates.end(),
                                   [] (const auto& a, const auto& b) { return a.score < b.score; });
    if (cinematicScore > (bestIt != candidates.end() ? bestIt->score : 0.0f) && cinematicScore > 0.36f)
        profile.archetype = PromptPatternArchetype::cinematic;
    else if (bestIt != candidates.end() && bestIt->score > 0.34f)
        profile.archetype = bestIt->archetype;

    profile.densityBias = juce::jlimit(-0.28f, 0.34f,
                                       clubScore * 0.10f
                                       + breakbeatScore * 0.12f
                                       + afroScore * 0.08f
                                       + latinScore * 0.05f
                                       - halftimeScore * 0.14f
                                       - cinematicScore * 0.12f);
    profile.anchorBias = juce::jlimit(-0.24f, 0.34f,
                                      straightScore * 0.24f
                                      + clubScore * 0.16f
                                      + halftimeScore * 0.18f
                                      - breakbeatScore * 0.10f
                                      - afroScore * 0.08f
                                      - latinScore * 0.06f);
    profile.offbeatBias = juce::jlimit(0.0f, 1.0f,
                                       0.08f
                                       + breakbeatScore * 0.44f
                                       + latinScore * 0.34f
                                       + afroScore * 0.38f
                                       + swingScore * 0.18f
                                       + intent.ukGarage * 0.14f);
    profile.tripletBias = juce::jlimit(0.0f, 1.0f,
                                       tripletScore * 0.86f
                                       + afroScore * 0.16f
                                       + intent.jazz * 0.08f);
    profile.swingAmount = juce::jlimit(0.0f, 1.0f,
                                       swingScore * 0.72f
                                       + intent.ukGarage * 0.14f
                                       + afroScore * 0.12f);
    profile.humanize = juce::jlimit(0.0f, 1.0f,
                                    intent.humanFeel * 0.72f
                                    + swingScore * 0.14f
                                    + afroScore * 0.18f
                                    + latinScore * 0.10f
                                    + breakbeatScore * 0.08f
                                    - straightScore * 0.16f
                                    - intent.techno * 0.12f);
    profile.tightness = juce::jlimit(0.18f, 0.94f,
                                     0.58f
                                     + straightScore * 0.18f
                                     + intent.techno * 0.12f
                                     + clubScore * 0.10f
                                     - swingScore * 0.10f
                                     - afroScore * 0.12f
                                     - latinScore * 0.10f
                                     - intent.humanFeel * 0.14f);
    return profile;
}

void blendPromptRhythmProfiles(PromptRhythmProfile& base,
                               const PromptRhythmProfile& memory,
                               float amount) noexcept
{
    const float blend = juce::jlimit(0.0f, 1.0f, amount);

    if (blend >= 0.48f)
        base.archetype = memory.archetype;

    base.densityBias = juce::jlimit(-0.40f, 0.40f, mixFloat(base.densityBias, memory.densityBias, blend));
    base.anchorBias = juce::jlimit(-0.40f, 0.40f, mixFloat(base.anchorBias, memory.anchorBias, blend));
    base.offbeatBias = juce::jlimit(0.0f, 1.0f, mixFloat(base.offbeatBias, memory.offbeatBias, blend));
    base.tripletBias = juce::jlimit(0.0f, 1.0f, mixFloat(base.tripletBias, memory.tripletBias, blend));
    base.swingAmount = juce::jlimit(0.0f, 1.0f, mixFloat(base.swingAmount, memory.swingAmount, blend));
    base.humanize = juce::jlimit(0.0f, 1.0f, mixFloat(base.humanize, memory.humanize, blend));
    base.tightness = juce::jlimit(0.18f, 0.94f, mixFloat(base.tightness, memory.tightness, blend));
}

PromptRhythmProfile morphPromptRhythmProfiles(const PromptRhythmProfile& a,
                                              const PromptRhythmProfile& b,
                                              float amount) noexcept
{
    PromptRhythmProfile profile = a;
    blendPromptRhythmProfiles(profile, b, amount);
    if (juce::jlimit(0.0f, 1.0f, amount) > 0.52f)
        profile.archetype = b.archetype;
    return profile;
}

PromptPatchTarget buildPromptPatchTarget(const juce::String& rawPrompt)
{
    const auto intent = analysePromptIntent(rawPrompt);
    PromptPatchTarget target;
    const auto rhythmProfile = buildPromptRhythmProfile(intent);
    target.summary = buildPromptSummary(intent);
    target.rhythmProfile = rhythmProfile;

    const float clubEnergy = juce::jlimit(0.0f, 1.0f, intent.house * 0.50f + intent.techno * 0.68f
                                                       + intent.trance * 0.42f + intent.edm * 0.44f
                                                       + intent.disco * 0.20f + intent.amapiano * 0.18f);
    const float brokenGroove = juce::jlimit(0.0f, 1.0f, intent.dnb * 0.72f + intent.breakbeat * 0.64f
                                                         + intent.ukGarage * 0.56f + intent.idm * 0.34f);
    const float urbanWeight = juce::jlimit(0.0f, 1.0f, intent.trap * 0.70f + intent.hiphop * 0.54f
                                                        + intent.drill * 0.72f + intent.reggaeton * 0.28f);
    const float afroLatinGroove = juce::jlimit(0.0f, 1.0f, intent.afrobeat * 0.66f + intent.amapiano * 0.58f
                                                            + intent.latin * 0.50f + intent.reggaeton * 0.44f
                                                            + intent.polyrhythm * 0.20f);
    const float soulfulWeight = juce::jlimit(0.0f, 1.0f, intent.jazz * 0.52f + intent.soul * 0.66f
                                                          + intent.funk * 0.62f + intent.disco * 0.30f);
    const float retroWeight = juce::jlimit(0.0f, 1.0f, intent.synthwave * 0.74f + intent.trance * 0.28f
                                                        + intent.disco * 0.20f + intent.vintage * 0.18f);
    const float heavyEdge = juce::jlimit(0.0f, 1.0f, intent.dubstep * 0.62f + intent.industrial * 0.72f
                                                      + intent.drill * 0.24f + intent.aggression * 0.16f);
    const float euphoricLift = juce::jlimit(0.0f, 1.0f, intent.trance * 0.76f + intent.edm * 0.46f
                                                         + intent.synthwave * 0.36f + intent.cinematic * 0.24f
                                                         + intent.orchestral * 0.18f);
    const float halftimeDrive = juce::jlimit(0.0f, 1.0f, intent.halftimeFeel * 0.76f + intent.trap * 0.34f
                                                          + intent.drill * 0.30f + intent.hiphop * 0.24f
                                                          + intent.dubstep * 0.16f);
    const float leftfieldWeight = juce::jlimit(0.0f, 1.0f, intent.idm * 0.70f + intent.weird * 0.22f
                                                            + intent.digital * 0.12f);

    const float dark = juce::jlimit(0.0f, 1.0f, 0.15f + intent.darkness * 0.78f + intent.bass * 0.16f
                                                     + intent.drone * 0.10f + intent.lofi * 0.08f
                                                     + intent.cinematic * 0.10f + urbanWeight * 0.08f
                                                     + heavyEdge * 0.10f - intent.brightness * 0.24f
                                                     - euphoricLift * 0.10f);
    const float bright = juce::jlimit(0.0f, 1.0f, 0.16f + intent.brightness * 0.76f + intent.shimmer * 0.12f
                                                       + intent.air * 0.08f + intent.cinematic * 0.08f
                                                       + euphoricLift * 0.12f + retroWeight * 0.10f
                                                       + intent.metallic * 0.10f - intent.darkness * 0.20f
                                                       - intent.lofi * 0.10f - heavyEdge * 0.08f);
    const float body = juce::jlimit(0.0f, 1.0f, 0.18f + intent.bass * 0.68f + intent.warmth * 0.18f
                                                     + intent.drone * 0.16f + intent.density * 0.18f
                                                     + urbanWeight * 0.14f + clubEnergy * 0.08f
                                                     + intent.punch * 0.10f - intent.air * 0.10f
                                                     - intent.width * 0.08f);
    const float motion = juce::jlimit(0.0f, 1.0f, 0.18f + intent.motion * 0.54f + intent.rhythm * 0.26f
                                                       + intent.evolution * 0.12f + intent.detune * 0.18f
                                                       + brokenGroove * 0.12f + afroLatinGroove * 0.12f
                                                       + clubEnergy * 0.08f + intent.acid * 0.06f
                                                       + leftfieldWeight * 0.08f - intent.calm * 0.10f);
    const float airiness = juce::jlimit(0.0f, 1.0f, 0.10f + intent.air * 0.60f + intent.wet * 0.16f
                                                         + intent.shimmer * 0.08f + intent.width * 0.24f
                                                         + intent.cinematic * 0.10f + euphoricLift * 0.08f
                                                         + intent.orchestral * 0.10f - intent.bass * 0.10f
                                                         - intent.density * 0.06f);
    const float wetness = juce::jlimit(0.0f, 1.0f, 0.06f + intent.wet * 0.66f + intent.drone * 0.10f
                                                        + intent.air * 0.08f + intent.cinematic * 0.18f
                                                        + intent.orchestral * 0.14f + soulfulWeight * 0.04f
                                                        + intent.lofi * 0.06f - intent.pluck * 0.16f
                                                        - intent.punch * 0.10f - urbanWeight * 0.06f);
    const float texture = juce::jlimit(0.0f, 1.0f, 0.12f + intent.grit * 0.48f + intent.organic * 0.14f
                                                        + intent.cold * 0.08f + intent.weird * 0.10f
                                                        + intent.metallic * 0.14f + intent.lofi * 0.18f
                                                        + intent.digital * 0.10f + heavyEdge * 0.12f
                                                        + leftfieldWeight * 0.08f);
    const float instability = juce::jlimit(0.0f, 1.0f, 0.08f + intent.grit * 0.28f + intent.weird * 0.26f
                                                            + intent.motion * 0.10f + intent.acid * 0.18f
                                                            + intent.detune * 0.16f + intent.digital * 0.08f
                                                            + leftfieldWeight * 0.10f + heavyEdge * 0.08f
                                                            - intent.softness * 0.10f - intent.calm * 0.10f);
    const float punchiness = juce::jlimit(0.0f, 1.0f, 0.10f + intent.punch * 0.56f + intent.aggression * 0.20f
                                                           + intent.pluck * 0.18f + clubEnergy * 0.08f
                                                           + urbanWeight * 0.10f + afroLatinGroove * 0.06f
                                                           - intent.drone * 0.12f);
    const float stereoWidth = juce::jlimit(0.0f, 1.0f, 0.10f + intent.width * 0.70f + airiness * 0.18f
                                                            + wetness * 0.08f + euphoricLift * 0.10f
                                                            + retroWeight * 0.12f - body * 0.08f);
    const float vintageTone = juce::jlimit(0.0f, 1.0f, 0.08f + intent.vintage * 0.56f + intent.lofi * 0.18f
                                                            + intent.warmth * 0.16f + soulfulWeight * 0.08f
                                                            + retroWeight * 0.08f - intent.digital * 0.10f);
    const float metallicTone = juce::jlimit(0.0f, 1.0f, intent.metallic * 0.70f + intent.digital * 0.14f
                                                             + intent.industrial * 0.18f + bright * 0.10f);
    const float acidTone = juce::jlimit(0.0f, 1.0f, intent.acid * 0.74f + intent.aggression * 0.14f
                                                         + motion * 0.08f + intent.techno * 0.06f);

    target.values[0]  = juce::jlimit(0.0f, 1.0f, 0.24f + body * 0.54f + dark * 0.10f - intent.pluck * 0.08f);
    target.values[1]  = juce::jlimit(0.0f, 1.0f, 0.18f + intent.warmth * 0.28f + body * 0.12f + intent.organic * 0.18f
                                                      + vintageTone * 0.24f + soulfulWeight * 0.10f
                                                      - intent.cold * 0.14f);
    target.values[2]  = juce::jlimit(0.0f, 1.0f, 0.22f + body * 0.22f + intent.drone * 0.22f + intent.bass * 0.14f
                                                      + intent.density * 0.12f - intent.lead * 0.08f);
    target.values[3]  = juce::jlimit(0.0f, 1.0f, 0.20f + bright * 0.56f + airiness * 0.08f + metallicTone * 0.08f
                                                      + stereoWidth * 0.06f + euphoricLift * 0.08f
                                                      - dark * 0.22f);
    target.values[4]  = juce::jlimit(0.0f, 1.0f, 0.20f + intent.aggression * 0.24f + motion * 0.18f + intent.lead * 0.12f
                                                      + intent.grit * 0.08f + punchiness * 0.18f + acidTone * 0.10f
                                                      + clubEnergy * 0.06f);
    target.values[5]  = juce::jlimit(0.0f, 1.0f, 0.12f + texture * 0.54f + intent.warmth * 0.08f
                                                      + metallicTone * 0.16f + vintageTone * 0.10f
                                                      + leftfieldWeight * 0.06f);
    target.values[6]  = juce::jlimit(0.0f, 1.0f, 0.06f + intent.rhythm * 0.56f + motion * 0.14f
                                                      + intent.pluck * 0.10f + punchiness * 0.12f + acidTone * 0.10f
                                                      + clubEnergy * 0.08f + brokenGroove * 0.10f
                                                      + afroLatinGroove * 0.08f);
    target.values[7]  = juce::jlimit(0.0f, 1.0f, 0.16f + motion * 0.34f + intent.drone * 0.16f + airiness * 0.10f
                                                      + intent.detune * 0.12f + intent.calm * 0.08f
                                                      + soulfulWeight * 0.04f);
    target.values[8]  = juce::jlimit(0.0f, 1.0f, 0.16f + intent.evolution * 0.38f + motion * 0.16f
                                                      + intent.organic * 0.12f + intent.cinematic * 0.10f
                                                      + intent.density * 0.08f + leftfieldWeight * 0.08f
                                                      + afroLatinGroove * 0.04f);
    target.values[9]  = juce::jlimit(0.0f, 1.0f, 0.24f + airiness * 0.32f + intent.shimmer * 0.08f
                                                      + intent.drone * 0.08f + stereoWidth * 0.26f
                                                      + intent.cinematic * 0.10f);
    target.values[10] = juce::jlimit(0.0f, 1.0f, 0.06f + wetness * 0.52f + airiness * 0.14f + intent.drone * 0.08f
                                                      + stereoWidth * 0.10f + intent.cinematic * 0.16f
                                                      + soulfulWeight * 0.04f);
    target.values[11] = juce::jlimit(0.0f, 1.0f, instability);
    target.values[12] = juce::jlimit(0.0f, 1.0f, 0.02f + intent.shimmer * 0.48f + bright * 0.08f
                                                      + intent.cinematic * 0.12f + euphoricLift * 0.10f);
    target.values[13] = juce::jlimit(0.0f, 1.0f, wetness * 0.58f + dark * 0.08f + intent.lofi * 0.10f + stereoWidth * 0.04f);
    target.values[14] = juce::jlimit(0.0f, 1.0f, bright * 0.58f + intent.shimmer * 0.14f + metallicTone * 0.10f
                                                      + euphoricLift * 0.08f - dark * 0.08f);
    target.values[15] = juce::jlimit(0.0f, 1.0f, 0.34f + intent.warmth * 0.16f + bright * 0.08f + vintageTone * 0.08f
                                                      + retroWeight * 0.06f - intent.cold * 0.18f
                                                      - dark * 0.08f);

    target.sequencerOn = intent.wantsSequencer || intent.rhythm > 0.40f || intent.pluck > 0.50f
                      || intent.punch > 0.48f || intent.acid > 0.54f || clubEnergy > 0.34f
                      || brokenGroove > 0.32f || urbanWeight > 0.38f || afroLatinGroove > 0.34f;
    target.sequencerProbOn = intent.evolution > 0.30f || intent.organic > 0.40f || intent.weird > 0.34f
                          || intent.lofi > 0.34f || rhythmProfile.humanize > 0.20f
                          || brokenGroove > 0.28f || afroLatinGroove > 0.28f;
    target.sequencerJitterOn = intent.motion > 0.36f || instability > 0.42f || intent.weird > 0.34f
                            || intent.detune > 0.42f || intent.acid > 0.38f
                            || rhythmProfile.humanize > 0.22f || rhythmProfile.swingAmount > 0.18f
                            || brokenGroove > 0.32f || afroLatinGroove > 0.30f;
    target.sequencerGate = juce::jlimit(0.12f, 0.92f, 0.24f + intent.drone * 0.38f + wetness * 0.08f
                                                     + intent.cinematic * 0.06f - intent.pluck * 0.24f
                                                     - punchiness * 0.10f + intent.rhythm * 0.06f
                                                     + halftimeDrive * 0.08f - clubEnergy * 0.06f
                                                     + soulfulWeight * 0.04f);

    const float rateDrive = intent.rhythm * 0.44f + intent.pluck * 0.20f + intent.motion * 0.10f
                          + punchiness * 0.12f + acidTone * 0.14f + clubEnergy * 0.18f
                          + brokenGroove * 0.20f + afroLatinGroove * 0.12f
                          - intent.cinematic * 0.08f;
    target.sequencerRateIndex = rateDrive > 0.72f ? 3 : rateDrive > 0.46f ? 2 : rateDrive > 0.22f ? 1 : 0;

    const float stepComplexity = intent.rhythm * 0.26f + intent.evolution * 0.18f + intent.weird * 0.12f
                               + intent.motion * 0.14f + intent.density * 0.14f + acidTone * 0.10f
                               + brokenGroove * 0.18f + afroLatinGroove * 0.16f + leftfieldWeight * 0.12f;

    if (rhythmProfile.archetype == PromptPatternArchetype::triplet
        || rhythmProfile.archetype == PromptPatternArchetype::afro
        || rhythmProfile.archetype == PromptPatternArchetype::latin)
    {
        target.sequencerStepCount = stepComplexity > 0.30f ? 12 : 8;
    }
    else if (rhythmProfile.archetype == PromptPatternArchetype::club
             || rhythmProfile.archetype == PromptPatternArchetype::straight
             || brokenGroove > 0.42f
             || clubEnergy > 0.48f)
    {
        target.sequencerStepCount = stepComplexity > 0.26f ? 16 : 8;
    }
    else if (rhythmProfile.archetype == PromptPatternArchetype::halftime || halftimeDrive > 0.42f)
    {
        target.sequencerStepCount = stepComplexity > 0.54f ? 12 : 8;
    }
    else
    {
        target.sequencerStepCount = stepComplexity > 0.74f ? 16 : stepComplexity > 0.48f ? 12 : stepComplexity > 0.24f ? 8 : 4;
    }

    if (intent.explicitWaveform >= 0)
        target.oscWaveIndex = intent.explicitWaveform;
    else if (intent.pluck > 0.52f || intent.rhythm > 0.62f || intent.acid > 0.50f
             || urbanWeight > 0.42f || intent.dubstep > 0.36f || intent.ukGarage > 0.36f)
        target.oscWaveIndex = 2;
    else if (intent.lead > 0.42f || intent.aggression > 0.42f || bright > 0.56f || metallicTone > 0.46f
             || intent.digital > 0.46f || clubEnergy > 0.34f || retroWeight > 0.32f
             || euphoricLift > 0.32f)
        target.oscWaveIndex = 1;
    else
        target.oscWaveIndex = 0;

    target.masterCompressor = juce::jlimit(0.04f, 0.68f, 0.06f + intent.aggression * 0.18f + intent.bass * 0.12f
                                                        + intent.rhythm * 0.06f + punchiness * 0.16f
                                                        + clubEnergy * 0.06f + urbanWeight * 0.08f
                                                        + heavyEdge * 0.06f);
    target.masterMix = juce::jlimit(0.72f, 1.0f, 0.84f + wetness * 0.08f + airiness * 0.02f
                                                    + intent.cinematic * 0.06f - intent.bass * 0.04f
                                                    - punchiness * 0.05f + euphoricLift * 0.04f
                                                    + soulfulWeight * 0.02f - heavyEdge * 0.04f);
    target.monoMaker = intent.wantsMono || intent.bass > 0.58f || (body > 0.62f && wetness < 0.48f)
                    || (punchiness > 0.62f && stereoWidth < 0.40f)
                    || clubEnergy > 0.46f || urbanWeight > 0.40f || intent.dubstep > 0.32f;
    target.monoMakerFreqHz = juce::jlimit(42.0f, 220.0f, 56.0f + body * 96.0f + punchiness * 28.0f
                                                       + intent.rhythm * 12.0f + urbanWeight * 10.0f
                                                       + clubEnergy * 8.0f);
    target.evolution = juce::jlimit(0.0f, 1.0f, 0.20f + intent.evolution * 0.40f + intent.motion * 0.14f
                                                     + intent.weird * 0.10f + intent.cinematic * 0.06f
                                                     + intent.detune * 0.10f + leftfieldWeight * 0.08f
                                                     + afroLatinGroove * 0.04f - intent.softness * 0.04f
                                                     - intent.calm * 0.10f - intent.straightFeel * 0.06f);

    return target;
}

juce::String promptMemoryValuePropertyName(int index)
{
    return "v" + juce::String(index);
}

float stateValue(const juce::ValueTree& state, const char* propertyName, float fallback)
{
    return (float) state.getProperty(propertyName, fallback);
}

void writePromptTargetToMemoryEntry(juce::ValueTree& entry, const PromptPatchTarget& target)
{
    for (size_t i = 0; i < target.values.size(); ++i)
        entry.setProperty(promptMemoryValuePropertyName((int) i), target.values[i], nullptr);

    entry.setProperty("seqOn", target.sequencerOn, nullptr);
    entry.setProperty("seqRate", target.sequencerRateIndex, nullptr);
    entry.setProperty("seqSteps", target.sequencerStepCount, nullptr);
    entry.setProperty("seqGate", target.sequencerGate, nullptr);
    entry.setProperty("seqProb", target.sequencerProbOn, nullptr);
    entry.setProperty("seqJitter", target.sequencerJitterOn, nullptr);
    entry.setProperty("oscWave", target.oscWaveIndex, nullptr);
    entry.setProperty("masterComp", target.masterCompressor, nullptr);
    entry.setProperty("masterMix", target.masterMix, nullptr);
    entry.setProperty("monoMaker", target.monoMaker, nullptr);
    entry.setProperty("monoFreq", target.monoMakerFreqHz, nullptr);
    entry.setProperty("evolution", target.evolution, nullptr);
    entry.setProperty("seqPattern", (int) target.rhythmProfile.archetype, nullptr);
    entry.setProperty("seqDensityBias", target.rhythmProfile.densityBias, nullptr);
    entry.setProperty("seqAnchorBias", target.rhythmProfile.anchorBias, nullptr);
    entry.setProperty("seqOffbeatBias", target.rhythmProfile.offbeatBias, nullptr);
    entry.setProperty("seqTripletBias", target.rhythmProfile.tripletBias, nullptr);
    entry.setProperty("seqSwing", target.rhythmProfile.swingAmount, nullptr);
    entry.setProperty("seqHumanize", target.rhythmProfile.humanize, nullptr);
    entry.setProperty("seqTightness", target.rhythmProfile.tightness, nullptr);
    entry.setProperty("summary", target.summary, nullptr);
}

PromptPatchTarget readPromptTargetFromMemoryEntry(const juce::ValueTree& entry)
{
    PromptPatchTarget target;
    for (size_t i = 0; i < factoryPresetDefaultValues.size(); ++i)
        target.values[i] = juce::jlimit(0.0f, 1.0f, stateValue(entry, promptMemoryValuePropertyName((int) i).toRawUTF8(), factoryPresetDefaultValues[i]));

    target.sequencerOn = (bool) entry.getProperty("seqOn", false);
    target.sequencerRateIndex = juce::jlimit(0, 3, (int) entry.getProperty("seqRate", 1));
    target.sequencerStepCount = juce::jlimit(4, 16, (int) entry.getProperty("seqSteps", 8));
    target.sequencerGate = juce::jlimit(0.12f, 0.92f, stateValue(entry, "seqGate", 0.5f));
    target.sequencerProbOn = (bool) entry.getProperty("seqProb", false);
    target.sequencerJitterOn = (bool) entry.getProperty("seqJitter", false);
    target.oscWaveIndex = juce::jlimit(0, 2, (int) entry.getProperty("oscWave", 0));
    target.masterCompressor = juce::jlimit(0.0f, 1.0f, stateValue(entry, "masterComp", 0.12f));
    target.masterMix = juce::jlimit(0.0f, 1.0f, stateValue(entry, "masterMix", 0.88f));
    target.monoMaker = (bool) entry.getProperty("monoMaker", false);
    target.monoMakerFreqHz = juce::jlimit(42.0f, 220.0f, stateValue(entry, "monoFreq", 80.0f));
    target.evolution = juce::jlimit(0.0f, 1.0f, stateValue(entry, "evolution", 0.5f));
    target.rhythmProfile.archetype = promptPatternArchetypeFromValue((int) entry.getProperty("seqPattern", 0));
    target.rhythmProfile.densityBias = juce::jlimit(-0.40f, 0.40f, stateValue(entry, "seqDensityBias", 0.0f));
    target.rhythmProfile.anchorBias = juce::jlimit(-0.40f, 0.40f, stateValue(entry, "seqAnchorBias", 0.0f));
    target.rhythmProfile.offbeatBias = juce::jlimit(0.0f, 1.0f, stateValue(entry, "seqOffbeatBias", 0.0f));
    target.rhythmProfile.tripletBias = juce::jlimit(0.0f, 1.0f, stateValue(entry, "seqTripletBias", 0.0f));
    target.rhythmProfile.swingAmount = juce::jlimit(0.0f, 1.0f, stateValue(entry, "seqSwing", 0.0f));
    target.rhythmProfile.humanize = juce::jlimit(0.0f, 1.0f, stateValue(entry, "seqHumanize", 0.0f));
    target.rhythmProfile.tightness = juce::jlimit(0.18f, 0.94f, stateValue(entry, "seqTightness", 0.58f));
    target.summary = entry.getProperty("summary", {}).toString();
    return target;
}

PromptPatchTarget capturePromptPatchTargetFromState(const juce::ValueTree& state)
{
    PromptPatchTarget target;
    for (size_t i = 0; i < factoryPresetDefaultValues.size(); ++i)
        target.values[i] = juce::jlimit(0.0f, 1.0f, stateValue(state, factoryPresetValueParameterIDs[i], factoryPresetDefaultValues[i]));

    target.sequencerOn = (bool) state.getProperty("sequencerOn", false);
    target.sequencerRateIndex = juce::jlimit(0, 3, (int) state.getProperty("sequencerRate", 1));
    target.sequencerStepCount = juce::jlimit(4, 16, (int) state.getProperty("sequencerSteps", 8));
    target.sequencerGate = juce::jlimit(0.12f, 0.92f, stateValue(state, "sequencerGate", 0.5f));
    target.sequencerProbOn = (bool) state.getProperty("sequencerProbOn", true);
    target.sequencerJitterOn = (bool) state.getProperty("sequencerJitterOn", true);
    target.oscWaveIndex = juce::jlimit(0, 2, (int) state.getProperty("oscWave", 0));
    target.masterCompressor = juce::jlimit(0.0f, 1.0f, stateValue(state, "masterCompressor", 0.12f));
    target.masterMix = juce::jlimit(0.0f, 1.0f, stateValue(state, "masterMix", 0.88f));
    target.monoMaker = (bool) state.getProperty("monoMakerToggle", false);
    target.monoMakerFreqHz = juce::jlimit(42.0f, 220.0f, stateValue(state, "monoMakerFreq", 80.0f));
    target.evolution = juce::jlimit(0.0f, 1.0f, stateValue(state, "evolution", 0.5f));
    return target;
}

juce::ValueTree makePromptMemoryEntry(const juce::String& prompt,
                                      const juce::String& summary,
                                      const PromptPatchTarget& target,
                                      float strength,
                                      int uses = 1)
{
    juce::ValueTree entry(promptMemoryEntryTag);
    const auto normalizedPrompt = normalizePromptText(prompt).trim();
    const auto tokens = tokenizePrompt(prompt);

    entry.setProperty("prompt", prompt, nullptr);
    entry.setProperty("normalized", normalizedPrompt, nullptr);
    entry.setProperty("tokens", tokens.joinIntoString("|"), nullptr);
    entry.setProperty("summary", summary.isNotEmpty() ? summary : target.summary, nullptr);
    entry.setProperty("strength", juce::jlimit(0.2f, 6.0f, strength), nullptr);
    entry.setProperty("uses", juce::jmax(1, uses), nullptr);
    writePromptTargetToMemoryEntry(entry, target);
    return entry;
}

float scorePromptMemoryEntryMatch(const juce::String& normalizedPrompt,
                                  const juce::StringArray& promptTokens,
                                  const juce::ValueTree& entry)
{
    const auto entryNormalized = entry.getProperty("normalized", {}).toString().trim();
    if (entryNormalized.isEmpty())
        return 0.0f;

    if (normalizedPrompt == entryNormalized)
        return 1.15f;

    juce::StringArray entryTokens;
    entryTokens.addTokens(entry.getProperty("tokens", {}).toString(), "|", "");
    entryTokens.removeEmptyStrings();

    const float tokenScore = promptTokenSimilarity(promptTokens, entryTokens);
    const bool containsPhrase = normalizedPrompt.length() >= 6
                             && entryNormalized.length() >= 6
                             && (normalizedPrompt.contains(entryNormalized) || entryNormalized.contains(normalizedPrompt));
    const float containsBonus = containsPhrase ? 0.18f : 0.0f;
    const float strength = (float) entry.getProperty("strength", 1.0f);
    const float useBoost = juce::jlimit(0.0f, 0.16f, (float) (int) entry.getProperty("uses", 1) * 0.03f);

    return juce::jlimit(0.0f, 1.35f,
                        (tokenScore + containsBonus) * juce::jlimit(0.72f, 1.20f, 0.84f + strength * 0.06f)
                        + useBoost);
}

PromptMemoryMatch findPromptMemoryMatch(const juce::String& prompt, const std::vector<juce::ValueTree>& entries)
{
    PromptMemoryMatch bestMatch;
    const auto normalizedPrompt = normalizePromptText(prompt).trim();
    const auto promptTokens = tokenizePrompt(prompt);

    for (const auto& entry : entries)
    {
        if (! entry.hasType(promptMemoryEntryTag))
            continue;

        const float score = scorePromptMemoryEntryMatch(normalizedPrompt, promptTokens, entry);
        if (score > bestMatch.score)
        {
            bestMatch.entry = entry.createCopy();
            bestMatch.score = score;
        }
    }

    if (bestMatch.score < 0.34f)
    {
        bestMatch.entry = {};
        bestMatch.score = 0.0f;
        bestMatch.blendAmount = 0.0f;
        return bestMatch;
    }

    bestMatch.blendAmount = juce::jlimit(0.14f, 0.58f, 0.10f + (bestMatch.score - 0.34f) * 0.58f);
    return bestMatch;
}

void blendPromptPatchTargets(PromptPatchTarget& base, const PromptPatchTarget& memory, float amount)
{
    const float blend = juce::jlimit(0.0f, 1.0f, amount);

    for (size_t i = 0; i < base.values.size(); ++i)
        base.values[i] = juce::jlimit(0.0f, 1.0f, mixFloat(base.values[i], memory.values[i], blend));

    if (blend > 0.30f)
        base.sequencerOn = base.sequencerOn || memory.sequencerOn;

    if (blend > 0.24f)
    {
        base.sequencerProbOn = base.sequencerProbOn || memory.sequencerProbOn;
        base.sequencerJitterOn = base.sequencerJitterOn || memory.sequencerJitterOn;
    }

    if (blend > 0.34f)
        base.monoMaker = base.monoMaker || memory.monoMaker;

    blendPromptRhythmProfiles(base.rhythmProfile, memory.rhythmProfile, blend);
    base.sequencerRateIndex = juce::jlimit(0, 3, mixInt(base.sequencerRateIndex, memory.sequencerRateIndex, blend));
    base.sequencerStepCount = juce::jlimit(4, 16, mixInt(base.sequencerStepCount, memory.sequencerStepCount, blend));
    base.sequencerGate = juce::jlimit(0.12f, 0.92f, mixFloat(base.sequencerGate, memory.sequencerGate, blend));
    base.oscWaveIndex = juce::jlimit(0, 2, mixInt(base.oscWaveIndex, memory.oscWaveIndex, blend));
    base.masterCompressor = juce::jlimit(0.0f, 1.0f, mixFloat(base.masterCompressor, memory.masterCompressor, blend));
    base.masterMix = juce::jlimit(0.0f, 1.0f, mixFloat(base.masterMix, memory.masterMix, blend));
    base.monoMakerFreqHz = juce::jlimit(42.0f, 220.0f, mixFloat(base.monoMakerFreqHz, memory.monoMakerFreqHz, blend));
    base.evolution = juce::jlimit(0.0f, 1.0f, mixFloat(base.evolution, memory.evolution, blend));
}

void setNormalizedParameter(juce::AudioProcessorValueTreeState& tree, const char* paramID, float normalizedValue)
{
    if (auto* parameter = tree.getParameter(paramID))
        parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalizedValue));
}

void setRangedParameter(juce::AudioProcessorValueTreeState& tree, const char* paramID, float realValue)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(tree.getParameter(paramID)))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(realValue));
}

void applyPromptTargetParameters(juce::AudioProcessorValueTreeState& tree, const PromptPatchTarget& target)
{
    for (size_t i = 0; i < factoryPresetValueParameterIDs.size(); ++i)
        setNormalizedParameter(tree, factoryPresetValueParameterIDs[i], target.values[i]);

    setNormalizedParameter(tree, "sequencerOn", target.sequencerOn ? 1.0f : 0.0f);
    setNormalizedParameter(tree, "sequencerRate", (float) target.sequencerRateIndex / 3.0f);
    setRangedParameter(tree, "sequencerSteps", (float) target.sequencerStepCount);
    setRangedParameter(tree, "sequencerGate", target.sequencerGate);
    setNormalizedParameter(tree, "sequencerProbOn", target.sequencerProbOn ? 1.0f : 0.0f);
    setNormalizedParameter(tree, "sequencerJitterOn", target.sequencerJitterOn ? 1.0f : 0.0f);
    setNormalizedParameter(tree, "oscWave", (float) target.oscWaveIndex / 2.0f);
    setNormalizedParameter(tree, "masterCompressor", target.masterCompressor);
    setNormalizedParameter(tree, "masterMix", target.masterMix);
    setNormalizedParameter(tree, "monoMakerToggle", target.monoMaker ? 1.0f : 0.0f);
    setRangedParameter(tree, "monoMakerFreq", target.monoMakerFreqHz);
    setNormalizedParameter(tree, "evolution", target.evolution);
}

void applyGrowLocksToPromptTarget(PromptPatchTarget& target,
                                  int growLockMask,
                                  const juce::AudioProcessorValueTreeState& tree)
{
    auto preserveValue = [&](size_t index, const char* paramID, float fallback)
    {
        target.values[index] = getParameterValue(tree, paramID, fallback);
    };

    if (hasGrowLock(growLockMask, GrowLockGroup::root))
    {
        preserveValue(0, "rootDepth", factoryPresetDefaultValues[0]);
        preserveValue(1, "rootSoil", factoryPresetDefaultValues[1]);
        preserveValue(2, "rootAnchor", factoryPresetDefaultValues[2]);
        target.monoMaker = getParameterValue(tree, "monoMakerToggle", 0.0f) > 0.5f;
        target.monoMakerFreqHz = getParameterValue(tree, "monoMakerFreq", 80.0f);
    }

    if (hasGrowLock(growLockMask, GrowLockGroup::motion))
    {
        preserveValue(3, "sapFlow", factoryPresetDefaultValues[3]);
        preserveValue(4, "sapVitality", factoryPresetDefaultValues[4]);
        preserveValue(5, "sapTexture", factoryPresetDefaultValues[5]);
        preserveValue(6, "pulseRate", factoryPresetDefaultValues[6]);
        preserveValue(7, "pulseBreath", factoryPresetDefaultValues[7]);
        preserveValue(8, "pulseGrowth", factoryPresetDefaultValues[8]);
        target.evolution = getParameterValue(tree, "evolution", 0.5f);
    }

    if (hasGrowLock(growLockMask, GrowLockGroup::air))
    {
        preserveValue(9, "canopy", factoryPresetDefaultValues[9]);
        preserveValue(10, "atmosphere", factoryPresetDefaultValues[10]);
        preserveValue(15, "ecoSystem", factoryPresetDefaultValues[15]);
    }

    if (hasGrowLock(growLockMask, GrowLockGroup::fx))
    {
        preserveValue(11, "instability", factoryPresetDefaultValues[11]);
        preserveValue(12, "bloom", factoryPresetDefaultValues[12]);
        preserveValue(13, "rain", factoryPresetDefaultValues[13]);
        preserveValue(14, "sun", factoryPresetDefaultValues[14]);
        target.masterCompressor = getParameterValue(tree, "masterCompressor", 0.12f);
        target.masterMix = getParameterValue(tree, "masterMix", 0.88f);
    }

    if (hasGrowLock(growLockMask, GrowLockGroup::sequencer))
    {
        target.sequencerOn = getParameterValue(tree, "sequencerOn", 0.0f) > 0.5f;
        target.sequencerRateIndex = juce::jlimit(0, 3, juce::roundToInt(getParameterValue(tree, "sequencerRate", 1.0f)));
        target.sequencerStepCount = juce::jlimit(4, 16, juce::roundToInt(getParameterValue(tree, "sequencerSteps", 8.0f)));
        target.sequencerGate = juce::jlimit(0.12f, 0.92f, getParameterValue(tree, "sequencerGate", 0.5f));
        target.sequencerProbOn = getParameterValue(tree, "sequencerProbOn", 1.0f) > 0.5f;
        target.sequencerJitterOn = getParameterValue(tree, "sequencerJitterOn", 1.0f) > 0.5f;
    }
}

PromptPatchTarget resolvePromptPatchTarget(const juce::String& prompt,
                                           const std::vector<juce::ValueTree>& promptMemorySnapshot,
                                           bool* usedRecall = nullptr)
{
    auto target = buildPromptPatchTarget(prompt);
    const auto memoryMatch = findPromptMemoryMatch(prompt, promptMemorySnapshot);
    const bool recalled = memoryMatch.entry.isValid();

    if (recalled)
    {
        blendPromptPatchTargets(target, readPromptTargetFromMemoryEntry(memoryMatch.entry), memoryMatch.blendAmount);
        if (! target.summary.containsIgnoreCase("Recall"))
            target.summary << " / Recall";
    }

    if (usedRecall != nullptr)
        *usedRecall = recalled;

    return target;
}

bool selectMorphBoolean(bool a, bool b, float amount) noexcept
{
    const float morph = juce::jlimit(0.0f, 1.0f, amount);
    if (morph < 0.34f)
        return a;
    if (morph > 0.66f)
        return b;
    return a || b;
}

juce::String getPromptLeadTag(const juce::String& summary)
{
    auto tags = juce::StringArray::fromTokens(summary, "/", "");
    tags.trim();
    tags.removeEmptyStrings();
    return tags.isEmpty() ? juce::String() : tags[0];
}

juce::String buildPromptMorphSummary(const PromptPatchTarget& a, const PromptPatchTarget& b, float amount)
{
    const int percent = juce::roundToInt(juce::jlimit(0.0f, 1.0f, amount) * 100.0f);
    const auto aTag = getPromptLeadTag(a.summary);
    const auto bTag = getPromptLeadTag(b.summary);

    juce::String summary = "Morph " + juce::String(percent) + "%";
    if (aTag.isNotEmpty() || bTag.isNotEmpty())
        summary << " / " << (aTag.isNotEmpty() ? aTag : juce::String("Seed A"))
                << " -> " << (bTag.isNotEmpty() ? bTag : juce::String("Seed B"));

    return summary;
}

PromptPatchTarget morphPromptPatchTargets(const PromptPatchTarget& a,
                                          const PromptPatchTarget& b,
                                          float amount)
{
    const float morph = juce::jlimit(0.0f, 1.0f, amount);
    PromptPatchTarget target;

    for (size_t i = 0; i < target.values.size(); ++i)
        target.values[i] = juce::jlimit(0.0f, 1.0f, mixFloat(a.values[i], b.values[i], morph));

    target.sequencerOn = selectMorphBoolean(a.sequencerOn, b.sequencerOn, morph);
    target.sequencerRateIndex = juce::jlimit(0, 3, mixInt(a.sequencerRateIndex, b.sequencerRateIndex, morph));
    target.sequencerStepCount = juce::jlimit(4, 16, mixInt(a.sequencerStepCount, b.sequencerStepCount, morph));
    target.sequencerGate = juce::jlimit(0.12f, 0.92f, mixFloat(a.sequencerGate, b.sequencerGate, morph));
    target.sequencerProbOn = selectMorphBoolean(a.sequencerProbOn, b.sequencerProbOn, morph);
    target.sequencerJitterOn = selectMorphBoolean(a.sequencerJitterOn, b.sequencerJitterOn, morph);
    target.oscWaveIndex = juce::jlimit(0, 2, mixInt(a.oscWaveIndex, b.oscWaveIndex, morph));
    target.masterCompressor = juce::jlimit(0.0f, 1.0f, mixFloat(a.masterCompressor, b.masterCompressor, morph));
    target.masterMix = juce::jlimit(0.0f, 1.0f, mixFloat(a.masterMix, b.masterMix, morph));
    target.monoMaker = selectMorphBoolean(a.monoMaker, b.monoMaker, morph);
    target.monoMakerFreqHz = juce::jlimit(42.0f, 220.0f, mixFloat(a.monoMakerFreqHz, b.monoMakerFreqHz, morph));
    target.evolution = juce::jlimit(0.0f, 1.0f, mixFloat(a.evolution, b.evolution, morph));
    target.rhythmProfile = morphPromptRhythmProfiles(a.rhythmProfile, b.rhythmProfile, morph);
    target.summary = buildPromptMorphSummary(a, b, morph);
    return target;
}

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

RootFlowAudioProcessor::MutationMode RootFlowAudioProcessor::getCurrentMutationMode() const noexcept
{
    const int rawValue = mutationMode.load(std::memory_order_relaxed);
    if (rawValue < (int) MutationMode::gentle || rawValue > (int) MutationMode::sequencer)
        return MutationMode::balanced;

    return (MutationMode) rawValue;
}

void RootFlowAudioProcessor::cycleMutationMode()
{
    const auto nextMode = ((int) getCurrentMutationMode() + 1) % ((int) MutationMode::sequencer + 1);
    mutationMode.store(nextMode, std::memory_order_relaxed);
}

void RootFlowAudioProcessor::setMutationMode(MutationMode mode) noexcept
{
    mutationMode.store((int) mode, std::memory_order_relaxed);
}

RootFlowAudioProcessor::MutationMode RootFlowAudioProcessor::getMutationMode() const noexcept
{
    return getCurrentMutationMode();
}

juce::String RootFlowAudioProcessor::getMutationModeShortLabel() const
{
    return ::getMutationModeShortLabel(getCurrentMutationMode());
}

juce::String RootFlowAudioProcessor::getMutationModeDisplayName() const
{
    return ::getMutationModeDisplayName(getCurrentMutationMode());
}

void RootFlowAudioProcessor::applyPromptPatch(const juce::String& prompt)
{
    const auto trimmedPrompt = prompt.trim();
    if (trimmedPrompt.isEmpty())
    {
        resetPromptRhythmState();
        const juce::ScopedLock lock(promptStateLock);
        lastPromptSummary = "Awaiting Seed";
        lastPromptSeed.clear();
        lastPromptSeedCanReinforce = false;
        return;
    }

    std::vector<juce::ValueTree> promptMemorySnapshot;
    {
        const juce::ScopedLock lock(promptMemoryLock);
        promptMemorySnapshot = promptMemoryEntries;
    }

    bool recalled = false;
    auto target = resolvePromptPatchTarget(trimmedPrompt, promptMemorySnapshot, &recalled);
    const int currentGrowLockMask = growLockMask.load(std::memory_order_relaxed);
    applyGrowLocksToPromptTarget(target, currentGrowLockMask, tree);

    if (! hasGrowLock(currentGrowLockMask, GrowLockGroup::sequencer))
    {
        currentPromptPatternArchetype = (int) target.rhythmProfile.archetype;
        currentPromptPatternDensityBias = target.rhythmProfile.densityBias;
        currentPromptPatternAnchorBias = target.rhythmProfile.anchorBias;
        currentPromptPatternOffbeatBias = target.rhythmProfile.offbeatBias;
        currentPromptPatternTripletBias = target.rhythmProfile.tripletBias;
        currentPromptPatternSwingAmount = target.rhythmProfile.swingAmount;
        currentPromptPatternHumanize = target.rhythmProfile.humanize;
        currentPromptPatternTightness = target.rhythmProfile.tightness;
    }

    presetLoadInProgress.fetch_add(1, std::memory_order_acq_rel);
    applyPromptTargetParameters(tree, target);
    presetLoadInProgress.fetch_sub(1, std::memory_order_acq_rel);

    if (target.sequencerOn && ! hasGrowLock(currentGrowLockMask, GrowLockGroup::sequencer))
        generateSmartSequencerPattern(getCurrentMutationMode() == MutationMode::sequencer ? MutationMode::sequencer
                                                                                          : MutationMode::balanced);

    {
        const juce::ScopedLock lock(promptStateLock);
        lastPromptSummary = target.summary;
        lastPromptSeed = trimmedPrompt;
        lastPromptSeedCanReinforce = true;
    }

    rememberPromptMemoryEntry(makePromptMemoryEntry(trimmedPrompt, target.summary, target,
                                                    recalled ? 0.30f : 0.48f),
                              recalled ? 0.18f : 0.30f);
    currentPresetDirty.store(1, std::memory_order_relaxed);
    requestProcessingStateReset();
    sequencerTriggered.store(true, std::memory_order_release);
}

void RootFlowAudioProcessor::applyPromptMorph(const juce::String& promptA,
                                              const juce::String& promptB,
                                              float morphAmount)
{
    const auto trimmedA = promptA.trim();
    const auto trimmedB = promptB.trim();

    if (trimmedA.isEmpty() && trimmedB.isEmpty())
    {
        resetPromptRhythmState();
        const juce::ScopedLock lock(promptStateLock);
        lastPromptSummary = "Awaiting Seed";
        lastPromptSeed.clear();
        lastPromptSeedCanReinforce = false;
        return;
    }

    if (trimmedB.isEmpty())
    {
        applyPromptPatch(trimmedA);
        return;
    }

    if (trimmedA.isEmpty())
    {
        applyPromptPatch(trimmedB);
        return;
    }

    std::vector<juce::ValueTree> promptMemorySnapshot;
    {
        const juce::ScopedLock lock(promptMemoryLock);
        promptMemorySnapshot = promptMemoryEntries;
    }

    bool recalledA = false;
    bool recalledB = false;
    const auto targetA = resolvePromptPatchTarget(trimmedA, promptMemorySnapshot, &recalledA);
    const auto targetB = resolvePromptPatchTarget(trimmedB, promptMemorySnapshot, &recalledB);
    auto target = morphPromptPatchTargets(targetA, targetB, morphAmount);
    const int currentGrowLockMask = growLockMask.load(std::memory_order_relaxed);
    applyGrowLocksToPromptTarget(target, currentGrowLockMask, tree);

    if (! hasGrowLock(currentGrowLockMask, GrowLockGroup::sequencer))
    {
        currentPromptPatternArchetype = (int) target.rhythmProfile.archetype;
        currentPromptPatternDensityBias = target.rhythmProfile.densityBias;
        currentPromptPatternAnchorBias = target.rhythmProfile.anchorBias;
        currentPromptPatternOffbeatBias = target.rhythmProfile.offbeatBias;
        currentPromptPatternTripletBias = target.rhythmProfile.tripletBias;
        currentPromptPatternSwingAmount = target.rhythmProfile.swingAmount;
        currentPromptPatternHumanize = target.rhythmProfile.humanize;
        currentPromptPatternTightness = target.rhythmProfile.tightness;
    }

    if (recalledA || recalledB)
        target.summary << " / Recall";

    presetLoadInProgress.fetch_add(1, std::memory_order_acq_rel);
    applyPromptTargetParameters(tree, target);
    presetLoadInProgress.fetch_sub(1, std::memory_order_acq_rel);

    if (target.sequencerOn && ! hasGrowLock(currentGrowLockMask, GrowLockGroup::sequencer))
        generateSmartSequencerPattern(getCurrentMutationMode() == MutationMode::sequencer ? MutationMode::sequencer
                                                                                          : MutationMode::balanced);

    const int morphPercent = juce::roundToInt(juce::jlimit(0.0f, 1.0f, morphAmount) * 100.0f);
    const juce::String morphSeedDescriptor = "morph " + juce::String(morphPercent) + ": "
                                          + trimmedA + " -> " + trimmedB;

    {
        const juce::ScopedLock lock(promptStateLock);
        lastPromptSummary = target.summary;
        lastPromptSeed = morphSeedDescriptor;
        lastPromptSeedCanReinforce = true;
    }

    rememberPromptMemoryEntry(makePromptMemoryEntry(morphSeedDescriptor,
                                                    target.summary,
                                                    target,
                                                    recalledA || recalledB ? 0.42f : 0.56f),
                              recalledA || recalledB ? 0.22f : 0.34f);
    currentPresetDirty.store(1, std::memory_order_relaxed);
    requestProcessingStateReset();
    sequencerTriggered.store(true, std::memory_order_release);
}

juce::String RootFlowAudioProcessor::getLastPromptSummary() const
{
    const juce::ScopedLock lock(promptStateLock);
    return lastPromptSummary;
}

juce::String RootFlowAudioProcessor::getLastPromptSeed() const
{
    const juce::ScopedLock lock(promptStateLock);
    return lastPromptSeed;
}

std::vector<RootFlowAudioProcessor::PromptMemoryPreview> RootFlowAudioProcessor::getPromptMemoryPreviews(int maxItems) const
{
    const int requestedItems = juce::jlimit(0, maxPromptMemoryEntries, maxItems);
    if (requestedItems <= 0)
        return {};

    juce::String lastSeed;
    juce::String lastSummary;
    {
        const juce::ScopedLock lock(promptStateLock);
        lastSeed = lastPromptSeed.trim();
        lastSummary = lastPromptSummary;
    }

    std::vector<juce::ValueTree> entries;
    {
        const juce::ScopedLock lock(promptMemoryLock);
        entries = promptMemoryEntries;
    }

    std::stable_sort(entries.begin(), entries.end(),
                     [] (const juce::ValueTree& a, const juce::ValueTree& b)
                     {
                         const float aScore = (float) a.getProperty("strength", 1.0f)
                                            + (float) (int) a.getProperty("uses", 1) * 0.16f;
                         const float bScore = (float) b.getProperty("strength", 1.0f)
                                            + (float) (int) b.getProperty("uses", 1) * 0.16f;
                         return aScore > bScore;
                     });

    std::vector<PromptMemoryPreview> previews;
    juce::StringArray normalizedPrompts;

    auto appendPreview = [&] (const juce::String& prompt,
                              const juce::String& summary,
                              float strength,
                              int uses)
    {
        const auto trimmedPrompt = prompt.trim();
        const auto normalizedPrompt = normalizePromptText(trimmedPrompt).trim();
        if (trimmedPrompt.isEmpty() || normalizedPrompt.isEmpty() || normalizedPrompts.contains(normalizedPrompt))
            return;

        PromptMemoryPreview preview;
        preview.prompt = trimmedPrompt;
        preview.summary = summary;
        preview.strength = strength;
        preview.uses = uses;
        previews.push_back(std::move(preview));
        normalizedPrompts.add(normalizedPrompt);
    };

    if (lastSeed.isNotEmpty())
        appendPreview(lastSeed, lastSummary, 7.0f, 1);

    for (const auto& entry : entries)
    {
        if (! entry.hasType(promptMemoryEntryTag))
            continue;

        appendPreview(entry.getProperty("prompt", {}).toString(),
                      entry.getProperty("summary", {}).toString(),
                      (float) entry.getProperty("strength", 1.0f),
                      juce::jmax(1, (int) entry.getProperty("uses", 1)));

        if ((int) previews.size() >= requestedItems)
            break;
    }

    if ((int) previews.size() > requestedItems)
        previews.resize((size_t) requestedItems);

    return previews;
}

bool RootFlowAudioProcessor::removePromptMemoryEntry(const juce::String& prompt)
{
    const auto trimmedPrompt = prompt.trim();
    const auto normalizedPrompt = normalizePromptText(trimmedPrompt).trim();
    if (normalizedPrompt.isEmpty())
        return false;

    bool removedAny = false;
    {
        const juce::ScopedLock lock(promptMemoryLock);
        auto newEnd = std::remove_if(promptMemoryEntries.begin(),
                                     promptMemoryEntries.end(),
                                     [&] (const juce::ValueTree& entry)
                                     {
                                         if (! entry.hasType(promptMemoryEntryTag))
                                             return false;

                                         const auto entryNormalized = entry.getProperty("normalized", {}).toString().trim();
                                         const bool shouldRemove = entryNormalized == normalizedPrompt;
                                         removedAny = removedAny || shouldRemove;
                                         return shouldRemove;
                                     });
        promptMemoryEntries.erase(newEnd, promptMemoryEntries.end());
    }

    {
        const juce::ScopedLock lock(promptStateLock);
        if (normalizePromptText(lastPromptSeed).trim() == normalizedPrompt)
        {
            lastPromptSeed.clear();
            lastPromptSeedCanReinforce = false;
            lastPromptSummary = "Awaiting Seed";
            removedAny = true;
        }
    }

    if (removedAny)
        currentPresetDirty.store(1, std::memory_order_relaxed);

    return removedAny;
}

void RootFlowAudioProcessor::setGrowLockEnabled(GrowLockGroup group, bool shouldEnable) noexcept
{
    const int bit = growLockBit(group);
    int currentMask = growLockMask.load(std::memory_order_relaxed);
    int nextMask = currentMask;

    do
    {
        nextMask = shouldEnable ? (currentMask | bit) : (currentMask & ~bit);
    }
    while (! growLockMask.compare_exchange_weak(currentMask, nextMask,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));
}

void RootFlowAudioProcessor::toggleGrowLock(GrowLockGroup group) noexcept
{
    setGrowLockEnabled(group, ! isGrowLockEnabled(group));
}

bool RootFlowAudioProcessor::isGrowLockEnabled(GrowLockGroup group) const noexcept
{
    return hasGrowLock(growLockMask.load(std::memory_order_relaxed), group);
}

void RootFlowAudioProcessor::resetPromptRhythmState() noexcept
{
    currentPromptPatternArchetype = 0;
    currentPromptPatternDensityBias = 0.0f;
    currentPromptPatternAnchorBias = 0.0f;
    currentPromptPatternOffbeatBias = 0.0f;
    currentPromptPatternTripletBias = 0.0f;
    currentPromptPatternSwingAmount = 0.0f;
    currentPromptPatternHumanize = 0.0f;
    currentPromptPatternTightness = 0.58f;
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
    resetPromptRhythmState();
    {
        const juce::ScopedLock lock(promptStateLock);
        lastPromptSummary = "Awaiting Seed";
        lastPromptSeed.clear();
        lastPromptSeedCanReinforce = false;
    }
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
    restoreSequencerState(userPresetState);
    clearPendingMappedParameterChanges();
    currentFactoryPresetIndex.store(-1, std::memory_order_relaxed);
    currentUserPresetIndex.store(userIndex, std::memory_order_relaxed);
    currentPresetDirty.store(0, std::memory_order_relaxed);
    resetPromptRhythmState();
    {
        const juce::ScopedLock lock(promptStateLock);
        lastPromptSummary = "Awaiting Seed";
        lastPromptSeed.clear();
        lastPromptSeedCanReinforce = false;
    }
    requestProcessingStateReset();
}

void RootFlowAudioProcessor::saveCurrentStateAsUserPreset()
{
    const auto capturedState = captureStateForUserPreset();
    juce::String promptSeedToRemember;
    juce::String promptSummaryToRemember;
    {
        const juce::ScopedLock lock(promptStateLock);
        if (lastPromptSeedCanReinforce && lastPromptSeed.isNotEmpty())
        {
            promptSeedToRemember = lastPromptSeed;
            promptSummaryToRemember = lastPromptSummary;
        }
    }

    if (promptSeedToRemember.isNotEmpty())
    {
        auto rememberedTarget = capturePromptPatchTargetFromState(capturedState);
        rememberedTarget.summary = promptSummaryToRemember;
        rememberedTarget.rhythmProfile.archetype = promptPatternArchetypeFromValue(currentPromptPatternArchetype);
        rememberedTarget.rhythmProfile.densityBias = currentPromptPatternDensityBias;
        rememberedTarget.rhythmProfile.anchorBias = currentPromptPatternAnchorBias;
        rememberedTarget.rhythmProfile.offbeatBias = currentPromptPatternOffbeatBias;
        rememberedTarget.rhythmProfile.tripletBias = currentPromptPatternTripletBias;
        rememberedTarget.rhythmProfile.swingAmount = currentPromptPatternSwingAmount;
        rememberedTarget.rhythmProfile.humanize = currentPromptPatternHumanize;
        rememberedTarget.rhythmProfile.tightness = currentPromptPatternTightness;
        rememberPromptMemoryEntry(makePromptMemoryEntry(promptSeedToRemember,
                                                        promptSummaryToRemember,
                                                        rememberedTarget,
                                                        1.20f),
                                  0.92f);
    }

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

void RootFlowAudioProcessor::appendSequencerState(juce::ValueTree& state) const
{
    if (auto existing = state.getChildWithName(sequencerStateTag); existing.isValid())
        state.removeChild(existing, nullptr);

    juce::ValueTree sequencerState(sequencerStateTag);
    sequencerState.setProperty("currentStep", currentSequencerStep.load(std::memory_order_relaxed), nullptr);

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    for (int i = 0; i < (int) sequencerSteps.size(); ++i)
    {
        const auto& step = sequencerSteps[(size_t) i];
        juce::ValueTree stepState(sequencerStepTag);
        stepState.setProperty("index", i, nullptr);
        stepState.setProperty("active", step.active, nullptr);
        stepState.setProperty("velocity", step.velocity, nullptr);
        stepState.setProperty("probability", step.probability, nullptr);
        stepState.setProperty("timingOffset", step.timingOffset, nullptr);
        sequencerState.addChild(stepState, -1, nullptr);
    }

    state.addChild(sequencerState, -1, nullptr);
}

void RootFlowAudioProcessor::restoreSequencerState(const juce::ValueTree& state)
{
    auto sequencerState = state.getChildWithName(sequencerStateTag);
    if (! sequencerState.isValid())
        return;

    std::array<SequencerStep, 16> restoredSteps {};
    for (auto& step : restoredSteps)
    {
        step.active = false;
        step.velocity = 0.8f;
        step.probability = 1.0f;
    }

    for (int i = 0; i < sequencerState.getNumChildren(); ++i)
    {
        auto stepState = sequencerState.getChild(i);
        if (! stepState.hasType(sequencerStepTag))
            continue;

        const int index = (int) stepState.getProperty("index", -1);
        if (! juce::isPositiveAndBelow(index, (int) restoredSteps.size()))
            continue;

        auto& step = restoredSteps[(size_t) index];
        step.active = (bool) stepState.getProperty("active", false);
        step.velocity = juce::jlimit(0.05f, 1.0f, (float) stepState.getProperty("velocity", 0.8f));
        step.probability = juce::jlimit(0.0f, 1.0f, (float) stepState.getProperty("probability", 1.0f));
        step.timingOffset = clampSequencerTimingOffset((float) stepState.getProperty("timingOffset", 0.0f));
    }

    {
        const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
        sequencerSteps = restoredSteps;
    }

    currentSequencerStep.store((int) sequencerState.getProperty("currentStep", 0), std::memory_order_relaxed);
}

void RootFlowAudioProcessor::appendPromptMemoryState(juce::ValueTree& state) const
{
    if (auto existing = state.getChildWithName(promptMemoryStateTag); existing.isValid())
        state.removeChild(existing, nullptr);

    juce::ValueTree promptMemoryState(promptMemoryStateTag);
    promptMemoryState.setProperty(promptMemoryVersionProperty, promptMemorySchemaVersion, nullptr);

    {
        const juce::ScopedLock promptLock(promptStateLock);
        promptMemoryState.setProperty(lastPromptSummaryProperty, lastPromptSummary, nullptr);
        promptMemoryState.setProperty(lastPromptSeedProperty, lastPromptSeed, nullptr);
        promptMemoryState.setProperty(lastPromptSeedValidProperty, lastPromptSeedCanReinforce, nullptr);
    }

    const juce::ScopedLock memoryLock(promptMemoryLock);
    for (const auto& entry : promptMemoryEntries)
    {
        if (entry.hasType(promptMemoryEntryTag))
            promptMemoryState.addChild(entry.createCopy(), -1, nullptr);
    }

    state.addChild(promptMemoryState, -1, nullptr);
}

void RootFlowAudioProcessor::restorePromptMemoryState(const juce::ValueTree& state)
{
    std::vector<juce::ValueTree> restoredEntries;
    juce::String restoredSummary = "Awaiting Seed";
    juce::String restoredSeed;
    bool restoredSeedValid = false;

    if (auto promptMemoryState = state.getChildWithName(promptMemoryStateTag); promptMemoryState.isValid())
    {
        restoredSummary = promptMemoryState.getProperty(lastPromptSummaryProperty, "Awaiting Seed").toString();
        restoredSeed = promptMemoryState.getProperty(lastPromptSeedProperty, {}).toString();
        restoredSeedValid = (bool) promptMemoryState.getProperty(lastPromptSeedValidProperty, restoredSeed.isNotEmpty());

        for (int i = 0; i < promptMemoryState.getNumChildren(); ++i)
        {
            auto entry = promptMemoryState.getChild(i);
            if (! entry.hasType(promptMemoryEntryTag))
                continue;

            restoredEntries.push_back(entry.createCopy());
        }
    }
    else
    {
        restoredSummary = "Awaiting Seed";
        restoredSeed.clear();
        restoredSeedValid = false;
    }

    {
        const juce::ScopedLock lock(promptStateLock);
        lastPromptSummary = restoredSummary.isNotEmpty() ? restoredSummary : juce::String("Awaiting Seed");
        lastPromptSeed = restoredSeed;
        lastPromptSeedCanReinforce = restoredSeedValid && restoredSeed.isNotEmpty();
    }

    {
        const juce::ScopedLock lock(promptMemoryLock);
        promptMemoryEntries = std::move(restoredEntries);
    }
}

void RootFlowAudioProcessor::rememberPromptMemoryEntry(juce::ValueTree entry, float reinforcement)
{
    if (! entry.hasType(promptMemoryEntryTag))
        return;

    const auto normalizedPrompt = entry.getProperty("normalized", {}).toString().trim();
    if (normalizedPrompt.isEmpty())
        return;

    entry.setProperty("strength",
                      juce::jlimit(0.2f, 6.0f, (float) entry.getProperty("strength", 1.0f) + reinforcement),
                      nullptr);
    entry.setProperty("uses", juce::jmax(1, (int) entry.getProperty("uses", 1)), nullptr);

    const auto entryTokens = entry.getProperty("tokens", {}).toString();

    const juce::ScopedLock lock(promptMemoryLock);
    int bestIndex = -1;
    float bestScore = 0.0f;

    juce::StringArray promptTokens;
    promptTokens.addTokens(entryTokens, "|", "");
    promptTokens.removeEmptyStrings();

    for (int i = 0; i < (int) promptMemoryEntries.size(); ++i)
    {
        const auto& existing = promptMemoryEntries[(size_t) i];
        if (! existing.hasType(promptMemoryEntryTag))
            continue;

        const auto existingNormalized = existing.getProperty("normalized", {}).toString().trim();
        if (existingNormalized == normalizedPrompt)
        {
            bestIndex = i;
            bestScore = 1.15f;
            break;
        }

        const float score = scorePromptMemoryEntryMatch(normalizedPrompt, promptTokens, existing);
        if (score > bestScore)
        {
            bestIndex = i;
            bestScore = score;
        }
    }

    if (bestIndex >= 0 && bestScore >= 0.72f)
    {
        auto& existing = promptMemoryEntries[(size_t) bestIndex];
        auto existingTarget = readPromptTargetFromMemoryEntry(existing);
        const auto newTarget = readPromptTargetFromMemoryEntry(entry);
        blendPromptPatchTargets(existingTarget, newTarget, juce::jlimit(0.18f, 0.86f, 0.20f + reinforcement * 0.24f));
        writePromptTargetToMemoryEntry(existing, existingTarget);
        existing.setProperty("prompt", entry.getProperty("prompt", {}), nullptr);
        existing.setProperty("normalized", normalizedPrompt, nullptr);
        existing.setProperty("tokens", entryTokens, nullptr);
        existing.setProperty("summary", entry.getProperty("summary", {}), nullptr);
        existing.setProperty("strength",
                             juce::jlimit(0.2f, 6.0f, (float) existing.getProperty("strength", 1.0f) + reinforcement),
                             nullptr);
        existing.setProperty("uses", (int) existing.getProperty("uses", 1) + 1, nullptr);
    }
    else
    {
        promptMemoryEntries.push_back(entry.createCopy());
    }

    while ((int) promptMemoryEntries.size() > maxPromptMemoryEntries)
    {
        int weakestIndex = 0;
        float weakestScore = std::numeric_limits<float>::max();

        for (int i = 0; i < (int) promptMemoryEntries.size(); ++i)
        {
            const auto& existing = promptMemoryEntries[(size_t) i];
            const float entryScore = (float) existing.getProperty("strength", 1.0f)
                                   + (float) (int) existing.getProperty("uses", 1) * 0.08f;
            if (entryScore < weakestScore)
            {
                weakestScore = entryScore;
                weakestIndex = i;
            }
        }

        promptMemoryEntries.erase(promptMemoryEntries.begin() + weakestIndex);
    }
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
    state.setProperty(mutationModeProperty, mutationModeToStateString(getCurrentMutationMode()), nullptr);
    state.setProperty(growLocksProperty, growLockMask.load(std::memory_order_relaxed), nullptr);
    state.setProperty(hoverEffectsEnabledProperty, RootFlow::areHoverEffectsEnabled(), nullptr);
    state.setProperty(idleEffectsEnabledProperty, RootFlow::areIdleEffectsEnabled(), nullptr);
    state.setProperty(popupOverlaysEnabledProperty, RootFlow::arePopupOverlaysEnabled(), nullptr);
    appendSequencerState(state);
    appendPromptMemoryState(state);

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
    resetPromptRhythmState();
    mutationMode.store((int) mutationModeFromStateValue(state.getProperty(mutationModeProperty, "balanced")),
                       std::memory_order_relaxed);
    growLockMask.store((int) state.getProperty(growLocksProperty, 0), std::memory_order_relaxed);
    RootFlow::setHoverEffectsEnabled((bool) state.getProperty(hoverEffectsEnabledProperty, true));
    RootFlow::setIdleEffectsEnabled((bool) state.getProperty(idleEffectsEnabledProperty, false));
    RootFlow::setPopupOverlaysEnabled((bool) state.getProperty(popupOverlaysEnabledProperty, false));
    restoreSequencerState(state);
    restorePromptMemoryState(state);

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
    appendSequencerState(state);
    if (auto child = state.getChildWithName(midiMappingStateTag); child.isValid())
        state.removeChild(child, nullptr);
    if (auto child = state.getChildWithName(midiBindingsTag); child.isValid())
        state.removeChild(child, nullptr);
    if (auto child = state.getChildWithName(userPresetsTag); child.isValid())
        state.removeChild(child, nullptr);
    if (auto child = state.getChildWithName(promptMemoryStateTag); child.isValid())
        state.removeChild(child, nullptr);
    state.removeProperty(presetIndexProperty, nullptr);
    state.removeProperty(mutationModeProperty, nullptr);
    state.removeProperty(growLocksProperty, nullptr);
    state.removeProperty(hoverEffectsEnabledProperty, nullptr);
    state.removeProperty(idleEffectsEnabledProperty, nullptr);
    state.removeProperty(popupOverlaysEnabledProperty, nullptr);
    state.removeProperty(lastPromptSummaryProperty, nullptr);
    state.removeProperty(lastPromptSeedProperty, nullptr);
    state.removeProperty(lastPromptSeedValidProperty, nullptr);
    return state;
}

void RootFlowAudioProcessor::markSequencerStateDirty() noexcept
{
    currentPresetDirty.store(1, std::memory_order_relaxed);
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
    appendCustomState(state);

    if (auto existingConnections = state.getChildWithName("CONNECTIONS"); existingConnections.isValid())
        state.removeChild(existingConnections, nullptr);
    
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

void RootFlowAudioProcessor::generateSmartSequencerPattern(MutationMode mode)
{
    auto& rng = juce::Random::getSystemRandom();
    const auto context = makeMutationContext(tree);
    const int stepCount = getSequencerLengthFromTree(tree);
    PromptRhythmProfile promptRhythmProfile;
    promptRhythmProfile.archetype = promptPatternArchetypeFromValue(currentPromptPatternArchetype);
    promptRhythmProfile.densityBias = currentPromptPatternDensityBias;
    promptRhythmProfile.anchorBias = currentPromptPatternAnchorBias;
    promptRhythmProfile.offbeatBias = currentPromptPatternOffbeatBias;
    promptRhythmProfile.tripletBias = currentPromptPatternTripletBias;
    promptRhythmProfile.swingAmount = currentPromptPatternSwingAmount;
    promptRhythmProfile.humanize = currentPromptPatternHumanize;
    promptRhythmProfile.tightness = currentPromptPatternTightness;
    std::array<SequencerStep, 16> generatedSteps {};
    buildSmartSequencerSteps(generatedSteps, stepCount, mode, context, rng, promptRhythmProfile);

    {
        const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
        sequencerSteps = generatedSteps;
    }

    currentSequencerStep.store(0, std::memory_order_relaxed);
    markSequencerStateDirty();
}

void RootFlowAudioProcessor::clearSequencerSteps()
{
    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    for (auto& step : sequencerSteps)
    {
        step.active = false;
        step.velocity = 0.8f;
        step.probability = 1.0f;
        step.timingOffset = 0.0f;
    }

    markSequencerStateDirty();
}

void RootFlowAudioProcessor::randomizeSequencerStepVelocity(int stepIndex)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    auto& step = sequencerSteps[(size_t) stepIndex];
    step.active = true;
    step.velocity = juce::Random::getSystemRandom().nextFloat() * 0.8f + 0.2f;
    markSequencerStateDirty();
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

    markSequencerStateDirty();
}

void RootFlowAudioProcessor::toggleSequencerStepActive(int stepIndex)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    sequencerSteps[(size_t) stepIndex].active = ! sequencerSteps[(size_t) stepIndex].active;
    markSequencerStateDirty();
}

void RootFlowAudioProcessor::adjustSequencerStepVelocity(int stepIndex, float delta)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    auto& step = sequencerSteps[(size_t) stepIndex];
    step.velocity = juce::jlimit(0.05f, 1.0f, step.velocity + delta);
    markSequencerStateDirty();
}

void RootFlowAudioProcessor::adjustSequencerStepProbability(int stepIndex, float delta)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    auto& step = sequencerSteps[(size_t) stepIndex];
    step.probability = juce::jlimit(0.0f, 1.0f, step.probability + delta);
    markSequencerStateDirty();
}

void RootFlowAudioProcessor::randomizeSequencerStepProbability(int stepIndex)
{
    if (! juce::isPositiveAndBelow(stepIndex, (int) sequencerSteps.size()))
        return;

    const juce::SpinLock::ScopedLockType lock(sequencerStateLock);
    auto& step = sequencerSteps[(size_t) stepIndex];
    step.probability = juce::Random::getSystemRandom().nextFloat();
    markSequencerStateDirty();
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
                        const int styleOffsetSamples = juce::roundToInt(step.timingOffset * (float) samplesPerStep * 0.24f);
                        const int jitterSamples = jitterOn
                            ? juce::roundToInt((rng.nextFloat() - 0.5f) * instability * samplesPerStep
                                               * (0.08f + std::abs(step.timingOffset) * 0.14f))
                            : 0;
                        const int triggerPos = juce::jlimit(0, numSamples - 1, i + styleOffsetSamples + jitterSamples);
                        
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
    auto& r = juce::Random::getSystemRandom();
    const auto mode = getCurrentMutationMode();
    const auto context = makeMutationContext(tree);
    const auto profile = getMutationProfile(mode, context);
    bool mutatedAnyParameter = false;

    for (const auto* id : mutationParameterIDs)
    {
        const juce::String paramID(id);
        auto* param = tree.getParameter(paramID);
        if (param == nullptr)
            continue;

        const auto lane = getMutationLane(paramID);
        const float laneScale = getLaneScale(lane, mode, context, profile);
        if (laneScale <= 0.0f)
            continue;

        const float previousValue = param->getValue();

        if (isDiscreteMutationParameter(paramID))
            mutateDiscreteParameter(*param, paramID, r, profile, laneScale);
        else
            mutateContinuousParameter(*param, paramID, r, lane, context, profile, laneScale);

        mutatedAnyParameter = mutatedAnyParameter || previousValue != param->getValue();
    }

    if (mode == MutationMode::sequencer)
    {
        if (auto* sequencerOn = tree.getParameter("sequencerOn"))
        {
            if (sequencerOn->getValue() < 0.5f)
            {
                sequencerOn->setValueNotifyingHost(1.0f);
                mutatedAnyParameter = true;
            }
        }

        if (auto* probabilityOn = tree.getParameter("sequencerProbOn"))
        {
            if (probabilityOn->getValue() < 0.5f)
            {
                probabilityOn->setValueNotifyingHost(1.0f);
                mutatedAnyParameter = true;
            }
        }

        if (auto* jitterOn = tree.getParameter("sequencerJitterOn"))
        {
            const float target = context.instability > 0.34f || context.evolution > 0.58f ? 1.0f : 0.0f;
            if (std::abs(jitterOn->getValue() - target) > 0.01f)
            {
                jitterOn->setValueNotifyingHost(target);
                mutatedAnyParameter = true;
            }
        }

        generateSmartSequencerPattern(mode);
    }

    if (mutatedAnyParameter)
        currentPresetDirty.store(1, std::memory_order_relaxed);

    // Trigger a visual energy pulse through the neural mycelium.
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
