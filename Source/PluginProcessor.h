#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <array>
#include <algorithm>
#include <memory>

#include "RootFlowModulators.h"
#include "RootFlowDSP.h"
#include "RootFlowVoice.h"
#include "RootFlowPresets.h"
#include "RootFlowMutation.h"
#include "UI/NodeSystem.h"

/**
 * Funky Moose Rootflow Synth - Main Audio Processor
 */
class RootFlowAudioProcessor : public juce::AudioProcessor,
                               private juce::HighResolutionTimer,
                               private juce::AudioProcessorValueTreeState::Listener
{
public:
    using MutationMode = RootFlow::MutationMode;
    using GrowLockGroup = RootFlow::GrowLockGroup;
    using SequencerStep = RootFlow::SequencerStep;

    RootFlowAudioProcessor();
    ~RootFlowAudioProcessor() override;

    // --- AudioProcessor Overrides ---
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Funky Moose Rootflow Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    bool supportsMPE() const override { return false; }
    double getTailLengthSeconds() const override { return 1.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override { return {}; }
    void changeProgramName(int index, const juce::String& newName) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- Data Structures ---
    struct MidiActivitySnapshot
    {
        enum class Type : int
        {
            none = 0,
            note,
            controller,
            pitchBend,
            pressure,
            timbre
        };

        uint32_t eventCounter = 0;
        Type type = Type::none;
        int controllerNumber = -1;
        int noteNumber = -1;
        int value = -1;
        int bendCents = 0;
        int channel = 1;
        bool wasMapped = false;
    };

    struct PresetMenuSection
    {
        juce::String title;
        int startIndex = 0;
        int count = 0;
    };

    struct PromptMemoryPreview
    {
        juce::String prompt;
        juce::String summary;
        float strength = 0.0f;
        int uses = 0;
    };

    using PromptRhythmProfile = RootFlow::PromptRhythmProfile;

    // --- Public API / UI Hooks ---
    float getSystemEnergy() const;
    float getSystemEnergyValue() const noexcept { return getSystemEnergy(); }
    float getOutputPeak() const noexcept { return currentOutputPeak.load(std::memory_order_relaxed); }
    
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    
    juce::StringArray getFactoryPresetNames() const;
    std::vector<PresetMenuSection> getFactoryPresetMenuSections() const;
    int getFactoryPresetCount() const noexcept;
    juce::StringArray getFactoryPresetSections() const;
    int getNumFactoryPresets() const;
    
    juce::StringArray getCombinedPresetNames() const;
    int getNumPresets() const;
    int getCombinedPresetCount() const noexcept;
    
    int getCurrentFactoryPresetIndex() const noexcept { return currentFactoryPresetIndex.load(std::memory_order_relaxed); }
    int getCurrentUserPresetIndex() const noexcept { return currentUserPresetIndex.load(std::memory_order_relaxed); }
    int getCurrentPresetMenuIndex() const noexcept;
    
    bool isCurrentPresetDirty() const noexcept { return currentPresetDirty.load(std::memory_order_relaxed) != 0; }
    
    void applyFactoryPreset(int index);
    void applyCombinedPreset(int menuIndex);
    void saveCurrentStateAsUserPreset();
    void deleteCurrentUserPreset();
    
    void setMidiLearnArmed(bool shouldArm);
    bool isMidiLearnArmed() const;
    void setMidiLearnTargetParameterID(const juce::String& paramID);
    juce::String getMidiLearnTargetParameterID() const;
    juce::String getMappedControllerTextForParameter(const juce::String& paramID) const;
    juce::String getParameterDisplayName(const juce::String& paramID) const;
    
    void resetToDefaultMpkMiniMappings();
    void clearMidiMappings();
    
    MidiActivitySnapshot getMidiActivitySnapshot() const noexcept;
    
    void setTestToneEnabled(bool shouldEnable) noexcept;
    bool isTestToneEnabled() const noexcept { return testToneEnabled.load(std::memory_order_relaxed) != 0; }
    
    float getRMS() const noexcept { return rmsLevel.load(std::memory_order_relaxed); }
    bool isSequencerEnabled() const noexcept { if (auto* p = tree.getRawParameterValue("sequencerOn")) return *p > 0.5f; return false; }

    float getModulatedValue(const juce::String& paramID) const;
    NodeSystem& getNodeSystem() { return nodeSystem; }

    // --- Cybernetic Mutation Engine ---
    void mutateSystem();
    void cycleMutationMode();
    void setMutationMode(MutationMode mode) noexcept;
    MutationMode getMutationMode() const noexcept;
    juce::String getMutationModeShortLabel() const;
    juce::String getMutationModeDisplayName() const;
    
    void applyPromptPatch(const juce::String& prompt);
    void applyPromptMorph(const juce::String& promptA, const juce::String& promptB, float morphAmount);
    
    juce::String getLastPromptSummary() const;
    juce::String getLastPromptSeed() const;
    std::vector<PromptMemoryPreview> getPromptMemoryPreviews(int maxItems) const;
    bool removePromptMemoryEntry(const juce::String& prompt);
    
    void setGrowLockEnabled(GrowLockGroup group, bool shouldEnable) noexcept;
    void toggleGrowLock(GrowLockGroup group) noexcept;
    bool isGrowLockEnabled(GrowLockGroup group) const noexcept;
    void markNodeSystemStateDirty() noexcept;

    // --- Analysis ---
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    bool getNextFFTBlock(float* destData);

    juce::AudioProcessorValueTreeState tree;
    std::atomic<bool> sequencerTriggered { false };
    std::atomic<float> beatPhase { 0.0f };

private:
    static constexpr int fftQueueCapacity = 8;
    static constexpr int mappedParameterSlotCount = 32;
    static constexpr uint32_t mappedParameterGestureTimeoutMs = 180;

    enum class MidiMappingMode : int
    {
        defaultMappings = 0,
        empty,
        custom
    };

    struct MidiBinding
    {
        int channel = 1;
        int controllerNumber = 0;
        juce::String parameterID;
        int parameterSlot = -1;
    };

    struct MidiBindingLookupSnapshot
    {
        std::array<int, 16 * 128> parameterSlots {};
        std::array<juce::String, mappedParameterSlotCount> parameterControllerTexts {};

        MidiBindingLookupSnapshot()
        {
            parameterSlots.fill(-1);
        }

        int getParameterSlot(int channel, int controllerNumber) const noexcept
        {
            if (channel < 1 || channel > 16 || ! juce::isPositiveAndBelow(controllerNumber, 128))
                return -1;

            return parameterSlots[(size_t)(channel - 1) * 128u + (size_t)controllerNumber];
        }
    };

    struct UserPreset
    {
        juce::String name;
        juce::ValueTree state;
    };

    struct ProcessingBlockState
    {
        float seasonMacro = 0.34f;
        float sourceDepth = 0.5f;
        float sourceAnchor = 0.5f;
        float flowRate = 0.5f;
        float flowEnergy = 0.5f;
        float flowTexture = 0.5f;
        float pulseWidth = 0.5f;
        float fieldComplexity = 0.65f;
        float radianceAmount = 0.0f;
        float chargeAmount = 0.0f;
        float dischargeAmount = 0.0f;
    };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleIncomingMidiMessages(juce::MidiBuffer& midiMessages);
    void addOrReplaceMidiBinding(int channel, int controllerNumber, const juce::String& paramID);
    
    void appendSequencerState(juce::ValueTree& state) const;
    void restoreSequencerState(const juce::ValueTree& state);
    void appendPromptMemoryState(juce::ValueTree& state) const;
    void restorePromptMemoryState(const juce::ValueTree& state);
    void appendNodeSystemState(juce::ValueTree& state) const;
    void restoreNodeSystemState(const juce::ValueTree& state);
    void rememberPromptMemoryEntry(juce::ValueTree entry, float reinforcement);
    void appendCustomState(juce::ValueTree& state) const;
    void restoreCustomState(const juce::ValueTree& state);
    
    juce::ValueTree captureStateForUserPreset();
    
    void recordMidiActivity(const juce::MidiMessage& message, bool wasMapped) noexcept;
    void resetMidiExpressionState() noexcept;
    void handleMidiExpressionController(int channel, int controllerNumber, int controllerValue) noexcept;
    void applyPitchbendRangeFromRpn(int channel) noexcept;
    void applyMpeZoneLayoutFromRpn(int masterChannel, int numMemberChannels) noexcept;
    
    void hiResTimerCallback() override;
    
    juce::String getMappedParameterIDForSlot(int parameterSlot) const;
    int getMappedParameterSlotForID(const juce::String& paramID) const noexcept;
    void rebuildMidiBindingSnapshot();
    
    void clearPendingMidiLearnBinding() noexcept;
    void queuePendingMidiLearnBinding(int channel, int controllerNumber, int parameterSlot) noexcept;
    void applyPendingMidiLearnBinding();
    
    void queueMappedParameterChange(int parameterSlot, float normalizedValue) noexcept;
    void flushPendingMappedParameterChanges();
    void clearPendingMappedParameterChanges();
    
    void pushNextSampleIntoFifo(float sample) noexcept;
    void pushNextFFTBlockIntoQueue() noexcept;
    
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    
    void requestProcessingStateReset() noexcept;
    void performPendingProcessingStateReset() noexcept;
    
    MutationMode getCurrentMutationMode() const noexcept;
    void generateSmartSequencerPattern(MutationMode mode);
    void markSequencerStateDirty() noexcept;
    void resetPromptRhythmState() noexcept;

    // --- Modular Helpers ---
    void prepareOversampling(int bs);
    void prepareSynth(double sampleRate, int blockSize);
    void prepareEffects(double sampleRate, int blockSize, int numChannels);
    void resetRuntimeState();

    bool handleOversamplingChangeIfNeeded(juce::AudioBuffer<float>& buffer);
    void applyPendingOversamplingReconfigure();
    void clearUnusedOutputChannels(juce::AudioBuffer<float>& buffer);
    void renderSynthAndVoices(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    void applyGlobalFx(juce::AudioBuffer<float>& buffer);
    void applyOutputSafety(juce::AudioBuffer<float>& buffer);
    void updateMetersAndFft(const juce::AudioBuffer<float>& buffer);

    // --- State & Atomic Sync ---
    std::atomic<int> currentPresetDirty { 0 };
    std::atomic<int> presetLoadInProgress { 0 };
    std::atomic<int> processingStateResetPending { 0 };
    std::atomic<int> oversamplingReconfigurePending { 0 };
    std::atomic<int> mutationMode { (int)MutationMode::balanced };
    std::atomic<int> growLockMask { 0 };
    std::atomic<float> lastSystemEnergy { 0.0f };
    std::atomic<float> rmsLevel { 0.0f };
    std::atomic<float> currentOutputPeak { 0.0f };
    juce::Random systemRng;

    bool isRealRootFlowPreset() const noexcept;

    // --- System Logic ---
    void updateSystemMetrics();
    RootFlowSystemFeedbackSnapshot currentSystemFeedback;
    
    ProcessingBlockState currentProcessingBlockState;
    std::atomic<int> currentFactoryPresetIndex { 0 };
    std::atomic<int> currentUserPresetIndex { -1 };
    
    mutable juce::CriticalSection promptStateLock;
    juce::String lastPromptSummary { "Awaiting Seed" };
    juce::String lastPromptSeed;
    bool lastPromptSeedCanReinforce = false;
    
    mutable juce::CriticalSection promptMemoryLock;
    std::vector<juce::ValueTree> promptMemoryEntries;
    RootFlow::PromptRhythmProfile promptRhythmProfile;

    // --- Midi & Control ---
    std::array<int, 16> midiRpnMsb {};
    std::array<int, 16> midiRpnLsb {};
    std::array<int, 16> midiDataEntryMsb {};
    std::array<int, 16> midiDataEntryLsb {};
    
    mutable juce::SpinLock midiMappingLock;
    mutable juce::CriticalSection presetStateLock;
    std::vector<MidiBinding> midiBindings;
    std::vector<UserPreset> userPresets;
    std::shared_ptr<const MidiBindingLookupSnapshot> midiBindingSnapshot;
    
    std::atomic<int> midiMappingMode { (int)MidiMappingMode::defaultMappings };
    std::atomic<int> midiLearnArmed { 0 };
    std::atomic<int> midiLearnTargetParameterSlot { -1 };
    std::atomic<int> pendingMidiLearnChannel { 1 };
    std::atomic<int> pendingMidiLearnControllerNumber { -1 };
    std::atomic<int> pendingMidiLearnParameterSlot { -1 };
    std::atomic<int> pendingMidiLearnDirty { 0 };
    
    std::atomic<uint32_t> midiEventCounter { 0 };
    std::atomic<int> lastMidiActivityType { (int)MidiActivitySnapshot::Type::none };
    std::atomic<int> lastMidiControllerNumber { -1 };
    std::atomic<int> lastMidiNoteNumber { -1 };
    std::atomic<int> lastMidiValue { -1 };
    std::atomic<int> lastMidiBendCents { 0 };
    std::atomic<int> lastMidiChannel { 1 };
    std::atomic<int> lastMidiWasMapped { 0 };
    
    juce::CriticalSection mappedParameterAutomationLock;
    std::array<juce::RangedAudioParameter*, mappedParameterSlotCount> mappedParameters {};
    std::array<std::atomic<float>, mappedParameterSlotCount> pendingMappedParameterValues {};
    std::array<std::atomic<int>, mappedParameterSlotCount> pendingMappedParameterDirty {};
    std::array<bool, mappedParameterSlotCount> mappedParameterGestureActive {};
    std::array<uint32_t, mappedParameterSlotCount> mappedParameterLastUpdateMs {};

    // --- DSP Components ---
    RootFlowModulationEngine modulation;
    RootFlowDSP::CoreResonator resonance;
    RootFlowDSP::FieldDelay field;
    RootFlowDSP::RadianceFinisher radiance;

    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> masterToneFilters;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> monoMakerFilters;
    
    juce::SmoothedValue<float> masterDriveSmoothed;
    juce::SmoothedValue<float> masterToneCutoffSmoothed;
    juce::SmoothedValue<float> masterCrossfeedSmoothed;
    juce::SmoothedValue<float> soilDriveSmoothed;
    juce::SmoothedValue<float> soilMixSmoothed;
    juce::SmoothedValue<float> soilBodyCutoffSmoothed;
    juce::SmoothedValue<float> soilToneCutoffSmoothed;
    juce::SmoothedValue<float> testToneLevelSmoothed;
    juce::SmoothedValue<float> masterMixSmoothed;
    juce::SmoothedValue<float> monoMakerFreqSmoothed;
    
    std::atomic<bool> monoMakerEnabled { false };

    mutable juce::CriticalSection processingLock;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int currentOversamplingFactor = 0;

    juce::dsp::Compressor<float> finalCompressor;
    juce::LinearSmoothedValue<float> masterVolumeSmoothed;

    std::array<float, 2> soilBodyState {};
    std::array<float, 2> soilToneState {};

    std::atomic<int> testToneEnabled { 0 };
    double testTonePhase = 0.0;
    int outputSilenceWatchdogBlocks = 0;
    int outputRunawayWatchdogBlocks = 0;
    int postFxSafetyBypassBlocksRemaining = 0;
    
    std::array<float, 2> postFxDcBlockPrevInput {};
    std::array<float, 2> postFxDcBlockPrevOutput {};
    std::array<float, 2> outputDcBlockPrevInput {};
    std::array<float, 2> outputDcBlockPrevOutput {};

    // --- FFT & Visualization ---
    juce::dsp::FFT forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> fifo {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> spectrum {};
    std::array<float, fftSize / 2> scopeData {};
    std::atomic<bool> fftReady { false };
    std::array<std::array<float, fftSize / 2>, fftQueueCapacity> fftBlockQueue {};
    juce::AbstractFifo fftBlockFifo { fftQueueCapacity };

    // --- Sequencer ---
public:
    std::array<SequencerStep, 16> getSequencerStepSnapshot() const;
    int getCurrentSequencerStep() const noexcept { return currentSequencerStep.load(std::memory_order_relaxed); }
    void clearSequencerSteps();
    void randomizeSequencerStepVelocity(int stepIndex);
    void cycleSequencerStepState(int stepIndex);
    void toggleSequencerStepActive(int stepIndex);
    void adjustSequencerStepVelocity(int stepIndex, float delta);
    void adjustSequencerStepProbability(int stepIndex, float delta);
    void randomizeSequencerStepProbability(int stepIndex);
    void previewSequencerStep(int stepIndex);
    void updateSequencer(int numSamples, juce::MidiBuffer& midiMessages);
    void resetSequencer();

    std::array<SequencerStep, 16> sequencerSteps;
    std::atomic<int> currentSequencerStep { 0 };

private:
    mutable juce::SpinLock sequencerStateLock;
    double samplesPerStep = 0.0;
    double sampleCounter = 0.0;
    int lastSequencerNote = -1;
    bool sequencerNoteActive = false;
    bool sequencerWasEnabled = false;
    double sequencerGateSamples = 0.0;

    juce::MidiKeyboardState keyboardState;
    std::vector<int> heldMidiNotes;
    juce::CriticalSection noteLock;
    int currentArpIndex = 0;

    int fifoIndex = 0;

    NodeSystem nodeSystem;

    juce::Synthesiser synth;
    RootFlowDSP::MidiExpressionState midiExpressionState;
    juce::AudioBuffer<float> drySafetyBuffer;
    
    int currentPromptPatternArchetype = 0;
    float currentPromptPatternDensityBias = 0.0f;
    float currentPromptPatternAnchorBias = 0.0f;
    float currentPromptPatternOffbeatBias = 0.0f;
    float currentPromptPatternTripletBias = 0.0f;
    float currentPromptPatternSwingAmount = 0.0f;
    float currentPromptPatternHumanize = 0.0f;
    float currentPromptPatternTightness = 0.6f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RootFlowAudioProcessor)
};
