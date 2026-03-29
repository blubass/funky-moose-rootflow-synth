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
        titleLabel.setFont(juce::Font(juce::FontOptions(26.0f).withStyle("Light")).withExtraKerningFactor(0.18f));
        titleLabel.setJustificationType(juce::Justification::centredRight);
        titleLabel.setColour(juce::Label::textColourId, RootFlow::text);

        // Bioluminescent Glow Effect for Title
        auto* glow = new juce::GlowEffect();
        glow->setGlowProperties(7.0f, RootFlow::accent.withAlpha(0.22f));
        titleLabel.setComponentEffect(glow);

        addAndMakeVisible(titleLabel);

        rootflowLabel.setText("ROOTFLOW SYNTH", juce::dontSendNotification);
        rootflowLabel.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("SemiBold")).withExtraKerningFactor(0.24f));
        rootflowLabel.setJustificationType(juce::Justification::centredLeft);
        rootflowLabel.setColour(juce::Label::textColourId,
                                RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.24f));

        auto* rootflowGlow = new juce::GlowEffect();
        rootflowGlow->setGlowProperties(13.5f, RootFlow::accent.withAlpha(0.56f));
        rootflowLabel.setComponentEffect(rootflowGlow);

        addAndMakeVisible(rootflowLabel);

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
        masterVolumeSlider.getProperties().set("rootflowStyle", "side-vertical");
        masterVolumeSlider.setName("VOLUME");

        addAndMakeVisible(masterMixSlider);
        masterMixSlider.setPopupDisplayEnabled(true, true, this);
        masterMixSlider.textFromValueFunction = [] (double value)
        {
            return juce::String(juce::roundToInt(value * 100.0)) + " %";
        };
        masterMixSlider.valueFromTextFunction = [] (const juce::String& text)
        {
            const auto numeric = text.upToFirstOccurrenceOf("%", false, false).trim().getDoubleValue();
            return juce::jlimit(0.0, 1.0, numeric / 100.0);
        };
        masterMixSlider.getProperties().set("rootflowStyle", "side-vertical");
        masterMixSlider.setName("MIX");

        addAndMakeVisible(monoMakerFreqSlider);
        monoMakerFreqSlider.setPopupDisplayEnabled(true, true, this);
        monoMakerFreqSlider.textFromValueFunction = [] (double value)
        {
            return juce::String(juce::roundToInt(value)) + " Hz";
        };
        monoMakerFreqSlider.valueFromTextFunction = [] (const juce::String& text)
        {
            return juce::jlimit(20.0, 400.0, text.getDoubleValue());
        };
        monoMakerFreqSlider.getProperties().set("rootflowStyle", "side-vertical");
        monoMakerFreqSlider.setName("FREQUENCY");

        addAndMakeVisible(monoMakerToggle);
        monoMakerToggle.setButtonText("MONO");

        addAndMakeVisible(masterCompressorSlider);
        masterCompressorSlider.setPopupDisplayEnabled(true, true, this);
        masterCompressorSlider.setTextValueSuffix(" Amt");
        masterCompressorSlider.getProperties().set("rootflowStyle", "side-vertical");
        masterCompressorSlider.setName("COMPRESSOR");

        addAndMakeVisible(volLabel);
        volLabel.setJustificationType(juce::Justification::centred);
        volLabel.setColour(juce::Label::textColourId, RootFlow::accentSoft);
        volLabel.setFont(juce::Font(juce::FontOptions(9.0f)));

        addAndMakeVisible(mixLabel);
        mixLabel.setJustificationType(juce::Justification::centred);
        mixLabel.setColour(juce::Label::textColourId, RootFlow::accentSoft);
        mixLabel.setFont(juce::Font(juce::FontOptions(9.0f)));

        addAndMakeVisible(freqLabel);
        freqLabel.setJustificationType(juce::Justification::centred);
        freqLabel.setColour(juce::Label::textColourId, RootFlow::accentSoft);
        freqLabel.setFont(juce::Font(juce::FontOptions(9.0f)));

        addAndMakeVisible(compLabel);
        compLabel.setJustificationType(juce::Justification::centred);
        compLabel.setColour(juce::Label::textColourId, RootFlow::accentSoft);
        compLabel.setFont(juce::Font(juce::FontOptions(9.0f)));

        volAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.tree, "masterVolume", masterVolumeSlider);
        mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.tree, "masterMix", masterMixSlider);
        monoFreqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.tree, "monoMakerFreq", monoMakerFreqSlider);
        monoToggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.tree, "monoMakerToggle", monoMakerToggle);
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

        auto titleCanopy = titleLabel.getBounds()
                              .getUnion(rootflowLabel.getBounds())
                              .toFloat()
                              .expanded(24.0f, 8.0f)
                              .translated(0.0f, 1.0f);
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

        const auto rootflowBounds = rootflowLabel.getBounds().toFloat();
        juce::ColourGradient rootflowAura(RootFlow::accent.withAlpha(0.18f + idleBreath * 0.06f),
                                          rootflowBounds.getCentreX(), rootflowBounds.getCentreY(),
                                          RootFlow::violet.withAlpha(0.12f + idleBreath * 0.05f),
                                          rootflowBounds.getX(), rootflowBounds.getBottom(), true);
        g.setGradientFill(rootflowAura);
        g.fillRoundedRectangle(rootflowBounds.expanded(12.0f, 6.0f), rootflowBounds.getHeight() * 0.54f);

        const auto dividerX = (float) ((titleLabel.getRight() + rootflowLabel.getX()) / 2);
        g.setColour(RootFlow::textMuted.withAlpha(0.18f + idleBreath * 0.06f));
        g.drawLine(dividerX, titleCanopy.getY() + 8.0f, dividerX, titleCanopy.getBottom() - 8.0f, 1.0f);

        RootFlow::drawGlowOrb(g, { titleCanopy.getX() + 20.0f, titleCanopy.getCentreY() }, 2.8f, RootFlow::accentSoft, 0.24f + idleBreath * 0.08f);
        RootFlow::drawGlowOrb(g, { titleCanopy.getRight() - 20.0f, titleCanopy.getCentreY() }, 2.8f, RootFlow::amber, 0.20f + idleBreath * 0.07f);
        RootFlow::drawGlowOrb(g, { rootflowBounds.getX() + 8.0f, rootflowBounds.getCentreY() }, 3.2f, RootFlow::accent, 0.34f + idleBreath * 0.10f);
        RootFlow::drawGlowOrb(g, { rootflowBounds.getRight() - 8.0f, rootflowBounds.getCentreY() }, 2.8f, RootFlow::violet, 0.26f + idleBreath * 0.08f);

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
        auto masterSectionBounds = volLabel.getBounds()
                                       .getUnion(masterVolumeSlider.getBounds())
                                       .getUnion(mixLabel.getBounds())
                                       .getUnion(masterMixSlider.getBounds())
                                       .getUnion(freqLabel.getBounds())
                                       .getUnion(monoMakerFreqSlider.getBounds())
                                       .getUnion(monoMakerToggle.getBounds())
                                       .getUnion(compLabel.getBounds())
                                       .getUnion(masterCompressorSlider.getBounds());
        if (! masterSectionBounds.isEmpty())
        {
            auto masterTabArea = juce::Rectangle<float>((float) masterSectionBounds.getX() + 8.0f,
                                                        seqTabArea.getY(),
                                                        juce::jmax(120.0f, (float) masterSectionBounds.getWidth() - 16.0f),
                                                        28.0f);
            RootFlow::drawTabLabel(g, masterTabArea, "MASTER SECTION");
        }

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
        const bool compactLayout = getWidth() < 1120 || getHeight() < 610;
        auto bounds = getLocalBounds().reduced(compactLayout ? 18 : 22,
                                               compactLayout ? 10 : 12);

        auto headerArea = bounds.removeFromTop(juce::jlimit(compactLayout ? 60 : 66,
                                                            compactLayout ? 80 : 86,
                                                            juce::roundToInt(getHeight() * (compactLayout ? 0.078f : 0.085f))));
        auto brandArea = headerArea.removeFromTop(compactLayout ? 40 : 44);
        const int mutateSize = juce::jlimit(compactLayout ? 28 : 30,
                                            compactLayout ? 36 : 38,
                                            brandArea.getHeight() - 4);
        auto mutateArea = brandArea.removeFromRight(mutateSize + (compactLayout ? 6 : 8));
        mutateButton.setBounds(mutateArea.withSizeKeepingCentre(mutateSize, mutateSize));
        titleLabel.setFont(juce::Font(juce::FontOptions(compactLayout ? 23.0f : 26.0f).withStyle("Light"))
                               .withExtraKerningFactor(compactLayout ? 0.16f : 0.18f));
        rootflowLabel.setFont(juce::Font(juce::FontOptions(compactLayout ? 15.0f : 18.0f).withStyle("SemiBold"))
                                  .withExtraKerningFactor(compactLayout ? 0.18f : 0.24f));

        auto titleBounds = brandArea.reduced(mutateSize + (compactLayout ? 14 : 18), 0)
                                   .withTrimmedBottom(compactLayout ? 10 : 8);
        const int clusterGap = compactLayout ? 14 : 20;
        const int rootflowWidth = juce::jlimit(compactLayout ? 170 : 190,
                                               compactLayout ? 240 : 280,
                                               juce::roundToInt(titleBounds.getWidth() * (compactLayout ? 0.34f : 0.31f)));
        auto titleCluster = titleBounds.withSizeKeepingCentre(titleBounds.getWidth(), titleBounds.getHeight());
        auto rootflowArea = titleCluster.removeFromRight(rootflowWidth);
        titleCluster.removeFromRight(clusterGap);
        titleLabel.setBounds(titleCluster);
        rootflowLabel.setBounds(rootflowArea);

        auto seqArea = bounds.removeFromTop(juce::jlimit(compactLayout ? 152 : 162,
                                                         compactLayout ? 198 : 214,
                                                         juce::roundToInt(getHeight() * (compactLayout ? 0.208f : 0.215f))));
        const int masterGap = compactLayout ? 16 : 22;
        const int masterWidth = juce::jlimit(compactLayout ? 208 : 224,
                                             compactLayout ? 288 : 320,
                                             juce::roundToInt(bounds.getWidth() * (compactLayout ? 0.26f : 0.28f)));
        const int seqMaxWidth = juce::jmax(compactLayout ? 548 : 580, bounds.getWidth() - masterWidth - masterGap);
        const int seqWidth = juce::jlimit(compactLayout ? 548 : 580,
                                          seqMaxWidth,
                                          juce::roundToInt(bounds.getWidth() * (compactLayout ? 0.64f : 0.66f)));
        const int topWidth = juce::jmin(bounds.getWidth(), seqWidth + masterGap + masterWidth);
        auto topRow = seqArea.withSizeKeepingCentre(topWidth, seqArea.getHeight() - (compactLayout ? 0 : 2))
                             .translated(0, compactLayout ? 2 : 4);
        auto seqRect = topRow.removeFromLeft(seqWidth);
        topRow.removeFromLeft(masterGap);
        bioSeq.setBounds(seqRect);

        auto masterArea = topRow;
        masterArea.removeFromTop(compactLayout ? 14 : 18);
        const int columnWidth = juce::jmax(compactLayout ? 44 : 46, masterArea.getWidth() / 4);

        auto volArea = masterArea.removeFromLeft(columnWidth).reduced(compactLayout ? 3 : 4, 0);
        volLabel.setBounds(volArea.removeFromTop(compactLayout ? 11 : 12));
        masterVolumeSlider.setBounds(volArea.withTrimmedTop(compactLayout ? 2 : 3)
                                           .withTrimmedBottom(compactLayout ? 4 : 6));

        auto mixArea = masterArea.removeFromLeft(columnWidth).reduced(compactLayout ? 3 : 4, 0);
        mixLabel.setBounds(mixArea.removeFromTop(compactLayout ? 11 : 12));
        masterMixSlider.setBounds(mixArea.withTrimmedTop(compactLayout ? 2 : 3)
                                         .withTrimmedBottom(compactLayout ? 4 : 6));

        auto freqArea = masterArea.removeFromLeft(columnWidth).reduced(compactLayout ? 3 : 4, 0);
        freqLabel.setBounds(freqArea.removeFromTop(compactLayout ? 11 : 12));
        freqArea.removeFromTop(compactLayout ? 2 : 3);
        auto toggleArea = freqArea.removeFromBottom(compactLayout ? 18 : 20);
        monoMakerFreqSlider.setBounds(freqArea.withTrimmedBottom(compactLayout ? 3 : 4));
        monoMakerToggle.setBounds(toggleArea.reduced(0, compactLayout ? 1 : 2));

        auto compArea = masterArea.reduced(compactLayout ? 3 : 4, 0);
        compLabel.setBounds(compArea.removeFromTop(compactLayout ? 11 : 12));
        masterCompressorSlider.setBounds(compArea.withTrimmedTop(compactLayout ? 2 : 3)
                                                 .withTrimmedBottom(compactLayout ? 4 : 6));

        bounds.removeFromTop(compactLayout ? 14 : 18);

        auto bottom = bounds.removeFromBottom(juce::jlimit(compactLayout ? 100 : 112,
                                                           compactLayout ? 128 : 138,
                                                           juce::roundToInt(getHeight() * (compactLayout ? 0.118f : 0.13f))));
        const int sideWidth = juce::jlimit(compactLayout ? 160 : 176,
                                           274,
                                           juce::roundToInt(bounds.getWidth() * (compactLayout ? 0.20f : 0.22f)));
        const int centerInset = juce::jlimit(compactLayout ? 4 : 6,
                                             compactLayout ? 14 : 18,
                                             juce::roundToInt(bounds.getWidth() * (compactLayout ? 0.010f : 0.012f)));
        const int bottomInset = juce::jlimit(compactLayout ? 14 : 18,
                                             compactLayout ? 54 : 72,
                                             juce::roundToInt(bounds.getWidth() * (compactLayout ? 0.05f : 0.07f)));

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
    juce::Label           rootflowLabel;
    juce::TextButton      mutateButton;
    RootPanel             rootPanel;
    PulsePanel            pulsePanel;
    CenterComponent       centerComponent;
    BottomPanel           bottomPanel;
    BioSequencerComponent bioSeq;
    juce::Slider masterVolumeSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider masterMixSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider monoMakerFreqSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::ToggleButton monoMakerToggle;
    juce::Slider masterCompressorSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> monoFreqAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoToggleAttach;
    juce::Label volLabel { "Vol", "VOL" };
    juce::Label mixLabel { "Mix", "MIX" };
    juce::Label compLabel { "Comp", "COMP" };
    juce::Label freqLabel { "Freq", "FREQ" };
    float                 pulsePhase = 0.0f;
};
