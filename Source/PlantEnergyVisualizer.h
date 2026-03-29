#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>
#include "UI/Utils/DesignTokens.h"

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


    PlantEnergyVisualizer (RootFlowAudioProcessor& p) : processor (p)
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

        const float energyScale = 0.75f + (smoothedEnergy * 0.28f + glowEnergy * 0.14f);

        // Main Energy Weaves
        drawMirroredEnergySeed(g, graphArea, historyA, 1.0f * energyScale, 0.0f, energySeedPhase);
        drawMirroredEnergySeed(g, graphArea, historyB, 0.72f * energyScale, phaseA, energySeedPhase * 1.15f);
        drawMirroredEnergySeed(g, graphArea, historyC, 0.44f * energyScale, phaseB, energySeedPhase * 0.82f);
        drawMirroredEnergySeed(g, graphArea, historyD, 0.22f * energyScale, phaseC, energySeedPhase * 1.45f);

        drawMyceliumLinks(g);
        drawMyceliumPulses(g);
        drawMyceliumNodes(g);
        drawSporeField(g);

        drawSpectralFilaments(g, graphArea);
        drawBioSequencer(g, bounds);

        showGrid(g, graphArea);
    }

    void resized() override
    {
        staticLayerDirty = true;
        myceliumLayoutDirty = true;
    }

    void timerCallback() override
    {
        updateAnimationTimerState();
        updateVisualizer();
        repaint();
    }

