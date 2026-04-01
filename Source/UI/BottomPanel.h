#pragma once
#include <JuceHeader.h>
#include "Utils/DesignTokens.h"

class BottomPanel : public juce::Component
{
public:
    BottomPanel()
    {
        bloom.setName("SOIL");
        rain.setName("AIR");
        sun.setName("SPACE");

        setupSlider(bloom);
        setupSlider(rain);
        setupSlider(sun);
        wireSlider(bloom);
        wireSlider(rain);
        wireSlider(sun);

        addAndMakeVisible(bloom);
        addAndMakeVisible(rain);
        addAndMakeVisible(sun);

        setOpaque(false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(28, 18);
        int w = area.getWidth() / 3;

        auto bloomLane = area.removeFromLeft(w).reduced(10, 0);
        auto rainLane = area.removeFromLeft(w).reduced(10, 0);
        auto sunLane = area.reduced(10, 0);

        bloom.setBounds(bloomLane.withTrimmedTop(38).withHeight(24));
        rain.setBounds(rainLane.withTrimmedTop(38).withHeight(24));
        sun.setBounds(sunLane.withTrimmedTop(38).withHeight(24));
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(4.0f);
        RootFlow::drawGlassPanel(g, area, 28.0f, 0.68f);

        g.setFont(RootFlow::getFont(11.2f).boldened());
        g.setColour(RootFlow::text.withAlpha(0.82f));
        g.drawText("AMBIENT FIELD", area.withHeight(18.0f).translated(0.0f, 4.0f), juce::Justification::centred, false);

        for (auto* slider : { &bloom, &rain, &sun })
        {
            auto well = slider->getBounds().toFloat().expanded(12.0f, 18.0f).translated(0.0f, -2.0f);
            auto tint = slider == &sun ? RootFlow::amber : (slider == &rain ? RootFlow::accent : RootFlow::accentSoft);
            const bool focused = RootFlow::isInteractiveHoverActive(*slider);

            g.setColour(juce::Colours::black.withAlpha(0.14f));
            g.fillRoundedRectangle(well, 22.0f);
            g.setColour(tint.withAlpha(focused ? 0.06f : 0.032f));
            g.fillRoundedRectangle(well.reduced(1.0f), 21.0f);
            g.setColour(tint.withAlpha(focused ? 0.14f : 0.045f));
            g.drawRoundedRectangle(well, 22.0f, focused ? 1.0f : 0.9f);

            if (focused)
            {
                g.setColour(tint.withAlpha(0.07f));
                g.drawRoundedRectangle(well.expanded(2.0f, 1.0f), 23.0f, 0.9f);
            }

            auto capsule = juce::Rectangle<float>(well.getX() + 16.0f, well.getY() + 6.0f, well.getWidth() - 32.0f, 18.0f);
            const auto capsuleText = slider == &bloom ? juce::String("GROUND TONE")
                                   : (slider == &rain ? juce::String("AIR CURRENT")
                                                      : juce::String("SOLAR FIELD"));
            drawFieldCapsule(g, capsule, capsuleText, tint);
        }

        RootFlow::drawBioThread(g, bloom.getBounds().toFloat().getCentre().translated(18.0f, 8.0f),
                                rain.getBounds().toFloat().getCentre().translated(-18.0f, 8.0f), RootFlow::accentSoft, 0.048f, 0.85f);
        RootFlow::drawBioThread(g, rain.getBounds().toFloat().getCentre().translated(18.0f, 8.0f),
                                sun.getBounds().toFloat().getCentre().translated(-18.0f, 8.0f), RootFlow::accent, 0.048f, 0.85f);

        drawSliderLabel(g, bloom, "SOIL");
        drawSliderLabel(g, rain, "AIR");
        drawSliderLabel(g, sun, "SPACE");
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        if (! RootFlow::arePopupOverlaysEnabled())
            return;

        if (auto* slider = getFocusedSlider())
            drawFocusBubble(g, *slider);
    }

    juce::Slider& getBloomSlider() { return bloom; }
    juce::Slider& getRainSlider() { return rain; }
    juce::Slider& getSunSlider() { return sun; }
    juce::Slider* getFocusedSlider() const { return findFocusedSlider(); }
    juce::Colour getFocusTint() const
    {
        if (auto* slider = findFocusedSlider())
            return getSliderTint(*slider);

        return RootFlow::accentSoft;
    }

    juce::String getFocusTitle() const
    {
        if (auto* slider = findFocusedSlider())
            return getSliderTitle(*slider);

        return {};
    }

    juce::String getFocusValueText() const
    {
        if (auto* slider = findFocusedSlider())
            return formatPercent(slider->getValue());

        return {};
    }

    juce::Point<float> getFocusAnchor() const
    {
        if (auto* slider = findFocusedSlider())
            return slider->getBounds().toFloat().getCentre().translated((float) getX(), (float) getY());

        return getBounds().toFloat().getCentre();
    }

private:
    static juce::String formatPercent(double value)
    {
        return juce::String(juce::roundToInt(juce::jlimit(0.0, 1.0, value) * 100.0)) + "%";
    }

    void setupSlider(juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.getProperties().set("rootflowStyle", "ambient-horizontal");
    }

    void wireSlider(RootFlow::InteractiveSlider& s)
    {
        s.onInteractionStateChange = [this] { repaint(); };
        s.onValueChange = [this] { repaint(); };
    }

    void drawSliderLabel(juce::Graphics& g, juce::Slider& slider, const juce::String& label) const
    {
        const auto tint = &slider == &sun ? RootFlow::amber
                         : (&slider == &rain ? RootFlow::accent
                                             : RootFlow::accentSoft);

        juce::String combinedText = formatPercent(slider.getValue()) + "  " + label;
        auto infoBounds = slider.getBounds().withY(slider.getBottom() + 3).withHeight(18).toFloat();
        auto chip = infoBounds.reduced(22.0f, 0.0f);

        RootFlow::drawGlassPanel(g, chip, chip.getHeight() * 0.5f, 0.46f);
        g.setColour(tint.withAlpha(0.08f));
        g.fillRoundedRectangle(chip.reduced(2.0f), chip.getHeight() * 0.42f);

        g.setFont(RootFlow::getFont(9.5f).boldened());
        g.setColour(RootFlow::text.withAlpha(0.98f));
        g.drawText(combinedText, infoBounds.toNearestInt(), juce::Justification::centred, false);
    }

    void drawFieldCapsule(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour tint) const
    {
        RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.5f, 0.50f);
        g.setColour(tint.withAlpha(0.08f));
        g.fillRoundedRectangle(area.reduced(2.0f), area.getHeight() * 0.42f);
        g.setFont(RootFlow::getFont(9.4f).boldened());
        g.setColour(RootFlow::text.interpolatedWith(tint, 0.10f).withAlpha(0.92f));
        g.drawFittedText(text, area.toNearestInt().reduced(8, 0), juce::Justification::centred, 1);
    }

