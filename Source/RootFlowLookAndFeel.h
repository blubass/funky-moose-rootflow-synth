#pragma once
#include <JuceHeader.h>

class RootFlowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RootFlowLookAndFeel()
    {
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(160, 255, 200).withAlpha(0.8f));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    void setWoodImage(const juce::Image& img) { woodImage = img; }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto centre = bounds.getCentre();
        const bool isFxKnob = slider.getProperties()["rfStyle"] == "fx";
        const bool isMainKnob = ! isFxKnob;
        const auto accentHueVar = slider.getProperties()["rfAccentHue"];
        const float accentHue = accentHueVar.isVoid() ? 0.325f : (float) (double) accentHueVar;
        const auto accentShadow = juce::Colour::fromHSV(accentHue, 0.70f, 0.30f, 1.0f);
        const auto accentGlow = juce::Colour::fromHSV(accentHue, 0.72f, 0.98f, 1.0f);
        const auto accentCore = juce::Colour::fromHSV(accentHue, 0.34f, 1.0f, 1.0f);
        const auto interactionBoostVar = slider.getProperties()["rfInteractionBoost"];
        const float interactionBoost = interactionBoostVar.isVoid() ? 1.0f : (float) (double) interactionBoostVar;
        const float microTime = (float) juce::Time::getMillisecondCounterHiRes() * 0.00020f;
        const float shimmer = 0.5f + 0.5f * std::sin(microTime + accentHue * juce::MathConstants<float>::twoPi + sliderPosProportional * 2.4f);
        const float ledShimmer = 0.96f + shimmer * 0.08f;
        const float haloScale = 0.98f + shimmer * 0.07f;
        const float focusGlowBoost = 0.95f + shimmer * 0.10f;
        const float interactionGlowBoost = 1.0f + (interactionBoost - 1.0f) * 2.30f;
        const float interactionHaloScale = 1.0f + (interactionBoost - 1.0f) * 0.80f;

        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float ledPresence = (isFxKnob ? 0.92f : 1.0f)
                                * (0.56f + 0.44f * std::pow(juce::jlimit(0.0f, 1.0f, sliderPosProportional), 0.78f));

        auto outerRing  = bounds;
        auto ledRing    = bounds.reduced(radius * (isFxKnob ? 0.045f : 0.025f));
        auto knobBounds = bounds.reduced(radius * (isFxKnob ? 0.16f : 0.10f));
        auto faceBounds = knobBounds.reduced(knobBounds.getWidth() * (isFxKnob ? 0.17f : 0.18f));

        g.setColour(juce::Colours::black.withAlpha(isFxKnob ? 0.32f : 0.40f));
        g.fillEllipse(outerRing.translated(0.0f, radius * (isFxKnob ? 0.11f : 0.15f)));

        {
            juce::ColourGradient outerGrad(
                juce::Colour(isFxKnob ? 70 : 78, isFxKnob ? 68 : 74, isFxKnob ? 64 : 68),
                outerRing.getTopLeft(),
                juce::Colour(10, 10, 10),
                outerRing.getBottomRight(),
                false
            );
            g.setGradientFill(outerGrad);
            g.fillEllipse(outerRing);

            g.setColour(isMainKnob ? accentGlow.withAlpha(0.18f * (1.0f + (interactionBoost - 1.0f) * 0.8f))
                                   : juce::Colour(128, 104, 74).withAlpha(0.10f));
            g.drawEllipse(outerRing.reduced(1.4f), isFxKnob ? 1.0f : 1.2f);
            g.setColour(juce::Colours::black.withAlpha(0.86f));
            g.drawEllipse(outerRing.reduced(0.8f), isFxKnob ? 1.6f : 1.9f);

            if (isMainKnob)
            {
                g.setColour(accentGlow.withAlpha(0.030f * interactionGlowBoost));
                g.fillEllipse(outerRing.expanded(radius * 0.10f, radius * 0.10f));
                g.setColour(juce::Colours::black.withAlpha(0.34f));
                g.fillEllipse(outerRing.reduced(4.0f).translated(0.0f, radius * 0.055f));
            }
        }

        // =====================================================
        // LED background arc (Deep Recess)
        {
            juce::Path bgArc;
            bgArc.addCentredArc(centre.x, centre.y,
                                ledRing.getWidth() * 0.5f, ledRing.getHeight() * 0.5f,
                                0.0f,
                                rotaryStartAngle, rotaryEndAngle,
                                true);

            g.setColour(isMainKnob ? accentShadow.withAlpha(0.88f)
                                   : juce::Colour(12, 30, 16).withAlpha(0.68f));
            g.strokePath(bgArc, juce::PathStrokeType(isFxKnob ? 2.2f : 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // =====================================================
        // active LED arc
        {
            juce::Path valueArc;
            valueArc.addCentredArc(centre.x, centre.y,
                                   ledRing.getWidth() * 0.5f, ledRing.getHeight() * 0.5f,
                                   0.0f,
                                   rotaryStartAngle, angle,
                                   true);

            g.setColour(isMainKnob ? accentGlow.withAlpha(0.18f * ledPresence * ledShimmer * interactionGlowBoost)
                                   : juce::Colour(118, 255, 108).withAlpha(0.085f * ledPresence * interactionGlowBoost));
            g.strokePath(valueArc, juce::PathStrokeType(isFxKnob ? 5.2f : 7.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(isMainKnob ? accentGlow.brighter(0.08f).withAlpha(0.34f * ledPresence * ledShimmer * interactionGlowBoost)
                                   : juce::Colour(145, 255, 128).withAlpha(0.18f * ledPresence * interactionGlowBoost));
            g.strokePath(valueArc, juce::PathStrokeType(isFxKnob ? 2.7f : 3.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(isMainKnob ? accentCore.withAlpha(0.98f * ledPresence)
                                   : juce::Colour(205, 255, 170).withAlpha(0.78f * ledPresence));
            g.strokePath(valueArc, juce::PathStrokeType(isFxKnob ? 1.40f : 2.05f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const float totalArcSpan = rotaryEndAngle - rotaryStartAngle;
            const float activeSpan = juce::jmax(0.0f, angle - rotaryStartAngle);
            const float focusSpan = juce::jmin(activeSpan, totalArcSpan * (isFxKnob ? 0.16f : 0.18f));

            if (focusSpan > 0.001f)
            {
                juce::Path focusArc;
                focusArc.addCentredArc(centre.x, centre.y,
                                       ledRing.getWidth() * 0.5f, ledRing.getHeight() * 0.5f,
                                       0.0f,
                                       angle - focusSpan, angle,
                                       true);

                g.setColour(isMainKnob ? accentCore.brighter(0.12f).withAlpha(0.54f * ledPresence * focusGlowBoost * interactionGlowBoost)
                                       : juce::Colour(228, 255, 190).withAlpha(0.36f * ledPresence * interactionGlowBoost));
                g.strokePath(focusArc, juce::PathStrokeType(isFxKnob ? 1.85f : 2.55f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // End LED dot
            auto dotPos = centre.getPointOnCircumference(ledRing.getWidth() * 0.5f, angle);
            const float outerDotRadius = (isFxKnob ? 3.8f : 4.5f) * (isMainKnob ? haloScale * interactionHaloScale : 1.0f);
            const float midDotRadius = (isFxKnob ? 1.95f : 2.20f) * (isMainKnob ? (0.98f + shimmer * 0.04f) * interactionHaloScale : 1.0f);
            const float coreDotRadius = isFxKnob ? 0.8f : 1.0f;
            g.setColour(isMainKnob ? accentGlow.withAlpha(0.26f * ledPresence * ledShimmer * interactionGlowBoost)
                                   : juce::Colour(200, 255, 176).withAlpha(0.14f * ledPresence * interactionGlowBoost));
            g.fillEllipse(dotPos.x - outerDotRadius, dotPos.y - outerDotRadius,
                          outerDotRadius * 2.0f, outerDotRadius * 2.0f);
            g.setColour(isMainKnob ? accentCore.withAlpha(0.92f * ledPresence)
                                   : juce::Colour(215, 255, 182).withAlpha(0.70f * ledPresence));
            g.fillEllipse(dotPos.x - midDotRadius, dotPos.y - midDotRadius,
                          midDotRadius * 2.0f, midDotRadius * 2.0f);
            g.setColour(isMainKnob ? accentCore.brighter(0.20f)
                                   : juce::Colour(248, 255, 228));
            g.fillEllipse(dotPos.x - coreDotRadius, dotPos.y - coreDotRadius,
                          coreDotRadius * 2.0f, coreDotRadius * 2.0f);
        }

        // =====================================================
        // knob body
        {
            if (isFxKnob)
            {
                g.setColour(juce::Colours::black.withAlpha(0.34f));
                g.fillEllipse(knobBounds.translated(0.0f, knobBounds.getHeight() * 0.06f));

                juce::ColourGradient sideGrad(
                    juce::Colour(96, 96, 96),
                    knobBounds.getTopLeft(),
                    juce::Colour(12, 12, 12),
                    knobBounds.getBottomRight(),
                    false
                );
                g.setGradientFill(sideGrad);
                g.fillEllipse(knobBounds);

                juce::ColourGradient faceGrad(
                    juce::Colour(78, 78, 78),
                    faceBounds.getX(), faceBounds.getY(),
                    juce::Colour(22, 22, 22),
                    faceBounds.getRight(), faceBounds.getBottom(),
                    false
                );
                g.setGradientFill(faceGrad);
                g.fillEllipse(faceBounds);

                g.setColour(juce::Colours::black.withAlpha(0.16f));
                g.fillEllipse(faceBounds.reduced(faceBounds.getWidth() * 0.15f)
                                        .translated(0.0f, faceBounds.getHeight() * 0.04f));

                auto innerCap = faceBounds.reduced(faceBounds.getWidth() * 0.22f);
                g.setColour(juce::Colour(36, 36, 36));
                g.fillEllipse(innerCap);

                g.setColour(juce::Colour(150, 150, 150).withAlpha(0.10f));
                g.drawEllipse(faceBounds.reduced(1.2f), 0.9f);
                g.setColour(juce::Colours::black.withAlpha(0.80f));
                g.drawEllipse(knobBounds.reduced(0.8f), 1.1f);
                g.drawEllipse(faceBounds.reduced(0.6f), 1.1f);
            }
            else if (woodImage.isValid())
            {
                g.setColour(juce::Colours::black.withAlpha(0.40f));
                g.fillEllipse(knobBounds.translated(0.0f, knobBounds.getHeight() * 0.07f));

                juce::ColourGradient sideGrad(
                    juce::Colour(78, 48, 28),
                    knobBounds.getTopLeft(),
                    juce::Colour(10, 6, 4),
                    knobBounds.getBottomRight(),
                    false
                );
                g.setGradientFill(sideGrad);
                g.fillEllipse(knobBounds);

                g.saveState();
                juce::Path clipPath;
                clipPath.addEllipse(faceBounds);
                g.reduceClipRegion(clipPath);

                g.setTiledImageFill(woodImage, x % 300, y % 300, 0.22f);
                g.fillEllipse(faceBounds);

                juce::ColourGradient faceShade(
                    juce::Colour(166, 126, 82).withAlpha(0.08f),
                    juce::Point<float>(faceBounds.getCentreX(), faceBounds.getY()),
                    juce::Colours::black.withAlpha(0.46f),
                    juce::Point<float>(faceBounds.getCentreX(), faceBounds.getBottom()),
                    false
                );
                g.setGradientFill(faceShade);
                g.fillEllipse(faceBounds);

                g.setColour(juce::Colours::black.withAlpha(0.12f));
                g.fillEllipse(faceBounds.reduced(faceBounds.getWidth() * 0.14f)
                                        .translated(0.0f, faceBounds.getHeight() * 0.05f));

                auto woodCap = faceBounds.reduced(faceBounds.getWidth() * 0.18f);
                g.setColour(juce::Colour(18, 10, 6).withAlpha(0.20f));
                g.fillEllipse(woodCap.translated(0.0f, woodCap.getHeight() * 0.03f));
                g.setColour(juce::Colours::black.withAlpha(0.64f));
                g.drawEllipse(woodCap.reduced(0.4f), 0.9f);

                g.setColour(juce::Colour(166, 126, 82).withAlpha(0.10f));
                g.drawEllipse(faceBounds.reduced(1.0f), 0.9f);
                g.setColour(juce::Colours::black.withAlpha(0.74f));
                g.drawEllipse(faceBounds.reduced(0.5f), 1.0f);

                g.restoreState();

                g.setColour(juce::Colour(120, 88, 56).withAlpha(0.10f));
                g.drawEllipse(knobBounds.reduced(1.2f), 1.0f);
                g.setColour(juce::Colours::black.withAlpha(0.80f));
                g.drawEllipse(knobBounds.reduced(0.7f), 1.4f);
            }
            else
            {
                g.setColour(juce::Colours::black.withAlpha(0.24f));
                g.fillEllipse(knobBounds.translated(0.0f, knobBounds.getHeight() * 0.05f));

                juce::ColourGradient sideGrad(
                    juce::Colour(58, 36, 22),
                    knobBounds.getTopLeft(),
                    juce::Colour(14, 8, 5),
                    knobBounds.getBottomRight(),
                    false
                );
                g.setGradientFill(sideGrad);
                g.fillEllipse(knobBounds);

                g.setColour(juce::Colour(44, 26, 16));
                g.fillEllipse(faceBounds);

                g.setColour(juce::Colour(45, 22, 10).withAlpha(0.22f));
                for (int i = 0; i < 6; ++i)
                {
                    float yy = faceBounds.getY() + faceBounds.getHeight() * (0.16f + 0.12f * (float)i);
                    g.drawLine(faceBounds.getX() + 7.0f, yy,
                               faceBounds.getRight() - 7.0f, yy + ((i % 2 == 0) ? 1.5f : -1.5f),
                               0.9f);
                }

                g.setColour(juce::Colours::black.withAlpha(0.70f));
                g.drawEllipse(faceBounds.reduced(0.5f), 1.0f);
                g.drawEllipse(knobBounds.reduced(0.7f), 1.2f);
            }
        }

        // =====================================================
        // pointer (Precise marker)
        {
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
            juce::Path pointer;
            const float pointerLength = faceBounds.getHeight() * (isFxKnob ? 0.25f : 0.30f);
            const float pointerThickness = isFxKnob ? 1.55f : 2.35f;
            pointer.addRoundedRectangle(-pointerThickness * 0.5f,
                                        -pointerLength,
                                        pointerThickness,
                                        pointerLength,
                                        0.9f);
            if (isMainKnob)
            {
                g.setColour(accentGlow.withAlpha(0.18f));
                g.fillPath(pointer,
                           juce::AffineTransform::scale(1.65f, 1.12f)
                               .translated(centre.x, centre.y));
            }

            g.setColour(isMainKnob ? accentCore.withAlpha(0.96f)
                                   : juce::Colour(245, 255, 220).withAlpha(0.93f));
            g.fillPath(pointer,
                       juce::AffineTransform::translation(centre.x, centre.y));
            g.restoreState();
        }

        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.drawEllipse(faceBounds, 1.2f);
    }

    juce::Font getLabelFont(juce::Label&) override
    {
        return juce::FontOptions().withHeight(11.0f).withStyle("Bold");
    }

private:
    juce::Image woodImage;
};
