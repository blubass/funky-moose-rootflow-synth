#pragma once
#include <JuceHeader.h>
#include <vector>
#include "NodeSystem.h"

class RootFlowAudioProcessor;

class EnergyDisplay : public juce::Component, private juce::Timer
{
public:
    EnergyDisplay() { setOpaque(false); }

    void setProcessor(RootFlowAudioProcessor* p);
    void triggerCoreBurst();

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    
    juce::Point<float> getNodePositionRelative(const juce::String& paramID) const;

private:
    void timerCallback() override;

    RootFlowAudioProcessor* processor = nullptr;
    std::vector<float> spectrum;
    float rms = 0.0f;
    float time = 0.0f;
    NodeSystem* nodeSystem = nullptr;
    int selectedNode = -1;
    int hoveredNode = -1;
    int connectionStart = -1;
    int connectionHover = -1;
    int seqFlashNode    = -1;
    float seqFlashTimer = 0.0f;
    float coreBurstTimer = 0.0f;
    float lastBeat      = 0.0f;

    int findNodeAt(juce::Point<float> mousePos);
    bool isNearConnection(juce::Point<float> p, const NodeConnection& c, const std::vector<FlowNode>& nodes) const;
    void drawKeyboardInteraction(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnergyDisplay)
};