    juce::Slider* findFocusedSlider() const
    {
        for (auto* slider : { static_cast<const juce::Slider*>(&bloom), static_cast<const juce::Slider*>(&rain), static_cast<const juce::Slider*>(&sun) })
            if (slider->isMouseButtonDown())
                return const_cast<juce::Slider*>(slider);

        for (auto* slider : { static_cast<const juce::Slider*>(&bloom), static_cast<const juce::Slider*>(&rain), static_cast<const juce::Slider*>(&sun) })
            if (RootFlow::isInteractiveHoverActive(*slider) && ! slider->isMouseButtonDown())
                return const_cast<juce::Slider*>(slider);

        return nullptr;
    }

    juce::Colour getSliderTint(const juce::Slider& slider) const
    {
        if (&slider == &sun)
            return RootFlow::amber;
        if (&slider == &rain)
            return RootFlow::accent;
        return RootFlow::accentSoft;
    }

    juce::String getSliderTitle(const juce::Slider& slider) const
    {
        if (&slider == &sun)
            return "SOLAR FIELD";
        if (&slider == &rain)
            return "AIR CURRENT";
        return "GROUND TONE";
    }

    void drawFocusBubble(juce::Graphics& g, juce::Slider& slider) const
    {
        auto bubble = juce::Rectangle<float>(120.0f, 34.0f)
                          .withCentre({ (float) slider.getBounds().getCentreX(), (float) slider.getY() - 12.0f });

        const auto shell = getLocalBounds().toFloat().reduced(4.0f);
        bubble.setX(juce::jlimit(shell.getX() + 8.0f,
                                 shell.getRight() - bubble.getWidth() - 8.0f,
                                 bubble.getX()));
        bubble.setY(juce::jlimit(shell.getY() + 22.0f,
                                 shell.getBottom() - bubble.getHeight() - 44.0f,
                                 bubble.getY()));

        const auto tint = getSliderTint(slider);
        RootFlow::drawGlassPanel(g, bubble, bubble.getHeight() * 0.44f, slider.isMouseButtonDown() ? 0.92f : 0.72f);
        g.setColour(tint.withAlpha(slider.isMouseButtonDown() ? 0.15f : 0.10f));
        g.fillRoundedRectangle(bubble.reduced(2.0f), bubble.getHeight() * 0.34f);

        auto inner = bubble.reduced(9.0f, 4.0f);
        auto topRow = inner.removeFromTop(8.0f);
        auto mainRow = inner;
        auto valueArea = mainRow.removeFromRight(34.0f);

        g.setFont(RootFlow::getFont(7.4f).boldened());
        g.setColour(RootFlow::textMuted.withAlpha(0.88f));
        g.drawText(slider.isMouseButtonDown() ? "MORPH" : "FOCUS", topRow, juce::Justification::centredLeft, false);

        g.setFont(RootFlow::getFont(9.2f).boldened());
        g.setColour((slider.isMouseButtonDown() ? tint.brighter(0.20f) : RootFlow::text).withAlpha(0.96f));
        g.drawFittedText(getSliderTitle(slider), mainRow.toNearestInt(), juce::Justification::centredLeft, 1);

        g.setFont(RootFlow::getFont(7.8f).boldened());
        g.setColour(tint.withAlpha(0.96f));
        g.drawFittedText(formatPercent(slider.getValue()), valueArea.toNearestInt(), juce::Justification::centredRight, 1);
        RootFlow::drawGlowOrb(g, { bubble.getRight() - 10.0f, bubble.getY() + 9.0f }, 2.7f, tint,
                              slider.isMouseButtonDown() ? 0.84f : 0.54f);
    }

    RootFlow::InteractiveSlider bloom, rain, sun;
};
