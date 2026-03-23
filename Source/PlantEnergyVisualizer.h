#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>

class PlantEnergyVisualizer : public juce::Component, public juce::Timer
{
private:
    static constexpr int historySize = 144;
    using HistoryBuffer = std::array<float, historySize>;

public:
    struct VisualizerState
    {
        float plantEnergy = 0.0f;
        float audioEnergy = 0.0f;
        float rootDepth = 0.5f;
        float rootSoil = 0.5f;
        float rootAnchor = 0.5f;
        float sapFlow = 0.5f;
        float sapVitality = 0.5f;
        float sapTexture = 0.5f;
        float pulseRate = 0.5f;
        float pulseBreath = 0.5f;
        float pulseGrowth = 0.5f;
        float canopy = 0.5f;
        float atmosphere = 0.5f;
        float instability = 0.5f;
        float bloom = 0.0f;
        float rain = 0.0f;
        float sun = 0.0f;
        float ecoSystem = 0.5f;
        int currentSequencerStep = -1;
        bool sequencerOn = false;
        std::array<bool, 16> sequencerStepActive {};
    };

    enum class SpeciesMode
    {
        lightFlow,
        classicSolid,
        abstractGhost
    };

    enum class GrowthColor
    {
        emerald,
        neon,
        moss,
        forest
    };

    void cycleDisplayMode() 
    { 
        currentSpecies = static_cast<SpeciesMode>((static_cast<int>(currentSpecies) + 1) % 3); 
    }

    void cycleColorPalette() 
    { 
        currentColor = static_cast<GrowthColor>((static_cast<int>(currentColor) + 1) % 4); 
        rebuildMyceliumLayout(getGraphArea(getLocalBounds().toFloat()));
    }


    PlantEnergyVisualizer()
    {
        updateAnimationTimerState();
    }

    void pushEnergyValue(float energy)
    {
        targetEnergy = energy;
    }

    void pushVisualizerState(const VisualizerState& state)
    {
        targetState = state;
    }

    SpeciesMode getCurrentSpecies() const { return currentSpecies; }
    GrowthColor getCurrentColor() const { return currentColor; }


    void triggerMidiImpulse(int noteNumber, float intensity, bool wasMapped)
    {
        const float clampedIntensity = juce::jlimit(0.16f, 1.0f, intensity);

        if (myceliumNodes.empty())
        {
            queuedImpulseNote = noteNumber;
            queuedImpulseIntensity = clampedIntensity;
            queuedImpulseMapped = wasMapped;
            hasQueuedImpulse = true;
            return;
        }

        launchMyceliumImpulse(noteNumber, clampedIntensity, wasMapped);
    }

