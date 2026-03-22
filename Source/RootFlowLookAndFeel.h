#pragma once
#include <JuceHeader.h>

class RootFlowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RootFlowLookAndFeel()
    {
        setColour(juce::Slider::thumbColourId, juce::Colour(0xff22ff66));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff22ff66));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(20, 35, 20));
        
        // ComboBox & Popup Base Colors
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(15, 18, 15));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xff22ff66));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff22ff66).withAlpha(0.3f));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff22ff66).withAlpha(0.7f));
        
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(15, 18, 15));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xff22ff66));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff22ff66).withAlpha(0.15f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    void setWoodImage(const juce::Image& img) { woodImage = img; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, 
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto lineW = 2.8f; 
        auto arcRadius = radius - lineW * 2.8f;
        auto center = bounds.getCentre();

        g.setColour(juce::Colour(0xff22ff66).withAlpha(0.04f));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

        if (slider.isEnabled())
        {
            auto fill = slider.findColour(juce::Slider::rotarySliderFillColourId);
            juce::Path valueArc;
            valueArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 
                                   0.0f, rotaryStartAngle, toAngle, true);
            
            for (float i = 1.0f; i <= 3.0f; ++i) {
                g.setColour(fill.withAlpha(0.12f / i));
                g.strokePath(valueArc, juce::PathStrokeType(lineW + i * 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour(fill.withAlpha(0.9f));
            g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        auto knobRadius = radius * 0.76f;
        auto knobRect = juce::Rectangle<float>(knobRadius * 2.0f, knobRadius * 2.0f).withCentre(center);
        
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillEllipse(knobRect.translated(0, 4));

        juce::ColourGradient rimGrad(juce::Colour(60, 65, 60), knobRect.getTopLeft(),
                                     juce::Colour(15, 18, 15), knobRect.getBottomRight(), false);
        g.setGradientFill(rimGrad);
        g.fillEllipse(knobRect);
        
        auto woodRect = knobRect.reduced(1.8f);
        juce::ColourGradient woodGrad(juce::Colour(68, 45, 30), woodRect.getTopLeft(),
                                      juce::Colour(22, 14, 10), woodRect.getBottomRight(), false);
        g.setGradientFill(woodGrad);
        g.fillEllipse(woodRect);

        juce::Point<float> thumbPoint(center.x + arcRadius * std::cos(toAngle - juce::MathConstants<float>::halfPi),
                                     center.y + arcRadius * std::sin(toAngle - juce::MathConstants<float>::halfPi));

        auto dotRect = juce::Rectangle<float>(5.0f, 5.0f).withCentre(thumbPoint);
        auto fill = slider.findColour(juce::Slider::rotarySliderFillColourId);
        
        g.setColour(fill.withAlpha(0.8f));
        g.fillEllipse(dotRect.expanded(2.0f));
        g.setColour(juce::Colours::white);
        g.fillEllipse(dotRect.reduced(0.5f));

        auto screwRect = juce::Rectangle<float>(knobRadius * 0.28f, knobRadius * 0.28f).withCentre(center);
        g.setColour(juce::Colour(95, 100, 105));
        g.fillEllipse(screwRect);
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRect(center.x - screwRect.getWidth() * 0.35f, center.y - 0.5f, screwRect.getWidth() * 0.7f, 1.0f);

        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(toAngle, center.x, center.y));
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.fillRect(center.x - 0.8f, center.y - knobRadius, 1.6f, knobRadius * 0.35f);
        g.restoreState();
    }

    // --- NEW: ComboBox Design ---
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, width, height).reduced(1.0f);
        
        juce::ColourGradient grad(juce::Colour(25, 28, 25), bounds.getTopLeft(),
                                  juce::Colour(10, 12, 10), bounds.getBottomRight(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        juce::Path arrow;
        arrow.addTriangle(buttonX + buttonW * 0.3f, buttonY + buttonH * 0.4f,
                          buttonX + buttonW * 0.7f, buttonY + buttonH * 0.4f,
                          buttonX + buttonW * 0.5f, buttonY + buttonH * 0.6f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.fillPath(arrow);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(juce::Colour(15, 18, 15));
        g.setColour(juce::Colour(0xff22ff66).withAlpha(0.4f));
        g.drawRect(0, 0, width, height, 1);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            const bool isSeparator, const bool isActive,
                            const bool isHighlighted, const bool isTicked,
                            const bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* const textColourToUse) override
    {
        if (isHighlighted) {
            g.fillAll(juce::Colour(0xff22ff66).withAlpha(0.15f));
        }

        g.setColour(isHighlighted ? juce::Colours::white : juce::Colour(0xff22ff66).withAlpha(0.8f));
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(text, area.reduced(10, 0), juce::Justification::centred, true);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        auto baseColor = backgroundColour;
        
        if (shouldDrawButtonAsDown)
            baseColor = baseColor.darker(0.4f);
        else if (shouldDrawButtonAsHighlighted)
            baseColor = baseColor.brighter(0.2f);

        g.setColour(baseColor);
        g.fillRoundedRectangle(bounds, 4.0f);

        // Bright Neon Outline
        g.setColour(juce::Colour(0xff22ff66).withAlpha(0.4f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.4f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override
    {
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.setColour(button.findColour(juce::TextButton::textColourOffId));
        auto area = button.getLocalBounds();
        g.drawText(button.getButtonText(), area, juce::Justification::centred, true);
    }

private:
    juce::Image woodImage;
};
