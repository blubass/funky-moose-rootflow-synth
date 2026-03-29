
#pragma once

#include "JuceHeader.h"
#include "RootFlowLookAndFeel.h"
#include "UI/MainLayoutComponent.h"

class RootFlowAudioProcessor;

class RootFlowAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit RootFlowAudioProcessorEditor(RootFlowAudioProcessor&);
    ~RootFlowAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

private:
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
    juce::TextButton presetSaveButton;
    juce::TextButton presetDeleteButton;
    juce::TextButton midiLearnButton;
    juce::TextButton testToneButton;
    juce::TextButton hoverToggleButton;
    juce::TextButton idleToggleButton;
    juce::TextButton popupToggleButton;
    juce::TextButton mutateButton;
    juce::ComboBox waveSelector;
    juce::MidiKeyboardComponent keyboardDrawer;
    std::unique_ptr<MainLayoutComponent> mainLayout;
    RootFlowLookAndFeel look;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<Attachment> rootDepthAtt;
    std::unique_ptr<Attachment> rootSoilAtt;
    std::unique_ptr<Attachment> rootAnchorAtt;
    std::unique_ptr<Attachment> sapFlowAtt;
    std::unique_ptr<Attachment> sapVitalityAtt;
    std::unique_ptr<Attachment> sapTextureAtt;
    std::unique_ptr<Attachment> pulseRateAtt;
    std::unique_ptr<Attachment> pulseBreathAtt;
    std::unique_ptr<Attachment> pulseGrowthAtt;
    std::unique_ptr<Attachment> canopyAtt;
    std::unique_ptr<Attachment> instabilityAtt;
    std::unique_ptr<Attachment> atmosAtt;
    std::unique_ptr<Attachment> seasonsAtt;
    std::unique_ptr<Attachment> bloomAtt;
    std::unique_ptr<Attachment> rainAtt;
    std::unique_ptr<Attachment> sunAtt;
    std::unique_ptr<ComboAttachment> waveAttachment;

    bool headerControlStateInitialised = false;
    int cachedPresetItemCount = -1;
    juce::Component* headerFocusedControl = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RootFlowAudioProcessorEditor)
};
