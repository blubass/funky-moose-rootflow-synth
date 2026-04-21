#include "RootFlowMutation.h"
#include "PluginProcessor.h" // We might need this for some constants, or we move constants to an internal header

namespace RootFlow
{
    // --- Internal Helpers & Templates ---
    namespace
    {
        constexpr int promptTargetSchemaVersion = 2;
        constexpr std::array<int, 16> legacyPromptValueIndexToCurrent {{
            0, 1, 2,
            3, 4, 5,
            6, 7, 8,
            9, 10, 11,
            13, 14, 15, 12
        }};

        float rhythmicPulse(float pos, std::initializer_list<float> divisions, float fuzz) noexcept
        {
            for (float div : divisions)
                if (std::abs(pos - div) < fuzz) return 1.0f - (std::abs(pos - div) / fuzz);
            return 0.0f;
        }

        float mixFloat(float a, float b, float blend) noexcept { return a + (b - a) * blend; }
        int mixInt(int a, int b, float blend) noexcept { return juce::roundToInt((float)a + (float)(b - a) * blend); }

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

        MutationProfile getMutationProfile(MutationMode mode, const MutationContext& ctx) noexcept
        {
            MutationProfile profile;
            switch (mode)
            {
                case MutationMode::gentle:
                    profile.continuousDepth = 0.030f + ctx.evolution * 0.040f;
                    profile.discreteChance = 0.16f + ctx.evolution * 0.08f;
                    profile.centerPull = 0.10f;
                    profile.sequencerDepth = 0.08f + ctx.motionWeight * 0.05f;
                    profile.masterScale = 0.42f;
                    profile.wildness = 0.15f;
                    break;
                case MutationMode::balanced:
                    profile.continuousDepth = 0.055f + ctx.evolution * 0.070f;
                    profile.discreteChance = 0.28f + ctx.evolution * 0.10f;
                    profile.centerPull = 0.06f;
                    profile.sequencerDepth = 0.12f + ctx.motionWeight * 0.06f;
                    profile.masterScale = 0.56f;
                    profile.wildness = 0.45f;
                    break;
                case MutationMode::wild:
                    profile.continuousDepth = 0.100f + ctx.evolution * 0.120f;
                    profile.discreteChance = 0.48f + ctx.evolution * 0.14f;
                    profile.centerPull = 0.02f;
                    profile.sequencerDepth = 0.18f + ctx.motionWeight * 0.08f;
                    profile.masterScale = 0.66f;
                    profile.wildness = 1.0f;
                    break;
                case MutationMode::sequencer:
                    profile.continuousDepth = 0.0f;
                    profile.discreteChance = 0.52f + ctx.evolution * 0.10f;
                    profile.centerPull = 0.0f;
                    profile.sequencerDepth = 0.18f + ctx.motionWeight * 0.10f + ctx.evolution * 0.08f;
                    profile.masterScale = 0.0f;
                    profile.wildness = 0.72f;
                    profile.sequencerOnly = true;
                    break;
            }
            return profile;
        }

        bool promptContainsAny(const juce::String& text, std::initializer_list<const char*> keywords) noexcept
        {
            for (const auto* k : keywords) if (text.containsIgnoreCase(k)) return true;
            return false;
        }

