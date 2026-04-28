
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/Utils/DesignTokens.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace
{
    using MutationMode = RootFlow::MutationMode;
    using GrowLockGroup = RootFlow::GrowLockGroup;
    using SequencerStep = RootFlow::SequencerStep;

    constexpr std::array<const char*, 30> managedParameterIDs {
        "sourceDepth", "sourceCore", "sourceAnchor",
        "flowRate", "flowEnergy", "flowTexture",
        "pulseFrequency", "pulseWidth", "pulseEnergy",
        "fieldComplexity", "fieldDepth",
        "instability",
        "radiance", "charge", "discharge",
        "systemMatrix",
        "sequencerOn", "sequencerRate", "sequencerSteps", "sequencerGate",
        "sequencerProbOn", "sequencerJitterOn",
        "oscWave",
        "masterVolume", "masterCompressor", "masterMix", "monoMakerToggle", "monoMakerFreq",
        "evolution", "oversampling"
    };

    struct LegacyParameterMapping
    {
        const char* oldID;
        const char* newID;
    };

    constexpr std::array<LegacyParameterMapping, 15> legacyParameterMappings {{
        { "rootDepth", "sourceDepth" },
        { "rootSoil", "sourceCore" },
        { "rootAnchor", "sourceAnchor" },
        { "sapFlow", "flowRate" },
        { "sapVitality", "flowEnergy" },
        { "sapTexture", "flowTexture" },
        { "pulseRate", "pulseFrequency" },
        { "pulseBreath", "pulseWidth" },
        { "pulseGrowth", "pulseEnergy" },
        { "canopy", "fieldComplexity" },
        { "atmosphere", "fieldDepth" },
        { "ecoSystem", "systemMatrix" },
        { "bloom", "radiance" },
        { "rain", "charge" },
        { "sun", "discharge" }
    }};

    juce::String mapLegacyParameterID(const juce::String& paramID)
    {
        for (const auto& mapping : legacyParameterMappings)
            if (paramID == mapping.oldID)
                return mapping.newID;

        return paramID;
    }

    juce::ValueTree findParameterStateChild(const juce::ValueTree& state, const juce::String& paramID)
    {
        for (const auto& child : state)
            if (child.hasType("PARAM") && child.getProperty("id").toString() == paramID)
                return child;

        return {};
    }

    inline float getParameterValue(const juce::AudioProcessorValueTreeState& vts, const juce::String& paramID, float defaultVal = 0.5f)
    {
        if (auto* p = vts.getRawParameterValue(paramID))
            return p->load();
        return defaultVal;
    }

    inline float getStateParameterValue(const juce::ValueTree& state, const juce::String& paramID, float defaultVal)
    {
        if (auto child = findParameterStateChild(state, paramID); child.isValid())
            return (float) child.getProperty("value", defaultVal);

        return defaultVal;
    }

    struct FactoryPresetMasterSettings
    {
        float compressor = 0.2f;
        float mix = 1.0f;
        float monoFreqHz = 80.0f;
        bool mono = false;
    };

    struct MutationProfile
    {
        float wildness = 0.5f;
        float timingTightness = 0.5f;
    };

    inline MutationProfile getMutationProfile(RootFlow::MutationMode mode, const RootFlow::MutationContext& context)
    {
        MutationProfile p;
        if (mode == RootFlow::MutationMode::gentle) { p.wildness = 0.12f; p.timingTightness = 0.88f; }
        else if (mode == RootFlow::MutationMode::wild) { p.wildness = 0.92f; p.timingTightness = 0.15f * (1.0f - context.instability); }
        else { p.wildness = 0.44f; p.timingTightness = 0.62f; }
        return p;
    }

    inline RootFlowDSP::SeasonMorph getSeasonMorph(float seasonMacro)
    {
        RootFlowDSP::SeasonMorph m;
        const float s = seasonMacro;
        
        // Spring: Peaks at 0.25 (range 0.0 to 0.5)
        m.spring = std::max(0.0f, 1.0f - std::abs(s - 0.25f) * 4.0f);
        // Summer: Peaks at 0.5 (range 0.25 to 0.75)
        m.summer = std::max(0.0f, 1.0f - std::abs(s - 0.5f) * 4.0f);
        // Autumn: Peaks at 0.75 (range 0.5 to 1.0)
        m.autumn = std::max(0.0f, 1.0f - std::abs(s - 0.75f) * 4.0f);
        // Winter: Peaks at 0.0 and 1.0 (Cyclic)
        m.winter = std::max(0.0f, 1.0f - std::abs(s - (s > 0.5f ? 1.0f : 0.0f)) * 4.0f);
        
        return m;
    }

    // Factory presets and sections are now accessed via RootFlow::factoryPresets
    // and RootFlow::factoryPresetSections defined in RootFlowPresets.h


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

    // --- Internal Mutation Logic moved to RootFlowMutation.h ---

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

inline juce::ValueTree makePromptMemoryEntry(const juce::String& prompt,
                                             const juce::String& summary,
                                             const RootFlow::PromptPatchTarget& target,
                                             float reinforcement)
{
    const auto normalizedPrompt = RootFlow::normalizePromptText(prompt).trim();
    auto tokens = RootFlow::tokenizePrompt(prompt);

    juce::ValueTree vt(promptMemoryEntryTag);
    vt.setProperty("prompt", prompt, nullptr);
    vt.setProperty("summary", summary, nullptr);
    vt.setProperty("normalized", normalizedPrompt, nullptr);
    vt.setProperty("tokens", tokens.joinIntoString("|"), nullptr);
    vt.setProperty("strength", juce::jlimit(0.2f, 6.0f, reinforcement), nullptr);
    vt.setProperty("uses", 1, nullptr);
    vt.setProperty("reinforcement", reinforcement, nullptr);
    vt.setProperty("timestamp", juce::Time::getCurrentTime().toMilliseconds(), nullptr);
    RootFlow::writePromptTargetToMemoryEntry(vt, target);
    return vt;
}



inline float scorePromptMemoryEntryMatch(const juce::String& normalizedPrompt,
                                         const juce::StringArray& tokens,
                                         const juce::ValueTree& entry)
{
    const auto existingPrompt = entry.getProperty("prompt", {}).toString();
    const auto existingNormalized = entry.getProperty("normalized",
                                                      RootFlow::normalizePromptText(existingPrompt)).toString().trim();

    if (existingNormalized.isEmpty())
        return 0.0f;

    if (existingNormalized == normalizedPrompt)
        return 1.15f;

    juce::StringArray existingTokens;
    existingTokens.addTokens(entry.getProperty("tokens", {}).toString(), "|", "");
    existingTokens.removeEmptyStrings();

    if (existingTokens.isEmpty())
        existingTokens = RootFlow::tokenizePrompt(existingPrompt);

    int overlapCount = 0;
    for (const auto& token : tokens)
        if (token.isNotEmpty() && existingTokens.contains(token))
            ++overlapCount;

    const float tokenScore = (tokens.isEmpty() || existingTokens.isEmpty())
        ? 0.0f
        : (2.0f * (float) overlapCount) / (float) (tokens.size() + existingTokens.size());
    const float phraseScore = (existingNormalized.contains(normalizedPrompt) || normalizedPrompt.contains(existingNormalized))
        ? 0.20f
        : 0.0f;
    const float strengthBonus = juce::jlimit(0.0f, 0.12f, (float) entry.getProperty("strength", 1.0f) * 0.02f);

    return juce::jlimit(0.0f, 1.0f, tokenScore + phraseScore + strengthBonus);
}

inline RootFlow::PromptPatchTarget capturePromptPatchTargetFromState(const juce::ValueTree& state)
{
    RootFlow::PromptPatchTarget target;

    for (size_t i = 0; i < RootFlow::factoryPresetValueParameterIDs.size(); ++i)
        target.values[i] = getStateParameterValue(state,
                                                  RootFlow::factoryPresetValueParameterIDs[i],
                                                  RootFlow::factoryPresetDefaultValues[i]);

    target.sequencerOn = getStateParameterValue(state, "sequencerOn", 0.0f) > 0.5f;
    target.sequencerRateIndex = juce::roundToInt(getStateParameterValue(state, "sequencerRate", 2.0f));
    target.sequencerStepCount = juce::roundToInt(getStateParameterValue(state, "sequencerSteps", 8.0f));
    target.sequencerGate = getStateParameterValue(state, "sequencerGate", target.sequencerGate);
    target.sequencerProbOn = getStateParameterValue(state, "sequencerProbOn", target.sequencerProbOn ? 1.0f : 0.0f) > 0.5f;
    target.sequencerJitterOn = getStateParameterValue(state, "sequencerJitterOn", target.sequencerJitterOn ? 1.0f : 0.0f) > 0.5f;
    target.oscWaveIndex = juce::roundToInt(getStateParameterValue(state, "oscWave", 0.0f));
    target.masterCompressor = getStateParameterValue(state, "masterCompressor", target.masterCompressor);
    target.masterMix = getStateParameterValue(state, "masterMix", target.masterMix);
    target.monoMaker = getStateParameterValue(state, "monoMakerToggle", target.monoMaker ? 1.0f : 0.0f) > 0.5f;
    target.monoMakerFreqHz = getStateParameterValue(state, "monoMakerFreq", target.monoMakerFreqHz);
    target.evolution = getStateParameterValue(state, "evolution", target.evolution);

    return target;
}

    void migrateLegacyParameterState(juce::ValueTree& state)
    {
        for (int i = state.getNumChildren(); --i >= 0;)
        {
            auto child = state.getChild(i);
            if (! child.hasType("PARAM"))
                continue;

            const auto currentID = child.getProperty("id", {}).toString();
            const auto migratedID = mapLegacyParameterID(currentID);
            if (migratedID == currentID)
                continue;

            if (auto replacement = findParameterStateChild(state, migratedID); replacement.isValid())
            {
                if (child.hasProperty("value"))
                    replacement.setProperty("value", child.getProperty("value"), nullptr);

                state.removeChild(child, nullptr);
            }
            else
            {
                child.setProperty("id", migratedID, nullptr);
            }
        }
    }

    void migrateLegacyPromptMemoryEntry(juce::ValueTree& entry)
    {
        const auto prompt = entry.getProperty("prompt", {}).toString();
        if (! entry.hasProperty("normalized"))
            entry.setProperty("normalized", RootFlow::normalizePromptText(prompt).trim(), nullptr);

        if (! entry.hasProperty("tokens"))
        {
            auto tokens = RootFlow::tokenizePrompt(prompt);
            entry.setProperty("tokens", tokens.joinIntoString("|"), nullptr);
        }

        if (! entry.hasProperty("strength"))
            entry.setProperty("strength",
                              juce::jlimit(0.2f, 6.0f, (float) entry.getProperty("reinforcement", 1.0f)),
                              nullptr);

        if (! entry.hasProperty("uses"))
            entry.setProperty("uses", 1, nullptr);
    }

    void migrateLegacyState(juce::ValueTree& state)
    {
        migrateLegacyParameterState(state);

        if (auto promptMemoryState = state.getChildWithName(promptMemoryStateTag); promptMemoryState.isValid())
        {
            for (int i = 0; i < promptMemoryState.getNumChildren(); ++i)
            {
                auto entry = promptMemoryState.getChild(i);
                if (entry.hasType(promptMemoryEntryTag))
                    migrateLegacyPromptMemoryEntry(entry);
            }
        }

        if (auto userPresetsState = state.getChildWithName(userPresetsTag); userPresetsState.isValid())
        {
            for (int i = 0; i < userPresetsState.getNumChildren(); ++i)
            {
                auto presetState = userPresetsState.getChild(i);
                if (presetState.hasType(userPresetTag) && presetState.getNumChildren() > 0)
                {
                    auto presetRoot = presetState.getChild(0);
                    migrateLegacyState(presetRoot);
                }
            }
        }
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

    using PromptPatternArchetype = RootFlow::PromptPatternArchetype;
    using PromptRhythmProfile = RootFlow::PromptRhythmProfile;

    constexpr auto presetGlobalOutputParameterID = "masterVolume";

    constexpr std::array<const char*, 4> factoryPresetUtilityParameterIDs {
        "masterCompressor", "masterMix", "monoMakerToggle", "monoMakerFreq"
    };

    inline PromptPatternArchetype promptPatternArchetypeFromValue(int value) noexcept
    {
        if (value >= 0 && value <= (int) PromptPatternArchetype::cinematic)
            return (PromptPatternArchetype) value;
        return PromptPatternArchetype::matrix;
    }

    constexpr std::array<const char*, 16> mutationParameterIDs {
        "sourceDepth", "sourceCore", "sourceAnchor",
        "flowRate", "flowEnergy", "flowTexture",
        "pulseFrequency", "pulseWidth", "pulseEnergy",
        "fieldComplexity", "fieldDepth",
        "instability",
        "radiance", "charge", "discharge",
        "systemMatrix"
    };

    inline RootFlow::MutationContext makeMutationContext(const juce::AudioProcessorValueTreeState& vts)
    {
        RootFlow::MutationContext c;
        c.instability = getParameterValue(vts, "instability");
        c.evolution = getParameterValue(vts, "evolution");
        return c;
    }

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

        case PromptPatternArchetype::matrix:
        default:
            break;
    }

    const float timingScale = juce::jlimit(0.34f, 1.0f, 1.04f - promptRhythmProfile.tightness * 0.54f);
    for (int i = 0; i < limitedStepCount; ++i)
        steps[(size_t) i].timingOffset = clampSequencerTimingOffset(steps[(size_t) i].timingOffset * timingScale);
}


    // --- Modularized Prompt & Mutation Logic ---
    // The previous monolithic block of prompt analysis, generation, and 
    // mutation logic has been migrated to RootFlowMutation.h/cpp.

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

