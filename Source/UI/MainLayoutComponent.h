#pragma once
#include <JuceHeader.h>
#include "RootPanel.h"
#include "PulsePanel.h"
#include "CenterComponent.h"
#include "BottomPanel.h"
#include "BioSequencerComponent.h"
#include "Utils/DesignTokens.h"

class RootFlowAudioProcessor;

class MainLayoutComponent : public juce::Component, private juce::Timer
{
public:
    MainLayoutComponent(RootFlowAudioProcessor& p)
        : rootPanel(), pulsePanel(), centerComponent(), bottomPanel(), bioSeq(p)
    {
        titleLabel.setText("FUNKY MOOSE", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(juce::FontOptions(34.0f).withStyle("Light")).withExtraKerningFactor(0.22f));
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setColour(juce::Label::textColourId, RootFlow::text);

        // Bioluminescent Glow Effect for Title
        auto* glow = new juce::GlowEffect();
        glow->setGlowProperties(10.0f, RootFlow::accent.withAlpha(0.34f));
        titleLabel.setComponentEffect(glow);

        addAndMakeVisible(titleLabel);

        // --- EVOLUTIONARY MUTATE BUTTON ---
        mutateButton.setButtonText("");
        mutateButton.setTooltip("Mutate the Plant's Essence");
        mutateButton.onClick = [&p] { p.mutatePlant(); };

        // Spore-like styling
        mutateButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        mutateButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(mutateButton);

        addAndMakeVisible(masterVolumeSlider);
        masterVolumeSlider.setPopupDisplayEnabled(true, true, this);
        masterVolumeSlider.setTextValueSuffix(" dB");

        addAndMakeVisible(masterCompressorSlider);
        masterCompressorSlider.setPopupDisplayEnabled(true, true, this);
        masterCompressorSlider.setTextValueSuffix(" Amt");

        addAndMakeVisible(volLabel);
        volLabel.setJustificationType(juce::Justification::centred);
        volLabel.setColour(juce::Label::textColourId, RootFlow::accentSoft);
        volLabel.setFont(juce::Font(juce::FontOptions(10.0f)));

        addAndMakeVisible(compLabel);
        compLabel.setJustificationType(juce::Justification::centred);
        compLabel.setColour(juce::Label::textColourId, RootFlow::accentSoft);
        compLabel.setFont(juce::Font(juce::FontOptions(10.0f)));

        volAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.tree, "masterVolume", masterVolumeSlider);
        compAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.tree, "masterCompressor", masterCompressorSlider);

        addAndMakeVisible(rootPanel);
        addAndMakeVisible(pulsePanel);
        addAndMakeVisible(centerComponent);
        addAndMakeVisible(bottomPanel);
        addAndMakeVisible(bioSeq);

        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        RootFlow::fillBackdrop(g, bounds);

        // Add the Mycelium Network for that deep organic "Bio-Web" look
        RootFlow::drawMyceliumNetwork(g, bounds, RootFlow::accentSoft, 0.08f, 1337);
        RootFlow::drawMyceliumNetwork(g, bounds, RootFlow::violet, 0.05f, 42);
        RootFlow::drawMyceliumNetwork(g, bounds, RootFlow::accent, 0.04f, 999);

        auto shell = bounds.reduced(14.0f, 8.0f).withTrimmedTop(10.0f);
        RootFlow::drawGlassPanel(g, shell, 26.0f, 1.2f);

        const bool idleEffectsEnabled = RootFlow::areIdleEffectsEnabled();
        const float idleBreath = idleEffectsEnabled ? (0.5f + 0.5f * std::sin(pulsePhase * juce::MathConstants<float>::twoPi))
                                                    : 0.0f;
        const auto systemTint = getSystemTint();
        if (idleEffectsEnabled)
        {
            juce::ColourGradient stageBreath(RootFlow::accentSoft.withAlpha(0.022f + idleBreath * 0.016f),
                                             shell.getCentreX(), shell.getY() + shell.getHeight() * 0.22f,
                                             juce::Colours::transparentBlack,
                                             shell.getCentreX(), shell.getBottom(), true);
            g.setGradientFill(stageBreath);
            g.fillEllipse(shell.getCentreX() - shell.getWidth() * 0.22f,
                          shell.getY() + shell.getHeight() * 0.12f,
                          shell.getWidth() * 0.44f,
                          shell.getHeight() * 0.62f);
        }

        g.setColour(RootFlow::textMuted.withAlpha(0.028f + idleBreath * 0.018f));
        g.drawRoundedRectangle(shell.reduced(8.0f, 6.0f), 22.0f, 1.0f);

        auto titleCanopy = titleLabel.getBounds().toFloat().expanded(26.0f, 8.0f).translated(0.0f, 1.0f);
        RootFlow::drawGlassPanel(g, titleCanopy, titleCanopy.getHeight() * 0.5f, 0.46f + idleBreath * 0.07f);
        g.setColour(systemTint.withAlpha(0.022f + idleBreath * 0.018f));
        g.fillRoundedRectangle(titleCanopy.reduced(2.0f), titleCanopy.getHeight() * 0.42f);

        juce::Path titleSheen;
        titleSheen.startNewSubPath(titleCanopy.getX() + titleCanopy.getWidth() * 0.12f, titleCanopy.getBottom() - 3.0f);
        titleSheen.cubicTo(titleCanopy.getX() + titleCanopy.getWidth() * 0.24f, titleCanopy.getY() + titleCanopy.getHeight() * 0.08f,
                           titleCanopy.getRight() - titleCanopy.getWidth() * 0.24f, titleCanopy.getY() + titleCanopy.getHeight() * 0.08f,
                           titleCanopy.getRight() - titleCanopy.getWidth() * 0.12f, titleCanopy.getBottom() - 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.028f + idleBreath * 0.016f));
        g.strokePath(titleSheen, juce::PathStrokeType(0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        RootFlow::drawGlowOrb(g, { titleCanopy.getX() + 20.0f, titleCanopy.getCentreY() }, 2.8f, RootFlow::accentSoft, 0.24f + idleBreath * 0.08f);
        RootFlow::drawGlowOrb(g, { titleCanopy.getRight() - 20.0f, titleCanopy.getCentreY() }, 2.8f, RootFlow::amber, 0.20f + idleBreath * 0.07f);

        juce::Path bridge;
        auto bridgeY = shell.getBottom() - 126.0f;
        bridge.startNewSubPath(shell.getX() + 120.0f, bridgeY);
        bridge.cubicTo(shell.getX() + 240.0f, shell.getBottom() - 44.0f,
                       shell.getRight() - 240.0f, shell.getBottom() - 44.0f,
                       shell.getRight() - 120.0f, bridgeY);
        g.setColour(RootFlow::accent.withAlpha(0.06f));
        g.strokePath(bridge, juce::PathStrokeType(11.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (! rootPanel.getBounds().isEmpty() && ! pulsePanel.getBounds().isEmpty())
        {
                if (idleEffectsEnabled)
                {
                    RootFlow::drawBioThread(g,
                                            titleCanopy.getBottomLeft().translated(42.0f, -4.0f),
                                            rootPanel.getBounds().toFloat().getTopRight().translated(-28.0f, 34.0f),
                                            RootFlow::accentSoft, 0.038f + idleBreath * 0.020f, 0.9f);
                    RootFlow::drawBioThread(g,
                                            titleCanopy.getBottomRight().translated(-42.0f, -4.0f),
                                            pulsePanel.getBounds().toFloat().getTopLeft().translated(28.0f, 34.0f),
                                            RootFlow::amber, 0.034f + idleBreath * 0.018f, 0.9f);
                }
                RootFlow::drawBioThread(g,
                                        rootPanel.getBounds().toFloat().getTopRight().translated(-22.0f, 78.0f),
                                        centerComponent.getBounds().toFloat().getTopLeft().translated(44.0f, 140.0f),
                                        RootFlow::accentSoft, 0.06f, 1.0f);
            RootFlow::drawBioThread(g,
                                    pulsePanel.getBounds().toFloat().getTopLeft().translated(22.0f, 78.0f),
                                    centerComponent.getBounds().toFloat().getTopRight().translated(-44.0f, 140.0f),
                                    RootFlow::accent, 0.06f, 1.0f);
            RootFlow::drawBioThread(g,
                                    bottomPanel.getBounds().toFloat().getCentre().translated(0.0f, -12.0f),
                                    centerComponent.getBounds().toFloat().getCentre().translated(0.0f, 160.0f),
                                    RootFlow::accent.withAlpha(0.70f), 0.055f, 1.1f);
        }

        auto mutateArea = mutateButton.getBounds().toFloat().expanded(3.0f);
        const bool mutateHovered = RootFlow::areHoverEffectsEnabled() && mutateButton.isMouseOverOrDragging();
        const bool mutatePressed = mutateButton.isMouseButtonDown();
        const float mutateGlow = mutatePressed ? 0.96f : (mutateHovered ? 0.78f : (0.56f + idleBreath * 0.08f));
        RootFlow::drawGlassPanel(g, mutateArea, mutateArea.getWidth() * 0.48f, mutateGlow);
        g.setColour(RootFlow::amber.withAlpha(mutatePressed ? 0.14f : (mutateHovered ? 0.10f : 0.06f + idleBreath * 0.02f)));
        g.fillEllipse(mutateArea.reduced(3.0f));
        g.setColour(juce::Colours::white.withAlpha(mutateHovered ? 0.10f : 0.05f));
        g.drawEllipse(mutateArea.reduced(4.0f), 0.8f);

        const auto mutateCentre = mutateArea.getCentre();
        juce::Path mutateGlyph;
        mutateGlyph.startNewSubPath(mutateCentre.x, mutateCentre.y - 6.0f);
        mutateGlyph.lineTo(mutateCentre.x - 5.4f, mutateCentre.y + 3.8f);
        mutateGlyph.lineTo(mutateCentre.x + 5.4f, mutateCentre.y + 3.8f);
        mutateGlyph.closeSubPath();
        g.setColour(RootFlow::amber.withAlpha(mutatePressed ? 0.92f : 0.74f));
        g.fillPath(mutateGlyph);

        g.setColour(RootFlow::accentSoft.withAlpha(mutateHovered ? 0.60f : 0.38f));
        g.drawLine(mutateCentre.x, mutateCentre.y - 4.4f, mutateCentre.x, mutateCentre.y + 6.0f, 1.1f);
        RootFlow::drawGlowOrb(g, { mutateCentre.x, mutateCentre.y - 6.0f }, 2.1f, RootFlow::amber, mutatePressed ? 0.94f : 0.72f);
        RootFlow::drawGlowOrb(g, { mutateCentre.x - 5.2f, mutateCentre.y + 4.0f }, 1.7f, RootFlow::accentSoft, mutateHovered ? 0.74f : 0.48f);
        RootFlow::drawGlowOrb(g, { mutateCentre.x + 5.2f, mutateCentre.y + 4.0f }, 1.7f, RootFlow::accent, mutateHovered ? 0.70f : 0.44f);

        auto mutateCaption = juce::Rectangle<float>(72.0f, 12.0f).withCentre({ mutateArea.getCentreX(), mutateArea.getBottom() + 10.0f });
        g.setFont(RootFlow::getFont(8.2f).boldened());
        g.setColour(RootFlow::amber.withAlpha(mutateHovered ? 0.88f : 0.64f));
        g.drawFittedText(mutatePressed ? "MUTATING" : "EVOLVE", mutateCaption.toNearestInt(), juce::Justification::centred, 1);

        auto subtitleArea = titleLabel.getBounds().toFloat().translated(0.0f, 24.0f).withHeight(16.0f);
        g.setColour(RootFlow::textMuted.withAlpha(0.68f));
        g.setFont(RootFlow::getFont(11.2f));
        g.drawFittedText("ROOTFLOW SYNTH", subtitleArea.toNearestInt(), juce::Justification::centred, 1);

        RootFlow::drawGlowOrb(g, { shell.getX() + 30.0f, shell.getY() + 26.0f }, 7.0f, RootFlow::accent, 0.8f);
        RootFlow::drawGlowOrb(g, { shell.getRight() - 30.0f, shell.getY() + 26.0f }, 7.0f, RootFlow::amber, 0.7f);
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        constexpr float sideTabLift = -10.0f;
        constexpr float seqTabLift = -18.0f;

        auto seqTab = bioSeq.getBounds().toFloat().withTrimmedBottom((float) bioSeq.getHeight() - 28.0f)
                                           .translated(0.0f, seqTabLift)
                                           .withTrimmedLeft(24.0f)
                                           .withTrimmedRight(24.0f);
        auto rootTab = rootPanel.getBounds().toFloat().withHeight(30.0f).translated(18.0f, sideTabLift);
        auto pulseTab = pulsePanel.getBounds().toFloat().withHeight(30.0f).translated(-18.0f, sideTabLift);
        auto coreTab = centerComponent.getBounds().toFloat().withHeight(30.0f).translated(0.0f, sideTabLift).withTrimmedLeft(140.0f).withTrimmedRight(140.0f);
        auto rootTabArea = rootTab.withWidth(152.0f);
        auto seqTabArea = seqTab.withWidth(324.0f).withCentre(seqTab.getCentre());
        auto coreTabArea = coreTab.withWidth(220.0f).withCentre(coreTab.getCentre());
        auto pulseTabArea = juce::Rectangle<float>(pulseTab.getRight() - 168.0f, pulseTab.getY(), 168.0f, pulseTab.getHeight());
        auto ambientBand = bottomPanel.getBounds().toFloat().reduced(52.0f, 10.0f).withHeight(24.0f);

        RootFlow::drawTabLabel(g, rootTabArea, "ROOT FIELD");
        RootFlow::drawTabLabel(g, seqTabArea, "BIO-SEQUENCER");
        RootFlow::drawTabLabel(g, coreTabArea, "CENTER PANEL");
        RootFlow::drawTabLabel(g, pulseTabArea, "PULSE FIELD");

        struct FocusSnapshot
        {
            enum class Zone { root, center, pulse, sequencer, ambient };

            bool active = false;
            Zone zone = Zone::center;
            juce::Point<float> anchor;
            juce::Colour tint;
            juce::String section;
            juce::String value;
        };

        FocusSnapshot focus;

        if (centerComponent.getFocusedSlider() != nullptr)
        {
            focus.active = true;
            focus.zone = FocusSnapshot::Zone::center;
            focus.anchor = centerComponent.getFocusAnchor();
            focus.tint = centerComponent.getFocusTint();
            focus.section = "CENTER CURRENT";
            focus.value = centerComponent.getFocusTitle() + " / " + centerComponent.getFocusValueText();
        }
        else if (rootPanel.getFocusedSlider() != nullptr)
        {
            focus.active = true;
            focus.zone = FocusSnapshot::Zone::root;
            focus.anchor = rootPanel.getFocusAnchor();
            focus.tint = rootPanel.getFocusTint();
            focus.section = "ROOT CURRENT";
            focus.value = rootPanel.getFocusTitle() + " / " + rootPanel.getFocusValueText();
        }
        else if (pulsePanel.getFocusedSlider() != nullptr)
        {
            focus.active = true;
            focus.zone = FocusSnapshot::Zone::pulse;
            focus.anchor = pulsePanel.getFocusAnchor();
            focus.tint = pulsePanel.getFocusTint();
            focus.section = "PULSE CURRENT";
            focus.value = pulsePanel.getFocusTitle() + " / " + pulsePanel.getFocusValueText();
        }
        else if (bottomPanel.getFocusedSlider() != nullptr)
        {
            focus.active = true;
            focus.zone = FocusSnapshot::Zone::ambient;
            focus.anchor = bottomPanel.getFocusAnchor();
            focus.tint = bottomPanel.getFocusTint();
            focus.section = "AMBIENT CURRENT";
            focus.value = bottomPanel.getFocusTitle() + " / " + bottomPanel.getFocusValueText();
        }
        else if (bioSeq.hasActivityFocus())
        {
            focus.active = true;
            focus.zone = FocusSnapshot::Zone::sequencer;
            focus.anchor = bioSeq.getFocusAnchor();
            focus.tint = bioSeq.getFocusTint();
            focus.section = "SEQUENCER CURRENT";
            focus.value = bioSeq.getFocusTitle() + " / " + bioSeq.getFocusValueText();
        }

        auto rootAnchor = rootPanel.getFocusAnchor();
        auto centerAnchor = centerComponent.getFocusAnchor();
        auto pulseAnchor = pulsePanel.getFocusAnchor();
        auto ambientAnchor = bottomPanel.getFocusAnchor();
        auto seqAnchor = bioSeq.getFocusAnchor();
        const bool popupOverlaysEnabled = RootFlow::arePopupOverlaysEnabled();

        if (! focus.active)
        {
            if (! RootFlow::areIdleEffectsEnabled())
                return;

            const float idleBreath = 0.5f + 0.5f * std::sin((pulsePhase + 0.12f) * juce::MathConstants<float>::twoPi);
            auto drawIdleMarker = [&g, idleBreath] (juce::Rectangle<float> area, juce::Colour tint)
            {
                g.setColour(tint.withAlpha(0.016f + idleBreath * 0.012f));
                g.fillRoundedRectangle(area.expanded(3.0f, 1.0f), area.getHeight() * 0.5f);
                g.setColour(tint.withAlpha(0.040f + idleBreath * 0.024f));
                g.drawRoundedRectangle(area.expanded(2.0f, 1.0f), area.getHeight() * 0.5f + 1.0f, 0.8f);
            };

            const auto idleTint = RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.42f);
            drawIdleMarker(rootTabArea, RootFlow::accentSoft);
            drawIdleMarker(seqTabArea, RootFlow::amber.interpolatedWith(RootFlow::accentSoft, 0.18f));
            drawIdleMarker(coreTabArea, idleTint);
            drawIdleMarker(pulseTabArea, RootFlow::accent);
            drawIdleMarker(ambientBand, RootFlow::accent.interpolatedWith(RootFlow::amber, 0.22f));

            if (popupOverlaysEnabled)
            {
                auto idleBubble = juce::Rectangle<float>(184.0f, 22.0f).withCentre({ coreTabArea.getCentreX(), coreTabArea.getY() - 16.0f });
                idleBubble.setX(juce::jlimit(30.0f, (float) getWidth() - idleBubble.getWidth() - 30.0f, idleBubble.getX()));
                idleBubble.setY(juce::jlimit(26.0f, (float) getHeight() - idleBubble.getHeight() - 26.0f, idleBubble.getY()));
                drawStageActivityBubble(g, idleBubble, "SYSTEM", "ORGANIC IDLE", idleTint);
            }

            drawActivityThread(g, rootAnchor, centerAnchor, RootFlow::accentSoft, 0.24f + idleBreath * 0.08f, 0.00f);
            drawActivityThread(g, pulseAnchor, centerAnchor, RootFlow::accent, 0.24f + idleBreath * 0.08f, 0.32f);
            drawActivityThread(g, ambientAnchor, centerAnchor, RootFlow::accent.interpolatedWith(RootFlow::amber, 0.18f), 0.20f + idleBreath * 0.06f, 0.56f);
            drawActivityThread(g, seqAnchor, centerAnchor, RootFlow::amber.interpolatedWith(RootFlow::accentSoft, 0.22f), 0.22f + idleBreath * 0.07f, 0.82f);
            RootFlow::drawGlowOrb(g, centerAnchor, 3.8f + idleBreath * 0.6f, idleTint, 0.16f + idleBreath * 0.08f);
            return;
        }

        auto activeTab = focus.zone == FocusSnapshot::Zone::root ? rootTabArea
                       : (focus.zone == FocusSnapshot::Zone::pulse ? pulseTabArea
                       : (focus.zone == FocusSnapshot::Zone::sequencer ? seqTabArea
                       : (focus.zone == FocusSnapshot::Zone::ambient ? ambientBand
                                                                       : coreTabArea)));

        g.setColour(focus.tint.withAlpha(0.055f));
        g.fillRoundedRectangle(activeTab.expanded(5.0f, 3.0f), activeTab.getHeight() * 0.5f);
        g.setColour(focus.tint.withAlpha(0.16f));
        g.drawRoundedRectangle(activeTab.expanded(4.0f, 2.0f), activeTab.getHeight() * 0.5f + 1.0f, 0.9f);

        const float bubbleCenterX = focus.zone == FocusSnapshot::Zone::sequencer ? seqTabArea.getCentreX()
                                 : (focus.zone == FocusSnapshot::Zone::ambient ? ambientBand.getCentreX()
                                                                               : coreTabArea.getCentreX());
        const float bubbleCenterY = focus.zone == FocusSnapshot::Zone::sequencer ? seqTabArea.getBottom() + 18.0f
                                 : (focus.zone == FocusSnapshot::Zone::ambient ? ambientBand.getY() - 16.0f
                                                                               : coreTabArea.getY() - 16.0f);
        if (popupOverlaysEnabled)
        {
            auto bubble = juce::Rectangle<float>(216.0f, 24.0f)
                              .withCentre({ bubbleCenterX, bubbleCenterY });
            bubble.setX(juce::jlimit(30.0f, (float) getWidth() - bubble.getWidth() - 30.0f, bubble.getX()));
            bubble.setY(juce::jlimit(26.0f, (float) getHeight() - bubble.getHeight() - 26.0f, bubble.getY()));
            drawStageActivityBubble(g, bubble, focus.section, focus.value, focus.tint);
        }

        RootFlow::drawGlowOrb(g, focus.anchor, 4.2f, focus.tint, 0.48f);
        RootFlow::drawGlowOrb(g, centerAnchor, 3.8f, focus.tint, 0.32f);

        if (focus.zone == FocusSnapshot::Zone::root)
        {
            drawActivityThread(g, focus.anchor, centerAnchor, focus.tint, 1.0f, 0.00f);
            drawActivityThread(g, centerAnchor, pulseAnchor, focus.tint.interpolatedWith(RootFlow::accent, 0.25f), 0.62f, 0.34f);
        }
        else if (focus.zone == FocusSnapshot::Zone::pulse)
        {
            drawActivityThread(g, focus.anchor, centerAnchor, focus.tint, 1.0f, 0.00f);
            drawActivityThread(g, centerAnchor, rootAnchor, focus.tint.interpolatedWith(RootFlow::accentSoft, 0.28f), 0.62f, 0.34f);
        }
        else if (focus.zone == FocusSnapshot::Zone::sequencer)
        {
            drawActivityThread(g, focus.anchor, centerAnchor, focus.tint, 1.0f, 0.00f);
            drawActivityThread(g, centerAnchor, rootAnchor, focus.tint.interpolatedWith(RootFlow::accentSoft, 0.24f), 0.48f, 0.22f);
            drawActivityThread(g, centerAnchor, pulseAnchor, focus.tint.interpolatedWith(RootFlow::amber, 0.20f), 0.48f, 0.54f);
        }
        else if (focus.zone == FocusSnapshot::Zone::ambient)
        {
            drawActivityThread(g, focus.anchor, centerAnchor, focus.tint, 0.94f, 0.00f);
            drawActivityThread(g, centerAnchor, rootAnchor, focus.tint.interpolatedWith(RootFlow::accentSoft, 0.18f), 0.40f, 0.28f);
            drawActivityThread(g, centerAnchor, pulseAnchor, focus.tint.interpolatedWith(RootFlow::amber, 0.18f), 0.40f, 0.56f);
            RootFlow::drawGlowOrb(g, ambientAnchor, 3.8f, focus.tint, 0.36f);
        }
        else
        {
            drawActivityThread(g, focus.anchor, rootAnchor, focus.tint.interpolatedWith(RootFlow::accentSoft, 0.18f), 0.88f, 0.00f);
            drawActivityThread(g, focus.anchor, pulseAnchor, focus.tint.interpolatedWith(RootFlow::amber, 0.18f), 0.88f, 0.42f);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(22, 12);

        auto headerArea = bounds.removeFromTop(juce::jlimit(66, 86, juce::roundToInt(getHeight() * 0.085f)));
        auto brandArea = headerArea.removeFromTop(44);
        const int mutateSize = juce::jlimit(30, 38, brandArea.getHeight() - 4);
        auto mutateArea = brandArea.removeFromRight(mutateSize + 8);
        mutateButton.setBounds(mutateArea.withSizeKeepingCentre(mutateSize, mutateSize));
        titleLabel.setBounds(brandArea.reduced(mutateSize + 18, 0));

        auto seqArea = bounds.removeFromTop(juce::jlimit(162, 214, juce::roundToInt(getHeight() * 0.215f)));
        const int seqWidth = juce::jlimit(620, 1040, juce::roundToInt(bounds.getWidth() * 0.78f));
        auto seqRect = seqArea.withSizeKeepingCentre(seqWidth, seqArea.getHeight() - 2).translated(0, 4);
        bioSeq.setBounds(seqRect);

        // Position Volume and Compressor side-by-side to the right of the Sequencer, matching its exact height
        auto fxArea = seqRect.withLeft(seqRect.getRight() + 16).withRight(seqRect.getRight() + 88);

        auto volArea = fxArea.removeFromLeft(fxArea.getWidth() / 2).reduced(2, 0);
        volLabel.setBounds(volArea.removeFromTop(14));
        masterVolumeSlider.setBounds(volArea);

        auto compArea = fxArea.reduced(2, 0);
        compLabel.setBounds(compArea.removeFromTop(14));
        masterCompressorSlider.setBounds(compArea);

        bounds.removeFromTop(18);

        auto bottom = bounds.removeFromBottom(juce::jlimit(112, 138, juce::roundToInt(getHeight() * 0.13f)));
        const int sideWidth = juce::jlimit(176, 274, juce::roundToInt(bounds.getWidth() * 0.22f));
        const int centerInset = juce::jlimit(6, 18, juce::roundToInt(bounds.getWidth() * 0.012f));
        const int bottomInset = juce::jlimit(18, 72, juce::roundToInt(bounds.getWidth() * 0.07f));

        auto left  = bounds.removeFromLeft(sideWidth).reduced(2, 0);
        auto right = bounds.removeFromRight(sideWidth).reduced(2, 0);

        rootPanel.setBounds(left.translated(-2, 0));
        pulsePanel.setBounds(right.translated(2, 0));
        centerComponent.setBounds(bounds.reduced(centerInset, 4));
        bottomPanel.setBounds(bottom.reduced(bottomInset, 0));
    }

    RootPanel&             getRootPanel()       { return rootPanel; }
    PulsePanel&            getPulsePanel()       { return pulsePanel; }
    CenterComponent&       getCenterComponent()  { return centerComponent; }
    BottomPanel&           getBottomPanel()      { return bottomPanel; }
    BioSequencerComponent& getBioSeq()           { return bioSeq; }

private:
    juce::Colour getSystemTint() const
    {
        if (centerComponent.getFocusedSlider() != nullptr)
            return centerComponent.getFocusTint();
        if (rootPanel.getFocusedSlider() != nullptr)
            return rootPanel.getFocusTint();
        if (pulsePanel.getFocusedSlider() != nullptr)
            return pulsePanel.getFocusTint();
        if (bottomPanel.getFocusedSlider() != nullptr)
            return bottomPanel.getFocusTint();
        if (bioSeq.hasActivityFocus())
            return bioSeq.getFocusTint();

        return RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.42f);
    }

    static juce::Point<float> interpolatePoint(juce::Point<float> a, juce::Point<float> b, float t)
    {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
    }

    static juce::Point<float> pointOnCubic(juce::Point<float> p0,
                                           juce::Point<float> p1,
                                           juce::Point<float> p2,
                                           juce::Point<float> p3,
                                           float t)
    {
        const auto ab = interpolatePoint(p0, p1, t);
        const auto bc = interpolatePoint(p1, p2, t);
        const auto cd = interpolatePoint(p2, p3, t);
        const auto abbc = interpolatePoint(ab, bc, t);
        const auto bccd = interpolatePoint(bc, cd, t);
        return interpolatePoint(abbc, bccd, t);
    }

    void drawActivityThread(juce::Graphics& g,
                            juce::Point<float> start,
                            juce::Point<float> end,
                            juce::Colour tint,
                            float strength,
                            float phaseOffset) const
    {
        const float verticalBias = end.y < start.y ? -52.0f : 40.0f;
        const auto controlA = juce::Point<float>(start.x + (end.x - start.x) * 0.28f,
                                                 start.y + verticalBias);
        const auto controlB = juce::Point<float>(start.x + (end.x - start.x) * 0.72f,
                                                 end.y - verticalBias * 0.70f);

        juce::Path stream;
        stream.startNewSubPath(start);
        stream.cubicTo(controlA.x, controlA.y,
                       controlB.x, controlB.y,
                       end.x, end.y);

        g.setColour(tint.withAlpha(0.035f * strength));
        g.strokePath(stream, juce::PathStrokeType((2.8f + 2.2f * strength),
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour(tint.withAlpha(0.13f * strength));
        g.strokePath(stream, juce::PathStrokeType((0.8f + 0.42f * strength),
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        auto phase = std::fmod(pulsePhase + phaseOffset, 1.0f);
        if (phase < 0.0f)
            phase += 1.0f;

        const auto lead = pointOnCubic(start, controlA, controlB, end, phase);
        const auto trail = pointOnCubic(start, controlA, controlB, end, std::fmod(phase + 0.22f, 1.0f));
        RootFlow::drawGlowOrb(g, lead, 3.3f + 0.9f * strength, tint, 0.42f * strength);
        RootFlow::drawGlowOrb(g, trail, 2.2f + 0.6f * strength, tint.interpolatedWith(juce::Colours::white, 0.12f), 0.16f * strength);
    }

    void drawStageActivityBubble(juce::Graphics& g,
                                 juce::Rectangle<float> area,
                                 const juce::String& section,
                                 const juce::String& value,
                                 juce::Colour tint) const
    {
        RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.5f, 0.72f);
        g.setColour(tint.withAlpha(0.07f));
        g.fillRoundedRectangle(area.reduced(2.0f), area.getHeight() * 0.42f);

        auto inner = area.reduced(9.0f, 4.0f);
        auto sectionArea = inner.removeFromTop(7.0f);
        g.setFont(RootFlow::getFont(7.0f).boldened());
        g.setColour(RootFlow::textMuted.withAlpha(0.86f));
        g.drawText(section, sectionArea, juce::Justification::centredLeft, false);

        g.setFont(RootFlow::getFont(9.2f).boldened());
        g.setColour(RootFlow::text.withAlpha(0.94f));
        g.drawFittedText(value, inner.toNearestInt(), juce::Justification::centredLeft, 1);
        RootFlow::drawGlowOrb(g, { area.getRight() - 9.0f, area.getCentreY() }, 2.4f, tint, 0.46f);
    }

    void timerCallback() override
    {
        pulsePhase = std::fmod(pulsePhase + 0.014f, 1.0f);
        repaint();
    }

    juce::Label           titleLabel;
    juce::TextButton      mutateButton;
    RootPanel             rootPanel;
    PulsePanel            pulsePanel;
    CenterComponent       centerComponent;
    BottomPanel           bottomPanel;
    BioSequencerComponent bioSeq;
    juce::Slider masterVolumeSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider masterCompressorSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compAttach;
    juce::Label volLabel { "Vol", "VOL" };
    juce::Label compLabel { "Comp", "COMP" };
    float                 pulsePhase = 0.0f;
};
