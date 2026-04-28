#pragma once
#include <JuceHeader.h>

namespace RootFlow
{
    class InteractiveSlider : public juce::Slider
    {
    public:
        std::function<void()> onInteractionStateChange;

        void mouseEnter(const juce::MouseEvent& e) override
        {
            juce::Slider::mouseEnter(e);
            notifyInteractionStateChange();
        }

        void mouseExit(const juce::MouseEvent& e) override
        {
            juce::Slider::mouseExit(e);
            notifyInteractionStateChange();
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            juce::Slider::mouseDown(e);
            notifyInteractionStateChange();
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            juce::Slider::mouseUp(e);
            notifyInteractionStateChange();
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            juce::Slider::mouseDrag(e);
            notifyInteractionStateChange();
        }

    private:
        void notifyInteractionStateChange()
        {
            if (onInteractionStateChange)
                onInteractionStateChange();

            if (auto* parent = getParentComponent())
                parent->repaint();
        }
    };

    static inline bool& hoverEffectsEnabledState()
    {
        static bool enabled = true;
        return enabled;
    }

    static inline bool& idleEffectsEnabledState()
    {
        static bool enabled = false;
        return enabled;
    }

    static inline bool& popupOverlaysEnabledState()
    {
        static bool enabled = false;
        return enabled;
    }

    static inline bool areHoverEffectsEnabled() noexcept
    {
        return hoverEffectsEnabledState();
    }

    static inline void setHoverEffectsEnabled(bool enabled) noexcept
    {
        hoverEffectsEnabledState() = enabled;
    }

    static inline bool areIdleEffectsEnabled() noexcept
    {
        return idleEffectsEnabledState();
    }

    static inline void setIdleEffectsEnabled(bool enabled) noexcept
    {
        idleEffectsEnabledState() = enabled;
    }

    static inline bool arePopupOverlaysEnabled() noexcept
    {
        return popupOverlaysEnabledState();
    }

    static inline void setPopupOverlaysEnabled(bool enabled) noexcept
    {
        popupOverlaysEnabledState() = enabled;
    }

    static inline bool isInteractiveHoverActive(const juce::Component& component) noexcept
    {
        return component.isMouseButtonDown()
            || (areHoverEffectsEnabled() && component.isMouseOverOrDragging());
    }

    // --- CYBERNETIC NEON PALETTE ---
    static inline juce::Colour bg          = juce::Colour(0xff05080a); // Deep Obsidian
    static inline juce::Colour panel       = juce::Colour(0xff0d1b24); // Dark Carbon
    static inline juce::Colour panelSoft   = juce::Colour(0xff122634); // Muted Carbon
    static inline juce::Colour accent      = juce::Colour(0xff00ffff); // Electric Cyan
    static inline juce::Colour accentSoft  = juce::Colour(0xff39ff14); // Matrix Green
    static inline juce::Colour violet      = juce::Colour(0xffbc13fe); // Neon Purple
    static inline juce::Colour amber       = juce::Colour(0xffffa500); // Tron Orange
    static inline juce::Colour text        = juce::Colour(0xffe0f7fa); // Data White
    static inline juce::Colour textMuted   = juce::Colour(0xff78909c); // Steel Grey

    static inline juce::Font getFont(float size = 14.0f)
    {
        return juce::Font(juce::FontOptions(size).withStyle("Plain")).withExtraKerningFactor(0.12f);
    }

