#include "EnergyDisplay.h"
#include "../PluginProcessor.h"
#include "Utils/DesignTokens.h"

namespace
{
juce::Colour getNodeTint(const FlowNode& node) noexcept
{
    const float canopyBias = juce::jlimit(0.0f, 1.0f, 1.0f - node.position.y);
    const float leftBias = juce::jlimit(0.0f, 1.0f, 1.0f - node.position.x);
    const float rightBias = juce::jlimit(0.0f, 1.0f, node.position.x);

    auto tint = RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.30f + canopyBias * 0.28f);
    tint = tint.interpolatedWith(RootFlow::amber,
                                 juce::jlimit(0.0f, 1.0f, node.value * 0.34f + node.energy * 0.16f + rightBias * 0.08f));
    tint = tint.interpolatedWith(RootFlow::violet,
                                 juce::jlimit(0.0f, 1.0f, leftBias * 0.10f + (1.0f - canopyBias) * 0.05f));
    return tint.interpolatedWith(juce::Colours::white, juce::jlimit(0.0f, 0.06f, node.energy * 0.06f));
}

juce::String formatPercent(float value)
{
    return juce::String(juce::roundToInt(juce::jlimit(0.0f, 1.0f, value) * 100.0f)) + "%";
}

juce::String getNodeDisplayName(const FlowNode& node, const RootFlowAudioProcessor* processor)
{
    if (processor != nullptr)
    {
        auto name = processor->getParameterDisplayName(node.paramID).trim();
        if (name.isNotEmpty())
            return name;
    }

    return node.paramID;
}

juce::String getNodeFocusStateLabel(int nodeIndex,
                                    int selectedNode,
                                    int hoveredNode,
                                    int connectionStart,
                                    int seqFlashNode,
                                    float seqFlashTimer)
{
    if (nodeIndex == selectedNode)
        return "Dragging";

    if (nodeIndex == connectionStart)
        return "Link Source";

    if (nodeIndex == hoveredNode)
        return "Hover";

    if (nodeIndex == seqFlashNode && seqFlashTimer > 0.0f)
        return "Step Pulse";

    return "Active";
}

void drawHudChip(juce::Graphics& g,
                 juce::Rectangle<float> area,
                 const juce::String& label,
                 const juce::String& value,
                 juce::Colour tint,
                 bool emphasise = false)
{
    RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.5f, emphasise ? 0.78f : 0.60f);

    auto inner = area.reduced(10.0f, 4.0f);
    auto labelArea = inner.removeFromTop(area.getHeight() * 0.36f);

    g.setColour(RootFlow::textMuted.withAlpha(0.84f));
    g.setFont(RootFlow::getFont(8.9f));
    g.drawText(label.toUpperCase(), labelArea, juce::Justification::centredLeft, false);

    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.drawFittedText(value, inner.toNearestInt().translated(0, 1), juce::Justification::centredLeft, 1);

    g.setColour((emphasise ? tint.brighter(0.12f) : RootFlow::text).withAlpha(0.96f));
    g.setFont(RootFlow::getFont(11.2f).boldened());
    g.drawFittedText(value, inner.toNearestInt(), juce::Justification::centredLeft, 1);

    RootFlow::drawGlowOrb(g, { area.getRight() - 11.0f, area.getCentreY() }, 3.0f, tint, emphasise ? 0.56f : 0.32f);
}

void drawHintBar(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text)
{
    RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.5f, 0.62f);

    auto inner = area.toNearestInt().reduced(12, 4);
    auto lines = juce::StringArray::fromLines(text);

    g.setColour(RootFlow::text.withAlpha(0.86f));
    g.setFont(RootFlow::getFont(lines.size() > 1 ? 9.6f : 10.0f).boldened());

    if (lines.size() <= 1)
    {
        g.drawFittedText(text, inner, juce::Justification::centred, 1);
        return;
    }

    auto top = inner.removeFromTop(inner.getHeight() / 2);
    g.drawFittedText(lines[0], top, juce::Justification::centred, 1);
    g.drawFittedText(lines[1], inner, juce::Justification::centred, 1);
}

void drawFocusBubble(juce::Graphics& g,
                     juce::Rectangle<float> area,
                     const juce::String& state,
                     const juce::String& name,
                     const juce::String& baseValue,
                     const juce::String& liveValue,
                     juce::Colour tint,
                     bool emphasise)
{
    RootFlow::drawGlassPanel(g, area, area.getHeight() * 0.34f, emphasise ? 0.84f : 0.66f);

    g.setColour(tint.withAlpha(emphasise ? 0.12f : 0.08f));
    g.fillRoundedRectangle(area.reduced(2.0f), area.getHeight() * 0.28f);

    auto inner = area.reduced(10.0f, 6.0f);
    auto stateArea = inner.removeFromTop(9.0f);
    auto nameArea = inner.removeFromTop(15.0f);
    auto metricsArea = inner.withTrimmedTop(2.0f);
    auto baseArea = metricsArea.removeFromLeft(metricsArea.getWidth() * 0.48f);

    g.setColour(RootFlow::textMuted.withAlpha(0.88f));
    g.setFont(RootFlow::getFont(7.8f).boldened());
    g.drawText(state.toUpperCase(), stateArea, juce::Justification::centredLeft, false);

    g.setColour((emphasise ? tint.brighter(0.20f) : RootFlow::text).withAlpha(0.96f));
    g.setFont(RootFlow::getFont(10.2f).boldened());
    g.drawFittedText(name.toUpperCase(), nameArea.toNearestInt(), juce::Justification::centredLeft, 1);

    g.setFont(RootFlow::getFont(8.0f).boldened());
    g.setColour(tint.withAlpha(0.96f));
    g.drawFittedText(baseValue.toUpperCase(), baseArea.toNearestInt(), juce::Justification::centredLeft, 1);

    g.setColour(RootFlow::text.withAlpha(0.86f));
    g.drawFittedText(liveValue.toUpperCase(), metricsArea.toNearestInt(), juce::Justification::centredRight, 1);

    RootFlow::drawGlowOrb(g, { area.getRight() - 12.0f, area.getY() + 13.0f }, 3.0f, tint, emphasise ? 0.62f : 0.38f);
}
}

