#include "RootFlowLookAndFeel.h"
#include "UI/Utils/DesignTokens.h"

namespace
{
struct SliderPalette
{
    juce::Colour primary;
    juce::Colour secondary;
};

SliderPalette getSliderPalette(const juce::String& name,
                               juce::Colour accent,
                               juce::Colour accentSoft) noexcept
{
    const auto upper = name.toUpperCase();
    const auto amber = juce::Colour(0xffffe5a8);
    const auto violet = juce::Colour(0xff7a6dff);

    if (upper.contains("ANCHOR") || upper.contains("SEASON"))
        return { violet, accent };

    if (upper.contains("GROWTH") || upper.contains("SUN") || upper.contains("SPACE") || upper.contains("CANOPY"))
        return { amber, accent };

    if (upper.contains("INSTABILITY"))
        return { amber.interpolatedWith(violet, 0.34f), violet };

    if (upper.contains("SOIL") || upper.contains("DEPTH") || upper.contains("VITALITY"))
        return { accentSoft, accent };

    if (upper.contains("AIR") || upper.contains("RAIN") || upper.contains("ATMOS") || upper.contains("FLOW") || upper.contains("RATE"))
        return { accent, accentSoft };

    if (upper.contains("VOLUME"))
        return { accent, accentSoft };

    if (upper.contains("MIX"))
        return { accentSoft, accent };

    if (upper.contains("FREQUENCY"))
        return { violet, accent };

    if (upper.contains("COMPRESSOR"))
        return { amber, accent };

    return { accent, accentSoft };
}
}