FactoryPresetMasterSettings getFactoryPresetMasterSettings(int index, const RootFlow::FactoryPreset& preset) noexcept
{
    const auto name = juce::String(preset.name).trim();
    const auto& values = preset.values;

    const float coreWeight = (values[0] + values[1] + values[2]) / 3.0f;
    const float motionWeight = (values[6] + values[7] + values[8]) / 3.0f;
    const float spectralWeight = (values[9] + values[10] + values[15]) / 3.0f;
    const float fxWeight = (values[12] + values[13] + values[14]) / 3.0f;
    const float instability = values[11];

    FactoryPresetMasterSettings settings;
    settings.compressor = juce::jlimit(0.05f, 0.58f,
                                       0.06f
                                       + coreWeight * 0.14f
                                       + motionWeight * 0.10f
                                       + instability * 0.20f
                                       + fxWeight * 0.05f);

    settings.mix = juce::jlimit(0.76f, 1.0f,
                                0.90f
                                + spectralWeight * 0.07f
                                + fxWeight * 0.06f
                                - coreWeight * 0.08f
                                - instability * 0.05f);

    settings.monoFreqHz = juce::jlimit(42.0f, 220.0f,
                                       52.0f
                                       + coreWeight * 90.0f
                                       + motionWeight * 28.0f
                                       + instability * 18.0f);

    settings.mono = coreWeight > 0.76f
                 || ((index >= 17 && index <= 22) && coreWeight > 0.56f)
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
                           const RootFlowDSP::SeasonMorph& seasonMorph,
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

    // Default Matrix-Sequencer Pattern
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

    addParam("sourceDepth", "Source Depth", 0.5f);
    addParam("sourceCore", "Source Core", 0.5f);
    addParam("sourceAnchor", "Source Anchor", 0.5f);
    addParam("flowRate", "Flow Rate", 0.5f);
    addParam("flowEnergy", "Flow Energy", 0.5f);
    addParam("flowTexture", "Flow Texture", 0.5f);
    addParam("pulseFrequency", "Pulse Frequency", 0.5f);
    addParam("pulseWidth", "Pulse Width", 0.5f);
    addParam("pulseEnergy", "Pulse Energy", 0.5f);
    addParam("fieldComplexity", "Field Complexity", 0.65f);
    addParam("fieldDepth", "Field Depth", 0.18f);
    addParam("instability", "Instability", 0.28f);
    addParam("radiance", "Radiance Flux", 0.0f);
    addParam("charge", "Charge Intake", 0.0f);
    addParam("discharge", "Discharge Pulse", 0.0f);
    addParam("systemMatrix", "System Matrix", 0.34f);

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
    const juce::CriticalSection::ScopedLockType processLock(processingLock);
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

    resonance.prepare(fxSpec);
    field.prepare(fxSpec);
    radiance.prepare(fxSpec);

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

    const juce::CriticalSection::ScopedTryLockType processLock(processingLock);
    if (! processLock.isLocked())
    {
        buffer.clear();
        return;
    }

    if (handleOversamplingChangeIfNeeded(buffer))
        return;

    clearUnusedOutputChannels(buffer);
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
    for (const auto& preset : RootFlow::factoryPresets)
        names.add(preset.name);
    return names;
}

