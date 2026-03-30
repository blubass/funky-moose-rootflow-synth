#pragma once
#include <JuceHeader.h>
#include "Utils/DesignTokens.h"
#include "EnergyDisplay.h"

class CenterComponent : public juce::Component
{
public:
    CenterComponent()
    {
        flow.setName("FLOW");
        vitality.setName("VITALITY");
        texture.setName("TEXTURE");
        canopy.setName("CANOPY");
        instability.setName("INSTABILITY");
        atmos.setName("ATMOS");
        seasons.setName("SEASONS");

        setupSlider(flow);
        setupSlider(vitality);
        setupSlider(texture);
        setupSlider(canopy);
        setupSlider(instability);
        setupSlider(atmos);
        setupSlider(seasons);
        wireSlider(flow);
        wireSlider(vitality);
        wireSlider(texture);
        wireSlider(canopy);
        wireSlider(instability);
        wireSlider(atmos);
        wireSlider(seasons);

        addAndMakeVisible(flow);
        addAndMakeVisible(vitality);
        addAndMakeVisible(texture);
        addAndMakeVisible(canopy);
        addAndMakeVisible(instability);
        addAndMakeVisible(atmos);
        addAndMakeVisible(seasons);
        addAndMakeVisible(energyDisplay);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(20);

        const int wingWidth = juce::jlimit(170, 250, juce::roundToInt(area.getWidth() * 0.25f));
        auto leftWing = area.removeFromLeft(wingWidth).reduced(16, 34);
        auto rightWing = area.removeFromRight(wingWidth).reduced(16, 34);
        auto displayArea = area.reduced(4, 18);

        layoutWing(leftWing, { &flow, &vitality, &texture, &canopy });
        layoutWing(rightWing, { &instability, &atmos, &seasons });
        energyDisplay.setBounds(displayArea);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(6.0f);
        auto leftShapeArea = area.withWidth(area.getWidth() * 0.27f).withTrimmedTop(24.0f).withTrimmedBottom(24.0f);
        auto rightShapeArea = leftShapeArea.withX(area.getRight() - leftShapeArea.getWidth());
        auto coreArea = area.withTrimmedLeft(leftShapeArea.getWidth() - 30.0f)
                            .withTrimmedRight(rightShapeArea.getWidth() - 30.0f)
                            .withTrimmedTop(44.0f)
                            .withTrimmedBottom(36.0f);
        auto* focusedSlider = findFocusedSlider();
        const bool focusOnLeft = focusedSlider == &flow || focusedSlider == &vitality
                              || focusedSlider == &texture || focusedSlider == &canopy;
        const bool focusOnRight = focusedSlider == &instability || focusedSlider == &atmos || focusedSlider == &seasons;
        const auto focusTint = focusedSlider != nullptr ? getSliderTint(*focusedSlider) : getGroveTint();
        const auto streamTint = getStreamTint();
        const auto groveTint = getGroveTint();
        const auto bloomTint = getBloomTint();
        const auto leftShape = RootFlow::makeOrganicPod(leftShapeArea, false);
        const auto rightShape = RootFlow::makeOrganicPod(rightShapeArea, true);

        RootFlow::drawOrganicPanel(g, leftShapeArea, false);
        RootFlow::drawOrganicPanel(g, rightShapeArea, true);
        RootFlow::drawGlassPanel(g, coreArea, 36.0f, 0.80f);

        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(leftShape);

            juce::ColourGradient leftCurrent(streamTint.withAlpha(0.05f), leftShapeArea.getX() + leftShapeArea.getWidth() * 0.58f, leftShapeArea.getY() + leftShapeArea.getHeight() * 0.18f,
                                             juce::Colours::transparentBlack, leftShapeArea.getX() + leftShapeArea.getWidth() * 0.20f, leftShapeArea.getBottom(), true);
            g.setGradientFill(leftCurrent);
            g.fillEllipse(leftShapeArea.getX() + leftShapeArea.getWidth() * 0.08f, leftShapeArea.getY() + leftShapeArea.getHeight() * 0.02f,
                          leftShapeArea.getWidth() * 0.78f, leftShapeArea.getHeight() * 0.78f);

            juce::ColourGradient leftGrove(groveTint.withAlpha(0.04f), leftShapeArea.getRight() - leftShapeArea.getWidth() * 0.16f, leftShapeArea.getBottom() - leftShapeArea.getHeight() * 0.18f,
                                           juce::Colours::transparentBlack, leftShapeArea.getX(), leftShapeArea.getY() + leftShapeArea.getHeight() * 0.40f, true);
            g.setGradientFill(leftGrove);
            g.fillEllipse(leftShapeArea.getX() + leftShapeArea.getWidth() * 0.10f, leftShapeArea.getY() + leftShapeArea.getHeight() * 0.30f,
                          leftShapeArea.getWidth() * 0.72f, leftShapeArea.getHeight() * 0.56f);
        }

        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(rightShape);