    static inline void fillBackdrop(juce::Graphics& g, juce::Rectangle<float> r, float phase = 0.0f)
    {
        const float slowBreath = 0.5f + 0.5f * std::sin(phase * juce::MathConstants<float>::pi);

        juce::ColourGradient base(bg.brighter(0.06f + slowBreath * 0.02f), r.getCentreX(), r.getY(),
                                  bg.darker(0.15f), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(base);
        g.fillRect(r);
    }

    static inline void drawAtmospherics(juce::Graphics& g, juce::Rectangle<float> r, float phase = 0.0f)
    {
        const float breath = 0.5f + 0.5f * std::sin(phase * juce::MathConstants<float>::twoPi);
        juce::ColourGradient overhead(accent.withAlpha(0.08f + breath * 0.04f), r.getCentreX(), r.getY(),
                                      juce::Colours::transparentBlack, r.getCentreX(), r.getY() + r.getHeight() * 0.75f, true);
        g.setGradientFill(overhead);
        g.fillEllipse(r.getX() - r.getWidth() * 0.2f, r.getY() - r.getHeight() * 0.45f, r.getWidth() * 1.4f, r.getHeight() * 1.1f);

        for (int i = 0; i < 2; ++i)
        {
            juce::Path ray;
            float pulseOffset = std::sin(phase * juce::MathConstants<float>::twoPi + (float) i * 1.3f) * 24.0f;
            float xBase = r.getX() + r.getWidth() * (0.18f + i * 0.34f) + pulseOffset;
            ray.startNewSubPath(xBase, r.getY());
            ray.lineTo(xBase + 160.0f, r.getY());
            ray.lineTo(xBase + 320.0f + pulseOffset * 1.8f, r.getBottom());
            ray.lineTo(xBase - 140.0f + pulseOffset * 1.8f, r.getBottom());
            ray.closeSubPath();
            float rayIntensity = (0.09f - i * 0.02f) * (0.7f + breath * 0.3f);
            juce::ColourGradient rayGrad(juce::Colours::white.withAlpha(rayIntensity), xBase + 80.0f, r.getY(),
                                         juce::Colours::transparentBlack, xBase + 80.0f, r.getBottom(), false);
            g.setGradientFill(rayGrad);
            g.fillPath(ray);
            g.setColour(accent.withAlpha(rayIntensity * 0.15f));
            g.strokePath(ray, juce::PathStrokeType(1.0f));
        }
    }

    static inline void drawGlassPanel(juce::Graphics& g, juce::Rectangle<float> r, float corner = 18.0f, float glow = 1.0f)
    {
        const float depth = juce::jlimit(1.0f, 8.0f, corner * 0.18f);
        
        g.setColour(juce::Colours::black.withAlpha(0.32f * glow));
        g.fillRoundedRectangle(r.translated(0.0f, depth * 2.0f + 3.0f).expanded(5.0f, 2.5f), corner + 5.0f);
        g.setColour(juce::Colours::black.withAlpha(0.14f * glow));
        g.fillRoundedRectangle(r.translated(0.0f, depth * 1.0f + 1.0f).expanded(2.0f, 1.0f), corner + 2.0f);

        juce::ColourGradient panelGrad(panelSoft.brighter(0.04f).withAlpha(0.68f * glow), r.getCentreX(), r.getY(),
                                       panel.darker(0.18f).withAlpha(0.74f * glow), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(panelGrad);
        g.fillRoundedRectangle(r, corner);

        g.setColour(juce::Colours::white.withAlpha(0.12f * glow));
        g.drawRoundedRectangle(r.reduced(0.5f), corner, 1.0f);
        
        juce::ColourGradient upperBevel(juce::Colours::white.withAlpha(0.08f * glow),
                                        r.getCentreX(), r.getY() + 1.2f,
                                        juce::Colours::transparentBlack,
                                        r.getCentreX(), r.getY() + r.getHeight() * 0.45f, false);
        g.setGradientFill(upperBevel);
        g.fillRoundedRectangle(r.reduced(1.2f), corner - 1.2f);

        g.setColour(accent.withAlpha(0.14f * glow));
        g.drawRoundedRectangle(r, corner, 1.2f);
    }

    static inline juce::Path makeSystemCore(juce::Rectangle<float> r, bool mirror = false)
    {
        juce::Path p;
        const float x = r.getX();
        const float y = r.getY();
        const float w = r.getWidth();
        const float h = r.getHeight();

        if (! mirror)
        {
            p.startNewSubPath(x + w * 0.15f, y + h * 0.10f);
            p.lineTo(x + w * 0.85f, y + h * 0.05f);
            p.lineTo(x + w * 0.95f, y + h * 0.40f);
            p.lineTo(x + w * 0.80f, y + h * 0.90f);
            p.lineTo(x + w * 0.10f, y + h * 0.95f);
            p.lineTo(x + w * 0.05f, y + h * 0.50f);
        }
        else
        {
            p.startNewSubPath(x + w * 0.85f, y + h * 0.10f);
            p.lineTo(x + w * 0.15f, y + h * 0.05f);
            p.lineTo(x + w * 0.05f, y + h * 0.40f);
            p.lineTo(x + w * 0.20f, y + h * 0.90f);
            p.lineTo(x + w * 0.90f, y + h * 0.95f);
            p.lineTo(x + w * 0.95f, y + h * 0.50f);
        }

        p.closeSubPath();
        return p;
    }

    static inline juce::Path makeSideColumnShell(juce::Rectangle<float> r, bool mirror = false)
    {
        juce::Path p;
        const float x = r.getX();
        const float y = r.getY();
        const float w = r.getWidth();
        const float h = r.getHeight();
        const float curve = w * 0.44f;

        if (! mirror)
        {
            p.startNewSubPath(x, y + curve);
            p.lineTo(x, y + h - curve);
            p.quadraticTo(x, y + h, x + curve, y + h);
            p.lineTo(x + w, y + h);
            p.lineTo(x + w, y);
            p.lineTo(x + curve, y);
            p.quadraticTo(x, y, x, y + curve);
        }
        else
        {
            p.startNewSubPath(x + w, y + curve);
            p.lineTo(x + w, y + h - curve);
            p.quadraticTo(x + w, y + h, x + w - curve, y + h);
            p.lineTo(x, y + h);
            p.lineTo(x, y);
            p.lineTo(x + w - curve, y);
            p.quadraticTo(x + w, y, x + w, y + curve);
        }

        p.closeSubPath();
        return p;
    }

    static inline void drawSystemPanel(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour tint, bool mirror = false, float alpha = 0.82f)
    {
        auto shape = makeSystemCore(r, mirror);
        {
            juce::Graphics::ScopedSaveState state(g);
            auto shadow = shape;
            shadow.applyTransform(juce::AffineTransform::translation(0.0f, 10.0f));
            g.setColour(juce::Colours::black.withAlpha(0.42f));
            g.fillPath(shadow);
        }

        juce::ColourGradient fill(panelSoft.brighter(0.12f).withAlpha(alpha * 0.82f), r.getX() + r.getWidth() * 0.42f, r.getY(),
                                   bg.brighter(0.12f).withAlpha(alpha * 0.88f), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(fill);
        g.fillPath(shape);

        juce::ColourGradient aura(tint.withAlpha(0.18f), r.getCentreX(), r.getY() + r.getHeight() * 0.15f,
                                  juce::Colours::transparentBlack, r.getCentreX(), r.getBottom(), true);
        g.setGradientFill(aura);
        g.fillPath(shape);

        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.strokePath(shape, juce::PathStrokeType(1.4f));
        g.setColour(tint.withAlpha(0.32f));
        g.strokePath(shape, juce::PathStrokeType(2.8f));
    }

    static inline void drawSideColumnShell(juce::Graphics& g, juce::Rectangle<float> r, bool mirror, juce::Colour tint)
    {
        auto shell = makeSideColumnShell(r, mirror);
        
        juce::ColourGradient fill(panel.withAlpha(0.92f), r.getCentreX(), r.getY(),
                                   bg.withAlpha(0.96f), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(fill);
        g.fillPath(shell);

        g.setColour(tint.withAlpha(0.18f));
        g.strokePath(shell, juce::PathStrokeType(2.0f));
        
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.strokePath(shell, juce::PathStrokeType(0.8f));
    }

    static inline void drawDataStream(juce::Graphics& g, juce::Point<float> start, juce::Point<float> end, juce::Colour colour, float alpha = 0.24f, float thickness = 1.2f)
    {
        juce::Path stream;
        stream.startNewSubPath(start);
        stream.cubicTo(start.x + (end.x - start.x) * 0.32f, start.y,
                      start.x + (end.x - start.x) * 0.68f, end.y,
                      end.x, end.y);

        g.setColour(colour.withAlpha(alpha * 0.34f));
        g.strokePath(stream, juce::PathStrokeType(thickness * 2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(colour.withAlpha(alpha * 0.72f));
        g.strokePath(stream, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static inline void drawSynapticMatrix(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float intensity = 0.12f, int seed = 42)
    {
        juce::Random rnd (seed);
        for (int i = 0; i < 14; ++i)
        {
            g.setColour(colour.withAlpha(intensity * (0.22f + rnd.nextFloat() * 0.14f)));
            float x1 = r.getX() + rnd.nextFloat() * r.getWidth();
            float y1 = r.getY() + rnd.nextFloat() * r.getHeight();
            float x2 = r.getX() + rnd.nextFloat() * r.getWidth();
            float y2 = r.getY() + rnd.nextFloat() * r.getHeight();
            g.drawLine(x1, y1, x2, y2, 0.8f + rnd.nextFloat() * 0.6f);
            
            if (rnd.nextFloat() > 0.88f)
            {
                const float glowSize = 2.2f + rnd.nextFloat() * 3.4f;
                g.setColour(colour.brighter(0.4f).withAlpha(intensity * 0.58f));
                g.fillEllipse(x1 - glowSize * 0.5f, y1 - glowSize * 0.5f, glowSize, glowSize);
            }
        }
    }

    static inline void drawCoreNode(juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour tint, float alpha = 1.0f)
    {
        g.setColour(juce::Colours::black.withAlpha(0.24f * alpha));
        g.fillEllipse(juce::Rectangle<float>(radius * 2.8f, radius * 2.8f).withCentre(centre.translated(0.0f, 2.0f)));
        
        juce::ColourGradient node(tint.brighter(0.3f).withAlpha(0.98f * alpha), centre.x, centre.y - radius,
                                  tint.darker(0.4f).withAlpha(0.98f * alpha), centre.x, centre.y + radius, true);
        g.setGradientFill(node);
        g.fillEllipse(juce::Rectangle<float>(radius * 2.1f, radius * 2.1f).withCentre(centre));

        g.setColour(juce::Colours::white.withAlpha(0.42f * alpha));
        g.drawEllipse(juce::Rectangle<float>(radius * 2.1f, radius * 2.1f).withCentre(centre), 1.2f);
    }

    enum class TabLabelStyle
    {
        soft,
        prominent
    };

    static inline void drawTabLabel(juce::Graphics& g,
                                    juce::Rectangle<float> r,
                                    const juce::String& text,
                                    TabLabelStyle style = TabLabelStyle::soft)
    {
        const bool prominent = style == TabLabelStyle::prominent;
        const auto fillTint = prominent ? accent : textMuted.interpolatedWith(accent, 0.28f);

        drawGlassPanel(g, r, r.getHeight() * 0.5f, prominent ? 0.72f : 0.44f);
        g.setColour(fillTint.withAlpha(prominent ? 0.034f : 0.018f));
        g.fillRoundedRectangle(r.reduced(2.0f), r.getHeight() * 0.40f);

        auto textArea = r.toNearestInt().reduced(10, 0);
        g.setFont(getFont(r.getHeight() * (prominent ? 0.42f : 0.38f)).boldened());
        g.setColour(juce::Colours::black.withAlpha(prominent ? 0.24f : 0.18f));
        g.drawFittedText(text, textArea.translated(0, 1), juce::Justification::centred, 1);

        g.setColour((prominent ? RootFlow::text : RootFlow::textMuted.interpolatedWith(RootFlow::text, 0.54f))
                        .withAlpha(prominent ? 0.92f : 0.78f));
        g.drawFittedText(text, textArea, juce::Justification::centred, 1);
    }

    static inline void drawGlowOrb(juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, float alpha = 1.0f)
    {
        g.setColour(colour.withAlpha(0.08f * alpha));
        g.fillEllipse(juce::Rectangle<float>(radius * 3.2f, radius * 3.2f).withCentre(centre));

        juce::ColourGradient orb(colour.brighter(0.24f).withAlpha(0.72f * alpha), centre.x, centre.y - radius * 0.7f,
                                 colour.darker(0.32f).withAlpha(0.72f * alpha), centre.x, centre.y + radius, true);
        g.setGradientFill(orb);
        g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre));

        g.setColour(juce::Colours::white.withAlpha(0.18f * alpha));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.72f, radius * 0.72f).withCentre({ centre.x - radius * 0.25f, centre.y - radius * 0.32f }));
    }

    static inline void drawPanel(juce::Graphics& g, juce::Rectangle<float> r)
    {
        drawGlassPanel(g, r, 20.0f, 1.0f);
    }
}