    void pushSpectrumData(const float* data, int size)
    {
        if (data == nullptr || size <= 0)
        {
            for (auto& value : spectrum)
                value *= 0.98f;

            return;
        }

        const int limit = juce::jmin(size, (int) spectrum.size());

        for (int i = 0; i < limit; ++i)
        {
            const float incoming = juce::jlimit(0.0f, 1.0f, data[i]);
            auto& displayValue = spectrum[(size_t) i];
            const float coeff = incoming > displayValue ? 0.15f : 0.06f;
            displayValue += (incoming - displayValue) * coeff;
        }

        for (int i = limit; i < (int) spectrum.size(); ++i)
            spectrum[(size_t) i] *= 0.98f;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto graphArea = getGraphArea(bounds);
        const float microTime = (float) juce::Time::getMillisecondCounterHiRes() * 0.00020f;
        const float microBreath = 0.5f + 0.5f * std::sin(microTime + ambientFieldPhase * 0.55f);
        const float glowBreathScale = 0.985f + microBreath * 0.045f;
        const float ambientBreathAlpha = 0.96f + microBreath * 0.10f;

        g.fillAll(juce::Colours::transparentBlack);
        ensureStaticLayer(bounds, graphArea);
        ensureMyceliumLayout(graphArea);
        drawCachedStaticLayer(g);

        drawAmbientField(g, graphArea, microBreath);
        drawAmbientBloom(g, graphArea);
        drawSpectralFilaments(g, graphArea);

        auto pathA = buildPath(graphArea, historyA, 1.00f, 0.0f);
        auto pathB = buildPath(graphArea, historyB, 0.82f, 1.2f);
        
        // --- WOW: Twisted Lifeline (Biological Strands) ---
        auto drawBioStrand = [&](const juce::Path& p, juce::Colour col, float thickness, float spiralAmount)
        {
            const float energyBoost = 1.0f + glowEnergy * 0.45f;
            g.setColour(col.withAlpha(0.08f + glowEnergy * 0.12f));
            strokeMirroredPath(g, graphArea, p, juce::PathStrokeType(thickness * 5.0f * energyBoost, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            g.setColour(col.withAlpha(0.65f + glowEnergy * 0.35f));
            strokeMirroredPath(g, graphArea, p, juce::PathStrokeType(thickness * energyBoost, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Inner spiraling tendrils
            if (glowEnergy > 0.05f)
            {
                for (int i = 0; i < 2; ++i)
                {
                    juce::Path tendril;
                    const float offset = (float) i * juce::MathConstants<float>::pi + phaseA * 4.0f;
                    
                    // Flatten and iterate to create spiraling effect (Fixed constructor)
                    auto it = juce::PathFlatteningIterator(p, juce::AffineTransform(), 1.0f);
                    bool first = true;
                    while (it.next())
                    {
                        auto pt = juce::Point<float>(it.x1, it.y1);
                        const float t = (float) tendril.getLength() / 200.0f;
                        const float s = std::sin(t * 12.0f + offset) * spiralAmount * glowEnergy;
                        pt.y += s;
                        
                        if (first) { tendril.startNewSubPath(pt); first = false; }
                        else tendril.lineTo(pt);
                    }
                    g.setColour(juce::Colours::white.withAlpha(0.15f * glowEnergy));
                    strokeMirroredPath(g, graphArea, tendril, juce::PathStrokeType(0.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
            }
        };

        const auto waveCol = (currentColor == GrowthColor::emerald) ? juce::Colour(160, 255, 200) :
                             (currentColor == GrowthColor::neon) ? juce::Colour(200, 255, 100) :
                             (currentColor == GrowthColor::moss) ? juce::Colour(180, 230, 160) :
                             juce::Colour(120, 180, 120);

        drawBioStrand(pathA, waveCol, 2.2f, 14.0f);
        drawBioStrand(pathB, waveCol.withMultipliedAlpha(0.4f), 0.8f, 6.0f);

        drawMirroredEnergySeed(g, graphArea, historyA, 1.00f, 0.0f, energySeedPhase);
        drawMyceliumNetwork(g, graphArea);
        drawSporeField(g, graphArea);
        drawBioSequencer(g, graphArea);
    }

    void timerCallback() override
    {
        updateLogic();
    }

    void visibilityChanged() override
    {
        updateAnimationTimerState();
    }

    void parentHierarchyChanged() override
    {
        updateAnimationTimerState();
    }

    void resized() override
    {
        invalidateStaticLayer();
        myceliumLayoutDirty = true;
    }

private:
    static juce::Rectangle<float> getGraphArea(juce::Rectangle<float> bounds)
    {
        auto display = bounds.reduced(16.0f, 20.0f);
        return display.reduced(0.0f, 18.0f);
    }

    struct MycelNode
    {
        juce::Point<float> anchor;
        float radius = 1.8f;
        float swayPhase = 0.0f;
        float swayAmount = 0.0f;
        float depthBias = 0.0f;
        juce::Colour baseColour;
    };

    struct MycelEdge
    {
        int from = 0;
        int to = 0;
        float thickness = 1.0f;
        float conductivity = 0.5f;
        float curvature = 0.0f;
    };

    struct MycelPulse
    {
        int fromNode = 0;
        int toNode = 0;
        int previousNode = -1;
        float progress = 0.0f;
        float speed = 0.02f;
        float intensity = 0.4f;
        int hopsRemaining = 1;
        bool accent = false;
    };

    struct SporeParticle
    {
        juce::Point<float> position;
        juce::Point<float> velocity;
        float life = 1.0f;
        float size = 2.0f;
        float brightness = 0.6f;
        int layer = 1; // 0: background, 1: mid, 2: foreground (bokeh)
        bool accent = false;
    };


    void invalidateStaticLayer()
    {
        staticLayerDirty = true;
    }

    void ensureStaticLayer(juce::Rectangle<float> bounds, juce::Rectangle<float> graphArea)
    {
        if (bounds.isEmpty())
            return;

        const float scale = juce::jmax(1.0f, juce::Component::getApproximateScaleFactorForComponent(this));
        const int imageWidth = juce::jmax(1, juce::roundToInt(bounds.getWidth() * scale));
        const int imageHeight = juce::jmax(1, juce::roundToInt(bounds.getHeight() * scale));
        const bool sizeChanged = staticLayer.getWidth() != imageWidth || staticLayer.getHeight() != imageHeight;
        const bool scaleChanged = std::abs(staticLayerScale - scale) > 0.001f;

        if (! staticLayerDirty && ! sizeChanged && ! scaleChanged)
            return;

        staticLayer = juce::Image(juce::Image::ARGB, imageWidth, imageHeight, true);
        staticLayerScale = scale;
        staticLayerDirty = false;

        juce::Graphics layerGraphics(staticLayer);
        layerGraphics.addTransform(juce::AffineTransform::scale(scale));
        drawStaticLayer(layerGraphics, bounds, graphArea);
    }

    void drawCachedStaticLayer(juce::Graphics& g) const
    {
        if (! staticLayer.isValid())
            return;

        juce::Graphics::ScopedSaveState staticLayerState(g);

        if (std::abs(staticLayerScale - 1.0f) > 0.001f)
            g.addTransform(juce::AffineTransform::scale(1.0f / staticLayerScale));

        g.drawImageAt(staticLayer, 0, 0);
    }

    void drawStaticLayer(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Rectangle<float> graphArea)
    {
        g.setColour(juce::Colour(8, 8, 7).withAlpha(0.95f));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colour(110, 90, 64).withAlpha(0.20f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawRoundedRectangle(bounds, 6.0f, 1.6f);

        juce::ColourGradient displayGrad(
            juce::Colour(1, 4, 2),
            graphArea.getTopLeft(),
            juce::Colour(5, 14, 8),
            graphArea.getBottomRight(),
            false
        );
        g.setGradientFill(displayGrad);
        g.fillRoundedRectangle(graphArea, 2.0f);
        g.setColour(juce::Colour(160, 255, 200).withAlpha(0.035f));
        g.fillRoundedRectangle(graphArea.reduced(2.0f), 2.0f);
        g.setColour(juce::Colour(110, 95, 70).withAlpha(0.12f));
        g.drawRect(graphArea, 1.0f);

        showGrid(g, graphArea);

        g.setColour(juce::Colour(170, 255, 198).withAlpha(0.12f));
        g.drawLine(graphArea.getX(), graphArea.getCentreY(), graphArea.getRight(), graphArea.getCentreY(), 0.8f);

        const float ledY = bounds.getY() + 9.0f;
        const float ledX = bounds.getRight() - 10.0f;
        g.setColour(juce::Colour(110, 255, 140));
        g.fillEllipse(ledX - 2.0f, ledY - 2.0f, 4.0f, 4.0f);
        g.setColour(juce::Colour(110, 255, 140).withAlpha(0.4f));
        g.fillEllipse(ledX - 10.0f, ledY - 1.5f, 3.0f, 3.0f);

        g.setColour(juce::Colour(172, 255, 204).withAlpha(0.22f));
        g.setFont(juce::FontOptions(8.8f).withStyle("Bold"));
        g.drawText("MYCEL FIELD", (int) graphArea.getX() + 8, (int) graphArea.getY() + 6, 82, 10, juce::Justification::left);
        g.setColour(juce::Colour(138, 112, 82).withAlpha(0.24f));
        g.drawText("ROOT SIGNAL", (int) graphArea.getRight() - 82, (int) graphArea.getBottom() - 15, 74, 10, juce::Justification::right);
    }

    void updateAnimationTimerState()
    {
        if (isShowing())
            startTimerHz(60);
        else
            stopTimer();
    }

    void updateLogic()
    {
        auto smoothValue = [](float& current, float target, float coeff)
        {
            current += (target - current) * coeff;
        };

        smoothValue(currentState.plantEnergy, targetState.plantEnergy, 0.08f);
        smoothValue(currentState.audioEnergy, targetState.audioEnergy, 0.10f);
        smoothValue(currentState.rootDepth, targetState.rootDepth, 0.07f);
        smoothValue(currentState.rootSoil, targetState.rootSoil, 0.07f);
        smoothValue(currentState.rootAnchor, targetState.rootAnchor, 0.07f);
        smoothValue(currentState.sapFlow, targetState.sapFlow, 0.08f);
        smoothValue(currentState.sapVitality, targetState.sapVitality, 0.08f);
        smoothValue(currentState.sapTexture, targetState.sapTexture, 0.08f);
        smoothValue(currentState.pulseRate, targetState.pulseRate, 0.08f);
        smoothValue(currentState.pulseBreath, targetState.pulseBreath, 0.08f);
        smoothValue(currentState.pulseGrowth, targetState.pulseGrowth, 0.08f);
        smoothValue(currentState.canopy, targetState.canopy, 0.07f);
        smoothValue(currentState.atmosphere, targetState.atmosphere, 0.06f);
        smoothValue(currentState.instability, targetState.instability, 0.06f);
        smoothValue(currentState.bloom, targetState.bloom, 0.08f);
        smoothValue(currentState.rain, targetState.rain, 0.08f);
        smoothValue(currentState.sun, targetState.sun, 0.08f);
        smoothValue(currentState.ecoSystem, targetState.ecoSystem, 0.05f);

        const float combinedEnergy = juce::jlimit(0.0f, 1.0f,
                                                  targetEnergy * 0.72f
                                                  + currentState.plantEnergy * 0.18f
                                                  + currentState.audioEnergy * 0.10f);
        smoothedEnergy += (combinedEnergy - smoothedEnergy) * 0.07f;
        ambientEnergy += (smoothedEnergy - ambientEnergy) * 0.035f;
        phaseA += 0.08f + currentState.pulseRate * 0.022f;
        phaseB += 0.05f + currentState.pulseBreath * 0.015f;
        phaseC += 0.035f + currentState.sapFlow * 0.012f;

        if (phaseA > juce::MathConstants<float>::twoPi) phaseA -= juce::MathConstants<float>::twoPi;
        if (phaseB > juce::MathConstants<float>::twoPi) phaseB -= juce::MathConstants<float>::twoPi;
        if (phaseC > juce::MathConstants<float>::twoPi) phaseC -= juce::MathConstants<float>::twoPi;

        const float spectral1 = getSpectralBand(2, 10);
        const float spectral2 = getSpectralBand(10, 32);
        const float spectral3 = getSpectralBand(32, 90);
        const float base = smoothedEnergy * 2.0f - 1.0f;
        const float chaos = 0.28f + smoothedEnergy * 0.42f + spectral2 * 0.18f;

        const float targetA = std::sin(phaseA) * 0.21f
                            + std::sin(phaseA * 2.1f + spectral2 * 2.6f) * 0.08f
                            + std::cos(phaseA * 1.2f - spectral3 * 2.8f) * 0.05f
                            + base * (0.34f + spectral1 * 0.16f);
        const float targetB = std::sin(phaseB + 1.2f) * 0.14f
                            + std::cos(phaseB * 1.9f) * 0.08f
                            + base * 0.14f
                            + (spectral2 - 0.12f) * 0.28f;
        const float targetC = std::sin(phaseC + 2.1f) * 0.10f
                            + std::cos(phaseC * 1.35f) * 0.08f
                            + std::sin(phaseC * 2.6f) * 0.05f
                            + base * 0.10f;
        const float targetD = std::sin(phaseA * 0.7f + phaseB) * 0.14f * chaos
                            - std::cos(phaseC * 1.7f + spectral1 * 4.2f) * 0.07f
                            + base * 0.08f;
        const float targetE = std::cos(phaseB * 0.9f - 1.1f) * 0.09f
                            + std::sin(phaseA * 1.5f + 0.7f) * 0.08f
                            + (spectral3 - 0.08f) * 0.18f;

        coreValueA += (targetA - coreValueA) * 0.18f;
        coreValueB += (targetB - coreValueB) * 0.16f;
        coreValueC += (targetC - coreValueC) * 0.14f;
        coreValueD += (targetD - coreValueD) * 0.12f;
        coreValueE += (targetE - coreValueE) * 0.10f;

        glowEnergy += (smoothedEnergy - glowEnergy) * 0.045f;
        hazeValueA += (coreValueA - hazeValueA) * 0.10f;
        hazeValueB += (coreValueB - hazeValueB) * 0.08f;
        ambientFieldPhase += 0.0032f + ambientEnergy * 0.004f + spectral1 * 0.002f;
        if (ambientFieldPhase > juce::MathConstants<float>::twoPi)
            ambientFieldPhase -= juce::MathConstants<float>::twoPi;

        energySeedPhase += 0.0040f + smoothedEnergy * 0.008f + spectral2 * 0.0035f;
        if (energySeedPhase > 1.0f)
            energySeedPhase -= 1.0f;

        myceliumDriftPhase += 0.010f
                            + currentState.pulseRate * 0.026f
                            + currentState.sapFlow * 0.012f
                            + currentState.instability * 0.016f;
        if (myceliumDriftPhase > juce::MathConstants<float>::twoPi)
            myceliumDriftPhase -= juce::MathConstants<float>::twoPi;

        updateMyceliumPulses();
        
        // --- NEW: Spectrum-to-Mycelium Mapping ---
        const int numRows = (int) myceliumRowStarts.size();
        auto graphArea = getGraphArea(getLocalBounds().toFloat());
        
        for (int r = 0; r < numRows; ++r)
        {
            const int startBin = (int) std::pow(2.0f, (float) r + 1.0f);
            const int endBin = (int) std::pow(2.0f, (float) r + 2.4f);
            const float bandLevel = getSpectralBand(startBin, endBin);
            
            const int rowStart = myceliumRowStarts[(size_t) r];
            const int rowCount = myceliumRowCounts[(size_t) r];
            
            for (int i = 0; i < rowCount; ++i)
            {
                const int nodeIdx = rowStart + i;
                if (juce::isPositiveAndBelow(nodeIdx, (int) myceliumNodeCharge.size()))
                {
                    const float shiver = 0.04f * std::sin(phaseA * 2.2f + (float) nodeIdx);
                    const float targetCharge = juce::jlimit(0.0f, 1.0f, bandLevel * (0.85f + shiver));
                    myceliumNodeCharge[(size_t) nodeIdx] = juce::jmax(myceliumNodeCharge[(size_t) nodeIdx], targetCharge);
                    
                    // --- NEW: Seed Interaction ---
                    // If the energy seed (traveling point) is near this node's horizontal position, charge it!
                    const float nodeXNorm = (myceliumNodes[(size_t) nodeIdx].anchor.x - graphArea.getX()) / graphArea.getWidth();
                    const float distToSeed = std::abs(nodeXNorm - energySeedPhase);
                    if (distToSeed < 0.06f)
                    {
                        const float seedInfluence = (1.0f - distToSeed / 0.06f) * (0.35f + smoothedEnergy * 0.45f);
                        myceliumNodeCharge[(size_t) nodeIdx] = juce::jmax(myceliumNodeCharge[(size_t) nodeIdx], seedInfluence);
                    }
                }
            }
        }

        updateSporeField();
        maybeSpawnAmbientSpores();
        processQueuedImpulseIfNeeded();

        writeHistorySample(historyA, coreValueA);
        writeHistorySample(historyB, coreValueB);
        writeHistorySample(historyC, coreValueC);
        writeHistorySample(historyD, coreValueD);
        writeHistorySample(historyE, coreValueE);
        writeHistorySample(hazeHistoryA, hazeValueA);
        writeHistorySample(hazeHistoryB, hazeValueB);
        historyWritePosition = (historyWritePosition + 1) % historySize;

        repaint();
    }

    void drawBioSequencer(juce::Graphics& g, juce::Rectangle<float> area)
    {
        if (! currentState.sequencerOn)
            return;

        const int numSteps = 16;
        const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.44f;
        const juce::Point<float> center = area.getCentre();
        const float energy = currentState.plantEnergy;
        
        for (int i = 0; i < numSteps; ++i)
        {
            const float angle = (float)i * juce::MathConstants<float>::twoPi / (float)numSteps - juce::MathConstants<float>::halfPi;
            const float x = center.x + std::cos(angle) * (radius + std::sin(ambientFieldPhase * 0.4f + (float)i * 0.5f) * 4.0f * energy);
            const float y = center.y + std::sin(angle) * (radius + std::cos(ambientFieldPhase * 0.4f + (float)i * 0.5f) * 4.0f * energy);
            
            const bool isActive = currentState.sequencerStepActive[(size_t)i];
            const bool isCurrent = (i == currentState.currentSequencerStep);
            
            float dotSize = isActive ? 3.5f : 1.5f;
            if (isCurrent) dotSize *= 1.8f;
            
            juce::Colour dotCol;
            if (isCurrent) dotCol = juce::Colours::white;
            else if (isActive) dotCol = juce::Colour(110, 255, 140).withAlpha(0.4f + energy * 0.4f);
            else dotCol = juce::Colour(110, 255, 140).withAlpha(0.1f);
            
            if (isActive)
            {
                const float pulse = 1.0f + 0.3f * std::sin(ambientFieldPhase * 3.0f + (float)i);
                dotSize *= (0.85f + pulse * 0.15f * energy);
            }

            g.setColour(dotCol);
            g.fillEllipse(x - dotSize * 0.5f, y - dotSize * 0.5f, dotSize, dotSize);
            
            if (isCurrent)
            {
                g.setColour(dotCol.withAlpha(0.2f * energy));
                const float ringSize = dotSize * (2.2f + std::sin(ambientFieldPhase * 4.0f) * 0.5f);
                g.drawEllipse(x - ringSize * 0.5f, y - ringSize * 0.5f, ringSize, ringSize, 0.8f);
            }
        }
    }

    void writeHistorySample(HistoryBuffer& history, float value)
    {
        history[historyWritePosition] = juce::jlimit(-1.0f, 1.0f, value);
    }

    void showGrid(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const int verticals = 8;
        const int horizontals = 5;

        for (int i = 0; i <= verticals; ++i)
        {
            float x = area.getX() + area.getWidth() * ((float)i / (float)verticals);
            g.setColour(juce::Colour(166, 138, 102).withAlpha(i == 0 || i == verticals ? 0.052f : 0.030f));
            g.drawLine(x, area.getY(), x, area.getBottom(), i == 0 || i == verticals ? 1.0f : 0.7f);
            g.setColour(juce::Colour(116, 230, 150).withAlpha(0.018f));
            g.drawLine(x, area.getY(), x, area.getBottom(), 0.45f);
        }

        for (int i = 0; i <= horizontals; ++i)
        {
            float y = area.getY() + area.getHeight() * ((float)i / (float)horizontals);
            g.setColour(juce::Colour(120, 220, 142).withAlpha(i == horizontals / 2 ? 0.060f : 0.022f));
            g.drawLine(area.getX(), y, area.getRight(), y, i == horizontals / 2 ? 0.8f : 0.55f);
        }
    }

    void drawSpectralFilaments(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Restored but subtle and integrated
        const int filaments = 18;
        const int binsPerFilament = juce::jmax(1, (int) spectrum.size() / filaments);

        for (int i = 0; i < filaments; ++i)
        {
            float peak = 0.0f;
            const int start = i * binsPerFilament;
            const int end = juce::jmin((int) spectrum.size(), start + binsPerFilament);

            for (int j = start; j < end; ++j)
                peak = juce::jmax(peak, spectrum[(size_t) j]);

            const float shaped = std::pow(juce::jlimit(0.0f, 1.0f, peak), 0.55f); 
            if (shaped < 0.025f)
                continue;

            const float x = area.getX() + area.getWidth() * ((float) i + 0.5f) / (float) filaments
                          + std::sin((float) i * 1.1f + phaseA * 0.4f) * 1.2f;
            const float halfHeight = area.getHeight() * (0.08f + shaped * 0.38f);

            g.setColour(juce::Colour(118, 255, 138).withAlpha(0.04f + shaped * 0.12f));
            g.drawLine(x, area.getCentreY() - halfHeight, x, area.getCentreY() + halfHeight, 1.6f);
            g.setColour(juce::Colour(230, 255, 210).withAlpha(0.06f + shaped * 0.15f));
            g.drawLine(x, area.getCentreY() - halfHeight * 0.9f, x, area.getCentreY() + halfHeight * 0.9f, 0.8f);
        }
    }

    void drawAmbientField(juce::Graphics& g, juce::Rectangle<float> area, float microBreath)
    {
        const float energy = 0.28f + ambientEnergy * 0.72f;
        const auto centre = area.getCentre();
        const float breathScale = 0.985f + microBreath * 0.040f;
        const float driftX = std::sin(ambientFieldPhase * 0.73f) * area.getWidth() * 0.012f;
        const float driftY = std::sin(ambientFieldPhase) * area.getHeight() * (0.045f + microBreath * 0.010f);

        for (int i = 0; i < 6; ++i)
        {
            const float t = (float) i / 5.0f;
            const float width = area.getWidth() * (0.22f + t * 0.48f) * (0.92f + energy * 0.22f) * breathScale;
            const float height = area.getHeight() * (0.08f + t * 0.11f) * (0.92f + energy * 0.18f) * (0.99f + microBreath * 0.03f);
            const float alpha = (0.020f - t * 0.0024f) * (0.72f + energy * 0.85f + glowEnergy * 0.22f) * (0.95f + microBreath * 0.08f);
            const float x = centre.x + driftX * (0.35f - t);
            const float y = centre.y + driftY * (0.55f - t);

            g.setColour(juce::Colour(116, 255, 136).withAlpha(alpha));
            g.fillEllipse(x - width * 0.5f, y - height * 0.5f, width, height);
        }

        const float coreWidth = area.getWidth() * (0.22f + energy * 0.16f) * breathScale;
        const float coreHeight = area.getHeight() * (0.055f + energy * 0.035f) * (0.99f + microBreath * 0.03f);
        g.setColour(juce::Colour(234, 255, 214).withAlpha((0.030f + energy * 0.030f + glowEnergy * 0.018f) * (0.95f + microBreath * 0.08f)));
        g.fillEllipse(centre.x + driftX * 0.25f - coreWidth * 0.5f,
                      centre.y + driftY * 0.25f - coreHeight * 0.5f,
                      coreWidth,
                      coreHeight);
    }

    juce::Path buildPath(juce::Rectangle<float> area, const HistoryBuffer& history, float amplitudeScale, float phaseOffset)
    {
        juce::Path p;
        p.preallocateSpace(historySize * 3);
        for (size_t i = 0; i < history.size(); ++i)
        {
            auto point = sampleHistoryPoint(area, history, amplitudeScale, phaseOffset, (float) i);
            const float x = point.x;
            const float y = point.y;
            if (i == 0) p.startNewSubPath(x, y);
            else p.lineTo(x, y);
        }
        return p;
    }

    float readHistorySample(const HistoryBuffer& history, int logicalIndex) const
    {
        return history[(historyWritePosition + (size_t) logicalIndex) % history.size()];
    }

    juce::Point<float> sampleHistoryPoint(juce::Rectangle<float> area,
                                          const HistoryBuffer& history,
                                          float amplitudeScale,
                                          float phaseOffset,
                                          float historyIndex) const
    {
        const int sampleCount = (int) history.size();

        if (sampleCount <= 1)
            return area.getCentre();

        const float maxIndex = (float) (sampleCount - 1);
        const float clampedIndex = juce::jlimit(0.0f, maxIndex, historyIndex);
        const int index0 = (int) std::floor(clampedIndex);
        const int index1 = juce::jmin(index0 + 1, sampleCount - 1);
        const float mix = clampedIndex - (float) index0;
        const float sample0 = readHistorySample(history, index0);
        const float sample1 = readHistorySample(history, index1);
        const float interpolatedValue = sample0 + (sample1 - sample0) * mix;
        const float dx = area.getWidth() / maxIndex;
        const float x = area.getX() + dx * clampedIndex;
        const float organicMod = 1.0f
                               + 0.15f * std::sin(clampedIndex * 0.25f + phaseOffset)
                               + 0.08f * std::cos(clampedIndex * 0.45f - phaseOffset * 0.5f);
        const float y = area.getCentreY() - interpolatedValue * area.getHeight() * 0.40f * amplitudeScale * organicMod;
        return { x, y };
    }

    static juce::AffineTransform makeMirrorTransform(juce::Rectangle<float> area)
    {
        return juce::AffineTransform::translation(-area.getCentreX(), -area.getCentreY())
            .scaled(1.0f, -1.0f)
            .translated(area.getCentreX(), area.getCentreY());
    }

    void strokeMirroredPath(juce::Graphics& g,
                            juce::Rectangle<float> area,
                            const juce::Path& path,
                            const juce::PathStrokeType& stroke)
    {
        g.strokePath(path, stroke);

        juce::Graphics::ScopedSaveState mirroredState(g);
        g.addTransform(makeMirrorTransform(area));
        g.strokePath(path, stroke);
    }

    void drawEnergySeed(juce::Graphics& g,
                        juce::Rectangle<float> area,
                        const HistoryBuffer& history,
                        float amplitudeScale,
                        float phaseOffset,
                        float phase)
    {
        const float flowSpeed = 0.020f + glowEnergy * 0.035f;
        const int tailLength = 12; // Longer tail

        for (int i = tailLength; i >= 0; --i)
        {
            const float trailPhase = wrapNormalised(phase - (float) i * flowSpeed);
            const float sampleIndex = trailPhase * (float) (history.size() - 1);
            auto point = sampleHistoryPoint(area, history, amplitudeScale, phaseOffset, sampleIndex);

            const float t = 1.0f - (float) i / (float) tailLength;
            const float intensity = (0.25f + glowEnergy * 0.75f) * t;
            
            // Spirit-Seed aesthetics: Inner core + glowing shroud
            const float radius = (1.8f + glowEnergy * 6.5f) * (0.6f + t * 0.8f);
            const float glowRadius = radius * (2.8f + t * 2.5f);
            
            const auto colour = juce::Colour(120, 255, 170).interpolatedWith(juce::Colour(240, 255, 220), t * 0.4f);

            // Outer spirit glow
            g.setColour(colour.withAlpha(0.04f * intensity));
            g.fillEllipse(point.x - glowRadius, point.y - glowRadius, glowRadius * 2.0f, glowRadius * 2.0f);
            
            // Mid glow
            g.setColour(colour.withAlpha(0.18f * intensity));
            g.fillEllipse(point.x - radius * 1.8f, point.y - radius * 1.8f, radius * 3.6f, radius * 3.6f);
            
            // Bright plasma core for the 'head'
            if (i == 0)
            {
                g.setColour(juce::Colours::white.withAlpha(0.85f * intensity));
                g.fillEllipse(point.x - radius * 0.8f, point.y - radius * 0.8f, radius * 1.6f, radius * 1.6f);
            }
            else
            {
                g.setColour(colour.withAlpha(0.65f * intensity));
                g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
            }
        }
    }

    void drawMirroredEnergySeed(juce::Graphics& g,
                                juce::Rectangle<float> area,
                                const HistoryBuffer& history,
                                float amplitudeScale,
                                float phaseOffset,
                                float phase)
    {
        drawEnergySeed(g, area, history, amplitudeScale, phaseOffset, phase);

        juce::Graphics::ScopedSaveState mirroredState(g);
        g.addTransform(makeMirrorTransform(area));
        drawEnergySeed(g, area, history, amplitudeScale, phaseOffset, phase);
    }

    void drawAmbientBloom(juce::Graphics& g, juce::Rectangle<float> area)
    {
        juce::ignoreUnused(g, area);
    }


    void ensureMyceliumLayout(juce::Rectangle<float> area)
    {
        const auto layoutBounds = area.getSmallestIntegerContainer();

        if (! myceliumLayoutDirty && layoutBounds == myceliumLayoutBounds)
            return;

        rebuildMyceliumLayout(area);
    }

    void rebuildMyceliumLayout(juce::Rectangle<float> area)
    {
        myceliumLayoutBounds = area.getSmallestIntegerContainer();
        myceliumLayoutDirty = false;
        myceliumNodes.clear();
        myceliumEdges.clear();
        myceliumRowStarts.clear();
        myceliumRowCounts.clear();
        myceliumNodeCharge.clear();

        if (area.isEmpty())
            return;

        juce::Random layoutRandom(0x524f4f54);
        constexpr std::array<int, 6> rowNodeCounts { 2, 3, 3, 2, 2, 1 }; 
        
        std::vector<juce::Colour> greenPalette;
        if (currentColor == GrowthColor::emerald) {
            greenPalette = { juce::Colour(0, 255, 127), juce::Colour(50, 205, 50), juce::Colour(144, 238, 144) };
        } else if (currentColor == GrowthColor::neon) {
            greenPalette = { juce::Colour(57, 255, 20), juce::Colour(0, 255, 0), juce::Colour(172, 255, 0) };
        } else if (currentColor == GrowthColor::moss) {
            greenPalette = { juce::Colour(60, 220, 100), juce::Colour(40, 180, 80), juce::Colour(110, 140, 60) };
        } else { // forest
            greenPalette = { juce::Colour(34, 139, 34), juce::Colour(0, 100, 0), juce::Colour(107, 142, 35) };
        }

        for (size_t row = 0; row < rowNodeCounts.size(); ++row)
        {
            const int rowCount = rowNodeCounts[row];
            myceliumRowStarts.push_back((int) myceliumNodes.size());
            myceliumRowCounts.push_back(rowCount);

            const float rowT = (float) row / (float) (rowNodeCounts.size() - 1);
            const float y = area.getBottom() - area.getHeight() * (0.10f + rowT * 0.72f);
            const float spread = area.getWidth() * (0.18f + rowT * 0.30f);
            const float rowCurve = std::sin(rowT * juce::MathConstants<float>::pi) * area.getWidth() * 0.04f;

            for (int i = 0; i < rowCount; ++i)
            {
                const float slotT = rowCount > 1 ? (float) i / (float) (rowCount - 1) : 0.5f;
                const float centred = slotT * 2.0f - 1.0f;
                const float x = area.getCentreX()
                              + centred * spread
                              + rowCurve * centred
                              + (layoutRandom.nextFloat() - 0.5f) * area.getWidth() * 0.05f;
                const float yJitter = (layoutRandom.nextFloat() - 0.5f) * area.getHeight() * 0.026f;

                MycelNode node;
                node.anchor = { x, y + yJitter };
                node.radius = 1.4f + rowT * 1.8f + layoutRandom.nextFloat() * 0.9f;
                node.swayPhase = layoutRandom.nextFloat() * juce::MathConstants<float>::twoPi;
                node.swayAmount = 0.8f + layoutRandom.nextFloat() * (1.8f + rowT * 1.2f);
                node.depthBias = rowT;
                node.baseColour = greenPalette[(size_t) layoutRandom.nextInt((int) greenPalette.size())];
                myceliumNodes.push_back(node);
            }
        }

        auto addEdge = [this](int from, int to, float thickness, float conductivity, float curvature)
        {
            if (from == to)
                return;

            const int minNode = juce::jmin(from, to);
            const int maxNode = juce::jmax(from, to);
            for (const auto& edge : myceliumEdges)
                if (edge.from == minNode && edge.to == maxNode)
                    return;

            myceliumEdges.push_back({ minNode,
                                      maxNode,
                                      thickness,
                                      conductivity,
                                      curvature });
        };

        for (size_t row = 1; row < myceliumRowStarts.size(); ++row)
        {
            const int currentStart = myceliumRowStarts[row];
            const int currentCount = myceliumRowCounts[row];
            const int previousStart = myceliumRowStarts[row - 1];
            const int previousCount = myceliumRowCounts[row - 1];

            for (int i = 0; i < currentCount; ++i)
            {
                const int nodeIndex = currentStart + i;
                int nearestA = previousStart;
                int nearestB = previousStart;
                float nearestDistA = std::numeric_limits<float>::max();
                float nearestDistB = std::numeric_limits<float>::max();

                for (int j = 0; j < previousCount; ++j)
                {
                    const int candidateIndex = previousStart + j;
                    const float distance = myceliumNodes[nodeIndex].anchor.getDistanceFrom(myceliumNodes[candidateIndex].anchor);

                    if (distance < nearestDistA)
                    {
                        nearestDistB = nearestDistA;
                        nearestB = nearestA;
                        nearestDistA = distance;
                        nearestA = candidateIndex;
                    }
                    else if (distance < nearestDistB)
                    {
                        nearestDistB = distance;
                        nearestB = candidateIndex;
                    }
                }

                const float thickness = 0.55f + (float) row * 0.22f;
                // Higher curvature for root-like look
                addEdge(nodeIndex, nearestA, thickness, 0.62f + (float) row * 0.05f, (layoutRandom.nextFloat() - 0.5f) * 0.85f);
                if (i % 2 == 0 || row >= myceliumRowStarts.size() - 2)
                    addEdge(nodeIndex, nearestB, thickness * 0.88f, 0.52f + (float) row * 0.04f, (layoutRandom.nextFloat() - 0.5f) * 0.65f);

                if (i > 0)
                    addEdge(nodeIndex, nodeIndex - 1, 0.45f + (float) row * 0.12f, 0.42f + (float) row * 0.03f, (layoutRandom.nextFloat() - 0.5f) * 0.55f);
            }
        }

        myceliumNodeCharge.assign(myceliumNodes.size(), 0.0f);
        myceliumPulses.clear();
        spores.clear();
        processQueuedImpulseIfNeeded();
    }

    std::vector<juce::Point<float>> buildMyceliumPositions() const
    {
        std::vector<juce::Point<float>> positions;
        positions.reserve(myceliumNodes.size());

        const float anchorStillness = 1.0f - currentState.rootAnchor * 0.55f;
        const float swayScale = 0.8f
                              + currentState.pulseBreath * 1.4f
                              + currentState.sapFlow * 0.8f
                              + currentState.instability * 0.9f;

        for (const auto& node : myceliumNodes)
        {
            const float phase = myceliumDriftPhase * (0.78f + node.depthBias * 0.44f) + node.swayPhase;
            const float swayX = std::sin(phase) * node.swayAmount * swayScale * anchorStillness;
            const float swayY = std::cos(phase * 0.82f) * node.swayAmount * 0.22f * (0.65f + currentState.pulseBreath * 0.40f);
            positions.push_back({ node.anchor.x + swayX, node.anchor.y + swayY });
        }

        return positions;
    }

    juce::Point<float> getMyceliumControlPoint(const std::vector<juce::Point<float>>& positions, const MycelEdge& edge) const
    {
        const auto start = positions[(size_t) edge.from];
        const auto end = positions[(size_t) edge.to];
        const auto mid = juce::Point<float>((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f);
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length = std::sqrt(dx * dx + dy * dy);

        if (length <= 0.001f)
            return mid;

        const float nx = -dy / length;
        const float ny = dx / length;
        const float bend = length * edge.curvature;
        return { mid.x + nx * bend, mid.y + ny * bend };
    }

    float getMyceliumEdgeActivity(int edgeIndex) const
    {
        if (! juce::isPositiveAndBelow(edgeIndex, (int) myceliumEdges.size()))
            return 0.0f;

        const auto& edge = myceliumEdges[(size_t) edgeIndex];
        float activity = 0.0f;

        if (juce::isPositiveAndBelow(edge.from, (int) myceliumNodeCharge.size()))
            activity = juce::jmax(activity, myceliumNodeCharge[(size_t) edge.from]);
        if (juce::isPositiveAndBelow(edge.to, (int) myceliumNodeCharge.size()))
            activity = juce::jmax(activity, myceliumNodeCharge[(size_t) edge.to]);

        for (const auto& pulse : myceliumPulses)
        {
            const bool sameEdge = (pulse.fromNode == edge.from && pulse.toNode == edge.to)
                               || (pulse.fromNode == edge.to && pulse.toNode == edge.from);
            if (! sameEdge)
                continue;

            const float centreBoost = 1.0f - std::abs(pulse.progress * 2.0f - 1.0f) * 0.55f;
            activity = juce::jmax(activity, pulse.intensity * centreBoost);
        }

        return juce::jlimit(0.0f, 1.0f, activity);
    }

    void drawMyceliumNetwork(juce::Graphics& g, juce::Rectangle<float> area)
    {
        juce::ignoreUnused(area);

        if (myceliumNodes.empty() || myceliumEdges.empty())
            return;

        const auto positions = buildMyceliumPositions();
        const auto soilColour = juce::Colour(114, 88, 62);
        const auto vitalityColour = (currentColor == GrowthColor::emerald) ? juce::Colour(80, 255, 160) :
                                   (currentColor == GrowthColor::neon) ? juce::Colour(180, 255, 40) :
                                   (currentColor == GrowthColor::moss) ? juce::Colour(120, 180, 80) :
                                   juce::Colour(60, 140, 60);
        const auto rainColour = juce::Colour(112, 212, 255);
        const auto sunColour = juce::Colour(255, 215, 144);

        const auto threadColour = soilColour.interpolatedWith(vitalityColour, 0.16f + currentState.sapVitality * 0.34f)
                                            .interpolatedWith(rainColour, currentState.rain * 0.16f)
                                            .interpolatedWith(sunColour, currentState.sun * 0.12f);

        // --- Multi-Mode Root System with Peak-Snapping ---
        const float snapStrength = glowEnergy * 25.0f;
        auto getSnappedPos = [&](juce::Point<float> p) {
            if (glowEnergy < 0.02f) return p;
            const float centerY = area.getCentreY();
            const float distToCenter = std::abs(p.y - centerY);
            const float influence = std::exp(-distToCenter * 0.02f);
            return juce::Point<float>(p.x, p.y + (centerY - p.y) * influence * snapStrength * 0.05f);
        };

        for (size_t i = 0; i < myceliumEdges.size(); ++i)
        {
            const auto& edge = myceliumEdges[i];
            auto start = getSnappedPos(positions[(size_t) edge.from]);
            auto end = getSnappedPos(positions[(size_t) edge.to]);
            const auto rawControl = getMyceliumControlPoint(positions, edge);
            const auto control = getSnappedPos(rawControl);
            const float edgeActivity = getMyceliumEdgeActivity((int) i);

            juce::Path thread;
            thread.startNewSubPath(start);
            thread.quadraticTo(control, end);


            if (currentSpecies == SpeciesMode::lightFlow)
            {
                // Draw gnarled root path as a base
                g.setColour(threadColour.withAlpha(0.025f + edgeActivity * 0.05f));
                g.strokePath(thread, juce::PathStrokeType(edge.thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                // Light Flow Particles along the root
                const int particlesPerRoot = 4 + (int) (edgeActivity * 12.0f);
                const float tOffset = phaseA * (0.4f + currentState.sapFlow * 1.5f);

                for (int p = 0; p < particlesPerRoot; ++p)
                {
                    const float pT = wrapNormalised((float) p / (float) particlesPerRoot + tOffset);
                    const float u = 1.0f - pT;
                    const auto pos = juce::Point<float>(u * u * start.x + 2.0f * u * pT * control.x + pT * pT * end.x,
                                                        u * u * start.y + 2.0f * u * pT * control.y + pT * pT * end.y);
                    
                    const float pSize = (1.1f + edgeActivity * 2.8f) * (0.6f + std::sin(pT * 10.0f + phaseB) * 0.4f);
                    const float pAlpha = (0.08f + edgeActivity * 0.45f) * std::sin(pT * juce::MathConstants<float>::pi);
                    
                    const auto pColour = vitalityColour.interpolatedWith(juce::Colours::white, edgeActivity * 0.5f);
                    g.setColour(pColour.withAlpha(pAlpha * 0.4f));
                    g.fillEllipse(pos.x - pSize * 2.5f, pos.y - pSize * 2.5f, pSize * 5.0f, pSize * 5.0f);
                    g.setColour(pColour.withAlpha(pAlpha));
                    g.fillEllipse(pos.x - pSize, pos.y - pSize, pSize * 2.0f, pSize * 2.0f);
                }
            }
            else if (currentSpecies == SpeciesMode::classicSolid)
            {
                const float thickness = edge.thickness * (1.0f + edgeActivity * 0.8f);
                g.setColour(threadColour.withAlpha(0.15f + edgeActivity * 0.35f));
                g.strokePath(thread, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                
                if (edgeActivity > 0.05f)
                {
                    g.setColour(vitalityColour.withAlpha(edgeActivity * 0.4f));
                    g.strokePath(thread, juce::PathStrokeType(thickness * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
            }
            else // abstractGhost
            {
                const float pulse = 0.5f + 0.5f * std::sin(phaseA * 2.0f + (float) i);
                g.setColour(vitalityColour.withAlpha((0.05f + edgeActivity * 0.25f) * pulse));
                g.strokePath(thread, juce::PathStrokeType(0.8f + edgeActivity * 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // Restored & Enhanced: Lateral connections (Mycelial Haze)
        for (size_t r = 0; r < myceliumRowStarts.size(); ++r)
        {
            const int startIdx = myceliumRowStarts[r];
            const int rowCount = myceliumRowCounts[r];
            for (int i = 0; i < rowCount - 1; ++i)
            {
                const float c1 = myceliumNodeCharge[(size_t) (startIdx + i)];
                const float c2 = myceliumNodeCharge[(size_t) (startIdx + i + 1)];
                const float lateralActivity = c1 * c2;
                
                if (lateralActivity > 0.02f)
                {
                    const auto p1 = positions[(size_t) (startIdx + i)];
                    const auto p2 = positions[(size_t) (startIdx + i + 1)];
                    const auto mid = (p1 + p2) * 0.5f + juce::Point<float>(0, lateralActivity * 18.0f * std::sin(phaseB + (float) r));
                    
                    g.setColour(vitalityColour.withAlpha(lateralActivity * 0.12f));
                    g.drawLine(p1.x, p1.y, p2.x, p2.y, 0.45f);
                    
                    // Small glowing 'spark' in the middle of the haze
                    const float sparkX = p1.x + (p2.x - p1.x) * wrapNormalised(phaseA * 0.8f + (float) i);
                    const float sparkY = p1.y + (p2.y - p1.y) * wrapNormalised(phaseA * 0.8f + (float) i);
                    g.setColour(juce::Colours::white.withAlpha(lateralActivity * 0.4f));
                    g.fillEllipse(sparkX - 1.2f, sparkY - 1.2f, 2.4f, 2.4f);
                }
            }
        }

        for (size_t i = 0; i < myceliumNodes.size(); ++i)
        {
            const auto& node = myceliumNodes[i];
            auto point = positions[i];
            const float charge = myceliumNodeCharge[i];
            
            // Node vibration
            if (charge > 0.1f)
            {
                point.x += 1.4f * charge * std::sin(phaseA * 4.2f + (float) i);
                point.y += 1.4f * charge * std::cos(phaseA * 4.8f + (float) i);
            }

            const float radius = node.radius * (0.92f + currentState.rootDepth * 0.26f);
            const auto nodeColour = node.baseColour
                                        .interpolatedWith(juce::Colour(112, 212, 255), currentState.rain * 0.12f)
                                        .interpolatedWith(juce::Colour(255, 222, 156), currentState.sun * 0.10f);

            if (charge > 0.004f)
            {
                const float glowRadius = radius * (3.0f + charge * 3.5f);
                g.setColour(nodeColour.withAlpha(0.080f + charge * 0.22f));
                g.fillEllipse(point.x - glowRadius, point.y - glowRadius, glowRadius * 2.0f, glowRadius * 2.0f);
            }

            g.setColour(threadColour.withAlpha(0.13f + currentState.rootSoil * 0.08f));
            g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour(nodeColour.withAlpha(0.18f + charge * 0.36f));
            g.fillEllipse(point.x - radius * 0.55f, point.y - radius * 0.55f, radius * 1.1f, radius * 1.1f);
        }

        for (const auto& pulse : myceliumPulses)
        {
            if (! juce::isPositiveAndBelow(pulse.fromNode, (int) positions.size())
                || ! juce::isPositiveAndBelow(pulse.toNode, (int) positions.size()))
                continue;

            const auto start = positions[(size_t) pulse.fromNode];
            const auto end = positions[(size_t) pulse.toNode];
            const auto control = getMyceliumControlPoint(positions, myceliumEdges[(size_t) findMyceliumEdgeIndex(pulse.fromNode, pulse.toNode)]);
            const float t = juce::jlimit(0.0f, 1.0f, pulse.progress);
            const float u = 1.0f - t;
            const auto point = juce::Point<float>(u * u * start.x + 2.0f * u * t * control.x + t * t * end.x,
                                                  u * u * start.y + 2.0f * u * t * control.y + t * t * end.y);

            const auto pulseColour = (pulse.accent ? juce::Colour(255, 226, 156) : juce::Colour(164, 255, 196))
                                        .interpolatedWith(juce::Colour(112, 212, 255), currentState.rain * 0.14f)
                                        .interpolatedWith(juce::Colour(255, 220, 136), currentState.sun * 0.10f);
            const float radius = 1.4f + pulse.intensity * (2.6f + currentState.bloom * 1.8f);
            const float glowRadius = radius * (2.2f + pulse.intensity * 0.8f);

            g.setColour(pulseColour.withAlpha(0.08f + pulse.intensity * 0.18f));
            g.fillEllipse(point.x - glowRadius, point.y - glowRadius, glowRadius * 2.0f, glowRadius * 2.0f);
            g.setColour(pulseColour.withAlpha(0.28f + pulse.intensity * 0.32f));
            g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour(juce::Colour(252, 255, 240).withAlpha(0.64f + pulse.intensity * 0.20f));
            g.fillEllipse(point.x - radius * 0.42f, point.y - radius * 0.42f, radius * 0.84f, radius * 0.84f);
        }
    }

    void drawSporeField(juce::Graphics& g, juce::Rectangle<float> area)
    {
        juce::ignoreUnused(area);

        for (const auto& spore : spores)
        {
            const auto baseColour = (spore.accent ? juce::Colour(255, 230, 160) : juce::Colour(170, 255, 205))
                                     .interpolatedWith(juce::Colour(110, 210, 255), currentState.rain * 0.12f)
                                     .interpolatedWith(juce::Colour(255, 225, 140), currentState.sun * 0.10f);
            
            float radius = spore.size;
            float alpha = juce::jlimit(0.0f, 1.0f, spore.life) * (0.04f + spore.brightness * 0.28f);
            
            // --- WOW: Bokeh / Depth Rendering ---
            if (spore.layer == 0) // Background (Sharp, Small)
            {
                radius *= 0.65f;
                alpha *= 0.55f;
                g.setColour(baseColour.withAlpha(alpha));
                g.fillEllipse(spore.position.x - radius, spore.position.y - radius, radius * 2.0f, radius * 2.0f);
            }
            else if (spore.layer == 1) // Midground (Normal)
            {
                radius *= (0.85f + spore.life * 0.45f);
                const float glowRadius = radius * 1.85f;
                g.setColour(baseColour.withAlpha(alpha * 0.45f));
                g.fillEllipse(spore.position.x - glowRadius, spore.position.y - glowRadius, glowRadius * 2.0f, glowRadius * 2.0f);
                g.setColour(baseColour.withAlpha(alpha));
                g.fillEllipse(spore.position.x - radius, spore.position.y - radius, radius * 2.0f, radius * 2.0f);
            }
            else // Foreground Bokeh (Large, Soft)
            {
                radius *= (4.5f + (1.0f - spore.life) * 2.5f);
                alpha *= 0.22f; // Very subtitle and soft
                const float innerRadius = radius * 0.75f;
                
                // Outer soft ring
                g.setColour(baseColour.withAlpha(alpha * 0.35f)); 
                g.fillEllipse(spore.position.x - radius, spore.position.y - radius, radius * 2.0f, radius * 2.0f);
                // Slightly brighter center
                g.setColour(baseColour.withAlpha(alpha * 0.65f));
                g.fillEllipse(spore.position.x - innerRadius, spore.position.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);
            }
        }
    }

    int selectMyceliumLaunchNode(int noteNumber) const
    {
        if (myceliumNodes.empty())
            return -1;

        if (myceliumRowStarts.empty() || myceliumRowCounts.empty())
            return noteNumber >= 0 ? juce::jlimit(0, (int) myceliumNodes.size() - 1, noteNumber % (int) myceliumNodes.size()) : 0;

        const float normalizedPitch = juce::jlimit(0.0f, 1.0f, ((float) noteNumber - 24.0f) / 72.0f);
        const int rowIndex = juce::jlimit(0, (int) myceliumRowStarts.size() - 1,
                                          juce::roundToInt(normalizedPitch * (float) ((int) myceliumRowStarts.size() - 1)));
        const int rowStart = myceliumRowStarts[(size_t) rowIndex];
        const int rowCount = myceliumRowCounts[(size_t) rowIndex];
        const int slot = rowCount > 1 ? std::abs(noteNumber) % rowCount : 0;
        return rowStart + slot;
    }

    void processQueuedImpulseIfNeeded()
    {
        if (! hasQueuedImpulse || myceliumNodes.empty())
            return;

        hasQueuedImpulse = false;
        launchMyceliumImpulse(queuedImpulseNote, queuedImpulseIntensity, queuedImpulseMapped);
    }

    void launchMyceliumImpulse(int noteNumber, float intensity, bool wasMapped)
    {
        const int nodeIndex = selectMyceliumLaunchNode(noteNumber);
        if (! juce::isPositiveAndBelow(nodeIndex, (int) myceliumNodes.size()))
            return;

        if (juce::isPositiveAndBelow(nodeIndex, (int) myceliumNodeCharge.size()))
            myceliumNodeCharge[(size_t) nodeIndex] = juce::jmax(myceliumNodeCharge[(size_t) nodeIndex], 0.36f + intensity * 0.64f);

        const int hopCount = 1 + juce::roundToInt(currentState.pulseGrowth * 2.0f + currentState.canopy * 1.2f);
        propagateMyceliumPulse(nodeIndex, -1, intensity, hopCount, wasMapped);
        spawnSporesAtNode(nodeIndex, intensity, wasMapped);
    }

    void emitMyceliumPulse(int fromNode, int toNode, int previousNode, float intensity, float conductivity, int hopsRemaining, bool accent)
    {
        if (myceliumPulses.size() >= maxMyceliumPulses || intensity < 0.08f)
            return;

        MycelPulse pulse;
        pulse.fromNode = fromNode;
        pulse.toNode = toNode;
        pulse.previousNode = previousNode;
        pulse.progress = 0.0f;
        pulse.speed = 0.010f + currentState.pulseRate * 0.018f + conductivity * 0.010f;
        pulse.intensity = juce::jlimit(0.0f, 1.0f, intensity);
        pulse.hopsRemaining = juce::jmax(0, hopsRemaining);
        pulse.accent = accent;
        myceliumPulses.push_back(pulse);
    }

    void propagateMyceliumPulse(int sourceNode, int previousNode, float intensity, int hopsRemaining, bool accent)
    {
        if (sourceNode < 0 || intensity < 0.08f)
            return;

        const int maxBranches = 1 + juce::roundToInt(currentState.canopy * 1.4f);
        int emittedBranches = 0;

        for (const auto& edge : myceliumEdges)
        {
            int nextNode = -1;

            if (edge.from == sourceNode && edge.to != previousNode)
                nextNode = edge.to;
            else if (edge.to == sourceNode && edge.from != previousNode)
                nextNode = edge.from;

            if (nextNode < 0)
                continue;

            emitMyceliumPulse(sourceNode,
                              nextNode,
                              previousNode,
                              intensity * (0.72f + edge.conductivity * 0.18f),
                              edge.conductivity,
                              hopsRemaining,
                              accent);

            if (++emittedBranches >= maxBranches)
                break;
        }
    }

    void updateMyceliumPulses()
    {
        if (! myceliumNodeCharge.empty())
            for (auto& charge : myceliumNodeCharge)
            {
                charge *= 0.90f + currentState.rootAnchor * 0.035f;
                if (charge < 0.001f)
                    charge = 0.0f;
            }

        for (int i = (int) myceliumPulses.size() - 1; i >= 0; --i)
        {
            auto& pulse = myceliumPulses[(size_t) i];
            pulse.progress += pulse.speed * (0.86f + currentState.pulseRate * 1.30f + currentState.sapFlow * 0.32f);

            if (pulse.progress < 1.0f)
                continue;

            if (juce::isPositiveAndBelow(pulse.toNode, (int) myceliumNodeCharge.size()))
                myceliumNodeCharge[(size_t) pulse.toNode] = juce::jmax(myceliumNodeCharge[(size_t) pulse.toNode], pulse.intensity * 0.90f);

            if (pulse.hopsRemaining > 0)
                propagateMyceliumPulse(pulse.toNode,
                                       pulse.fromNode,
                                       pulse.intensity * (0.68f + currentState.pulseGrowth * 0.10f),
                                       pulse.hopsRemaining - 1,
                                       pulse.accent);

            myceliumPulses.erase(myceliumPulses.begin() + i);
        }
    }

    void spawnSporesAtNode(int nodeIndex, float intensity, bool accent)
    {
        if (! juce::isPositiveAndBelow(nodeIndex, (int) myceliumNodes.size()))
            return;

        const auto positions = buildMyceliumPositions();
        const auto origin = positions[(size_t) nodeIndex];
        const int particleCount = 3 + juce::roundToInt(intensity * 4.0f + currentState.bloom * 4.0f);

        for (int i = 0; i < particleCount && spores.size() < maxSpores; ++i)
        {
            const float angle = -juce::MathConstants<float>::halfPi + (random.nextFloat() - 0.5f) * 1.15f;
            const float speed = 0.22f + intensity * 0.78f + random.nextFloat() * 0.42f;

            SporeParticle particle;
            particle.position = origin;
            particle.velocity = { std::cos(angle) * speed * (0.45f + currentState.rain * 0.16f),
                                  std::sin(angle) * speed * (0.88f + currentState.sun * 0.10f) };
            
            // Randomly assign layer for Depth/Bokeh effect
            const float lRand = random.nextFloat();
            if (lRand < 0.22f) particle.layer = 0;      // Background
            else if (lRand < 0.90f) particle.layer = 1; // Midground
            else particle.layer = 2;                    // Bokeh Foreground (sparse)

            particle.life = 0.48f + random.nextFloat() * 0.32f + intensity * 0.22f;
            particle.size = 1.2f + random.nextFloat() * (1.6f + currentState.bloom * 1.4f);
            
            if (particle.layer == 2) {
                particle.velocity *= 0.45f; // Foreground particles move slower (parallax)
                particle.life *= 1.45f;    // And last longer
            }

            particle.brightness = 0.36f + intensity * 0.42f + random.nextFloat() * 0.22f;
            particle.accent = accent;
            spores.push_back(particle);
        }
    }

    void updateSporeField()
    {
        const float horizontalWind = std::sin(myceliumDriftPhase * 0.82f) * (0.010f + currentState.rain * 0.020f);
        const float upwardLift = 0.010f + currentState.bloom * 0.018f + currentState.sun * 0.006f;

        for (int i = (int) spores.size() - 1; i >= 0; --i)
        {
            auto& spore = spores[(size_t) i];
            spore.velocity.x += (horizontalWind - spore.velocity.x * 0.08f) * 0.020f;
            spore.velocity.y -= upwardLift * (0.22f + spore.brightness * 0.12f);
            spore.position += spore.velocity;
            spore.life -= 0.010f + currentState.sun * 0.002f + currentState.rain * 0.001f;

            if (spore.life <= 0.0f)
                spores.erase(spores.begin() + i);
        }
    }

    void maybeSpawnAmbientSpores()
    {
        if (myceliumNodes.empty() || spores.size() >= maxSpores)
            return;

        const float chance = 0.004f
                           + currentState.bloom * 0.010f
                           + ambientEnergy * 0.006f
                           + currentState.atmosphere * 0.004f;
        if (random.nextFloat() >= chance)
            return;

        const int rowIndex = juce::jlimit(0, (int) myceliumRowStarts.size() - 1,
                                          juce::roundToInt((float) (myceliumRowStarts.size() - 1) * (0.45f + currentState.canopy * 0.40f)));
        const int rowStart = myceliumRowStarts[(size_t) rowIndex];
        const int rowCount = myceliumRowCounts[(size_t) rowIndex];
        const int nodeIndex = rowStart + juce::jlimit(0, rowCount - 1, random.nextInt(juce::jmax(1, rowCount)));

        spawnSporesAtNode(nodeIndex, 0.18f + ambientEnergy * 0.18f, false);
    }

    int findMyceliumEdgeIndex(int fromNode, int toNode) const
    {
        const int minNode = juce::jmin(fromNode, toNode);
        const int maxNode = juce::jmax(fromNode, toNode);

        for (size_t i = 0; i < myceliumEdges.size(); ++i)
            if (myceliumEdges[i].from == minNode && myceliumEdges[i].to == maxNode)
                return (int) i;

        return 0;
    }

    static float wrapNormalised(float value)
    {
        while (value < 0.0f)
            value += 1.0f;
        while (value >= 1.0f)
            value -= 1.0f;
        return value;
    }

    float getSpectralBand(int start, int end) const
    {
        float peak = 0.0f;
        const int cappedEnd = juce::jmin(end, (int) spectrum.size());
        for (int i = juce::jmax(0, start); i < cappedEnd; ++i)
            peak = juce::jmax(peak, spectrum[(size_t) i]);
        return juce::jlimit(0.0f, 1.0f, peak);
    }

    HistoryBuffer historyA {};
    HistoryBuffer historyB {};
    HistoryBuffer historyC {};
    HistoryBuffer historyD {};
    HistoryBuffer historyE {};
    HistoryBuffer hazeHistoryA {};
    HistoryBuffer hazeHistoryB {};
    std::array<float, 512> spectrum {};
    juce::Image staticLayer;
    float staticLayerScale = 1.0f;
    bool staticLayerDirty = true;
    juce::Rectangle<int> myceliumLayoutBounds;
    bool myceliumLayoutDirty = true;
    size_t historyWritePosition = 0;
    float targetEnergy = 0.0f, smoothedEnergy = 0.0f;
    float ambientEnergy = 0.0f;
    float phaseA = 0.0f, phaseB = 1.4f, phaseC = 2.6f;
    float glowEnergy = 0.0f;
    float coreValueA = 0.0f, coreValueB = 0.0f, coreValueC = 0.0f, coreValueD = 0.0f, coreValueE = 0.0f;
    float hazeValueA = 0.0f, hazeValueB = 0.0f;
    float energySeedPhase = 0.0f;
    float ambientFieldPhase = 0.0f;
    float myceliumDriftPhase = 0.0f;
    VisualizerState targetState {};
    VisualizerState currentState {};
    std::vector<MycelNode> myceliumNodes;
    std::vector<MycelEdge> myceliumEdges;
    std::vector<int> myceliumRowStarts;
    std::vector<int> myceliumRowCounts;
    std::vector<float> myceliumNodeCharge;
    std::vector<MycelPulse> myceliumPulses;
    std::vector<SporeParticle> spores;
    juce::Random random { 0x53504f52 };
    bool hasQueuedImpulse = false;
    int queuedImpulseNote = -1;
    float queuedImpulseIntensity = 0.0f;
    bool queuedImpulseMapped = false;
    static constexpr size_t maxMyceliumPulses = 96;
    static constexpr size_t maxSpores = 480;

    SpeciesMode currentSpecies = SpeciesMode::lightFlow;
    GrowthColor currentColor = GrowthColor::emerald;
};