private:
    struct MycelNode { juce::Point<float> anchor; float radius; float swayPhase; float swayAmount; float depthBias; juce::Colour baseColour; };
    struct MycelEdge { int from; int to; float length; float thickness; float stiffness; };
    struct MycelPulse { int fromNode; int toNode; float progress; float speed; float intensity; bool accent; int hopsRemaining; };
    struct SporeParticle { juce::Point<float> position; juce::Point<float> velocity; float life; float size; float brightness; int layer; bool accent; };

    juce::Rectangle<float> getGraphArea(juce::Rectangle<float> bounds) const
    {
        return bounds.reduced(bounds.getWidth() * 0.06f, bounds.getHeight() * 0.08f);
    }

    void updateAnimationTimerState()
    {
        if (isShowing()) {
            if (! isTimerRunning()) startTimerHz(60);
        } else {
            stopTimer();
        }
    }

    void updateVisualizer()
    {
        const float smoothing = 0.065f;
        smoothedEnergy += (targetEnergy - smoothedEnergy) * smoothing;
        ambientEnergy += (smoothedEnergy * 0.75f - ambientEnergy) * 0.015f;
        glowEnergy += (currentState.audioEnergy * 0.85f - glowEnergy) * 0.08f;

        energySeedPhase += 0.024f + smoothedEnergy * 0.052f + currentState.sapFlow * 0.035f;
        ambientFieldPhase += 0.012f + currentState.atmosphere * 0.025f;
        myceliumDriftPhase += 0.008f + currentState.instability * 0.012f;

        currentState.plantEnergy = smoothedEnergy;
        currentState.audioEnergy = currentState.audioEnergy * 0.82f;
        currentState.currentSequencerStep = targetState.currentSequencerStep;
        currentState.sequencerOn = targetState.sequencerOn;
        currentState.sequencerStepActive = targetState.sequencerStepActive;
        currentState.rootDepth = targetState.rootDepth;
        currentState.rootSoil = targetState.rootSoil;
        currentState.rootAnchor = targetState.rootAnchor;
        currentState.sapFlow = targetState.sapFlow;
        currentState.sapVitality = targetState.sapVitality;
        currentState.sapTexture = targetState.sapTexture;
        currentState.pulseRate = targetState.pulseRate;
        currentState.pulseBreath = targetState.pulseBreath;
        currentState.pulseGrowth = targetState.pulseGrowth;
        currentState.canopy = targetState.canopy;
        currentState.atmosphere = targetState.atmosphere;
        currentState.instability = targetState.instability;
        currentState.bloom = targetState.bloom;
        currentState.rain = targetState.rain;
        currentState.sun = targetState.sun;

        const float time = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        const float basePhaseA = time * (0.85f + currentState.pulseRate * 0.5f);
        const float basePhaseB = time * (1.25f + currentState.sapFlow * 0.4f);

        const float activity = 0.25f + smoothedEnergy * 0.75f;
        coreValueA = std::sin(basePhaseA) * activity;
        coreValueB = std::cos(basePhaseA * 0.65f + 1.2f) * activity;
        coreValueC = std::sin(basePhaseB * 0.82f - 0.5f) * activity;
        coreValueD = std::cos(basePhaseA * 1.34f + 2.2f) * activity;
        coreValueE = std::sin(basePhaseB * 1.15f + 3.1f) * activity;

        hazeValueA = std::sin(time * 0.32f) * (0.4f + currentState.atmosphere * 0.6f);
        hazeValueB = std::cos(time * 0.44f + 0.7f) * (0.4f + currentState.atmosphere * 0.6f);

        updateMyceliumPulses();

        if (random.nextFloat() < (0.008f + smoothedEnergy * 0.012f + currentState.pulseRate * 0.008f))
        {
            if (! myceliumNodes.empty())
            {
                const int node = random.nextInt((int) myceliumNodes.size());
                const float intensity = 0.35f + random.nextFloat() * 0.55f;
                launchMyceliumPulse(node, -1, intensity, false, 2 + random.nextInt(3));
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
            else if (isActive) dotCol = RootFlow::accent.withAlpha(0.4f + energy * 0.4f);
            else dotCol = RootFlow::accent.withAlpha(0.1f);

            if (isActive)
            {
                const float pulse = 1.0f + 0.3f * std::sin(ambientFieldPhase * 3.0f + (float)i);
                dotSize *= (0.85f + pulse * 0.15f * energy);
            }

            if (isActive || isCurrent)
            {
                g.setColour(dotCol.withAlpha(isActive ? 0.15f : 0.25f));
                g.fillEllipse(x - dotSize * 2.0f, y - dotSize * 2.0f, dotSize * 4.0f, dotSize * 4.0f);
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
        for (int i = 0; i <= 8; ++i)
        {
            float x = area.getX() + area.getWidth() * ((float)i / 8.0f);
            g.setColour(RootFlow::accent.withAlpha(0.04f));
            g.drawLine(x, area.getY(), x, area.getBottom(), 0.5f);
        }

        for (int i = 0; i <= 5; ++i)
        {
            float y = area.getY() + area.getHeight() * ((float)i / 5.0f);
            g.setColour(RootFlow::accent.withAlpha(0.04f));
            g.drawLine(area.getX(), y, area.getRight(), y, 0.5f);
        }
    }

    void drawSpectralFilaments(juce::Graphics& g, juce::Rectangle<float> area)
    {
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

            g.setColour(RootFlow::accent.withAlpha(0.05f + shaped * 0.15f));
            g.drawLine(x, area.getCentreY() - halfHeight, x, area.getCentreY() + halfHeight, 1.2f);
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

            g.setColour(RootFlow::accent.withAlpha(alpha));
            g.fillEllipse(x - width * 0.5f, y - height * 0.5f, width, height);
        }

        const float coreWidth = area.getWidth() * (0.22f + energy * 0.16f) * breathScale;
        const float coreHeight = area.getHeight() * (0.055f + energy * 0.035f) * (0.99f + microBreath * 0.03f);
        g.setColour(juce::Colours::white.withAlpha((0.030f + energy * 0.030f + glowEnergy * 0.018f) * (0.95f + microBreath * 0.08f)));
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
        return juce::AffineTransform::verticalFlip(area.getHeight()).translated(0, area.getY() * 2.0f);
    }

    void drawEnergySeed(juce::Graphics& g,
                        juce::Rectangle<float> area,
                        const HistoryBuffer& history,
                        float amplitudeScale,
                        float phaseOffset,
                        float phase)
    {
        auto p = buildPath(area, history, amplitudeScale, phaseOffset);

        // --- Bio-Tech Upgrade: Dual-Pass Glow ---
        // 1. Glow Layer (Soft bio-field)
        g.setColour(RootFlow::accent.withAlpha(0.12f));
        g.strokePath(p, juce::PathStrokeType(5.0f));

        // 2. Core Layer (Sharp pulse)
        g.setColour(RootFlow::accent.withAlpha(0.85f));
        g.strokePath(p, juce::PathStrokeType(1.5f));

        const size_t segmentCount = 12;
        const auto colour = RootFlow::accent;
        const float microTime = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;

        for (size_t i = 0; i < segmentCount; ++i)
        {
            const float t = (float) i / (float) (segmentCount - 1);
            const float historyIndex = t * (float) (historySize - 1);
            const auto point = sampleHistoryPoint(area, history, amplitudeScale, phaseOffset, historyIndex);

            const float pulse = 0.5f + 0.5f * std::sin(microTime * 4.2f + t * 8.0f + phase);
            const float intensity = (0.22f + pulse * 0.78f) * (0.15f + (1.0f - t) * 0.85f);
            const float radius = (1.5f + pulse * 0.82f) * (0.95f + (1.0f - t) * 1.15f);

            // Outer Bio-Glow Dot
            g.setColour(colour.withAlpha(0.18f * intensity));
            g.fillEllipse(point.x - radius * 2.2f, point.y - radius * 2.2f, radius * 4.4f, radius * 4.4f);

            // Bright Bio-Core
            if (i == 0) g.setColour(juce::Colours::white.withAlpha(0.9f * intensity));
            else g.setColour(colour.withAlpha(0.75f * intensity));

            g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
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
                node.baseColour = RootFlow::accent.interpolatedWith(juce::Colours::white, layoutRandom.nextFloat() * 0.3f);
                myceliumNodes.push_back(node);
                myceliumNodeCharge.push_back(0.0f);
            }

            if (row > 0)
            {
                const int prevRowStart = myceliumRowStarts[row - 1];
                const int prevRowCount = myceliumRowCounts[row - 1];
                const int currentRowStart = myceliumRowStarts[row];
                const int currentRowCount = myceliumRowCounts[row];

                for (int i = 0; i < currentRowCount; ++i)
                {
                    const int target = currentRowStart + i;
                    const int source = prevRowStart + layoutRandom.nextInt(prevRowCount);
                    addEdge(source, target, layoutRandom);
                    if (layoutRandom.nextFloat() < 0.35f)
                        addEdge(prevRowStart + layoutRandom.nextInt(prevRowCount), target, layoutRandom);
                }
            }
        }
    }

    void addEdge(int from, int to, juce::Random& r)
    {
        MycelEdge edge;
        edge.from = from;
        edge.to = to;
        edge.length = myceliumNodes[(size_t) from].anchor.getDistanceFrom(myceliumNodes[(size_t) to].anchor);
        edge.thickness = 0.8f + r.nextFloat() * 1.4f;
        edge.stiffness = 0.45f + r.nextFloat() * 0.35f;
        myceliumEdges.push_back(edge);
    }

    std::vector<juce::Point<float>> buildMyceliumPositions() const
    {
        std::vector<juce::Point<float>> positions;
        positions.reserve(myceliumNodes.size());

        for (const auto& node : myceliumNodes)
        {
            const float sway = std::sin(myceliumDriftPhase + node.swayPhase) * node.swayAmount * (0.85f + smoothedEnergy * 0.45f);
            positions.push_back({ node.anchor.x + sway, node.anchor.y + sway * 0.35f });
        }

        return positions;
    }

    void drawMyceliumLinks(juce::Graphics& g)
    {
        if (myceliumEdges.empty()) return;

        const auto positions = buildMyceliumPositions();
        const float energy = smoothedEnergy;

        for (const auto& edge : myceliumEdges)
        {
            const auto p1 = positions[(size_t) edge.from];
            const auto p2 = positions[(size_t) edge.to];
            const float charge = (myceliumNodeCharge[(size_t) edge.from] + myceliumNodeCharge[(size_t) edge.to]) * 0.5f;

            g.setColour(RootFlow::accent.withAlpha(0.08f + charge * 0.22f + energy * 0.06f));
            g.drawLine(p1.x, p1.y, p2.x, p2.y, edge.thickness * (0.92f + charge * 0.65f));

            if (charge > 0.15f)
            {
                g.setColour(juce::Colours::white.withAlpha(charge * 0.12f));
                g.drawLine(p1.x, p1.y, p2.x, p2.y, edge.thickness * 0.45f);
            }
        }
    }

    void drawMyceliumNodes(juce::Graphics& g)
    {
        const auto positions = buildMyceliumPositions();
        const float energy = smoothedEnergy;

        for (size_t i = 0; i < myceliumNodes.size(); ++i)
        {
            const auto& node = myceliumNodes[i];
            const auto pos = positions[i];
            const float charge = myceliumNodeCharge[i];
            const float pulse = 0.95f + 0.12f * std::sin(myceliumDriftPhase * 2.2f + node.swayPhase);
            const float r = node.radius * pulse * (1.0f + charge * 0.55f);

            g.setColour(node.baseColour.withAlpha(0.18f + charge * 0.35f + energy * 0.12f));
            g.fillEllipse(pos.x - r * 1.8f, pos.y - r * 1.8f, r * 3.6f, r * 3.6f);

            g.setColour(node.baseColour.interpolatedWith(juce::Colours::white, 0.4f + charge * 0.5f).withAlpha(0.65f + charge * 0.35f));
            g.fillEllipse(pos.x - r, pos.y - r, r * 2.0f, r * 2.0f);

            if (charge > 0.45f)
            {
                g.setColour(juce::Colours::white.withAlpha((charge - 0.45f) * 0.82f));
                g.fillEllipse(pos.x - r * 0.45f, pos.y - r * 0.45f, r * 0.9f, r * 0.9f);
            }
        }
    }

    void drawMyceliumPulses(juce::Graphics& g)
    {
        if (myceliumPulses.empty()) return;

        const auto positions = buildMyceliumPositions();
        for (const auto& pulse : myceliumPulses)
        {
            const auto p1 = positions[(size_t) pulse.fromNode];
            const auto p2 = positions[(size_t) pulse.toNode];
            const auto pos = p1 + (p2 - p1) * pulse.progress;

            const float r = (1.8f + pulse.intensity * 1.6f) * (0.85f + 0.25f * std::sin(pulse.progress * juce::MathConstants<float>::pi));
            const auto col = pulse.accent ? juce::Colours::white : RootFlow::accent;

            g.setColour(col.withAlpha(0.22f * pulse.intensity));
            g.fillEllipse(pos.x - r * 2.4f, pos.y - r * 2.4f, r * 4.8f, r * 4.8f);
            g.setColour(col.withAlpha(0.85f * pulse.intensity));
            g.fillEllipse(pos.x - r, pos.y - r, r * 2.0f, r * 2.0f);
        }
    }

    void drawSporeField(juce::Graphics& g)
    {
        for (const auto& spore : spores)
        {
            const float lifeFade = std::sin(juce::jlimit(0.0f, 1.0f, spore.life / 0.8f) * juce::MathConstants<float>::pi);
            const float alpha = spore.brightness * lifeFade * (spore.layer == 0 ? 0.35f : (spore.layer == 2 ? 0.95f : 0.65f));

            juce::Colour col = RootFlow::accent;
            if (spore.accent) col = juce::Colours::white;

            g.setColour(col.withAlpha(alpha));
            const float size = spore.size * (0.85f + 0.35f * std::sin(myceliumDriftPhase * 1.4f + spore.position.x * 0.01f));

            if (spore.layer == 2) {
                g.setColour(col.withAlpha(alpha * 0.32f));
                g.fillEllipse(spore.position.x - size * 2.2f, spore.position.y - size * 2.2f, size * 4.4f, size * 4.4f);
            }

            g.fillEllipse(spore.position.x - size * 0.5f, spore.position.y - size * 0.5f, size, size);
        }
    }

    void launchMyceliumPulse(int fromNode, int excludeToNode, float intensity, bool accent, int hops)
    {
        if (! juce::isPositiveAndBelow(fromNode, (int) myceliumNodes.size()) || myceliumPulses.size() >= maxMyceliumPulses)
            return;

        std::vector<int> targets;
        for (const auto& edge : myceliumEdges)
        {
            if (edge.from == fromNode && edge.to != excludeToNode) targets.push_back(edge.to);
            else if (edge.to == fromNode && edge.from != excludeToNode) targets.push_back(edge.from);
        }

        if (targets.empty()) return;

        const int targetNode = targets[(size_t) random.nextInt((int) targets.size())];

        MycelPulse pulse;
        pulse.fromNode = fromNode;
        pulse.toNode = targetNode;
        pulse.progress = 0.0f;
        pulse.speed = 0.025f + random.nextFloat() * 0.035f;
        pulse.intensity = intensity;
        pulse.accent = accent;
        pulse.hopsRemaining = hops;
        myceliumPulses.push_back(pulse);

        if (random.nextFloat() < 0.28f * intensity)
            spawnSporesAtNode(fromNode, intensity * 0.72f, accent);
    }

    void propagateMyceliumPulse(int fromNode, int excludeToNode, float intensity, int hops, bool accent)
    {
        if (hops <= 0 || intensity < 0.06f) return;
        launchMyceliumPulse(fromNode, excludeToNode, intensity, accent, hops);
    }

    void processQueuedImpulseIfNeeded()
    {
        if (! hasQueuedImpulse || myceliumNodes.empty()) return;
        launchMyceliumImpulse(queuedImpulseNote, queuedImpulseIntensity, queuedImpulseMapped);
        hasQueuedImpulse = false;
    }

    void launchMyceliumImpulse(int noteNumber, float intensity, bool wasMapped)
    {
        juce::ignoreUnused(noteNumber);
        const int nodeCount = (int) myceliumNodes.size();
        const int startNode = random.nextInt(juce::jmin(nodeCount, 4)); // Start near bottom
        launchMyceliumPulse(startNode, -1, intensity, wasMapped, 4 + random.nextInt(3));
        globalBeatCharge = juce::jmax(globalBeatCharge, intensity);
    }

    void ensureStaticLayer(juce::Rectangle<float> bounds, juce::Rectangle<float> graphArea)
    {
        const int w = juce::roundToInt(bounds.getWidth());
        const int h = juce::roundToInt(bounds.getHeight());
        if (! staticLayerDirty && staticLayer.isValid() && staticLayer.getWidth() == w && staticLayer.getHeight() == h)
            return;

        staticLayer = juce::Image(juce::Image::ARGB, w, h, true);
        juce::Graphics g(staticLayer);
        staticLayerDirty = false;
    }

    void drawCachedStaticLayer(juce::Graphics& g)
    {
        if (staticLayer.isValid())
            g.drawImageAt(staticLayer, 0, 0);
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

            const float lRand = random.nextFloat();
            if (lRand < 0.22f) particle.layer = 0;
            else if (lRand < 0.90f) particle.layer = 1;
            else particle.layer = 2;

            particle.life = 0.48f + random.nextFloat() * 0.32f + intensity * 0.22f;
            particle.size = 1.2f + random.nextFloat() * (1.6f + currentState.bloom * 1.4f);

            if (particle.layer == 2) {
                particle.velocity *= 0.45f;
                particle.life *= 1.45f;
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
        while (value < 0.0f) value += 1.0f;
        while (value >= 1.0f) value -= 1.0f;
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

    HistoryBuffer historyA {}, historyB {}, historyC {}, historyD {}, historyE {};
    HistoryBuffer hazeHistoryA {}, hazeHistoryB {};
    std::array<float, 512> spectrum {};
    juce::Image staticLayer;
    float staticLayerScale = 1.0f;
    bool staticLayerDirty = true;
    juce::Rectangle<int> myceliumLayoutBounds;
    bool myceliumLayoutDirty = true;
    size_t historyWritePosition = 0;
    float targetEnergy = 0.0f, smoothedEnergy = 0.0f, ambientEnergy = 0.0f, glowEnergy = 0.0f;
    float phaseA = 0.0f, phaseB = 1.4f, phaseC = 2.6f;
    float coreValueA = 0.0f, coreValueB = 0.0f, coreValueC = 0.0f, coreValueD = 0.0f, coreValueE = 0.0f;
    float hazeValueA = 0.0f, hazeValueB = 0.0f;
    float energySeedPhase = 0.0f, ambientFieldPhase = 0.0f, myceliumDriftPhase = 0.0f;
    VisualizerState targetState {}, currentState {};
    std::vector<MycelNode> myceliumNodes;
    std::vector<MycelEdge> myceliumEdges;
    std::vector<int> myceliumRowStarts, myceliumRowCounts;
    std::vector<float> myceliumNodeCharge;
    std::vector<MycelPulse> myceliumPulses;
    std::vector<SporeParticle> spores;
    juce::Random random { 0x53504f52 };
    bool hasQueuedImpulse = false;
    int queuedImpulseNote = -1;
    float queuedImpulseIntensity = 0.0f;
    bool queuedImpulseMapped = false;
    static constexpr size_t maxMyceliumPulses = 96, maxSpores = 480;

    RootFlowAudioProcessor& processor;
    float globalBeatCharge = 0.0f;
    SpeciesMode currentSpecies = SpeciesMode::lightFlow;
    GrowthColor currentColor = GrowthColor::emerald;
};
