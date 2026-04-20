#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>
#include "UI/Utils/DesignTokens.h"

class MatrixEnergyVisualizer : public juce::Component, public juce::Timer
{
private:
    static constexpr int historySize = 144;
    using HistoryBuffer = std::array<float, historySize>;

public:
    struct VisualizerState
    {
        float systemEnergy = 0.0f;
        float audioEnergy = 0.0f;
        float sourceDepth = 0.5f;
        float sourceCore = 0.5f;
        float sourceAnchor = 0.5f;
        float flowRate = 0.5f;
        float flowEnergy = 0.5f;
        float flowTexture = 0.5f;
        float pulseFrequency = 0.5f;
        float pulseWidth = 0.5f;
        float pulseEnergy = 0.5f;
        float fieldComplexity = 0.5f;
        float fieldDepth = 0.5f;
        float instability = 0.5f;
        float radiance = 0.0f;
        float charge = 0.0f;
        float discharge = 0.0f;
        float seasonMacro = 0.5f;
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
        rebuildMatrixLayout(getGraphArea(getLocalBounds().toFloat()));
    }


    MatrixEnergyVisualizer (RootFlowAudioProcessor& p) : processor (p)
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

        if (matrixNodes.empty())
        {
            queuedImpulseNote = noteNumber;
            queuedImpulseIntensity = clampedIntensity;
            queuedImpulseMapped = wasMapped;
            hasQueuedImpulse = true;
            return;
        }

        launchMatrixImpulse(noteNumber, clampedIntensity, wasMapped);
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
        ensureMatrixLayout(graphArea);
        drawCachedStaticLayer(g);

        drawAmbientField(g, graphArea, microBreath);
        drawAmbientBloom(g, graphArea);

        const float energyScale = 0.75f + (smoothedEnergy * 0.28f + glowEnergy * 0.14f);

        // Main Energy Weaves
        drawMirroredEnergySeed(g, graphArea, historyA, 1.0f * energyScale, 0.0f, energySeedPhase);
        drawMirroredEnergySeed(g, graphArea, historyB, 0.72f * energyScale, phaseA, energySeedPhase * 1.15f);
        drawMirroredEnergySeed(g, graphArea, historyC, 0.44f * energyScale, phaseB, energySeedPhase * 0.82f);
        drawMirroredEnergySeed(g, graphArea, historyD, 0.22f * energyScale, phaseC, energySeedPhase * 1.45f);

        drawMatrixLinks(g);
        drawMatrixPulses(g);
        drawMatrixNodes(g);
        drawBurstField(g);

        drawSpectralFilaments(g, graphArea);
        drawPulseMatrixSequencerVisuals(g, bounds);

        showGrid(g, graphArea);
    }

    void resized() override
    {
        staticLayerDirty = true;
        matrixLayoutDirty = true;
    }

    void timerCallback() override
    {
        updateAnimationTimerState();
        updateVisualizer();
        repaint();
    }

