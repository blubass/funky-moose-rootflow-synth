#pragma once
#include <JuceHeader.h>
#include <array>

namespace RootFlow
{
    enum class MutationMode : int
    {
        gentle = 0,
        balanced,
        wild,
        sequencer
    };

    enum class GrowLockGroup : int
    {
        root = 0,
        core = 0, // Alias for legacy/UI consistency
        motion,
        spectral,
        fx,
        sequencer
    };

    struct SequencerStep
    {
        float velocity = 0.8f;
        float probability = 1.0f;
        float timingOffset = 0.0f;
        bool active = true;
    };

    struct MutationContext
    {
        float evolution = 0.5f;
        float instability = 0.0f;
        float coreWeight = 0.5f;
        float motionWeight = 0.5f;
        float spectralWeight = 0.5f;
        float fxWeight = 0.5f;
        float energy = 0.5f;
        int growLockMask = 0;

        bool isLocked(GrowLockGroup group) const noexcept 
        { 
            return (growLockMask & (1 << (int)group)) != 0; 
        }
    };

    enum class PromptPatternArchetype : int
    {
        matrix = 0,
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
        PromptPatternArchetype archetype = PromptPatternArchetype::matrix;
        float densityBias = 0.0f;
        float anchorBias = 0.0f;
        float offbeatBias = 0.0f;
        float tripletBias = 0.0f;
        float swingAmount = 0.0f;
        float humanize = 0.0f;
        float tightness = 0.58f;
    };

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
        float complexity = 0.0f;
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

    // --- Logic Functions ---
    void buildSmartSequencerSteps(std::array<SequencerStep, 16>& steps,
                                   int stepCount,
                                   MutationMode mode,
                                   const MutationContext& context,
                                   juce::Random& rng,
                                   const PromptRhythmProfile& promptRhythmProfile);

    float getLaneScale(int lane, MutationMode mode, const MutationContext& context) noexcept;

    void mutateContinuousParameter(juce::RangedAudioParameter& param,
                                   const juce::String& paramID,
                                   juce::Random& rng,
                                   int lane,
                                   const MutationContext& context,
                                   MutationMode mode,
                                   float laneScale) noexcept;

    void mutateDiscreteParameter(juce::RangedAudioParameter& param,
                                 const juce::String& paramID,
                                 juce::Random& rng,
                                 MutationMode mode,
                                 float laneScale) noexcept;

    bool isDiscreteMutationParameter(const juce::String& paramID) noexcept;
    int getMutationLane(const juce::String& paramID) noexcept;

    PromptPatchTarget morphPromptPatchTargets(const PromptPatchTarget& a,
                                              const PromptPatchTarget& b,
                                              float morphAmount) noexcept;

    void writePromptTargetToMemoryEntry(juce::ValueTree& vt, const PromptPatchTarget& target);
    void blendPromptPatchTargets(PromptPatchTarget& target, const PromptPatchTarget& extra, float amount) noexcept;
    PromptPatchTarget readPromptTargetFromMemoryEntry(const juce::ValueTree& vt) noexcept;

    PromptIntent analysePromptIntent(const juce::String& rawPrompt);
    PromptPatchTarget generatePromptPatch(const PromptIntent& intent, juce::Random& rng);
    
    juce::String normalizePromptText(juce::String text);
    juce::StringArray tokenizePrompt(const juce::String& rawPrompt);

    PromptPatchTarget buildPromptPatchTarget(const juce::String& rawPrompt);
    PromptMemoryMatch findPromptMemoryMatch(const juce::String& prompt, const std::vector<juce::ValueTree>& entries);
    PromptPatchTarget resolvePromptPatchTarget(const juce::String& prompt,
                                               const std::vector<juce::ValueTree>& promptMemorySnapshot,
                                               bool* usedRecall = nullptr);
    void applyPromptTargetParameters(juce::AudioProcessorValueTreeState& tree, const RootFlow::PromptPatchTarget& target);
    void applyGrowLocksToPromptTarget(RootFlow::PromptPatchTarget& target,
                                      int growLockMask,
                                      const juce::AudioProcessorValueTreeState& tree);
    

}
