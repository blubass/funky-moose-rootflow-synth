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
        glow->setGlowProperties(3.2f, RootFlow::accent.withAlpha(0.08f));
        titleLabel.setComponentEffect(glow);

        addAndMakeVisible(titleLabel);

        rootflowLabel.setText("ROOTFLOW SYNTH", juce::dontSendNotification);
        rootflowLabel.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("SemiBold")).withExtraKerningFactor(0.24f));
        rootflowLabel.setJustificationType(juce::Justification::centredLeft);
        rootflowLabel.setColour(juce::Label::textColourId,
                                RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.24f));

        auto* rootflowGlow = new juce::GlowEffect();
        rootflowGlow->setGlowProperties(5.6f, RootFlow::accent.withAlpha(0.16f));
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

        auto styleMasterLabel = [] (juce::Label& label)
        {
            label.setJustificationType(juce::Justification::centred);
            label.setColour(juce::Label::textColourId,
                            RootFlow::textMuted.interpolatedWith(RootFlow::accentSoft, 0.22f).withAlpha(0.72f));
            label.setFont(juce::Font(juce::FontOptions(7.8f)).withExtraKerningFactor(0.16f));
        };

        addAndMakeVisible(volLabel);
        styleMasterLabel(volLabel);

        addAndMakeVisible(mixLabel);
        styleMasterLabel(mixLabel);

        addAndMakeVisible(freqLabel);
        styleMasterLabel(freqLabel);

        addAndMakeVisible(compLabel);
        styleMasterLabel(compLabel);

        monoMakerToggle.setAlpha(0.82f);

        volAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.tree, "masterVolume", masterVolumeSlider);
        mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.tree, "masterMix", masterMixSlider);
        monoFreqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.tree, "monoMakerFreq", monoMakerFreqSlider);
        monoToggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.tree, "monoMakerToggle", monoMakerToggle);
        compAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.tree, "masterCompressor", masterCompressorSlider);

        addAndMakeVisible(rootPanel);
        addAndMakeVisible(pulsePanel);
        addAndMakeVisible(bottomPanel);
        addAndMakeVisible(bioSeq);
        addAndMakeVisible(centerComponent); // Added last to ensure Evolution Master is visible on top

        setOpaque(false);
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // The backdrop is now drawn globally by the Editor to ensure full window coverage.
        // We only draw the organic overlays here.

        // Add the Mycelium Network for that deep organic "Bio-Web" look
        RootFlow::drawMyceliumNetwork(g, bounds, RootFlow::accentSoft, 0.045f, 1337);
        RootFlow::drawMyceliumNetwork(g, bounds, RootFlow::accent, 0.026f, 999);

        auto shell = bounds.reduced(14.0f, 8.0f).withTrimmedTop(10.0f);
        auto cabinetBody = shell.expanded(10.0f, 12.0f).translated(0.0f, 6.0f);
        auto cabinetLip = shell.expanded(4.0f, 5.0f);
        auto cabinetPocket = shell.reduced(12.0f, 10.0f);
        const float braceWidth = juce::jmin(32.0f, cabinetBody.getWidth() * 0.030f);
        const float braceHeight = juce::jmax(150.0f, cabinetBody.getHeight() * 0.44f);
        const float braceY = juce::jmin(cabinetBody.getBottom() - braceHeight - 44.0f,
                                        shell.getY() + shell.getHeight() * 0.28f);
        auto leftBrace = juce::Rectangle<float>(braceWidth, braceHeight)
                             .withPosition(cabinetBody.getX() + 18.0f, braceY);
        auto rightBrace = leftBrace.withX(cabinetBody.getRight() - braceWidth - 15.0f);
        auto frontApron = juce::Rectangle<float>(juce::jmax(240.0f, cabinetBody.getWidth() - 140.0f), 42.0f)
                              .withCentre({ cabinetBody.getCentreX(), shell.getBottom() + 12.0f });
        frontApron = frontApron.withY(shell.getBottom() - 10.0f);
        auto bridgeShelf = juce::Rectangle<float>(juce::jmax(220.0f, shell.getWidth() - 180.0f), 16.0f)
                               .withCentre({ shell.getCentreX(), shell.getY() + 18.0f });

        auto drawCabinetBrace = [&g](juce::Rectangle<float> area, juce::Colour tint)
        {
            g.setColour(juce::Colours::black.withAlpha(0.22f));
            g.fillRoundedRectangle(area.translated(0.0f, 4.0f).expanded(1.0f, 0.6f), area.getWidth() * 0.46f);

            juce::ColourGradient braceGrad(RootFlow::panelSoft.brighter(0.16f).interpolatedWith(tint, 0.08f),
                                           area.getCentreX(), area.getY(),
                                           RootFlow::bg.darker(0.34f),
                                           area.getCentreX(), area.getBottom(), false);
            g.setGradientFill(braceGrad);
            g.fillRoundedRectangle(area, area.getWidth() * 0.42f);

            juce::ColourGradient braceSheen(juce::Colours::white.withAlpha(0.11f),
                                            area.getCentreX(), area.getY(),
                                            juce::Colours::transparentBlack,
                                            area.getCentreX(), area.getY() + area.getHeight() * 0.42f, false);
            g.setGradientFill(braceSheen);
            g.fillRoundedRectangle(area.reduced(1.0f), area.getWidth() * 0.38f);

            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.drawRoundedRectangle(area.reduced(0.8f), area.getWidth() * 0.38f, 0.8f);
            g.setColour(tint.withAlpha(0.16f));
            g.drawRoundedRectangle(area, area.getWidth() * 0.42f, 1.0f);

            g.setColour(juce::Colours::black.withAlpha(0.18f));
            g.drawLine(area.getCentreX(), area.getY() + 12.0f, area.getCentreX(), area.getBottom() - 12.0f, 1.4f);
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.drawLine(area.getCentreX() - 0.8f, area.getY() + 10.0f, area.getCentreX() - 0.8f, area.getBottom() - 14.0f, 0.8f);
        };

        auto drawFastener = [&g](juce::Point<float> centre, float radius, juce::Colour tint)
        {
            auto bolt = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);
            g.setColour(juce::Colours::black.withAlpha(0.22f));
            g.fillEllipse(bolt.translated(0.0f, 1.6f));
            juce::ColourGradient boltGrad(juce::Colours::white.withAlpha(0.30f), bolt.getCentreX(), bolt.getY(),
                                          tint.darker(0.36f).withAlpha(0.92f), bolt.getCentreX(), bolt.getBottom(), false);
            g.setGradientFill(boltGrad);
            g.fillEllipse(bolt);
            g.setColour(juce::Colours::white.withAlpha(0.24f));
            g.drawEllipse(bolt.reduced(0.8f), 0.7f);
            g.setColour(juce::Colours::black.withAlpha(0.26f));
            g.drawLine(centre.x - radius * 0.38f, centre.y + radius * 0.10f,
                       centre.x + radius * 0.38f, centre.y - radius * 0.10f, 1.1f);
        };

        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.fillRoundedRectangle(cabinetBody.translated(0.0f, 8.0f), 34.0f);
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        g.fillRoundedRectangle(cabinetBody.translated(0.0f, 3.0f), 32.0f);

        juce::ColourGradient cabinetGrad(RootFlow::panelSoft.brighter(0.04f), cabinetBody.getCentreX(), cabinetBody.getY(),
                                         RootFlow::bg.darker(0.28f), cabinetBody.getCentreX(), cabinetBody.getBottom(), false);
        g.setGradientFill(cabinetGrad);
        g.fillRoundedRectangle(cabinetBody, 32.0f);

        juce::ColourGradient cabinetTopBevel(juce::Colours::white.withAlpha(0.09f), cabinetBody.getCentreX(), cabinetBody.getY(),
                                             juce::Colours::transparentBlack, cabinetBody.getCentreX(), cabinetBody.getY() + cabinetBody.getHeight() * 0.30f, false);
        g.setGradientFill(cabinetTopBevel);
        g.fillRoundedRectangle(cabinetBody.reduced(1.0f), 31.0f);

        juce::ColourGradient cabinetLowerShade(juce::Colours::transparentBlack, cabinetBody.getCentreX(), cabinetBody.getY() + cabinetBody.getHeight() * 0.40f,
                                               juce::Colours::black.withAlpha(0.22f), cabinetBody.getCentreX(), cabinetBody.getBottom(), false);
        g.setGradientFill(cabinetLowerShade);
        g.fillRoundedRectangle(cabinetBody.reduced(1.0f), 31.0f);

        g.setColour(RootFlow::accent.withAlpha(0.10f));
        g.drawRoundedRectangle(cabinetBody, 32.0f, 1.2f);
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(cabinetBody.reduced(1.5f), 30.5f, 0.9f);

        juce::ColourGradient lipGrad(RootFlow::panelSoft.brighter(0.14f), cabinetLip.getCentreX(), cabinetLip.getY(),
                                     RootFlow::panel.darker(0.16f), cabinetLip.getCentreX(), cabinetLip.getBottom(), false);
        g.setGradientFill(lipGrad);
        g.fillRoundedRectangle(cabinetLip, 29.0f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(cabinetLip.reduced(1.0f), 28.0f, 0.8f);

        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.fillRoundedRectangle(cabinetPocket, 22.0f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawRoundedRectangle(cabinetPocket, 22.0f, 0.8f);

        drawCabinetBrace(leftBrace, RootFlow::accentSoft);
        drawCabinetBrace(rightBrace, RootFlow::violet);

        g.setColour(juce::Colours::black.withAlpha(0.24f));
        g.fillRoundedRectangle(frontApron.translated(0.0f, 6.0f), 18.0f);
        juce::ColourGradient apronGrad(RootFlow::panelSoft.brighter(0.12f), frontApron.getCentreX(), frontApron.getY(),
                                       RootFlow::bg.darker(0.30f), frontApron.getCentreX(), frontApron.getBottom(), false);
        g.setGradientFill(apronGrad);
        g.fillRoundedRectangle(frontApron, 17.0f);
        juce::ColourGradient apronSheen(juce::Colours::white.withAlpha(0.10f), frontApron.getCentreX(), frontApron.getY(),
                                        juce::Colours::transparentBlack, frontApron.getCentreX(), frontApron.getY() + frontApron.getHeight() * 0.46f, false);
        g.setGradientFill(apronSheen);
        g.fillRoundedRectangle(frontApron.reduced(1.0f), 16.0f);
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(frontApron.reduced(1.0f), 16.0f, 0.8f);
        g.setColour(RootFlow::accent.withAlpha(0.14f));
        g.drawRoundedRectangle(frontApron, 17.0f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.drawLine(frontApron.getX() + 26.0f, frontApron.getY() + 10.0f,
                   frontApron.getRight() - 26.0f, frontApron.getY() + 10.0f, 1.0f);

        juce::ColourGradient shelfGrad(juce::Colours::white.withAlpha(0.08f), bridgeShelf.getCentreX(), bridgeShelf.getY(),
                                       juce::Colours::transparentBlack, bridgeShelf.getCentreX(), bridgeShelf.getBottom(), false);
        g.setGradientFill(shelfGrad);
        g.fillRoundedRectangle(bridgeShelf, bridgeShelf.getHeight() * 0.5f);

        drawFastener({ cabinetLip.getX() + 18.0f, cabinetLip.getY() + 18.0f }, 4.8f, RootFlow::accentSoft);
        drawFastener({ cabinetLip.getRight() - 18.0f, cabinetLip.getY() + 18.0f }, 4.8f, RootFlow::amber);
        drawFastener({ frontApron.getX() + 24.0f, frontApron.getCentreY() }, 4.6f, RootFlow::accent);
        drawFastener({ frontApron.getRight() - 24.0f, frontApron.getCentreY() }, 4.6f, RootFlow::violet);

        RootFlow::drawGlassPanel(g, shell, 26.0f, 0.78f);

        const bool idleEffectsEnabled = RootFlow::areIdleEffectsEnabled();
        const float idleBreath = idleEffectsEnabled ? (0.5f + 0.5f * std::sin(pulsePhase * juce::MathConstants<float>::twoPi))
                                                    : 0.0f;
        const auto systemTint = getSystemTint();
        if (idleEffectsEnabled)
        {
            juce::ColourGradient stageBreath(RootFlow::accentSoft.withAlpha(0.014f + idleBreath * 0.010f),
                                             shell.getCentreX(), shell.getY() + shell.getHeight() * 0.22f,
                                             juce::Colours::transparentBlack,
                                             shell.getCentreX(), shell.getBottom(), true);
            g.setGradientFill(stageBreath);
            g.fillEllipse(shell.getCentreX() - shell.getWidth() * 0.22f,
                          shell.getY() + shell.getHeight() * 0.12f,
                          shell.getWidth() * 0.44f,
                          shell.getHeight() * 0.56f);
        }

        g.setColour(RootFlow::textMuted.withAlpha(0.012f + idleBreath * 0.010f));
        g.drawRoundedRectangle(shell.reduced(8.0f, 6.0f), 22.0f, 1.0f);

        auto titleCanopy = titleLabel.getBounds()
                              .getUnion(rootflowLabel.getBounds())
                              .toFloat()
                              .expanded(20.0f, 6.0f)
                              .translated(0.0f, 1.0f);
        auto titleFascia = titleCanopy.expanded(14.0f, 8.0f).translated(0.0f, 3.0f);
        g.setColour(juce::Colours::black.withAlpha(0.13f));
        g.fillRoundedRectangle(titleFascia.translated(0.0f, 3.0f), titleFascia.getHeight() * 0.54f);
        juce::ColourGradient fasciaGrad(RootFlow::panelSoft.brighter(0.04f), titleFascia.getCentreX(), titleFascia.getY(),
                                        RootFlow::panel.darker(0.20f), titleFascia.getCentreX(), titleFascia.getBottom(), false);
        g.setGradientFill(fasciaGrad);
        g.fillRoundedRectangle(titleFascia, titleFascia.getHeight() * 0.54f);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawRoundedRectangle(titleFascia.reduced(1.0f), titleFascia.getHeight() * 0.50f, 0.8f);
        RootFlow::drawGlassPanel(g, titleCanopy, titleCanopy.getHeight() * 0.5f, 0.18f + idleBreath * 0.03f);
        g.setColour(systemTint.withAlpha(0.006f + idleBreath * 0.004f));
        g.fillRoundedRectangle(titleCanopy.reduced(2.0f), titleCanopy.getHeight() * 0.42f);

        juce::Path titleSheen;
        titleSheen.startNewSubPath(titleCanopy.getX() + titleCanopy.getWidth() * 0.12f, titleCanopy.getBottom() - 3.0f);
        titleSheen.cubicTo(titleCanopy.getX() + titleCanopy.getWidth() * 0.24f, titleCanopy.getY() + titleCanopy.getHeight() * 0.08f,
                           titleCanopy.getRight() - titleCanopy.getWidth() * 0.24f, titleCanopy.getY() + titleCanopy.getHeight() * 0.08f,
                           titleCanopy.getRight() - titleCanopy.getWidth() * 0.12f, titleCanopy.getBottom() - 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.008f + idleBreath * 0.005f));
        g.strokePath(titleSheen, juce::PathStrokeType(0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto rootflowBounds = rootflowLabel.getBounds().toFloat();
        juce::ColourGradient rootflowAura(RootFlow::accent.withAlpha(0.03f + idleBreath * 0.015f),
                                          rootflowBounds.getCentreX(), rootflowBounds.getCentreY(),
                                          RootFlow::violet.withAlpha(0.02f + idleBreath * 0.01f),
                                          rootflowBounds.getX(), rootflowBounds.getBottom(), true);
        g.setGradientFill(rootflowAura);
        g.fillRoundedRectangle(rootflowBounds.expanded(12.0f, 6.0f), rootflowBounds.getHeight() * 0.54f);

        const auto dividerX = (float) ((titleLabel.getRight() + rootflowLabel.getX()) / 2);
        g.setColour(RootFlow::textMuted.withAlpha(0.045f + idleBreath * 0.015f));
        g.drawLine(dividerX, titleCanopy.getY() + 8.0f, dividerX, titleCanopy.getBottom() - 8.0f, 1.0f);

        juce::Path bridge;
        auto bridgeY = shell.getBottom() - 126.0f;
        bridge.startNewSubPath(shell.getX() + 120.0f, bridgeY);
        bridge.cubicTo(shell.getX() + 240.0f, shell.getBottom() - 44.0f,
                       shell.getRight() - 240.0f, shell.getBottom() - 44.0f,
                       shell.getRight() - 120.0f, bridgeY);
        g.setColour(RootFlow::accent.withAlpha(0.024f));
        g.strokePath(bridge, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (! rootPanel.getBounds().isEmpty() && ! pulsePanel.getBounds().isEmpty())
        {
                if (idleEffectsEnabled)
                {
                    RootFlow::drawBioThread(g,
                                            titleCanopy.getBottomLeft().translated(42.0f, -4.0f),
                                            rootPanel.getBounds().toFloat().getTopRight().translated(-28.0f, 34.0f),
                                            RootFlow::accentSoft, 0.022f + idleBreath * 0.012f, 0.85f);
                    RootFlow::drawBioThread(g,
                                            titleCanopy.getBottomRight().translated(-42.0f, -4.0f),
                                            pulsePanel.getBounds().toFloat().getTopLeft().translated(28.0f, 34.0f),
                                            RootFlow::amber, 0.020f + idleBreath * 0.010f, 0.85f);
                }
                RootFlow::drawBioThread(g,
                                        rootPanel.getBounds().toFloat().getTopRight().translated(-22.0f, 78.0f),
                                        centerComponent.getBounds().toFloat().getTopLeft().translated(44.0f, 140.0f),
                                        RootFlow::accentSoft, 0.036f, 0.95f);
            RootFlow::drawBioThread(g,
                                    pulsePanel.getBounds().toFloat().getTopLeft().translated(22.0f, 78.0f),
                                    centerComponent.getBounds().toFloat().getTopRight().translated(-44.0f, 140.0f),
                                    RootFlow::accent, 0.036f, 0.95f);
            RootFlow::drawBioThread(g,
                                    bottomPanel.getBounds().toFloat().getCentre().translated(0.0f, -12.0f),
                                    centerComponent.getBounds().toFloat().getCentre().translated(0.0f, 160.0f),
                                    RootFlow::accent.withAlpha(0.70f), 0.032f, 1.0f);
        }

        auto mutateArea = mutateButton.getBounds().toFloat().expanded(3.0f);
        const bool mutateHovered = RootFlow::areHoverEffectsEnabled() && mutateButton.isMouseOverOrDragging();
        const bool mutatePressed = mutateButton.isMouseButtonDown();
        const float mutateGlow = mutatePressed ? 0.84f : (mutateHovered ? 0.68f : (0.48f + idleBreath * 0.05f));
        RootFlow::drawGlassPanel(g, mutateArea, mutateArea.getWidth() * 0.48f, mutateGlow);
        g.setColour(RootFlow::amber.withAlpha(mutatePressed ? 0.10f : (mutateHovered ? 0.08f : 0.045f + idleBreath * 0.015f)));
        g.fillEllipse(mutateArea.reduced(3.0f));
        g.setColour(juce::Colours::white.withAlpha(mutateHovered ? 0.07f : 0.035f));
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
        g.setColour(RootFlow::amber.withAlpha(mutateHovered ? 0.78f : 0.58f));
        g.drawFittedText(mutatePressed ? "MUTATING" : "EVOLVE", mutateCaption.toNearestInt(), juce::Justification::centred, 1);

        RootFlow::drawGlowOrb(g, { shell.getX() + 30.0f, shell.getY() + 26.0f }, 6.2f, RootFlow::accent, 0.52f);
        RootFlow::drawGlowOrb(g, { shell.getRight() - 30.0f, shell.getY() + 26.0f }, 6.2f, RootFlow::amber, 0.46f);

        // --- GLOBAL PARAMETER TETHERS ---
        drawAllPanelTethers(g);
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
        auto energyBounds = centerComponent.getEnergyDisplay().getBounds().toFloat()
                                              .translated((float) centerComponent.getX(),
                                                          (float) centerComponent.getY());
        auto rootTabArea = rootTab.withWidth(152.0f);
        auto seqTabArea = seqTab.withWidth(324.0f).withCentre(seqTab.getCentre());
        auto coreTabArea = juce::Rectangle<float>(juce::jlimit(180.0f, 250.0f, energyBounds.getWidth() * 0.72f), 30.0f)
                               .withCentre({ energyBounds.getCentreX(), energyBounds.getY() - 9.0f });
        auto pulseTabArea = juce::Rectangle<float>(pulseTab.getRight() - 168.0f, pulseTab.getY(), 168.0f, pulseTab.getHeight());
        auto ambientBand = bottomPanel.getBounds().toFloat().reduced(52.0f, 10.0f).withHeight(24.0f);

        RootFlow::drawTabLabel(g, rootTabArea, "ROOT FIELD");
        RootFlow::drawTabLabel(g, seqTabArea, "BIO-SEQUENCER", RootFlow::TabLabelStyle::prominent);
        RootFlow::drawTabLabel(g, coreTabArea, "CENTER PANEL");
        RootFlow::drawTabLabel(g, pulseTabArea, "PULSE FIELD");

        // --- EVOLUTION MASTER LABEL (DRAWN ON TOP OF EVERYTHING) ---
        auto evolutionBox = centerComponent.getEvolution().getBounds().toFloat()
                                          .translated((float) centerComponent.getX(),
                                                      (float) centerComponent.getY());
        auto evolutionLabelArea = evolutionBox.withHeight(20.0f).translated(0.0f, -20.0f);
        
        RootFlow::drawTabLabel(g,
                               evolutionLabelArea.withTrimmedLeft(10.0f).withTrimmedRight(10.0f),
                               "EVOLUTION MASTER");

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
                g.setColour(tint.withAlpha(0.010f + idleBreath * 0.008f));
                g.fillRoundedRectangle(area.expanded(3.0f, 1.0f), area.getHeight() * 0.5f);
                g.setColour(tint.withAlpha(0.026f + idleBreath * 0.016f));
                g.drawRoundedRectangle(area.expanded(2.0f, 1.0f), area.getHeight() * 0.5f + 1.0f, 0.8f);
            };

            const auto idleTint = RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.42f);
            drawIdleMarker(rootTabArea, RootFlow::accentSoft);
            drawIdleMarker(seqTabArea, RootFlow::amber.interpolatedWith(RootFlow::accentSoft, 0.18f));
            drawIdleMarker(coreTabArea, idleTint);
            drawIdleMarker(pulseTabArea, RootFlow::accent);
            drawIdleMarker(ambientBand, RootFlow::accent.interpolatedWith(RootFlow::amber, 0.22f));

            drawActivityThread(g, rootAnchor, centerAnchor, RootFlow::accentSoft, 0.16f + idleBreath * 0.05f, 0.00f);
            drawActivityThread(g, pulseAnchor, centerAnchor, RootFlow::accent, 0.16f + idleBreath * 0.05f, 0.32f);
            drawActivityThread(g, ambientAnchor, centerAnchor, RootFlow::accent.interpolatedWith(RootFlow::amber, 0.18f), 0.14f + idleBreath * 0.04f, 0.56f);
            drawActivityThread(g, seqAnchor, centerAnchor, RootFlow::amber.interpolatedWith(RootFlow::accentSoft, 0.22f), 0.15f + idleBreath * 0.05f, 0.82f);
            RootFlow::drawGlowOrb(g, centerAnchor, 3.4f + idleBreath * 0.4f, idleTint, 0.10f + idleBreath * 0.05f);
            return;
        }

        auto activeTab = focus.zone == FocusSnapshot::Zone::root ? rootTabArea
                       : (focus.zone == FocusSnapshot::Zone::pulse ? pulseTabArea
                       : (focus.zone == FocusSnapshot::Zone::sequencer ? seqTabArea
                       : (focus.zone == FocusSnapshot::Zone::ambient ? ambientBand
                                                                       : coreTabArea)));

        g.setColour(focus.tint.withAlpha(0.035f));
        g.fillRoundedRectangle(activeTab.expanded(5.0f, 3.0f), activeTab.getHeight() * 0.5f);
        g.setColour(focus.tint.withAlpha(0.10f));
        g.drawRoundedRectangle(activeTab.expanded(4.0f, 2.0f), activeTab.getHeight() * 0.5f + 1.0f, 0.9f);

        const float bubbleCenterX = focus.zone == FocusSnapshot::Zone::sequencer ? seqTabArea.getCentreX()
                                 : (focus.zone == FocusSnapshot::Zone::ambient ? ambientBand.getCentreX()
                                                                               : coreTabArea.getCentreX());
        const float bubbleCenterY = focus.zone == FocusSnapshot::Zone::sequencer ? seqTabArea.getBottom() + 18.0f
                                 : (focus.zone == FocusSnapshot::Zone::ambient ? ambientBand.getY() - 16.0f
                                                                               : coreTabArea.getY() - 16.0f);
        if (popupOverlaysEnabled)
        {
            auto bubble = juce::Rectangle<float>(204.0f, 22.0f)
                              .withCentre({ bubbleCenterX, bubbleCenterY });
            bubble.setX(juce::jlimit(30.0f, (float) getWidth() - bubble.getWidth() - 30.0f, bubble.getX()));
            bubble.setY(juce::jlimit(26.0f, (float) getHeight() - bubble.getHeight() - 26.0f, bubble.getY()));
            drawStageActivityBubble(g, bubble, focus.section, focus.value, focus.tint);
        }

        RootFlow::drawGlowOrb(g, focus.anchor, 3.8f, focus.tint, 0.34f);
        RootFlow::drawGlowOrb(g, centerAnchor, 3.4f, focus.tint, 0.22f);

        if (focus.zone == FocusSnapshot::Zone::root)
        {
            drawActivityThread(g, focus.anchor, centerAnchor, focus.tint, 0.82f, 0.00f);
            drawActivityThread(g, centerAnchor, pulseAnchor, focus.tint.interpolatedWith(RootFlow::accent, 0.25f), 0.46f, 0.34f);
        }
        else if (focus.zone == FocusSnapshot::Zone::pulse)
        {
            drawActivityThread(g, focus.anchor, centerAnchor, focus.tint, 0.82f, 0.00f);
            drawActivityThread(g, centerAnchor, rootAnchor, focus.tint.interpolatedWith(RootFlow::accentSoft, 0.28f), 0.46f, 0.34f);
        }
        else if (focus.zone == FocusSnapshot::Zone::sequencer)
        {
            drawActivityThread(g, focus.anchor, centerAnchor, focus.tint, 0.82f, 0.00f);
            drawActivityThread(g, centerAnchor, rootAnchor, focus.tint.interpolatedWith(RootFlow::accentSoft, 0.24f), 0.36f, 0.22f);
            drawActivityThread(g, centerAnchor, pulseAnchor, focus.tint.interpolatedWith(RootFlow::amber, 0.20f), 0.36f, 0.54f);
        }
        else if (focus.zone == FocusSnapshot::Zone::ambient)
        {
            drawActivityThread(g, focus.anchor, centerAnchor, focus.tint, 0.78f, 0.00f);
            drawActivityThread(g, centerAnchor, rootAnchor, focus.tint.interpolatedWith(RootFlow::accentSoft, 0.18f), 0.30f, 0.28f);
            drawActivityThread(g, centerAnchor, pulseAnchor, focus.tint.interpolatedWith(RootFlow::amber, 0.18f), 0.30f, 0.56f);
            RootFlow::drawGlowOrb(g, ambientAnchor, 3.4f, focus.tint, 0.24f);
        }
        else
        {
            drawActivityThread(g, focus.anchor, rootAnchor, focus.tint.interpolatedWith(RootFlow::accentSoft, 0.18f), 0.70f, 0.00f);
            drawActivityThread(g, focus.anchor, pulseAnchor, focus.tint.interpolatedWith(RootFlow::amber, 0.18f), 0.70f, 0.42f);
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

    float getPulsePhase() const noexcept { return (float) pulsePhase; }
    juce::Rectangle<int> getTitleSeedDockBounds() const
    {
        if (titleLabel.getBounds().isEmpty() || rootflowLabel.getBounds().isEmpty())
            return {};

        auto titleCanopy = titleLabel.getBounds()
                              .getUnion(rootflowLabel.getBounds())
                              .toFloat()
                              .expanded(24.0f, 8.0f)
                              .translated(0.0f, 1.0f);
        auto titleFascia = titleCanopy.expanded(18.0f, 10.0f).translated(0.0f, 4.0f);
        auto dock = titleFascia.reduced(26.0f, 6.0f);

        const auto titleBounds = titleLabel.getBounds().toFloat();
        const auto titleFont = titleLabel.getFont();
        const float titleTextWidth = juce::GlyphArrangement::getStringBounds(titleFont, titleLabel.getText()).getWidth() + 12.0f;
        const float titleTextLeft = titleBounds.getRight() - titleTextWidth;
        dock.setRight(juce::jmin(dock.getRight(), titleTextLeft - 18.0f));

        if (dock.getWidth() < 110.0f)
            return {};

        const float rowHeight = juce::jlimit(20.0f, 26.0f, dock.getHeight() - 4.0f);
        return juce::Rectangle<float>(dock.getX(),
                                      dock.getCentreY() - rowHeight * 0.5f,
                                      dock.getWidth(),
                                      rowHeight)
            .toNearestInt();
    }

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

    void drawAllPanelTethers(juce::Graphics& g)
    {
        auto& root = getRootPanel();
        auto& pulse = getPulsePanel();
        
        // Root Panel Tethers
        drawParameterTether(g, root.getDepthSlider(), "rootDepth");
        drawParameterTether(g, root.getSoilSlider(), "rootSoil");
        drawParameterTether(g, root.getAnchorSlider(), "rootAnchor");
        
        // Pulse Panel Tethers
        drawParameterTether(g, pulse.getRateSlider(), "pulseRate");
        drawParameterTether(g, pulse.getBreathSlider(), "pulseBreath");
        drawParameterTether(g, pulse.getGrowthSlider(), "pulseGrowth");

        // Master Sliders
        drawParameterTether(g, masterVolumeSlider, "masterVolume");
        drawParameterTether(g, masterMixSlider, "masterMix");
        drawParameterTether(g, monoMakerFreqSlider, "monoMakerFreq");
        drawParameterTether(g, masterCompressorSlider, "masterCompressor");
    }

    void drawParameterTether(juce::Graphics& g, juce::Slider& slider, const juce::String& paramID)
    {
        auto& center = getCenterComponent();
        auto& display = center.getEnergyDisplay();
        
        // Get actual node position
        const auto nodeRelPos = display.getNodePositionRelative(paramID);
        const auto displayBounds = display.getBounds().toFloat().translated((float)center.getX(), (float)center.getY());
        
        const auto targetNodePos = juce::Point<float>(
            displayBounds.getX() + nodeRelPos.x * displayBounds.getWidth(),
            displayBounds.getY() + nodeRelPos.y * displayBounds.getHeight()
        );

        // Get slider source point
        auto sliderBounds = slider.getBounds().toFloat();
        if (auto* parent = slider.getParentComponent())
            sliderBounds = sliderBounds.translated((float)parent->getX(), (float)parent->getY());
            
        const bool leftSide = sliderBounds.getCentreX() < getWidth() * 0.5f;
        const auto start = juce::Point<float>(leftSide ? sliderBounds.getRight() - 6.0f : sliderBounds.getX() + 6.0f, 
                                              sliderBounds.getCentreY());
        
        const bool isFocused = slider.isMouseOverOrDragging() || slider.isMouseButtonDown();
        const float alphaBase = isFocused ? 0.12f : 0.022f;
        const auto tint = RootFlow::accent.interpolatedWith(RootFlow::violet, leftSide ? 0.1f : 0.4f);

        juce::Path tether;
        tether.startNewSubPath(start);
        
        float dist = start.getDistanceFrom(targetNodePos);
        float ctrlOffset = juce::jmin(120.0f, dist * 0.4f);
        
        tether.cubicTo(start.x + (leftSide ? ctrlOffset : -ctrlOffset), start.y,
                       targetNodePos.x + (leftSide ? -ctrlOffset * 0.5f : ctrlOffset * 0.5f), targetNodePos.y,
                       targetNodePos.x, targetNodePos.y);

        g.setColour(tint.withAlpha(alphaBase * 0.4f));
        g.strokePath(tether, juce::PathStrokeType(isFocused ? 3.0f : 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(tint.withAlpha(alphaBase));
        g.strokePath(tether, juce::PathStrokeType(isFocused ? 1.0f : 0.6f));
        
        if (isFocused)
        {
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.strokePath(tether, juce::PathStrokeType(0.4f));
        }
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
        pulsePhase = std::fmod(pulsePhase + 0.010f, 1.0f);
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
