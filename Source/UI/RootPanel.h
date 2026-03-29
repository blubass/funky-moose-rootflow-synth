#pragma once
#include <JuceHeader.h>
#include "Utils/DesignTokens.h"

class RootPanel : public juce::Component
{
public:
    RootPanel()
    {
        depth.setName("DEPTH");
        soil.setName("SOIL");
        anchor.setName("ANCHOR");

        setupVerticalSlider(depth);
        setupVerticalSlider(soil);
        setupHorizontalSlider(anchor);
        wireSlider(depth);
        wireSlider(soil);
        wireSlider(anchor);

        addAndMakeVisible(depth);
        addAndMakeVisible(soil);
        addAndMakeVisible(anchor);
    }

    void resized() override
    {
        auto topChamber = getTopChamberBounds();
        auto bottomChamber = getBottomChamberBounds();

        auto topInner = topChamber.reduced(14, 14);
        const int laneWidth = juce::jlimit(52, 64, juce::roundToInt((float) topInner.getWidth() * 0.34f));

        auto depthLane = juce::Rectangle<int>(topInner.getX() + 4,
                                              topInner.getY() + 2,
                                              laneWidth,
                                              topInner.getHeight() - 8);
        auto soilLane = juce::Rectangle<int>(topInner.getRight() - laneWidth - 4,
                                             topInner.getY() + 14,
                                             laneWidth,
                                             topInner.getHeight() - 20);

        depth.setBounds(depthLane);
        soil.setBounds(soilLane);

        auto anchorLane = bottomChamber.reduced(10, 12);
        anchor.setBounds(anchorLane.withTrimmedTop(20).withTrimmedBottom(18));
    }

    void paint(juce::Graphics& g) override
    {
        auto shell = getShellBounds().toFloat();
        auto topChamber = getTopChamberBounds().toFloat().translated(-6.0f, 0.0f);
        auto bottomChamber = getBottomChamberBounds().toFloat().translated(8.0f, 0.0f);
        const auto strataTint = getDepthTint();
        const auto soilTint = getSoilTint();
        const auto anchorTint = getAnchorTint();
        const auto bridgeTint = soilTint.interpolatedWith(anchorTint, 0.34f);
        const auto shellPath = RootFlow::makeSideColumnShell(shell, false);

        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(shellPath);

            juce::ColourGradient upperWash(strataTint.withAlpha(0.16f), shell.getX() + shell.getWidth() * 0.26f, shell.getY() + shell.getHeight() * 0.12f,
                                           juce::Colours::transparentBlack, shell.getCentreX(), shell.getY() + shell.getHeight() * 0.54f, true);
            g.setGradientFill(upperWash);
            g.fillEllipse(shell.getX() - shell.getWidth() * 0.04f, shell.getY() + shell.getHeight() * 0.02f,
                          shell.getWidth() * 0.82f, shell.getHeight() * 0.42f);

            juce::ColourGradient lowerWash(anchorTint.withAlpha(0.12f), shell.getX() + shell.getWidth() * 0.32f, shell.getBottom() - shell.getHeight() * 0.16f,
                                           juce::Colours::transparentBlack, shell.getCentreX(), shell.getY() + shell.getHeight() * 0.56f, true);
            g.setGradientFill(lowerWash);
            g.fillEllipse(shell.getX() + shell.getWidth() * 0.02f, shell.getBottom() - shell.getHeight() * 0.40f,
                          shell.getWidth() * 0.74f, shell.getHeight() * 0.34f);
        }

        RootFlow::drawSideColumnShell(g, shell, false, strataTint.interpolatedWith(soilTint, 0.22f));
        RootFlow::drawMembraneChamber(g, topChamber, strataTint, false, 1.08f);
        RootFlow::drawMembraneChamber(g, bottomChamber, anchorTint, false, 0.92f);

        drawChamberBadge(g,
                         juce::Rectangle<float>(topChamber.getX() + 18.0f, topChamber.getY() + 10.0f, topChamber.getWidth() - 44.0f, 18.0f),
                         "STRATA CORE",
                         strataTint);
        drawChamberBadge(g,
                         juce::Rectangle<float>(bottomChamber.getX() + 16.0f, bottomChamber.getY() + 10.0f, bottomChamber.getWidth() - 38.0f, 18.0f),
                         "ANCHOR WELL",
                         anchorTint);

        const auto topCore = topChamber.getCentre().translated(-12.0f, -topChamber.getHeight() * 0.24f);
        const auto bottomCore = bottomChamber.getCentre().translated(-6.0f, bottomChamber.getHeight() * 0.18f);
        RootFlow::drawGlowOrb(g, topCore, 11.5f, strataTint, 0.76f);
        RootFlow::drawGlowOrb(g, bottomCore, 10.0f, anchorTint, 0.72f);