        float promptMetric(const juce::String& text, std::initializer_list<const char*> keywords, float weight = 1.0f) noexcept
        {
            float hits = 0;
            for (const auto* k : keywords) if (text.containsIgnoreCase(k)) hits += 1.0f;
            return juce::jlimit(0.0f, 1.0f, (hits * weight) / 2.0f);
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
                default: return PromptPatternArchetype::matrix;
            }
        }
    }

    // --- Implementation ---

    void buildSmartSequencerSteps(std::array<SequencerStep, 16>& steps,
                                   int stepCount,
                                   MutationMode mode,
                                   const MutationContext& context,
                                   juce::Random& rng,
                                   const PromptRhythmProfile& promptRhythmProfile)
    {
        const auto profile = getMutationProfile(mode, context);
        const int limitedStepCount = juce::jlimit(1, 16, stepCount);
        
        const float baseDensity = juce::jlimit(0.16f, 0.82f, 
                                               0.20f + context.motionWeight * 0.26f + context.evolution * 0.12f + 
                                               promptRhythmProfile.densityBias + context.instability * 0.08f + 
                                               (mode == MutationMode::gentle ? -0.07f : (mode == MutationMode::wild ? 0.08f : 0.0f)));

        const int targetActiveCount = juce::jlimit(mode == MutationMode::gentle ? 1 : 2, 
                                                   limitedStepCount, 
                                                   juce::roundToInt((float)limitedStepCount * baseDensity));

        for (int i = 0; i < 16; ++i)
        {
            steps[(size_t)i].active = false;
            steps[(size_t)i].velocity = 0.72f;
            steps[(size_t)i].probability = 1.0f;
            steps[(size_t)i].timingOffset = 0.0f;
        }

        for (int i = 0; i < limitedStepCount; ++i)
        {
            float pos = (float)i / (float)limitedStepCount;
            float downbeat = rhythmicPulse(pos, { 0.0f, 0.25f, 0.5f, 0.75f }, 0.1f);
            
            if (downbeat > 0.9f || rng.nextFloat() < baseDensity)
            {
                steps[(size_t)i].active = true;
                steps[(size_t)i].velocity = 0.6f + rng.nextFloat() * 0.4f;
                steps[(size_t)i].probability = 0.7f + rng.nextFloat() * 0.3f;
            }
        }
    }

    juce::String normalizePromptText(juce::String text)
    {
        text = text.toLowerCase();
        text = text.replace("\n", " ").replace("\t", " ");
        text = text.replace("ä", "ae").replace("ö", "oe").replace("ü", "ue").replace("ß", "ss");
        return text;
    }

    juce::StringArray tokenizePrompt(const juce::String& rawPrompt)
    {
        auto normalized = normalizePromptText(rawPrompt);
        juce::StringArray tokens;
        tokens.addTokens(normalized, " ,.;:!?/\\|()[]{}<>+-_=*&^%$#@~`'\"", "");
        tokens.removeEmptyStrings();
        return tokens;
    }

    PromptIntent analysePromptIntent(const juce::String& rawPrompt)
    {
        const auto prompt = normalizePromptText(rawPrompt);
        PromptIntent intent;

        intent.darkness = promptMetric(prompt, { "dark", "deep", "night", "black", "shadow", "moody" });
        intent.brightness = promptMetric(prompt, { "bright", "light", "glow", "shimmer", "clear", "crisp" });
        intent.warmth = promptMetric(prompt, { "warm", "analog", "tube", "saturation", "vintage", "soft" });
        intent.cold = promptMetric(prompt, { "cold", "icy", "glass", "crystal", "sterile", "frost" });
        intent.air = promptMetric(prompt, { "air", "airy", "wide", "open", "breath", "hazy" });
        intent.wet = promptMetric(prompt, { "wet", "reverb", "space", "ambient", "hall", "wash" });
        intent.bass = promptMetric(prompt, { "bass", "sub", "lowend", "bottom", "fundament", "808" });
        intent.motion = promptMetric(prompt, { "moving", "motion", "flow", "drift", "animated", "swirl" });
        intent.rhythm = promptMetric(prompt, { "rhythm", "groove", "beat", "pulse", "sequence", "arp" });
        intent.aggression = promptMetric(prompt, { "aggressive", "hard", "sharp", "push", "fierce", "nasty" });
        intent.softness = promptMetric(prompt, { "soft", "smooth", "gentle", "round", "silky", "calm" });
        intent.grit = promptMetric(prompt, { "dirty", "grit", "rough", "raw", "noise", "distort" });
        intent.shimmer = promptMetric(prompt, { "shimmer", "sparkle", "bloom", "glow", "ethereal" });
        intent.complexity = promptMetric(prompt, { "complex", "matrix", "grid", "system", "synthetic", "terminal" });
        intent.evolution = promptMetric(prompt, { "reconfig", "matrix shift", "morph", "unfold", "transform" });
        intent.weird = promptMetric(prompt, { "weird", "alien", "chaos", "abstract", "glitch", "wonky" });
        
        intent.techno = promptMetric(prompt, { "techno", "warehouse", "peak time" }, 1.3f);
        intent.house = promptMetric(prompt, { "house", "deep house", "club house" }, 1.3f);
        intent.trance = promptMetric(prompt, { "trance", "uplifting", "euphoric" }, 1.3f);
        intent.synthwave = promptMetric(prompt, { "synthwave", "retrowave", "80s" }, 1.3f);
        
        return intent;
    }

    PromptPatchTarget generatePromptPatch(const PromptIntent& intent, juce::Random& rng)
    {
        PromptPatchTarget target;
        for (int i = 0; i < 16; ++i) target.values[(size_t)i] = 0.5f;

        // Simplified mapping based on intent
        target.values[0] = juce::jlimit(0.0f, 1.0f, 0.4f + intent.darkness * 0.5f - intent.brightness * 0.3f); // sourceDepth
        target.values[1] = juce::jlimit(0.0f, 1.0f, 0.5f + intent.warmth * 0.4f - intent.cold * 0.3f);       // sourceCore
        target.values[3] = juce::jlimit(0.0f, 1.0f, 0.3f + intent.motion * 0.6f);                          // flowRate
        target.values[9] = juce::jlimit(0.0f, 1.0f, 0.2f + intent.air * 0.7f);                             // fieldComplexity
        target.values[10] = juce::jlimit(0.0f, 1.0f, 0.1f + intent.wet * 0.8f);                            // fieldDepth
        target.values[11] = juce::jlimit(0.0f, 1.0f, 0.2f + intent.weird * 0.6f + intent.grit * 0.2f);     // instability

        target.sequencerOn = intent.rhythm > 0.4f || intent.techno > 0.5f;
        target.oscWaveIndex = intent.brightness > 0.6f ? 1 : (intent.bass > 0.6f ? 2 : 0);
        
        target.summary = "Quantum Matrix Core Engine";
        return target;
    }

    float getLaneScale(int lane, MutationMode mode, const MutationContext& context) noexcept
    {
        const auto profile = getMutationProfile(mode, context);
        float scale = 1.0f;
        if (lane == 0) scale = 0.82f; // Core
        if (lane == 1) scale = 0.94f; // Flow
        if (lane == 2) scale = 1.12f; // Pulse
        
        if (context.isLocked((GrowLockGroup)lane)) return 0.0f;
        
        return scale * profile.masterScale;
    }

    void mutateContinuousParameter(juce::RangedAudioParameter& param,
                                   const juce::String& paramID,
                                   juce::Random& rng,
                                   int lane,
                                   const MutationContext& context,
                                   MutationMode mode,
                                   float laneScale) noexcept
    {
        const auto profile = getMutationProfile(mode, context);
        float current = param.getValue();
        float delta = (rng.nextFloat() * 2.0f - 1.0f) * profile.continuousDepth * laneScale;
        
        // Contextual pull toward center if instability is low
        if (context.instability < 0.35f)
            delta += (0.5f - current) * profile.centerPull;
            
        param.setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, current + delta));
    }

    void mutateDiscreteParameter(juce::RangedAudioParameter& param,
                                 const juce::String& paramID,
                                 juce::Random& rng,
                                 MutationMode mode,
                                 float laneScale) noexcept
    {
        // Simple discrete mutation for wave/rate etc
        if (rng.nextFloat() < 0.22f * laneScale)
        {
            if (paramID == "oscWave")
                param.setValueNotifyingHost((float)rng.nextInt(3) / 2.0f); // Normalize 0,1,2
            else if (paramID == "sequencerSteps")
                param.setValueNotifyingHost((float)(rng.nextInt(13)) / 12.0f); // Normalize steps
        }
    }

    PromptPatchTarget morphPromptPatchTargets(const PromptPatchTarget& a,
                                              const PromptPatchTarget& b,
                                              float morphAmount) noexcept
    {
        PromptPatchTarget result = a;
        for (size_t i = 0; i < a.values.size(); ++i)
            result.values[i] = mixFloat(a.values[i], b.values[i], morphAmount);
            
        result.sequencerRateIndex = morphAmount < 0.5f ? a.sequencerRateIndex : b.sequencerRateIndex;
        result.oscWaveIndex = morphAmount < 0.5f ? a.oscWaveIndex : b.oscWaveIndex;
        result.summary = (morphAmount < 0.5f ? a.summary : b.summary) + " (Morphed)";
        return result;
    }

    bool isDiscreteMutationParameter(const juce::String& paramID) noexcept
    {
        return paramID == "oscWave" || paramID == "sequencerRate" || paramID == "sequencerSteps";
    }

    int getMutationLane(const juce::String& paramID) noexcept
    {
        if (paramID.contains("source") || paramID.contains("osc")) return 0; // Core/Source
        if (paramID.contains("flow") || paramID.contains("radiance")) return 1; // Flow/Texture
        if (paramID.contains("pulse") || paramID.contains("field")) return 2; // Pulse/Space
        return 3;
    }

    void writePromptTargetToMemoryEntry(juce::ValueTree& vt, const PromptPatchTarget& target)
    {
        vt.setProperty("promptTargetVersion", promptTargetSchemaVersion, nullptr);
        vt.setProperty("summary", target.summary, nullptr);
        vt.setProperty("sequencerOn", target.sequencerOn, nullptr);
        vt.setProperty("oscWaveIndex", target.oscWaveIndex, nullptr);
        vt.setProperty("sequencerRateIndex", target.sequencerRateIndex, nullptr);
        vt.setProperty("sequencerStepCount", target.sequencerStepCount, nullptr);
        vt.setProperty("sequencerGate", target.sequencerGate, nullptr);
        vt.setProperty("sequencerProbOn", target.sequencerProbOn, nullptr);
        vt.setProperty("sequencerJitterOn", target.sequencerJitterOn, nullptr);
        vt.setProperty("masterCompressor", target.masterCompressor, nullptr);
        vt.setProperty("masterMix", target.masterMix, nullptr);
        vt.setProperty("monoMaker", target.monoMaker, nullptr);
        vt.setProperty("monoMakerFreqHz", target.monoMakerFreqHz, nullptr);
        vt.setProperty("evolution", target.evolution, nullptr);
        vt.setProperty("rhythmArchetype", (int) target.rhythmProfile.archetype, nullptr);
        vt.setProperty("rhythmDensityBias", target.rhythmProfile.densityBias, nullptr);
        vt.setProperty("rhythmAnchorBias", target.rhythmProfile.anchorBias, nullptr);
        vt.setProperty("rhythmOffbeatBias", target.rhythmProfile.offbeatBias, nullptr);
        vt.setProperty("rhythmTripletBias", target.rhythmProfile.tripletBias, nullptr);
        vt.setProperty("rhythmSwingAmount", target.rhythmProfile.swingAmount, nullptr);
        vt.setProperty("rhythmHumanize", target.rhythmProfile.humanize, nullptr);
        vt.setProperty("rhythmTightness", target.rhythmProfile.tightness, nullptr);

        for (auto existingValues = vt.getChildWithName("values"); existingValues.isValid(); existingValues = vt.getChildWithName("values"))
            vt.removeChild(existingValues, nullptr);
        
        juce::ValueTree values("values");
        for (float val : target.values)
        {
            juce::ValueTree v("v");
            v.setProperty("val", val, nullptr);
            values.appendChild(v, nullptr);
        }
        vt.appendChild(values, nullptr);
    }

    void blendPromptPatchTargets(PromptPatchTarget& target, const PromptPatchTarget& extra, float amount) noexcept
    {
        for (size_t i = 0; i < target.values.size(); ++i)
            target.values[i] = mixFloat(target.values[i], extra.values[i], amount);
            
        if (amount > 0.5f)
        {
            target.oscWaveIndex = extra.oscWaveIndex;
            target.sequencerRateIndex = extra.sequencerRateIndex;
        }
    }

    PromptPatchTarget readPromptTargetFromMemoryEntry(const juce::ValueTree& vt) noexcept
    {
        PromptPatchTarget target;
        target.summary = vt.getProperty("summary", "Restored Patch");
        target.sequencerOn = vt.getProperty("sequencerOn", false);
        target.oscWaveIndex = vt.getProperty("oscWaveIndex", 0);
        target.sequencerRateIndex = vt.getProperty("sequencerRateIndex", 4);
        target.sequencerStepCount = vt.getProperty("sequencerStepCount", target.sequencerStepCount);
        target.sequencerGate = vt.getProperty("sequencerGate", target.sequencerGate);
        target.sequencerProbOn = vt.getProperty("sequencerProbOn", target.sequencerProbOn);
        target.sequencerJitterOn = vt.getProperty("sequencerJitterOn", target.sequencerJitterOn);
        target.masterCompressor = vt.getProperty("masterCompressor", target.masterCompressor);
        target.masterMix = vt.getProperty("masterMix", target.masterMix);
        target.monoMaker = vt.getProperty("monoMaker", target.monoMaker);
        target.monoMakerFreqHz = vt.getProperty("monoMakerFreqHz", target.monoMakerFreqHz);
        target.evolution = vt.getProperty("evolution", target.evolution);
        target.rhythmProfile.archetype = (RootFlow::PromptPatternArchetype) (int) vt.getProperty("rhythmArchetype",
                                                                                                   (int) target.rhythmProfile.archetype);
        target.rhythmProfile.densityBias = vt.getProperty("rhythmDensityBias", target.rhythmProfile.densityBias);
        target.rhythmProfile.anchorBias = vt.getProperty("rhythmAnchorBias", target.rhythmProfile.anchorBias);
        target.rhythmProfile.offbeatBias = vt.getProperty("rhythmOffbeatBias", target.rhythmProfile.offbeatBias);
        target.rhythmProfile.tripletBias = vt.getProperty("rhythmTripletBias", target.rhythmProfile.tripletBias);
        target.rhythmProfile.swingAmount = vt.getProperty("rhythmSwingAmount", target.rhythmProfile.swingAmount);
        target.rhythmProfile.humanize = vt.getProperty("rhythmHumanize", target.rhythmProfile.humanize);
        target.rhythmProfile.tightness = vt.getProperty("rhythmTightness", target.rhythmProfile.tightness);
        
        if (auto values = vt.getChildWithName("values"); values.isValid())
        {
            const bool legacyOrder = (int) vt.getProperty("promptTargetVersion", 0) < promptTargetSchemaVersion;

            for (int i = 0; i < 16 && i < values.getNumChildren(); ++i)
            {
                const int valueIndex = legacyOrder ? legacyPromptValueIndexToCurrent[(size_t) i] : i;
                if (juce::isPositiveAndBelow(valueIndex, values.getNumChildren()))
                    target.values[(size_t) i] = values.getChild(valueIndex).getProperty("val", target.values[(size_t) i]);
            }
        }
        
        return target;
    }

    void applyPromptTargetParameters(juce::AudioProcessorValueTreeState& tree, const RootFlow::PromptPatchTarget& target)
    {
        auto setBoolParameter = [&tree](const juce::String& paramID, bool value)
        {
            if (auto* param = tree.getParameter(paramID))
                param->setValueNotifyingHost(value ? 1.0f : 0.0f);
        };

        auto setDenormalizedParameter = [&tree](const juce::String& paramID, float value)
        {
            if (auto* param = dynamic_cast<juce::RangedAudioParameter*>(tree.getParameter(paramID)))
                param->setValueNotifyingHost(param->convertTo0to1(value));
        };

        for (int i = 0; i < 16; ++i)
            setDenormalizedParameter(RootFlow::factoryPresetValueParameterIDs[(size_t) i],
                                     juce::jlimit(0.0f, 1.0f, target.values[(size_t) i]));
        
        setBoolParameter("sequencerOn", target.sequencerOn);
        setDenormalizedParameter("sequencerRate", (float) juce::jlimit(0, 3, target.sequencerRateIndex));
        setDenormalizedParameter("sequencerSteps", (float) juce::jlimit(4, 16, target.sequencerStepCount));
        setDenormalizedParameter("sequencerGate", juce::jlimit(0.1f, 1.0f, target.sequencerGate));
        setBoolParameter("sequencerProbOn", target.sequencerProbOn);
        setBoolParameter("sequencerJitterOn", target.sequencerJitterOn);
        setDenormalizedParameter("oscWave", (float) juce::jlimit(0, 2, target.oscWaveIndex));
        setDenormalizedParameter("masterCompressor", juce::jlimit(0.0f, 1.0f, target.masterCompressor));
        setDenormalizedParameter("masterMix", juce::jlimit(0.0f, 1.0f, target.masterMix));
        setBoolParameter("monoMakerToggle", target.monoMaker);
        setDenormalizedParameter("monoMakerFreq", juce::jlimit(20.0f, 400.0f, target.monoMakerFreqHz));
        setDenormalizedParameter("evolution", juce::jlimit(0.0f, 1.0f, target.evolution));
    }

    void applyGrowLocksToPromptTarget(RootFlow::PromptPatchTarget& target,
                                      int growLockMask,
                                      const juce::AudioProcessorValueTreeState& tree)
    {
        for (int i = 0; i < 16; ++i)
        {
            const int lane = getMutationLane(RootFlow::factoryPresetValueParameterIDs[(size_t)i]);
            if ((growLockMask & (1 << lane)) != 0)
            {
                if (auto* param = tree.getParameter(RootFlow::factoryPresetValueParameterIDs[(size_t)i]))
                    target.values[(size_t)i] = param->getValue();
            }
        }
    }

    PromptPatchTarget buildPromptPatchTarget(const juce::String& rawPrompt)
    {
        auto intent = analysePromptIntent(rawPrompt);
        juce::Random rng (juce::Time::getCurrentTime().toMilliseconds());
        return generatePromptPatch(intent, rng);
    }

    PromptMemoryMatch findPromptMemoryMatch(const juce::String& prompt, const std::vector<juce::ValueTree>& entries)
    {
        PromptMemoryMatch match;
        const auto normalized = normalizePromptText(prompt).trim();
        auto promptTokens = tokenizePrompt(prompt);

        for (const auto& entry : entries)
        {
            const auto entryPrompt = entry.getProperty("prompt", {}).toString();
            const auto entryNormalized = entry.getProperty("normalized",
                                                           normalizePromptText(entryPrompt)).toString().trim();
            if (entryNormalized.isEmpty())
                continue;

            float score = 0.0f;
            if (entryNormalized == normalized)
            {
                score = 1.15f;
            }
            else
            {
                juce::StringArray entryTokens;
                entryTokens.addTokens(entry.getProperty("tokens", {}).toString(), "|", "");
                entryTokens.removeEmptyStrings();

                if (entryTokens.isEmpty())
                    entryTokens = tokenizePrompt(entryPrompt);

                int overlapCount = 0;
                for (const auto& token : promptTokens)
                    if (token.isNotEmpty() && entryTokens.contains(token))
                        ++overlapCount;

                const float tokenScore = (promptTokens.isEmpty() || entryTokens.isEmpty())
                    ? 0.0f
                    : (2.0f * (float) overlapCount) / (float) (promptTokens.size() + entryTokens.size());
                const float phraseScore = (entryNormalized.contains(normalized) || normalized.contains(entryNormalized))
                    ? 0.20f
                    : 0.0f;
                const float strengthBonus = juce::jlimit(0.0f, 0.12f, (float) entry.getProperty("strength", 1.0f) * 0.02f);
                score = juce::jlimit(0.0f, 1.0f, tokenScore + phraseScore + strengthBonus);
            }

            if (score > match.score)
            {
                match.entry = entry;
                match.score = score;
                match.blendAmount = juce::jlimit(0.0f, 1.0f, 1.0f - score);
            }
        }

        return match;
    }

    PromptPatchTarget resolvePromptPatchTarget(const juce::String& prompt,
                                               const std::vector<juce::ValueTree>& promptMemorySnapshot,
                                               bool* usedRecall)
    {
        auto match = findPromptMemoryMatch(prompt, promptMemorySnapshot);
        if (match.score > 0.9f)
        {
            if (usedRecall) *usedRecall = true;
            return readPromptTargetFromMemoryEntry(match.entry);
        }
        
        if (usedRecall) *usedRecall = false;
        return buildPromptPatchTarget(prompt);
    }
}
