#pragma once
#include <JuceHeader.h>
#include "Utils/DesignTokens.h"

// ============================================================
//  AtmosphericOverlay  v4 — Bioluminescent Panel Response
//
//  God Rays: GONE. Replaced by soft panel-edge glow that
//            emanates FROM the panels on MIDI input.
//  Bio-Dust: Always faintly visible, surges on MIDI.
//
//  Both driven by:  intensity = "atmosphere" param (0..1)
//                   midiVelocity = smoothed MIDI velocity (0..1)
// ============================================================
class AtmosphericOverlay : public juce::Component
{
public:
    AtmosphericOverlay()
    {
        setOpaque(false);
        setInterceptsMouseClicks(false, false);
    }

    void setPhase(float p)        { phase        = p; }
    void setIntensity(float i)    { intensity    = juce::jlimit(0.0f, 1.0f, i); }
    void setMidiVelocity(float v) { midiVelocity = juce::jlimit(0.0f, 1.0f, v); }

    void setSequencerStep(int step, float velocity)
    {
        if (step >= 0 && step < 16)
        {
            if (step != lastStep || velocity > 0.05f)
            {
                seqGlowDecay[step] = 1.0f;
                lastStep = step;
            }
        }
        else
        {
            lastStep = -1;
        }
    }

    void updateDecays()
    {
        for (float& d : seqGlowDecay)
            d = std::max(0.0f, d - 0.032f); // Slightly faster decay (was 0.025)
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float midi  = midiVelocity * 0.72f;              // 0..1, slightly damped for a calmer feel
        const float atmos = intensity * 0.76f;                 // keep atmosphere, but less visual pressure
        const float slow  = 0.5f + 0.5f * std::sin(phase * 0.5f);   // very slow breath
        const float fast  = 0.5f + 0.5f * std::sin(phase * 3.0f);   // faster flicker

        // -------------------------------------------------------
        //  BIO-DUST  — always faintly visible, surges with MIDI
        //  Fixed: minimum alpha is large enough to always draw
        // -------------------------------------------------------
        {
            const float dustFloor  = 0.04f;
            const float dustRest   = dustFloor * (0.26f + atmos * 0.56f);
            const float dustSurge  = midi * 0.26f;
            const float dustAlpha  = dustRest + dustSurge;

            for (int i = 0; i < 64; ++i)
            {
                // Deterministic but slowly drifting positions
                const float fx = std::fmod(std::abs(std::sin((float) i * 117.713f) * 43758.54f), 1.0f);
                const float fy = std::fmod(std::abs(
                    std::sin((float) i * 87.127f + phase * 0.15f) * 9631.27f), 1.0f);

                const float x    = r.getX() + fx * r.getWidth();
                const float y    = r.getY() + fy * r.getHeight();
                // Finer: 0.5 – 2.5 px (was 1.2 – 5.4)
                const float size = 0.5f + (float)(i % 5) * 0.5f;

                // Individual breath per particle
                const float breath  = 0.4f + 0.6f * std::sin(phase * 1.8f + (float) i * 0.9f);
                // Finer alpha — each particle more delicate
                const float alpha   = dustAlpha * breath * 0.32f;

                const juce::Colour col =
                    (i %  7 == 0) ? juce::Colours::cyan.withAlpha(alpha)
                  : (i % 11 == 0) ? RootFlow::violet.withAlpha(alpha)
                  :                 juce::Colours::white.withAlpha(alpha);

                g.setColour(col);
                g.fillEllipse(x - size * 0.5f, y - size * 0.5f, size, size);
            }
        }

        // -------------------------------------------------------
        //  PANEL GLOW  — only on MIDI input
        //  Soft radial blooms that appear to emanate FROM inside
        //  the panels — like bioluminescence, not searchlights.
        //  Positions: approximate panel centres in a 1240x820 layout.
        // -------------------------------------------------------
        if (midi > 0.02f)
        {
            const float glowStr = midi * (0.22f + atmos * 0.58f) * (0.82f + slow * 0.18f);

            // Panel hotspots (relative positions 0..1 in x and y)
            // Left panel, center, right panel, bottom
            struct Hotspot { float rx, ry, radiusX, radiusY; juce::Colour tint; };
            const Hotspot spots[] = {
                { 0.15f, 0.50f, 0.22f, 0.45f, RootFlow::accentSoft },   // left panel
                { 0.50f, 0.45f, 0.28f, 0.42f, RootFlow::accent     },   // center
                { 0.85f, 0.50f, 0.22f, 0.45f, RootFlow::violet     },   // right panel
                { 0.50f, 0.82f, 0.40f, 0.20f, RootFlow::amber      },   // bottom
            };

            for (int i = 0; i < 4; ++i)
            {
                auto& s = spots[i];
                const bool isBottom = (i == 3);
                const float hotspotGlowStr = glowStr * (isBottom ? 0.62f : 1.0f);

                const float cx = r.getX() + r.getWidth()  * s.rx;
                const float cy = r.getY() + r.getHeight() * s.ry;
                const float rx = r.getWidth()  * s.radiusX;
                const float ry = r.getHeight() * s.radiusY;

                // Inner bright core — tight, punchy
                const float coreAlpha = hotspotGlowStr * 0.16f * (0.66f + fast * 0.34f);
                juce::ColourGradient core(
                    s.tint.brighter(0.3f).withAlpha(coreAlpha), cx, cy,
                    juce::Colours::transparentBlack, cx + rx * 0.5f, cy, true);
                g.setGradientFill(core);
                g.fillEllipse(cx - rx * 0.5f, cy - ry * 0.5f, rx, ry);

                // Mid halo — medium radius, visible
                const float haloAlpha = hotspotGlowStr * 0.08f;
                juce::ColourGradient halo(
                    s.tint.withAlpha(haloAlpha), cx, cy,
                    juce::Colours::transparentBlack, cx + rx, cy, true);
                g.setGradientFill(halo);
                g.fillEllipse(cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);

                // Outer bloom — very large, very soft, for ambient bleed
                const float bloomAlpha = hotspotGlowStr * 0.03f;
                juce::ColourGradient bloom(
                    s.tint.withAlpha(bloomAlpha), cx, cy,
                    juce::Colours::transparentBlack, cx + rx * 1.8f, cy, true);
                g.setGradientFill(bloom);
                g.fillEllipse(cx - rx * 1.8f, cy - ry * 1.8f, rx * 3.6f, ry * 3.6f);
            }
        }
        // -------------------------------------------------------
        //  SEQUENCER GLOW — dynamic step-based blooms
        //  Triggers on step change, decays for "organic trail"
        // -------------------------------------------------------
        for (int i = 0; i < 16; ++i)
        {
            if (seqGlowDecay[i] > 0.01f)
            {
                const float glow = seqGlowDecay[i] * (0.3f + atmos * 0.7f);
                const auto pos = getStepScreenPos(i, r);
                const float radius = r.getWidth() * 0.065f;
                
                juce::Colour tint = RootFlow::accent;
                if (i < 4) tint = RootFlow::accentSoft;
                else if (i < 8) tint = RootFlow::accent;
                else if (i < 12) tint = RootFlow::amber;
                else tint = RootFlow::violet;

                // 1. Super Bloom — very large, subtle ambient wash
                const float superRadius = radius * 2.8f;
                juce::ColourGradient superG(tint.withAlpha(glow * 0.06f), pos.x, pos.y,
                                            juce::Colours::transparentBlack, pos.x + superRadius, pos.y, true);
                g.setGradientFill(superG);
                g.fillEllipse(pos.x - superRadius, pos.y - superRadius, superRadius * 2.0f, superRadius * 2.0f);

                // 2. Main Glow — the primary light source
                juce::ColourGradient gred(tint.withAlpha(glow * 0.28f), pos.x, pos.y,
                                          juce::Colours::transparentBlack, pos.x + radius, pos.y, true);
                g.setGradientFill(gred);
                g.fillEllipse(pos.x - radius, pos.y - radius, radius * 2.0f, radius * 2.0f);
                
                // 3. Sharp core spike — the "energy center"
                juce::ColourGradient core(juce::Colours::white.withAlpha(glow * 0.18f), pos.x, pos.y,
                                          juce::Colours::transparentBlack, pos.x + radius * 0.35f, pos.y, true);
                g.setGradientFill(core);
                g.fillEllipse(pos.x - radius * 0.35f, pos.y - radius * 0.35f, radius * 0.7f, radius * 0.7f);
            }
        }

    }

private:
    // Manual mapping of sequencer steps to screen coordinates (1240x820 layout)
    // This replicates BioSequencerComponent::cellCentre but in global overlay space
    juce::Point<float> getStepScreenPos(int index, juce::Rectangle<float> r) const
    {
        // 1. Calculate relative to BioSequencer bounds first
        // MainLayout positions BioSequencer at roughly:
        // x: 0.17 * width (ish, depends on master section)
        // y: 0.10 * height + header
        
        // Accurate layout math from PluginEditor/MainLayout:
        const float headerH = 66.0f;
        const float seqH = r.getHeight() * 0.215f;
        const float seqW = r.getWidth() * 0.66f;
        const float seqX = (r.getWidth() - (seqW + 22.0f + 320.0f)) * 0.5f; // Approximated
        const float seqY = headerH + 10.0f;
        
        // Pad Area inside BioSequencer (replicated from BioSequencerComponent::getPadArea)
        const float padTop = 52.0f + 16.0f;
        const float padBot = 58.0f + 6.0f;
        const float padH = seqH - padTop - padBot;
        const float padW = seqW - 40.0f;
        const float cellW = padW / 9.0f;

        const bool topRow = index < 8;
        const int column = topRow ? index : index - 8;
        const float arch = std::abs(((float) column - 3.5f) / 3.5f);
        
        const float lx = (topRow ? (column + 0.78f) : (column + 1.22f)) * cellW + 20.0f;
        const float ly = padH * (topRow ? (0.38f - (1.0f - arch) * 0.04f)
                                        : (0.74f - (1.0f - arch) * 0.02f)) + padTop;

        return { seqX + lx, seqY + ly };
    }

    float phase        = 0.0f;
    float intensity    = 0.18f;
    float midiVelocity = 0.0f;
    float seqGlowDecay[16] = { 0 };
    int lastStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AtmosphericOverlay)
};