        RootFlow::drawBioThread(g, { depth.getBounds().getCentreX() + 10.0f, depth.getBounds().getBottom() - 18.0f }, topCore, strataTint, 0.18f, 0.95f);
        RootFlow::drawBioThread(g, { soil.getBounds().getCentreX() + 10.0f, soil.getBounds().getBottom() - 18.0f }, topCore.translated(18.0f, 20.0f), soilTint, 0.18f, 0.95f);
        RootFlow::drawBioThread(g, { anchor.getBounds().getCentreX() + 10.0f, anchor.getBounds().getBottom() - 18.0f }, bottomCore, anchorTint, 0.20f, 0.95f);
        RootFlow::drawBioThread(g, { topChamber.getRight() - 26.0f, topChamber.getBottom() - 34.0f }, { bottomChamber.getX() + 24.0f, bottomChamber.getY() + 24.0f }, bridgeTint, 0.14f, 1.0f);

        RootFlow::drawOrbSocket(g, { bottomChamber.getX() + 34.0f, bottomChamber.getBottom() - 22.0f }, 9.0f, soilTint, 0.68f);
        RootFlow::drawOrbSocket(g, { bottomChamber.getRight() - 28.0f, bottomChamber.getBottom() - 20.0f }, 8.0f, anchorTint, 0.62f);

        drawSliderLabel(g, depth, "DEPTH");
        drawSliderLabel(g, soil, "SOIL");
        drawSliderLabel(g, anchor, "ANCHOR");
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        if (! RootFlow::arePopupOverlaysEnabled())
            return;

