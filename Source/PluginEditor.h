
#pragma once

#include <array>

#include "JuceHeader.h"
#include "RootFlowLookAndFeel.h"
#include "UI/MainLayoutComponent.h"
#include "UI/AtmosphericOverlay.h"

class RootFlowAudioProcessor;

class RootFlowAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit RootFlowAudioProcessorEditor(RootFlowAudioProcessor&);
    ~RootFlowAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
    void applyPromptSeed();
    void showPatchMenu();
    void showUtilityMenu();
    void setMidiLearnEnabled(bool enabled);
    void setHoverEffectsEnabled(bool enabled);
    void setIdleEffectsEnabled(bool enabled);
    void setPopupOverlaysEnabled(bool enabled);
    void setToneProbeEnabled(bool enabled);
    void setGrowLockEnabled(RootFlowAudioProcessor::GrowLockGroup group, bool enabled);
    void applyStylePromptRecipe(int recipeIndex);
    void recallSeedMemoryPrompt(int slotIndex);
    void deleteSeedMemoryPrompt(int slotIndex);
    void refreshHeaderControlState();
    void updateAnimationTimerState();
    void registerHeaderControl(juce::Component&);
    void clearHeaderFocusIfNeeded(juce::Component&);
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    RootFlowAudioProcessor& audioProcessor;

    juce::ComboBox presetBox;
    juce::TextButton patchMenuButton;
    juce::TextButton presetSaveButton;
    juce::TextButton presetDeleteButton;
    juce::TextButton midiLearnButton;
    juce::TextButton utilityMenuButton;
    juce::TextButton testToneButton;
    juce::TextButton hoverToggleButton;
    juce::TextButton idleToggleButton;
    juce::TextButton popupToggleButton;
    juce::TextButton mutateModeButton;
    juce::TextButton mutateButton;
    juce::TextButton growLockCoreButton;
    juce::TextButton growLockMotionButton;
    juce::TextButton growLockSpectralButton;
    juce::TextButton growLockFxButton;
    juce::TextButton growLockSeqButton;
    juce::TextEditor promptEditor;
    juce::TextEditor morphPromptEditor;
    juce::Slider promptMorphSlider;
    juce::TextButton promptApplyButton;
    std::array<juce::TextButton, 3> seedMemoryButtons;
    std::array<juce::String, 3> seedMemoryPrompts;
    std::array<juce::String, 3> seedMemorySummaries;
    juce::ComboBox waveSelector;
    juce::MidiKeyboardComponent keyboardDrawer;
    std::unique_ptr<MainLayoutComponent> mainLayout;
    AtmosphericOverlay atmosphericOverlay;
    RootFlowLookAndFeel look;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<Attachment> sourceDepthAtt;
    std::unique_ptr<Attachment> sourceCoreAtt;
    std::unique_ptr<Attachment> sourceAnchorAtt;
    std::unique_ptr<Attachment> flowRateAtt;
    std::unique_ptr<Attachment> flowEnergyAtt;
    std::unique_ptr<Attachment> flowTextureAtt;
    std::unique_ptr<Attachment> pulseFrequencyAtt;
    std::unique_ptr<Attachment> pulseWidthAtt;
    std::unique_ptr<Attachment> pulseEnergyAtt;
    std::unique_ptr<Attachment> fieldComplexityAtt;
    std::unique_ptr<Attachment> instabilityAtt;
    std::unique_ptr<Attachment> fieldDepthAtt;
    std::unique_ptr<Attachment> systemMatrixAtt;
    std::unique_ptr<Attachment> radianceAtt;
    std::unique_ptr<Attachment> chargeAtt;
    std::unique_ptr<Attachment> dischargeAtt;
    std::unique_ptr<Attachment> evolutionAtt;
    std::unique_ptr<ComboAttachment> waveAttachment;

    juce::ComboBox oversamplingSelector;
    std::unique_ptr<ComboAttachment> oversamplingAttachment;

    bool headerControlStateInitialised = false;
    int cachedPresetItemCount = -1;
    juce::Component* headerFocusedControl = nullptr;
    float smoothedMidiVelocity = 0.0f; // for MIDI-reactive data-vapor decay

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RootFlowAudioProcessorEditor)
};
