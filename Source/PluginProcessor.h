
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

class RootFlowAudioProcessor : public juce::AudioProcessor,
                               private juce::HighResolutionTimer,
                               private juce::AudioProcessorValueTreeState::Listener
{
public:
    RootFlowAudioProcessor();
    ~RootFlowAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RootFlow"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    bool supportsMPE() const override { return true; }
    double getTailLengthSeconds() const override { return 1.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

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

    float getPlantEnergy() const;
    float getPlantEnergyValue() const noexcept { return getPlantEnergy(); }
    float getOutputPeak() const noexcept { return currentOutputPeak.load(std::memory_order_relaxed); }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    juce::StringArray getFactoryPresetNames() const;
    std::vector<PresetMenuSection> getFactoryPresetMenuSections() const;
    int getFactoryPresetCount() const noexcept;
    juce::StringArray getCombinedPresetNames() const;
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

    // Spectrum Analysis for Visualizer
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    bool getNextFFTBlock(float* destData);

    juce::AudioProcessorValueTreeState tree;

private:
    static constexpr int fftQueueCapacity = 8;
    static constexpr int mappedParameterSlotCount = 16;
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
            if (! juce::isPositiveAndBelow(channel, 17) || ! juce::isPositiveAndBelow(controllerNumber, 128))
                return -1;

            return parameterSlots[(size_t) (channel - 1) * 128u + (size_t) controllerNumber];
        }
    };

    struct UserPreset
    {
        juce::String name;
        juce::ValueTree state;
    };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleIncomingMidiMessages(juce::MidiBuffer& midiMessages);
    void addOrReplaceMidiBinding(int channel, int controllerNumber, const juce::String& paramID);
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

    std::atomic<float> lastPlantEnergy { 0.0f };
    std::atomic<float> currentOutputPeak { 0.0f };
    RootFlowBioFeedbackSnapshot currentBioFeedback;
    std::atomic<int> currentFactoryPresetIndex { 0 };
    std::atomic<int> currentUserPresetIndex { -1 };
    std::atomic<int> currentPresetDirty { 0 };
    std::atomic<int> presetLoadInProgress { 0 };
    std::atomic<int> processingStateResetPending { 0 };

    juce::MidiKeyboardState keyboardState;
    juce::Synthesiser synth;
    juce::AudioBuffer<float> drySafetyBuffer;
    RootFlowVoice::MidiExpressionState midiExpressionState;
    std::array<int, 16> midiRpnMsb {};
    std::array<int, 16> midiRpnLsb {};
    std::array<int, 16> midiDataEntryMsb {};
    std::array<int, 16> midiDataEntryLsb {};
    mutable juce::SpinLock midiMappingLock;
    mutable juce::CriticalSection presetStateLock;
    std::vector<MidiBinding> midiBindings;
    std::vector<UserPreset> userPresets;
    std::shared_ptr<const MidiBindingLookupSnapshot> midiBindingSnapshot;
    std::atomic<int> midiMappingMode { (int) MidiMappingMode::defaultMappings };
    std::atomic<int> midiLearnArmed { 0 };
    std::atomic<int> midiLearnTargetParameterSlot { -1 };
    std::atomic<int> pendingMidiLearnChannel { 1 };
    std::atomic<int> pendingMidiLearnControllerNumber { -1 };
    std::atomic<int> pendingMidiLearnParameterSlot { -1 };
    std::atomic<int> pendingMidiLearnDirty { 0 };
    std::atomic<uint32_t> midiEventCounter { 0 };
    std::atomic<int> lastMidiActivityType { (int) MidiActivitySnapshot::Type::none };
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
    RootFlowModulationEngine modulation;
    RootFlowDSP::BloomProcessor bloom;
    RootFlowDSP::RainProcessor rain;
    RootFlowDSP::SunProcessor sun;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> masterToneFilters;
    juce::SmoothedValue<float> masterDriveSmoothed;
    juce::SmoothedValue<float> masterToneCutoffSmoothed;
    juce::SmoothedValue<float> masterCrossfeedSmoothed;
    juce::SmoothedValue<float> soilDriveSmoothed;
    juce::SmoothedValue<float> soilMixSmoothed;
    juce::SmoothedValue<float> soilBodyCutoffSmoothed;
    juce::SmoothedValue<float> soilToneCutoffSmoothed;
    juce::SmoothedValue<float> testToneLevelSmoothed;
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

    // FFT members
    juce::dsp::FFT forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> fifo {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> scopeData {};
    std::array<std::array<float, fftSize / 2>, fftQueueCapacity> fftBlockQueue {};
    juce::AbstractFifo fftBlockFifo { fftQueueCapacity };
    int fifoIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RootFlowAudioProcessor)
};
