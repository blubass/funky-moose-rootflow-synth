
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace
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

constexpr std::array<const char*, 21> presetParameterIDs {
    "rootDepth", "rootSoil", "rootAnchor",
    "sapFlow", "sapVitality", "sapTexture",
    "pulseRate", "pulseBreath", "pulseGrowth",
    "canopy", "atmosphere",
    "instability",
    "bloom", "rain", "sun",
    "ecoSystem",
    "sequencerOn", "sequencerRate", "sequencerSteps", "sequencerGate",
    "oscWave"
};

const std::array<FactoryPreset, 80> factoryPresets {{
    { "ROOTFLOW INIT", { 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.65f, 0.18f, 0.28f, 0.00f, 0.00f, 0.00f, 0.34f } },
    { "DEEP MYCELIUM",   { 0.36f, 0.28f, 0.16f, 0.62f, 0.78f, 0.40f, 0.30f, 0.34f, 0.28f, 0.68f, 0.26f, 0.14f, 0.42f, 0.20f, 0.10f, 0.08f } },
    { "CHLOROPHYLL SCREAM", { 0.42f, 0.30f, 0.24f, 0.54f, 0.72f, 0.30f, 0.24f, 0.32f, 0.26f, 0.62f, 0.18f, 0.10f, 0.20f, 0.12f, 0.06f, 0.12f } },
    { "PHOTOSYNTHESIS", { 0.44f, 0.34f, 0.22f, 0.58f, 0.66f, 0.36f, 0.28f, 0.30f, 0.34f, 0.64f, 0.24f, 0.12f, 0.28f, 0.12f, 0.38f, 0.22f } },
    { "WOODLAND RAIN",  { 0.52f, 0.48f, 0.42f, 0.46f, 0.52f, 0.24f, 0.20f, 0.26f, 0.20f, 0.58f, 0.20f, 0.08f, 0.12f, 0.18f, 0.22f, 0.56f } },
    { "ANCIENT OAK",    { 0.38f, 0.24f, 0.14f, 0.68f, 0.80f, 0.42f, 0.32f, 0.30f, 0.36f, 0.66f, 0.26f, 0.14f, 0.36f, 0.10f, 0.22f, 0.04f } },
    { "NEURAL ROOT",    { 0.30f, 0.22f, 0.12f, 0.64f, 0.82f, 0.30f, 0.18f, 0.24f, 0.18f, 0.60f, 0.20f, 0.08f, 0.18f, 0.08f, 0.04f, 0.18f } },
    { "MORNING DEW",    { 0.32f, 0.24f, 0.14f, 0.72f, 0.86f, 0.38f, 0.26f, 0.28f, 0.22f, 0.64f, 0.28f, 0.10f, 0.34f, 0.06f, 0.18f, 0.14f } },
    { "SILK CLEARING", { 0.40f, 0.28f, 0.18f, 0.58f, 0.74f, 0.28f, 0.22f, 0.30f, 0.24f, 0.56f, 0.18f, 0.06f, 0.16f, 0.04f, 0.30f, 0.20f } },

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
    { "Robo Sprout      ", { 0.40f, 0.20f, 0.40f, 0.50f, 0.30f, 0.20f, 1.00f, 0.50f, 0.80f, 0.65f, 0.18f, 0.28f, 0.30f, 0.30f, 0.40f, 0.50f } }
}};

constexpr std::array<FactoryPresetSectionDefinition, 14> factoryPresetSections {{
    { "START",    0, 1 },
    { "FRIENDLY", 1, 8 },
    { "MOTION",   9, 8 },
    { "DEEP",    17, 6 },
    { "WILD",    23, 7 },
    { "BIO: LEGACY",  30, 3 },
    { "BIO: GIANTS",  33, 6 },
    { "BIO: CREEPERS",39, 6 },
    { "BIO: LUMINA",  45, 6 },
    { "BIO: SPORES",  51, 5 },
    { "BIO: AQUATIC",  56, 5 },
    { "BIO: DESERT",   61, 5 },
    { "BIO: HUNTERS",  66, 4 },
    { "BIO: XENO",     70, 10 }
}};

constexpr auto midiBindingsTag = "MidiBindings";
constexpr auto midiMappingStateTag = "MidiMappingState";
constexpr auto midiBindingTag = "Binding";
constexpr auto userPresetsTag = "UserPresets";
constexpr auto userPresetTag = "UserPreset";
constexpr auto presetIndexProperty = "factoryPresetIndex";
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

float onePoleCoeffFromCutoff(float cutoffHz, float sampleRate) noexcept
{
    const float safeSampleRate = juce::jmax(1.0f, sampleRate);
    const float clampedCutoff = juce::jlimit(20.0f, safeSampleRate * 0.45f, cutoffHz);
    return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * clampedCutoff / safeSampleRate);
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
    return std::pow(juce::jlimit(0.0f, 1.0f, value), juce::jmax(0.1f, curve));
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
    for (size_t i = 0; i < presetParameterIDs.size(); ++i)
    {
        mappedParameters[i] = tree.getParameter(presetParameterIDs[i]);
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

    for (const auto* paramID : presetParameterIDs)
        tree.addParameterListener(paramID, this);

    startTimer(33);
}