private:
    struct MatrixNode { juce::Point<float> anchor; float radius; float swayPhase; float swayAmount; float depthBias; juce::Colour baseColour; };
    struct MatrixEdge { int from; int to; float length; float thickness; float stiffness; };
    struct MatrixPulse { int fromNode; int toNode; float progress; float speed; float intensity; bool accent; int hopsRemaining; };
    struct BurstParticle { juce::Point<float> position; juce::Point<float> velocity; float life; float size; float brightness; int layer; bool accent; };

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

        energySeedPhase += 0.024f + smoothedEnergy * 0.052f + currentState.flowRate * 0.035f;
        ambientFieldPhase += 0.012f + currentState.fieldDepth * 0.025f;
        matrixDriftPhase += 0.008f + currentState.instability * 0.012f;

        currentState.systemEnergy = smoothedEnergy;
        currentState.audioEnergy = currentState.audioEnergy * 0.82f;
        currentState.currentSequencerStep = targetState.currentSequencerStep;
        currentState.sequencerOn = targetState.sequencerOn;
        currentState.sequencerStepActive = targetState.sequencerStepActive;
        currentState.sourceDepth = targetState.sourceDepth;
        currentState.sourceCore = targetState.sourceCore;
        currentState.sourceAnchor = targetState.sourceAnchor;
        currentState.flowRate = targetState.flowRate;
        currentState.flowEnergy = targetState.flowEnergy;
        currentState.flowTexture = targetState.flowTexture;
        currentState.pulseFrequency = targetState.pulseFrequency;
        currentState.pulseWidth = targetState.pulseWidth;
        currentState.pulseEnergy = targetState.pulseEnergy;
        currentState.fieldComplexity = targetState.fieldComplexity;
        currentState.fieldDepth = targetState.fieldDepth;
        currentState.instability = targetState.instability;
        currentState.radiance = targetState.radiance;
        currentState.charge = targetState.charge;
        currentState.discharge = targetState.discharge;

        const float time = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        const float basePhaseA = time * (0.85f + currentState.pulseFrequency * 0.5f);
        const float basePhaseB = time * (1.25f + currentState.flowRate * 0.4f);

        const float activity = 0.25f + smoothedEnergy * 0.75f;
        coreValueA = std::sin(basePhaseA) * activity;
        coreValueB = std::cos(basePhaseA * 0.65f + 1.2f) * activity;
        coreValueC = std::sin(basePhaseB * 0.82f - 0.5f) * activity;
        coreValueD = std::cos(basePhaseA * 1.34f + 2.2f) * activity;
        coreValueE = std::sin(basePhaseB * 1.15f + 3.1f) * activity;

        hazeValueA = std::sin(time * 0.32f) * (0.4f + currentState.fieldDepth * 0.6f);
        hazeValueB = std::cos(time * 0.44f + 0.7f) * (0.4f + currentState.fieldDepth * 0.6f);

        updateMatrixPulses();

        if (random.nextFloat() < (0.008f + smoothedEnergy * 0.012f + currentState.pulseFrequency * 0.008f))
        {
            if (! matrixNodes.empty())
            {
                const int node = random.nextInt((int) matrixNodes.size());
                const float intensity = 0.35f + random.nextFloat() * 0.55f;
                launchMatrixPulse(node, -1, intensity, false, 2 + random.nextInt(3));
            }
        }

        updateBurstField();
        maybeSpawnAmbientBursts();
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

    void drawPulseMatrixSequencerVisuals(juce::Graphics& g, juce::Rectangle<float> area)
    {
        if (! currentState.sequencerOn)
            return;

        const int numSteps = 16;
        const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.44f;
        const juce::Point<float> center = area.getCentre();
        const float energy = currentState.systemEnergy;

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


    void ensureMatrixLayout(juce::Rectangle<float> area)
    {
        const auto layoutBounds = area.getSmallestIntegerContainer();

        if (! matrixLayoutDirty && layoutBounds == matrixLayoutBounds)
            return;

        rebuildMatrixLayout(area);
    }

    void rebuildMatrixLayout(juce::Rectangle<float> area)
    {
        matrixLayoutBounds = area.getSmallestIntegerContainer();
        matrixLayoutDirty = false;
        matrixNodes.clear();
        matrixEdges.clear();
        matrixRowStarts.clear();
        matrixRowCounts.clear();
        matrixNodeCharge.clear();

        if (area.isEmpty())
            return;

        juce::Random layoutRandom(0x54524f4e); // "TRON"
        constexpr std::array<int, 6> rowNodeCounts { 2, 3, 3, 2, 2, 1 };

        for (size_t row = 0; row < rowNodeCounts.size(); ++row)
        {
            const int rowCount = rowNodeCounts[row];
            matrixRowStarts.push_back((int) matrixNodes.size());
            matrixRowCounts.push_back(rowCount);

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

                MatrixNode node;
                node.anchor = { x, y + yJitter };
                node.radius = 1.4f + rowT * 1.8f + layoutRandom.nextFloat() * 0.9f;
                node.swayPhase = layoutRandom.nextFloat() * juce::MathConstants<float>::twoPi;
                node.swayAmount = 0.8f + layoutRandom.nextFloat() * (1.8f + rowT * 1.2f);
                node.depthBias = rowT;
                node.baseColour = RootFlow::accent.interpolatedWith(juce::Colours::white, layoutRandom.nextFloat() * 0.3f);
                matrixNodes.push_back(node);
                matrixNodeCharge.push_back(0.0f);
            }

            if (row > 0)
            {
                const int prevRowStart = matrixRowStarts[row - 1];
                const int prevRowCount = matrixRowCounts[row - 1];
                const int currentRowStart = matrixRowStarts[row];
                const int currentRowCount = matrixRowCounts[row];

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
        MatrixEdge edge;
        edge.from = from;
        edge.to = to;
        edge.length = matrixNodes[(size_t) from].anchor.getDistanceFrom(matrixNodes[(size_t) to].anchor);
        edge.thickness = 0.8f + r.nextFloat() * 1.4f;
        edge.stiffness = 0.45f + r.nextFloat() * 0.35f;
        matrixEdges.push_back(edge);
    }

    std::vector<juce::Point<float>> buildMatrixPositions() const
    {
        std::vector<juce::Point<float>> positions;
        positions.reserve(matrixNodes.size());

        for (const auto& node : matrixNodes)
        {
            const float sway = std::sin(matrixDriftPhase + node.swayPhase) * node.swayAmount * (0.85f + smoothedEnergy * 0.45f);
            positions.push_back({ node.anchor.x + sway, node.anchor.y + sway * 0.35f });
        }

        return positions;
    }

    void drawMatrixLinks(juce::Graphics& g)
    {
        if (matrixEdges.empty()) return;

        const auto positions = buildMatrixPositions();
        const float energy = smoothedEnergy;

        for (const auto& edge : matrixEdges)
        {
            const auto p1 = positions[(size_t) edge.from];
            const auto p2 = positions[(size_t) edge.to];
            const float charge = (matrixNodeCharge[(size_t) edge.from] + matrixNodeCharge[(size_t) edge.to]) * 0.5f;

            g.setColour(RootFlow::accent.withAlpha(0.08f + charge * 0.22f + energy * 0.06f));
            g.drawLine(p1.x, p1.y, p2.x, p2.y, edge.thickness * (0.92f + charge * 0.65f));

            if (charge > 0.15f)
            {
                g.setColour(juce::Colours::white.withAlpha(charge * 0.12f));
                g.drawLine(p1.x, p1.y, p2.x, p2.y, edge.thickness * 0.45f);
            }
        }
    }

    void drawMatrixNodes(juce::Graphics& g)
    {
        const auto positions = buildMatrixPositions();
        const float energy = smoothedEnergy;

        for (size_t i = 0; i < matrixNodes.size(); ++i)
        {
            const auto& node = matrixNodes[i];
            const auto pos = positions[i];
            const float charge = matrixNodeCharge[i];
            const float pulse = 0.95f + 0.12f * std::sin(matrixDriftPhase * 2.2f + node.swayPhase);
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

    void drawMatrixPulses(juce::Graphics& g)
    {
        if (matrixPulses.empty()) return;

        const auto positions = buildMatrixPositions();
        for (const auto& pulse : matrixPulses)
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

    void drawBurstField(juce::Graphics& g)
    {
        for (const auto& burst : dataBursts)
        {
            const float lifeFade = std::sin(juce::jlimit(0.0f, 1.0f, burst.life / 0.8f) * juce::MathConstants<float>::pi);
            const float alpha = burst.brightness * lifeFade * (burst.layer == 0 ? 0.35f : (burst.layer == 2 ? 0.95f : 0.65f));

            juce::Colour col = RootFlow::accent;
            if (burst.accent) col = juce::Colours::white;

            g.setColour(col.withAlpha(alpha));
            const float size = burst.size * (0.85f + 0.35f * std::sin(matrixDriftPhase * 1.4f + burst.position.x * 0.01f));

            if (burst.layer == 2) {
                g.setColour(col.withAlpha(alpha * 0.32f));
                g.fillEllipse(burst.position.x - size * 2.2f, burst.position.y - size * 2.2f, size * 4.4f, size * 4.4f);
            }

            g.fillEllipse(burst.position.x - size * 0.5f, burst.position.y - size * 0.5f, size, size);
        }
    }

    void launchMatrixPulse(int fromNode, int excludeToNode, float intensity, bool accent, int hops)
    {
        if (! juce::isPositiveAndBelow(fromNode, (int) matrixNodes.size()) || matrixPulses.size() >= maxMatrixPulses)
            return;

        std::vector<int> targets;
        for (const auto& edge : matrixEdges)
        {
            if (edge.from == fromNode && edge.to != excludeToNode) targets.push_back(edge.to);
            else if (edge.to == fromNode && edge.from != excludeToNode) targets.push_back(edge.from);
        }

        if (targets.empty()) return;

        const int targetNode = targets[(size_t) random.nextInt((int) targets.size())];

        MatrixPulse pulse;
        pulse.fromNode = fromNode;
        pulse.toNode = targetNode;
        pulse.progress = 0.0f;
        pulse.speed = 0.025f + random.nextFloat() * 0.035f;
        pulse.intensity = intensity;
        pulse.accent = accent;
        pulse.hopsRemaining = hops;
        matrixPulses.push_back(pulse);

        if (random.nextFloat() < 0.28f * intensity)
            spawnBurstsAtNode(fromNode, intensity * 0.72f, accent);
    }

    void propagateMatrixPulse(int fromNode, int excludeToNode, float intensity, int hops, bool accent)
    {
        if (hops <= 0 || intensity < 0.06f) return;
        launchMatrixPulse(fromNode, excludeToNode, intensity, accent, hops);
    }

    void processQueuedImpulseIfNeeded()
    {
        if (! hasQueuedImpulse || matrixNodes.empty()) return;
        launchMatrixImpulse(queuedImpulseNote, queuedImpulseIntensity, queuedImpulseMapped);
        hasQueuedImpulse = false;
    }

    void launchMatrixImpulse(int noteNumber, float intensity, bool wasMapped)
    {
        juce::ignoreUnused(noteNumber);
        const int nodeCount = (int) matrixNodes.size();
        const int startNode = random.nextInt(juce::jmin(nodeCount, 4)); // Start near bottom
        launchMatrixPulse(startNode, -1, intensity, wasMapped, 4 + random.nextInt(3));
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

    void updateMatrixPulses()
    {
        if (! matrixNodeCharge.empty())
            for (auto& charge : matrixNodeCharge)
            {
                charge *= 0.90f + currentState.sourceAnchor * 0.035f;
                if (charge < 0.001f)
                    charge = 0.0f;
            }

        for (int i = (int) matrixPulses.size() - 1; i >= 0; --i)
        {
            auto& pulse = matrixPulses[(size_t) i];
            pulse.progress += pulse.speed * (0.86f + currentState.pulseFrequency * 1.30f + currentState.flowRate * 0.32f);

            if (pulse.progress < 1.0f)
                continue;

            if (juce::isPositiveAndBelow(pulse.toNode, (int) matrixNodeCharge.size()))
                matrixNodeCharge[(size_t) pulse.toNode] = juce::jmax(matrixNodeCharge[(size_t) pulse.toNode], pulse.intensity * 0.90f);

            if (pulse.hopsRemaining > 0)
                propagateMatrixPulse(pulse.toNode,
                                       pulse.fromNode,
                                       pulse.intensity * (0.68f + currentState.pulseGrowth * 0.10f),
                                       pulse.hopsRemaining - 1,
                                       pulse.accent);

            matrixPulses.erase(matrixPulses.begin() + i);
        }
    }

    void spawnBurstsAtNode(int nodeIndex, float intensity, bool accent)
    {
        if (! juce::isPositiveAndBelow(nodeIndex, (int) matrixNodes.size()))
            return;

        const auto positions = buildMatrixPositions();
        const auto origin = positions[(size_t) nodeIndex];
        const int particleCount = 3 + juce::roundToInt(intensity * 4.0f + currentState.bloom * 4.0f);

        for (int i = 0; i < particleCount && dataBursts.size() < maxBursts; ++i)
        {
            const float angle = -juce::MathConstants<float>::halfPi + (random.nextFloat() - 0.5f) * 1.15f;
            const float speed = 0.22f + intensity * 0.78f + random.nextFloat() * 0.42f;

            BurstParticle particle;
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
            dataBursts.push_back(particle);
        }
    }

    void updateBurstField()
    {
        const float horizontalWind = std::sin(matrixDriftPhase * 0.82f) * (0.010f + currentState.rain * 0.020f);
        const float upwardLift = 0.010f + currentState.bloom * 0.018f + currentState.sun * 0.006f;

        for (int i = (int) dataBursts.size() - 1; i >= 0; --i)
        {
            auto& burst = dataBursts[(size_t) i];
            burst.velocity.x += (horizontalWind - burst.velocity.x * 0.08f) * 0.020f;
            burst.velocity.y -= upwardLift * (0.22f + burst.brightness * 0.12f);
            burst.position += burst.velocity;
            burst.life -= 0.010f + currentState.sun * 0.002f + currentState.rain * 0.001f;

            if (burst.life <= 0.0f)
                dataBursts.erase(dataBursts.begin() + i);
        }
    }

    void maybeSpawnAmbientBursts()
    {
        if (matrixNodes.empty() || dataBursts.size() >= maxBursts)
            return;

        const float chance = 0.004f
                           + currentState.bloom * 0.010f
                           + ambientEnergy * 0.006f
                           + currentState.atmosphere * 0.004f;
        if (random.nextFloat() >= chance)
            return;

        const int rowIndex = juce::jlimit(0, (int) matrixRowStarts.size() - 1,
                                          juce::roundToInt((float) (matrixRowStarts.size() - 1) * (0.45f + currentState.canopy * 0.40f)));
        const int rowStart = matrixRowStarts[(size_t) rowIndex];
        const int rowCount = matrixRowCounts[(size_t) rowIndex];
        const int nodeIndex = rowStart + juce::jlimit(0, rowCount - 1, random.nextInt(juce::jmax(1, rowCount)));

        spawnBurstsAtNode(nodeIndex, 0.18f + ambientEnergy * 0.18f, false);
    }

    int findMatrixEdgeIndex(int fromNode, int toNode) const
    {
        const int minNode = juce::jmin(fromNode, toNode);
        const int maxNode = juce::jmax(fromNode, toNode);
        for (size_t i = 0; i < matrixEdges.size(); ++i)
            if (matrixEdges[i].from == minNode && matrixEdges[i].to == maxNode)
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
    juce::Rectangle<int> matrixLayoutBounds;
    bool matrixLayoutDirty = true;
    size_t historyWritePosition = 0;
    float targetEnergy = 0.0f, smoothedEnergy = 0.0f, ambientEnergy = 0.0f, glowEnergy = 0.0f;
    float phaseA = 0.0f, phaseB = 1.4f, phaseC = 2.6f;
    float coreValueA = 0.0f, coreValueB = 0.0f, coreValueC = 0.0f, coreValueD = 0.0f, coreValueE = 0.0f;
    float hazeValueA = 0.0f, hazeValueB = 0.0f;
    float energySeedPhase = 0.0f, ambientFieldPhase = 0.0f, matrixDriftPhase = 0.0f;
    VisualizerState targetState {}, currentState {};
    std::vector<MatrixNode> matrixNodes;
    std::vector<MatrixEdge> matrixEdges;
    std::vector<int> matrixRowStarts, matrixRowCounts;
    std::vector<float> matrixNodeCharge;
    std::vector<MatrixPulse> matrixPulses;
    std::vector<BurstParticle> dataBursts;
    juce::Random random { 0x53504f52 };
    bool hasQueuedImpulse = false;
    int queuedImpulseNote = -1;
    float queuedImpulseIntensity = 0.0f;
    bool queuedImpulseMapped = false;
    static constexpr size_t maxMatrixPulses = 96, maxBursts = 480;

    RootFlowAudioProcessor& processor;
    float globalBeatCharge = 0.0f;
    SpeciesMode currentSpecies = SpeciesMode::lightFlow;
    GrowthColor currentColor = GrowthColor::emerald;
};