RootFlowLookAndFeel::RootFlowLookAndFeel()
{
    bg         = getBgColor();
    panel      = getPanelColor();
    accent     = getAccentColor();
    accentSoft = getAccentSoftColor();
    text       = getTextColor();

    // Global Slider Colors for internal JUCE logic
    setColour(juce::Slider::thumbColourId, accent);
    setColour(juce::Slider::rotarySliderFillColourId, accent);
    setColour(juce::Slider::rotarySliderOutlineColourId, bg.brighter(0.05f));
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

void RootFlowLookAndFeel::fillResizableWindowBackground(juce::Graphics& g,
                                                        int w, int h,
                                                        const juce::BorderSize<int>&,
                                                        juce::ResizableWindow&)
{
    g.fillAll(bg);

    juce::ColourGradient grad(bg.brighter(0.02f), w * 0.5f, 0,
                               bg.darker(0.18f), w * 0.5f, (float)h, false);
    g.setGradientFill(grad);
    g.fillRect(0, 0, w, h);
}

void RootFlowLookAndFeel::drawPanel(juce::Graphics& g, juce::Rectangle<float> r)
{
    RootFlow::drawPanel(g, r);
}

void RootFlowLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                           int x, int y, int width, int height,
                                           float sliderPos,
                                           float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(4.5f);
    const auto palette = getSliderPalette(slider.getName(), accent, accentSoft);
    const auto primary = palette.primary;
    const auto secondary = palette.secondary;
    const bool hot = RootFlow::isInteractiveHoverActive(slider);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float trackRadius = radius - 5.0f;

    // Deep Outer Shadow
    g.setColour(juce::Colours::black.withAlpha(0.42f));
    g.fillEllipse(juce::Rectangle<float>(radius * 2.38f, radius * 2.38f).withCentre(centre.translated(0.0f, 9.0f)));
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillEllipse(juce::Rectangle<float>(radius * 2.22f, radius * 2.22f).withCentre(centre.translated(0.0f, 4.0f)));
    
    // Housing / Socket
    g.setColour(panel.darker(0.60f).withAlpha(0.96f));
    g.fillEllipse(juce::Rectangle<float>(radius * 2.12f, radius * 2.12f).withCentre(centre));

    juce::ColourGradient housing(panel.brighter(0.22f), centre.x, centre.y - radius,
                                 bg.darker(0.45f), centre.x, centre.y + radius, false);
    g.setGradientFill(housing);
    g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre));

    // Top Rim Highlight (Plastisch)
    juce::ColourGradient topBulge(juce::Colours::white.withAlpha(0.14f), centre.x, centre.y - radius,
                                  juce::Colours::transparentBlack, centre.x, centre.y + radius * 0.15f, false);
    g.setGradientFill(topBulge);
    g.fillEllipse(juce::Rectangle<float>(radius * 1.94f, radius * 1.94f).withCentre(centre.translated(0.0f, -1.2f)));

    // Inner Recess Shadow
    juce::ColourGradient innerShade(juce::Colours::transparentBlack, centre.x, centre.y - radius * 0.15f,
                                    juce::Colours::black.withAlpha(0.28f), centre.x, centre.y + radius, false);
    g.setGradientFill(innerShade);
    g.fillEllipse(juce::Rectangle<float>(radius * 1.90f, radius * 1.90f).withCentre(centre.translated(0.0f, 0.8f)));

    // Fine detail rings for depth
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawEllipse(juce::Rectangle<float>(radius * 1.94f, radius * 1.94f).withCentre(centre.translated(0.0f, -0.6f)), 1.1f);
    g.setColour(primary.withAlpha(hot ? 0.28f : 0.16f));
    g.drawEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre), 1.6f);
    
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.drawEllipse(juce::Rectangle<float>(radius * 1.78f, radius * 1.78f).withCentre(centre), 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.26f));
    g.drawEllipse(juce::Rectangle<float>(radius * 1.60f, radius * 1.60f).withCentre(centre.translated(0.0f, 1.2f)), 0.9f);

    for (int i = 0; i <= 12; ++i)
    {
        const float tickAngle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * ((float) i / 12.0f);
        const auto p1 = juce::Point<float>(centre.x + std::cos(tickAngle - juce::MathConstants<float>::halfPi) * (trackRadius + 2.0f),
                                           centre.y + std::sin(tickAngle - juce::MathConstants<float>::halfPi) * (trackRadius + 2.0f));
        const auto p2 = juce::Point<float>(centre.x + std::cos(tickAngle - juce::MathConstants<float>::halfPi) * (trackRadius + 6.0f),
                                           centre.y + std::sin(tickAngle - juce::MathConstants<float>::halfPi) * (trackRadius + 6.0f));
        g.setColour((i % 3 == 0 ? primary : juce::Colours::white).withAlpha(i == 6 ? 0.18f : 0.09f));
        g.drawLine({ p1, p2 }, i == 6 ? 1.4f : 1.0f);
    }

    juce::Path fullArc;
    fullArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.strokePath(fullArc, juce::PathStrokeType(6.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.strokePath(fullArc, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(primary.withAlpha(hot ? 0.32f : 0.22f));
    g.strokePath(valueArc, juce::PathStrokeType(9.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(primary.withAlpha(0.92f));
    g.strokePath(valueArc, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.26f));
    g.strokePath(valueArc, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto coreBounds = juce::Rectangle<float>(radius * 1.48f, radius * 1.48f).withCentre(centre);
    
    // Core Shadow
    g.setColour(juce::Colours::black.withAlpha(0.36f));
    g.fillEllipse(coreBounds.expanded(8.0f).translated(0.0f, 6.0f));
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillEllipse(coreBounds.expanded(4.0f).translated(0.0f, 2.0f));

    g.setColour(panel.darker(0.45f).withAlpha(0.96f));
    g.fillEllipse(coreBounds.expanded(2.5f));
    
    juce::ColourGradient coreGrad(secondary.brighter(0.25f).withAlpha(0.96f), coreBounds.getCentreX(), coreBounds.getY(),
                                  panel.darker(0.18f).withAlpha(0.98f), coreBounds.getCentreX(), coreBounds.getBottom(), false);
    g.setGradientFill(coreGrad);
    g.fillEllipse(coreBounds);

    g.saveState();
    juce::Path coreClip;
    coreClip.addEllipse(coreBounds);
    g.reduceClipRegion(coreClip);
    juce::Random woodRnd (slider.getName().hashCode());
    g.setColour(juce::Colours::black.withAlpha(0.12f));
    for (int i = 0; i < 12; ++i)
    {
        const float gx = coreBounds.getX() + woodRnd.nextFloat() * coreBounds.getWidth();
        juce::Path grain;
        grain.startNewSubPath(gx, coreBounds.getY());
        grain.quadraticTo(gx + woodRnd.nextFloat() * 20.0f - 10.0f, coreBounds.getCentreY(),
                          gx + woodRnd.nextFloat() * 40.0f - 20.0f, coreBounds.getBottom());
        g.strokePath(grain, juce::PathStrokeType(0.5f + woodRnd.nextFloat() * 1.0f));
    }
    g.restoreState();

    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.fillEllipse(coreBounds.withHeight(coreBounds.getHeight() * 0.46f).translated(0.0f, -coreBounds.getHeight() * 0.08f));
    g.setColour(primary.withAlpha(0.24f));
    g.drawEllipse(coreBounds, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.drawEllipse(coreBounds.reduced(2.4f).translated(0.0f, 1.0f), 0.8f);

    const float gripInset = coreBounds.getWidth() * 0.18f;
    for (int i = -1; i <= 1; ++i)
    {
        const float gripX = coreBounds.getCentreX() + (float) i * coreBounds.getWidth() * 0.12f;
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.drawLine(gripX, coreBounds.getY() + gripInset + 2.0f, gripX, coreBounds.getBottom() - gripInset + 2.0f, 1.6f);
        g.setColour(juce::Colours::white.withAlpha(0.14f));
        g.drawLine(gripX, coreBounds.getY() + gripInset, gripX, coreBounds.getBottom() - gripInset, 0.9f);
    }

    juce::Path pointer;
    auto pointerRect = juce::Rectangle<float>(radius * 0.20f, radius * 0.98f).withCentre({ centre.x, centre.y - radius * 0.24f });
    pointer.addRoundedRectangle(pointerRect, pointerRect.getWidth() * 0.5f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
    
    // Deep physical shadow for the pointer
    g.setColour(juce::Colours::black.withAlpha(0.34f));
    g.fillPath(pointer, juce::AffineTransform::translation(0.0f, 3.5f));
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillPath(pointer, juce::AffineTransform::translation(0.0f, 1.8f));
    
    g.setColour(primary.withAlpha(0.96f));
    g.fillPath(pointer);
    
    // Pointer Detail
    g.setColour(juce::Colours::white.withAlpha(0.42f));
    g.fillPath(pointer, juce::AffineTransform::scale(0.50f, 0.68f, centre.x, centre.y));
    
    g.setColour(juce::Colours::black.withAlpha(0.14f));
    g.strokePath(pointer, juce::PathStrokeType(0.6f));

    const float orbRadius = 3.8f + sliderPos * 2.4f;
    const auto thumbPoint = juce::Point<float>(centre.x + std::cos(angle - juce::MathConstants<float>::halfPi) * trackRadius,
                                               centre.y + std::sin(angle - juce::MathConstants<float>::halfPi) * trackRadius);
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillEllipse(juce::Rectangle<float>(orbRadius * 3.2f, orbRadius * 3.2f).withCentre(thumbPoint.translated(0.0f, 2.4f)));
    g.setColour(primary.withAlpha(0.34f));
    g.fillEllipse(juce::Rectangle<float>(orbRadius * 3.0f, orbRadius * 3.0f).withCentre(thumbPoint));
    g.setColour(primary.withAlpha(0.96f));
    g.fillEllipse(juce::Rectangle<float>(orbRadius * 1.55f, orbRadius * 1.55f).withCentre(thumbPoint));
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.fillEllipse(juce::Rectangle<float>(orbRadius * 0.58f, orbRadius * 0.58f).withCentre(thumbPoint.translated(-0.6f, -0.7f)));

    auto jewel = juce::Rectangle<float>(radius * 0.34f, radius * 0.34f).withCentre(centre);
    g.setColour(primary.withAlpha(0.26f));
    g.fillEllipse(jewel.expanded(4.0f));
    juce::ColourGradient jewelGrad(juce::Colours::white.withAlpha(0.86f), jewel.getCentreX(), jewel.getY(),
                                   primary.withAlpha(0.95f), jewel.getCentreX(), jewel.getBottom(), false);
    g.setGradientFill(jewelGrad);
    g.fillEllipse(jewel);
}

void RootFlowLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                           int x, int y, int width, int height,
                                           float sliderPos, float, float,
                                           const juce::Slider::SliderStyle style,
                                           juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(4.0f);
    const bool vertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;
    const auto palette = getSliderPalette(slider.getName(), accent, accentSoft);
    const auto primary = palette.primary;
    const auto secondary = palette.secondary;
    const bool hot = RootFlow::isInteractiveHoverActive(slider);
    const auto styleTag = slider.getProperties().getWithDefault("rootflowStyle", juce::var()).toString();
    const bool sideVertical = vertical && styleTag == "side-vertical";
    const bool podHorizontal = ! vertical && styleTag == "pod-horizontal";
    const bool ambientSlider = ! vertical && styleTag == "ambient-horizontal";

    if (vertical)
    {
        const float minWidth = sideVertical ? 9.0f : 20.0f;
        auto housing = juce::Rectangle<float>(juce::jmax(minWidth, bounds.getWidth() * (sideVertical ? 0.50f : 0.44f)),
                                              bounds.getHeight() - (sideVertical ? 2.0f : 6.0f))
                           .withCentre(bounds.getCentre());
        const float trackWidthFactor = sideVertical ? 0.26f : 0.48f;
        auto track = juce::Rectangle<float>(juce::jmax(4.0f, housing.getWidth() * trackWidthFactor),
                                            housing.getHeight() - (sideVertical ? 10.0f : 16.0f))
                         .withCentre(housing.getCentre());

        // Deep Housing Shadow
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        g.fillRoundedRectangle(housing.expanded(sideVertical ? 2.2f : 9.0f, sideVertical ? 5.2f : 7.6f).translated(0.0f, sideVertical ? 4.2f : 5.8f),
                               housing.getWidth() * 0.60f);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRoundedRectangle(housing.translated(0.0f, sideVertical ? 2.4f : 3.2f),
                               housing.getWidth() * 0.58f);
        
        g.setColour(panel.darker(0.50f).withAlpha(0.96f));
        g.fillRoundedRectangle(housing.expanded(sideVertical ? 0.6f : 1.5f, sideVertical ? 0.8f : 1.2f),
                               housing.getWidth() * 0.60f);

        juce::ColourGradient shellGrad(panel.brighter(0.20f), housing.getCentreX(), housing.getY(),
                                       bg.darker(0.22f), housing.getCentreX(), housing.getBottom(), false);
        g.setGradientFill(shellGrad);
        g.fillRoundedRectangle(housing, housing.getWidth() * 0.58f);

        juce::ColourGradient topBulge(juce::Colours::white.withAlpha(0.12f), housing.getCentreX(), housing.getY(),
                                      juce::Colours::transparentBlack, housing.getCentreX(), housing.getY() + housing.getHeight() * 0.38f, false);
        g.setGradientFill(topBulge);
        g.fillRoundedRectangle(housing.reduced(1.2f), housing.getWidth() * 0.54f);

        juce::ColourGradient lowerOcclusion(juce::Colours::transparentBlack, housing.getCentreX(), housing.getY() + housing.getHeight() * 0.44f,
                                            juce::Colours::black.withAlpha(0.22f), housing.getCentreX(), housing.getBottom(), false);
        g.setGradientFill(lowerOcclusion);
        g.fillRoundedRectangle(housing.reduced(1.4f), housing.getWidth() * 0.52f);

        g.setColour(primary.withAlpha(hot ? 0.22f : 0.12f));
        g.drawRoundedRectangle(housing, housing.getWidth() * 0.58f, 1.4f);
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawRoundedRectangle(housing.reduced(1.2f), housing.getWidth() * 0.52f, 0.9f);

        for (int i = 0; i <= 5; ++i)
        {
            const float t = (float) i / 5.0f;
            const float yy = juce::jmap(t, track.getBottom(), track.getY());
            g.setColour((i == 0 || i == 5 ? primary : juce::Colours::white).withAlpha(i == 0 || i == 5 ? 0.16f : 0.08f));
            g.drawLine(housing.getX() + (sideVertical ? 3.0f : 4.0f), yy,
                       housing.getRight() - (sideVertical ? 3.0f : 4.0f), yy,
                       i == 0 || i == 5 ? 1.1f : 0.8f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.fillRoundedRectangle(track.expanded(1.0f, 1.6f), track.getWidth() * 0.56f);
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.fillRoundedRectangle(track, track.getWidth() * 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(track, track.getWidth() * 0.5f, 0.8f);

        auto fill = juce::Rectangle<float>(track.getX() + 1.2f,
                                           juce::jlimit(track.getY(), track.getBottom(), sliderPos),
                                           track.getWidth() - 2.4f,
                                           track.getBottom() - juce::jlimit(track.getY(), track.getBottom(), sliderPos));

        if (! fill.isEmpty())
        {
            juce::ColourGradient active(primary.withAlpha(0.98f), fill.getCentreX(), fill.getY(),
                                        secondary.withAlpha(0.92f), fill.getCentreX(), fill.getBottom(), false);
            g.setGradientFill(active);
            g.fillRoundedRectangle(fill, fill.getWidth() * 0.48f);
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.fillRoundedRectangle(fill.withWidth(fill.getWidth() * 0.26f).withCentre(fill.getCentre().withX(fill.getCentreX() - fill.getWidth() * 0.16f)),
                                   fill.getWidth() * 0.36f);
        }

        g.setColour(primary.withAlpha(0.18f));
        g.fillEllipse(juce::Rectangle<float>(housing.getWidth() * (sideVertical ? 0.56f : 0.72f),
                                             housing.getWidth() * (sideVertical ? 0.56f : 0.72f))
                          .withCentre({ housing.getCentreX(), track.getBottom() + (sideVertical ? 4.0f : 5.0f) }));

        auto knobBounds = juce::Rectangle<float>(housing.getWidth() * (sideVertical ? 1.02f : 1.36f),
                                                 housing.getWidth() * (sideVertical ? 0.62f : 0.98f))
                              .withCentre({ housing.getCentreX(), juce::jlimit(track.getY(), track.getBottom(), sliderPos) });
        
        // Knob Shadow (Physical)
        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.fillRoundedRectangle(knobBounds.expanded(sideVertical ? 3.0f : 6.4f, sideVertical ? 3.8f : 4.8f).translated(0.0f, 3.5f),
                               knobBounds.getHeight() * 0.60f);
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        g.fillRoundedRectangle(knobBounds.expanded(sideVertical ? 1.5f : 3.2f, sideVertical ? 1.8f : 2.4f).translated(0.0f, 1.8f),
                               knobBounds.getHeight() * 0.56f);

        g.setColour(primary.withAlpha(hot ? 0.38f : 0.28f));
        g.fillRoundedRectangle(knobBounds.expanded(sideVertical ? 2.2f : 5.4f, sideVertical ? 1.6f : 3.4f),
                               knobBounds.getHeight() * 0.56f);

        juce::ColourGradient knobGrad(secondary.brighter(0.20f), knobBounds.getCentreX(), knobBounds.getY(),
                                      primary.darker(0.18f), knobBounds.getCentreX(), knobBounds.getBottom(), false);
        g.setGradientFill(knobGrad);
        g.fillRoundedRectangle(knobBounds, knobBounds.getHeight() * 0.54f);
        
        g.setColour(juce::Colours::white.withAlpha(0.56f));
        g.drawRoundedRectangle(knobBounds.reduced(0.8f), knobBounds.getHeight() * 0.48f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.32f));
        g.fillRoundedRectangle(knobBounds.withHeight(knobBounds.getHeight() * 0.38f).translated(0.0f, -knobBounds.getHeight() * 0.08f),
                               knobBounds.getHeight() * 0.36f);
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.drawRoundedRectangle(knobBounds.reduced(1.8f).translated(0.0f, 1.2f), knobBounds.getHeight() * 0.44f, 0.9f);

        // Tactile Grip Ridges (Increased density and depth with hover glow)
        for (int i = -2; i <= 2; ++i)
        {
            const float ridgeY = knobBounds.getCentreY() + (float) i * knobBounds.getHeight() * 0.14f;
            g.setColour(juce::Colours::black.withAlpha(0.28f));
            g.drawLine(knobBounds.getX() + knobBounds.getWidth() * 0.18f, ridgeY + 1.2f,
                       knobBounds.getRight() - knobBounds.getWidth() * 0.18f, ridgeY + 1.2f, 1.4f);
            
            const float alpha = hot ? 0.32f : 0.18f;
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawLine(knobBounds.getX() + knobBounds.getWidth() * 0.18f, ridgeY,
                       knobBounds.getRight() - knobBounds.getWidth() * 0.18f, ridgeY, hot ? 1.1f : 0.9f);
        }
    }
    else if (podHorizontal)
    {
        auto housing = juce::Rectangle<float>(bounds.getWidth() - 2.0f, juce::jmax(15.0f, bounds.getHeight() * 0.40f)).withCentre(bounds.getCentre());
        auto track = juce::Rectangle<float>(housing.getWidth() - 12.0f, juce::jmax(4.5f, housing.getHeight() * 0.16f)).withCentre(housing.getCentre());
        const float thumbX = juce::jlimit(track.getX(), track.getRight(), sliderPos);

        g.setColour(juce::Colours::black.withAlpha(0.38f));
        g.fillRoundedRectangle(housing.expanded(5.4f, 4.8f).translated(0.0f, 3.8f), housing.getHeight() * 0.58f);
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        g.fillRoundedRectangle(housing.translated(0.0f, 2.2f), housing.getHeight() * 0.56f);
        g.setColour(panel.darker(0.48f).withAlpha(0.96f));
        g.fillRoundedRectangle(housing.expanded(1.0f, 0.8f), housing.getHeight() * 0.58f);

        juce::ColourGradient shellGrad(panel.brighter(0.18f), housing.getX(), housing.getCentreY(),
                                       bg.darker(0.16f), housing.getRight(), housing.getCentreY(), false);
        g.setGradientFill(shellGrad);
        g.fillRoundedRectangle(housing, housing.getHeight() * 0.56f);

        juce::ColourGradient topBulge(juce::Colours::white.withAlpha(0.10f), housing.getCentreX(), housing.getY(),
                                      juce::Colours::transparentBlack, housing.getCentreX(), housing.getY() + housing.getHeight() * 0.52f, false);
        g.setGradientFill(topBulge);
        g.fillRoundedRectangle(housing.reduced(1.2f), housing.getHeight() * 0.52f);
        juce::ColourGradient lowerOcclusion(juce::Colours::transparentBlack, housing.getCentreX(), housing.getY() + housing.getHeight() * 0.44f,
                                            juce::Colours::black.withAlpha(0.18f), housing.getCentreX(), housing.getBottom(), false);
        g.setGradientFill(lowerOcclusion);
        g.fillRoundedRectangle(housing.reduced(1.2f), housing.getHeight() * 0.50f);

        g.setColour(primary.withAlpha(hot ? 0.20f : 0.12f));
        g.drawRoundedRectangle(housing, housing.getHeight() * 0.56f, 1.2f);
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.drawRoundedRectangle(housing.reduced(1.2f), housing.getHeight() * 0.50f, 0.8f);

        for (int i = 0; i <= 6; ++i)
        {
            const float t = (float) i / 6.0f;
            const float xx = juce::jmap(t, track.getX(), track.getRight());
            g.setColour((i == 0 || i == 6 ? primary : juce::Colours::white).withAlpha(i == 3 ? 0.12f : 0.06f));
            g.drawLine(xx, housing.getY() + 3.5f, xx, housing.getBottom() - 3.5f, i == 3 ? 1.0f : 0.7f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.fillRoundedRectangle(track.expanded(1.2f, 1.4f), track.getHeight() * 0.60f);
        g.setColour(juce::Colours::black.withAlpha(0.24f));
        g.fillRoundedRectangle(track, track.getHeight() * 0.55f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(track, track.getHeight() * 0.55f, 0.7f);

        auto fill = juce::Rectangle<float>(track.getX(),
                                           track.getY() + 1.0f,
                                           juce::jmax(0.0f, thumbX - track.getX()),
                                           track.getHeight() - 2.0f);
        if (! fill.isEmpty())
        {
            juce::ColourGradient active(primary.withAlpha(0.94f), fill.getX(), fill.getCentreY(),
                                        secondary.withAlpha(0.88f), fill.getRight(), fill.getCentreY(), false);
            g.setGradientFill(active);
            g.fillRoundedRectangle(fill, fill.getHeight() * 0.50f);
            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.fillRoundedRectangle(fill.withHeight(fill.getHeight() * 0.36f).translated(0.0f, -fill.getHeight() * 0.10f),
                                   fill.getHeight() * 0.20f);
        }

        auto thumb = juce::Rectangle<float>(juce::jmax(11.0f, housing.getHeight() * 0.54f), housing.getHeight() - 4.0f)
                         .withCentre({ thumbX, housing.getCentreY() });
        g.setColour(juce::Colours::black.withAlpha(0.24f));
        g.fillRoundedRectangle(thumb.expanded(4.0f, 3.0f).translated(0.0f, 2.0f), thumb.getWidth() * 0.50f);
        g.setColour(primary.withAlpha(hot ? 0.24f : 0.18f));
        g.fillRoundedRectangle(thumb.expanded(3.0f, 2.0f), thumb.getWidth() * 0.48f);

        juce::ColourGradient thumbGrad(secondary.brighter(0.12f), thumb.getCentreX(), thumb.getY(),
                                       primary.darker(0.10f), thumb.getCentreX(), thumb.getBottom(), false);
        g.setGradientFill(thumbGrad);
        g.fillRoundedRectangle(thumb, thumb.getWidth() * 0.44f);
        g.setColour(juce::Colours::white.withAlpha(0.42f));
        g.drawRoundedRectangle(thumb.reduced(0.8f), thumb.getWidth() * 0.40f, 0.8f);
        g.setColour(juce::Colours::white.withAlpha(0.24f));
        g.drawLine(thumb.getCentreX(), thumb.getY() + 3.0f, thumb.getCentreX(), thumb.getBottom() - 3.0f, 0.9f);
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        g.drawRoundedRectangle(thumb.reduced(1.5f).translated(0.0f, 1.0f), thumb.getWidth() * 0.36f, 0.7f);

        // Tactile Grip Ridges (Horizontal density with hover glow)
        for (int i = -2; i <= 2; ++i)
        {
            const float ridgeX = thumb.getCentreX() + (float) i * thumb.getWidth() * 0.14f;
            g.setColour(juce::Colours::black.withAlpha(0.28f));
            g.drawLine(ridgeX + 0.8f, thumb.getY() + thumb.getHeight() * 0.18f + 1.0f,
                       ridgeX + 0.8f, thumb.getBottom() - thumb.getHeight() * 0.18f + 1.0f, 1.4f);
            
            const float alpha = hot ? 0.35f : 0.18f;
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawLine(ridgeX, thumb.getY() + thumb.getHeight() * 0.18f,
                       ridgeX, thumb.getBottom() - thumb.getHeight() * 0.18f, hot ? 1.1f : 0.9f);
        }
    }
    else if (ambientSlider)
    {
        auto housing = juce::Rectangle<float>(bounds.getWidth() - 2.0f, juce::jmax(18.0f, bounds.getHeight() * 0.76f)).withCentre(bounds.getCentre());
        auto track = housing.reduced(16.0f, juce::jmax(4.0f, housing.getHeight() * 0.34f));
        const float thumbX = juce::jlimit(track.getX(), track.getRight(), sliderPos);

        g.setColour(juce::Colours::black.withAlpha(0.42f));
        g.fillRoundedRectangle(housing.expanded(6.5f, 4.8f).translated(0.0f, 4.0f), housing.getHeight() * 0.58f);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRoundedRectangle(housing.translated(0.0f, 2.2f), housing.getHeight() * 0.56f);
        g.setColour(panel.darker(0.52f).withAlpha(0.96f));
        g.fillRoundedRectangle(housing.expanded(1.0f, 0.8f), housing.getHeight() * 0.58f);

        juce::ColourGradient shellGrad(panel.brighter(0.16f), housing.getX(), housing.getCentreY(),
                                       bg.darker(0.20f), housing.getRight(), housing.getCentreY(), false);
        g.setGradientFill(shellGrad);
        g.fillRoundedRectangle(housing, housing.getHeight() * 0.56f);

        juce::ColourGradient topBulge(juce::Colours::white.withAlpha(0.12f), housing.getCentreX(), housing.getY(),
                                      juce::Colours::transparentBlack, housing.getCentreX(), housing.getY() + housing.getHeight() * 0.54f, false);
        g.setGradientFill(topBulge);
        g.fillRoundedRectangle(housing.reduced(1.2f), housing.getHeight() * 0.52f);
        juce::ColourGradient lowerOcclusion(juce::Colours::transparentBlack, housing.getCentreX(), housing.getY() + housing.getHeight() * 0.44f,
                                            juce::Colours::black.withAlpha(0.20f), housing.getCentreX(), housing.getBottom(), false);
        g.setGradientFill(lowerOcclusion);
        g.fillRoundedRectangle(housing.reduced(1.2f), housing.getHeight() * 0.50f);

        g.setColour(primary.withAlpha(hot ? 0.22f : 0.12f));
        g.drawRoundedRectangle(housing, housing.getHeight() * 0.56f, 1.3f);
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.drawRoundedRectangle(housing.reduced(1.2f), housing.getHeight() * 0.50f, 0.9f);

        for (int i = 0; i <= 4; ++i)
        {
            const float t = (float) i / 4.0f;
            const float xx = juce::jmap(t, track.getX(), track.getRight());
            g.setColour((i == 0 || i == 4 ? primary : juce::Colours::white).withAlpha(i == 2 ? 0.14f : 0.07f));
            g.drawLine(xx, housing.getY() + 5.0f, xx, housing.getBottom() - 5.0f, i == 2 ? 1.1f : 0.8f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.34f));
        g.fillRoundedRectangle(track.expanded(1.2f, 1.6f), track.getHeight() * 0.54f);
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.fillRoundedRectangle(track, track.getHeight() * 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(track, track.getHeight() * 0.5f, 0.7f);

        auto fill = juce::Rectangle<float>(track.getX(),
                                           track.getY() + 1.0f,
                                           juce::jmax(0.0f, thumbX - track.getX()),
                                           track.getHeight() - 2.0f);
        if (! fill.isEmpty())
        {
            juce::ColourGradient active(primary.withAlpha(0.94f), fill.getX(), fill.getCentreY(),
                                        secondary.withAlpha(0.88f), fill.getRight(), fill.getCentreY(), false);
            g.setGradientFill(active);
            g.fillRoundedRectangle(fill, fill.getHeight() * 0.48f);
            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.fillRoundedRectangle(fill.withHeight(fill.getHeight() * 0.36f).translated(0.0f, -fill.getHeight() * 0.10f),
                                   fill.getHeight() * 0.20f);
        }

        auto thumb = juce::Rectangle<float>(juce::jmax(14.0f, housing.getHeight() * 0.30f), housing.getHeight() - 6.0f)
                         .withCentre({ thumbX, housing.getCentreY() });
        g.setColour(juce::Colours::black.withAlpha(0.24f));
        g.fillRoundedRectangle(thumb.expanded(5.0f, 3.0f).translated(0.0f, 2.0f), thumb.getWidth() * 0.50f);
        g.setColour(primary.withAlpha(hot ? 0.24f : 0.18f));
        g.fillRoundedRectangle(thumb.expanded(4.0f, 2.0f), thumb.getWidth() * 0.48f);

        juce::ColourGradient thumbGrad(secondary.brighter(0.12f), thumb.getCentreX(), thumb.getY(),
                                       primary.darker(0.10f), thumb.getCentreX(), thumb.getBottom(), false);
        g.setGradientFill(thumbGrad);
        g.fillRoundedRectangle(thumb, thumb.getWidth() * 0.44f);
        g.setColour(juce::Colours::white.withAlpha(0.42f));
        g.drawRoundedRectangle(thumb.reduced(0.8f), thumb.getWidth() * 0.40f, 0.8f);
        g.setColour(juce::Colours::white.withAlpha(0.24f));
        g.drawLine(thumb.getCentreX(), thumb.getY() + 4.0f, thumb.getCentreX(), thumb.getBottom() - 4.0f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        g.drawRoundedRectangle(thumb.reduced(1.6f).translated(0.0f, 1.0f), thumb.getWidth() * 0.36f, 0.7f);

        // Tactile Grip Ridges (Ambient style with hover glow)
        for (int i = -2; i <= 2; ++i)
        {
            const float ridgeX = thumb.getCentreX() + (float) i * thumb.getWidth() * 0.14f;
            g.setColour(juce::Colours::black.withAlpha(0.32f));
            g.drawLine(ridgeX + 0.8f, thumb.getY() + thumb.getHeight() * 0.20f + 1.2f,
                       ridgeX + 0.8f, thumb.getBottom() - thumb.getHeight() * 0.20f + 1.2f, 1.5f);
            
            const float alpha = hot ? 0.40f : 0.20f;
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawLine(ridgeX, thumb.getY() + thumb.getHeight() * 0.20f,
                       ridgeX, thumb.getBottom() - thumb.getHeight() * 0.20f, hot ? 1.2f : 1.0f);
        }
    }
    else
    {
        auto housing = juce::Rectangle<float>(bounds.getWidth() - 6.0f, juce::jmax(18.0f, bounds.getHeight() * 0.54f)).withCentre(bounds.getCentre());
        auto track = juce::Rectangle<float>(housing.getWidth() - 18.0f, juce::jmax(7.0f, housing.getHeight() * 0.18f)).withCentre(housing.getCentre());

        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.fillRoundedRectangle(housing.expanded(5.0f, 5.6f).translated(0.0f, 3.4f), housing.getHeight() * 0.62f);
        g.setColour(juce::Colours::black.withAlpha(0.14f));
        g.fillRoundedRectangle(housing.translated(0.0f, 1.8f), housing.getHeight() * 0.60f);
        g.setColour(panel.darker(0.42f).withAlpha(0.94f));
        g.fillRoundedRectangle(housing.expanded(0.8f, 0.8f), housing.getHeight() * 0.62f);

        juce::ColourGradient shellGrad(panel.brighter(0.14f), housing.getX(), housing.getCentreY(),
                                       bg.darker(0.14f), housing.getRight(), housing.getCentreY(), false);
        g.setGradientFill(shellGrad);
        g.fillRoundedRectangle(housing, housing.getHeight() * 0.60f);

        juce::ColourGradient topBulge(juce::Colours::white.withAlpha(0.09f), housing.getCentreX(), housing.getY(),
                                      juce::Colours::transparentBlack, housing.getCentreX(), housing.getY() + housing.getHeight() * 0.54f, false);
        g.setGradientFill(topBulge);
        g.fillRoundedRectangle(housing.reduced(1.0f), housing.getHeight() * 0.54f);
        juce::ColourGradient lowerOcclusion(juce::Colours::transparentBlack, housing.getCentreX(), housing.getY() + housing.getHeight() * 0.44f,
                                            juce::Colours::black.withAlpha(0.18f), housing.getCentreX(), housing.getBottom(), false);
        g.setGradientFill(lowerOcclusion);
        g.fillRoundedRectangle(housing.reduced(1.0f), housing.getHeight() * 0.52f);

        g.setColour(primary.withAlpha(hot ? 0.18f : 0.10f));
        g.drawRoundedRectangle(housing, housing.getHeight() * 0.60f, 1.2f);
        g.setColour(juce::Colours::white.withAlpha(0.14f));
        g.drawRoundedRectangle(housing.reduced(1.0f), housing.getHeight() * 0.52f, 0.8f);

        for (int i = 0; i <= 6; ++i)
        {
            const float t = (float) i / 6.0f;
            const float xx = juce::jmap(t, track.getX(), track.getRight());
            g.setColour((i == 0 || i == 6 ? primary : juce::Colours::white).withAlpha(i == 0 || i == 6 ? 0.16f : 0.08f));
            g.drawLine(xx, housing.getY() + 4.0f, xx, housing.getBottom() - 4.0f, i == 3 ? 1.2f : 0.8f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.fillRoundedRectangle(track, track.getHeight() * 0.55f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(track, track.getHeight() * 0.55f, 0.7f);

        auto fill = juce::Rectangle<float>(track.getX(),
                                           track.getY() + 1.0f,
                                           juce::jmax(0.0f, juce::jlimit(track.getX(), track.getRight(), sliderPos) - track.getX()),
                                           track.getHeight() - 2.0f);
        if (! fill.isEmpty())
        {
            juce::ColourGradient active(primary.withAlpha(0.98f), fill.getX(), fill.getCentreY(),
                                        secondary.withAlpha(0.92f), fill.getRight(), fill.getCentreY(), false);
            g.setGradientFill(active);
            g.fillRoundedRectangle(fill, fill.getHeight() * 0.50f);
            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.fillRoundedRectangle(fill.withHeight(fill.getHeight() * 0.40f).translated(0.0f, -fill.getHeight() * 0.12f),
                                   fill.getHeight() * 0.24f);
        }

        g.setColour(primary.withAlpha(0.20f));
        g.fillEllipse(juce::Rectangle<float>(housing.getHeight() * 0.74f, housing.getHeight() * 0.74f).withCentre({ track.getX(), housing.getCentreY() }));
        g.fillEllipse(juce::Rectangle<float>(housing.getHeight() * 0.74f, housing.getHeight() * 0.74f).withCentre({ track.getRight(), housing.getCentreY() }));

        auto knobBounds = juce::Rectangle<float>(housing.getHeight() * 0.96f, housing.getHeight() * 0.96f)
                              .withCentre({ juce::jlimit(track.getX(), track.getRight(), sliderPos), housing.getCentreY() });
        g.setColour(juce::Colours::black.withAlpha(0.26f));
        g.fillEllipse(knobBounds.expanded(5.0f).translated(0.0f, 3.0f));
        g.setColour(primary.withAlpha(hot ? 0.32f : 0.24f));
        g.fillEllipse(knobBounds.expanded(4.0f));

        juce::ColourGradient knobGrad(secondary.brighter(0.20f), knobBounds.getCentreX(), knobBounds.getY(),
                                      primary.darker(0.10f), knobBounds.getCentreX(), knobBounds.getBottom(), false);
        g.setGradientFill(knobGrad);
        g.fillEllipse(knobBounds);
        g.setColour(juce::Colours::white.withAlpha(0.50f));
        g.drawEllipse(knobBounds.reduced(0.8f), 0.9f);
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.fillEllipse(knobBounds.withSizeKeepingCentre(knobBounds.getWidth() * 0.28f, knobBounds.getHeight() * 0.28f)
                                 .translated(-knobBounds.getWidth() * 0.12f, -knobBounds.getHeight() * 0.14f));
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.drawEllipse(knobBounds.reduced(2.0f).translated(0.0f, 1.0f), 0.8f);

        // Tactile Grip Ridges (Standard style with hover glow)
        for (int i = -2; i <= 2; ++i)
        {
            const float ridgeX = knobBounds.getCentreX() + (float) i * knobBounds.getWidth() * 0.12f;
            g.setColour(juce::Colours::black.withAlpha(0.28f));
            g.drawLine(ridgeX + 0.8f, knobBounds.getY() + knobBounds.getHeight() * 0.22f + 1.2f,
                       ridgeX + 0.8f, knobBounds.getBottom() - knobBounds.getHeight() * 0.22f + 1.2f, 1.4f);
            
            const float alpha = hot ? 0.38f : 0.20f;
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawLine(ridgeX, knobBounds.getY() + knobBounds.getHeight() * 0.22f,
                       ridgeX, knobBounds.getBottom() - knobBounds.getHeight() * 0.22f, hot ? 1.0f : 0.8f);
        }
    }
}

void RootFlowLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                               juce::Button& b,
                                               const juce::Colour&,
                                               bool isHovered,
                                               bool isPressed)
{
    auto r = b.getLocalBounds().toFloat().reduced(2.0f);
    const bool hoverVisual = RootFlow::areHoverEffectsEnabled() && isHovered;
    const auto style = b.getProperties()["rootflowStyle"].toString();
    
    // Check if it's an organic "spore" button (buttons without text/icons usually)
    if (b.getButtonText().isEmpty())
    {
        juce::Path p;
        p.addEllipse(r);

        float alpha = isPressed ? 0.80f : (hoverVisual ? 0.55f : 0.32f);
        
        // Biological Spore Glow
        juce::ColourGradient grad(accent.withAlpha(alpha), r.getCentreX(), r.getCentreY(),
                                  accent.withAlpha(0.0f), r.getCentreX(), r.getBottom(), true);
        g.setGradientFill(grad);
        g.fillPath(p);

        // Vibrant Core Outline
        g.setColour(accent.withAlpha(alpha * 0.58f));
        g.strokePath(p, juce::PathStrokeType(1.6f));
        
        // Internal Spark
        if (hoverVisual || isPressed)
        {
            g.setColour(juce::Colours::white.withAlpha(isPressed ? 0.46f : 0.24f));
            g.fillEllipse(r.getCentreX() - 2.0f, r.getCentreY() - 2.0f, 4.0f, 4.0f);
        }
    }
    else
    {
        const bool toggled = b.getToggleState();
        if (style == "header-flat")
        {
            auto base = RootFlow::panelSoft.interpolatedWith(RootFlow::panel, 0.42f).withAlpha(toggled ? 0.86f : 0.76f);
            if (hoverVisual) base = base.brighter(0.08f);
            if (isPressed) base = accent.withAlpha(0.12f);

            g.setColour(juce::Colours::black.withAlpha(0.14f));
            g.fillRoundedRectangle(r.translated(0.0f, 1.6f), 8.5f);

            juce::ColourGradient btnGrad(base.brighter(toggled ? 0.16f : 0.08f), r.getCentreX(), r.getY(),
                                         base.darker(0.16f), r.getCentreX(), r.getBottom(), false);
            g.setGradientFill(btnGrad);
            g.fillRoundedRectangle(r, 8.0f);

            g.setColour((toggled ? accent : accentSoft).withAlpha(toggled ? 0.18f : (hoverVisual ? 0.12f : 0.07f)));
            g.drawRoundedRectangle(r, 8.0f, toggled ? 1.1f : 0.9f);

            g.setColour(juce::Colours::white.withAlpha(hoverVisual ? 0.05f : 0.03f));
            g.fillRoundedRectangle(r.withHeight(r.getHeight() * 0.42f).reduced(1.0f, 0.0f), 7.0f);
            return;
        }

        if (style == "sequencer-flat")
        {
            auto base = juce::Colour(0xff10181b).withAlpha(toggled ? 0.80f : 0.70f);
            if (hoverVisual) base = base.brighter(0.05f);
            if (isPressed) base = base.brighter(0.08f);

            g.setColour(juce::Colours::black.withAlpha(0.10f));
            g.fillRoundedRectangle(r.translated(0.0f, 1.0f), 8.0f);

            juce::ColourGradient btnGrad(base.brighter(toggled ? 0.10f : 0.04f), r.getCentreX(), r.getY(),
                                         base.darker(0.12f), r.getCentreX(), r.getBottom(), false);
            g.setGradientFill(btnGrad);
            g.fillRoundedRectangle(r, 7.5f);

            g.setColour((toggled ? accent : RootFlow::textMuted).withAlpha(toggled ? 0.10f : (hoverVisual ? 0.07f : 0.04f)));
            g.drawRoundedRectangle(r, 7.5f, toggled ? 1.0f : 0.8f);

            g.setColour((toggled ? accent : juce::Colours::white).withAlpha(toggled ? 0.03f : 0.02f));
            g.fillRoundedRectangle(r.reduced(1.0f).withHeight(r.getHeight() * 0.40f), 6.5f);
            return;
        }

        auto base = juce::Colour(0xff0d1815).withAlpha(toggled ? 0.88f : 0.80f);
        if (hoverVisual) base = base.brighter(0.12f);
        if (isPressed) base = accent.withAlpha(0.14f);
        auto bezel = r.expanded(2.8f, 2.4f).translated(0.0f, 1.2f);

        // Deep Bezel Shadow
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.fillRoundedRectangle(bezel.translated(0.0f, 3.5f), 12.0f);

        juce::ColourGradient bezelGrad(RootFlow::panelSoft.brighter(0.10f).withAlpha(0.38f), bezel.getCentreX(), bezel.getY(),
                                       bg.darker(0.32f).withAlpha(0.56f), bezel.getCentreX(), bezel.getBottom(), false);
        g.setGradientFill(bezelGrad);
        g.fillRoundedRectangle(bezel, 12.0f);
        g.setColour(juce::Colours::black.withAlpha(0.12f));
        g.drawRoundedRectangle(bezel.reduced(0.8f), 11.2f, 1.0f);

        // Button Depression
        g.setColour(juce::Colours::black.withAlpha(toggled ? 0.20f : 0.16f));
        g.fillRoundedRectangle(r.translated(0.0f, 3.8f).expanded(2.0f, 0.8f), 11.0f);
        g.setColour(juce::Colours::black.withAlpha(0.08f));
        g.fillRoundedRectangle(r.translated(0.0f, 1.8f), 10.5f);

        juce::ColourGradient btnGrad(base.brighter(toggled ? 0.28f : 0.14f), r.getCentreX(), r.getY(),
                                     base.darker(0.24f), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(btnGrad);
        g.fillRoundedRectangle(r, 10.0f);

        juce::ColourGradient topBulge(juce::Colours::white.withAlpha(toggled ? 0.08f : 0.05f),
                                      r.getCentreX(), r.getY(),
                                      juce::Colours::transparentBlack,
                                      r.getCentreX(), r.getY() + r.getHeight() * 0.48f, false);
        g.setGradientFill(topBulge);
        g.fillRoundedRectangle(r.reduced(1.0f), 8.8f);

        juce::ColourGradient lowerOcclusion(juce::Colours::transparentBlack,
                                            r.getCentreX(), r.getY() + r.getHeight() * 0.44f,
                                            juce::Colours::black.withAlpha(0.12f),
                                            r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(lowerOcclusion);
        g.fillRoundedRectangle(r.reduced(1.2f), 8.6f);

        g.setColour((toggled ? accent : accentSoft).withAlpha(toggled ? 0.22f : (hoverVisual ? 0.18f : 0.10f)));
        g.drawRoundedRectangle(r, 9.5f, toggled ? 1.3f : 1.0f);

        g.setColour(juce::Colours::white.withAlpha(toggled ? 0.08f : 0.05f));
        g.drawRoundedRectangle(r.reduced(1.0f), 8.4f, 0.8f);

        if (toggled)
        {
            g.setColour(accent.withAlpha(0.08f));
            g.fillRoundedRectangle(r.expanded(1.8f), 11.0f);
        }
    }
}

void RootFlowLookAndFeel::drawButtonText(juce::Graphics& g,
                                         juce::TextButton& button,
                                         bool,
                                         bool)
{
    const auto style = button.getProperties()["rootflowStyle"].toString();
    const bool flatHeaderButton = style == "header-flat";
    const bool flatSequencerButton = style == "sequencer-flat";
    const float fontHeight = flatHeaderButton ? 10.2f
                           : (flatSequencerButton ? 9.2f
                              : (button.getHeight() < 34 || button.getWidth() < 68 ? 11.0f : 12.0f));
    const float kerning = flatHeaderButton ? 0.10f
                         : (flatSequencerButton ? 0.08f
                            : (button.getWidth() < 72 ? 0.12f : 0.18f));
    g.setFont(juce::Font(juce::FontOptions(fontHeight).withStyle("SemiBold")).withExtraKerningFactor(kerning));
    g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                          : juce::TextButton::textColourOffId));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 0), juce::Justification::centred, 1);
}

void RootFlowLookAndFeel::fillTextEditorBackground(juce::Graphics& g,
                                                   int width, int height,
                                                   juce::TextEditor& editor)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    const bool focused = editor.hasKeyboardFocus(true);
    const bool hovered = RootFlow::areHoverEffectsEnabled() && editor.isMouseOverOrDragging();
    const auto frameTint = editor.findColour(focused ? juce::TextEditor::focusedOutlineColourId
                                                     : juce::TextEditor::outlineColourId,
                                             true);
    auto fillBase = editor.findColour(juce::TextEditor::backgroundColourId, true);

    if (fillBase.getAlpha() == 0)
        fillBase = RootFlow::panelSoft.withAlpha(0.44f);

    g.setColour(juce::Colours::black.withAlpha(0.20f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 2.8f), 10.0f);

    juce::ColourGradient body(fillBase.brighter(focused ? 0.12f : 0.05f).withAlpha(focused ? 0.86f : 0.78f),
                              bounds.getCentreX(), bounds.getY(),
                              fillBase.darker(0.18f).withAlpha(0.90f),
                              bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(body);
    g.fillRoundedRectangle(bounds, 9.0f);

    g.setColour(frameTint.withAlpha(focused ? 0.06f : (hovered ? 0.04f : 0.03f)));
    g.fillRoundedRectangle(bounds.reduced(1.2f), 7.6f);

    juce::ColourGradient sheen(juce::Colours::white.withAlpha(focused ? 0.10f : 0.05f),
                               bounds.getCentreX(), bounds.getY(),
                               juce::Colours::transparentBlack,
                               bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.55f, false);
    g.setGradientFill(sheen);
    g.fillRoundedRectangle(bounds.reduced(1.0f), 7.8f);
}

void RootFlowLookAndFeel::drawTextEditorOutline(juce::Graphics& g,
                                                int width, int height,
                                                juce::TextEditor& editor)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
    const bool focused = editor.hasKeyboardFocus(true);
    const bool hovered = RootFlow::areHoverEffectsEnabled() && editor.isMouseOverOrDragging();
    auto rim = editor.findColour(focused ? juce::TextEditor::focusedOutlineColourId
                                         : juce::TextEditor::outlineColourId,
                                 true);

    if (! editor.isEnabled())
        rim = rim.withMultipliedAlpha(0.45f);

    g.setColour(juce::Colours::white.withAlpha(focused ? 0.10f : 0.04f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 8.2f, 0.8f);

    g.setColour(rim.withAlpha(focused ? 0.42f : (hovered ? 0.24f : 0.16f)));
    g.drawRoundedRectangle(bounds, 9.0f, focused ? 1.4f : 1.0f);

    g.setColour(rim.withAlpha(focused ? 0.10f : 0.05f));
    g.drawLine(bounds.getX() + 10.0f,
               bounds.getBottom() - 1.2f,
               bounds.getRight() - 10.0f,
               bounds.getBottom() - 1.2f,
               focused ? 1.2f : 0.8f);
}

void RootFlowLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& l)
{
    auto font = l.getFont();
    if (font.getHeight() <= 0.0f)
        font = juce::Font(juce::FontOptions(13.0f));

    const float fittedHeight = juce::jlimit(8.0f, font.getHeight(), (float) juce::jmax(8, l.getHeight() - 1));
    font = font.withHeight(fittedHeight).withExtraKerningFactor(fittedHeight <= 10.0f ? 0.16f : 0.12f);
    const auto colour = l.findColour(juce::Label::textColourId);

    // Subtle Bio-Glow for labels
    g.setFont(font);
    g.setColour(colour.withAlpha(0.15f));
    g.drawFittedText(l.getText(), l.getLocalBounds().translated(0, 1), l.getJustificationType(), 1);

    g.setColour(colour);
    g.drawFittedText(l.getText(), l.getLocalBounds(), l.getJustificationType(), 1);
}

juce::Font RootFlowLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    const bool compact = box.getWidth() < 110;
    const float fontSize = compact ? 11.0f : 10.8f;
    const float kerning = compact ? 0.10f : 0.06f;
    return juce::Font(juce::FontOptions(fontSize).withStyle("SemiBold")).withExtraKerningFactor(kerning);
}

void RootFlowLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    auto bounds = box.getLocalBounds().reduced(box.getWidth() < 110 ? 10 : 12, 4);
    bounds.removeFromRight(22);

    label.setBounds(bounds);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setMinimumHorizontalScale(0.8f);
}

void RootFlowLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox& box)
{
    auto r = box.getLocalBounds().toFloat().reduced(0.5f);
    auto bezel = r.expanded(3.0f, 2.4f).translated(0.0f, 1.2f);
    const bool hovered = RootFlow::areHoverEffectsEnabled() && box.isMouseOver();

    g.setColour(juce::Colours::black.withAlpha(0.32f));
    g.fillRoundedRectangle(bezel.translated(0.0f, 4.0f), 12.5f);

    juce::ColourGradient bezelGrad(RootFlow::panelSoft.brighter(0.10f).withAlpha(0.38f), bezel.getCentreX(), bezel.getY(),
                                   bg.darker(0.28f).withAlpha(0.56f), bezel.getCentreX(), bezel.getBottom(), false);
    g.setGradientFill(bezelGrad);
    g.fillRoundedRectangle(bezel, 12.5f);
    g.setColour(juce::Colours::black.withAlpha(0.10f));
    g.drawRoundedRectangle(bezel.reduced(0.8f), 11.5f, 1.0f);

    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.fillRoundedRectangle(r.translated(0.0f, 4.0f).expanded(1.8f, 0.8f), 11.5f);
    g.setColour(juce::Colours::black.withAlpha(0.08f));
    g.fillRoundedRectangle(r.translated(0.0f, 1.8f), 10.8f);

    juce::ColourGradient boxGrad(RootFlow::panelSoft.brighter(0.04f).withAlpha(0.86f), r.getCentreX(), r.getY(),
                                 RootFlow::panel.darker(0.16f).withAlpha(0.82f), r.getCentreX(), r.getBottom(), false);
    g.setGradientFill(boxGrad);
    g.fillRoundedRectangle(r, 10.5f);

    juce::ColourGradient topBulge(juce::Colours::white.withAlpha(hovered ? 0.10f : 0.06f),
                                  r.getCentreX(), r.getY(),
                                  juce::Colours::transparentBlack,
                                  r.getCentreX(), r.getY() + r.getHeight() * 0.52f, false);
    g.setGradientFill(topBulge);
    g.fillRoundedRectangle(r.reduced(1.2f), 9.6f);

    juce::ColourGradient lowerOcclusion(juce::Colours::transparentBlack,
                                        r.getCentreX(), r.getY() + r.getHeight() * 0.44f,
                                        juce::Colours::black.withAlpha(0.16f),
                                        r.getCentreX(), r.getBottom(), false);
    g.setGradientFill(lowerOcclusion);
    g.fillRoundedRectangle(r.reduced(1.4f), 9.4f);

    g.setColour((box.hasKeyboardFocus(true) ? accent : accentSoft).withAlpha(hovered ? 0.24f : 0.14f));
    g.drawRoundedRectangle(r, 10.5f, 1.2f);
    
    g.setColour(juce::Colours::white.withAlpha(0.09f));
    g.drawRoundedRectangle(r.reduced(1.2f), 9.2f, 0.9f);

    auto sheen = r.withHeight(r.getHeight() * 0.46f);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.fillRoundedRectangle(sheen, 10.0f);

    auto arrowChamber = juce::Rectangle<float>(18.0f, r.getHeight() - 8.0f)
                            .withCentre({ r.getRight() - 14.0f, r.getCentreY() });
    g.setColour(juce::Colours::black.withAlpha(0.12f));
    g.fillRoundedRectangle(arrowChamber.translated(0.0f, 1.8f), 7.0f);
    juce::ColourGradient arrowChamberGrad(panel.brighter(0.05f), arrowChamber.getCentreX(), arrowChamber.getY(),
                                          bg.darker(0.12f), arrowChamber.getCentreX(), arrowChamber.getBottom(), false);
    g.setGradientFill(arrowChamberGrad);
    g.fillRoundedRectangle(arrowChamber, 6.8f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(arrowChamber.reduced(0.8f), 6.0f, 0.7f);

    auto arrowArea = r.withTrimmedLeft(r.getWidth() - 18.0f).withSize(8.0f, 6.0f).withCentre(r.getCentre().withX(r.getRight() - 11.0f));
    juce::Path p;
    p.addTriangle(arrowArea.getX(), arrowArea.getY(), arrowArea.getRight(), arrowArea.getY(), arrowArea.getCentreX(), arrowArea.getBottom());
    g.setColour(accent.withAlpha(0.62f));
    g.fillPath(p);
}

void RootFlowLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.setColour(bg.brighter(0.02f));
    g.fillAll();
    
    g.setColour(accent.withAlpha(0.08f));
    g.drawRect(0, 0, width, height, 1);
}

void RootFlowLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                             bool, bool isActive, bool isHighlighted, bool isTicked, bool,
                                             const juce::String& text, const juce::String&,
                                             const juce::Drawable*, const juce::Colour*)
{
    if (isHighlighted && isActive)
    {
        g.setColour(accent.withAlpha(0.10f));
        g.fillRect(area.reduced(1));
    }

    g.setColour(isActive ? (isHighlighted ? juce::Colours::white : RootFlowLookAndFeel::getTextColor()) : RootFlowLookAndFeel::getTextColor().withAlpha(0.3f));
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    
    auto r = area.reduced(8, 0).toFloat();
    g.drawText(text, r, juce::Justification::centredLeft);

    if (isTicked)
    {
        auto tickArea = area.withTrimmedLeft(area.getWidth() - 20).reduced(6).toFloat();
        g.setColour(accent);
        g.fillEllipse(tickArea);
    }
}