RootFlowAudioProcessor::~RootFlowAudioProcessor()
{
    stopTimer();

    for (const auto* paramID : presetParameterIDs)
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
    
    params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("oscWave", 1), "Waveform", 
                                                                  juce::StringArray { "Sine", "Saw", "Pulse" }, 0));

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
    resetMidiExpressionState();
    synth.clearVoices();
    for (int i = 0; i < 12; ++i)
    {
        auto* voice = new RootFlowVoice();
        voice->setEngine(&modulation); // Link to bio-feedback engine
        voice->setSampleRate(sr, bs);
        synth.addVoice(voice);
    }

    synth.clearSounds();
    synth.addSound(new RootFlowSound());
    synth.setNoteStealingEnabled(true);
    synth.setCurrentPlaybackSampleRate(sr);
    synth.setMinimumRenderingSubdivisionSize(4, false);
    drySafetyBuffer.setSize(juce::jmax(1, getTotalNumOutputChannels()),
                            juce::jmax(bs, 1),
                            false,
                            false,
                            true);
    drySafetyBuffer.clear();

    modulation.prepare(sr, bs);

    juce::dsp::ProcessSpec fxSpec;
    fxSpec.sampleRate = sr > 0.0 ? sr : 44100.0;
    fxSpec.maximumBlockSize = (juce::uint32) juce::jmax(bs, 1);
    fxSpec.numChannels = (juce::uint32) juce::jmax(1, getTotalNumOutputChannels());

    bloom.prepare(fxSpec);
    rain.prepare(fxSpec);
    sun.prepare(fxSpec);

    juce::dsp::ProcessSpec monoSpec { sr > 0.0 ? sr : 44100.0, (juce::uint32) juce::jmax(bs, 1), 1 };
    for (auto& filter : masterToneFilters)
    {
        filter.reset();
        filter.prepare(monoSpec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setResonance(0.52f);
        filter.setCutoffFrequency(7600.0f);
    }

    const auto safeSampleRate = sr > 0.0 ? sr : 44100.0;
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
    soilBodyCutoffSmoothed.reset(safeSampleRate, 0.14);
    soilBodyCutoffSmoothed.setCurrentAndTargetValue(360.0f);
    soilToneCutoffSmoothed.reset(safeSampleRate, 0.14);
    soilToneCutoffSmoothed.setCurrentAndTargetValue(3600.0f);
    testToneLevelSmoothed.reset(safeSampleRate, 0.04);
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
}

void RootFlowAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    buffer.clear();
    performPendingProcessingStateReset();

    const float rootDepthBase = *tree.getRawParameterValue("rootDepth");
    const float rootSoilBase = *tree.getRawParameterValue("rootSoil");
    const float rootAnchorBase = *tree.getRawParameterValue("rootAnchor");
    const float sapFlowBase = *tree.getRawParameterValue("sapFlow");
    const float sapVitalityBase = *tree.getRawParameterValue("sapVitality");
    const float sapTextureBase = *tree.getRawParameterValue("sapTexture");
    const float pulseRateBase = *tree.getRawParameterValue("pulseRate");
    const float pulseBreathBase = *tree.getRawParameterValue("pulseBreath");
    const float pulseGrowthBase = *tree.getRawParameterValue("pulseGrowth");
    const float canopyBase = *tree.getRawParameterValue("canopy");
    const float atmosphereBase = *tree.getRawParameterValue("atmosphere");
    const float instabilityBase = *tree.getRawParameterValue("instability");
    const float bloomBase = *tree.getRawParameterValue("bloom");
    const float rainBase = *tree.getRawParameterValue("rain");
    const float sunBase = *tree.getRawParameterValue("sun");
    const float seasonMacro = *tree.getRawParameterValue("ecoSystem");
    const auto seasonMorph = getSeasonMorph(seasonMacro);

    auto seasonBlend = [&](float base, float springOffset, float summerOffset, float autumnOffset, float winterOffset)
    {
        return juce::jlimit(0.0f, 1.0f,
                            base
                            + seasonMorph.spring * springOffset
                            + seasonMorph.summer * summerOffset
                            + seasonMorph.autumn * autumnOffset
                            + seasonMorph.winter * winterOffset);
    };

    const float rootDepth = emphasizeMacroResponse(seasonBlend(rootDepthBase, 0.22f, 0.12f, 0.18f, -0.32f), 0.015f);
    const float rootSoil = emphasizeMacroResponse(seasonBlend(rootSoilBase, -0.12f, -0.04f, 0.44f, 0.18f), 0.018f);
    const float rootAnchor = emphasizeMacroResponse(seasonBlend(rootAnchorBase, -0.42f, -0.15f, 0.24f, 0.48f), 0.012f);
    const float sapFlow = emphasizeMacroResponse(seasonBlend(sapFlowBase, 0.28f, 0.24f, 0.04f, -0.32f), 0.12f);
    const float sapVitality = emphasizeMacroResponse(seasonBlend(sapVitalityBase, 0.44f, 0.28f, -0.12f, -0.44f), 0.10f);
    const float sapTexture = emphasizeMacroResponse(seasonBlend(sapTextureBase, -0.05f, 0.12f, 0.48f, 0.22f), 0.08f);
    const float pulseRate = emphasizeMacroResponse(seasonBlend(pulseRateBase, 0.48f, 0.18f, -0.22f, -0.44f), 0.008f);
    const float pulseBreath = emphasizeMacroResponse(seasonBlend(pulseBreathBase, -0.24f, -0.08f, 0.36f, 0.24f), 0.012f);
    const float pulseGrowth = emphasizeMacroResponse(seasonBlend(pulseGrowthBase, 0.48f, 0.14f, -0.12f, -0.44f), 0.008f);
    const float canopy = emphasizeMacroResponse(seasonBlend(canopyBase, 0.12f, 0.22f, -0.04f, -0.32f), 0.09f);
    const float atmosphereAmount = shapeAtmosphereResponse(seasonBlend(atmosphereBase, 0.32f, 0.44f, 0.36f, 0.52f));
    const float instabilityAmount = emphasizeMacroResponse(seasonBlend(instabilityBase, 0.05f, 0.12f, 0.22f, -0.14f), 0.025f);
    const float bloomAmount = applySeasonalFxScale(bloomBase, seasonMorph, 0.22f, 0.14f, -0.10f, -0.22f, 0.03f);
    const float rainAmount = applySeasonalFxScale(rainBase, seasonMorph, 0.08f, -0.02f, 0.38f, 0.12f, 0.016f);
    const float sunAmount = applySeasonalFxScale(sunBase, seasonMorph, 0.40f, 0.16f, 0.00f, -0.16f, 0.006f);
    const float sampleRate = (float) juce::jmax(1.0, getSampleRate());

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<RootFlowVoice*>(synth.getVoice(i)))
        {
            voice->setFlow(sapFlow);
            voice->setVitality(sapVitality);
            voice->setTexture(sapTexture);
            voice->setDepth(rootDepth);
            voice->setGrowth(pulseGrowth);
            voice->setPulseRate(pulseRate);
            voice->setPulseAmount(pulseBreath);
            voice->setCanopy(canopy);
            voice->setWaveform((int)(*tree.getRawParameterValue("oscWave")));
        }
    }

    // --- UNISON LOGIC ---
    int unisonCount = 1;
    if (seasonMacro > 0.4f)  unisonCount = 2;
    if (seasonMacro > 0.82f) unisonCount = 3;

    if (unisonCount > 1)
    {
        juce::MidiBuffer unisonMidi;
        for (const auto metadata : midiMessages)
        {
            auto msg = metadata.getMessage();
            int pos = metadata.samplePosition;
            for (int u = 0; u < unisonCount; ++u)
                unisonMidi.addEvent(msg, pos);
        }
        midiMessages.swapWith(unisonMidi);
    }

    handleIncomingMidiMessages(midiMessages);
    updateSequencer(numSamples, midiMessages);
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);
    synth.renderNextBlock(buffer, midiMessages, 0, numSamples);
    if (drySafetyBuffer.getNumChannels() != numChannels || drySafetyBuffer.getNumSamples() != numSamples)
        drySafetyBuffer.setSize(numChannels, numSamples, false, false, true);
    drySafetyBuffer.makeCopyOf(buffer, true);
    float preFxPeak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto* channelData = buffer.getReadPointer(channel);
        for (int i = 0; i < numSamples; ++i)
            preFxPeak = juce::jmax(preFxPeak, std::abs(channelData[i]));
    }

    int activeVoiceCount = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<RootFlowVoice*>(synth.getVoice(i)))
            activeVoiceCount += voice->isVoiceActive() ? 1 : 0;
    }

    modulation.setParams(rootDepth,
                         rootSoil,
                         rootAnchor,
                         sapFlow,
                         sapVitality,
                         sapTexture,
                         pulseRate,
                         pulseBreath,
                         pulseGrowth);

    modulation.update(buffer);
    currentBioFeedback = modulation.getBioFeedbackSnapshot();
    const float plantEnergy = currentBioFeedback.plantEnergy;
    lastPlantEnergy.store(plantEnergy, std::memory_order_relaxed);

    const float ecoRaw = RootFlowDSP::clamp01(*tree.getRawParameterValue("ecoSystem"));
    const float ecoMaster = std::pow(ecoRaw, 0.70f); // Concave curve: FX gets through more quickly

    const float seasonalFxLift = seasonMorph.spring * 0.09f
                               + seasonMorph.summer * 0.14f
                               + seasonMorph.autumn * 0.12f
                               - seasonMorph.winter * 0.06f;
    const float fxEnergy = juce::jlimit(0.0f, 1.0f,
                                        plantEnergy * (0.46f
                                                     + seasonMorph.spring * 0.06f
                                                     + seasonMorph.summer * 0.12f
                                                     + seasonMorph.autumn * 0.16f
                                                     + seasonMorph.winter * 0.04f)
                                        + 0.01f
                                        + seasonalFxLift * 0.28f) * ecoMaster;

    const bool postFxSafetyBypassActive = postFxSafetyBypassBlocksRemaining > 0;

    bloom.setParams(bloomAmount * ecoMaster,
                    fxEnergy,
                    sapVitality,
                    pulseBreath,
                    canopy,
                    rootAnchor);
    if (! postFxSafetyBypassActive)
        bloom.process(buffer);

    rain.setParams(rainAmount * ecoMaster,
                   fxEnergy,
                   sapFlow,
                   sapTexture,
                   pulseBreath,
                   canopy);
    if (! postFxSafetyBypassActive)
        rain.process(buffer);

    sun.setParams(sunAmount * ecoMaster,
                  fxEnergy,
                  rootDepth,
                  sapVitality,
                  pulseBreath,
                  canopy);
    if (! postFxSafetyBypassActive)
        sun.process(buffer);

    // Strip slow-moving bias from the FX network before the master stage reacts to it.
    float postFxPeak = postFxSafetyBypassActive ? preFxPeak : 0.0f;
    auto applyPostFxDcBlock = [this](float sample, int channelIndex)
    {
        const float safeInput = sanitizeOutputSample(sample, 1.5f);
        const float y = safeInput
                      - postFxDcBlockPrevInput[(size_t) channelIndex]
                      + postFxDcBlockPole * postFxDcBlockPrevOutput[(size_t) channelIndex];
        postFxDcBlockPrevInput[(size_t) channelIndex] = safeInput;
        postFxDcBlockPrevOutput[(size_t) channelIndex] = sanitizeOutputSample(y, 1.5f);
        return postFxDcBlockPrevOutput[(size_t) channelIndex];
    };

    if (! postFxSafetyBypassActive)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);

            for (int i = 0; i < numSamples; ++i)
            {
                const float blockedSample = applyPostFxDcBlock(channelData[i], channel);
                channelData[i] = blockedSample;
                postFxPeak = juce::jmax(postFxPeak, std::abs(blockedSample));
            }
        }
    }

    masterDriveSmoothed.setTargetValue(0.90f
                                       + fxEnergy * 0.18f
                                       + canopy * 0.12f
                                       + pulseGrowth * 0.08f
                                       - seasonMorph.spring * 0.04f
                                       - seasonMorph.summer * 0.02f
                                       + seasonMorph.autumn * 0.12f
                                       + seasonMorph.winter * 0.04f);
    masterToneCutoffSmoothed.setTargetValue(3200.0f
                                            + canopy * 3800.0f
                                            + sapVitality * 2000.0f
                                            + fxEnergy * 1800.0f
                                            + seasonMorph.spring * 2000.0f
                                            + seasonMorph.summer * 980.0f
                                            - seasonMorph.autumn * 480.0f
                                            - seasonMorph.winter * 3000.0f);
    masterCrossfeedSmoothed.setTargetValue(0.006f
                                           + canopy * 0.014f
                                           + fxEnergy * 0.008f
                                           - seasonMorph.spring * 0.004f
                                           - seasonMorph.summer * 0.008f
                                           + seasonMorph.autumn * 0.014f
                                           + seasonMorph.winter * 0.018f);
    soilDriveSmoothed.setTargetValue(1.04f
                                     + rootSoil * 1.14f
                                     + rootDepth * 0.86f
                                     + currentBioFeedback.tension * 0.12f
                                     + fxEnergy * 0.06f
                                     - seasonMorph.spring * 0.03f
                                     + seasonMorph.summer * 0.01f
                                     + seasonMorph.autumn * 0.18f
                                     + seasonMorph.winter * 0.06f);
    soilMixSmoothed.setTargetValue(0.05f
                                   + rootSoil * 0.40f
                                   + rootDepth * 0.26f
                                   + currentBioFeedback.plantEnergy * 0.04f
                                   - seasonMorph.spring * 0.02f
                                   - seasonMorph.summer * 0.01f
                                   + seasonMorph.autumn * 0.10f
                                   + seasonMorph.winter * 0.03f);
    soilBodyCutoffSmoothed.setTargetValue(180.0f
                                          + rootSoil * 300.0f
                                          + rootDepth * 90.0f
                                          + currentBioFeedback.growthCycle * 110.0f
                                          - seasonMorph.spring * 65.0f
                                          + seasonMorph.autumn * 160.0f
                                          + seasonMorph.winter * 55.0f);
    soilToneCutoffSmoothed.setTargetValue(2100.0f
                                          + canopy * 2300.0f
                                          + (1.0f - rootSoil) * 700.0f
                                          + sapVitality * 600.0f
                                          + seasonMorph.spring * 760.0f
                                          + seasonMorph.summer * 220.0f
                                          - seasonMorph.autumn * 260.0f
                                          - seasonMorph.winter * 1300.0f);

    auto* leftData = buffer.getWritePointer(0);
    auto* rightData = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;
    const float testTonePhaseDelta = juce::MathConstants<double>::twoPi * 440.0 / (double) sampleRate;
    const float springAirMix = seasonMorph.spring * 0.22f;
    const float summerAirMix = seasonMorph.summer * 0.10f;
    const float autumnBodyMix = seasonMorph.autumn * 0.18f;
    const float winterStill = seasonMorph.winter * 0.16f;
    const float seasonOutputTrim = 1.08f
                                 + seasonMorph.spring * 0.05f
                                 + seasonMorph.summer * 0.04f
                                 - seasonMorph.autumn * 0.01f
                                 - seasonMorph.winter * 0.06f;
    const float canopyOutputTrim = 1.04f - canopy * 0.02f;
    const float frostMix = seasonMorph.winter * 0.12f;
    const float frostEdge = 1.0f + seasonMorph.winter * 0.30f;
    const auto applyFrost = [&](float sample)
    {
        const float frozen = std::tanh(sample * frostEdge);
        const float crystalline = juce::jlimit(-1.0f, 1.0f,
                                               frozen * (0.96f - seasonMorph.winter * 0.06f)
                                               + sample * (0.04f + seasonMorph.winter * 0.03f));
        return juce::jmap(frostMix, sample, crystalline);
    };
    testToneLevelSmoothed.setTargetValue(isTestToneEnabled() ? 1.0f : 0.0f);
    bool invalidAudioDetected = false;
    float rawOutputPeak = 0.0f;
    float finalOutputPeak = 0.0f;

    auto applyOutputDcBlock = [this](float sample, int channelIndex)
    {
        const float safeInput = sanitizeOutputSample(sample, 1.5f);
        const float y = safeInput
                      - outputDcBlockPrevInput[(size_t) channelIndex]
                      + outputDcBlockPole * outputDcBlockPrevOutput[(size_t) channelIndex];
        outputDcBlockPrevInput[(size_t) channelIndex] = safeInput;
        outputDcBlockPrevOutput[(size_t) channelIndex] = sanitizeOutputSample(y, 1.5f);
        return outputDcBlockPrevOutput[(size_t) channelIndex];
    };
    // Recovery paths reset the entire post-FX/master chain together so a bad block
    // does not keep re-entering the same muted or runaway state.
    auto resetPostFxState = [&, this]()
    {
        bloom.reset();
        rain.reset();
        sun.reset();
        for (auto& filter : masterToneFilters)
        {
            filter.reset();
            filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            filter.setResonance(0.52f);
            filter.setCutoffFrequency(masterToneCutoffSmoothed.getCurrentValue());
        }

        soilBodyState.fill(0.0f);
        soilToneState.fill(0.0f);
        postFxDcBlockPrevInput.fill(0.0f);
        postFxDcBlockPrevOutput.fill(0.0f);
        outputDcBlockPrevInput.fill(0.0f);
        outputDcBlockPrevOutput.fill(0.0f);
    };
    auto restoreDrySafetyBuffer = [&, this]()
    {
        postFxDcBlockPrevInput.fill(0.0f);
        postFxDcBlockPrevOutput.fill(0.0f);
        outputDcBlockPrevInput.fill(0.0f);
        outputDcBlockPrevOutput.fill(0.0f);
        finalOutputPeak = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto* dryData = drySafetyBuffer.getReadPointer(channel);
            auto* outData = buffer.getWritePointer(channel);

            for (int i = 0; i < numSamples; ++i)
            {
                const float safeDry = sanitizeOutputSample(dryData[i], outputSafetyLimit);
                outData[i] = safeDry;
                finalOutputPeak = juce::jmax(finalOutputPeak, std::abs(safeDry));
            }
        }

        currentOutputPeak.store(finalOutputPeak, std::memory_order_relaxed);
    };

    if (postFxSafetyBypassActive)
    {
        restoreDrySafetyBuffer();
        postFxSafetyBypassBlocksRemaining = juce::jmax(0, postFxSafetyBypassBlocksRemaining - 1);
    }
    else for (int i = 0; i < numSamples; ++i)
    {
        const float drive = masterDriveSmoothed.getNextValue();
        const float toneCutoff = masterToneCutoffSmoothed.getNextValue();
        const float crossfeed = masterCrossfeedSmoothed.getNextValue();
        const float soilDrive = soilDriveSmoothed.getNextValue();
        const float soilMix = soilMixSmoothed.getNextValue();
        const float soilBodyCoeff = onePoleCoeffFromCutoff(soilBodyCutoffSmoothed.getNextValue(), sampleRate);
        const float soilToneCoeff = onePoleCoeffFromCutoff(soilToneCutoffSmoothed.getNextValue(), sampleRate);
        const float driveComp = 1.0f / (1.0f + juce::jmax(0.0f, drive - 1.0f) * 0.85f);

        masterToneFilters[0].setCutoffFrequency(toneCutoff * 0.985f);
        if (rightData != nullptr)
            masterToneFilters[1].setCutoffFrequency(toneCutoff * 1.015f);

        const float rawDryL = leftData[i];
        const float rawDryR = rightData != nullptr ? rightData[i] : rawDryL;
        invalidAudioDetected = invalidAudioDetected
                            || (! std::isfinite(rawDryL))
                            || (rightData != nullptr && ! std::isfinite(rawDryR));

        const float dryL = sanitizeOutputSample(rawDryL, 1.5f);
        const float dryR = sanitizeOutputSample(rawDryR, 1.5f);
        const float glueL = dryL + dryR * crossfeed * 0.5f;
        const float glueR = dryR + dryL * crossfeed * 0.5f;

        soilBodyState[0] += soilBodyCoeff * (glueL - soilBodyState[0]);
        const float soilBias = 0.02f + soilMix * 0.08f;
        const float earthyL = glueL + soilBodyState[0] * (0.08f + soilMix * 0.24f);
        float soilL = std::tanh(earthyL * soilDrive + soilBias) - std::tanh(soilBias);
        soilL += 0.10f * std::tanh((earthyL - soilBodyState[0] * 0.18f) * (0.68f + soilDrive * 0.08f));
        soilToneState[0] += soilToneCoeff * (soilL - soilToneState[0]);
        const float soilShapedL = soilL * (0.64f + soilMix * 0.04f) + soilToneState[0] * (0.18f - soilMix * 0.02f);
        const float soilMixedL = glueL * (1.0f - soilMix) + soilShapedL * soilMix;

        float soilMixedR = soilMixedL;
        if (rightData != nullptr)
        {
            soilBodyState[1] += soilBodyCoeff * (glueR - soilBodyState[1]);
            const float earthyR = glueR + soilBodyState[1] * (0.08f + soilMix * 0.24f);
            float soilR = std::tanh(earthyR * soilDrive + soilBias) - std::tanh(soilBias);
            soilR += 0.10f * std::tanh((earthyR - soilBodyState[1] * 0.18f) * (0.68f + soilDrive * 0.08f));
            soilToneState[1] += soilToneCoeff * (soilR - soilToneState[1]);
            const float soilShapedR = soilR * (0.64f + soilMix * 0.04f) + soilToneState[1] * (0.18f - soilMix * 0.02f);
            soilMixedR = glueR * (1.0f - soilMix) + soilShapedR * soilMix;
        }

        float tonedL = masterToneFilters[0].processSample(0, soilMixedL);
        float tonedR = rightData != nullptr ? masterToneFilters[1].processSample(0, soilMixedR) : tonedL;

        const float airyL = soilMixedL - tonedL;
        tonedL += airyL * (springAirMix + summerAirMix);
        tonedL = juce::jmap(autumnBodyMix, tonedL, tonedL + soilBodyState[0] * 0.18f);
        tonedL *= 1.0f - winterStill * 0.12f;

        if (rightData != nullptr)
        {
            const float airyR = soilMixedR - tonedR;
            tonedR += airyR * (springAirMix + summerAirMix);
            tonedR = juce::jmap(autumnBodyMix, tonedR, tonedR + soilBodyState[1] * 0.18f);
            tonedR *= 1.0f - winterStill * 0.12f;
        }

        if (frostMix > 0.001f)
        {
            tonedL = applyFrost(tonedL);
            if (rightData != nullptr)
                tonedR = applyFrost(tonedR);
        }

        const float drivenL = std::tanh(tonedL * drive) * driveComp * 0.98f;
        float finalL = juce::jmap(juce::jlimit(0.0f, 0.24f, juce::jmax(0.0f, drive - 1.0f) * 0.40f),
                                  tonedL,
                                  drivenL) * seasonOutputTrim * canopyOutputTrim;
        if (rightData != nullptr)
        {
            const float drivenR = std::tanh(tonedR * drive) * driveComp * 0.98f;
            float finalR = juce::jmap(juce::jlimit(0.0f, 0.24f, juce::jmax(0.0f, drive - 1.0f) * 0.40f),
                                      tonedR,
                                      drivenR) * seasonOutputTrim * canopyOutputTrim;
            const float testTone = std::sin(testTonePhase) * 0.06f * testToneLevelSmoothed.getNextValue();
            
            // Apply Master Boost (approx +9dB) with soft-rounding to prevent 'scratching'
            float boostedL = (finalL + testTone) * 2.85f;
            float boostedR = (finalR + testTone) * 2.85f;
            
            // Deep-Sine Friendly Soft Limiter
            finalL = boostedL / (1.15f + std::abs(boostedL * 0.18f));
            finalR = boostedR / (1.15f + std::abs(boostedR * 0.18f));

            invalidAudioDetected = invalidAudioDetected || (! std::isfinite(finalL)) || (! std::isfinite(finalR));
            rawOutputPeak = juce::jmax(rawOutputPeak, std::abs(finalL));
            rawOutputPeak = juce::jmax(rawOutputPeak, std::abs(finalR));
            finalL = applyOutputDcBlock(finalL, 0);
            finalR = applyOutputDcBlock(finalR, 1);
            
            leftData[i] = sanitizeOutputSample(finalL, outputSafetyLimit);
            rightData[i] = sanitizeOutputSample(finalR, outputSafetyLimit);
            finalOutputPeak = juce::jmax(finalOutputPeak, std::abs(leftData[i]));
            finalOutputPeak = juce::jmax(finalOutputPeak, std::abs(rightData[i]));
        }
        else
        {
            const float testTone = std::sin(testTonePhase) * 0.06f * testToneLevelSmoothed.getNextValue();
            finalL += testTone;
            invalidAudioDetected = invalidAudioDetected || (! std::isfinite(finalL));
            rawOutputPeak = juce::jmax(rawOutputPeak, std::abs(finalL));
            finalL = applyOutputDcBlock(finalL, 0);
            leftData[i] = sanitizeOutputSample(finalL, outputSafetyLimit);
            finalOutputPeak = juce::jmax(finalOutputPeak, std::abs(leftData[i]));
        }

        testTonePhase += testTonePhaseDelta;
        if (testTonePhase >= juce::MathConstants<double>::twoPi)
            testTonePhase -= juce::MathConstants<double>::twoPi;
    }

    if (invalidAudioDetected)
    {
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<RootFlowVoice*>(synth.getVoice(i)))
                voice->reset();
        }

        resetPostFxState();
        outputSilenceWatchdogBlocks = 0;
        outputRunawayWatchdogBlocks = 0;
        postFxSafetyBypassBlocksRemaining = 0;
    }
    else
    {
        const bool fxStageMutedUnexpectedly = preFxPeak > silenceWatchdogPreFxThreshold
                                           && postFxPeak < silenceWatchdogPostFxThreshold
                                           && postFxPeak < preFxPeak * 0.04f;
        const bool masterStageMutedUnexpectedly = postFxPeak > silenceWatchdogPreFxThreshold
                                               && finalOutputPeak < silenceWatchdogPostFxThreshold
                                               && finalOutputPeak < postFxPeak * 0.04f;
        const bool fxChainMutedUnexpectedly = fxStageMutedUnexpectedly || masterStageMutedUnexpectedly;
        const bool activeVoicesWentSilent = activeVoiceCount > 0
                                         && preFxPeak < silenceWatchdogPostFxThreshold
                                         && finalOutputPeak < silenceWatchdogPostFxThreshold;

        if (fxChainMutedUnexpectedly || activeVoicesWentSilent)
        {
            if (fxChainMutedUnexpectedly && activeVoiceCount > 0)
            {
                restoreDrySafetyBuffer();
                resetPostFxState();
                postFxSafetyBypassBlocksRemaining = postFxSafetyBypassDurationBlocks;
            }

            ++outputSilenceWatchdogBlocks;

            if (outputSilenceWatchdogBlocks >= silenceWatchdogTriggerBlocks)
            {
                restoreDrySafetyBuffer();
                for (int i = 0; i < synth.getNumVoices(); ++i)
                {
                    if (auto* voice = dynamic_cast<RootFlowVoice*>(synth.getVoice(i)))
                        voice->recoverFromSilentState();
                }

                modulation.prepare(sampleRate, numSamples);
                currentBioFeedback = {};
                lastPlantEnergy.store(0.0f, std::memory_order_relaxed);
                resetPostFxState();
                testTonePhase = 0.0;
                outputSilenceWatchdogBlocks = 0;
                outputRunawayWatchdogBlocks = 0;
                postFxSafetyBypassBlocksRemaining = postFxSafetyBypassDurationBlocks;
            }
        }
        else
        {
            outputSilenceWatchdogBlocks = 0;
        }

        const bool runawayOutputDetected = activeVoiceCount == 0
                                        && ! isTestToneEnabled()
                                        && preFxPeak < runawayWatchdogPreFxThreshold
                                        && rawOutputPeak > runawayWatchdogOutputThreshold;

        if (runawayOutputDetected)
        {
            ++outputRunawayWatchdogBlocks;

            if (outputRunawayWatchdogBlocks >= runawayWatchdogTriggerBlocks)
            {
                buffer.clear();
                resetPostFxState();
                finalOutputPeak = 0.0f;
                outputRunawayWatchdogBlocks = 0;
                postFxSafetyBypassBlocksRemaining = 0;
            }
        }
        else
        {
            outputRunawayWatchdogBlocks = 0;
        }
    }


    // Feed FFT and track peak
    float maxPeak = 0.0f;
    const int numChansUsed = juce::jmin(2, buffer.getNumChannels());
    
    for (int i = 0; i < numSamples; ++i)
    {
        float sampleMix = 0.0f;
        for (int ch = 0; ch < numChansUsed; ++ch)
        {
            const float s = buffer.getSample(ch, i);
            maxPeak = juce::jmax(maxPeak, std::abs(s));
            sampleMix += s;
        }
        pushNextSampleIntoFifo(sampleMix * (numChansUsed > 1 ? 0.707f : 1.0f));
    }
    
    currentOutputPeak.store(maxPeak, std::memory_order_relaxed);
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
    presetLoadInProgress.fetch_add(1, std::memory_order_acq_rel);
    for (size_t i = 0; i < presetParameterIDs.size(); ++i)
    {
        if (auto* parameter = tree.getParameter(presetParameterIDs[i]))
            parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, preset.values[i]));
    }
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
}