juce::StringArray RootFlowAudioProcessor::getFactoryPresetSections() const
{
    juce::StringArray sections;
    
    for (const auto& section : RootFlow::factoryPresetSections)
        sections.add(section.title);
        
    return sections;
}

int RootFlowAudioProcessor::getNumFactoryPresets() const
{
    return (int) RootFlow::factoryPresets.size();
}

juce::StringArray RootFlowAudioProcessor::getCombinedPresetNames() const
{
    auto names = getFactoryPresetNames();

    const juce::ScopedLock lock(presetStateLock);
    for (const auto& preset : userPresets)
        names.add(preset.name);

    return names;
}

int RootFlowAudioProcessor::getNumPresets() const
{
    return (int) RootFlow::factoryPresets.size() + (int) userPresets.size();
}

int RootFlowAudioProcessor::getCurrentPresetMenuIndex() const noexcept
{
    const int factoryPresetIndex = currentFactoryPresetIndex.load(std::memory_order_relaxed);
    if (factoryPresetIndex >= 0)
        return factoryPresetIndex;

    const int userPresetIndex = currentUserPresetIndex.load(std::memory_order_relaxed);
    if (userPresetIndex >= 0)
    {
        return (int) RootFlow::factoryPresets.size() + userPresetIndex;
    } return -1;
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
    auto target = RootFlow::resolvePromptPatchTarget(trimmedPrompt, promptMemorySnapshot, &recalled);
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
    const auto targetA = RootFlow::resolvePromptPatchTarget(trimmedA, promptMemorySnapshot, &recalledA);
    const auto targetB = RootFlow::resolvePromptPatchTarget(trimmedB, promptMemorySnapshot, &recalledB);
    auto target = RootFlow::morphPromptPatchTargets(targetA, targetB, morphAmount);
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
        const auto normalizedPrompt = RootFlow::normalizePromptText(trimmedPrompt).trim();
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
    const auto normalizedPrompt = RootFlow::normalizePromptText(trimmedPrompt).trim();
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
        if (RootFlow::normalizePromptText(lastPromptSeed).trim() == normalizedPrompt)
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
    if (! juce::isPositiveAndBelow(index, (int) RootFlow::factoryPresets.size()))
        return;

    const auto& preset = RootFlow::factoryPresets[(size_t) index];
    const auto masterSettings = getFactoryPresetMasterSettings(index, preset);
    presetLoadInProgress.fetch_add(1, std::memory_order_acq_rel);

    // Bio-Jitter: each load is subtly unique, like a plant that never
    // grows exactly the same way twice (+/- 1% variation)
    juce::Random bioRng (juce::Time::getCurrentTime().toMilliseconds());
    const size_t numPresetValues = preset.values.size(); // 16

    for (size_t i = 0; i < RootFlow::factoryPresetValueParameterIDs.size(); ++i)
    {
        if (auto* parameter = tree.getParameter(RootFlow::factoryPresetValueParameterIDs[i]))
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

    const int factoryCount = (int) RootFlow::factoryPresets.size();
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

    migrateLegacyState(userPresetState);

    const float preservedMasterVolume = tree.getParameter(presetGlobalOutputParameterID) != nullptr
        ? tree.getParameter(presetGlobalOutputParameterID)->getValue()
        : 0.0f;

    presetLoadInProgress.fetch_add(1, std::memory_order_acq_rel);
    tree.replaceState(userPresetState);
    if (auto* parameter = tree.getParameter(presetGlobalOutputParameterID))
        parameter->setValueNotifyingHost(preservedMasterVolume);
    presetLoadInProgress.fetch_sub(1, std::memory_order_acq_rel);
    restoreSequencerState(userPresetState);
    restoreNodeSystemState(userPresetState);
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

int RootFlowAudioProcessor::getCombinedPresetCount() const noexcept
{
    return getFactoryPresetCount() + (int) userPresets.size();
}

int RootFlowAudioProcessor::getFactoryPresetCount() const noexcept
{
    return (int) RootFlow::factoryPresets.size();
}

std::vector<RootFlowAudioProcessor::PresetMenuSection> RootFlowAudioProcessor::getFactoryPresetMenuSections() const
{
    std::vector<PresetMenuSection> sections;
    for (const auto& def : RootFlow::factoryPresetSections)
    {
        PresetMenuSection section;
        section.title = def.title;
        section.startIndex = def.startIndex;
        section.count = def.count;
        sections.push_back(section);
    }
    return sections;
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

            migrateLegacyPromptMemoryEntry(entry);
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
        auto existingTarget = RootFlow::readPromptTargetFromMemoryEntry(existing);
        const auto newTarget = RootFlow::readPromptTargetFromMemoryEntry(entry);
        RootFlow::blendPromptPatchTargets(existingTarget, newTarget, juce::jlimit(0.18f, 0.86f, 0.20f + reinforcement * 0.24f));
        RootFlow::writePromptTargetToMemoryEntry(existing, existingTarget);
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

    applyPendingOversamplingReconfigure();

    if (processingStateResetPending.load(std::memory_order_acquire) != 0)
        performPendingProcessingStateReset();

    nodeSystem.update(getRMS(), (float)juce::Time::getMillisecondCounterHiRes() * 0.001f, beatPhase.load());
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
            { 20, "sourceDepth" },
            { 21, "flowRate" },
            { 22, "flowEnergy" },
            { 23, "pulseWidth" },
            { 24, "fieldComplexity" },
            { 25, "radiance" },
            { 26, "charge" },
            { 27, "discharge" }
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

                // Cybernetic pulse jitter – the node breathes from the hit
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

    appendNodeSystemState(state);
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

    restoreNodeSystemState(state);

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
    appendNodeSystemState(state);
    if (auto child = state.getChildWithName(midiMappingStateTag); child.isValid())
        state.removeChild(child, nullptr);
    if (auto child = state.getChildWithName(midiBindingsTag); child.isValid())
        state.removeChild(child, nullptr);
    if (auto child = state.getChildWithName("CONNECTIONS"); child.isValid())
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

void RootFlowAudioProcessor::appendNodeSystemState(juce::ValueTree& state) const
{
    if (auto existing = state.getChildWithName(nodeSystemTag); existing.isValid())
        state.removeChild(existing, nullptr);

    juce::ValueTree nodeSystemState(nodeSystemTag);
    {
        const juce::ScopedLock nodeLock(nodeSystem.getLock());
        const auto& nodes = nodeSystem.getNodes();
        for (int i = 0; i < (int) nodes.size(); ++i)
        {
            juce::ValueTree nState(nodeTag);
            nState.setProperty("idx", i, nullptr);
            nState.setProperty("x", nodes[(size_t) i].position.x, nullptr);
            nState.setProperty("y", nodes[(size_t) i].position.y, nullptr);
            nodeSystemState.addChild(nState, -1, nullptr);
        }

        const auto& connections = nodeSystem.getConnections();
        for (const auto& c : connections)
        {
            juce::ValueTree cState(connectionTag);
            cState.setProperty("src", c.source, nullptr);
            cState.setProperty("dst", c.target, nullptr);
            cState.setProperty("amt", c.amount, nullptr);
            cState.setProperty("h", c.health, nullptr);
            cState.setProperty("p", c.isPersistent, nullptr);
            nodeSystemState.addChild(cState, -1, nullptr);
        }
    }

    state.addChild(nodeSystemState, -1, nullptr);
}

void RootFlowAudioProcessor::restoreNodeSystemState(const juce::ValueTree& state)
{
    auto nodeSystemState = state.getChildWithName(nodeSystemTag);
    if (! nodeSystemState.isValid())
        return;

    const juce::ScopedLock nodeLock(nodeSystem.getLock());
    auto& nodes = nodeSystem.getNodes();
    auto& connections = nodeSystem.getConnections();

    for (auto& node : nodes)
        node.slotIndex = getMappedParameterSlotForID(node.paramID);

    connections.clear();

    for (int i = 0; i < nodeSystemState.getNumChildren(); ++i)
    {
        auto child = nodeSystemState.getChild(i);
        if (child.hasType(nodeTag))
        {
            const int idx = child.getProperty("idx");
            if (! juce::isPositiveAndBelow(idx, (int) nodes.size()))
                continue;

            nodes[(size_t) idx].position.x = child.getProperty("x", nodes[(size_t) idx].position.x);
            nodes[(size_t) idx].position.y = child.getProperty("y", nodes[(size_t) idx].position.y);
            continue;
        }

        if (! child.hasType(connectionTag))
            continue;

        NodeConnection connection;
        connection.source = child.getProperty("src", -1);
        connection.target = child.getProperty("dst", -1);
        connection.amount = child.getProperty("amt", 0.5f);
        connection.health = child.getProperty("h", 1.0f);
        connection.isPersistent = child.getProperty("p", false);
        connection.targetSlot = juce::isPositiveAndBelow(connection.target, (int) nodes.size())
            ? nodes[(size_t) connection.target].slotIndex
            : -1;

        for (int particleIndex = 0; particleIndex < 4; ++particleIndex)
        {
            FlowParticle particle;
            particle.t = juce::Random::getSystemRandom().nextFloat();
            particle.speed = 0.1f + juce::Random::getSystemRandom().nextFloat() * 0.3f;
            connection.particles.push_back(particle);
        }

        connections.push_back(std::move(connection));
    }
}

void RootFlowAudioProcessor::markSequencerStateDirty() noexcept
{
    currentPresetDirty.store(1, std::memory_order_relaxed);
}

void RootFlowAudioProcessor::markNodeSystemStateDirty() noexcept
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

float RootFlowAudioProcessor::getSystemEnergy() const
{
    return lastSystemEnergy.load(std::memory_order_relaxed);
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
    juce::ignoreUnused(newValue);

    if (parameterID == "oversampling")
        oversamplingReconfigurePending.store(1, std::memory_order_release);

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

    const juce::CriticalSection::ScopedLockType processLock(processingLock);
    const double safeSampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const int safeBlockSize = juce::jmax(getBlockSize(), 1);

    modulation.prepare(safeSampleRate, safeBlockSize);
    currentSystemFeedback = {};
    lastSystemEnergy.store(0.0f, std::memory_order_relaxed);

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<RootFlowVoice*>(synth.getVoice(i)))
        {
            voice->setEngine(&modulation);
            voice->setMidiExpressionState(&midiExpressionState);
            voice->setSampleRate(safeSampleRate, safeBlockSize);
        }
    }

    resonance.reset();
    field.reset();
    radiance.reset();

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
            migrateLegacyState(restoredState);
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
    const float energy = currentSystemFeedback.systemEnergy;

    double speedMod = 0.95 + (energy * 0.1);

    if (isRealRootFlowPreset())
        speedMod *= (1.0 + (systemRng.nextFloat() - 0.5) * 0.035);

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

bool RootFlowAudioProcessor::isRealRootFlowPreset() const noexcept
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

void RootFlowAudioProcessor::mutateSystem()
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

        const int lane = RootFlow::getMutationLane(paramID);
        const float laneScale = RootFlow::getLaneScale(lane, mode, context);
        if (laneScale <= 0.0f)
            continue;

        const float previousValue = param->getValue();

        if (RootFlow::isDiscreteMutationParameter(paramID))
            RootFlow::mutateDiscreteParameter(*param, paramID, r, mode, laneScale);
        else
            RootFlow::mutateContinuousParameter(*param, paramID, r, lane, context, mode, laneScale);

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

    // Trigger a visual energy pulse through the neural matrix.
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
            oversamplingReconfigurePending.store(1, std::memory_order_release);
            buffer.clear();
            return true;
        }
    }

    return false;
}