        if (auto* slider = findFocusedSlider())
            drawFocusBubble(g, *slider);
    }

    juce::Slider& getDepthSlider() { return depth; }
    juce::Slider& getSoilSlider() { return soil; }
    juce::Slider& getAnchorSlider() { return anchor; }
    juce::Slider* getFocusedSlider() const { return findFocusedSlider(); }
    juce::Colour getFocusTint() const
    {
        if (auto* slider = findFocusedSlider())
            return getSliderTint(*slider);

        return getDepthTint();
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

    static juce::Colour getDepthTint()
    {
        return RootFlow::violet.interpolatedWith(RootFlow::accentSoft, 0.36f);
    }

    static juce::Colour getSoilTint()
    {
        return RootFlow::accent.interpolatedWith(RootFlow::violet, 0.18f);
    }

    static juce::Colour getAnchorTint()
    {
        return RootFlow::violet.interpolatedWith(RootFlow::accent, 0.14f);
    }

    void setupVerticalSlider(juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity(320);
        s.getProperties().set("rootflowStyle", "side-vertical");
    }

    void setupHorizontalSlider(juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity(320);
        s.getProperties().set("rootflowStyle", "pod-horizontal");
    }

    void wireSlider(RootFlow::InteractiveSlider& s)
    {
        s.onInteractionStateChange = [this] { repaint(); };
        s.onValueChange = [this] { repaint(); };
    }

    juce::Rectangle<int> getShellBounds() const
    {
        return getLocalBounds().reduced(6, 8).withTrimmedTop(18).withTrimmedBottom(6);
    }

    juce::Rectangle<int> getTopChamberBounds() const
    {
        auto shell = getShellBounds();
        return juce::Rectangle<int>(shell.getX() + 8,
                                    shell.getY() + 20,
                                    juce::roundToInt(shell.getWidth() * 0.82f),
                                    juce::roundToInt(shell.getHeight() * 0.52f));
    }

    juce::Rectangle<int> getBottomChamberBounds() const
    {
        auto shell = getShellBounds();
        return juce::Rectangle<int>(shell.getX() + 4,
                                    shell.getBottom() - juce::roundToInt(shell.getHeight() * 0.40f) - 12,
                                    juce::roundToInt(shell.getWidth() * 0.84f),
                                    juce::roundToInt(shell.getHeight() * 0.38f));
    }

    void drawSliderLabel(juce::Graphics& g, juce::Slider& slider, const juce::String& text) const
    {
        const auto tint = getSliderTint(slider);
        const bool horizontalSlider = slider.getSliderStyle() == juce::Slider::LinearHorizontal;

        juce::String combinedText = formatPercent(slider.getValue()) + "  " + text;

        auto infoBounds = juce::Rectangle<float>(horizontalSlider ? 130.0f : 86.0f, 18.0f)
                              .withCentre({ (float) slider.getBounds().getCentreX(), (float) slider.getBottom() + 16.0f });

        auto chip = infoBounds.reduced(horizontalSlider ? 10.0f : 4.0f, 0.0f);
        RootFlow::drawGlassPanel(g, chip, chip.getHeight() * 0.5f, 0.50f);
        g.setColour(tint.withAlpha(0.09f));
        g.fillRoundedRectangle(chip.reduced(2.0f), chip.getHeight() * 0.44f);

        g.setFont(RootFlow::getFont(9.5f).boldened());
        g.setColour(RootFlow::text.withAlpha(0.94f));
        g.drawText(combinedText, infoBounds.toNearestInt(), juce::Justification::centred, false);
    }

    void drawChamberBadge(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour tint) const
    {
        RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.5f, 0.58f);
        g.setColour(tint.withAlpha(0.09f));
        g.fillRoundedRectangle(area.reduced(2.0f), area.getHeight() * 0.42f);
        g.setFont(RootFlow::getFont(9.0f).boldened());
        g.setColour(RootFlow::text.withAlpha(0.78f));
        g.drawText(text, area, juce::Justification::centred, false);
    }

    juce::Slider* findFocusedSlider() const
    {
        for (auto* slider : { static_cast<const juce::Slider*>(&depth), static_cast<const juce::Slider*>(&soil), static_cast<const juce::Slider*>(&anchor) })
            if (slider->isMouseButtonDown())
                return const_cast<juce::Slider*>(slider);

        for (auto* slider : { static_cast<const juce::Slider*>(&depth), static_cast<const juce::Slider*>(&soil), static_cast<const juce::Slider*>(&anchor) })
            if (RootFlow::isInteractiveHoverActive(*slider) && ! slider->isMouseButtonDown())
                return const_cast<juce::Slider*>(slider);

        return nullptr;
    }

    juce::Colour getSliderTint(const juce::Slider& slider) const
    {
        if (&slider == &depth)
            return getDepthTint();
        if (&slider == &soil)
            return getSoilTint();
        return getAnchorTint();
    }

    juce::String getSliderTitle(const juce::Slider& slider) const
    {
        if (&slider == &depth)
            return "ROOT DEPTH";
        if (&slider == &soil)
            return "SOIL DENSITY";
        return "ANCHOR FORCE";
    }

    void drawFocusBubble(juce::Graphics& g, juce::Slider& slider) const
    {
        auto bubble = juce::Rectangle<float>(112.0f, 36.0f)
                          .withCentre({ (float) slider.getBounds().getCentreX(), (float) slider.getY() - 14.0f });

        if (bubble.getY() < (float) getShellBounds().getY() + 26.0f)
            bubble = bubble.withY((float) slider.getBottom() + 10.0f);

        bubble.setX(juce::jlimit((float) getShellBounds().getX() + 8.0f,
                                 (float) getShellBounds().getRight() - bubble.getWidth() - 8.0f,
                                 bubble.getX()));
        bubble.setY(juce::jlimit((float) getShellBounds().getY() + 26.0f,
                                 (float) getShellBounds().getBottom() - bubble.getHeight() - 40.0f,
                                 bubble.getY()));

        const auto tint = getSliderTint(slider);
        RootFlow::drawGlassPanel(g, bubble, bubble.getHeight() * 0.42f, slider.isMouseButtonDown() ? 0.92f : 0.72f);
        g.setColour(tint.withAlpha(slider.isMouseButtonDown() ? 0.16f : 0.10f));
        g.fillRoundedRectangle(bubble.reduced(2.0f), bubble.getHeight() * 0.34f);

        auto inner = bubble.reduced(9.0f, 4.0f);
        auto topRow = inner.removeFromTop(9.0f);
        auto mainRow = inner;
        auto valueArea = mainRow.removeFromRight(34.0f);

        g.setFont(RootFlow::getFont(7.6f).boldened());
        g.setColour(RootFlow::textMuted.withAlpha(0.88f));
        g.drawText(slider.isMouseButtonDown() ? "MORPH" : "FOCUS", topRow, juce::Justification::centredLeft, false);

        g.setFont(RootFlow::getFont(9.4f).boldened());
        g.setColour((slider.isMouseButtonDown() ? tint.brighter(0.20f) : RootFlow::text).withAlpha(0.96f));
        g.drawFittedText(getSliderTitle(slider), mainRow.toNearestInt(), juce::Justification::centredLeft, 1);

        g.setFont(RootFlow::getFont(8.0f).boldened());
        g.setColour(tint.withAlpha(0.96f));
        g.drawFittedText(formatPercent(slider.getValue()), valueArea.toNearestInt(), juce::Justification::centredRight, 1);
        RootFlow::drawGlowOrb(g, { bubble.getRight() - 10.0f, bubble.getY() + 9.5f }, 2.8f, tint,
                              slider.isMouseButtonDown() ? 0.88f : 0.56f);
    }

    RootFlow::InteractiveSlider depth, soil, anchor;
};