juce::String RootFlowAudioProcessor::getMappedParameterIDForSlot(int parameterSlot) const
{
    if (! juce::isPositiveAndBelow(parameterSlot, (int) presetParameterIDs.size()))
        return {};

    return presetParameterIDs[(size_t) parameterSlot];
}

int RootFlowAudioProcessor::getMappedParameterSlotForID(const juce::String& paramID) const noexcept
{
    for (size_t i = 0; i < presetParameterIDs.size(); ++i)
    {
        if (paramID == presetParameterIDs[i])
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
            const juce::ScopedLock sl (noteLock);
            int note = message.getNoteNumber();
            if (std::find(heldMidiNotes.begin(), heldMidiNotes.end(), note) == heldMidiNotes.end())
                heldMidiNotes.push_back(note);
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
}

void RootFlowAudioProcessor::restoreCustomState(const juce::ValueTree& state)
{
    currentFactoryPresetIndex.store((int) state.getProperty(presetIndexProperty, 0), std::memory_order_relaxed);
    currentUserPresetIndex.store(-1, std::memory_order_relaxed);

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
        filter.setCutoffFrequency(7600.0f);
    }

    masterDriveSmoothed.setCurrentAndTargetValue(1.0f);
    masterToneCutoffSmoothed.setCurrentAndTargetValue(7600.0f);
    masterCrossfeedSmoothed.setCurrentAndTargetValue(0.0f);
    soilDriveSmoothed.setCurrentAndTargetValue(1.0f);
    soilMixSmoothed.setCurrentAndTargetValue(0.0f);
    soilBodyCutoffSmoothed.setCurrentAndTargetValue(360.0f);
    soilToneCutoffSmoothed.setCurrentAndTargetValue(3600.0f);
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
}

