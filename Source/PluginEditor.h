
#pragma once
#include <array>
#include <limits>
#include "JuceHeader.h"

class RootFlowAudioProcessor;
#include "PlantEnergyVisualizer.h"
#include "RootFlowLookAndFeel.h"

class RootFlowAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    RootFlowAudioProcessorEditor(RootFlowAudioProcessor&);
    ~RootFlowAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    void drawWoodFrame(juce::Graphics& g, juce::Rectangle<int> area);
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> area);
    void drawSequencerPanel(juce::Graphics& g, juce::Rectangle<int> area);
    void drawMainPanel(juce::Graphics& g, juce::Rectangle<int> area);
    void drawSectionPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title);
    void drawCanopyPod(juce::Graphics& g, juce::Rectangle<int> area);
    void drawInstabilityPod(juce::Graphics& g, juce::Rectangle<int> area);
    void drawFxModule(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title);
    void drawAtmosPod(juce::Graphics& g, juce::Rectangle<int> area);
    void drawSeasonPod(juce::Graphics& g, juce::Rectangle<int> area);
    void drawVisualizerHousing(juce::Graphics& g, juce::Rectangle<int> area);
    void drawKeyboardDrawer(juce::Graphics& g, juce::Rectangle<int> area);
    void drawAmbientPulse(juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius);
    void drawEnergyFlowOverlay(juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius);
    void drawTexturedWoodPanel(juce::Graphics& g,
                               juce::Rectangle<float> area,
                               float cornerRadius,
                               juce::Colour topTint,
                               juce::Colour bottomTint,
                               float textureAlpha,
                               float grainAlpha,
                               float vignetteAlpha);
    void drawProceduralWoodPanel(juce::Graphics& g,
                                 juce::Rectangle<float> area,
                                 float cornerRadius,
                                 juce::Colour baseColour,
                                 juce::Colour grainColour,
                                 float grainAlpha);
    void drawFxIcon(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& type);
    void drawScrew(juce::Graphics& g, juce::Point<float> centre, float radius);
    void drawStatusLight(juce::Graphics& g, juce::Point<float> centre, juce::Colour colour, float radius);
    void drawValueReadout(juce::Graphics& g, float value, juce::Rectangle<int> area,
                          const juce::String& suffix, int decimals = 0,
                          juce::Colour coreColour = juce::Colour(188, 255, 205).withAlpha(0.94f),
                          juce::Colour glowColour = juce::Colour(118, 255, 142).withAlpha(0.18f));
    void configureStandaloneWindow();
    void updateAnimationTimerState();
    void updateVirtualKeyboardState();
    void releaseVirtualKeyboardNotes();
    void updateKeyboardDrawerAnimation();
    void toggleKeyboardDrawer();
    bool isKeyboardDrawerAvailable() const noexcept;
    bool isVirtualKeyboardInputEnabled() const noexcept;
    void refreshHeaderControlState();
    void setMidiStatusMessage(const juce::String& text, juce::Colour colour);

    void setupKnob(juce::Slider& s, juce::Label& l, const juce::String& labelText, bool showVal = false);
    void setupLabel(juce::Label& l, bool primary = true);

    RootFlowAudioProcessor& audioProcessor;
    juce::ComboBox presetBox;
    juce::TextButton presetSaveButton;
    juce::TextButton presetDeleteButton;
    juce::TextButton midiLearnButton;
    juce::TextButton mpkDefaultButton;
    juce::TextButton midiClearButton;
    juce::TextButton testToneButton;
    juce::MidiKeyboardComponent keyboardDrawer;
    juce::TextButton keyboardDrawerButton;
    juce::TextButton mutateButton;
    juce::ComboBox waveSelector;
    PlantEnergyVisualizer visualizer;
    RootFlowLookAndFeel look;
    void loadAssets();
    
    juce::Image woodBackground;
    juce::Image rootsBackground;
    juce::Image rootsOverlaySoft;
    juce::Image rootsOverlayDetail;
    juce::Image rootsOverlayDetailBlurred;
    juce::Image mooseLogo;

    
    juce::Rectangle<int> headerBoardRect, sequencerBoardRect, mainFieldRect, rootRect, sapRect, pulseRect, canopyRect, visualizerRect, visualizerFrameRect, keyboardDrawerRect;
    juce::Rectangle<int> bloomRect, rainRect, sunRect, innerRect;
    juce::Rectangle<int> atmosRect;
    juce::Rectangle<int> seasonsRect;
    juce::Rectangle<int> instabilityRect;
    juce::Rectangle<int> sequencerRect;

    // ROOT Section
    juce::Slider rootDepthSlider, rootSoilSlider, rootAnchorSlider;
    juce::Label rootDepthLabel, rootSoilLabel, rootAnchorLabel;

    // SAP Section
    juce::Slider sapFlowSlider, sapVitalitySlider, sapTextureSlider;
    juce::Label sapFlowLabel, sapVitalityLabel, sapTextureLabel;

    // PULSE Section
    juce::Slider pulseRateSlider, pulseBreathSlider, pulseGrowthSlider;
    juce::Label pulseRateLabel, pulseBreathLabel, pulseGrowthLabel;

    // CANOPY Section
    juce::Slider canopySlider;
    juce::Label canopyLabel;

    juce::Slider instabilitySlider;
    juce::Label instabilityLabel;

    juce::Slider atmosSlider;
    juce::Label atmosLabel;

    juce::Slider seasonsSlider;
    juce::Label seasonsLabel;

    // Effect Modules
    juce::Slider bloomSlider, rainSlider, sunSlider;
    juce::Label bloomLabel, rainLabel, sunLabel;


    juce::Label titleLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<Attachment> rootDepthAtt, rootSoilAtt, rootAnchorAtt,
                               sapFlowAtt, sapVitalityAtt, sapTextureAtt,
                               pulseRateAtt, pulseBreathAtt, pulseGrowthAtt,
                               canopyAtt, atmosAtt, seasonsAtt,
                               bloomAtt, rainAtt, sunAtt,
                               instabilityAtt;
    
    std::unique_ptr<ButtonAttachment> seqOnAtt;
    std::unique_ptr<ComboAttachment> seqRateAtt, waveAttachment;

    juce::TextButton seqToggle;
    juce::ComboBox seqRateSelector;
    juce::ComboBox seqStepsSelector;

    bool standaloneWindowConfigured = false;
    float uiPulsePhase = 0.0f;
    float rootsDriftPhase = 0.0f;
    float energyFlowPhase = 0.0f;
    float smoothedUiEnergy = 0.0f;
    float keyboardDrawerOpenAmount = 0.0f;
    float keyboardDrawerTarget = 0.0f;
    float midiIndicatorLevel = 0.0f;
    bool headerControlStateInitialised = false;
    bool cachedMidiLearnArmed = false;
    bool cachedTestToneEnabled = false;
    bool cachedPresetDirty = false;
    int cachedPresetItemCount = -1;
    int cachedPresetMenuIndex = std::numeric_limits<int>::min();
    int cachedUserPresetIndex = std::numeric_limits<int>::min();
    uint32_t lastSeenMidiEventCounter = 0;
    juce::String midiStatusText { "READY" };
    juce::Colour midiStatusColour { juce::Colour(154, 236, 178) };
    static constexpr int virtualKeyboardBaseMidiNote = 48;
    std::array<bool, 13> virtualKeyDownStates {};

    juce::TextButton vizModeButton, vizColorButton;


};