            juce::ColourGradient rightBloom(bloomTint.withAlpha(0.05f), rightShapeArea.getX() + rightShapeArea.getWidth() * 0.34f, rightShapeArea.getY() + rightShapeArea.getHeight() * 0.16f,
                                            juce::Colours::transparentBlack, rightShapeArea.getRight() - rightShapeArea.getWidth() * 0.10f, rightShapeArea.getBottom(), true);
            g.setGradientFill(rightBloom);
            g.fillEllipse(rightShapeArea.getX() + rightShapeArea.getWidth() * 0.12f, rightShapeArea.getY() + rightShapeArea.getHeight() * 0.04f,
                          rightShapeArea.getWidth() * 0.76f, rightShapeArea.getHeight() * 0.72f);

            juce::ColourGradient rightCurrent(groveTint.withAlpha(0.04f), rightShapeArea.getRight() - rightShapeArea.getWidth() * 0.18f, rightShapeArea.getBottom() - rightShapeArea.getHeight() * 0.16f,
                                              juce::Colours::transparentBlack, rightShapeArea.getCentreX(), rightShapeArea.getY() + rightShapeArea.getHeight() * 0.34f, true);
            g.setGradientFill(rightCurrent);
            g.fillEllipse(rightShapeArea.getX() + rightShapeArea.getWidth() * 0.16f, rightShapeArea.getY() + rightShapeArea.getHeight() * 0.34f,
                          rightShapeArea.getWidth() * 0.70f, rightShapeArea.getHeight() * 0.52f);
        }

        juce::ColourGradient coreSpectrum(streamTint.withAlpha(0.032f), coreArea.getX() + coreArea.getWidth() * 0.26f, coreArea.getY() + coreArea.getHeight() * 0.24f,
                                          bloomTint.withAlpha(0.026f), coreArea.getRight() - coreArea.getWidth() * 0.22f, coreArea.getBottom() - coreArea.getHeight() * 0.18f, false);
        g.setGradientFill(coreSpectrum);
        g.fillRoundedRectangle(coreArea.reduced(4.0f), 32.0f);

        juce::ColourGradient coreHeart(groveTint.withAlpha(0.045f), coreArea.getCentreX(), coreArea.getCentreY(),
                                       juce::Colours::transparentBlack, coreArea.getCentreX(), coreArea.getBottom(), true);
        g.setGradientFill(coreHeart);
        g.fillEllipse(coreArea.expanded(-coreArea.getWidth() * 0.20f, -coreArea.getHeight() * 0.24f));

        if (focusOnLeft)
        {
            g.setColour(focusTint.withAlpha(0.025f));
            g.fillPath(leftShape);
            g.setColour(focusTint.withAlpha(0.08f));
            g.strokePath(leftShape, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (focusOnRight)
        {
            g.setColour(focusTint.withAlpha(0.025f));
            g.fillPath(rightShape);
            g.setColour(focusTint.withAlpha(0.08f));
            g.strokePath(rightShape, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (focusedSlider != nullptr)
        {
            g.setColour(focusTint.withAlpha(0.022f));
            g.fillRoundedRectangle(coreArea.reduced(2.0f), 32.0f);
            g.setColour(focusTint.withAlpha(0.09f));
            g.drawRoundedRectangle(coreArea.reduced(1.5f), 34.0f, 0.9f);
        }

        auto canopyArea = coreArea.withTrimmedBottom(coreArea.getHeight() * 0.52f).translated(0.0f, -10.0f);
        juce::Path canopy;
        canopy.startNewSubPath(canopyArea.getX() + canopyArea.getWidth() * 0.10f, canopyArea.getBottom());
        canopy.cubicTo(canopyArea.getX() + canopyArea.getWidth() * 0.18f, canopyArea.getY() + canopyArea.getHeight() * 0.08f,
                       canopyArea.getCentreX() - canopyArea.getWidth() * 0.12f, canopyArea.getY() - canopyArea.getHeight() * 0.12f,
                       canopyArea.getCentreX(), canopyArea.getY() + canopyArea.getHeight() * 0.12f);
        canopy.cubicTo(canopyArea.getCentreX() + canopyArea.getWidth() * 0.12f, canopyArea.getY() - canopyArea.getHeight() * 0.12f,
                       canopyArea.getRight() - canopyArea.getWidth() * 0.18f, canopyArea.getY() + canopyArea.getHeight() * 0.08f,
                       canopyArea.getRight() - canopyArea.getWidth() * 0.10f, canopyArea.getBottom());
        canopy.lineTo(canopyArea.getX() + canopyArea.getWidth() * 0.16f, canopyArea.getBottom());
        canopy.closeSubPath();

        juce::ColourGradient canopyGlow(bloomTint.withAlpha(0.045f), canopyArea.getCentreX(), canopyArea.getY(),
                                        juce::Colours::transparentBlack, canopyArea.getCentreX(), canopyArea.getBottom(), true);
        g.setGradientFill(canopyGlow);
        g.fillPath(canopy);

        auto basinArea = coreArea.withTrimmedTop(coreArea.getHeight() * 0.68f)
                                .reduced(coreArea.getWidth() * 0.16f, 0.0f)
                                .translated(0.0f, 14.0f);
        juce::ColourGradient basinGlow(groveTint.withAlpha(0.045f), basinArea.getCentreX(), basinArea.getY(),
                                       juce::Colours::transparentBlack, basinArea.getCentreX(), basinArea.getBottom(), true);
        g.setGradientFill(basinGlow);
        g.fillEllipse(basinArea);

        juce::ColourGradient glow(streamTint.withAlpha(0.035f), coreArea.getCentreX(), coreArea.getCentreY(),
                                  juce::Colours::transparentBlack, coreArea.getCentreX(), coreArea.getBottom(), true);
        g.setGradientFill(glow);
        g.fillEllipse(coreArea.expanded(-coreArea.getWidth() * 0.14f, -coreArea.getHeight() * 0.18f));

        const auto topLeftHub = juce::Point<float>(coreArea.getCentreX() - coreArea.getWidth() * 0.18f, coreArea.getY() + coreArea.getHeight() * 0.10f);
        const auto topRightHub = juce::Point<float>(coreArea.getCentreX() + coreArea.getWidth() * 0.18f, coreArea.getY() + coreArea.getHeight() * 0.10f);
        const auto bottomHub = juce::Point<float>(coreArea.getCentreX(), coreArea.getBottom() - coreArea.getHeight() * 0.12f);
        RootFlow::drawOrbSocket(g, topLeftHub, 6.6f, streamTint, 0.34f);
        RootFlow::drawOrbSocket(g, topRightHub, 6.6f, bloomTint, 0.34f);
        RootFlow::drawOrbSocket(g, bottomHub, 8.2f, groveTint, 0.40f);
        RootFlow::drawBioThread(g, topLeftHub, energyDisplay.getBounds().toFloat().getCentre().translated(-46.0f, -56.0f), streamTint, 0.08f, 1.15f);
        RootFlow::drawBioThread(g, topRightHub, energyDisplay.getBounds().toFloat().getCentre().translated(46.0f, -56.0f), bloomTint, 0.08f, 1.15f);
        RootFlow::drawBioThread(g, bottomHub, energyDisplay.getBounds().toFloat().getCentre().translated(0.0f, 82.0f), groveTint, 0.08f, 1.30f);

        drawConnectionFibres(g, focusedSlider);
        drawSliderLabels(g, focusedSlider);
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        if (! RootFlow::arePopupOverlaysEnabled())
            return;

        if (auto* slider = findFocusedSlider())
            drawFocusBubble(g, *slider);
    }

    juce::Slider& getFlowSlider() { return flow; }
    juce::Slider& getVitalitySlider() { return vitality; }
    juce::Slider& getTextureSlider() { return texture; }
    juce::Slider& getCanopySlider() { return canopy; }
    juce::Slider& getInstabilitySlider() { return instability; }
    juce::Slider& getAtmosSlider() { return atmos; }
    juce::Slider& getSeasonsSlider() { return seasons; }
    EnergyDisplay& getEnergyDisplay() { return energyDisplay; }
    juce::Slider* getFocusedSlider() const { return findFocusedSlider(); }
    juce::Colour getFocusTint() const
    {
        if (auto* slider = findFocusedSlider())
            return getSliderTint(*slider);

        return getGroveTint();
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
    void setupSlider(juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    }

    void wireSlider(RootFlow::InteractiveSlider& s)
    {
        s.onInteractionStateChange = [this] { repaint(); };
        s.onValueChange = [this] { repaint(); };
    }

    static juce::String formatPercent(double value)
    {
        return juce::String(juce::roundToInt(juce::jlimit(0.0, 1.0, value) * 100.0)) + "%";
    }

    static juce::Colour getStreamTint()
    {
        return RootFlow::accent.interpolatedWith(RootFlow::violet, 0.18f);
    }

    static juce::Colour getGroveTint()
    {
        return RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.40f);
    }

    static juce::Colour getBloomTint()
    {
        return RootFlow::amber.interpolatedWith(RootFlow::accentSoft, 0.18f);
    }

    void layoutWing(juce::Rectangle<int> area, std::initializer_list<juce::Slider*> sliders)
    {
        const int count = (int) sliders.size();
        if (count <= 0)
            return;

        const int slotHeight = area.getHeight() / count;
        for (auto* slider : sliders)
        {
            auto row = area.removeFromTop(slotHeight);
            slider->setBounds(row.removeFromBottom(26).reduced(12, 0));
        }
    }

    void drawSliderLabels(juce::Graphics& g, const juce::Slider* focusedSlider)
    {
        for (auto* slider : { &flow, &vitality, &texture, &canopy, &instability, &atmos, &seasons })
        {
            auto labelArea = slider->getBounds().withHeight(16).withBottom(slider->getY() - 1);
            const auto just = slider->getBounds().getCentreX() < getWidth() / 2
                ? juce::Justification::bottomLeft
                : juce::Justification::bottomRight;
            const bool isFocused = slider == focusedSlider;
            const auto tint = getSliderTint(*slider);
            g.setFont(RootFlow::getFont(isFocused ? 11.2f : 10.2f).boldened());
            g.setColour(juce::Colours::black.withAlpha(isFocused ? 0.32f : 0.22f));
            g.drawText(slider->getName(), labelArea.translated(0, 1), just, false);
            g.setColour((isFocused ? RootFlow::text.interpolatedWith(tint, 0.22f)
                                   : RootFlow::text.interpolatedWith(tint, 0.08f)).withAlpha(isFocused ? 0.98f : 0.94f));
            g.drawText(slider->getName(), labelArea, just, false);
        }
    }

    void drawConnectionFibres(juce::Graphics& g, const juce::Slider* focusedSlider)
    {
        const auto target = energyDisplay.getBounds().toFloat().getCentre();

        auto drawFibre = [&g, target, focusedSlider, this] (juce::Slider& slider)
        {
            const auto colour = getSliderTint(slider);
            auto b = slider.getBounds().toFloat();
            const bool leftSide = b.getCentreX() < target.x;
            const auto start = juce::Point<float>(leftSide ? b.getRight() - 12.0f : b.getX() + 12.0f, b.getCentreY());
            const auto end = juce::Point<float>(leftSide ? target.x - 46.0f : target.x + 46.0f,
                                                target.y + (b.getCentreY() - target.y) * 0.22f);
            const bool emphasise = &slider == focusedSlider;

            juce::Path fibre;
            fibre.startNewSubPath(start);
            fibre.cubicTo(start.x + (leftSide ? 48.0f : -48.0f), start.y - 12.0f,
                          end.x + (leftSide ? -36.0f : 36.0f), end.y - 18.0f,
                          end.x, end.y);

            g.setColour(colour.withAlpha(emphasise ? 0.07f : 0.04f));
            g.strokePath(fibre, juce::PathStrokeType(emphasise ? 4.0f : 2.8f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            g.setColour(colour.withAlpha(emphasise ? 0.14f : 0.09f));
            g.strokePath(fibre, juce::PathStrokeType(emphasise ? 1.0f : 0.8f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        };

        drawFibre(flow);
        drawFibre(vitality);
        drawFibre(texture);
        drawFibre(canopy);
        drawFibre(instability);
        drawFibre(atmos);
        drawFibre(seasons);
    }

    juce::Slider* findFocusedSlider() const
    {
        for (auto* slider : { static_cast<const juce::Slider*>(&flow), static_cast<const juce::Slider*>(&vitality),
                              static_cast<const juce::Slider*>(&texture), static_cast<const juce::Slider*>(&canopy),
                              static_cast<const juce::Slider*>(&instability), static_cast<const juce::Slider*>(&atmos),
                              static_cast<const juce::Slider*>(&seasons) })
        {
            if (slider->isMouseButtonDown())
                return const_cast<juce::Slider*>(slider);
        }

        for (auto* slider : { static_cast<const juce::Slider*>(&flow), static_cast<const juce::Slider*>(&vitality),
                              static_cast<const juce::Slider*>(&texture), static_cast<const juce::Slider*>(&canopy),
                              static_cast<const juce::Slider*>(&instability), static_cast<const juce::Slider*>(&atmos),
                              static_cast<const juce::Slider*>(&seasons) })
        {
            if (RootFlow::isInteractiveHoverActive(*slider) && ! slider->isMouseButtonDown())
                return const_cast<juce::Slider*>(slider);
        }

        return nullptr;
    }

    juce::Colour getSliderTint(const juce::Slider& slider) const
    {
        if (&slider == &flow)
            return RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.38f);
        if (&slider == &vitality)
            return getGroveTint();
        if (&slider == &texture)
            return getStreamTint();
        if (&slider == &canopy)
            return getBloomTint();
        if (&slider == &instability)
            return RootFlow::violet.interpolatedWith(RootFlow::accent, 0.18f);
        if (&slider == &atmos)
            return RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.18f);

        return RootFlow::violet.interpolatedWith(RootFlow::accentSoft, 0.28f);
    }

    juce::String getSliderTitle(const juce::Slider& slider) const
    {
        if (&slider == &flow)
            return "NEURAL FLOW";
        if (&slider == &vitality)
            return "VITALITY";
        if (&slider == &texture)
            return "TEXTURE FIELD";
        if (&slider == &canopy)
            return "CANOPY";
        if (&slider == &instability)
            return "INSTABILITY";
        if (&slider == &atmos)
            return "ATMOS";

        return "SEASONS";
    }

    void drawFocusBubble(juce::Graphics& g, juce::Slider& slider) const
    {
        auto bubble = juce::Rectangle<float>(118.0f, 36.0f)
                          .withCentre({ (float) slider.getBounds().getCentreX(), (float) slider.getY() - 16.0f });

        auto limit = getLocalBounds().toFloat().reduced(12.0f, 16.0f);
        bubble.setX(juce::jlimit(limit.getX(), limit.getRight() - bubble.getWidth(), bubble.getX()));

        if (bubble.getY() < limit.getY())
            bubble = bubble.withY((float) slider.getBottom() + 10.0f);

        bubble.setY(juce::jlimit(limit.getY(), limit.getBottom() - bubble.getHeight(), bubble.getY()));

        const auto tint = getSliderTint(slider);
        RootFlow::drawGlassPanel(g, bubble, bubble.getHeight() * 0.42f, slider.isMouseButtonDown() ? 0.94f : 0.72f);
        g.setColour(tint.withAlpha(slider.isMouseButtonDown() ? 0.16f : 0.10f));
        g.fillRoundedRectangle(bubble.reduced(2.0f), bubble.getHeight() * 0.34f);

        auto inner = bubble.reduced(9.0f, 4.0f);
        auto topRow = inner.removeFromTop(9.0f);
        auto mainRow = inner;
        auto valueArea = mainRow.removeFromRight(36.0f);

        g.setFont(RootFlow::getFont(7.6f).boldened());
        g.setColour(RootFlow::textMuted.withAlpha(0.88f));
        g.drawText(slider.isMouseButtonDown() ? "MORPH" : "FOCUS", topRow, juce::Justification::centredLeft, false);

        g.setFont(RootFlow::getFont(9.4f).boldened());
        g.setColour((slider.isMouseButtonDown() ? tint.brighter(0.20f) : RootFlow::text).withAlpha(0.96f));
        g.drawFittedText(getSliderTitle(slider), mainRow.toNearestInt(), juce::Justification::centredLeft, 1);

        g.setFont(RootFlow::getFont(7.9f).boldened());
        g.setColour(tint.withAlpha(0.96f));
        g.drawFittedText(formatPercent(slider.getValue()), valueArea.toNearestInt(), juce::Justification::centredRight, 1);
        RootFlow::drawGlowOrb(g, { bubble.getRight() - 10.0f, bubble.getY() + 9.5f }, 2.8f, tint,
                              slider.isMouseButtonDown() ? 0.88f : 0.56f);
    }

    RootFlow::InteractiveSlider flow, vitality, texture, canopy, instability, atmos, seasons;
    EnergyDisplay energyDisplay;
};
