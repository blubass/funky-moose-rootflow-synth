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

    // Subtile Bio-Depth Gradient
    juce::ColourGradient grad(bg.brighter(0.04f), w * 0.5f, 0,
                               bg.darker(0.24f), w * 0.5f, (float)h, false);
    g.setGradientFill(grad);
    g.fillRect(0, 0, w, h);
}

void RootFlowLookAndFeel::drawPanel(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(getPanelColor());
    g.fillRoundedRectangle(r, 10.0f);

    // Subtile Glow-Field Border
    g.setColour(getAccentColor().withAlpha(0.08f));
    g.drawRoundedRectangle(r, 10.0f, 1.0f);
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

    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.fillEllipse(juce::Rectangle<float>(radius * 2.08f, radius * 2.08f).withCentre(centre.translated(0.0f, 3.0f)));

    juce::ColourGradient housing(panel.brighter(0.18f), centre.x, centre.y - radius,
                                 bg.darker(0.34f), centre.x, centre.y + radius, false);
    g.setGradientFill(housing);
    g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre));

    g.setColour(primary.withAlpha(hot ? 0.18f : 0.10f));
    g.drawEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre), 1.4f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawEllipse(juce::Rectangle<float>(radius * 1.76f, radius * 1.76f).withCentre(centre), 0.9f);

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
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.strokePath(fullArc, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(primary.withAlpha(hot ? 0.26f : 0.18f));
    g.strokePath(valueArc, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(primary.withAlpha(0.92f));
    g.strokePath(valueArc, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.26f));
    g.strokePath(valueArc, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto coreBounds = juce::Rectangle<float>(radius * 1.42f, radius * 1.42f).withCentre(centre);
    juce::ColourGradient coreGrad(secondary.brighter(0.20f).withAlpha(0.94f), coreBounds.getCentreX(), coreBounds.getY(),
                                  panel.darker(0.12f).withAlpha(0.96f), coreBounds.getCentreX(), coreBounds.getBottom(), false);
    g.setGradientFill(coreGrad);
    g.fillEllipse(coreBounds);

    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.fillEllipse(coreBounds.withHeight(coreBounds.getHeight() * 0.46f).translated(0.0f, -coreBounds.getHeight() * 0.08f));
    g.setColour(primary.withAlpha(0.18f));
    g.drawEllipse(coreBounds, 1.0f);

    juce::Path pointer;
    auto pointerRect = juce::Rectangle<float>(radius * 0.16f, radius * 0.88f).withCentre({ centre.x, centre.y - radius * 0.24f });
    pointer.addRoundedRectangle(pointerRect, pointerRect.getWidth() * 0.5f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
    g.setColour(primary.withAlpha(0.90f));
    g.fillPath(pointer);
    g.setColour(juce::Colours::white.withAlpha(0.34f));
    g.fillPath(pointer, juce::AffineTransform::scale(0.55f, 0.72f, centre.x, centre.y));

    const float orbRadius = 3.8f + sliderPos * 2.4f;
    const auto thumbPoint = juce::Point<float>(centre.x + std::cos(angle - juce::MathConstants<float>::halfPi) * trackRadius,
                                               centre.y + std::sin(angle - juce::MathConstants<float>::halfPi) * trackRadius);
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
        auto housing = juce::Rectangle<float>(sideVertical ? juce::jmax(16.0f, bounds.getWidth() * 0.26f)
                                                           : juce::jmax(24.0f, bounds.getWidth() * 0.44f),
                                              bounds.getHeight() - (sideVertical ? 2.0f : 6.0f))
                           .withCentre(bounds.getCentre());
        auto track = juce::Rectangle<float>(sideVertical ? juce::jmax(6.5f, housing.getWidth() * 0.34f)
                                                         : juce::jmax(11.0f, housing.getWidth() * 0.48f),
                                            housing.getHeight() - (sideVertical ? 10.0f : 16.0f))
                         .withCentre(housing.getCentre());

        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.fillRoundedRectangle(housing.expanded(sideVertical ? 4.0f : 6.0f, sideVertical ? 3.0f : 4.0f),
                               housing.getWidth() * 0.58f);

        juce::ColourGradient shellGrad(panel.brighter(0.16f), housing.getCentreX(), housing.getY(),
                                       bg.darker(0.14f), housing.getCentreX(), housing.getBottom(), false);
        g.setGradientFill(shellGrad);
        g.fillRoundedRectangle(housing, housing.getWidth() * 0.56f);

        g.setColour(primary.withAlpha(hot ? 0.18f : 0.10f));
        g.drawRoundedRectangle(housing, housing.getWidth() * 0.56f, 1.2f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(housing.reduced(1.0f), housing.getWidth() * 0.50f, 0.8f);

        for (int i = 0; i <= 5; ++i)
        {
            const float t = (float) i / 5.0f;
            const float yy = juce::jmap(t, track.getBottom(), track.getY());
            g.setColour((i == 0 || i == 5 ? primary : juce::Colours::white).withAlpha(i == 0 || i == 5 ? 0.16f : 0.08f));
            g.drawLine(housing.getX() + (sideVertical ? 3.0f : 4.0f), yy,
                       housing.getRight() - (sideVertical ? 3.0f : 4.0f), yy,
                       i == 0 || i == 5 ? 1.1f : 0.8f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRoundedRectangle(track, track.getWidth() * 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
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

        auto knobBounds = juce::Rectangle<float>(housing.getWidth() * (sideVertical ? 1.04f : 1.18f),
                                                 housing.getWidth() * (sideVertical ? 0.64f : 0.84f))
                              .withCentre({ housing.getCentreX(), juce::jlimit(track.getY(), track.getBottom(), sliderPos) });
        g.setColour(primary.withAlpha(hot ? 0.30f : 0.22f));
        g.fillRoundedRectangle(knobBounds.expanded(sideVertical ? 3.0f : 5.0f, sideVertical ? 2.0f : 3.0f),
                               knobBounds.getHeight() * 0.54f);

        juce::ColourGradient knobGrad(secondary.brighter(0.16f), knobBounds.getCentreX(), knobBounds.getY(),
                                      primary.darker(0.12f), knobBounds.getCentreX(), knobBounds.getBottom(), false);
        g.setGradientFill(knobGrad);
        g.fillRoundedRectangle(knobBounds, knobBounds.getHeight() * 0.52f);
        g.setColour(juce::Colours::white.withAlpha(0.42f));
        g.drawRoundedRectangle(knobBounds.reduced(0.8f), knobBounds.getHeight() * 0.46f, 0.9f);
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.fillRoundedRectangle(knobBounds.withHeight(knobBounds.getHeight() * 0.36f).translated(0.0f, -knobBounds.getHeight() * 0.08f),
                               knobBounds.getHeight() * 0.34f);
    }
    else if (podHorizontal)
    {
        auto housing = juce::Rectangle<float>(bounds.getWidth() - 2.0f, juce::jmax(15.0f, bounds.getHeight() * 0.40f)).withCentre(bounds.getCentre());
        auto track = juce::Rectangle<float>(housing.getWidth() - 12.0f, juce::jmax(4.5f, housing.getHeight() * 0.16f)).withCentre(housing.getCentre());
        const float thumbX = juce::jlimit(track.getX(), track.getRight(), sliderPos);

        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.fillRoundedRectangle(housing.expanded(3.0f, 3.0f), housing.getHeight() * 0.56f);

        juce::ColourGradient shellGrad(panel.brighter(0.14f), housing.getX(), housing.getCentreY(),
                                       bg.darker(0.14f), housing.getRight(), housing.getCentreY(), false);
        g.setGradientFill(shellGrad);
        g.fillRoundedRectangle(housing, housing.getHeight() * 0.54f);

        g.setColour(primary.withAlpha(hot ? 0.16f : 0.09f));
        g.drawRoundedRectangle(housing, housing.getHeight() * 0.54f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawRoundedRectangle(housing.reduced(1.0f), housing.getHeight() * 0.48f, 0.7f);

        for (int i = 0; i <= 6; ++i)
        {
            const float t = (float) i / 6.0f;
            const float xx = juce::jmap(t, track.getX(), track.getRight());
            g.setColour((i == 0 || i == 6 ? primary : juce::Colours::white).withAlpha(i == 3 ? 0.12f : 0.06f));
            g.drawLine(xx, housing.getY() + 3.5f, xx, housing.getBottom() - 3.5f, i == 3 ? 1.0f : 0.7f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRoundedRectangle(track, track.getHeight() * 0.55f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
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
        g.setColour(primary.withAlpha(hot ? 0.22f : 0.16f));
        g.fillRoundedRectangle(thumb.expanded(3.0f, 2.0f), thumb.getWidth() * 0.48f);

        juce::ColourGradient thumbGrad(secondary.brighter(0.12f), thumb.getCentreX(), thumb.getY(),
                                       primary.darker(0.10f), thumb.getCentreX(), thumb.getBottom(), false);
        g.setGradientFill(thumbGrad);
        g.fillRoundedRectangle(thumb, thumb.getWidth() * 0.44f);
        g.setColour(juce::Colours::white.withAlpha(0.34f));
        g.drawRoundedRectangle(thumb.reduced(0.8f), thumb.getWidth() * 0.40f, 0.8f);
        g.setColour(juce::Colours::white.withAlpha(0.20f));
        g.drawLine(thumb.getCentreX(), thumb.getY() + 3.0f, thumb.getCentreX(), thumb.getBottom() - 3.0f, 0.9f);
    }
    else if (ambientSlider)
    {
        auto housing = juce::Rectangle<float>(bounds.getWidth() - 2.0f, juce::jmax(18.0f, bounds.getHeight() * 0.76f)).withCentre(bounds.getCentre());
        auto track = housing.reduced(16.0f, juce::jmax(4.0f, housing.getHeight() * 0.34f));
        const float thumbX = juce::jlimit(track.getX(), track.getRight(), sliderPos);

        g.setColour(juce::Colours::black.withAlpha(0.24f));
        g.fillRoundedRectangle(housing.expanded(4.0f, 4.0f), housing.getHeight() * 0.56f);

        juce::ColourGradient shellGrad(panel.brighter(0.14f), housing.getX(), housing.getCentreY(),
                                       bg.darker(0.18f), housing.getRight(), housing.getCentreY(), false);
        g.setGradientFill(shellGrad);
        g.fillRoundedRectangle(housing, housing.getHeight() * 0.54f);

        g.setColour(primary.withAlpha(hot ? 0.18f : 0.10f));
        g.drawRoundedRectangle(housing, housing.getHeight() * 0.54f, 1.1f);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawRoundedRectangle(housing.reduced(1.0f), housing.getHeight() * 0.48f, 0.8f);

        for (int i = 0; i <= 4; ++i)
        {
            const float t = (float) i / 4.0f;
            const float xx = juce::jmap(t, track.getX(), track.getRight());
            g.setColour((i == 0 || i == 4 ? primary : juce::Colours::white).withAlpha(i == 2 ? 0.14f : 0.07f));
            g.drawLine(xx, housing.getY() + 5.0f, xx, housing.getBottom() - 5.0f, i == 2 ? 1.1f : 0.8f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.24f));
        g.fillRoundedRectangle(track, track.getHeight() * 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
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
        g.setColour(primary.withAlpha(hot ? 0.22f : 0.16f));
        g.fillRoundedRectangle(thumb.expanded(4.0f, 2.0f), thumb.getWidth() * 0.48f);

        juce::ColourGradient thumbGrad(secondary.brighter(0.12f), thumb.getCentreX(), thumb.getY(),
                                       primary.darker(0.10f), thumb.getCentreX(), thumb.getBottom(), false);
        g.setGradientFill(thumbGrad);
        g.fillRoundedRectangle(thumb, thumb.getWidth() * 0.44f);
        g.setColour(juce::Colours::white.withAlpha(0.34f));
        g.drawRoundedRectangle(thumb.reduced(0.8f), thumb.getWidth() * 0.40f, 0.8f);
        g.setColour(juce::Colours::white.withAlpha(0.20f));
        g.drawLine(thumb.getCentreX(), thumb.getY() + 4.0f, thumb.getCentreX(), thumb.getBottom() - 4.0f, 1.0f);
    }
    else
    {
        auto housing = juce::Rectangle<float>(bounds.getWidth() - 6.0f, juce::jmax(18.0f, bounds.getHeight() * 0.54f)).withCentre(bounds.getCentre());
        auto track = juce::Rectangle<float>(housing.getWidth() - 18.0f, juce::jmax(7.0f, housing.getHeight() * 0.18f)).withCentre(housing.getCentre());

        g.setColour(juce::Colours::black.withAlpha(0.26f));
        g.fillRoundedRectangle(housing.expanded(3.0f, 4.0f), housing.getHeight() * 0.62f);

        juce::ColourGradient shellGrad(panel.brighter(0.14f), housing.getX(), housing.getCentreY(),
                                       bg.darker(0.14f), housing.getRight(), housing.getCentreY(), false);
        g.setGradientFill(shellGrad);
        g.fillRoundedRectangle(housing, housing.getHeight() * 0.60f);

        g.setColour(primary.withAlpha(hot ? 0.18f : 0.10f));
        g.drawRoundedRectangle(housing, housing.getHeight() * 0.60f, 1.2f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(housing.reduced(1.0f), housing.getHeight() * 0.52f, 0.8f);

        for (int i = 0; i <= 6; ++i)
        {
            const float t = (float) i / 6.0f;
            const float xx = juce::jmap(t, track.getX(), track.getRight());
            g.setColour((i == 0 || i == 6 ? primary : juce::Colours::white).withAlpha(i == 0 || i == 6 ? 0.16f : 0.08f));
            g.drawLine(xx, housing.getY() + 4.0f, xx, housing.getBottom() - 4.0f, i == 3 ? 1.2f : 0.8f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillRoundedRectangle(track, track.getHeight() * 0.55f);

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

        g.setColour(primary.withAlpha(0.16f));
        g.fillEllipse(juce::Rectangle<float>(housing.getHeight() * 0.74f, housing.getHeight() * 0.74f).withCentre({ track.getX(), housing.getCentreY() }));
        g.fillEllipse(juce::Rectangle<float>(housing.getHeight() * 0.74f, housing.getHeight() * 0.74f).withCentre({ track.getRight(), housing.getCentreY() }));

        auto knobBounds = juce::Rectangle<float>(housing.getHeight() * 0.96f, housing.getHeight() * 0.96f)
                              .withCentre({ juce::jlimit(track.getX(), track.getRight(), sliderPos), housing.getCentreY() });
        g.setColour(primary.withAlpha(hot ? 0.30f : 0.22f));
        g.fillEllipse(knobBounds.expanded(4.0f));

        juce::ColourGradient knobGrad(secondary.brighter(0.20f), knobBounds.getCentreX(), knobBounds.getY(),
                                      primary.darker(0.10f), knobBounds.getCentreX(), knobBounds.getBottom(), false);
        g.setGradientFill(knobGrad);
        g.fillEllipse(knobBounds);
        g.setColour(juce::Colours::white.withAlpha(0.42f));
        g.drawEllipse(knobBounds.reduced(0.8f), 0.9f);
        g.setColour(juce::Colours::white.withAlpha(0.26f));
        g.fillEllipse(knobBounds.withSizeKeepingCentre(knobBounds.getWidth() * 0.28f, knobBounds.getHeight() * 0.28f)
                                 .translated(-knobBounds.getWidth() * 0.12f, -knobBounds.getHeight() * 0.14f));
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
    
    // Check if it's an organic "spore" button (buttons without text/icons usually)
    if (b.getButtonText().isEmpty())
    {
        juce::Path p;
        p.addEllipse(r);

        float alpha = isPressed ? 0.95f : (hoverVisual ? 0.7f : 0.45f);
        
        // Biological Spore Glow
        juce::ColourGradient grad(accent.withAlpha(alpha), r.getCentreX(), r.getCentreY(),
                                  accent.withAlpha(0.0f), r.getCentreX(), r.getBottom(), true);
        g.setGradientFill(grad);
        g.fillPath(p);

        // Vibrant Core Outline
        g.setColour(accent.withAlpha(alpha + 0.05f));
        g.strokePath(p, juce::PathStrokeType(1.6f));
        
        // Internal Spark
        if (hoverVisual || isPressed)
        {
            g.setColour(juce::Colours::white.withAlpha(isPressed ? 0.8f : 0.4f));
            g.fillEllipse(r.getCentreX() - 2.0f, r.getCentreY() - 2.0f, 4.0f, 4.0f);
        }
    }
    else
    {
        const bool toggled = b.getToggleState();
        auto base = juce::Colour(0xff0d1815).withAlpha(toggled ? 0.92f : 0.84f);
        if (hoverVisual) base = base.brighter(0.14f);
        if (isPressed) base = accent.withAlpha(0.18f);

        juce::ColourGradient btnGrad(base.brighter(toggled ? 0.22f : 0.10f), r.getCentreX(), r.getY(),
                                     base.darker(0.18f), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill(btnGrad);
        g.fillRoundedRectangle(r, 9.5f);

        g.setColour((toggled ? accent : accentSoft).withAlpha(toggled ? 0.34f : (hoverVisual ? 0.28f : 0.14f)));
        g.drawRoundedRectangle(r, 9.5f, toggled ? 1.3f : 1.0f);

        g.setColour(juce::Colours::white.withAlpha(toggled ? 0.12f : 0.07f));
        g.drawRoundedRectangle(r.reduced(1.0f), 8.4f, 0.8f);

        if (toggled)
        {
            g.setColour(accent.withAlpha(0.14f));
            g.fillRoundedRectangle(r.expanded(1.8f), 11.0f);
        }
    }
}

void RootFlowLookAndFeel::drawButtonText(juce::Graphics& g,
                                         juce::TextButton& button,
                                         bool,
                                         bool)
{
    g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("SemiBold")).withExtraKerningFactor(0.18f));
    g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                          : juce::TextButton::textColourOffId));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

void RootFlowLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& l)
{
    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(13.0f)).withExtraKerningFactor(0.12f));

    // Subtle Bio-Glow for labels
    g.setColour(text.withAlpha(0.15f));
    g.drawText(l.getText(), l.getLocalBounds().translated(0, 1), juce::Justification::centred);
    
    g.setColour(text);
    g.drawText(l.getText(), l.getLocalBounds(), juce::Justification::centred);
}

void RootFlowLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox& box)
{
    auto r = box.getLocalBounds().toFloat().reduced(0.5f);
    
    juce::ColourGradient boxGrad(juce::Colour(0xff0f1d18).withAlpha(0.94f), r.getCentreX(), r.getY(),
                                 juce::Colour(0xff091510).withAlpha(0.90f), r.getCentreX(), r.getBottom(), false);
    g.setGradientFill(boxGrad);
    g.fillRoundedRectangle(r, 10.0f);
    
    g.setColour((box.hasKeyboardFocus(true) ? accent : accentSoft).withAlpha((RootFlow::areHoverEffectsEnabled() && box.isMouseOver()) ? 0.30f : 0.14f));
    g.drawRoundedRectangle(r, 10.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(r.reduced(1.0f), 9.0f, 0.8f);

    auto sheen = r.withHeight(r.getHeight() * 0.46f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillRoundedRectangle(sheen, 10.0f);

    auto arrowArea = r.withTrimmedLeft(r.getWidth() - 18.0f).withSize(8.0f, 6.0f).withCentre(r.getCentre().withX(r.getRight() - 12.0f));
    juce::Path p;
    p.addTriangle(arrowArea.getX(), arrowArea.getY(), arrowArea.getRight(), arrowArea.getY(), arrowArea.getCentreX(), arrowArea.getBottom());
    g.setColour(accent.withAlpha(0.78f));
    g.fillPath(p);
}

void RootFlowLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.setColour(bg.brighter(0.04f));
    g.fillAll();
    
    g.setColour(accent.withAlpha(0.12f));
    g.drawRect(0, 0, width, height, 1);
}

void RootFlowLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                             bool, bool isActive, bool isHighlighted, bool isTicked, bool,
                                             const juce::String& text, const juce::String&,
                                             const juce::Drawable*, const juce::Colour*)
{
    if (isHighlighted && isActive)
    {
        g.setColour(accent.withAlpha(0.15f));
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