void EnergyDisplay::setProcessor(RootFlowAudioProcessor* p)
{
    processor = p;
    if (processor != nullptr)
    {
        nodeSystem = &processor->getNodeSystem();
        setInterceptsMouseClicks(true, true);
        startTimerHz(60);
    }
    else
    {
        stopTimer();
        nodeSystem = nullptr;
    }
}

void EnergyDisplay::timerCallback()
{
    if (processor == nullptr || nodeSystem == nullptr)
        return;

    rms  = processor->getRMS();
    time += 1.0f / 60.0f;
    
    if (sporeBurstTimer > 0.0f)
        sporeBurstTimer = juce::jmax(0.0f, sporeBurstTimer - 0.04f);
    
    // --- Sequencer → Node energy pulse ---
    if (processor->sequencerTriggered.exchange(false))
    {
        const int step = processor->getCurrentSequencerStep();
        const juce::ScopedLock nodeLock(nodeSystem->getLock());
        auto& nodes = nodeSystem->getNodes();

        if (! nodes.empty())
        {
            // Map step (0-15) to node index cyclically
            int nodeIdx = step % (int) nodes.size();
            nodes[nodeIdx].energy = juce::jmin(1.0f, nodes[nodeIdx].energy + 0.75f);

            // Also speed up particles on connections involving that node
            for (auto& c : nodeSystem->getConnections())
            {
                bool involved = (c.source == nodeIdx || c.target == nodeIdx);
                if (involved)
                {
                    for (auto& p : c.particles)
                        p.speed = juce::jmin(1.5f, p.speed + 0.4f);
                }
            }

            seqFlashNode  = nodeIdx;
            seqFlashTimer = 1.0f;  // full brightness flash
        }
    }

    // Decay flash
    seqFlashTimer = juce::jmax(0.0f, seqFlashTimer - 0.07f);

    // --- BEAT SYNC ---
    float currentBeat = processor->beatPhase.load();
    if (std::isnan(currentBeat) || std::isinf(currentBeat))
        currentBeat = 0.0f;

    if (currentBeat < lastBeat) // Beat Reset / Burst
    {
        const juce::ScopedLock nodeLock(nodeSystem->getLock());
        for (auto& c : nodeSystem->getConnections())
        {
            for (auto& p : c.particles)
                p.t = 0.0f; // Reset to start for burst effect
        }
    }
    lastBeat = currentBeat;

    float newSpectrum[RootFlowAudioProcessor::fftSize / 2];
    if (processor->getNextFFTBlock(newSpectrum))
        spectrum.assign(newSpectrum, newSpectrum + RootFlowAudioProcessor::fftSize / 2);

    repaint();
}

void EnergyDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    float width  = bounds.getWidth();
    float height = bounds.getHeight();
    float midY   = height * 0.5f;
    const auto crownLeft = juce::Point<float>(width * 0.34f, height * 0.12f);
    const auto crownRight = juce::Point<float>(width * 0.66f, height * 0.12f);
    const auto rootHub = juce::Point<float>(width * 0.50f, height * 0.86f);
    const auto streamTint = RootFlow::accent.interpolatedWith(RootFlow::violet, 0.16f);
    const auto groveTint = RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.38f);
    const auto bloomTint = RootFlow::amber.interpolatedWith(RootFlow::accentSoft, 0.16f);

    juce::ColourGradient nucleus(groveTint.withAlpha(0.05f), width * 0.48f, height * 0.52f,
                                 juce::Colours::transparentBlack, width * 0.48f, height, true);
    g.setGradientFill(nucleus);
    g.fillEllipse(bounds.reduced(width * 0.12f, height * 0.12f));

    juce::ColourGradient canopyGlow(bloomTint.withAlpha(0.05f), width * 0.64f, height * 0.12f,
                                    juce::Colours::transparentBlack, width * 0.64f, height * 0.42f, true);
    g.setGradientFill(canopyGlow);
    g.fillEllipse(width * 0.46f, height * 0.02f, width * 0.28f, height * 0.26f);

    juce::ColourGradient rootGlow(groveTint.withAlpha(0.06f), rootHub.x, height * 0.70f,
                                  juce::Colours::transparentBlack, rootHub.x, height, true);
    g.setGradientFill(rootGlow);
    g.fillEllipse(width * 0.26f, height * 0.68f, width * 0.48f, height * 0.24f);

    juce::ColourGradient mist(streamTint.withAlpha(0.04f), width * 0.22f, height * 0.28f,
                              juce::Colours::transparentBlack, width * 0.10f, height * 0.75f, true);
    g.setGradientFill(mist);
    g.fillEllipse(width * 0.08f, height * 0.14f, width * 0.34f, height * 0.56f);

    juce::ColourGradient rightBloom(bloomTint.withAlpha(0.04f), width * 0.78f, height * 0.30f,
                                    juce::Colours::transparentBlack, width * 0.92f, height * 0.74f, true);
    g.setGradientFill(rightBloom);
    g.fillEllipse(width * 0.62f, height * 0.14f, width * 0.30f, height * 0.48f);

    juce::ColourGradient centreFlow(groveTint.withAlpha(0.04f), width * 0.50f, height * 0.42f,
                                    juce::Colours::transparentBlack, width * 0.50f, height * 0.76f, true);
    g.setGradientFill(centreFlow);
    g.fillEllipse(width * 0.22f, height * 0.24f, width * 0.56f, height * 0.44f);

    auto viewport = bounds.reduced(width * 0.03f, height * 0.04f);
    g.setColour(juce::Colours::black.withAlpha(0.10f));
    g.drawRoundedRectangle(viewport, 28.0f, 0.9f);
    g.setColour(groveTint.withAlpha(0.06f));
    g.drawRoundedRectangle(viewport.reduced(3.0f), 25.0f, 0.9f);

    if (sporeBurstTimer > 0.0f)
    {
        g.setColour(RootFlow::accent.interpolatedWith(juce::Colours::white, 0.6f).withAlpha(sporeBurstTimer * 0.16f));
        g.fillRoundedRectangle(viewport, 28.0f);
        
        g.setColour(RootFlow::amber.withAlpha(sporeBurstTimer * 0.08f));
        g.fillRoundedRectangle(viewport, 28.0f);
    }

    juce::Path fieldArc;
    fieldArc.startNewSubPath(width * 0.14f, height * 0.22f);
    fieldArc.cubicTo(width * 0.24f, height * 0.06f,
                     width * 0.76f, height * 0.06f,
                     width * 0.86f, height * 0.22f);
    g.setColour(juce::Colours::white.withAlpha(0.022f));
    g.strokePath(fieldArc, juce::PathStrokeType(0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (int ring = 0; ring < 3; ++ring)
    {
        auto halo = juce::Rectangle<float>(width * (0.22f + ring * 0.05f), height * (0.28f + ring * 0.02f),
                                           width * (0.56f - ring * 0.10f), height * (0.34f - ring * 0.04f));
        const auto haloTint = ring == 0 ? groveTint : (ring == 1 ? streamTint : bloomTint);
        g.setColour(haloTint.withAlpha(0.011f - ring * 0.002f));
        g.drawEllipse(halo, 0.9f);
    }

    RootFlow::drawOrbSocket(g, crownLeft, 6.4f, streamTint, 0.38f);
    RootFlow::drawOrbSocket(g, crownRight, 6.4f, bloomTint, 0.38f);
    RootFlow::drawOrbSocket(g, rootHub, 9.0f, groveTint, 0.44f);
    RootFlow::drawBioThread(g, crownLeft, { width * 0.42f, height * 0.36f }, streamTint, 0.10f, 1.05f);
    RootFlow::drawBioThread(g, crownRight, { width * 0.58f, height * 0.36f }, bloomTint, 0.10f, 1.05f);
    RootFlow::drawBioThread(g, rootHub, { width * 0.46f, height * 0.60f }, groveTint, 0.10f, 1.15f);
    RootFlow::drawBioThread(g, rootHub, { width * 0.54f, height * 0.60f }, RootFlow::accentSoft, 0.10f, 1.15f);

    for (int i = 0; i < 20; ++i)
    {
        const float fx = std::abs(std::sin((float) i * 13.147f + time * 0.05f));
        const float fy = std::abs(std::sin((float) i * 7.531f + 0.8f + time * 0.03f));
        const float px = bounds.getX() + fx * width;
        const float py = bounds.getY() + fy * height;
        const float size = 1.0f + (float) (i % 3);
        const auto starTint = i % 5 == 0 ? groveTint
                            : (i % 7 == 0 ? bloomTint
                                          : (i % 3 == 0 ? streamTint
                                                        : juce::Colours::white));
        g.setColour(starTint.withAlpha(0.035f + (float) (i % 4) * 0.018f));
        g.fillEllipse(px, py, size, size);
    }

    if (!spectrum.empty())
    {
        juce::Path path;
        int displaySize = (int)spectrum.size() / 2; 

        for (int i = 0; i < displaySize; ++i)
        {
            float x = (float)i / (displaySize - 1) * width;
            float magnitude = spectrum[i] * height * (0.4f + rms * 2.0f);
            float y = midY - magnitude;

            if (i == 0) path.startNewSubPath(x, y);
            else        path.lineTo(x, y);
        }

        auto fill = path;
        fill.lineTo(width, midY + height * 0.06f);
        fill.lineTo(0.0f, midY + height * 0.06f);
        fill.closeSubPath();
        g.setColour(RootFlow::accent.withAlpha(0.02f + rms * 0.04f));
        g.fillPath(fill);

        g.setColour(RootFlow::accent.withAlpha(0.05f));
        g.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    NodeSystemSnapshot snapshot;
    if (nodeSystem != nullptr)
        snapshot = nodeSystem->createSnapshot();

    const auto& nodes = snapshot.nodes;
    const auto& connections = snapshot.connections;

    int activeNodes = 0;
    for (const auto& node : nodes)
        if (node.energy > 0.24f || node.value > 0.56f)
            ++activeNodes;

    const auto interactionState = connectionStart >= 0 ? juce::String("LINK")
                               : (selectedNode >= 0 ? juce::String("DRAG")
                                                    : (seqFlashTimer > 0.0f ? juce::String("PULSE")
                                                                            : juce::String("GROVE")));
    const bool hoverEffectsEnabled = RootFlow::areHoverEffectsEnabled();
    const int focusNodeIndex = juce::isPositiveAndBelow(selectedNode, (int) nodes.size()) ? selectedNode
                            : (juce::isPositiveAndBelow(connectionStart, (int) nodes.size()) ? connectionStart
                            : ((hoverEffectsEnabled && juce::isPositiveAndBelow(hoveredNode, (int) nodes.size())) ? hoveredNode
                            : ((seqFlashTimer > 0.0f && juce::isPositiveAndBelow(seqFlashNode, (int) nodes.size())) ? seqFlashNode
                                                                                                                        : -1)));
    const FlowNode* focusNode = juce::isPositiveAndBelow(focusNodeIndex, (int) nodes.size())
                                  ? &nodes[(size_t) focusNodeIndex]
                                  : nullptr;
    const auto focusName = focusNode != nullptr ? getNodeDisplayName(*focusNode, processor) : juce::String();
    const float focusLiveValue = focusNode != nullptr && processor != nullptr
                                   ? processor->getModulatedValue(focusNode->paramID)
                                   : (focusNode != nullptr ? focusNode->value : 0.0f);

    auto titleChip = juce::Rectangle<float>(width * 0.08f, height * 0.10f, juce::jmin(206.0f, width * 0.33f), 38.0f);
    drawHudChip(g, titleChip, "Field", "NEURAL GROVE", RootFlow::accentSoft, seqFlashTimer > 0.0f);

    auto statsStrip = juce::Rectangle<float>(titleChip.getRight() + 10.0f,
                                             titleChip.getY(),
                                             width - (titleChip.getRight() + 10.0f) - width * 0.05f,
                                             titleChip.getHeight());

    if (statsStrip.getWidth() > 210.0f)
    {
        const int chipCount = statsStrip.getWidth() > 330.0f ? 4 : 3;
        const float chipGap = 7.0f;
        const float chipWidth = (statsStrip.getWidth() - chipGap * (float) (chipCount - 1)) / (float) chipCount;

        auto nextChip = [&statsStrip, chipWidth, chipGap]()
        {
            auto chip = statsStrip.removeFromLeft(chipWidth);
            statsStrip.removeFromLeft(chipGap);
            return chip;
        };

        drawHudChip(g, nextChip(), "RMS", formatPercent(rms), RootFlow::accent, rms > 0.35f);
        drawHudChip(g, nextChip(), "Nodes", juce::String(activeNodes) + "/" + juce::String((int) nodes.size()), RootFlow::accentSoft,
                    activeNodes > 0);

        if (chipCount == 4)
            drawHudChip(g, nextChip(), "Links", juce::String((int) connections.size()), RootFlow::amber, ! connections.empty());

        drawHudChip(g, nextChip(), "State", interactionState, RootFlow::violet,
                    interactionState != "GROVE");
    }

    juce::String hintText = "DRAG NODE / RIGHT LINK / WHEEL EDGE\nSHIFT MUTATE / DOUBLE RESET";
    if (juce::isPositiveAndBelow(connectionStart, (int) nodes.size()))
    {
        const auto sourceName = getNodeDisplayName(nodes[(size_t) connectionStart], processor).toUpperCase();
        if (juce::isPositiveAndBelow(connectionHover, (int) nodes.size()) && connectionHover != connectionStart)
        {
            hintText = "CREATE LINK\n" + sourceName + " -> "
                     + getNodeDisplayName(nodes[(size_t) connectionHover], processor).toUpperCase();
        }
        else
        {
            hintText = "LINK FROM " + sourceName + "\nRELEASE ON TARGET NODE";
        }
    }
    else if (focusNode != nullptr)
    {
        const auto focusNameUpper = focusName.toUpperCase();
        const auto liveValueText = formatPercent(focusLiveValue);

        if (focusNodeIndex == selectedNode)
            hintText = focusNameUpper + " " + liveValueText + "\nDRAG TO MORPH / RELEASE TO COMMIT";
        else if (hoverEffectsEnabled && focusNodeIndex == hoveredNode)
            hintText = focusNameUpper + " " + liveValueText + "\nDRAG MOVE / RIGHT LINK / SHIFT MUTATE";
        else if (focusNodeIndex == seqFlashNode && seqFlashTimer > 0.0f)
            hintText = focusNameUpper + " " + liveValueText + "\nSEQUENCER PULSE / LIVE MODULATION";
    }

    if (RootFlow::arePopupOverlaysEnabled()
        && (focusNode != nullptr || connectionStart >= 0 || selectedNode >= 0 || seqFlashTimer > 0.0f))
    {
        auto hintBar = juce::Rectangle<float>(width * 0.19f, height - 34.0f, width * 0.62f, 28.0f);
        drawHintBar(g, hintBar, hintText);
    }

    if (nodes.empty())
        return;

    // --- 2. PULSING NERVE CONNECTIONS ---
    for (auto& c : connections)
    {
        if (! juce::isPositiveAndBelow(c.source, (int) nodes.size())
            || ! juce::isPositiveAndBelow(c.target, (int) nodes.size()))
            continue;

        auto& a = nodes[c.source];
        auto& b = nodes[c.target];

        float x1 = a.position.x * width, y1 = a.position.y * height;
        float x2 = b.position.x * width, y2 = b.position.y * height;

        // Draw Nerve Path (Bézier Curve)
        juce::Path nervePath;
        nervePath.startNewSubPath(x1, y1);

        float midX = (x1 + x2) * 0.5f;
        float midY = (y1 + y2) * 0.5f;
        
        // Biological breathing oscillation for the nerve curves
        float curveIntensity = 35.0f * std::abs(c.amount);
        float ctrlX = midX + std::sin(time * 0.8f + (float)c.source) * curveIntensity;
        float ctrlY = midY + std::cos(time * 0.8f + (float)c.target) * curveIntensity;

        nervePath.quadraticTo(ctrlX, ctrlY, x2, y2);

        // Mycelium Health visual scaling
        float healthScale = c.health;
        float absAmt = std::abs(c.amount) * healthScale;
        juce::Colour connCol = c.amount < 0.0f
            ? RootFlow::amber.interpolatedWith(RootFlow::violet, 0.28f)
            : RootFlow::accent.interpolatedWith(RootFlow::accentSoft, 0.35f);
        
        // Fading based on health
        float alpha = (0.16f + (a.energy + b.energy) * 0.22f) * healthScale;
        
        if (healthScale < 0.3f) // Flickering for dying mycelium
        {
            float flicker = 0.7f + 0.3f * std::sin(time * 25.0f + (float)c.source);
            alpha *= flicker;
        }

        g.setColour(connCol.withAlpha(0.07f * alpha * absAmt));
        g.strokePath(nervePath, juce::PathStrokeType(5.5f * healthScale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(connCol.withAlpha(0.42f * alpha));
        g.strokePath(nervePath, juce::PathStrokeType(juce::jmax(0.4f, 1.0f * healthScale)));
        
        g.setColour(juce::Colours::white.withAlpha(0.35f * a.energy * healthScale));
        g.strokePath(nervePath, juce::PathStrokeType(0.6f * healthScale));

        g.setColour(connCol.withAlpha((0.08f + absAmt * 0.10f) * healthScale));
        g.fillEllipse(x1 - 2.0f * healthScale, y1 - 2.0f * healthScale, 4.0f * healthScale, 4.0f * healthScale);
        g.fillEllipse(x2 - 2.0f * healthScale, y2 - 2.0f * healthScale, 4.0f * healthScale, 4.0f * healthScale);

        float pathLen = nervePath.getLength();
        for (auto& p : c.particles)
        {
            // Density of particles also drops with health
            if (juce::Random::getSystemRandom().nextFloat() > (healthScale + 0.2f)) continue;

            for (size_t h = 0; h < p.history.size(); ++h)
            {
                auto histPos = nervePath.getPointAlongPath(p.history[h] * pathLen);
                const float trailAlpha = (1.0f - (float) h / (float) p.history.size()) * 0.12f * alpha;
                const float trailSize = (0.7f + (1.0f - (float) h / (float) p.history.size()) * 1.6f) * healthScale;
                g.setColour(connCol.withAlpha(trailAlpha));
                g.fillEllipse(histPos.x - trailSize * 0.5f, histPos.y - trailSize * 0.5f, trailSize, trailSize);
            }

            auto pathPos = nervePath.getPointAlongPath(p.t * pathLen);
            float px = pathPos.getX();
            float py = pathPos.getY();
            
            float pSize = 1.6f + (p.speed * 2.6f);

            g.setColour(connCol.withAlpha(0.22f * alpha));
            g.fillEllipse(px - pSize, py - pSize, pSize * 2.0f, pSize * 2.0f);

            g.setColour(juce::Colours::white.withAlpha(0.8f * alpha));
            g.fillEllipse(px - 1.5f, py - 1.5f, 3.0f, 3.0f);
        }
    }

    drawKeyboardInteraction(g);

    if (juce::isPositiveAndBelow(connectionStart, (int) nodes.size()))
    {
        auto& startNode = nodes[connectionStart];
        float x1 = startNode.position.x * width;
        float y1 = startNode.position.y * height;
        auto mousePos = getMouseXYRelative().toFloat();

        g.setColour(juce::Colour(0xff3cff9a).withAlpha(0.6f));
        float dashPattern[] = { 4.0f, 4.0f };
        g.drawDashedLine(juce::Line<float>(x1, y1, mousePos.x, mousePos.y), dashPattern, 2, 1.5f);
    }

    juce::Point<float> focusNodePos;
    float focusNodeSize = 0.0f;
    juce::Colour focusNodeTint = RootFlow::accent;

    for (int nodeIndex = 0; nodeIndex < (int) nodes.size(); ++nodeIndex)
    {
        auto& n = nodes[(size_t) nodeIndex];
        float x = n.position.x * width;
        float y = n.position.y * height;
        
        float beat = lastBeat;
        if (std::isnan(beat) || std::isinf(beat)) beat = 0.0f;
        float bPulse = std::pow(std::sin(beat * juce::MathConstants<float>::pi) * 0.5f + 0.5f, 2.0f);

        float size = 4.5f + n.value * 6.5f + n.energy * 8.0f + bPulse * 3.8f;
        const auto nodeColour = getNodeTint(n);
        const bool isSelected = nodeIndex == selectedNode;
        const bool isHovered = hoverEffectsEnabled && nodeIndex == hoveredNode;
        const bool isLinkSource = nodeIndex == connectionStart;
        const bool isLinkTarget = connectionStart >= 0 && nodeIndex == connectionHover && connectionHover != connectionStart;
        const bool isFocus = nodeIndex == focusNodeIndex;

        if (n.isDragging)
        {
            g.setColour(nodeColour.withAlpha(0.18f));
            g.fillEllipse(x - size * 2.2f, y - size * 2.2f, size * 4.4f, size * 4.4f);
        }

        if (isFocus || isLinkTarget)
        {
            const auto ringTint = isLinkSource ? RootFlow::accentSoft
                               : (isLinkTarget ? RootFlow::amber
                                               : nodeColour);
            const float ringRadius = size * (isSelected ? 1.74f : 1.56f) + (isHovered ? 4.0f : 2.0f);

            g.setColour(ringTint.withAlpha(isSelected || isLinkTarget ? 0.12f : 0.08f));
            g.fillEllipse(x - ringRadius, y - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(isSelected ? 0.28f : 0.14f));
            g.drawEllipse(x - ringRadius, y - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f, isSelected ? 1.4f : 1.0f);
            g.setColour(ringTint.withAlpha(isSelected || isLinkTarget ? 0.58f : 0.34f));
            g.drawEllipse(x - ringRadius * 0.88f, y - ringRadius * 0.88f, ringRadius * 1.76f, ringRadius * 1.76f, 1.1f);
        }

        float glowAlpha = 0.12f + n.energy * 0.32f + n.value * 0.16f;
        g.setColour(nodeColour.withAlpha(juce::jlimit(0.0f, 0.34f, glowAlpha)));
        g.fillEllipse(x - size * 1.08f, y - size * 1.08f, size * 2.16f, size * 2.16f);

        juce::ColourGradient core(nodeColour.brighter(0.28f).withAlpha(0.95f), x, y - size * 0.8f,
                                  nodeColour.darker(0.30f).withAlpha(0.92f), x, y + size, true);
        g.setGradientFill(core);
        g.fillEllipse(x - size * 0.62f, y - size * 0.62f, size * 1.24f, size * 1.24f);

        g.setColour(juce::Colours::white.withAlpha(0.54f + n.activity * 0.20f));
        g.fillEllipse(x - size * 0.15f, y - size * 0.15f, size * 0.30f, size * 0.30f);

        const auto sparkPos = juce::Point<float>(x + std::cos(time * 1.4f + (float) nodeIndex) * size * 0.82f,
                                                 y + std::sin(time * 1.4f + (float) nodeIndex) * size * 0.82f);
        g.setColour(juce::Colours::white.withAlpha(0.10f + n.energy * 0.32f));
        g.fillEllipse(sparkPos.x - 1.1f, sparkPos.y - 1.1f, 2.2f, 2.2f);

        if (nodeIndex == seqFlashNode && seqFlashTimer > 0.0f)
        {
            float flashR = size + 4.0f + (1.0f - seqFlashTimer) * 14.0f;
            g.setColour(juce::Colours::white.withAlpha(seqFlashTimer * 0.62f));
            g.drawEllipse(x - flashR, y - flashR, flashR * 2.0f, flashR * 2.0f, 1.8f);

            g.setColour(RootFlow::accent.withAlpha(seqFlashTimer * 0.40f));
            g.fillEllipse(x - size * 0.7f, y - size * 0.7f, size * 1.4f, size * 1.4f);
        }

        if (isFocus)
        {
            focusNodePos = { x, y };
            focusNodeSize = size;
            focusNodeTint = isLinkSource ? RootFlow::accentSoft : nodeColour;
        }
    }

    if (RootFlow::arePopupOverlaysEnabled() && focusNode != nullptr)
    {
        auto bubble = juce::Rectangle<float>(juce::jlimit(144.0f, 196.0f, width * 0.27f), 50.0f)
                          .withCentre({ focusNodePos.x, focusNodePos.y - focusNodeSize - 14.0f });

        if (bubble.getY() < bounds.getY() + 10.0f)
            bubble = bubble.withY(focusNodePos.y + focusNodeSize + 12.0f);

        bubble.setX(juce::jlimit(bounds.getX() + 8.0f,
                                 bounds.getRight() - bubble.getWidth() - 8.0f,
                                 bubble.getX()));
        bubble.setY(juce::jlimit(bounds.getY() + 8.0f,
                                 bounds.getBottom() - bubble.getHeight() - 42.0f,
                                 bubble.getY()));

        drawFocusBubble(g,
                        bubble,
                        getNodeFocusStateLabel(focusNodeIndex, selectedNode, hoverEffectsEnabled ? hoveredNode : -1, connectionStart, seqFlashNode, seqFlashTimer),
                        focusName,
                        "Base " + formatPercent(focusNode->value),
                        "Live " + formatPercent(focusLiveValue),
                        focusNodeTint,
                        focusNodeIndex == selectedNode
                            || focusNodeIndex == connectionStart
                            || (focusNodeIndex == seqFlashNode && seqFlashTimer > 0.0f));
    }
}

void EnergyDisplay::triggerSporeBurst()
{
    sporeBurstTimer = 1.0f;
    if (nodeSystem != nullptr)
    {
        const juce::ScopedLock nodeLock(nodeSystem->getLock());
        for (auto& n : nodeSystem->getNodes())
        {
            n.energy = 1.0f;
            n.activity = 1.0f;
        }

        for (auto& c : nodeSystem->getConnections())
        {
            for (auto& p : c.particles)
                p.speed = 2.5f;
        }
    }
}

void EnergyDisplay::resized() {}

int EnergyDisplay::findNodeAt(juce::Point<float> mousePos)
{
    float width = (float)getWidth();
    float height = (float)getHeight();
    if (nodeSystem == nullptr) return -1;
    const auto snapshot = nodeSystem->createSnapshot();
    const auto& nodes = snapshot.nodes;

    for (int i = 0; i < (int)nodes.size(); ++i)
    {
        float x = nodes[i].position.x * width;
        float y = nodes[i].position.y * height;
        // Radius vergrößert für besseres Greifen
        if (mousePos.getDistanceFrom({ x, y }) < 25.0f) 
            return i;
    }
    return -1;
}

void EnergyDisplay::mouseDown(const juce::MouseEvent& e)
{
    if (nodeSystem == nullptr)
        return;

    int hit = findNodeAt(e.position);
    if (hit >= 0)
    {
        hoveredNode = hit;

        // SHIFT + Click: Mutate individual node parameter
        if (e.mods.isShiftDown())
        {
            const float newVal = juce::Random::getSystemRandom().nextFloat();
            juce::String paramID;
            {
                const juce::ScopedLock nodeLock(nodeSystem->getLock());
                auto& nodes = nodeSystem->getNodes();
                if (! juce::isPositiveAndBelow(hit, (int) nodes.size()))
                    return;

                auto& n = nodes[(size_t) hit];
                n.value = newVal;
                n.energy = 1.0f; // Visual burst
                paramID = n.paramID;
            }

            if (processor != nullptr && paramID.isNotEmpty())
            {
                if (auto* p = processor->tree.getParameter(paramID))
                    p->setValueNotifyingHost(newVal);
            }
        }
        else if (e.mods.isRightButtonDown())
        {
            // Right click to start connection
            connectionStart = hit;
            connectionHover = -1;
        }
        else
        {
            selectedNode = hit;
            connectionHover = -1;
            const juce::ScopedLock nodeLock(nodeSystem->getLock());
            auto& nodes = nodeSystem->getNodes();
            if (! juce::isPositiveAndBelow(selectedNode, (int) nodes.size()))
                return;

            nodes[(size_t) selectedNode].isDragging = true;
            nodes[(size_t) selectedNode].energy = juce::jmin(1.0f, nodes[(size_t) selectedNode].energy + 0.4f);
        }
        repaint();
    }
}

void EnergyDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (connectionStart >= 0)
    {
        const int newHover = findNodeAt(e.position);
        if (connectionHover != newHover || hoveredNode != newHover)
        {
            connectionHover = newHover;
            hoveredNode = newHover;
            repaint();
        }
        return;
    }

    if (selectedNode < 0 || nodeSystem == nullptr) return;

    float nx = juce::jlimit(0.02f, 0.98f, e.position.x / (float)getWidth());
    float ny = juce::jlimit(0.02f, 0.98f, e.position.y / (float)getHeight());

    juce::String paramID;
    {
        const juce::ScopedLock nodeLock(nodeSystem->getLock());
        auto& nodes = nodeSystem->getNodes();
        if (! juce::isPositiveAndBelow(selectedNode, (int) nodes.size()))
            return;

        auto& n = nodes[(size_t) selectedNode];
        n.position = { nx, ny };

        // Update Parameter (Y = Value)
        n.value = 1.0f - ny;
        paramID = n.paramID;
    }

    if (processor != nullptr && paramID.isNotEmpty())
    {
        if (auto* p = processor->tree.getParameter(paramID))
            p->setValueNotifyingHost(1.0f - ny);
    }

    hoveredNode = selectedNode;
    repaint();
}

void EnergyDisplay::mouseUp(const juce::MouseEvent& e)
{
    if (connectionStart >= 0 && nodeSystem != nullptr)
    {
        if (connectionHover >= 0 && connectionHover != connectionStart)
            nodeSystem->addConnection(connectionStart, connectionHover);
        
        connectionStart = -1;
        connectionHover = -1;
        hoveredNode = RootFlow::areHoverEffectsEnabled() ? findNodeAt(e.position) : -1;
        repaint();
        return;
    }

    if (selectedNode >= 0 && nodeSystem != nullptr)
    {
        const juce::ScopedLock nodeLock(nodeSystem->getLock());
        auto& nodes = nodeSystem->getNodes();
        if (juce::isPositiveAndBelow(selectedNode, (int) nodes.size()))
            nodes[(size_t) selectedNode].isDragging = false;

        selectedNode = -1;
        hoveredNode = RootFlow::areHoverEffectsEnabled() ? findNodeAt(e.position) : -1;
        repaint();
    }
}

void EnergyDisplay::mouseMove(const juce::MouseEvent& e)
{
    const int newHover = RootFlow::areHoverEffectsEnabled() ? findNodeAt(e.position) : -1;
    if (newHover != hoveredNode)
    {
        hoveredNode = newHover;
        repaint();
    }

    if (newHover >= 0)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void EnergyDisplay::mouseExit(const juce::MouseEvent&)
{
    hoveredNode = -1;
    if (connectionStart < 0)
        connectionHover = -1;

    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void EnergyDisplay::mouseDoubleClick(const juce::MouseEvent&)
{
    if (nodeSystem == nullptr) return;
    
    auto snapshot = nodeSystem->createSnapshot();
    auto& nodes = snapshot.nodes;
    if (nodes.empty()) return;

    for (int i = 0; i < (int)nodes.size(); ++i)
    {
        nodes[i].position = NodeSystem::getDefaultNodePosition(i);
        nodes[i].energy = 1.0f;
        nodes[i].activity = 1.0f;
        nodes[i].value = 1.0f - nodes[i].position.y;
    }

    {
        const juce::ScopedLock nodeLock(nodeSystem->getLock());
        auto& liveNodes = nodeSystem->getNodes();
        for (int i = 0; i < (int) nodes.size() && i < (int) liveNodes.size(); ++i)
        {
            liveNodes[(size_t) i].position = nodes[(size_t) i].position;
            liveNodes[(size_t) i].energy = nodes[(size_t) i].energy;
            liveNodes[(size_t) i].activity = nodes[(size_t) i].activity;
            liveNodes[(size_t) i].value = nodes[(size_t) i].value;
        }
    }

    if (processor != nullptr)
    {
        for (const auto& node : nodes)
        {
            if (auto* p = processor->tree.getParameter(node.paramID))
                p->setValueNotifyingHost(node.value);
        }
    }
    
    seqFlashNode = 0;
    seqFlashTimer = 1.0f;
    repaint();
}

bool EnergyDisplay::isNearConnection(juce::Point<float> p, const NodeConnection& c, const std::vector<FlowNode>& nodes) const
{
    if (! juce::isPositiveAndBelow(c.source, (int) nodes.size())
        || ! juce::isPositiveAndBelow(c.target, (int) nodes.size()))
        return false;

    float x1 = nodes[c.source].position.x * getWidth();
    float y1 = nodes[c.source].position.y * getHeight();
    float x2 = nodes[c.target].position.x * getWidth();
    float y2 = nodes[c.target].position.y * getHeight();
    
    juce::Line<float> line(x1, y1, x2, y2);
    juce::Point<float> dummy;
    return line.getDistanceFromPoint(p, dummy) < 10.0f;
}

void EnergyDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (nodeSystem == nullptr)
        return;

    const auto snapshot = nodeSystem->createSnapshot();
    std::vector<int> hitConnections;
    for (int i = 0; i < (int) snapshot.connections.size(); ++i)
    {
        if (isNearConnection(e.position, snapshot.connections[(size_t) i], snapshot.nodes))
            hitConnections.push_back(i);
    }

    if (hitConnections.empty())
        return;

    bool changed = false;
    {
        const juce::ScopedLock nodeLock(nodeSystem->getLock());
        auto& connections = nodeSystem->getConnections();
        for (int connectionIndex : hitConnections)
        {
            if (! juce::isPositiveAndBelow(connectionIndex, (int) connections.size()))
                continue;

            auto& c = connections[(size_t) connectionIndex];
            const float sign = c.amount < 0.0f ? -1.0f : 1.0f;
            const float magnitude = juce::jlimit(0.0f, 1.0f, std::abs(c.amount) + wheel.deltaY * 0.1f);
            c.amount = sign * magnitude;
            changed = true;
        }
    }

    if (changed) repaint();
}

void EnergyDisplay::drawKeyboardInteraction(juce::Graphics& g)
{
    if (processor == nullptr) return;

    auto& kbdState = processor->getKeyboardState();
    float w = (float)getWidth();
    float h = (float)getHeight();
    const auto rootHub = juce::Point<float>(w * 0.5f, h * 0.86f);
    bool anyNoteOn = false;

    for (int n = 21; n < 109; ++n)  // Piano range A0-C8
    {
        if (kbdState.isNoteOnForChannels(0xffff, n))
        {
            anyNoteOn = true;
            float xPos = ((float)(n - 21) / 88.0f) * w;

            // Organic sap stream wobbling up toward the root hub
            juce::Path stream;
            stream.startNewSubPath(xPos, h);
            float wobble = std::sin(time * 2.5f + (float)n * 0.3f) * 22.0f;
            stream.quadraticTo(xPos + wobble, h * 0.66f, rootHub.x, rootHub.y);

            // Outer bio-glow
            g.setColour(RootFlow::accent.withAlpha(0.12f));
            g.strokePath(stream, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
            // Core strand
            g.setColour(RootFlow::accent.withAlpha(0.40f));
            g.strokePath(stream, juce::PathStrokeType(1.0f));

            // Riding energy particle
            float pT = std::fmod(time * 2.2f + (float)n * 0.17f, 1.0f);
            auto pPos = stream.getPointAlongPath(pT * stream.getLength());
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.fillEllipse(pPos.x - 2.5f, pPos.y - 2.5f, 5.0f, 5.0f);
            g.setColour(RootFlow::accent.withAlpha(0.26f));
            g.fillEllipse(pPos.x - 5.0f, pPos.y - 5.0f, 10.0f, 10.0f);
        }
    }

    if (anyNoteOn)
    {
        g.setColour(RootFlow::accent.withAlpha(0.12f));
        g.fillEllipse(rootHub.x - 26.0f, rootHub.y - 26.0f, 52.0f, 52.0f);
        g.setColour(juce::Colours::white.withAlpha(0.24f));
        g.drawEllipse(rootHub.x - 18.0f, rootHub.y - 18.0f, 36.0f, 36.0f, 1.2f);
    }
}