juce::AudioProcessorEditor* RootFlowAudioProcessor::createEditor()
{
    return new RootFlowAudioProcessorEditor(*this);
}

void RootFlowAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = tree.copyState();
    appendCustomState(state);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RootFlowAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (tree.state.getType()))
        {
            auto restoredState = juce::ValueTree::fromXml(*xmlState);
            restoreCustomState(restoredState);
            presetLoadInProgress.fetch_add(1, std::memory_order_acq_rel);
            tree.replaceState (restoredState);
            presetLoadInProgress.fetch_sub(1, std::memory_order_acq_rel);
            currentPresetDirty.store(0, std::memory_order_relaxed);
            requestProcessingStateReset();
        }
}

void RootFlowAudioProcessor::resetSequencer()
{
    currentSequencerStep = 0;
    sampleCounter = 0;
    lastSequencerNote = -1;
    sequencerNoteActive = false;
    samplesPerStep = 44100.0 * 0.125;
}

void RootFlowAudioProcessor::updateSequencer(int numSamples, juce::MidiBuffer& midiMessages)
{
    const bool isEnabled = *tree.getRawParameterValue("sequencerOn") > 0.5f;
    if (!isEnabled)
    {
        if (sequencerNoteActive)
        {
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, lastSequencerNote), 0);
            sequencerNoteActive = false;
        }
        return;
    }

    const int rateIndex = (int)*tree.getRawParameterValue("sequencerRate");
    const int numSteps = (int)*tree.getRawParameterValue("sequencerSteps");
    const float gateParam = *tree.getRawParameterValue("sequencerGate");

    // --- BIO-SENSITIVITY ---
    // Wir nutzen die aktuelle Energie, um das "Verhalten" zu beeinflussen
    const float energy = currentBioFeedback.plantEnergy;
    
    // Zeitliche Drift: Bei hoher Energie wird der Sequencer minimal schneller/nervöser
    const double speedMod = 0.95 + (energy * 0.1); 
    const float modulatedGate = juce::jlimit(0.1f, 0.95f, gateParam * (0.8f + energy * 0.4f));

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
    
    double division = 0.25; // 1/16th Default
    if (rateIndex == 0)      division = 1.0;   // 1/4
    else if (rateIndex == 1)  division = 0.5;   // 1/8
    else if (rateIndex == 3) division = 0.125; // 1/32

    samplesPerStep = (samplesPerBeat * division) / speedMod;
    sequencerGateSamples = samplesPerStep * modulatedGate;

    for (int i = 0; i < numSamples; ++i)
    {
        sampleCounter++;

        if (sampleCounter >= samplesPerStep)
        {
            sampleCounter -= samplesPerStep;

            // Note Off für den alten Step
            if (sequencerNoteActive)
            {
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, lastSequencerNote), i);
                sequencerNoteActive = false;
            }

            currentSequencerStep = (currentSequencerStep + 1) % juce::jmax(1, numSteps);
            auto& step = sequencerSteps[(size_t)currentSequencerStep];

            // --- BIO-LOGIK: Wahrscheinlichkeit ---
            // Wenn die Energie niedrig ist, "vergisst" die Pflanze manchmal einen Trigger
            bool energyTrigger = juce::Random::getSystemRandom().nextFloat() < (0.7f + energy * 0.3f);

            if (step.active && energyTrigger)
            {
                const juce::ScopedLock sl (noteLock);
                
                // Velocity wird durch die Energie beeinflusst (mehr Energie = akzentuierter)
                float dynamicVelocity = juce::jlimit(0.05f, 1.0f, step.velocity * (0.6f + energy * 0.4f));
                
                if (!heldMidiNotes.empty())
                {
                    currentArpIndex = (currentArpIndex + 1) % (int)heldMidiNotes.size();
                    lastSequencerNote = heldMidiNotes[(size_t)currentArpIndex];
                }
                else
                {
                    // Ambient Mode: Wenn keine Tasten gedrückt sind, generiert die Energie "Wurzelnoten" (Pentatonisch)
                    const int pentatonicScale[] = { 0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24 };
                    const int scaleSize = 11;
                    int scaleIndex = (int)(energy * (scaleSize - 1.0f)); 
                    scaleIndex = juce::jlimit(0, scaleSize - 1, scaleIndex);
                    
                    // Grundton 36 (C2) + generativer Offset aus Pentatonik
                    lastSequencerNote = 36 + pentatonicScale[scaleIndex]; 
                }
                
                midiMessages.addEvent(juce::MidiMessage::noteOn(1, lastSequencerNote, dynamicVelocity), i);
                sequencerNoteActive = true;
                sequencerTriggered.store(true);
            }
        }

        // Gate-Check für Note Off innerhalb des Samples-Blocks
        if (sequencerNoteActive && sampleCounter >= sequencerGateSamples)
        {
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, lastSequencerNote), i);
            sequencerNoteActive = false;
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RootFlowAudioProcessor();
}