void RootFlowAudioProcessor::applyPendingOversamplingReconfigure()
{
    if (oversamplingReconfigurePending.exchange(0, std::memory_order_acq_rel) == 0)
        return;

    const int targetOversamplingFactor = tree.getRawParameterValue("oversampling") != nullptr
        ? (int) tree.getRawParameterValue("oversampling")->load()
        : 0;

    if (targetOversamplingFactor == currentOversamplingFactor)
        return;

    prepareToPlay(getSampleRate(), getBlockSize());
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

            // Moose with safety belt: soft limit first, then hard limit
            x = RootFlowDSP::softLimit(x);
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
        *tree.getRawParameterValue("sourceDepth"),
        *tree.getRawParameterValue("sourceCore"),
        *tree.getRawParameterValue("sourceAnchor"),
        *tree.getRawParameterValue("flowRate"),
        *tree.getRawParameterValue("flowEnergy"),
        *tree.getRawParameterValue("flowTexture"),
        *tree.getRawParameterValue("pulseFrequency"),
        *tree.getRawParameterValue("pulseWidth"),
        *tree.getRawParameterValue("pulseEnergy"),
        *tree.getRawParameterValue("fieldComplexity"),
        *tree.getRawParameterValue("fieldDepth"),
        *tree.getRawParameterValue("instability"),
        *tree.getRawParameterValue("radiance"),
        *tree.getRawParameterValue("charge"),
        *tree.getRawParameterValue("discharge"),
        *tree.getRawParameterValue("systemMatrix")
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

    const float systemMatrix = getBlockModulatedValue(15);
    const auto seasonMorph = RootFlowDSP::getSeasonMorph(systemMatrix);
    auto seasonBlend = [&](float base, float springOffset, float summerOffset, float autumnOffset, float winterOffset) {
        return juce::jlimit(0.0f, 1.0f, base + seasonMorph.spring * springOffset + seasonMorph.summer * summerOffset + seasonMorph.autumn * autumnOffset + seasonMorph.winter * winterOffset);
    };

    const float sourceDepth = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(0), 0.22f, 0.12f, 0.18f, -0.32f), 0.015f);
    const float sourceCore = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(1), -0.12f, -0.04f, 0.44f, 0.18f), 0.018f);
    const float sourceAnchor = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(2), -0.42f, -0.15f, 0.24f, 0.48f), 0.012f);
    const float flowRate = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(3), 0.28f, 0.24f, 0.04f, -0.32f), 0.12f);
    const float flowEnergy = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(4), 0.44f, 0.28f, -0.12f, -0.44f), 0.10f);
    const float flowTexture = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(5), -0.05f, 0.12f, 0.48f, 0.22f), 0.08f);
    const float pulseFrequency = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(6), 0.48f, 0.18f, -0.22f, -0.44f), 0.008f);
    const float pulseWidth = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(7), -0.24f, -0.08f, 0.36f, 0.24f), 0.012f);
    const float pulseEnergy = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(8), 0.48f, 0.14f, -0.12f, -0.44f), 0.008f);
    const float fieldComplexity = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(9), 0.12f, 0.22f, -0.04f, -0.32f), 0.09f);
    const float instabilityAmount = emphasizeMacroResponse(seasonBlend(getBlockModulatedValue(11), 0.05f, 0.12f, 0.22f, -0.14f), 0.025f);
    const float radianceAmount = applySeasonalFxScale(getBlockModulatedValue(12), seasonMorph, 0.22f, 0.14f, -0.10f, -0.22f, 0.65f);
    const float chargeAmount   = applySeasonalFxScale(getBlockModulatedValue(13), seasonMorph, 0.08f, -0.02f, 0.38f, 0.12f, 0.85f);
    const float dischargeAmount = applySeasonalFxScale(getBlockModulatedValue(14), seasonMorph, 0.40f, 0.16f, 0.00f, -0.16f, 1.05f);

    currentProcessingBlockState.seasonMacro = systemMatrix;
    currentProcessingBlockState.sourceDepth = sourceDepth;
    currentProcessingBlockState.sourceAnchor = sourceAnchor;
    currentProcessingBlockState.flowRate = flowRate;
    currentProcessingBlockState.flowEnergy = flowEnergy;
    currentProcessingBlockState.flowTexture = flowTexture;
    currentProcessingBlockState.pulseWidth = pulseWidth;
    currentProcessingBlockState.fieldComplexity = fieldComplexity;
    currentProcessingBlockState.radianceAmount = radianceAmount;
    currentProcessingBlockState.chargeAmount = chargeAmount;
    currentProcessingBlockState.dischargeAmount = dischargeAmount;

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<RootFlowVoice*>(synth.getVoice(i)))
        {
            voice->setFlow(flowRate); voice->setEnergy(flowEnergy); voice->setTexture(flowTexture);
            voice->setDepth(sourceDepth); voice->setPulseEnergy(pulseEnergy); voice->setPulseFrequency(pulseFrequency);
            voice->setPulseWidth(pulseWidth); voice->setFieldComplexity(fieldComplexity); voice->setInstability(instabilityAmount);
            voice->setWaveform((int)(*tree.getRawParameterValue("oscWave")));
        }
    }

    // Internal voice unison count is determined by the system matrix at the time of note-on
    // to avoid clicks from dynamically changing voice counts.
    // (Handled inside RootFlowVoice::startNote now)

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

    modulation.setParams(sourceDepth, sourceCore, sourceAnchor, flowRate, flowEnergy, flowTexture, pulseFrequency, pulseWidth, pulseEnergy);
    modulation.update(procBuffer);
    currentSystemFeedback = modulation.getSystemFeedbackSnapshot();
    lastSystemEnergy.store(currentSystemFeedback.systemEnergy, std::memory_order_relaxed);
}

