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

    static inline juce::Colour bg          = juce::Colour(0xff061019);
    static inline juce::Colour panel       = juce::Colour(0xff102133);
    static inline juce::Colour panelSoft   = juce::Colour(0xff173349);
    static inline juce::Colour accent      = juce::Colour(0xff74f7ff);
    static inline juce::Colour accentSoft  = juce::Colour(0xff93ff8b);
    static inline juce::Colour violet      = juce::Colour(0xff7a6dff);
    static inline juce::Colour amber       = juce::Colour(0xffffe5a8);
    static inline juce::Colour text        = juce::Colour(0xffd7f3ff);
    static inline juce::Colour textMuted   = juce::Colour(0xff8fb8ca);

    static inline juce::Font getFont(float size = 14.0f)
    {
        return juce::Font(juce::FontOptions(size).withStyle("Plain")).withExtraKerningFactor(0.1f);
    }

    static inline void fillBackdrop(juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::ColourGradient base(bg.brighter(0.12f), r.getCentreX(), r.getY() - r.getHeight() * 0.10f,
                                  bg.darker(0.18f), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(base);
        g.fillRect(r);

        juce::ColourGradient nebula(violet.withAlpha(0.10f), r.getX() + r.getWidth() * 0.18f, r.getY() + r.getHeight() * 0.22f,
                                    juce::Colours::transparentBlack, r.getX() + r.getWidth() * 0.35f, r.getY() + r.getHeight() * 0.55f, true);
        g.setGradientFill(nebula);
        g.fillEllipse(r.getX() + r.getWidth() * 0.02f, r.getY() + r.getHeight() * 0.08f, r.getWidth() * 0.42f, r.getHeight() * 0.34f);

        juce::ColourGradient tealMist(accent.withAlpha(0.09f), r.getRight() - r.getWidth() * 0.22f, r.getY() + r.getHeight() * 0.16f,
                                      juce::Colours::transparentBlack, r.getRight() - r.getWidth() * 0.12f, r.getY() + r.getHeight() * 0.44f, true);
        g.setGradientFill(tealMist);
        g.fillEllipse(r.getRight() - r.getWidth() * 0.36f, r.getY() + r.getHeight() * 0.04f, r.getWidth() * 0.34f, r.getHeight() * 0.30f);

        juce::ColourGradient growthMist(accentSoft.withAlpha(0.06f), r.getX() + r.getWidth() * 0.40f, r.getY() + r.getHeight() * 0.34f,
                                        juce::Colours::transparentBlack, r.getX() + r.getWidth() * 0.46f, r.getBottom() - r.getHeight() * 0.18f, true);
        g.setGradientFill(growthMist);
        g.fillEllipse(r.getX() + r.getWidth() * 0.26f, r.getY() + r.getHeight() * 0.24f, r.getWidth() * 0.30f, r.getHeight() * 0.42f);

        juce::ColourGradient amberHaze(amber.withAlpha(0.05f), r.getCentreX(), r.getY() + r.getHeight() * 0.10f,
                                       juce::Colours::transparentBlack, r.getCentreX(), r.getY() + r.getHeight() * 0.36f, true);
        g.setGradientFill(amberHaze);
        g.fillEllipse(r.getCentreX() - r.getWidth() * 0.18f, r.getY() - r.getHeight() * 0.02f, r.getWidth() * 0.36f, r.getHeight() * 0.24f);

        for (int i = 0; i < 28; ++i)
        {
            const float fx = std::fmod(std::sin((float) i * 91.713f) * 43758.54f, 1.0f);
            const float fy = std::fmod(std::sin((float) i * 47.127f) * 9631.27f, 1.0f);
            const float x = r.getX() + std::abs(fx) * r.getWidth();
            const float y = r.getY() + std::abs(fy) * r.getHeight() * 0.82f;
            const float size = 0.65f + (float) (i % 3) * 0.50f;
            const juce::Colour star = (i % 5 == 0 ? accent : juce::Colours::white).withAlpha(0.035f + (float) (i % 7) * 0.018f);
            g.setColour(star);
            g.fillEllipse(x, y, size, size);
        }
    }

    static inline void drawGlassPanel(juce::Graphics& g, juce::Rectangle<float> r, float corner = 18.0f, float glow = 1.0f)
    {
        const float depth = juce::jlimit(1.0f, 8.0f, corner * 0.18f);
        const auto seat = r.expanded(3.4f, 3.0f).translated(0.0f, depth * 0.34f + 1.0f);
        juce::ColourGradient seatGrad(panelSoft.brighter(0.08f).withAlpha(0.26f * glow),
                                      seat.getCentreX(), seat.getY(),
                                      bg.darker(0.40f).withAlpha(0.58f * glow),
                                      seat.getCentreX(), seat.getBottom(), false);
        g.setGradientFill(seatGrad);
        g.fillRoundedRectangle(seat, corner + 3.0f);
        g.setColour(juce::Colours::black.withAlpha(0.12f * glow));
        g.drawRoundedRectangle(seat.reduced(1.0f), corner + 2.0f, 0.9f);

        g.setColour(juce::Colours::black.withAlpha(0.20f * glow));
        g.fillRoundedRectangle(r.translated(0.0f, depth + 2.0f).expanded(2.0f, 1.0f), corner + 1.5f);
        g.setColour(juce::Colours::black.withAlpha(0.12f * glow));
        g.fillRoundedRectangle(r.translated(0.0f, depth * 0.55f).expanded(0.8f, 0.4f), corner + 0.8f);

        juce::ColourGradient panelGrad(panelSoft.brighter(0.10f).interpolatedWith(accent, 0.04f), r.getCentreX(), r.getY(),
                                       panel.darker(0.12f).interpolatedWith(violet, 0.08f), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(panelGrad);
        g.fillRoundedRectangle(r, corner);

        juce::ColourGradient upperBevel(juce::Colours::white.withAlpha(0.070f * glow),
                                        r.getCentreX(), r.getY() + 1.0f,
                                        juce::Colours::transparentBlack,
                                        r.getCentreX(), r.getY() + r.getHeight() * 0.44f, false);
        g.setGradientFill(upperBevel);
        g.fillRoundedRectangle(r.reduced(1.0f), corner - 1.0f);

        juce::ColourGradient lowerOcclusion(juce::Colours::transparentBlack,
                                            r.getCentreX(), r.getY() + r.getHeight() * 0.46f,
                                            juce::Colours::black.withAlpha(0.14f * glow),
                                            r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(lowerOcclusion);
        g.fillRoundedRectangle(r.reduced(1.8f), corner - 1.8f);

        juce::ColourGradient colourFilm(accent.withAlpha(0.020f * glow), r.getX(), r.getY(),
                                        violet.withAlpha(0.018f * glow), r.getRight(), r.getBottom(), false);
        g.setGradientFill(colourFilm);
        g.fillRoundedRectangle(r.reduced(2.0f), corner - 2.0f);

        juce::ColourGradient innerPocket(juce::Colours::transparentBlack,
                                         r.getCentreX(), r.getY() + r.getHeight() * 0.18f,
                                         juce::Colours::black.withAlpha(0.10f * glow),
                                         r.getCentreX(), r.getBottom() - 1.0f, false);
        g.setGradientFill(innerPocket);
        g.fillRoundedRectangle(r.reduced(2.8f), corner - 2.8f);

        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clip;
            clip.addRoundedRectangle(r.reduced(2.0f), corner - 2.0f);
            g.reduceClipRegion(clip);

            juce::ColourGradient leftWall(juce::Colours::white.withAlpha(0.024f * glow),
                                          r.getX() + r.getWidth() * 0.08f, r.getY(),
                                          juce::Colours::transparentBlack,
                                          r.getX() + r.getWidth() * 0.18f, r.getBottom(), false);
            g.setGradientFill(leftWall);
            g.fillRect(juce::Rectangle<float>(r.getX() + 2.0f, r.getY() + 2.0f,
                                              juce::jmax(8.0f, r.getWidth() * 0.12f), r.getHeight() - 4.0f));

            juce::ColourGradient rightWall(juce::Colours::transparentBlack,
                                           r.getRight() - r.getWidth() * 0.18f, r.getY(),
                                           juce::Colours::black.withAlpha(0.070f * glow),
                                           r.getRight() - r.getWidth() * 0.06f, r.getBottom(), false);
            g.setGradientFill(rightWall);
            g.fillRect(juce::Rectangle<float>(r.getRight() - juce::jmax(8.0f, r.getWidth() * 0.12f) - 2.0f,
                                              r.getY() + 2.0f,
                                              juce::jmax(8.0f, r.getWidth() * 0.12f), r.getHeight() - 4.0f));
        }

        auto lowerLip = juce::Rectangle<float>(r.getWidth() - 10.0f, juce::jmax(8.0f, r.getHeight() * 0.18f))
                            .withCentre({ r.getCentreX(), r.getBottom() - juce::jmax(5.0f, r.getHeight() * 0.11f) });
        juce::ColourGradient lowerLipGrad(juce::Colours::transparentBlack,
                                          lowerLip.getCentreX(), lowerLip.getY(),
                                          juce::Colours::white.withAlpha(0.050f * glow),
                                          lowerLip.getCentreX(), lowerLip.getBottom(), false);
        g.setGradientFill(lowerLipGrad);
        g.fillRoundedRectangle(lowerLip, juce::jmax(6.0f, corner - 7.0f));

        g.setColour(juce::Colours::white.withAlpha(0.014f * glow));
        g.fillRoundedRectangle(r.reduced(3.4f).withHeight(r.getHeight() * 0.40f), corner - 3.4f);

        g.setColour(accent.withAlpha(0.12f * glow));
        g.drawRoundedRectangle(r, corner, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.12f * glow));
        g.drawRoundedRectangle(r.reduced(1.5f), corner - 1.5f, 0.8f);

        g.setColour(juce::Colours::black.withAlpha(0.16f * glow));
        g.drawRoundedRectangle(r.reduced(3.0f).translated(0.0f, 1.0f), corner - 3.0f, 0.8f);

        juce::Path highlight;
        highlight.startNewSubPath(r.getX() + corner, r.getY() + 1.5f);
        highlight.quadraticTo(r.getCentreX(), r.getY() - r.getHeight() * 0.08f, r.getRight() - corner, r.getY() + r.getHeight() * 0.08f);
        g.setColour(juce::Colours::white.withAlpha(0.080f * glow));
        g.strokePath(highlight, juce::PathStrokeType(1.2f));
    }

    static inline juce::Path makeOrganicPod(juce::Rectangle<float> r, bool mirror = false)
    {
        juce::Path p;
        const float x = r.getX();
        const float y = r.getY();
        const float w = r.getWidth();
        const float h = r.getHeight();

        if (! mirror)
        {
            p.startNewSubPath(x + w * 0.18f, y + h * 0.06f);
            p.cubicTo(x + w * 0.76f, y - h * 0.01f, x + w * 0.96f, y + h * 0.16f, x + w * 0.84f, y + h * 0.38f);
            p.cubicTo(x + w * 0.74f, y + h * 0.56f, x + w * 0.96f, y + h * 0.82f, x + w * 0.68f, y + h * 0.94f);
            p.cubicTo(x + w * 0.34f, y + h, x + w * 0.08f, y + h * 0.90f, x + w * 0.08f, y + h * 0.60f);
            p.cubicTo(x + w * 0.05f, y + h * 0.40f, x + w * 0.04f, y + h * 0.16f, x + w * 0.18f, y + h * 0.06f);
        }
        else
        {
            p.startNewSubPath(x + w * 0.82f, y + h * 0.06f);
            p.cubicTo(x + w * 0.24f, y - h * 0.01f, x + w * 0.04f, y + h * 0.16f, x + w * 0.16f, y + h * 0.38f);
            p.cubicTo(x + w * 0.26f, y + h * 0.56f, x + w * 0.04f, y + h * 0.82f, x + w * 0.32f, y + h * 0.94f);
            p.cubicTo(x + w * 0.66f, y + h, x + w * 0.92f, y + h * 0.90f, x + w * 0.92f, y + h * 0.60f);
            p.cubicTo(x + w * 0.95f, y + h * 0.40f, x + w * 0.96f, y + h * 0.16f, x + w * 0.82f, y + h * 0.06f);
        }

        p.closeSubPath();
        return p;
    }

    static inline void drawOrganicPanel(juce::Graphics& g, juce::Rectangle<float> r, bool mirror = false)
    {
        auto shape = makeOrganicPod(r, mirror);
        {
            juce::Graphics::ScopedSaveState state(g);
            auto shadow = shape;
            shadow.applyTransform(juce::AffineTransform::translation(0.0f, 8.0f));
            g.setColour(juce::Colours::black.withAlpha(0.18f));
            g.fillPath(shadow);
            shadow.applyTransform(juce::AffineTransform::translation(0.0f, -4.0f));
            g.setColour(juce::Colours::black.withAlpha(0.09f));
            g.fillPath(shadow);
        }

        juce::ColourGradient fill(panelSoft.brighter(0.16f).interpolatedWith(accent, 0.02f), r.getX() + r.getWidth() * 0.42f, r.getY(),
                                  violet.darker(0.18f).interpolatedWith(panel, 0.58f), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(fill);
        g.fillPath(shape);

        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(shape);

            juce::ColourGradient topBulge(juce::Colours::white.withAlpha(0.09f),
                                          r.getCentreX(), r.getY() + r.getHeight() * 0.02f,
                                          juce::Colours::transparentBlack,
                                          r.getCentreX(), r.getY() + r.getHeight() * 0.42f, false);
            g.setGradientFill(topBulge);
            g.fillEllipse(r.getX() + r.getWidth() * 0.08f, r.getY() - r.getHeight() * 0.04f,
                          r.getWidth() * 0.84f, r.getHeight() * 0.46f);

            juce::ColourGradient lowerShade(juce::Colours::transparentBlack,
                                            r.getCentreX(), r.getY() + r.getHeight() * 0.46f,
                                            juce::Colours::black.withAlpha(0.18f),
                                            r.getCentreX(), r.getBottom() + r.getHeight() * 0.06f, false);
            g.setGradientFill(lowerShade);
            g.fillEllipse(r.getX() + r.getWidth() * 0.06f, r.getY() + r.getHeight() * 0.44f,
                          r.getWidth() * 0.88f, r.getHeight() * 0.58f);
        }

        juce::ColourGradient aura(accent.withAlpha(0.06f), r.getCentreX(), r.getY() + r.getHeight() * 0.12f,
                                  juce::Colours::transparentBlack, r.getCentreX(), r.getBottom(), true);
        g.setGradientFill(aura);
        g.fillPath(shape);

        juce::ColourGradient violetVeil(violet.withAlpha(0.05f), mirror ? r.getX() + r.getWidth() * 0.26f : r.getRight() - r.getWidth() * 0.26f,
                                        r.getY() + r.getHeight() * 0.18f,
                                        juce::Colours::transparentBlack, r.getCentreX(), r.getBottom(), true);
        g.setGradientFill(violetVeil);
        g.fillPath(shape);

        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.strokePath(shape, juce::PathStrokeType(1.2f));
        g.setColour(accent.withAlpha(0.14f));
        g.strokePath(shape, juce::PathStrokeType(2.8f));
    }

    static inline juce::Path makeSideColumnShell(juce::Rectangle<float> r, bool mirror = false)
    {
        juce::Path p;
        const float x = r.getX();
        const float y = r.getY();
        const float w = r.getWidth();
        const float h = r.getHeight();

        if (! mirror)
        {
            p.startNewSubPath(x + w * 0.28f, y + h * 0.02f);
            p.cubicTo(x + w * 0.80f, y + h * 0.00f, x + w * 0.98f, y + h * 0.10f, x + w * 0.96f, y + h * 0.26f);
            p.cubicTo(x + w * 0.94f, y + h * 0.38f, x + w * 0.74f, y + h * 0.44f, x + w * 0.72f, y + h * 0.54f);
            p.cubicTo(x + w * 0.70f, y + h * 0.68f, x + w * 0.96f, y + h * 0.78f, x + w * 0.94f, y + h * 0.94f);
            p.cubicTo(x + w * 0.92f, y + h * 1.00f, x + w * 0.72f, y + h * 1.02f, x + w * 0.42f, y + h * 0.98f);
            p.cubicTo(x + w * 0.18f, y + h * 0.94f, x + w * 0.06f, y + h * 0.84f, x + w * 0.10f, y + h * 0.64f);
            p.cubicTo(x + w * 0.14f, y + h * 0.50f, x + w * 0.04f, y + h * 0.34f, x + w * 0.10f, y + h * 0.14f);
            p.cubicTo(x + w * 0.14f, y + h * 0.06f, x + w * 0.18f, y + h * 0.02f, x + w * 0.28f, y + h * 0.02f);
        }
        else
        {
            p.startNewSubPath(x + w * 0.72f, y + h * 0.02f);
            p.cubicTo(x + w * 0.20f, y + h * 0.00f, x + w * 0.02f, y + h * 0.10f, x + w * 0.04f, y + h * 0.26f);
            p.cubicTo(x + w * 0.06f, y + h * 0.38f, x + w * 0.26f, y + h * 0.44f, x + w * 0.28f, y + h * 0.54f);
            p.cubicTo(x + w * 0.30f, y + h * 0.68f, x + w * 0.04f, y + h * 0.78f, x + w * 0.06f, y + h * 0.94f);
            p.cubicTo(x + w * 0.08f, y + h * 1.00f, x + w * 0.28f, y + h * 1.02f, x + w * 0.58f, y + h * 0.98f);
            p.cubicTo(x + w * 0.82f, y + h * 0.94f, x + w * 0.94f, y + h * 0.84f, x + w * 0.90f, y + h * 0.64f);
            p.cubicTo(x + w * 0.86f, y + h * 0.50f, x + w * 0.96f, y + h * 0.34f, x + w * 0.90f, y + h * 0.14f);
            p.cubicTo(x + w * 0.86f, y + h * 0.06f, x + w * 0.82f, y + h * 0.02f, x + w * 0.72f, y + h * 0.02f);
        }

        p.closeSubPath();
        return p;
    }

    static inline void drawSideColumnShell(juce::Graphics& g, juce::Rectangle<float> r, bool mirror = false, juce::Colour tint = accent)
    {
        auto shell = makeSideColumnShell(r, mirror);
        {
            juce::Graphics::ScopedSaveState state(g);
            auto shadow = shell;
            shadow.applyTransform(juce::AffineTransform::translation(0.0f, 8.0f));
            g.setColour(juce::Colours::black.withAlpha(0.18f));
            g.fillPath(shadow);
            shadow.applyTransform(juce::AffineTransform::translation(0.0f, -4.0f));
            g.setColour(juce::Colours::black.withAlpha(0.09f));
            g.fillPath(shadow);
        }

        juce::ColourGradient fill(violet.withAlpha(0.20f), r.getX() + r.getWidth() * (mirror ? 0.68f : 0.32f), r.getY(),
                                  panelSoft.brighter(0.14f), r.getCentreX(), r.getY() + r.getHeight() * 0.38f, false);
        g.setGradientFill(fill);
        g.fillPath(shell);

        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(shell);

            juce::ColourGradient topBulge(juce::Colours::white.withAlpha(0.08f),
                                          r.getCentreX(), r.getY(),
                                          juce::Colours::transparentBlack,
                                          r.getCentreX(), r.getY() + r.getHeight() * 0.40f, false);
            g.setGradientFill(topBulge);
            g.fillEllipse(r.getX() + r.getWidth() * 0.04f, r.getY() - r.getHeight() * 0.04f,
                          r.getWidth() * 0.92f, r.getHeight() * 0.42f);

            juce::ColourGradient lowerShade(juce::Colours::transparentBlack,
                                            r.getCentreX(), r.getY() + r.getHeight() * 0.44f,
                                            juce::Colours::black.withAlpha(0.18f),
                                            r.getCentreX(), r.getBottom(), false);
            g.setGradientFill(lowerShade);
            g.fillEllipse(r.getX() + r.getWidth() * 0.06f, r.getY() + r.getHeight() * 0.46f,
                          r.getWidth() * 0.88f, r.getHeight() * 0.54f);
        }

        juce::ColourGradient wash(tint.withAlpha(0.08f), r.getCentreX(), r.getY() + r.getHeight() * 0.12f,
                                  juce::Colours::transparentBlack, r.getCentreX(), r.getBottom(), true);
        g.setGradientFill(wash);
        g.fillPath(shell);

        juce::ColourGradient innerBloom(accent.withAlpha(0.035f), mirror ? r.getX() + r.getWidth() * 0.34f : r.getRight() - r.getWidth() * 0.34f,
                                        r.getY() + r.getHeight() * 0.20f,
                                        juce::Colours::transparentBlack, r.getCentreX(), r.getBottom(), true);
        g.setGradientFill(innerBloom);
        g.fillPath(shell);

        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(shell);

            for (int i = 0; i < 10; ++i)
            {
                const float fx = 0.18f + std::abs(std::sin((float) i * 0.71f)) * 0.66f;
                const float fy = 0.06f + std::abs(std::sin((float) i * 1.17f + 0.8f)) * 0.88f;
                const float radius = 10.0f + (float) ((i * 7) % 17);
                const auto bubble = juce::Rectangle<float>(radius * 1.8f, radius * 1.5f)
                                        .withCentre({ r.getX() + r.getWidth() * fx, r.getY() + r.getHeight() * fy });
                const auto cellTint = (i % 4 == 0) ? violet
                                     : (i % 4 == 1) ? tint
                                     : (i % 4 == 2) ? accentSoft
                                                    : amber;
                g.setColour(cellTint.withAlpha(0.03f + (float) (i % 5) * 0.014f));
                g.fillEllipse(bubble);
            }
        }

        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.strokePath(shell, juce::PathStrokeType(1.3f));
        g.setColour(tint.withAlpha(0.18f));
        g.strokePath(shell, juce::PathStrokeType(2.8f));

        juce::Path highlight;
        if (! mirror)
        {
            highlight.startNewSubPath(r.getX() + r.getWidth() * 0.26f, r.getY() + r.getHeight() * 0.08f);
            highlight.cubicTo(r.getX() + r.getWidth() * 0.12f, r.getY() + r.getHeight() * 0.24f,
                              r.getX() + r.getWidth() * 0.18f, r.getY() + r.getHeight() * 0.70f,
                              r.getX() + r.getWidth() * 0.32f, r.getY() + r.getHeight() * 0.90f);
        }
        else
        {
            highlight.startNewSubPath(r.getRight() - r.getWidth() * 0.26f, r.getY() + r.getHeight() * 0.08f);
            highlight.cubicTo(r.getRight() - r.getWidth() * 0.12f, r.getY() + r.getHeight() * 0.24f,
                              r.getRight() - r.getWidth() * 0.18f, r.getY() + r.getHeight() * 0.70f,
                              r.getRight() - r.getWidth() * 0.32f, r.getY() + r.getHeight() * 0.90f);
        }
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.strokePath(highlight, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static inline juce::Path makeMembraneChamber(juce::Rectangle<float> r, bool mirror = false)
    {
        juce::Path p;
        const float x = r.getX();
        const float y = r.getY();
        const float w = r.getWidth();
        const float h = r.getHeight();

        if (! mirror)
        {
            p.startNewSubPath(x + w * 0.18f, y + h * 0.08f);
            p.cubicTo(x + w * 0.64f, y - h * 0.02f, x + w * 0.94f, y + h * 0.10f, x + w * 0.90f, y + h * 0.30f);
            p.cubicTo(x + w * 0.86f, y + h * 0.46f, x + w * 0.88f, y + h * 0.72f, x + w * 0.74f, y + h * 0.90f);
            p.cubicTo(x + w * 0.50f, y + h * 1.00f, x + w * 0.18f, y + h * 0.98f, x + w * 0.12f, y + h * 0.76f);
            p.cubicTo(x + w * 0.08f, y + h * 0.56f, x + w * 0.06f, y + h * 0.22f, x + w * 0.18f, y + h * 0.08f);
        }
        else
        {
            p.startNewSubPath(x + w * 0.82f, y + h * 0.08f);
            p.cubicTo(x + w * 0.36f, y - h * 0.02f, x + w * 0.06f, y + h * 0.10f, x + w * 0.10f, y + h * 0.30f);
            p.cubicTo(x + w * 0.14f, y + h * 0.46f, x + w * 0.12f, y + h * 0.72f, x + w * 0.26f, y + h * 0.90f);
            p.cubicTo(x + w * 0.50f, y + h * 1.00f, x + w * 0.82f, y + h * 0.98f, x + w * 0.88f, y + h * 0.76f);
            p.cubicTo(x + w * 0.92f, y + h * 0.56f, x + w * 0.94f, y + h * 0.22f, x + w * 0.82f, y + h * 0.08f);
        }

        p.closeSubPath();
        return p;
    }

    static inline void drawMembraneChamber(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour tint, bool mirror = false, float glow = 1.0f)
    {
        auto chamber = makeMembraneChamber(r, mirror);
        {
            juce::Graphics::ScopedSaveState state(g);
            auto shadow = chamber;
            shadow.applyTransform(juce::AffineTransform::translation(0.0f, 6.0f));
            g.setColour(juce::Colours::black.withAlpha(0.16f * glow));
            g.fillPath(shadow);
            shadow.applyTransform(juce::AffineTransform::translation(0.0f, -3.0f));
            g.setColour(juce::Colours::black.withAlpha(0.08f * glow));
            g.fillPath(shadow);
        }

        juce::ColourGradient fill(panelSoft.brighter(0.16f).interpolatedWith(tint, 0.06f), r.getCentreX(), r.getY(),
                                  violet.darker(0.22f), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(fill);
        g.fillPath(chamber);

        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(chamber);

            juce::ColourGradient topBulge(juce::Colours::white.withAlpha(0.08f * glow),
                                          r.getCentreX(), r.getY(),
                                          juce::Colours::transparentBlack,
                                          r.getCentreX(), r.getY() + r.getHeight() * 0.38f, false);
            g.setGradientFill(topBulge);
            g.fillEllipse(r.getX() + r.getWidth() * 0.10f, r.getY() - r.getHeight() * 0.05f,
                          r.getWidth() * 0.80f, r.getHeight() * 0.42f);

            juce::ColourGradient lowerShade(juce::Colours::transparentBlack,
                                            r.getCentreX(), r.getY() + r.getHeight() * 0.42f,
                                            juce::Colours::black.withAlpha(0.18f * glow),
                                            r.getCentreX(), r.getBottom() + r.getHeight() * 0.04f, false);
            g.setGradientFill(lowerShade);
            g.fillEllipse(r.getX() + r.getWidth() * 0.08f, r.getY() + r.getHeight() * 0.48f,
                          r.getWidth() * 0.84f, r.getHeight() * 0.50f);
        }

        juce::ColourGradient tintWash(tint.withAlpha(0.10f * glow), r.getCentreX(), r.getY() + r.getHeight() * 0.10f,
                                      juce::Colours::transparentBlack, r.getCentreX(), r.getBottom(), true);
        g.setGradientFill(tintWash);
        g.fillPath(chamber);

        juce::ColourGradient innerBloom(accent.interpolatedWith(amber, 0.25f).withAlpha(0.032f * glow),
                                        mirror ? r.getX() + r.getWidth() * 0.26f : r.getRight() - r.getWidth() * 0.26f,
                                        r.getY() + r.getHeight() * 0.18f,
                                        juce::Colours::transparentBlack, r.getCentreX(), r.getBottom(), true);
        g.setGradientFill(innerBloom);
        g.fillPath(chamber);

        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(chamber);

            for (int i = 0; i < 8; ++i)
            {
                const float fx = 0.18f + std::abs(std::sin((float) i * 0.63f + 0.2f)) * 0.62f;
                const float fy = 0.10f + std::abs(std::sin((float) i * 1.11f + 0.7f)) * 0.76f;
                const float size = 16.0f + (float) ((i * 5) % 14);
                auto cell = juce::Rectangle<float>(size * 1.45f, size * 1.15f)
                                .withCentre({ r.getX() + r.getWidth() * fx, r.getY() + r.getHeight() * fy });
                const auto cellTint = (i % 4 == 0) ? tint
                                     : (i % 4 == 1) ? violet
                                     : (i % 4 == 2) ? accent
                                                    : amber;
                g.setColour(cellTint.withAlpha(0.03f + (float) (i % 4) * 0.012f));
                g.fillEllipse(cell);
            }
        }

        g.setColour(juce::Colours::white.withAlpha(0.14f * glow));
        g.strokePath(chamber, juce::PathStrokeType(1.2f));
        g.setColour(tint.withAlpha(0.20f * glow));
        g.strokePath(chamber, juce::PathStrokeType(2.1f));
    }

    static inline void drawBioThread(juce::Graphics& g, juce::Point<float> start, juce::Point<float> end, juce::Colour colour, float alpha = 0.24f, float thickness = 1.2f)
    {
        juce::Path fibre;
        fibre.startNewSubPath(start);
        fibre.cubicTo(start.x + (end.x - start.x) * 0.32f, start.y - 8.0f,
                      start.x + (end.x - start.x) * 0.72f, end.y + 8.0f,
                      end.x, end.y);

        g.setColour(colour.withAlpha(alpha * 0.44f));
        g.strokePath(fibre, juce::PathStrokeType(thickness * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(colour.withAlpha(alpha * 0.82f));
        g.strokePath(fibre, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colours::white.withAlpha(alpha * 0.18f));
        g.strokePath(fibre, juce::PathStrokeType(juce::jmax(0.6f, thickness * 0.34f),
                                                 juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    static inline void drawMyceliumNetwork(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float intensity = 0.12f, int seed = 42)
    {
        juce::Random rnd (seed);

        for (int i = 0; i < 12; ++i)
        {
            g.setColour(colour.withAlpha(intensity * (0.16f + rnd.nextFloat() * 0.12f)));
            juce::Path branch;
            float x1 = r.getX() + rnd.nextFloat() * r.getWidth();
            float y1 = r.getY() + rnd.nextFloat() * r.getHeight();
            float x2 = r.getX() + rnd.nextFloat() * r.getWidth();
            float y2 = r.getY() + rnd.nextFloat() * r.getHeight();
            
            branch.startNewSubPath(x1, y1);
            branch.cubicTo(x1 + (x2 - x1) * 0.3f + rnd.nextFloat() * 60.0f - 30.0f, y1 + (y2 - y1) * 0.7f - rnd.nextFloat() * 60.0f + 30.0f,
                           x1 + (x2 - x1) * 0.7f - rnd.nextFloat() * 60.0f + 30.0f, y1 + (y2 - y1) * 0.3f + rnd.nextFloat() * 60.0f - 30.0f,
                           x2, y2);

            g.strokePath(branch, juce::PathStrokeType(0.55f + rnd.nextFloat() * 0.7f));

            if (rnd.nextFloat() > 0.55f)
            {
                g.setColour(colour.brighter(0.2f).withAlpha(intensity * 0.32f));
                juce::Path fine;
                const float t = 0.3f + rnd.nextFloat() * 0.4f;
                const auto p = branch.getPointAlongPath(t * branch.getLength());
                fine.startNewSubPath(p);
                fine.quadraticTo(p.x + rnd.nextFloat() * 40.0f - 20.0f, p.y + rnd.nextFloat() * 40.0f - 20.0f,
                                 p.x + rnd.nextFloat() * 80.0f - 40.0f, p.y + rnd.nextFloat() * 80.0f - 40.0f);
                g.strokePath(fine, juce::PathStrokeType(0.3f + rnd.nextFloat() * 0.2f));
            }

            if (rnd.nextFloat() > 0.92f)
            {
                const auto p = branch.getPointAlongPath(rnd.nextFloat() * branch.getLength());
                const float glowSize = 1.5f + rnd.nextFloat() * 2.6f;
                g.setColour(colour.brighter(0.5f).withAlpha(intensity * 0.46f));
                g.fillEllipse(p.x - glowSize * 0.5f, p.y - glowSize * 0.5f, glowSize, glowSize);
            }
        }
    }

    static inline void drawOrbSocket(juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour tint, float alpha = 1.0f)
    {
        g.setColour(juce::Colours::black.withAlpha(0.16f * alpha));
        g.fillEllipse(juce::Rectangle<float>(radius * 2.8f, radius * 2.8f).withCentre(centre.translated(0.0f, 1.5f)));
        g.setColour(tint.withAlpha(0.22f * alpha));
        g.drawEllipse(juce::Rectangle<float>(radius * 2.2f, radius * 2.2f).withCentre(centre), 1.1f);

        g.setColour(tint.withAlpha(0.20f * alpha));
        g.fillEllipse(juce::Rectangle<float>(radius * 3.2f, radius * 3.2f).withCentre(centre));

        juce::ColourGradient orb(tint.brighter(0.4f).withAlpha(0.95f * alpha), centre.x, centre.y - radius * 0.7f,
                                 tint.darker(0.35f).withAlpha(0.95f * alpha), centre.x, centre.y + radius, true);
        g.setGradientFill(orb);
        g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre));

        g.setColour(juce::Colours::white.withAlpha(0.30f * alpha));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.72f, radius * 0.72f).withCentre({ centre.x - radius * 0.25f, centre.y - radius * 0.32f }));
    }

    static inline void drawTabLabel(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text)
    {
        drawGlassPanel(g, r, r.getHeight() * 0.5f, 0.88f);
        g.setColour(accentSoft.withAlpha(0.035f));
        g.fillRoundedRectangle(r.reduced(2.0f), r.getHeight() * 0.40f);

        auto textArea = r.toNearestInt().reduced(10, 0);
        g.setFont(getFont(r.getHeight() * 0.40f).boldened());
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.drawFittedText(text, textArea.translated(0, 1), juce::Justification::centred, 1);

        g.setColour(RootFlow::text.withAlpha(0.97f));
        g.drawFittedText(text, textArea, juce::Justification::centred, 1);
    }

    static inline void drawGlowOrb(juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, float alpha = 1.0f)
    {
        g.setColour(colour.withAlpha(0.18f * alpha));
        g.fillEllipse(juce::Rectangle<float>(radius * 2.8f, radius * 2.8f).withCentre(centre));

        juce::ColourGradient orb(colour.brighter(0.4f).withAlpha(0.95f * alpha), centre.x, centre.y - radius * 0.7f,
                                 colour.darker(0.35f).withAlpha(0.95f * alpha), centre.x, centre.y + radius, true);
        g.setGradientFill(orb);
        g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre));

        g.setColour(juce::Colours::white.withAlpha(0.30f * alpha));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.72f, radius * 0.72f).withCentre({ centre.x - radius * 0.25f, centre.y - radius * 0.32f }));
    }

    static inline void drawPanel(juce::Graphics& g, juce::Rectangle<float> r)
    {
        drawGlassPanel(g, r, 18.0f, 1.0f);
    }

}
