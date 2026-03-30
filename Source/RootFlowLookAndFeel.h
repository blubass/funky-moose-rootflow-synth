#pragma once
#include <JuceHeader.h>

class RootFlowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RootFlowLookAndFeel();

    // SLIDER
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle, juce::Slider&) override;

    // BACKGROUND
    void fillResizableWindowBackground(juce::Graphics&, int w, int h,
                                       const juce::BorderSize<int>&, juce::ResizableWindow&) override;

    // LABEL
    void drawLabel(juce::Graphics&, juce::Label&) override;

    // BUTTON
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour&, bool isHovered, bool isPressed) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool isHovered, bool isPressed) override;

    // PANEL HELPER (Global Static)
    static void drawPanel(juce::Graphics&, juce::Rectangle<float>);

    // COMBO BOX
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    // POPUP MENU
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColourToUse) override;

    // Global Accessor for Design Tokens (Colors)
    static juce::Colour getBgColor()         { return juce::Colour(0xff081521); }
    static juce::Colour getPanelColor()      { return juce::Colour(0xff15314a); }
    static juce::Colour getAccentColor()     { return juce::Colour(0xff74f7ff); }
    static juce::Colour getAccentSoftColor() { return juce::Colour(0xff93ff8b); }
    static juce::Colour getTextColor()       { return juce::Colour(0xffd7f3ff); }

private:
    juce::Colour bg, panel, accent, accentSoft, text;
};