void RootFlowAudioProcessor::applyGlobalFx(juce::AudioBuffer<float>& procBuffer)
{
    const float systemEnergy = currentSystemFeedback.systemEnergy;
    lastSystemEnergy.store(systemEnergy, std::memory_order_relaxed);

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
    const float ecoRaw = RootFlowDSP::clamp01(*tree.getRawParameterValue("systemMatrix"));
    const float ecoMaster = std::pow(ecoRaw, 1.35f);
    const auto seasonMorph = RootFlowDSP::getSeasonMorph(blockState.seasonMacro);
    const float fxEnergy = juce::jlimit(0.0f, 1.0f, systemEnergy * (0.46f + seasonMorph.spring * 0.06f + seasonMorph.summer * 0.12f + seasonMorph.autumn * 0.16f + seasonMorph.winter * 0.04f) + 0.01f) * ecoMaster;

    resonance.setParams(blockState.radianceAmount * ecoMaster, fxEnergy, blockState.flowEnergy, blockState.pulseWidth, blockState.fieldComplexity, blockState.sourceAnchor);
    resonance.process(procBuffer);

    field.setParams(blockState.chargeAmount * ecoMaster, fxEnergy, blockState.flowRate, blockState.flowTexture, blockState.pulseWidth, blockState.fieldComplexity);
    field.process(procBuffer);

    radiance.setParams(blockState.dischargeAmount * ecoMaster, fxEnergy, blockState.sourceDepth, blockState.flowEnergy, blockState.pulseWidth, blockState.fieldComplexity);
    radiance.process(procBuffer);

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
            if ((i & 15) == 0)
            {
                const float clampedFreq = clampStateVariableCutoff(monoFreq, fxSampleRate);
                monoMakerFilters[0].setCutoffFrequency(clampedFreq);
                monoMakerFilters[1].setCutoffFrequency(clampedFreq);
            }

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
