#pragma once
#include <JuceHeader.h>
#include "Utils/DesignTokens.h"

class RootFlowAudioProcessor;

// ============================================================
//  BioSequencerComponent
//  A 16-step bio-tech sequencer that visually matches the
//  node network: circular bio-nodes, energy glow, HSV pulsing.
// ============================================================
class BioSequencerComponent : public juce::Component,
                               private juce::Timer
{
public:
    BioSequencerComponent(RootFlowAudioProcessor& p) : proc(p)
    {
        // Power button
        powerButton.setButtonText("OFF");
        powerButton.setClickingTogglesState(true);
        seqOnAtt = std::make_unique<juce::ButtonParameterAttachment>(
            *p.tree.getParameter("sequencerOn"), powerButton);

        powerButton.onStateChange = [this]
        {
            if (! powerButton.getToggleState())
                hoveredStep = -1;

            refreshPowerButtonText();
            repaint();
        };
        addAndMakeVisible(powerButton);

        // Rate Box
        rateBox.addItem("1/4", 1);
        rateBox.addItem("1/8", 2);
        rateBox.addItem("1/16", 3);
        rateBox.addItem("1/32", 4);
        rateAtt = std::make_unique<juce::ComboBoxParameterAttachment>(
            *p.tree.getParameter("sequencerRate"), rateBox);
        addAndMakeVisible(rateBox);

        // Steps Box
        stepBox.addItem("4", 1);
        stepBox.addItem("8", 2);
        stepBox.addItem("12", 3);
        stepBox.addItem("16", 4);
        stepAtt = std::make_unique<juce::ComboBoxParameterAttachment>(
            *p.tree.getParameter("sequencerSteps"), stepBox);
        addAndMakeVisible(stepBox);

        gateSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        gateSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        gateSlider.getProperties().set("rootflowStyle", "ambient-horizontal");
        gateSlider.setMouseDragSensitivity(260);
        gateSlider.onValueChange = [this] { repaint(); };
        gateAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.tree, "sequencerGate", gateSlider);
        addAndMakeVisible(gateSlider);

        refreshPowerButtonText();
        startTimerHz(60);
    }

    ~BioSequencerComponent() override { stopTimer(); }

    void timerCallback() override { repaint(); }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14, 10);
        area.removeFromTop(8);
        auto controlBand = area.removeFromTop(24);
        rateBox.setBounds(controlBand.removeFromLeft(74));
        controlBand.removeFromLeft(8);
        stepBox.setBounds(controlBand.removeFromLeft(64));
        powerButton.setBounds(controlBand.removeFromRight(56));

        gateSlider.setBounds(getGateSliderBounds().toNearestInt());
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (! powerButton.getToggleState())
            return;

        int hit = stepAt(e.position);
        if (hit < 0)
            return;

        // ALT + Click: Clear/Reset entire sequence
        if (e.mods.isAltDown())
        {
            proc.clearSequencerSteps();
        }
        // SHIFT + Click: Randomize this step's velocity
        else if (e.mods.isShiftDown())
        {
            proc.randomizeSequencerStepVelocity(hit);
        }
        // Right Click: Cycle velocity states
        else if (e.mods.isRightButtonDown())
        {
            proc.cycleSequencerStepState(hit);
        }
        // Regular Left Click: Toggle state
        else
        {
            proc.toggleSequencerStepActive(hit);
        }

        // Manual visual trigger for feedback
        const auto steps = proc.getSequencerStepSnapshot();
        if (steps[(size_t) hit].active)
        {
            proc.previewSequencerStep(hit);
        }

        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override
    {
        int hit = stepAt(e.position);
        if (hit < 0)
            return;

        hoveredStep = hit;
        proc.adjustSequencerStepVelocity(hit, wheel.deltaY * 0.12f);
        repaint();
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int hit = (RootFlow::areHoverEffectsEnabled() && powerButton.getToggleState()) ? stepAt(e.position) : -1;
        if (hit != hoveredStep)
        {
            hoveredStep = hit;
            repaint();
        }

        setMouseCursor(hit >= 0 ? juce::MouseCursor::PointingHandCursor
                                 : juce::MouseCursor::NormalCursor);
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hoveredStep = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    static juce::Path getHexPath(float x, float y, float radius)
    {
        juce::Path p;
        for (int i = 0; i < 6; ++i)
        {
            float angle = juce::MathConstants<float>::twoPi * (float)i / 6.0f - juce::MathConstants<float>::halfPi;
            float px = x + std::cos(angle) * radius;
            float py = y + std::sin(angle) * radius;
            if (i == 0) p.startNewSubPath(px, py);
            else        p.lineTo(px, py);
        }
        p.closeSubPath();
        return p;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        bool isOn   = powerButton.getToggleState();
        const auto steps = proc.getSequencerStepSnapshot();
        const int sequenceLength = getSequenceLength();
        const int curStep = getCurrentVisibleStep();
        const int focusStep = getFocusStepIndex();
        const int activeCount = [&steps, sequenceLength]()
        {
            int count = 0;
            const int limit = juce::jmin(sequenceLength, (int) steps.size());
            for (int i = 0; i < limit; ++i)
                if (steps[(size_t) i].active)
                    ++count;
            return count;
        }();
        const float gateValue = (float) gateSlider.getValue();
        const auto statusText = isOn
            ? juce::String("STEP ") + juce::String(curStep + 1).paddedLeft('0', 2) + "/" + juce::String(sequenceLength)
            : juce::String("DORMANT");

        RootFlow::drawGlassPanel(g, bounds, 30.0f, 0.92f);
        if (!isOn)
        {
            g.setColour(juce::Colour(0xff0b1410).withAlpha(0.38f));
            g.fillRoundedRectangle(bounds, 28.0f);
        }

        drawControlCapsule(g, rateBox.getBounds().toFloat().expanded(4.0f, 4.0f), RootFlow::accentSoft);
        drawControlCapsule(g, stepBox.getBounds().toFloat().expanded(4.0f, 4.0f), RootFlow::accent);
        drawControlCapsule(g, powerButton.getBounds().toFloat().expanded(4.0f, 4.0f), RootFlow::amber);

        auto statusArea = juce::Rectangle<float>((float) stepBox.getRight() + 12.0f,
                                                 (float) rateBox.getY() + 1.0f,
                                                 (float) powerButton.getX() - (float) stepBox.getRight() - 24.0f,
                                                 22.0f);
        if (statusArea.getWidth() > 92.0f)
        {
            drawControlCapsule(g, statusArea, isOn ? RootFlow::accent : RootFlow::textMuted);

            auto statusInner = statusArea.reduced(10.0f, 3.0f);
            g.setFont(RootFlow::getFont(10.4f).boldened());
            g.setColour((isOn ? RootFlow::text : RootFlow::textMuted).withAlpha(0.92f));
            g.drawFittedText(statusText + " / " + juce::String(activeCount) + " ACTIVE",
                             statusInner.toNearestInt(),
                             juce::Justification::centred,
                             1);
        }

        auto padArea = getPadArea();
        auto legendArea = getLegendArea();
        const float cellW = padArea.getWidth() / 9.0f;
        const float cellUnit = juce::jmin(cellW, padArea.getHeight());
        const float baseRadius = juce::jmin(cellW * 0.20f, padArea.getHeight() * 0.23f);
        float beat   = proc.beatPhase.load();
        float bPulse = std::pow(std::sin(beat * juce::MathConstants<float>::twoPi) * 0.5f + 0.5f, 4.0f);
        const auto hub = juce::Point<float>(padArea.getCentreX(), padArea.getBottom() + 4.0f);
        const auto crownHub = juce::Point<float>(bounds.getCentreX(), padArea.getY() + padArea.getHeight() * 0.08f);
        juce::Point<float> focusStepCentre;
        float focusStepRadius = 0.0f;
        juce::Colour focusStepColour = RootFlow::accent;
        juce::String focusStateText;

        auto crownRail = juce::Rectangle<float>(padArea.getX() + padArea.getWidth() * 0.10f,
                                                padArea.getY() + padArea.getHeight() * 0.04f,
                                                padArea.getWidth() * 0.80f,
                                                padArea.getHeight() * 0.10f);
        RootFlow::drawGlassPanel(g, crownRail, crownRail.getHeight() * 0.58f, 0.32f);
        juce::ColourGradient railGlow(RootFlow::accent.withAlpha(0.05f), crownRail.getCentreX(), crownRail.getY(),
                                      juce::Colours::transparentBlack, crownRail.getCentreX(), crownRail.getBottom() + padArea.getHeight() * 0.14f, false);
        g.setGradientFill(railGlow);
        g.fillRoundedRectangle(crownRail.reduced(2.0f), crownRail.getHeight() * 0.50f);
        g.setColour(juce::Colours::white.withAlpha(0.035f));
        g.drawRoundedRectangle(crownRail.reduced(1.0f), crownRail.getHeight() * 0.48f, 0.75f);

        auto drawClusterBed = [&](int startIndex, juce::Colour tint)
        {
            auto first = cellCentre(startIndex, padArea);
            float minX = first.x, minY = first.y, maxX = first.x, maxY = first.y;

            for (int i = startIndex + 1; i < startIndex + 4; ++i)
            {
                auto p = cellCentre(i, padArea);
                minX = juce::jmin(minX, p.x);
                minY = juce::jmin(minY, p.y);
                maxX = juce::jmax(maxX, p.x);
                maxY = juce::jmax(maxY, p.y);
            }

            auto bed = juce::Rectangle<float>(minX, minY, maxX - minX, maxY - minY)
                           .expanded(cellW * 0.22f, cellUnit * 0.26f);
            g.setColour(tint.withAlpha(isOn ? 0.030f : 0.014f));
            g.fillRoundedRectangle(bed, bed.getHeight() * 0.44f);
            g.setColour(tint.withAlpha(isOn ? 0.06f : 0.025f));
            g.drawRoundedRectangle(bed, bed.getHeight() * 0.44f, 0.9f);
        };

        drawClusterBed(0, RootFlow::accentSoft);
        drawClusterBed(4, RootFlow::accent);
        drawClusterBed(8, RootFlow::amber);
        drawClusterBed(12, RootFlow::violet);

        RootFlow::drawOrbSocket(g, crownHub.translated(-56.0f, -2.0f), 5.5f, RootFlow::accentSoft, 0.30f);
        RootFlow::drawOrbSocket(g, crownHub.translated(56.0f, -2.0f), 5.5f, RootFlow::amber, 0.30f);
        RootFlow::drawOrbSocket(g, hub, 7.2f, RootFlow::accent, 0.36f);
        RootFlow::drawBioThread(g, crownHub.translated(-56.0f, -2.0f), crownHub.translated(-8.0f, 12.0f), RootFlow::accentSoft, 0.075f, 1.0f);
        RootFlow::drawBioThread(g, crownHub.translated(56.0f, -2.0f), crownHub.translated(8.0f, 12.0f), RootFlow::amber, 0.075f, 1.0f);
        RootFlow::drawBioThread(g, crownHub, hub.translated(0.0f, -16.0f), RootFlow::accent, 0.075f, 1.0f);

        for (int i = 0; i < 15; ++i)
        {
            const bool sameRow = (i < 7) || (i >= 8 && i < 15);
            if (! sameRow)
                continue;

            auto a = cellCentre(i, padArea);
            auto b = cellCentre(i + 1, padArea);
            g.setColour(RootFlow::accent.withAlpha(0.04f));
            g.drawLine({ a, b }, 0.9f);
        }

        for (int i = 0; i < 8; ++i)
        {
            auto a = cellCentre(i, padArea);
            auto b = cellCentre(i + 8, padArea);
            RootFlow::drawBioThread(g, a, b, RootFlow::accent.withAlpha(0.72f), 0.05f, 0.8f);
        }

        for (int i = 0; i < 16; ++i)
        {
            const auto& step = steps[(size_t) i];
            bool active   = step.active && isOn;
            bool isCur    = (i == curStep);
            bool isHover  = RootFlow::areHoverEffectsEnabled() && (i == hoveredStep);
            bool isFocus  = (i == focusStep);

            auto centre = cellCentre(i, padArea);
            float x = centre.x;
            float y = centre.y;

            float radius = baseRadius * (isCur ? (1.08f + bPulse * 0.10f) : 1.0f);
            auto hex = getHexPath(x, y, radius);

            juce::Colour baseColor = active ? juce::Colour::fromHSV(0.38f + step.velocity * 0.12f, 0.95f, 1.0f, 1.0f) : juce::Colours::grey.withAlpha(0.15f);
            if (isCur && active) baseColor = baseColor.brighter(0.4f);

            if (isFocus)
            {
                const float focusRadius = radius * (isCur ? 1.48f : 1.36f) + (isHover ? 2.4f : 1.2f);
                const auto focusTint = active ? baseColor : RootFlow::textMuted;
                g.setColour(focusTint.withAlpha(active ? 0.16f : 0.08f));
                g.fillPath(getHexPath(x, y, focusRadius));
                g.setColour(juce::Colours::white.withAlpha(isHover ? 0.24f : 0.16f));
                g.strokePath(getHexPath(x, y, focusRadius), juce::PathStrokeType(isCur ? 1.5f : 1.0f));
                g.setColour(focusTint.withAlpha(isHover || isCur ? 0.58f : 0.36f));
                g.strokePath(getHexPath(x, y, focusRadius * 0.88f), juce::PathStrokeType(1.0f));
            }

            g.setColour(baseColor.withAlpha(active ? 0.18f : 0.04f));
            g.fillPath(getHexPath(x, y, radius * 1.24f));

            if (step.velocity > 0.05f)
            {
                const float ringRadius = radius * (1.10f + step.velocity * 0.12f);
                g.setColour(baseColor.withAlpha(active ? (0.14f + step.velocity * 0.14f) : 0.05f));
                g.strokePath(getHexPath(x, y, ringRadius), juce::PathStrokeType(isCur ? 1.6f : 1.0f));
            }

            juce::ColourGradient glassGrad(baseColor.withAlpha(active ? 0.95f : 0.12f), x, y - radius * 0.5f,
                                            baseColor.darker(0.6f).withAlpha(active ? 0.9f : 0.08f), x, y + radius, true);
            g.setGradientFill(glassGrad);
            g.fillPath(hex);

            if (active)
            {
                g.setColour(juce::Colours::white.withAlpha(0.4f * step.velocity));
                g.fillPath(getHexPath(x, y, radius * 0.3f));

                juce::Path rootlet;
                rootlet.startNewSubPath(x, y + radius * 0.2f);
                rootlet.quadraticTo(x + std::sin((float) i * 0.6f) * 12.0f, y + radius * 1.6f, hub.x + (x - hub.x) * 0.15f, hub.y);
                g.setColour(baseColor.withAlpha(0.22f));
                g.strokePath(rootlet, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour(juce::Colours::white.withAlpha(0.14f));
                g.strokePath(rootlet, juce::PathStrokeType(0.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                RootFlow::drawBioThread(g, crownHub, { x, y - radius * 0.3f }, baseColor, 0.08f + step.velocity * 0.06f, 0.9f);
            }

            g.setColour(juce::Colours::white.withAlpha(active ? 0.22f : 0.04f));
            g.fillEllipse(x - radius * 0.4f, y - radius * 0.6f, radius * 0.6f, radius * 0.3f);

            g.setColour(baseColor.withAlpha(active ? 0.7f : 0.1f));
            g.strokePath(hex, juce::PathStrokeType(1.0f));

            if (isCur)
            {
                g.setColour(juce::Colours::white.withAlpha(0.5f + bPulse * 0.5f));
                g.strokePath(getHexPath(x, y, radius * 1.3f), juce::PathStrokeType(1.2f));
                RootFlow::drawBioThread(g, { x, y + radius * 0.15f }, hub, baseColor.brighter(0.2f), 0.18f, 1.3f);
            }

            if ((i % 4) == 0)
            {
                g.setFont(RootFlow::getFont(8.2f));
                g.setColour(RootFlow::text.withAlpha(0.28f));
                g.drawText(juce::String(i + 1),
                           juce::roundToInt(x - 8.0f),
                           juce::roundToInt(y + radius + 8.0f),
                           16, 8,
                           juce::Justification::centred);
            }

            if (isFocus)
            {
                focusStepCentre = centre;
                focusStepRadius = radius;
                focusStepColour = active ? baseColor : RootFlow::textMuted;
                focusStateText = isCur ? "CURRENT"
                               : (isHover ? "HOVER"
                                          : (active ? "ACTIVE" : "DORMANT"));
            }
        }

        if (RootFlow::arePopupOverlaysEnabled() && juce::isPositiveAndBelow(focusStep, sequenceLength))
        {
            const auto& step = steps[(size_t) focusStep];
            auto bubble = juce::Rectangle<float>(114.0f, 36.0f)
                              .withCentre({ focusStepCentre.x, focusStepCentre.y - focusStepRadius - 15.0f });

            if (bubble.getY() < bounds.getY() + 12.0f)
                bubble = bubble.withY(focusStepCentre.y + focusStepRadius + 10.0f);

            bubble.setX(juce::jlimit(bounds.getX() + 8.0f,
                                     bounds.getRight() - bubble.getWidth() - 8.0f,
                                     bubble.getX()));
            bubble.setY(juce::jlimit(bounds.getY() + 8.0f,
                                     juce::jmax(bounds.getY() + 8.0f, getGateFieldArea().getY() - bubble.getHeight() - 8.0f),
                                     bubble.getY()));

            drawStepBubble(g,
                           bubble,
                           focusStateText,
                           "STEP " + juce::String(focusStep + 1).paddedLeft('0', 2),
                           step.active ? "VEL " + formatPercent(step.velocity) : "VEL --",
                           focusStepColour,
                           focusStep == hoveredStep || focusStep == curStep);
        }

        drawControlCapsule(g, legendArea, isOn ? RootFlow::accentSoft : RootFlow::textMuted);
        auto legendInner = legendArea.toNearestInt().reduced(12, 2);

        auto gateFieldArea = getGateFieldArea();
        drawControlCapsule(g, gateFieldArea, isOn ? RootFlow::amber : RootFlow::textMuted);

        auto gateLabelArea = juce::Rectangle<float>(gateFieldArea.getX() + 12.0f, gateFieldArea.getY(), 44.0f, gateFieldArea.getHeight());
        auto gateValueArea = juce::Rectangle<float>(gateFieldArea.getRight() - 52.0f, gateFieldArea.getY(), 40.0f, gateFieldArea.getHeight());
        auto gateTrackGlow = gateSlider.getBounds().toFloat().expanded(10.0f, 4.0f);

        g.setColour((isOn ? RootFlow::amber : RootFlow::textMuted).withAlpha(isOn ? 0.06f : 0.04f));
        g.fillRoundedRectangle(gateTrackGlow, gateTrackGlow.getHeight() * 0.48f);
        g.setColour((isOn ? RootFlow::amber : RootFlow::textMuted).withAlpha(isOn ? 0.18f : 0.10f));
        g.drawRoundedRectangle(gateTrackGlow, gateTrackGlow.getHeight() * 0.48f, 0.9f);

        g.setFont(RootFlow::getFont(8.9f).boldened());
        g.setColour((isOn ? RootFlow::textMuted : RootFlow::textMuted.darker(0.16f)).withAlpha(0.92f));
        g.drawFittedText("GATE", gateLabelArea.toNearestInt(), juce::Justification::centredLeft, 1);

        g.setColour((isOn ? RootFlow::amber : RootFlow::text).withAlpha(0.92f));
        g.drawFittedText(formatPercent(gateValue), gateValueArea.toNearestInt(), juce::Justification::centredRight, 1);
        RootFlow::drawGlowOrb(g,
                              { gateValueArea.getX() - 6.0f, gateFieldArea.getCentreY() },
                              3.0f,
                              RootFlow::amber,
                              isOn ? (0.28f + gateValue * 0.24f) : 0.12f);

        g.setFont(RootFlow::getFont(8.8f).boldened());
        g.setColour((isOn ? RootFlow::textMuted : RootFlow::textMuted.darker(0.12f)).withAlpha(0.90f));
        g.drawFittedText("CLICK TOGGLE / RIGHT CYCLE / WHEEL LEVEL / SHIFT RANDOM / ALT CLEAR",
                         legendInner,
                         juce::Justification::centred,
                         1);

        float ledAlpha = isOn ? 1.0f : 0.2f;
        juce::Colour ledCol = isOn ? RootFlow::accent : juce::Colour(0xff334433);
        auto btn = powerButton.getBounds().toFloat();
        float ledX = btn.getX() - 10.0f;
        float ledY = btn.getCentreY();
        g.setColour(ledCol.withAlpha(ledAlpha * 0.4f));
        g.fillEllipse(ledX - 4.0f, ledY - 4.0f, 8.0f, 8.0f);
        g.setColour(ledCol.withAlpha(ledAlpha));
        g.fillEllipse(ledX - 2.0f, ledY - 2.0f, 4.0f, 4.0f);
    }

    bool hasActivityFocus() const
    {
        return getFocusMode() != FocusMode::none;
    }

    juce::Point<float> getFocusAnchor() const
    {
        if (isGateFocused())
            return getGateFieldArea().getCentre().translated((float) getX(), (float) getY());

        if (const int focusStep = getFocusStepIndex(); juce::isPositiveAndBelow(focusStep, getSequenceLength()))
            return cellCentre(focusStep, getPadArea()).translated((float) getX(), (float) getY());

        return getBounds().toFloat().getCentre();
    }

    juce::Colour getFocusTint() const
    {
        if (isGateFocused())
            return RootFlow::amber;

        if (const int focusStep = getFocusStepIndex(); juce::isPositiveAndBelow(focusStep, getSequenceLength()))
            return getStepTint(focusStep);

        return RootFlow::textMuted;
    }

    juce::String getFocusTitle() const
    {
        if (isGateFocused())
            return "GATE FIELD";

        if (const int focusStep = getFocusStepIndex(); juce::isPositiveAndBelow(focusStep, getSequenceLength()))
            return "STEP " + juce::String(focusStep + 1).paddedLeft('0', 2);

        return {};
    }

    juce::String getFocusValueText() const
    {
        if (isGateFocused())
            return formatPercent((float) gateSlider.getValue());

        if (const int focusStep = getFocusStepIndex(); juce::isPositiveAndBelow(focusStep, getSequenceLength()))
        {
            const auto steps = proc.getSequencerStepSnapshot();
            const auto& step = steps[(size_t) focusStep];
            return step.active ? "VEL " + formatPercent(step.velocity)
                               : juce::String("VEL --");
        }

        return {};
    }

private:
    enum class FocusMode { none, gate, stepHover, stepCurrent };

    void refreshPowerButtonText()
    {
        powerButton.setButtonText(powerButton.getToggleState() ? "ON" : "OFF");
    }

    RootFlowAudioProcessor& proc;
    juce::TextButton        powerButton;
    juce::ComboBox          rateBox;
    juce::ComboBox          stepBox;
    juce::Slider            gateSlider;
    int                     hoveredStep = -1;

    std::unique_ptr<juce::ButtonParameterAttachment> seqOnAtt;
    std::unique_ptr<juce::ComboBoxParameterAttachment> rateAtt;
    std::unique_ptr<juce::ComboBoxParameterAttachment> stepAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAtt;

    void drawControlCapsule(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour tint) const
    {
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRoundedRectangle(area, area.getHeight() * 0.52f);

        juce::ColourGradient fill(RootFlow::panel.brighter(0.14f), area.getCentreX(), area.getY(),
                                  RootFlow::panel.darker(0.04f), area.getCentreX(), area.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(area.reduced(1.0f), area.getHeight() * 0.48f);

        g.setColour(tint.withAlpha(0.16f));
        g.drawRoundedRectangle(area.reduced(1.0f), area.getHeight() * 0.48f, 1.0f);
    }

    void drawStepBubble(juce::Graphics& g,
                        juce::Rectangle<float> area,
                        const juce::String& state,
                        const juce::String& stepName,
                        const juce::String& valueText,
                        juce::Colour tint,
                        bool emphasise) const
    {
        RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.40f, emphasise ? 0.92f : 0.72f);
        g.setColour(tint.withAlpha(emphasise ? 0.14f : 0.08f));
        g.fillRoundedRectangle(area.reduced(2.0f), area.getHeight() * 0.34f);

        auto inner = area.reduced(9.0f, 4.0f);
        auto topRow = inner.removeFromTop(9.0f);
        auto mainRow = inner;
        auto valueArea = mainRow.removeFromRight(36.0f);

        g.setFont(RootFlow::getFont(7.6f).boldened());
        g.setColour(RootFlow::textMuted.withAlpha(0.88f));
        g.drawText(state, topRow, juce::Justification::centredLeft, false);

        g.setFont(RootFlow::getFont(9.6f).boldened());
        g.setColour((emphasise ? tint.brighter(0.20f) : RootFlow::text).withAlpha(0.96f));
        g.drawFittedText(stepName, mainRow.toNearestInt(), juce::Justification::centredLeft, 1);

        g.setFont(RootFlow::getFont(7.9f).boldened());
        g.setColour(tint.withAlpha(0.96f));
        g.drawFittedText(valueText, valueArea.toNearestInt(), juce::Justification::centredRight, 1);

        RootFlow::drawGlowOrb(g, { area.getRight() - 10.0f, area.getY() + 9.5f }, 2.8f, tint, emphasise ? 0.86f : 0.54f);
    }

    juce::Rectangle<float> getPadArea() const
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        const float topTrim = juce::jmax(52.0f, (float) powerButton.getBottom() - bounds.getY() + 16.0f);
        const auto gateArea = getGateFieldArea();
        const float bottomTrim = juce::jmax(58.0f, bounds.getBottom() - gateArea.getY() + 6.0f);
        return bounds.withTrimmedTop(topTrim).withTrimmedBottom(bottomTrim).reduced(20.0f, 0.0f);
    }

    juce::Rectangle<float> getLegendArea() const
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        return juce::Rectangle<float>(bounds.getX() + 24.0f,
                                      bounds.getBottom() - 26.0f,
                                      bounds.getWidth() - 48.0f,
                                      22.0f);
    }

    juce::Rectangle<float> getGateFieldArea() const
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        auto legendArea = getLegendArea();
        const float width = juce::jlimit(220.0f, bounds.getWidth() - 120.0f, bounds.getWidth() * 0.44f);
        return juce::Rectangle<float>(width, 22.0f)
            .withCentre({ bounds.getCentreX(), legendArea.getY() - 15.0f });
    }

    juce::Rectangle<float> getGateSliderBounds() const
    {
        auto gateArea = getGateFieldArea();
        return gateArea.withTrimmedLeft(54.0f).withTrimmedRight(52.0f).reduced(0.0f, 1.0f);
    }

    static juce::String formatPercent(float value)
    {
        return juce::String(juce::roundToInt(value * 100.0f)) + "%";
    }

    int getSequenceLength() const
    {
        return juce::jmax(4, stepBox.getText().getIntValue());
    }

    int getCurrentVisibleStep() const
    {
        if (! powerButton.getToggleState())
            return -1;

        const int currentStep = proc.getCurrentSequencerStep();
        const int sequenceLength = getSequenceLength();
        return currentStep >= 0 ? currentStep % juce::jmax(1, sequenceLength) : -1;
    }

    int getFocusStepIndex() const
    {
        if (! powerButton.getToggleState())
            return -1;

        const int sequenceLength = getSequenceLength();
        if (RootFlow::areHoverEffectsEnabled() && juce::isPositiveAndBelow(hoveredStep, sequenceLength))
            return hoveredStep;

        return getCurrentVisibleStep();
    }

    bool isGateFocused() const
    {
        return gateSlider.isMouseButtonDown() || (RootFlow::areHoverEffectsEnabled() && gateSlider.isMouseOverOrDragging());
    }

    FocusMode getFocusMode() const
    {
        if (isGateFocused())
            return FocusMode::gate;

        if (RootFlow::areHoverEffectsEnabled() && juce::isPositiveAndBelow(hoveredStep, getSequenceLength()))
            return FocusMode::stepHover;

        if (powerButton.getToggleState() && getCurrentVisibleStep() >= 0)
            return FocusMode::stepCurrent;

        return FocusMode::none;
    }

    juce::Colour getStepTint(int stepIndex) const
    {
        const auto steps = proc.getSequencerStepSnapshot();
        if (! juce::isPositiveAndBelow(stepIndex, (int) steps.size()))
            return RootFlow::textMuted;

        const auto& step = steps[(size_t) stepIndex];
        if (! step.active)
            return RootFlow::textMuted;

        auto tint = juce::Colour::fromHSV(0.38f + step.velocity * 0.12f, 0.95f, 1.0f, 1.0f);
        if (stepIndex == getCurrentVisibleStep())
            tint = tint.brighter(0.4f);

        return tint;
    }

    int stepAt(juce::Point<float> p) const
    {
        auto padArea = getPadArea();
        const float cellW = padArea.getWidth() / 9.0f;

        for (int i = 0; i < 16; ++i)
        {
            auto centre = cellCentre(i, padArea);
            if (p.getDistanceFrom(centre) < juce::jmin(cellW * 0.28f, padArea.getHeight() * 0.24f))
                return i;
        }
        return -1;
    }

    static juce::Point<float> cellCentre(int index, juce::Rectangle<float> padArea)
    {
        const float cellW = padArea.getWidth() / 9.0f;
        const bool topRow = index < 8;
        const int column = topRow ? index : index - 8;
        const float arch = std::abs(((float) column - 3.5f) / 3.5f);
        const float x = padArea.getX() + (topRow ? (column + 0.78f) : (column + 1.22f)) * cellW;
        const float y = padArea.getY() + padArea.getHeight() * (topRow ? (0.38f - (1.0f - arch) * 0.04f)
                                                                        : (0.74f - (1.0f - arch) * 0.02f));
        return { x, y };
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BioSequencerComponent)
};
