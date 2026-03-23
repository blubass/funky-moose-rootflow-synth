
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::Image createMatteWoodImage(const juce::Image& source)
{
    if (! source.isValid())
        return {};

    juce::Image matteWood(juce::Image::ARGB, source.getWidth(), source.getHeight(), true);
    const juce::Colour darkWood(22, 13, 8);
    const juce::Colour midWood(76, 44, 22);

    for (int y = 0; y < source.getHeight(); ++y)
    {
        for (int x = 0; x < source.getWidth(); ++x)
        {
            auto pixel = source.getPixelAt(x, y);
            float localSum = 0.0f;
            int localCount = 0;

            for (int oy = -3; oy <= 3; ++oy)
            {
                const int sampleY = juce::jlimit(0, source.getHeight() - 1, y + oy);
                for (int ox = -3; ox <= 3; ++ox)
                {
                    const int sampleX = juce::jlimit(0, source.getWidth() - 1, x + ox);
                    localSum += source.getPixelAt(sampleX, sampleY).getBrightness();
                    ++localCount;
                }
            }

            const float brightness = pixel.getBrightness();
            const float localAverage = localCount > 0 ? localSum / (float) localCount : brightness;
            const float detail = brightness - localAverage;

            const float grainMix = juce::jlimit(0.0f, 1.0f,
                                                0.34f + detail * 5.4f + (localAverage - 0.42f) * 0.08f);
            matteWood.setPixelAt(x, y, darkWood.interpolatedWith(midWood, grainMix).withAlpha(pixel.getFloatAlpha()));
        }
    }

    return matteWood;
}

juce::Image createBlurredCopy(const juce::Image& source, float blurRadius)
{
    if (! source.isValid())
        return {};

    auto blurred = source.createCopy();
    juce::ImageConvolutionKernel kernel(5);
    kernel.createGaussianBlur(blurRadius);
    kernel.applyToImage(blurred, blurred, blurred.getBounds());
    return blurred;
}

juce::Image createPanelNoiseImage(int size)
{
    juce::Image noise(juce::Image::ARGB, size, size, true);
    juce::Random random(0x5A17C0DE);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const auto value = (juce::uint8) juce::jlimit(78, 196, 112 + (int) random.nextInt(72));
            const auto alpha = (juce::uint8) juce::jlimit(0, 255, 14 + (int) random.nextInt(28));
            noise.setPixelAt(x, y, juce::Colour::fromRGBA(value, value, value, alpha));
        }
    }

    return noise;
}

const juce::Image& getPanelNoiseImage()
{
    static const juce::Image noise = createPanelNoiseImage(192);
    return noise;
}

constexpr float getRainDelayTimeSeconds(float amount) noexcept
{
    return (85.0f + std::pow(juce::jlimit(0.0f, 1.0f, amount), 0.84f) * 390.0f) / 1000.0f;
}

struct SeasonDisplayInfo
{
    juce::String name;
    juce::String descriptor;
    juce::Colour core;
    juce::Colour glow;
    juce::Colour panelTop;
    juce::Colour panelBottom;
    juce::Colour rim;
};

SeasonDisplayInfo getSeasonDisplayInfo(float value)
{
    struct Palette
    {
        const char* name;
        const char* descriptor;
        juce::Colour core;
        juce::Colour glow;
        juce::Colour panelTop;
        juce::Colour panelBottom;
        juce::Colour rim;
    };

    static const Palette spring {
        "SPRING",
        "BRIGHT / QUICK",
        juce::Colour(198, 255, 190),
        juce::Colour(128, 255, 174),
        juce::Colour(52, 46, 22),
        juce::Colour(24, 22, 12),
        juce::Colour(162, 220, 138)
    };
    static const Palette summer {
        "SUMMER",
        "LUSH / OPEN",
        juce::Colour(255, 232, 150),
        juce::Colour(255, 208, 108),
        juce::Colour(66, 46, 18),
        juce::Colour(28, 18, 10),
        juce::Colour(224, 188, 104)
    };
    static const Palette autumn {
        "AUTUMN",
        "DRIFT / EARTH",
        juce::Colour(255, 194, 126),
        juce::Colour(255, 154, 98),
        juce::Colour(64, 34, 18),
        juce::Colour(28, 14, 10),
        juce::Colour(218, 138, 88)
    };
    static const Palette winter {
        "WINTER",
        "COLD / STILL",
        juce::Colour(198, 238, 255),
        juce::Colour(126, 214, 255),
        juce::Colour(30, 40, 46),
        juce::Colour(14, 18, 26),
        juce::Colour(146, 214, 236)
    };

    const float x = juce::jlimit(0.0f, 1.0f, value);
    constexpr float segment = 1.0f / 3.0f;
    const Palette* a = &spring;
    const Palette* b = &summer;
    float t = 0.0f;

    if (x <= segment)
    {
        t = x / segment;
        a = &spring;
        b = &summer;
    }
    else if (x <= segment * 2.0f)
    {
        t = (x - segment) / segment;
        a = &summer;
        b = &autumn;
    }
    else
    {
        t = (x - segment * 2.0f) / segment;
        a = &autumn;
        b = &winter;
    }

    const Palette& dominant = t < 0.5f ? *a : *b;
    return {
        dominant.name,
        dominant.descriptor,
        a->core.interpolatedWith(b->core, t),
        a->glow.interpolatedWith(b->glow, t),
        a->panelTop.interpolatedWith(b->panelTop, t),
        a->panelBottom.interpolatedWith(b->panelBottom, t),
        a->rim.interpolatedWith(b->rim, t)
    };
}

struct AtmosDisplayInfo
{
    juce::String name;
    juce::String descriptor;
    juce::Colour core;
    juce::Colour glow;
    juce::Colour panelTop;
    juce::Colour panelBottom;
    juce::Colour rim;
};

AtmosDisplayInfo getAtmosDisplayInfo(float value)
{
    struct Palette
    {
        const char* name;
        const char* descriptor;
        juce::Colour core;
        juce::Colour glow;
        juce::Colour panelTop;
        juce::Colour panelBottom;
        juce::Colour rim;
    };

    static const Palette seed {
        "SEED",
        "BARE / AIR",
        juce::Colour(232, 226, 188),
        juce::Colour(176, 214, 146),
        juce::Colour(54, 44, 22),
        juce::Colour(24, 18, 10),
        juce::Colour(204, 186, 126)
    };
    static const Palette mist {
        "MIST",
        "LEAF / BREEZE",
        juce::Colour(204, 255, 234),
        juce::Colour(126, 246, 212),
        juce::Colour(20, 48, 40),
        juce::Colour(10, 22, 18),
        juce::Colour(142, 244, 214)
    };
    static const Palette canopy {
        "CANOPY",
        "SOFT / WIDE",
        juce::Colour(194, 244, 255),
        juce::Colour(122, 214, 255),
        juce::Colour(18, 44, 50),
        juce::Colour(10, 18, 24),
        juce::Colour(146, 220, 246)
    };
    static const Palette weather {
        "WEATHER",
        "WIND / CINEMA",
        juce::Colour(222, 238, 255),
        juce::Colour(168, 214, 255),
        juce::Colour(24, 34, 50),
        juce::Colour(10, 14, 24),
        juce::Colour(188, 216, 246)
    };

    const float x = juce::jlimit(0.0f, 1.0f, value);
    constexpr float segment = 1.0f / 3.0f;
    const Palette* a = &seed;
    const Palette* b = &mist;
    float t = 0.0f;

    if (x <= segment)
    {
        t = x / segment;
        a = &seed;
        b = &mist;
    }
    else if (x <= segment * 2.0f)
    {
        t = (x - segment) / segment;
        a = &mist;
        b = &canopy;
    }
    else
    {
        t = (x - segment * 2.0f) / segment;
        a = &canopy;
        b = &weather;
    }

    const Palette& dominant = t < 0.5f ? *a : *b;
    return {
        dominant.name,
        dominant.descriptor,
        a->core.interpolatedWith(b->core, t),
        a->glow.interpolatedWith(b->glow, t),
        a->panelTop.interpolatedWith(b->panelTop, t),
        a->panelBottom.interpolatedWith(b->panelBottom, t),
        a->rim.interpolatedWith(b->rim, t)
    };
}

struct InstabilityDisplayInfo
{
    juce::String name;
    juce::String descriptor;
    juce::Colour core;
    juce::Colour glow;
    juce::Colour panelTop;
    juce::Colour panelBottom;
    juce::Colour rim;
};

InstabilityDisplayInfo getInstabilityDisplayInfo(float value)
{
    struct Palette
    {
        const char* name;
        const char* descriptor;
        juce::Colour core;
        juce::Colour glow;
        juce::Colour panelTop;
        juce::Colour panelBottom;
        juce::Colour rim;
    };

    static const Palette rooted {
        "ROOTED",
        "TIGHT / STILL",
        juce::Colour(226, 232, 198),
        juce::Colour(170, 202, 138),
        juce::Colour(58, 42, 24),
        juce::Colour(24, 18, 10),
        juce::Colour(196, 184, 128)
    };
    static const Palette living {
        "LIVING",
        "SOFT / HUMAN",
        juce::Colour(214, 255, 202),
        juce::Colour(132, 248, 164),
        juce::Colour(26, 48, 30),
        juce::Colour(12, 20, 14),
        juce::Colour(156, 240, 174)
    };
    static const Palette feral {
        "FERAL",
        "DRIFT / BLOOM",
        juce::Colour(255, 214, 160),
        juce::Colour(255, 162, 112),
        juce::Colour(64, 32, 18),
        juce::Colour(26, 14, 10),
        juce::Colour(240, 168, 112)
    };

    const float x = juce::jlimit(0.0f, 1.0f, value);
    const Palette* a = &rooted;
    const Palette* b = &living;
    float t = 0.0f;

    if (x <= 0.5f)
    {
        t = x / 0.5f;
        a = &rooted;
        b = &living;
    }
    else
    {
        t = (x - 0.5f) / 0.5f;
        a = &living;
        b = &feral;
    }

    const Palette& dominant = t < 0.5f ? *a : *b;
    return {
        dominant.name,
        dominant.descriptor,
        a->core.interpolatedWith(b->core, t),
        a->glow.interpolatedWith(b->glow, t),
        a->panelTop.interpolatedWith(b->panelTop, t),
        a->panelBottom.interpolatedWith(b->panelBottom, t),
        a->rim.interpolatedWith(b->rim, t)
    };
}

float getPanelBreathe(const juce::Rectangle<float>& area, float rateScale = 1.0f) noexcept
{
    const float time = (float) juce::Time::getMillisecondCounterHiRes();
    const float phase = time * (0.00020f * rateScale) + area.getX() * 0.010f + area.getY() * 0.008f;
    return 0.5f + 0.5f * std::sin(phase);
}

juce::Colour getPanelBreathingColour(juce::Colour baseColour,
                                     const juce::Rectangle<float>& area,
                                     float baseLift,
                                     float breatheLift,
                                     float rateScale = 1.0f) noexcept
{
    const float breathe = getPanelBreathe(area, rateScale);
    const float targetBrightness = juce::jlimit(0.0f, 1.0f,
                                                baseColour.getBrightness() + baseLift + breathe * breatheLift);
    return baseColour.withBrightness(targetBrightness).withAlpha(baseColour.getFloatAlpha());
}

struct CachedEditorAssets
{
    juce::Image woodBackground;
    juce::Image rootsBackground;
    juce::Image rootsOverlaySoft;
    juce::Image rootsOverlayDetail;
    juce::Image rootsOverlayDetailBlurred;
    juce::Image mooseLogo;
};


const CachedEditorAssets& getCachedEditorAssets()
{
    static const CachedEditorAssets cache = []()
    {
        CachedEditorAssets assets;

        const auto rawWood = juce::ImageFileFormat::loadFrom(BinaryData::wood_texture_png, BinaryData::wood_texture_pngSize);
        assets.woodBackground = createMatteWoodImage(rawWood);
        assets.rootsBackground = juce::ImageFileFormat::loadFrom(BinaryData::roots_bg_png, BinaryData::roots_bg_pngSize);
        assets.rootsOverlaySoft = juce::ImageFileFormat::loadFrom(BinaryData::roots_overlay_soft_png, BinaryData::roots_overlay_soft_pngSize);
        assets.rootsOverlayDetail = juce::ImageFileFormat::loadFrom(BinaryData::roots_overlay_detail_png, BinaryData::roots_overlay_detail_pngSize);
        assets.mooseLogo = juce::ImageFileFormat::loadFrom(BinaryData::moose_logo_png, BinaryData::moose_logo_pngSize);
        assets.rootsOverlayDetailBlurred = createBlurredCopy(assets.rootsOverlayDetail, 2.0f);
        return assets;
    }();


    return cache;
}

constexpr std::array<juce_wchar, 13> virtualKeyboardCharacters { 'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k' };

void drawGlowText(juce::Graphics& g,
                  const juce::String& text,
                  juce::Rectangle<int> area,
                  const juce::Font& font,
                  juce::Colour coreColour,
                  juce::Colour innerGlowColour,
                  juce::Colour outerGlowColour,
                  juce::Justification justification = juce::Justification::centred)
{
    g.setFont(font);
    g.setColour(outerGlowColour);
    g.drawFittedText(text, area.expanded(14, 8).translated(0, 1), justification, 1);
    g.setColour(innerGlowColour);
    g.drawFittedText(text, area.expanded(6, 3), justification, 1);
    g.setColour(coreColour);
    g.drawFittedText(text, area, justification, 1);
}

juce::String formatReadoutText(float value, int decimals, const juce::String& suffix)
{
    auto text = juce::String(value, juce::jmax(0, decimals));

    if (decimals > 0 && text.containsChar('.'))
    {
        while (text.endsWithChar('0'))
            text = text.dropLastCharacters(1);

        if (text.endsWithChar('.'))
            text = text.dropLastCharacters(1);
    }

    return text + suffix;
}

int measureTextWidth(const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, text, 0.0f, 0.0f);
    return juce::roundToInt(glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true).getWidth());
}

void drawPanelDepthOverlay(juce::Graphics& g,
                           juce::Rectangle<float> area,
                           float cornerRadius,
                           juce::Colour topHighlight,
                           juce::Colour bottomShadow)
{
    juce::ColourGradient topGlow(
        topHighlight,
        area.getCentreX(),
        area.getY() + area.getHeight() * 0.02f,
        juce::Colours::transparentBlack,
        area.getCentreX(),
        area.getY() + area.getHeight() * 0.36f,
        false
    );
    g.setGradientFill(topGlow);
    g.fillRoundedRectangle(area, cornerRadius);

    juce::ColourGradient bottomShade(
        juce::Colours::transparentBlack,
        area.getCentreX(),
        area.getY() + area.getHeight() * 0.42f,
        bottomShadow,
        area.getCentreX(),
        area.getBottom(),
        false
    );
    g.setGradientFill(bottomShade);
    g.fillRoundedRectangle(area, cornerRadius);
}

void drawPanelFinishOverlay(juce::Graphics& g,
                            juce::Rectangle<float> area,
                            float cornerRadius,
                            juce::Colour glowColour,
                            float glowAlpha,
                            float noiseAlpha)
{
    juce::Path clip;
    clip.addRoundedRectangle(area, cornerRadius);

    g.saveState();
    g.reduceClipRegion(clip);

    const float microTime = (float) juce::Time::getMillisecondCounterHiRes() * 0.00020f;
    const float drift = std::sin(microTime + area.getX() * 0.012f + area.getY() * 0.009f);
    const float driftSecondary = std::cos(microTime * 0.83f + area.getWidth() * 0.010f);
    const float glowShiftX = drift * juce::jmin(area.getWidth() * 0.010f, 6.0f);
    const float glowShiftY = driftSecondary * juce::jmin(area.getHeight() * 0.006f, 3.0f);
    const float animatedGlowAlpha = glowAlpha * (0.95f + 0.08f * (0.5f + 0.5f * std::sin(microTime * 0.76f + area.getWidth() * 0.013f)));

    juce::ColourGradient innerGlow(
        glowColour.withAlpha(animatedGlowAlpha),
        area.getCentreX() + glowShiftX,
        area.getY() + area.getHeight() * 0.20f + glowShiftY,
        juce::Colours::transparentBlack,
        area.getCentreX() - glowShiftX * 0.4f,
        area.getBottom(),
        false
    );
    innerGlow.addColour(0.48, glowColour.withAlpha(animatedGlowAlpha * 0.24f));
    g.setGradientFill(innerGlow);
    g.fillRoundedRectangle(area, cornerRadius);

    juce::ColourGradient topKiss(
        juce::Colours::white.withAlpha(animatedGlowAlpha * 0.12f),
        area.getCentreX() + glowShiftX * 0.35f,
        area.getY() + glowShiftY * 0.4f,
        juce::Colours::transparentBlack,
        area.getCentreX(),
        area.getY() + area.getHeight() * 0.16f,
        false
    );
    g.setGradientFill(topKiss);
    g.fillRoundedRectangle(area.reduced(0.8f), juce::jmax(0.0f, cornerRadius - 0.8f));

    const auto& panelNoise = getPanelNoiseImage();
    if (panelNoise.isValid() && noiseAlpha > 0.0f)
    {
        g.setTiledImageFill(panelNoise, area.getX() * 0.23f, area.getY() * 0.19f, 0.72f);
        g.setOpacity(noiseAlpha);
        g.fillRoundedRectangle(area, cornerRadius);
        g.setOpacity(1.0f);
    }

    g.restoreState();
}

struct SectionAccent
{
    float accentHue = 0.33f;
    juce::Colour bodyTint;
    juce::Colour panelGlow;
    juce::Colour lineGlow;
    juce::Colour lineCore;
    juce::Colour textCore;
    juce::Colour textGlow;
    juce::Colour readoutCore;
    juce::Colour readoutGlow;
    juce::Colour labelColour;
};

SectionAccent getSectionAccent(const juce::String& sectionTag)
{
    if (sectionTag == "ROOT" || sectionTag == "root")
    {
        return {
            0.205f,
            juce::Colour(48, 36, 18),
            juce::Colour(186, 166, 96),
            juce::Colour(210, 192, 112),
            juce::Colour(242, 230, 180),
            juce::Colour(245, 238, 204),
            juce::Colour(208, 188, 112),
            juce::Colour(238, 226, 180),
            juce::Colour(206, 178, 104),
            juce::Colour(238, 224, 182)
        };
    }

    if (sectionTag == "PULSE" || sectionTag == "pulse")
    {
        return {
            0.125f,
            juce::Colour(52, 38, 14),
            juce::Colour(224, 182, 84),
            juce::Colour(245, 214, 118),
            juce::Colour(255, 242, 194),
            juce::Colour(250, 240, 214),
            juce::Colour(238, 206, 108),
            juce::Colour(246, 234, 182),
            juce::Colour(230, 196, 98),
            juce::Colour(242, 228, 184)
        };
    }

    if (sectionTag == "SEASONS" || sectionTag == "seasons")
    {
        return {
            0.165f,
            juce::Colour(54, 42, 20),
            juce::Colour(228, 194, 112),
            juce::Colour(242, 214, 142),
            juce::Colour(252, 242, 196),
            juce::Colour(248, 240, 214),
            juce::Colour(236, 206, 124),
            juce::Colour(246, 234, 188),
            juce::Colour(228, 194, 106),
            juce::Colour(242, 228, 184)
        };
    }

    if (sectionTag == "ATMOS" || sectionTag == "atmos")
    {
        return {
            0.455f,
            juce::Colour(16, 34, 30),
            juce::Colour(118, 232, 214),
            juce::Colour(164, 244, 232),
            juce::Colour(214, 250, 246),
            juce::Colour(222, 246, 240),
            juce::Colour(136, 238, 218),
            juce::Colour(202, 250, 236),
            juce::Colour(126, 230, 212),
            juce::Colour(196, 242, 228)
        };
    }

    if (sectionTag == "INSTABILITY" || sectionTag == "instability")
    {
        return {
            0.095f,
            juce::Colour(44, 28, 18),
            juce::Colour(255, 178, 112),
            juce::Colour(255, 214, 154),
            juce::Colour(255, 238, 202),
            juce::Colour(250, 238, 214),
            juce::Colour(244, 186, 118),
            juce::Colour(252, 232, 188),
            juce::Colour(240, 176, 106),
            juce::Colour(244, 222, 186)
        };
    }

    return {
        0.395f,
        juce::Colour(16, 42, 28),
        juce::Colour(92, 214, 162),
        juce::Colour(122, 248, 188),
        juce::Colour(206, 255, 228),
        juce::Colour(234, 250, 238),
        juce::Colour(132, 255, 194),
        juce::Colour(198, 255, 222),
        juce::Colour(116, 255, 188),
        juce::Colour(194, 244, 214)
    };
}

SectionAccent getSliderAccent(const juce::Slider& slider)
{
    return getSectionAccent(slider.getProperties()["rfAccentTag"].toString());
}

juce::Colour getStatusReadyColour() noexcept
{
    return juce::Colour(154, 236, 178);
}

juce::Colour getStatusMapColour() noexcept
{
    return juce::Colour(244, 204, 128);
}

juce::Colour getStatusInfoColour() noexcept
{
    return juce::Colour(166, 222, 214);
}

juce::Colour getStatusWarnColour() noexcept
{
    return juce::Colour(244, 184, 136);
}
}





RootFlowAudioProcessorEditor::RootFlowAudioProcessorEditor(RootFlowAudioProcessor& p)
    : juce::AudioProcessorEditor(p),
      audioProcessor(p),
      keyboardDrawer(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard),
      visualizer(p)
{

    setLookAndFeel(&look);
    setOpaque(true);
    setResizable(true, true);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    setResizeLimits(400, 500, 1600, 2000);
    updateAnimationTimerState();

    // Initial hardware setup using standardized helpers
    auto setup = [&](juce::Slider& s, juce::Label& l, const juce::String& name, std::unique_ptr<Attachment>& att, const juce::String& paramID, bool showVal) {
        setupKnob(s, l, name, showVal);
        s.getProperties().set("rfParamID", paramID);
        att = std::make_unique<Attachment>(audioProcessor.tree, paramID, s);
    };

    setup(rootDepthSlider, rootDepthLabel, "DEPTH", rootDepthAtt, "rootDepth", true);
    setup(rootSoilSlider, rootSoilLabel, "SOIL", rootSoilAtt, "rootSoil", true);
    setup(rootAnchorSlider, rootAnchorLabel, "ANCHOR", rootAnchorAtt, "rootAnchor", true);
    
    setup(sapFlowSlider, sapFlowLabel, "FLOW", sapFlowAtt, "sapFlow", true);
    setup(sapVitalitySlider, sapVitalityLabel, "VITALITY", sapVitalityAtt, "sapVitality", true);
    setup(sapTextureSlider, sapTextureLabel, "TEXTURE", sapTextureAtt, "sapTexture", true);
    
    setup(pulseRateSlider, pulseRateLabel, "RATE", pulseRateAtt, "pulseRate", true);
    setup(pulseBreathSlider, pulseBreathLabel, "BREATH", pulseBreathAtt, "pulseBreath", true);
    setup(pulseGrowthSlider, pulseGrowthLabel, "GROWTH", pulseGrowthAtt, "pulseGrowth", true);

    setup(canopySlider, canopyLabel, "OPEN", canopyAtt, "canopy", true);
    setup(instabilitySlider, instabilityLabel, "DRIFT", instabilityAtt, "instability", false);
    setup(atmosSlider, atmosLabel, "ATMOS", atmosAtt, "atmosphere", false);
    setup(seasonsSlider, seasonsLabel, "SEASONS", seasonsAtt, "ecoSystem", false);

    setup(bloomSlider, bloomLabel, "SOIL", bloomAtt, "bloom", false);
    setup(rainSlider, rainLabel, "AIR", rainAtt, "rain", false);
    setup(sunSlider, sunLabel, "SPACE", sunAtt, "sun", false);
    instabilitySlider.getProperties().set("rfStyle", "macro");
    atmosSlider.getProperties().set("rfStyle", "macro");
    seasonsSlider.getProperties().set("rfStyle", "macro");
    bloomSlider.getProperties().set("rfStyle", "fx");
    rainSlider.getProperties().set("rfStyle", "fx");
    sunSlider.getProperties().set("rfStyle", "fx");

    auto applySectionAccent = [](juce::Slider& slider, juce::Label& label, const juce::String& sectionTag)
    {
        const auto accent = getSectionAccent(sectionTag);
        slider.getProperties().set("rfAccentTag", sectionTag);
        slider.getProperties().set("rfAccentHue", accent.accentHue);
        label.setColour(juce::Label::textColourId, accent.labelColour.withAlpha(0.95f));
    };

    applySectionAccent(rootDepthSlider, rootDepthLabel, "ROOT");
    applySectionAccent(rootSoilSlider, rootSoilLabel, "ROOT");
    applySectionAccent(rootAnchorSlider, rootAnchorLabel, "ROOT");
    applySectionAccent(sapFlowSlider, sapFlowLabel, "SAP");
    applySectionAccent(sapVitalitySlider, sapVitalityLabel, "SAP");
    applySectionAccent(sapTextureSlider, sapTextureLabel, "SAP");
    applySectionAccent(pulseRateSlider, pulseRateLabel, "PULSE");
    applySectionAccent(pulseBreathSlider, pulseBreathLabel, "PULSE");
    applySectionAccent(pulseGrowthSlider, pulseGrowthLabel, "PULSE");
    applySectionAccent(canopySlider, canopyLabel, "CANOPY");
    applySectionAccent(instabilitySlider, instabilityLabel, "INSTABILITY");
    applySectionAccent(atmosSlider, atmosLabel, "ATMOS");
    applySectionAccent(seasonsSlider, seasonsLabel, "SEASONS");
    canopyLabel.setVisible(false);
    canopySlider.setTooltip("Open the canopy layer for more width, air and shimmer");
    instabilityLabel.setVisible(false);
    instabilitySlider.setTooltip("Scale voice drift, wander and detune with subtle per-note variation");
    atmosLabel.setVisible(false);
    atmosSlider.setTooltip("Blend the atmosphere from seed air to wide weather");
    seasonsLabel.setVisible(false);
    seasonsSlider.setTooltip("Morph the ecosystem between Spring, Summer, Autumn and Winter");
    bloomLabel.setVisible(false);
    bloomSlider.setTooltip("Add heavy soil resonance, dark warmth and earth-bound depth");
    rainLabel.setVisible(false);
    rainSlider.setTooltip("Add moving air, wind-swept delays and leaf-motion breath");
    sunLabel.setVisible(false);
    sunSlider.setTooltip("Open the cosmic space with wide shimmer and blooming light");
    rootDepthSlider.setTooltip("Add body, low-end weight and depth");
    rootSoilSlider.setTooltip("Increase earthy tone and low-mid saturation");
    rootAnchorSlider.setTooltip("Tighten the voice around a darker, steadier center");
    sapFlowSlider.setTooltip("Increase organic motion and internal flow");
    sapVitalitySlider.setTooltip("Brighten the core tone and harmonic life");
    sapTextureSlider.setTooltip("Roughen the surface with more grain and texture");
    pulseRateSlider.setTooltip("Set the speed of the pulse motion");
    pulseBreathSlider.setTooltip("Increase pulse depth and filter sweep");
    pulseGrowthSlider.setTooltip("Add pulse energy, spread and agitation");

    keyboardDrawer.setAvailableRange(36, 84);
    keyboardDrawer.setLowestVisibleKey(36);
    keyboardDrawer.setMidiChannel(1);
    keyboardDrawer.setScrollButtonsVisible(false);
    keyboardDrawer.setWantsKeyboardFocus(false);
    keyboardDrawer.setMouseClickGrabsKeyboardFocus(false);
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(228, 216, 188));
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(26, 22, 20));
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(52, 34, 22).withAlpha(0.46f));
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(116, 255, 164).withAlpha(0.22f));
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(168, 255, 188).withAlpha(0.52f));
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::textLabelColourId, juce::Colour(52, 34, 20).withAlpha(0.72f));
    keyboardDrawer.setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha(0.28f));
    keyboardDrawer.setVisible(false);
    addAndMakeVisible(keyboardDrawer);

    keyboardDrawerButton.setButtonText("KEYS");
    keyboardDrawerButton.setTriggeredOnMouseDown(false);
    keyboardDrawerButton.setClickingTogglesState(false);
    keyboardDrawerButton.setColour(juce::TextButton::buttonColourId, juce::Colour(56, 38, 24).withAlpha(0.86f));
    keyboardDrawerButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(88, 58, 32).withAlpha(0.92f));
    keyboardDrawerButton.setColour(juce::TextButton::textColourOffId, juce::Colour(230, 255, 224).withAlpha(0.92f));
    keyboardDrawerButton.setColour(juce::TextButton::textColourOnId, juce::Colour(252, 255, 244));
    keyboardDrawerButton.onClick = [this] { toggleKeyboardDrawer(); };
    keyboardDrawerButton.setVisible(isKeyboardDrawerAvailable());
    addAndMakeVisible(keyboardDrawerButton);

    presetBox.setTextWhenNothingSelected("PRESET");
    presetBox.setJustificationType(juce::Justification::centredLeft);
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack); // Transparent to show custom drawHeader background
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colour(236, 248, 222).withAlpha(0.96f));
    presetBox.setColour(juce::ComboBox::arrowColourId, juce::Colour(186, 255, 198).withAlpha(0.84f));
    presetBox.onChange = [this]
    {
        const int presetIndex = presetBox.getSelectedId() - 1;
        if (presetIndex < 0)
            return;

        audioProcessor.applyCombinedPreset(presetIndex);
        setMidiStatusMessage("PRESET " + presetBox.getText().toUpperCase(),
                             getStatusReadyColour());
        midiIndicatorLevel = juce::jmax(midiIndicatorLevel, 0.42f);
        refreshHeaderControlState();
    };
    addAndMakeVisible(presetBox);
    presetBox.setTooltip("Factory and user presets");

    auto setupHeaderButton = [](juce::TextButton& button, const juce::String& text, juce::Colour colour)
    {
        button.setButtonText(text);
        button.setTriggeredOnMouseDown(false);
        button.setClickingTogglesState(false);
        button.setColour(juce::TextButton::buttonColourId, colour);
        button.setColour(juce::TextButton::buttonOnColourId, colour.brighter(0.18f));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(232, 255, 226).withAlpha(0.94f));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(252, 255, 246));
    };

    setupHeaderButton(midiLearnButton, "MAP", juce::Colour(62, 40, 22).withAlpha(0.88f));
    midiLearnButton.onClick = [this]
    {
        const bool nextState = ! audioProcessor.isMidiLearnArmed();
        audioProcessor.setMidiLearnArmed(nextState);
        setMidiStatusMessage(nextState ? "MIDI MAP ARMED" : "MIDI MAP OFF",
                             nextState ? getStatusMapColour()
                                       : getStatusReadyColour());
        midiIndicatorLevel = juce::jmax(midiIndicatorLevel, nextState ? 0.50f : 0.28f);
        refreshHeaderControlState();
    };
    addAndMakeVisible(midiLearnButton);
    midiLearnButton.setTooltip("Arm MIDI mapping for the next touched knob");

    setupHeaderButton(mpkDefaultButton, "MPK", juce::Colour(48, 34, 18).withAlpha(0.84f));
    mpkDefaultButton.onClick = [this]
    {
        audioProcessor.resetToDefaultMpkMiniMappings();
        setMidiStatusMessage("MPK MAP LOADED",
                             getStatusMapColour());
        midiIndicatorLevel = juce::jmax(midiIndicatorLevel, 0.44f);
        refreshHeaderControlState();
    };
    addAndMakeVisible(mpkDefaultButton);
    mpkDefaultButton.setTooltip("Load the default Akai MPK mini knob mapping");

    setupHeaderButton(midiClearButton, "UNMAP", juce::Colour(44, 26, 18).withAlpha(0.84f));
    midiClearButton.onClick = [this]
    {
        audioProcessor.clearMidiMappings();
        setMidiStatusMessage("MIDI MAP CLEARED",
                             getStatusWarnColour());
        midiIndicatorLevel = juce::jmax(midiIndicatorLevel, 0.40f);
        refreshHeaderControlState();
    };
    addAndMakeVisible(midiClearButton);
    midiClearButton.setTooltip("Clear all MIDI mappings");

    setupHeaderButton(testToneButton, "TEST", juce::Colour(46, 30, 18).withAlpha(0.84f));
    testToneButton.setClickingTogglesState(true);
    testToneButton.onClick = [this]
    {
        const bool enabled = testToneButton.getToggleState();
        audioProcessor.setTestToneEnabled(enabled);
        setMidiStatusMessage(enabled ? "TEST TONE ON" : "TEST TONE OFF",
                             enabled ? getStatusMapColour()
                                     : getStatusReadyColour());
        midiIndicatorLevel = juce::jmax(midiIndicatorLevel, enabled ? 0.38f : 0.24f);
        refreshHeaderControlState();
    };
    addAndMakeVisible(testToneButton);
    testToneButton.setTooltip("Toggle a built-in 440 Hz output test tone");

    setupHeaderButton(presetSaveButton, "SAVE", juce::Colour(58, 38, 20).withAlpha(0.88f));
    presetSaveButton.onClick = [this]
    {
        audioProcessor.saveCurrentStateAsUserPreset();
        setMidiStatusMessage("PRESET SAVED",
                             getStatusReadyColour());
        midiIndicatorLevel = juce::jmax(midiIndicatorLevel, 0.46f);
        refreshHeaderControlState();
    };
    addAndMakeVisible(presetSaveButton);
    presetSaveButton.setTooltip("Save current settings as user preset");

    setupHeaderButton(presetDeleteButton, "DEL", juce::Colour(42, 24, 18).withAlpha(0.84f));
    presetDeleteButton.onClick = [this]
    {
        audioProcessor.deleteCurrentUserPreset();
        setMidiStatusMessage("PRESET DELETED",
                             getStatusWarnColour());
        midiIndicatorLevel = juce::jmax(midiIndicatorLevel, 0.34f);
        refreshHeaderControlState();
    };
    addAndMakeVisible(presetDeleteButton);
    presetDeleteButton.setTooltip("Delete selected user preset");

    addAndMakeVisible(visualizer);
    visualizer.setInterceptsMouseClicks(false, false);
    visualizer.setAlpha(0.92f);

    auto setupVizButton = [](juce::TextButton& b, const juce::String& tip) {
        b.setButtonText("");
        b.setTooltip(tip);
        b.setAlpha(0.0f); // Hidden but clickable
        b.setAlwaysOnTop(true);
    };

    addAndMakeVisible(vizModeButton);
    setupVizButton(vizModeButton, "Switch Visualizer Species");
    vizModeButton.onClick = [this] { visualizer.cycleDisplayMode(); };

    addAndMakeVisible(vizColorButton);
    setupVizButton(vizColorButton, "Switch Growth Cycle Colors");
    vizColorButton.onClick = [this] { visualizer.cycleColorPalette(); };

    rootDepthSlider.setTextValueSuffix("%");
    rootSoilSlider.setTextValueSuffix("%");
    rootAnchorSlider.setTextValueSuffix("%");
    sapFlowSlider.setTextValueSuffix("%");
    sapVitalitySlider.setTextValueSuffix("%");
    sapTextureSlider.setTextValueSuffix("%");
    pulseRateSlider.setTextValueSuffix(" Hz");
    pulseRateSlider.setNumDecimalPlacesToDisplay(1);
    pulseBreathSlider.setTextValueSuffix("%");
    pulseGrowthSlider.setTextValueSuffix("%");
    canopySlider.setTextValueSuffix("%");
    instabilitySlider.setTextValueSuffix("%");
    rainSlider.setNumDecimalPlacesToDisplay(1);



    loadAssets();

    // --- MUTATION BUTTON SETUP ---
    mutateButton.setButtonText("MUTATE");
    mutateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(15, 22, 15));
    mutateButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff22ff66));
    mutateButton.setLookAndFeel(&look);
    addAndMakeVisible(mutateButton);
    mutateButton.toFront(false);

    mutateButton.onClick = [this] {
        auto& random = juce::Random::getSystemRandom();
        auto randomizeParam = [&](const juce::String& name) {
            if (auto* p = audioProcessor.tree.getRawParameterValue(name)) {
                *p = random.nextFloat();
            }
        };

        // Mutate DNA
        randomizeParam("rootDepth"); randomizeParam("rootSoil"); randomizeParam("rootAnchor");
        randomizeParam("sapFlow"); randomizeParam("sapVitality"); randomizeParam("sapTexture");
        randomizeParam("pulseRate"); randomizeParam("pulseBreath"); randomizeParam("pulseGrowth");
        
        // Environment
        randomizeParam("ecoSystem"); 
        randomizeParam("bloom"); 
        randomizeParam("rain"); 
        randomizeParam("sun");

        setMidiStatusMessage("BIO-DNA MUTATED: NEW GROWTH DETECTED", getStatusInfoColour());
        midiIndicatorLevel = juce::jmax(midiIndicatorLevel, 0.6f);
    };

    // Waveform Switch Setup
    waveSelector.addItemList({ "SINE", "SAW", "PULSE" }, 1);
    waveSelector.setSelectedId(1, juce::dontSendNotification);
    waveSelector.setJustificationType(juce::Justification::centred);
    waveSelector.setLookAndFeel(&look);
    addAndMakeVisible(waveSelector);

    waveAttachment = std::make_unique<ComboAttachment>(audioProcessor.tree, "oscWave", waveSelector);

    // --- BIO-SEQUENCER SETUP ---
    setupHeaderButton(seqToggle, "SEQ Off", juce::Colour(25, 45, 25));
    seqToggle.setClickingTogglesState(true);
    addAndMakeVisible(seqToggle);
    seqOnAtt = std::make_unique<ButtonAttachment>(audioProcessor.tree, "sequencerOn", seqToggle);
    seqToggle.onStateChange = [this] {
        seqToggle.setButtonText(seqToggle.getToggleState() ? "SEQ On" : "SEQ Off");
        repaint();
    };

    seqRateSelector.addItemList({ "1/4", "1/8", "1/16", "1/32" }, 1);
    seqRateSelector.setLookAndFeel(&look);
    addAndMakeVisible(seqRateSelector);
    seqRateAtt = std::make_unique<ComboAttachment>(audioProcessor.tree, "sequencerRate", seqRateSelector);

    seqStepsSelector.addItemList({ "4 Steps", "8 Steps", "12 Steps", "16 Steps" }, 1);
    seqStepsSelector.setLookAndFeel(&look);
    addAndMakeVisible(seqStepsSelector);
    seqStepsSelector.onChange = [this] {
        if (auto* p = audioProcessor.tree.getRawParameterValue("sequencerSteps"))
            *p = (float)((seqStepsSelector.getSelectedItemIndex() + 1) * 4);
    };

    setSize(950, 700);
    refreshHeaderControlState();
}

RootFlowAudioProcessorEditor::~RootFlowAudioProcessorEditor() { setLookAndFeel(nullptr); }

void RootFlowAudioProcessorEditor::visibilityChanged()
{
    updateAnimationTimerState();

    if (! isShowing())
        releaseVirtualKeyboardNotes();
}

void RootFlowAudioProcessorEditor::parentHierarchyChanged()
{
    updateAnimationTimerState();
}

bool RootFlowAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (! isVirtualKeyboardInputEnabled())
        return false;

    const auto pressed = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());

    for (size_t i = 0; i < virtualKeyboardCharacters.size(); ++i)
    {
        if (pressed != virtualKeyboardCharacters[i])
            continue;

        if (! virtualKeyDownStates[i])
        {
            virtualKeyDownStates[i] = true;
            audioProcessor.getKeyboardState().noteOn(1, virtualKeyboardBaseMidiNote + (int) i, (juce::uint8) 100);
        }

        return true;
    }

    return false;
}

bool RootFlowAudioProcessorEditor::keyStateChanged(bool)
{
    if (! isVirtualKeyboardInputEnabled())
    {
        releaseVirtualKeyboardNotes();
        return false;
    }

    updateVirtualKeyboardState();
    return false;
}

void RootFlowAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (isVirtualKeyboardInputEnabled())
        grabKeyboardFocus();

    auto localPos = e.getPosition();
    if (sequencerRect.contains(localPos))
    {
        const int numSteps = 16;
        float stepW = (float)sequencerRect.getWidth() / (float)numSteps;
        int step = (int)((localPos.getX() - sequencerRect.getX()) / stepW);
        step = juce::jlimit(0, numSteps - 1, step);
        
        audioProcessor.sequencerSteps[(size_t)step].active = !audioProcessor.sequencerSteps[(size_t)step].active;
        repaint();
    }
}

void RootFlowAudioProcessorEditor::loadAssets()
{
    const auto& cachedAssets = getCachedEditorAssets();
    woodBackground = cachedAssets.woodBackground;
    rootsBackground = cachedAssets.rootsBackground;
    rootsOverlaySoft = cachedAssets.rootsOverlaySoft;
    rootsOverlayDetail = cachedAssets.rootsOverlayDetail;
    rootsOverlayDetailBlurred = cachedAssets.rootsOverlayDetailBlurred;
    mooseLogo = cachedAssets.mooseLogo;


    look.setWoodImage(woodBackground);
}

void RootFlowAudioProcessorEditor::drawPresetMenu(juce::Graphics& g, juce::Rectangle<int> area)
{
    const float energy = audioProcessor.getPlantEnergy();
    
    // 1. Der "Glas"-Hintergrund für den Namen
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(area.toFloat(), 4.0f);
    
    // 2. Ein dezentes Leuchten passend zur Energie
    if (energy > 0.05f)
    {
        g.setColour(juce::Colour(80, 255, 150).withAlpha(energy * 0.2f));
        g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 4.0f, 1.5f);
    }

    // 3. Blatt-Icon links neben dem Namen
    auto leafArea = area.removeFromLeft(area.getHeight()).reduced(6).toFloat();
    g.setColour(juce::Colour(120, 220, 150).withAlpha(0.8f + energy * 0.2f));
    
    juce::Path leaf;
    leaf.startNewSubPath(leafArea.getCentreX(), leafArea.getBottom());
    leaf.quadraticTo(leafArea.getRight(), leafArea.getCentreY(), leafArea.getCentreX(), leafArea.getY());
    leaf.quadraticTo(leafArea.getX(), leafArea.getCentreY(), leafArea.getCentreX(), leafArea.getBottom());
    g.fillPath(leaf);
}

void RootFlowAudioProcessorEditor::drawSequencerPanel(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto fArea = area.toFloat();
    const float cornerRadius = 9.0f;
    
    // Wood Background
    drawProceduralWoodPanel(g, fArea, cornerRadius, 
                            juce::Colour(45, 34, 28),
                            juce::Colour(30, 22, 18), 
                            0.18f);

    // Varnish & Glow
    g.setColour(juce::Colour(110, 255, 140).withAlpha(0.04f));
    g.fillRoundedRectangle(fArea.reduced(1.0f), cornerRadius);
    
    // Frame
    g.setColour(juce::Colour(25, 18, 12));
    g.drawRoundedRectangle(fArea, cornerRadius, 2.5f);
    
    g.setColour(juce::Colour(85, 75, 65).withAlpha(0.4f));
    g.drawRoundedRectangle(fArea.reduced(2.0f), cornerRadius - 1.0f, 1.0f);
    
    // Label
    g.setColour(juce::Colour(110, 255, 140).withAlpha(0.45f));
    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.drawText("BIO-SEQUENCER", area.withTrimmedLeft(18).withHeight(20), juce::Justification::left);
}

void RootFlowAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawWoodFrame(g, bounds);

    if (rootsBackground.isValid())
    {
        const int imgW = rootsBackground.getWidth();
        const int imgH = rootsBackground.getHeight();

        const int cW = (int) (getWidth() * 0.42f);
        const int cH = (int) (getHeight() * 0.24f);
        const int driftX = juce::roundToInt(std::sin(rootsDriftPhase * 0.65f) * 14.0f);
        const int driftY = juce::roundToInt(std::cos(rootsDriftPhase * 0.41f) * 8.0f);

        g.setOpacity(0.12f);
        g.drawImage(rootsBackground,
            -18 + driftX, -14 + driftY, cW, cH,
            0, 0, imgW / 2, imgH / 2);

        g.setOpacity(0.10f);
        g.drawImage(rootsBackground,
            getWidth() - cW + 18 - driftX, -10 - driftY, cW, cH,
            imgW / 2, 0, imgW / 2, imgH / 2);
    }

    if (rootsOverlayDetailBlurred.isValid())
    {
        const int imgW = rootsOverlayDetailBlurred.getWidth();
        const int imgH = rootsOverlayDetailBlurred.getHeight();
        const int cW = (int) (getWidth() * 0.42f);
        const int cH = (int) (getHeight() * 0.24f);

        g.setOpacity(0.18f);
        g.drawImage(rootsOverlayDetailBlurred,
            -10, -10, cW, cH,
            0, 0, imgW / 2, imgH / 2);

        g.setOpacity(0.15f);
        g.drawImage(rootsOverlayDetailBlurred,
            getWidth() - cW + 10, -10, cW, cH,
            imgW / 2, 0, imgW / 2, imgH / 2);

        g.setOpacity(1.0f);
    }

    drawMainPanel(g, innerRect);
    drawHeader(g, headerBoardRect);
    drawSequencerPanel(g, sequencerBoardRect);
    drawSectionPanel(g, rootRect, "ROOT");
    drawSectionPanel(g, sapRect, "SAP");
    drawSectionPanel(g, pulseRect, "PULSE");

    auto drawSectionAura = [&](juce::Rectangle<int> sectionRect,
                               const juce::String& sectionTag,
                               std::initializer_list<juce::Slider*> sliders)
    {
        const auto accent = getSectionAccent(sectionTag);
        auto auraArea = sectionRect.toFloat().reduced(6.0f, 10.0f);
        auraArea.removeFromTop(18.0f);

        juce::Path clip;
        clip.addRoundedRectangle(auraArea, 12.0f);
        g.saveState();
        g.reduceClipRegion(clip);

        juce::Rectangle<float> groupArea;
        bool hasSliderBounds = false;

        for (auto* slider : sliders)
        {
            if (slider == nullptr)
                continue;

            const auto boostVar = slider->getProperties()["rfInteractionBoost"];
            const float interactionBoost = boostVar.isVoid() ? 1.0f : (float) (double) boostVar;
            auto knobBounds = slider->getBounds().toFloat();
            auto localGroup = knobBounds.expanded(knobBounds.getWidth() * 0.30f,
                                                  knobBounds.getHeight() * 0.24f)
                                      .translated(0.0f, knobBounds.getHeight() * 0.06f);

            groupArea = hasSliderBounds ? groupArea.getUnion(localGroup) : localGroup;
            hasSliderBounds = true;

            auto broadAura = knobBounds.expanded(knobBounds.getWidth() * (0.26f + (interactionBoost - 1.0f) * 0.40f),
                                                 knobBounds.getHeight() * (0.20f + (interactionBoost - 1.0f) * 0.32f))
                                     .translated(0.0f, knobBounds.getHeight() * 0.05f);
            auto focusedAura = knobBounds.expanded(knobBounds.getWidth() * (0.12f + (interactionBoost - 1.0f) * 0.22f),
                                                   knobBounds.getHeight() * (0.10f + (interactionBoost - 1.0f) * 0.18f))
                                       .translated(0.0f, knobBounds.getHeight() * 0.03f);

            g.setColour(accent.panelGlow.withAlpha(0.028f * interactionBoost * 1.9f));
            g.fillEllipse(broadAura);
            g.setColour(accent.lineGlow.withAlpha(0.040f * interactionBoost * 2.1f));
            g.fillEllipse(focusedAura);
        }

        if (hasSliderBounds)
        {
            auto fieldArea = groupArea.expanded(groupArea.getWidth() * 0.06f,
                                                groupArea.getHeight() * 0.08f);
            const float radius = juce::jmin(fieldArea.getWidth(), fieldArea.getHeight()) * 0.18f;

            juce::ColourGradient fieldWash(
                accent.panelGlow.withAlpha(0.026f),
                fieldArea.getCentreX(),
                fieldArea.getY() + fieldArea.getHeight() * 0.24f,
                juce::Colours::transparentBlack,
                fieldArea.getCentreX(),
                fieldArea.getBottom(),
                false
            );
            fieldWash.addColour(0.48, accent.panelGlow.withAlpha(0.010f));
            g.setGradientFill(fieldWash);
            g.fillRoundedRectangle(fieldArea, radius);
        }

        g.restoreState();
    };

    // --- BIO-SEQUENCER GRID DRAWING ---
    if (sequencerRect.getWidth() > 0)
    {
        g.saveState();
        const float energy = smoothedUiEnergy;
        const int numSteps = 16;
        float stepW = (float)sequencerRect.getWidth() / (float)numSteps;
        auto drawArea = sequencerRect.toFloat();
        
        for (int i = 0; i < numSteps; ++i)
        {
            auto r = drawArea.removeFromLeft(stepW).reduced(2.0f);
            const bool isActive = audioProcessor.sequencerSteps[(size_t)i].active;
            const bool isCurrent = (i == audioProcessor.currentSequencerStep) && (audioProcessor.isSequencerEnabled());
            
            juce::Colour cellCol = juce::Colour(55, 120, 75).withAlpha(0.25f);
            if (isActive) 
                cellCol = juce::Colour(110, 255, 140).withAlpha(0.58f + energy * 0.38f);
            
            if (isCurrent)
            {
                g.setColour(juce::Colours::white.withAlpha(0.92f + energy * 0.08f));
                g.fillRoundedRectangle(r, 2.5f);
            }
            else
            {
                g.setColour(cellCol);
                g.fillRoundedRectangle(r, 2.5f);
                
                if (isActive)
                {
                    g.setColour(cellCol.brighter(0.2f).withAlpha(0.6f));
                    g.drawRoundedRectangle(r, 2.5f, 1.3f);
                }
            }
        }
        g.restoreState();
    }

    drawSectionAura(rootRect, "ROOT", { &rootDepthSlider, &rootSoilSlider, &rootAnchorSlider });
    drawSectionAura(sapRect, "SAP", { &sapFlowSlider, &sapVitalitySlider, &sapTextureSlider });
    drawSectionAura(pulseRect, "PULSE", { &pulseRateSlider, &pulseBreathSlider, &pulseGrowthSlider });

    drawCanopyPod(g, canopyRect);
    drawInstabilityPod(g, instabilityRect);

    // --- BIO-GLOW IM GEHÄUSE ---
    // Ein sehr subtiles Leuchten, das den Visualizer umrahmt
    float energy = audioProcessor.getPlantEnergy();
    if (energy > 0.05f)
    {
        auto glowArea = visualizerFrameRect.expanded(juce::roundToInt(40.0f * (getWidth() / 1100.0f)));
        juce::ColourGradient glow (juce::Colour(0, 200, 100).withAlpha(energy * 0.12f), 
                                   (float)visualizerFrameRect.getCentreX(), (float)visualizerFrameRect.getCentreY(),
                                   juce::Colours::transparentBlack, 
                                   (float)visualizerFrameRect.getCentreX(), (float)visualizerFrameRect.getY() - 40.0f, true);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(glowArea.toFloat(), 12.0f);
    }

    drawVisualizerHousing(g, visualizerFrameRect);
    drawFxModule(g, bloomRect, "BLOOM");
    drawFxModule(g, rainRect, "RAIN");
    drawFxModule(g, sunRect, "SUN");
    drawAmbientPulse(g, innerRect.toFloat().reduced(6.0f), 16.0f);

    auto drawColumnValues = [&](juce::Slider& slider, float value, const juce::String& suffix, int decimals)
    {
        const auto accent = getSliderAccent(slider);
        const auto boostVar = slider.getProperties()["rfInteractionBoost"];
        const float interactionBoost = boostVar.isVoid() ? 1.0f : (float) (double) boostVar;
        auto knobBounds = slider.getBounds();
        auto readout = knobBounds.withY(knobBounds.getBottom() + juce::roundToInt(10.0f))
                                .withHeight(22)
                                .translated(0, 20);
        drawValueReadout(g,
                         value,
                         readout,
                         suffix,
                         decimals,
                         accent.readoutCore.withAlpha(juce::jmin(1.0f, 0.92f + (interactionBoost - 1.0f) * 0.40f)),
                         accent.readoutGlow.withAlpha(0.18f * (1.0f + (interactionBoost - 1.0f) * 2.6f)));
    };

    drawColumnValues(rootDepthSlider, rootDepthSlider.getValue() * 100.0f, "%", 0);
    drawColumnValues(rootSoilSlider, rootSoilSlider.getValue() * 100.0f, "%", 0);
    drawColumnValues(rootAnchorSlider, rootAnchorSlider.getValue() * 100.0f, "%", 0);
    drawColumnValues(pulseRateSlider, 0.2f + pulseRateSlider.getValue() * 5.8f, " Hz", 1);
    drawColumnValues(pulseBreathSlider, pulseBreathSlider.getValue() * 100.0f, "%", 0);
    drawColumnValues(pulseGrowthSlider, pulseGrowthSlider.getValue() * 100.0f, "%", 0);

    if (keyboardDrawerOpenAmount > 0.001f)
        drawKeyboardDrawer(g, keyboardDrawerRect);
}

void RootFlowAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const float w = static_cast<float>(bounds.getWidth());
    const float h = static_cast<float>(bounds.getHeight());
    
    // --- DIE GOLDENE SKALIERUNG (Reference: 950x700) ---
    float scaleW = w / 950.0f;
    float scaleH = h / 700.0f;
    float scale = juce::jmin(scaleW, scaleH);

    const int frameInset = juce::roundToInt(10.0f * scale);
    innerRect = bounds.reduced(frameInset);

    // --- 1. HEADER (Shifted down slightly to avoid cutoff) ---
    headerBoardRect = juce::Rectangle<int>(innerRect.getX(), innerRect.getY() + juce::roundToInt(10.0f * scale), innerRect.getWidth(), juce::roundToInt(108.0f * scale));

    // --- 2. SEQUENCER ---
    sequencerBoardRect = juce::Rectangle<int>(innerRect.getX(), headerBoardRect.getBottom() + juce::roundToInt(6.0f * scale), innerRect.getWidth(), juce::roundToInt(52.0f * scale));

    // --- 3. MAIN AREA ---
    mainFieldRect = juce::Rectangle<int>(innerRect.getX(), sequencerBoardRect.getBottom() + juce::roundToInt(10.0f * scale), innerRect.getWidth(), juce::roundToInt(innerRect.getHeight() * 0.54f));

    const int knobSize = juce::roundToInt(72.0f * scale); 
    const int labelHeight = 18; 

    const int sideWidth = knobSize + juce::roundToInt(40.0f * scale);
    rootRect = { mainFieldRect.getX() + juce::roundToInt(12.0f * scale), mainFieldRect.getY(), sideWidth, mainFieldRect.getHeight() };
    pulseRect = { mainFieldRect.getRight() - sideWidth - juce::roundToInt(12.0f * scale), mainFieldRect.getY(), sideWidth, mainFieldRect.getHeight() };

    auto layoutColumn = [&](juce::Rectangle<int> area, juce::Slider* sliders[], juce::Label* labels[]) {
        int spacing = (area.getHeight() - (knobSize * 3)) / 4;
        for (int i = 0; i < 3; ++i) {
            int y = area.getY() + spacing + i * (knobSize + spacing);
            sliders[i]->setBounds(area.getCentreX() - knobSize/2, y, knobSize, knobSize);
            labels[i]->setBounds(area.getX(), sliders[i]->getBottom() - 1, area.getWidth(), labelHeight);
        }
    };
    layoutColumn(rootRect, (juce::Slider*[]){&rootDepthSlider, &rootSoilSlider, &rootAnchorSlider}, (juce::Label*[]){&rootDepthLabel, &rootSoilLabel, &rootAnchorLabel});
    layoutColumn(pulseRect, (juce::Slider*[]){&pulseRateSlider, &pulseBreathSlider, &pulseGrowthSlider}, (juce::Label*[]){&pulseRateLabel, &pulseBreathLabel, &pulseGrowthLabel});

    int centreX = rootRect.getRight() + 4;
    int centreW = pulseRect.getX() - centreX - 4;

    int sapSize = juce::roundToInt(68.0f * scale);
    int sapSpacing = (centreW - (sapSize * 3)) / 4;
    for (int i = 0; i < 3; ++i) {
        juce::Slider* sls[] = { &sapFlowSlider, &sapVitalitySlider, &sapTextureSlider };
        juce::Label* labs[] = { &sapFlowLabel, &sapVitalityLabel, &sapTextureLabel };
        int x = centreX + sapSpacing + i * (sapSize + sapSpacing);
        sls[i]->setBounds(x, mainFieldRect.getY() + 6, sapSize, sapSize);
        labs[i]->setBounds(x - 10, sls[i]->getBottom() - 1, sapSize + 20, labelHeight);
    }

    const int podWidth = juce::roundToInt(185.0f * scale);
    const int podHeight = juce::roundToInt(84.0f * scale);
    const int podY = sapFlowLabel.getBottom() + 12;
    canopyRect = { centreX + (centreW / 2) - podWidth - 8, podY, podWidth, podHeight };
    instabilityRect = { centreX + (centreW / 2) + 8, podY, podWidth, podHeight };

    canopySlider.setBounds(canopyRect.getCentreX() - 25, canopyRect.getY() + 14, 50, 50);
    canopyLabel.setBounds(canopyRect.getX(), canopySlider.getBottom() - 2, canopyRect.getWidth(), labelHeight);
    instabilitySlider.setBounds(instabilityRect.getX() + 8, instabilityRect.getY() + 18, 44, 44);

    visualizerFrameRect = juce::Rectangle<int>(centreX + 8, canopyRect.getBottom() + 12, centreW - 16, mainFieldRect.getBottom() - canopyRect.getBottom() - 14);
    visualizer.setBounds(visualizerFrameRect.reduced(10, 24));

    const int macroW = juce::roundToInt(200.0f * scale);
    const int macroH = juce::roundToInt(62.0f * scale);
    const int macroY = headerBoardRect.getY() + juce::roundToInt(40.0f * scale);
    atmosRect = { headerBoardRect.getX() + 8, macroY, macroW, macroH };
    atmosSlider.setBounds(atmosRect.getRight() - 42, macroY + 14, 34, 34);
    seasonsRect = { headerBoardRect.getRight() - macroW - 8, macroY, macroW, macroH };
    seasonsSlider.setBounds(seasonsRect.getX() + 6, macroY + 14, 34, 34);

    int kShift = (keyboardDrawerOpenAmount > 0) ? juce::roundToInt(50.0f * keyboardDrawerOpenAmount * scale) : 0;
    int fxTop = mainFieldRect.getBottom() + juce::roundToInt(10.0f * scale) - kShift;
    int fxBottom = innerRect.getBottom() - juce::roundToInt(6.0f * scale) - kShift;
    int fxW = (innerRect.getWidth() - 36) / 3;
    bloomRect = { innerRect.getX(), fxTop, fxW, fxBottom - fxTop };
    rainRect = { bloomRect.getRight() + 18, fxTop, fxW, fxBottom - fxTop };
    sunRect = { rainRect.getRight() + 18, fxTop, fxW, fxBottom - fxTop };

    for (int i = 0; i < 3; ++i) {
        juce::Slider* sls[] = { &bloomSlider, &rainSlider, &sunSlider };
        sls[i]->setBounds(juce::Rectangle<int>(bloomRect.getX() + i * (fxW + 18), fxTop, fxW, fxBottom - fxTop).getCentreX() - 30, fxTop + 24, 60, 60);
    }

    waveSelector.setBounds(headerBoardRect.getX() + 10, headerBoardRect.getY() + 8, 80, 22);
    presetBox.setBounds(waveSelector.getRight() + 4, headerBoardRect.getY() + 8, 140, 22);
    mutateButton.setBounds(presetBox.getRight() + 4, headerBoardRect.getY() + 8, 74, 22);
    presetSaveButton.setBounds(mutateButton.getRight() + 4, headerBoardRect.getY() + 8, 50, 22);
    presetDeleteButton.setBounds(presetSaveButton.getRight() + 4, headerBoardRect.getY() + 8, 46, 22);
    
    int rX = headerBoardRect.getRight() - 10;
    auto placeR = [&](juce::Component& c, int w) { rX -= w; c.setBounds(rX, headerBoardRect.getY() + 8, w, 22); rX -= 4; };
    if (isKeyboardDrawerAvailable()) placeR(keyboardDrawerButton, 64);
    placeR(testToneButton, 54); placeR(midiClearButton, 60); placeR(mpkDefaultButton, 60); placeR(midiLearnButton, 60);

    seqToggle.setBounds(sequencerBoardRect.getX() + 10, sequencerBoardRect.getY() + 14, 88, 22);
    seqRateSelector.setBounds(seqToggle.getRight() + 4, sequencerBoardRect.getY() + 14, 88, 22);
    seqStepsSelector.setBounds(seqRateSelector.getRight() + 4, sequencerBoardRect.getY() + 14, 88, 22);
    sequencerRect = { seqStepsSelector.getRight() + 12, sequencerBoardRect.getY() + 18, sequencerBoardRect.getRight() - seqStepsSelector.getRight() - 25, 20 };

    if (isKeyboardDrawerAvailable()) {
        int targetVY = bounds.getBottom() - 102;
        int dY = juce::roundToInt(bounds.getBottom() + 10 + (targetVY - (bounds.getBottom() + 10)) * keyboardDrawerOpenAmount);
        keyboardDrawerRect = { mainFieldRect.getX(), dY, mainFieldRect.getWidth(), 100 };
        auto kBounds = keyboardDrawerRect.reduced(10, 10); kBounds.removeFromTop(20);
        keyboardDrawer.setBounds(kBounds);
        keyboardDrawer.setVisible(keyboardDrawerOpenAmount > 0.001f);
    }
    vizModeButton.setBounds(visualizerFrameRect.getRight() - 28, visualizerFrameRect.getY() + 8, 18, 18);
    vizColorButton.setBounds(visualizerFrameRect.getRight() - 50, visualizerFrameRect.getY() + 10, 18, 18);
}

void RootFlowAudioProcessorEditor::setMidiStatusMessage(const juce::String& text, juce::Colour colour)
{
    midiStatusText = text;
    midiStatusColour = colour;
}


void RootFlowAudioProcessorEditor::timerCallback()
{
    if (! isShowing())
        return;

    configureStandaloneWindow();
    updateKeyboardDrawerAnimation();
    const float plantEnergy = audioProcessor.getPlantEnergyValue();
    const float outputPeak = audioProcessor.getOutputPeak();
    const float dynamicEnergy = std::pow(juce::jlimit(0.0f, 1.0f, outputPeak), 0.5f);
    visualizer.pushEnergyValue(dynamicEnergy * (0.8f + plantEnergy * 0.4f));
    auto parameterValue = [this](const char* parameterID)
    {
        if (auto* raw = audioProcessor.tree.getRawParameterValue(parameterID))
            return raw->load();

        return 0.0f;
    };

    PlantEnergyVisualizer::VisualizerState visualizerState;
    visualizerState.plantEnergy = plantEnergy;
    visualizerState.audioEnergy = dynamicEnergy;
    visualizerState.rootDepth = parameterValue("rootDepth");
    visualizerState.rootSoil = parameterValue("rootSoil");
    visualizerState.rootAnchor = parameterValue("rootAnchor");
    visualizerState.sapFlow = parameterValue("sapFlow");
    visualizerState.sapVitality = parameterValue("sapVitality");
    visualizerState.sapTexture = parameterValue("sapTexture");
    visualizerState.pulseRate = parameterValue("pulseRate");
    visualizerState.pulseBreath = parameterValue("pulseBreath");
    visualizerState.pulseGrowth = parameterValue("pulseGrowth");
    visualizerState.canopy = parameterValue("canopy");
    visualizerState.atmosphere = parameterValue("atmosphere");
    visualizerState.instability = parameterValue("instability");
    visualizerState.bloom = parameterValue("bloom");
    visualizerState.rain = parameterValue("rain");
    visualizerState.sun = parameterValue("sun");
    visualizerState.ecoSystem = parameterValue("ecoSystem");
    
    // Bio-Sequencer State
    visualizerState.currentSequencerStep = audioProcessor.currentSequencerStep;
    visualizerState.sequencerOn = (parameterValue("sequencerOn") > 0.5f);
    for (int i = 0; i < 16; ++i)
        visualizerState.sequencerStepActive[(size_t)i] = audioProcessor.sequencerSteps[(size_t)i].active;

    visualizer.pushVisualizerState(visualizerState);
    smoothedUiEnergy += (dynamicEnergy - smoothedUiEnergy) * 0.15f; // Faster reactivity
    uiPulsePhase += 0.045f + smoothedUiEnergy * 0.10f;

    rootsDriftPhase += 0.011f + smoothedUiEnergy * 0.022f;
    energyFlowPhase += 0.005f + smoothedUiEnergy * 0.010f;

    if (uiPulsePhase > juce::MathConstants<float>::twoPi)
        uiPulsePhase -= juce::MathConstants<float>::twoPi;

    if (rootsDriftPhase > juce::MathConstants<float>::twoPi)
        rootsDriftPhase -= juce::MathConstants<float>::twoPi;

    while (energyFlowPhase > 1.0f)
        energyFlowPhase -= 1.0f;
    
    float fftData[RootFlowAudioProcessor::fftSize / 2];
    if (audioProcessor.getNextFFTBlock(fftData))
    {
        visualizer.pushSpectrumData(fftData, RootFlowAudioProcessor::fftSize / 2);
        visualizer.repaint();
    }

    const auto midiSnapshot = audioProcessor.getMidiActivitySnapshot();
    if (midiSnapshot.eventCounter != lastSeenMidiEventCounter)
    {
        lastSeenMidiEventCounter = midiSnapshot.eventCounter;
        midiIndicatorLevel = 1.0f;

        if (midiSnapshot.type == RootFlowAudioProcessor::MidiActivitySnapshot::Type::note
            && midiSnapshot.noteNumber >= 0
            && midiSnapshot.value > 0)
        {
            const float noteIntensity = juce::jmap((float) midiSnapshot.value / 127.0f, 0.24f, 1.0f);
            visualizer.triggerMidiImpulse(midiSnapshot.noteNumber,
                                          noteIntensity * (0.82f + dynamicEnergy * 0.36f),
                                          midiSnapshot.wasMapped);
        }

        switch (midiSnapshot.type)
        {
            case RootFlowAudioProcessor::MidiActivitySnapshot::Type::pitchBend:
            {
                const float bendSemitones = (float) midiSnapshot.bendCents / 100.0f;
                const auto sign = bendSemitones > 0.001f ? "+" : "";
                setMidiStatusMessage("PB " + juce::String(sign) + formatReadoutText(bendSemitones, 2, " ST"),
                                     getStatusMapColour());
                break;
            }

            case RootFlowAudioProcessor::MidiActivitySnapshot::Type::pressure:
                if (midiSnapshot.value >= 0)
                    setMidiStatusMessage("PRESS " + formatReadoutText((float) midiSnapshot.value * 100.0f / 127.0f, 0, "%"),
                                         getStatusReadyColour());
                break;

            case RootFlowAudioProcessor::MidiActivitySnapshot::Type::timbre:
                if (midiSnapshot.value >= 0)
                    setMidiStatusMessage("TIMBRE " + formatReadoutText((float) midiSnapshot.value * 100.0f / 127.0f, 0, "%"),
                                         getStatusInfoColour());
                break;

            case RootFlowAudioProcessor::MidiActivitySnapshot::Type::controller:
                if (midiSnapshot.controllerNumber >= 0)
                {
                    setMidiStatusMessage("CC " + juce::String(midiSnapshot.controllerNumber),
                                         midiSnapshot.wasMapped ? getStatusReadyColour()
                                                                : getStatusInfoColour());
                    if (midiSnapshot.wasMapped)
                        midiStatusText << " ACTIVE";
                }
                break;

            case RootFlowAudioProcessor::MidiActivitySnapshot::Type::note:
                if (midiSnapshot.noteNumber >= 0)
                    setMidiStatusMessage("NOTE " + juce::MidiMessage::getMidiNoteName(midiSnapshot.noteNumber, true, true, 3).toUpperCase(),
                                         getStatusReadyColour());
                break;

            case RootFlowAudioProcessor::MidiActivitySnapshot::Type::none:
            default:
                break;
        }
    }

    midiIndicatorLevel += (0.0f - midiIndicatorLevel) * 0.12f;

    const bool learnArmed = audioProcessor.isMidiLearnArmed();
    const auto learnTarget = audioProcessor.getMidiLearnTargetParameterID();
    if (learnArmed)
    {
        setMidiStatusMessage(learnTarget.isNotEmpty()
                           ? "MAP " + audioProcessor.getParameterDisplayName(learnTarget).toUpperCase() + " - TURN A MIDI KNOB"
                           : "MIDI MAP ARMED",
                             getStatusMapColour());
        midiIndicatorLevel = juce::jmax(midiIndicatorLevel, 0.26f);
    }

    refreshHeaderControlState();

    auto relaxInteractionBoost = [](juce::Slider& slider)
    {
        auto& props = slider.getProperties();
        const auto boostVar = props["rfInteractionBoost"];
        const float currentBoost = boostVar.isVoid() ? 1.0f : (float) (double) boostVar;
        const bool isDragging = props["rfInteractionActive"];
        const float targetBoost = isDragging ? 1.30f : 1.0f;
        float nextBoost = currentBoost + (targetBoost - currentBoost) * (isDragging ? 0.28f : 0.12f);

        if (std::abs(nextBoost - 1.0f) < 0.0015f && ! isDragging)
            nextBoost = 1.0f;

        props.set("rfInteractionBoost", nextBoost);
    };

    juce::Slider* allSliders[] = {
        &rootDepthSlider, &rootSoilSlider, &rootAnchorSlider,
        &sapFlowSlider, &sapVitalitySlider, &sapTextureSlider,
        &pulseRateSlider, &pulseBreathSlider, &pulseGrowthSlider,
        &canopySlider,
        &instabilitySlider,
        &atmosSlider,
        &seasonsSlider,
        &bloomSlider, &rainSlider, &sunSlider
    };

    for (auto* slider : allSliders)
    {
        relaxInteractionBoost(*slider);

        const auto paramID = slider->getProperties()["rfParamID"].toString();
        if (learnArmed && learnTarget.isNotEmpty() && paramID == learnTarget)
        {
            const auto boostVar = slider->getProperties()["rfInteractionBoost"];
            const float currentBoost = boostVar.isVoid() ? 1.0f : (float) (double) boostVar;
            slider->getProperties().set("rfInteractionBoost", juce::jmax(currentBoost, 1.28f));
        }
    }

    repaint();
}

void RootFlowAudioProcessorEditor::updateKeyboardDrawerAnimation()
{
    if (! isKeyboardDrawerAvailable())
        return;

    const float previousAmount = keyboardDrawerOpenAmount;
    keyboardDrawerOpenAmount += (keyboardDrawerTarget - keyboardDrawerOpenAmount) * 0.18f;

    if (std::abs(keyboardDrawerOpenAmount - keyboardDrawerTarget) < 0.0015f)
        keyboardDrawerOpenAmount = keyboardDrawerTarget;

    keyboardDrawerButton.setButtonText(keyboardDrawerTarget > 0.5f ? "HIDE" : "KEYS");
    keyboardDrawerButton.setColour(juce::TextButton::buttonColourId,
                                   juce::Colour(56, 38, 24).withAlpha(keyboardDrawerTarget > 0.5f ? 0.94f : 0.86f));

    if (std::abs(keyboardDrawerOpenAmount - previousAmount) > 0.0005f)
        resized();
}

void RootFlowAudioProcessorEditor::toggleKeyboardDrawer()
{
    if (! isKeyboardDrawerAvailable())
        return;

    // Zielwert umschalten (0.0 = zu, 1.0 = offen)
    keyboardDrawerTarget = (keyboardDrawerTarget > 0.5f) ? 0.0f : 1.0f;
    keyboardDrawer.setVisible(true);

    const int baseWidth = 900;
    const int baseHeight = 640;
    const int drawerExtension = 90; // Kompakte Höhe für 13" MacBooks

    if (keyboardDrawerTarget > 0.5f)
    {
        // Fenster nur vergrößern, wenn es aktuell zu klein für die Tasten ist
        if (getHeight() < baseHeight + drawerExtension)
        {
            setSize(getWidth(), baseHeight + drawerExtension);
        }
        grabKeyboardFocus();
    }
    else
    {
        // Beim Schließen auf Standardhöhe zurückgehen, falls nicht manuell groß gezogen
        if (getHeight() <= baseHeight + drawerExtension + 10)
        {
            setSize(getWidth(), baseHeight);
        }
        releaseVirtualKeyboardNotes();
    }
}

bool RootFlowAudioProcessorEditor::isKeyboardDrawerAvailable() const noexcept
{
#if JucePlugin_Build_Standalone
    return true;
#else
    return false;
#endif
}

bool RootFlowAudioProcessorEditor::isVirtualKeyboardInputEnabled() const noexcept
{
    return isKeyboardDrawerAvailable() && keyboardDrawerTarget > 0.5f;
}

void RootFlowAudioProcessorEditor::refreshHeaderControlState()
{
    const bool learnArmed = audioProcessor.isMidiLearnArmed();
    const bool testToneEnabled = audioProcessor.isTestToneEnabled();
    const bool presetDirty = audioProcessor.isCurrentPresetDirty();
    const int presetItemCount = audioProcessor.getCombinedPresetCount();
    const int factoryPresetCount = audioProcessor.getFactoryPresetCount();
    const int currentPresetIndex = audioProcessor.getCurrentPresetMenuIndex();
    const int currentUserPresetIndex = audioProcessor.getCurrentUserPresetIndex();
    const auto presetNames = audioProcessor.getCombinedPresetNames();
    const auto presetSections = audioProcessor.getFactoryPresetMenuSections();

    if (! headerControlStateInitialised || presetItemCount != cachedPresetItemCount)
    {
        presetBox.clear(juce::dontSendNotification);

        bool needsSeparator = false;
        for (const auto& section : presetSections)
        {
            if (section.count <= 0)
                continue;

            if (needsSeparator)
                presetBox.addSeparator();

            presetBox.addSectionHeading(section.title);

            const int endIndex = juce::jmin(section.startIndex + section.count, factoryPresetCount);
            for (int i = section.startIndex; i < endIndex; ++i)
                presetBox.addItem(presetNames[i], i + 1);

            needsSeparator = true;
        }

        if (presetNames.size() > factoryPresetCount)
        {
            if (needsSeparator)
                presetBox.addSeparator();

            presetBox.addSectionHeading("USER");
            for (int i = factoryPresetCount; i < presetNames.size(); ++i)
                presetBox.addItem(presetNames[i], i + 1);
        }

        cachedPresetItemCount = presetItemCount;
    }

    if (! headerControlStateInitialised
        || currentPresetIndex != cachedPresetMenuIndex
        || presetDirty != cachedPresetDirty)
    {
        if (! presetBox.isPopupActive())
        {
            if (presetDirty && juce::isPositiveAndBelow(currentPresetIndex, presetNames.size()))
            {
                presetBox.setTextWhenNothingSelected(presetNames[currentPresetIndex] + " *");
                if (presetBox.getSelectedId() != 0)
                    presetBox.setSelectedId(0, juce::dontSendNotification);
            }
            else
            {
                presetBox.setTextWhenNothingSelected("PRESET");

                if (currentPresetIndex >= 0)
                {
                    if (presetBox.getSelectedId() != currentPresetIndex + 1)
                        presetBox.setSelectedId(currentPresetIndex + 1, juce::dontSendNotification);
                }
                else if (presetBox.getSelectedId() != 0)
                {
                    presetBox.setSelectedId(0, juce::dontSendNotification);
                }
            }
        }

        cachedPresetMenuIndex = currentPresetIndex;
        cachedPresetDirty = presetDirty;
    }

    if (! headerControlStateInitialised || learnArmed != cachedMidiLearnArmed)
    {
        midiLearnButton.setButtonText(learnArmed ? "ARMED" : "MAP");
        midiLearnButton.setColour(juce::TextButton::buttonColourId,
                                  learnArmed
                                      ? juce::Colour(112, 66, 28).withAlpha(0.94f)
                                      : juce::Colour(62, 40, 22).withAlpha(0.88f));
        midiLearnButton.setColour(juce::TextButton::buttonOnColourId,
                                  learnArmed
                                      ? juce::Colour(142, 88, 36).withAlpha(0.96f)
                                      : juce::Colour(84, 56, 28).withAlpha(0.92f));
        cachedMidiLearnArmed = learnArmed;
    }

    if (! headerControlStateInitialised || testToneEnabled != cachedTestToneEnabled)
    {
        testToneButton.setToggleState(testToneEnabled, juce::dontSendNotification);
        testToneButton.setColour(juce::TextButton::buttonColourId,
                                 testToneEnabled
                                     ? juce::Colour(118, 78, 28).withAlpha(0.94f)
                                     : juce::Colour(46, 30, 18).withAlpha(0.84f));
        testToneButton.setColour(juce::TextButton::buttonOnColourId,
                                 testToneEnabled
                                     ? juce::Colour(164, 112, 36).withAlpha(0.96f)
                                     : juce::Colour(78, 50, 24).withAlpha(0.90f));
        cachedTestToneEnabled = testToneEnabled;
    }

    if (! headerControlStateInitialised || currentUserPresetIndex != cachedUserPresetIndex)
    {
        const bool hasUserPreset = currentUserPresetIndex >= 0;
        presetDeleteButton.setEnabled(hasUserPreset);
        presetDeleteButton.setAlpha(hasUserPreset ? 1.0f : 0.45f);
        cachedUserPresetIndex = currentUserPresetIndex;
    }

    headerControlStateInitialised = true;
}

void RootFlowAudioProcessorEditor::updateAnimationTimerState()
{
    if (isShowing())
        startTimerHz(60);
    else
        stopTimer();
}

void RootFlowAudioProcessorEditor::updateVirtualKeyboardState()
{
    if (! isVirtualKeyboardInputEnabled())
    {
        releaseVirtualKeyboardNotes();
        return;
    }

    for (size_t i = 0; i < virtualKeyboardCharacters.size(); ++i)
    {
        const int lowerKeyCode = (int) virtualKeyboardCharacters[i];
        const int upperKeyCode = (int) juce::CharacterFunctions::toUpperCase(virtualKeyboardCharacters[i]);
        const bool isDown = juce::KeyPress::isKeyCurrentlyDown(lowerKeyCode)
                         || juce::KeyPress::isKeyCurrentlyDown(upperKeyCode);

        if (isDown == virtualKeyDownStates[i])
            continue;

        const int midiNote = virtualKeyboardBaseMidiNote + (int) i;
        if (isDown)
            audioProcessor.getKeyboardState().noteOn(1, midiNote, (juce::uint8) 100);
        else
            audioProcessor.getKeyboardState().noteOff(1, midiNote, 0.0f);

        virtualKeyDownStates[i] = isDown;
    }
}

void RootFlowAudioProcessorEditor::releaseVirtualKeyboardNotes()
{
    for (size_t i = 0; i < virtualKeyDownStates.size(); ++i)
    {
        if (! virtualKeyDownStates[i])
            continue;

        audioProcessor.getKeyboardState().noteOff(1, virtualKeyboardBaseMidiNote + (int) i, 0.0f);
        virtualKeyDownStates[i] = false;
    }
}

void RootFlowAudioProcessorEditor::configureStandaloneWindow()
{
#if JucePlugin_Build_Standalone
    if (standaloneWindowConfigured)
        return;

    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        standaloneWindowConfigured = true;
        window->setUsingNativeTitleBar(false);
        // Keep the JUCE title bar so the built-in Options menu exposes Audio/MIDI settings.
        window->setTitleBarHeight(36);
        window->setResizable(true, false);
        window->setResizeLimits(400, 500, 1600, 2000);
        window->setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(20, 12, 7));
        window->getContentComponent()->setOpaque(true);
        window->getContentComponent()->setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(20, 12, 7));
    }
#endif
}

void RootFlowAudioProcessorEditor::drawTexturedWoodPanel(juce::Graphics& g,
                                                         juce::Rectangle<float> area,
                                                         float cornerRadius,
                                                         juce::Colour topTint,
                                                         juce::Colour bottomTint,
                                                         float textureAlpha,
                                                         float grainAlpha,
                                                         float vignetteAlpha)
{
    juce::Path clip;
    clip.addRoundedRectangle(area, cornerRadius);

    g.saveState();
    g.reduceClipRegion(clip);

    const auto breathedTopTint = getPanelBreathingColour(topTint, area, 0.007f, 0.016f, 0.85f);
    const auto breathedBottomTint = getPanelBreathingColour(bottomTint, area.translated(0.0f, 24.0f), 0.005f, 0.012f, 0.78f);
    g.setColour(breathedTopTint.interpolatedWith(breathedBottomTint, 0.5f));
    g.fillRoundedRectangle(area, cornerRadius);

    if (woodBackground.isValid())
    {
        const float textureScale = juce::jlimit(0.42f, 1.05f,
                                                0.52f + juce::jmin(area.getWidth(), area.getHeight()) / 900.0f);
        g.setTiledImageFill(woodBackground,
                            area.getX() * 0.36f,
                            area.getY() * 0.24f,
                            textureScale);
        g.setOpacity(textureAlpha);
        g.fillRoundedRectangle(area, cornerRadius);
        g.setOpacity(1.0f);
    }

    g.setColour(juce::Colour(10, 7, 4).withAlpha(vignetteAlpha * 0.22f));
    g.fillRoundedRectangle(area, cornerRadius);

    g.restoreState();

    const auto finishGlow = topTint.interpolatedWith(juce::Colour(142, 255, 170), 0.18f);
    drawPanelFinishOverlay(g,
                           area.reduced(0.5f),
                           juce::jmax(0.0f, cornerRadius - 0.5f),
                           finishGlow,
                           0.020f + grainAlpha * 0.16f,
                           0.008f + grainAlpha * 0.08f);
}

void RootFlowAudioProcessorEditor::drawProceduralWoodPanel(juce::Graphics& g,
                                                           juce::Rectangle<float> area,
                                                           float cornerRadius,
                                                           juce::Colour baseColour,
                                                           juce::Colour grainColour,
                                                           float grainAlpha)
{
    juce::Path clip;
    clip.addRoundedRectangle(area, cornerRadius);

    g.saveState();
    g.reduceClipRegion(clip);

    const auto breathedBaseColour = getPanelBreathingColour(baseColour, area, 0.010f, 0.022f);
    const auto breathedGrainColour = getPanelBreathingColour(grainColour, area.translated(16.0f, -10.0f), 0.003f, 0.014f, 0.92f);

    g.setColour(breathedBaseColour);
    g.fillRoundedRectangle(area, cornerRadius);

    const int lines = juce::jmax(12, juce::roundToInt(area.getHeight() / 12.0f));
    for (int i = 0; i < lines; ++i)
    {
        const float t = lines > 1 ? (float) i / (float) (lines - 1) : 0.0f;
        const float y = area.getY() + area.getHeight() * (0.08f + t * 0.84f);

        juce::Path grain;
        grain.startNewSubPath(area.getX() - 8.0f, y);

        for (int step = 1; step <= 16; ++step)
        {
            const float stepT = (float) step / 16.0f;
            const float x = area.getX() + area.getWidth() * stepT;
            const float wobble = std::sin(stepT * 8.0f + i * 0.41f) * (0.8f + area.getHeight() * 0.002f)
                               + std::cos(stepT * 18.0f + i * 0.17f) * 0.55f;
            grain.lineTo(x, y + wobble);
        }

        g.setColour(breathedGrainColour.withAlpha(grainAlpha * (0.38f + 0.22f * std::sin(i * 0.73f + 1.4f))));
        g.strokePath(grain, juce::PathStrokeType(0.9f));
    }

    g.setColour(juce::Colour(12, 8, 5).withAlpha(0.12f));
    g.fillRoundedRectangle(area, cornerRadius);
    g.restoreState();

    const auto finishGlow = baseColour.interpolatedWith(juce::Colour(148, 255, 176), 0.14f);
    drawPanelFinishOverlay(g,
                           area.reduced(0.5f),
                           juce::jmax(0.0f, cornerRadius - 0.5f),
                           finishGlow,
                           0.018f + grainAlpha * 0.12f,
                           0.006f + grainAlpha * 0.06f);
}

void RootFlowAudioProcessorEditor::drawWoodFrame(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();
    drawTexturedWoodPanel(g, r, 2.0f,
                          juce::Colour(72, 44, 26),
                          juce::Colour(30, 18, 11),
                          0.42f, 0.18f, 0.28f);

    g.setColour(juce::Colour(126, 90, 58).withAlpha(0.12f));
    g.fillRect(juce::Rectangle<float>(r.getX(), r.getY(), r.getWidth(), 18.0f));
    g.fillRect(juce::Rectangle<float>(r.getX(), r.getY(), 16.0f, r.getHeight()));
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillRect(juce::Rectangle<float>(r.getX(), r.getBottom() - 20.0f, r.getWidth(), 20.0f));
    g.fillRect(juce::Rectangle<float>(r.getRight() - 18.0f, r.getY(), 18.0f, r.getHeight()));

    float inset = 16.0f;
    auto innerR = r.reduced(inset);
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.drawRoundedRectangle(innerR, 10.0f, 8.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(innerR.translated(0.5f, 0.5f), 10.0f, 3.0f);

    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawRoundedRectangle(innerR.translated(1.0f, 1.0f), 10.0f, 4.0f);

    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawRoundedRectangle(innerR.translated(2.0f, 2.0f), 10.0f, 5.0f);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRect(r, 2.0f);
}

void RootFlowAudioProcessorEditor::drawHeader(juce::Graphics& g, juce::Rectangle<int> area)
{
    float scale = juce::jmin(getWidth() / 1024.0f, getHeight() / 1024.0f);
    auto header = area.toFloat();

    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(header.translated(0.0f, 2.5f), 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.44f));
    g.fillRoundedRectangle(header.translated(0.0f, 7.5f), 14.0f);
    drawTexturedWoodPanel(g, header, 12.0f,
                          juce::Colour(76, 46, 27),
                          juce::Colour(32, 18, 10),
                          0.46f, 0.16f, 0.24f);
    drawPanelDepthOverlay(g, header.reduced(0.8f), 12.0f,
                          juce::Colour(196, 148, 98).withAlpha(0.08f),
                          juce::Colours::black.withAlpha(0.18f));

    g.setColour(juce::Colours::black.withAlpha(0.42f));
    g.drawRoundedRectangle(header.reduced(0.5f), 12.0f, 2.0f);
    g.setColour(juce::Colour(132, 96, 62).withAlpha(0.10f));
    g.drawRoundedRectangle(header.reduced(2.0f), 12.0f, 1.0f);
    drawTexturedWoodPanel(g, header.reduced(5.0f), 9.0f,
                          juce::Colour(56, 32, 19),
                          juce::Colour(24, 14, 8),
                          0.36f, 0.12f, 0.18f);
    drawPanelDepthOverlay(g, header.reduced(5.0f), 9.0f,
                          juce::Colour(216, 182, 136).withAlpha(0.045f),
                          juce::Colours::black.withAlpha(0.22f));
    g.setColour(juce::Colours::black.withAlpha(0.26f));
    g.drawRoundedRectangle(header.reduced(5.0f), 9.0f, 1.2f);
    g.setColour(juce::Colour(188, 255, 170).withAlpha(0.040f));
    g.fillRoundedRectangle(header.reduced(9.0f, 8.0f), 8.0f);

    // Specular Highlight for richness (Reflection)
    juce::ColourGradient spec(juce::Colours::white.withAlpha(0.07f), header.getX(), header.getY(),
                              juce::Colours::transparentBlack, header.getX(), header.getY() + 18.0f, false);
    g.setGradientFill(spec);
    g.fillRoundedRectangle(header.reduced(2.0f), 12.0f);


    auto content = area.reduced(juce::roundToInt(18.0f * scale), juce::roundToInt(8.0f * scale));
    const int controlLaneHeight = juce::roundToInt(36.0f * scale);
    const int statusLaneHeight = juce::roundToInt(18.0f * scale);
    auto brandingArea = content.withTrimmedTop(controlLaneHeight)
                               .withTrimmedBottom(statusLaneHeight)
                               .reduced(juce::roundToInt(42.0f * scale), juce::roundToInt(2.0f * scale));
    brandingArea = brandingArea.withTrimmedLeft(atmosRect.isEmpty() ? 0
                                                                    : juce::jmax(0, atmosRect.getWidth() - juce::roundToInt(24.0f * scale)))
                               .withTrimmedRight(seasonsRect.isEmpty() ? 0
                                                                       : juce::jmax(0, seasonsRect.getWidth() - juce::roundToInt(24.0f * scale)));
    const int brandingGap = juce::roundToInt(3.0f * scale);
    
    const int audioLineHeight = juce::jmax(juce::roundToInt(11.0f * scale),
                                           juce::roundToInt(brandingArea.getHeight() * 0.18f));
    const int titleLineHeight = juce::jmax(juce::roundToInt(22.0f * scale),
                                           juce::roundToInt(brandingArea.getHeight() * 0.32f));
    const int logoLineHeight = juce::jmax(juce::roundToInt(28.0f * scale),
                                          brandingArea.getHeight() - audioLineHeight - titleLineHeight - brandingGap * 2);

    auto logoArea = brandingArea.removeFromTop(logoLineHeight)
                                .translated(0, juce::roundToInt(2.0f * scale));
    brandingArea.removeFromTop(brandingGap);
    auto text1 = brandingArea.removeFromTop(audioLineHeight);
    brandingArea.removeFromTop(brandingGap);
    auto text2 = brandingArea.removeFromTop(titleLineHeight).translated(0, -juce::roundToInt(1.0f * scale));

    {
        const float cursiveFontHeight = juce::jlimit(38.0f * scale,
                                                     52.0f * scale,
                                                     logoArea.getHeight() * 1.28f);
        auto cursiveFont = juce::Font(juce::FontOptions().withName("Brush Script MT").withHeight(cursiveFontHeight).withStyle("Italic"));
        
        const auto gColor = visualizer.getCurrentColor();
        auto brandCol = (gColor == PlantEnergyVisualizer::GrowthColor::emerald) ? juce::Colour(100, 255, 160) :
                       (gColor == PlantEnergyVisualizer::GrowthColor::neon) ? juce::Colour(180, 255, 80) :
                       (gColor == PlantEnergyVisualizer::GrowthColor::moss) ? juce::Colour(140, 230, 120) :
                       juce::Colour(80, 200, 110);

        const float reactiveGlow = 0.40f + 0.60f * smoothedUiEnergy;
        const float pulse = 0.94f + 0.06f * std::sin((float) juce::Time::getMillisecondCounterHiRes() * 0.006f);

        // --- Layout Titles ---
        auto topArea = logoArea;
        auto midArea = text1;
        auto bottomArea = text2.translated(0, -juce::roundToInt(2.0f * scale));

        // Draw 'Funky Moose' with Glow
        g.setFont(cursiveFont);
        g.setColour(brandCol.withAlpha(0.15f * reactiveGlow * pulse));
        g.drawFittedText("Funky Moose", topArea.expanded(12 * scale, 4 * scale).translated(0, 1), juce::Justification::centred, 1);
        g.setColour(brandCol.withAlpha(0.28f * reactiveGlow));
        g.drawFittedText("Funky Moose", topArea.expanded(6 * scale, 2 * scale), juce::Justification::centred, 1);
        
        g.setColour(brandCol.withAlpha(0.75f + 0.25f * smoothedUiEnergy));
        g.drawFittedText("Funky Moose", topArea, juce::Justification::centred, 1);
        g.setColour(brandCol.interpolatedWith(juce::Colours::white, 0.75f).withAlpha(0.96f));
        g.drawFittedText("Funky Moose", topArea.translated(0, -1), juce::Justification::centred, 1);

        // Subtitle
        auto subCol = brandCol.interpolatedWith(juce::Colour(240, 255, 230), 0.25f);
        drawGlowText(g, "FUNKY MOOSE AUDIO", midArea,
                     juce::Font(juce::FontOptions(juce::jlimit(10.5f * scale,
                                                               13.0f * scale,
                                                               midArea.getHeight() * 0.90f)).withStyle("Bold")),
                     subCol,
                     brandCol.withAlpha(0.18f + 0.12f * smoothedUiEnergy),
                     juce::Colours::transparentBlack,
                     juce::Justification::centred);

        // ROOTFLOW
        drawGlowText(g, "ROOTFLOW", bottomArea,
                     juce::Font(juce::FontOptions(juce::jlimit(24.0f * scale,
                                                               33.0f * scale,
                                                               bottomArea.getHeight() * 1.04f)).withStyle("Bold")),
                     brandCol.interpolatedWith(juce::Colours::white, 0.55f),
                     brandCol.withAlpha(0.20f + 0.20f * smoothedUiEnergy),
                     brandCol.withAlpha(0.08f + 0.12f * smoothedUiEnergy),
                     juce::Justification::centred);
    }





    drawAtmosPod(g, atmosRect);
    drawSeasonPod(g, seasonsRect);

    auto statusArea = juce::Rectangle<int>(area.getX() + juce::roundToInt(22.0f * scale),
                                           area.getBottom() - juce::roundToInt(22.0f * scale),
                                           area.getWidth() - juce::roundToInt(44.0f * scale),
                                           juce::roundToInt(14.0f * scale));
    const auto indicatorColour = midiStatusColour;
    const auto statusTextColour = juce::Colour(228, 238, 220).interpolatedWith(indicatorColour, 0.18f);
    drawStatusLight(g,
                    { statusArea.getX() + 6.0f, statusArea.getCentreY() + 0.5f },
                    indicatorColour.withAlpha(0.30f + midiIndicatorLevel * 0.70f),
                    3.8f + midiIndicatorLevel * 1.8f);

                 drawGlowText(g, midiStatusText, statusArea.withTrimmedLeft(18),
                 juce::Font(juce::FontOptions(11.5f * scale).withStyle("Bold")),
                 statusTextColour.withAlpha(0.92f),
                 indicatorColour.withAlpha(0.16f + midiIndicatorLevel * 0.10f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    // Render the custom glassy background for the preset selector component
    drawPresetMenu(g, presetBox.getBounds());
}

void RootFlowAudioProcessorEditor::drawMainPanel(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();
    const float motionEnergy = juce::jlimit(0.0f, 1.0f, smoothedUiEnergy);
    const float pulse = 0.5f + 0.5f * std::sin(uiPulsePhase);
    const float panelBreathe = getPanelBreathe(r, 0.82f);

    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), 16.0f);
    g.setColour(juce::Colours::black.withAlpha(0.44f));
    g.fillRoundedRectangle(r.translated(0.0f, 6.5f), 16.0f);

    juce::ColourGradient panelGrad(
        getPanelBreathingColour(juce::Colour(10, 10, 8), r, 0.015f, 0.028f, 0.82f),
        r.getTopLeft(),
        getPanelBreathingColour(juce::Colour(20, 18, 14), r.translated(0.0f, 32.0f), 0.012f, 0.022f, 0.78f),
        r.getBottomRight(),
        false
    );
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(r, 14.0f);
    drawPanelDepthOverlay(g, r.reduced(0.8f), 14.0f,
                          juce::Colour(184, 146, 92).withAlpha(0.055f),
                          juce::Colours::black.withAlpha(0.18f));

    if (rootsBackground.isValid() || rootsOverlaySoft.isValid() || rootsOverlayDetailBlurred.isValid() || rootsOverlayDetail.isValid())
    {
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle(r.reduced(1.0f), 14.0f);
        g.reduceClipRegion(clip);

        const int driftX = juce::roundToInt(std::sin(rootsDriftPhase) * area.getWidth() * 0.016f);
        const int driftY = juce::roundToInt(std::cos(rootsDriftPhase * 0.63f) * area.getHeight() * 0.010f);

        if (rootsBackground.isValid())
        {
            g.setOpacity(0.07f + motionEnergy * 0.03f);
            g.drawImageWithin(rootsBackground,
                              area.getX() - 34 + driftX,
                              area.getY() - 22 + driftY,
                              area.getWidth() + 68,
                              area.getHeight() + 44,
                              juce::RectanglePlacement::fillDestination);
        }

        if (rootsOverlaySoft.isValid())
        {
            g.setOpacity(0.12f + pulse * 0.025f + motionEnergy * 0.04f);
            g.drawImageWithin(rootsOverlaySoft,
                              area.getX() - 12 - driftX / 2,
                              area.getY() - 8 + driftY,
                              area.getWidth() + 24,
                              area.getHeight() + 16,
                              juce::RectanglePlacement::fillDestination);
        }

        if (rootsOverlayDetailBlurred.isValid())
        {
            g.setOpacity(0.06f + pulse * 0.05f);
            g.drawImageWithin(rootsOverlayDetailBlurred,
                              area.getX() - 20 + driftX / 3,
                              area.getY() - 12 - driftY / 2,
                              area.getWidth() + 40,
                              area.getHeight() + 24,
                              juce::RectanglePlacement::fillDestination);
        }

        if (rootsOverlayDetail.isValid())
        {
            g.setOpacity(0.025f + motionEnergy * 0.03f);
            g.drawImageWithin(rootsOverlayDetail,
                              area.getX() - 6,
                              area.getY() - 4,
                              area.getWidth() + 12,
                              area.getHeight() + 8,
                              juce::RectanglePlacement::stretchToFit);
        }

        drawEnergyFlowOverlay(g, r.reduced(2.0f), 14.0f);
        g.setOpacity(1.0f);
        g.restoreState();
    }

    drawPanelFinishOverlay(g,
                           r.reduced(1.2f),
                           13.4f,
                           juce::Colour(118, 255, 150),
                           0.020f + motionEnergy * 0.018f,
                           0.010f);

    g.setColour(juce::Colour(125, 255, 170).withAlpha(0.015f + motionEnergy * 0.010f + panelBreathe * 0.007f));
    g.fillRoundedRectangle(r.reduced(2.0f), 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.34f));
    g.drawRoundedRectangle(r.reduced(2.0f), 14.0f, 5.0f);
    g.setColour(juce::Colours::black.withAlpha(0.70f));
    g.drawRoundedRectangle(r.reduced(0.5f), 14.0f, 2.0f);
    g.setColour(juce::Colour(132, 104, 66).withAlpha(0.08f));
    g.drawRoundedRectangle(r.reduced(5.0f), 12.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.36f));
    g.drawRoundedRectangle(r.reduced(9.0f), 10.0f, 1.0f);
}

void RootFlowAudioProcessorEditor::drawAmbientPulse(juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius)
{
    const float pulse = 0.5f + 0.5f * std::sin(uiPulsePhase);
    const float microTime = (float) juce::Time::getMillisecondCounterHiRes() * 0.00020f;
    const float breathe = 0.5f + 0.5f * std::sin(microTime * 0.72f + uiPulsePhase * 0.55f);
    const float glowAlpha = 0.010f + pulse * 0.014f + smoothedUiEnergy * 0.018f + breathe * 0.004f;
    auto pulseArea = area.expanded(1.5f + breathe * 1.2f, 0.8f + breathe * 0.6f);

    juce::ColourGradient pulseGlow(
        juce::Colour(146, 255, 176).withAlpha(glowAlpha),
        pulseArea.getCentreX(),
        pulseArea.getY() + pulseArea.getHeight() * 0.20f,
        juce::Colours::transparentBlack,
        pulseArea.getCentreX(),
        pulseArea.getBottom(),
        false
    );

    g.setGradientFill(pulseGlow);
    g.fillRoundedRectangle(pulseArea, cornerRadius + 1.0f);
}

void RootFlowAudioProcessorEditor::drawEnergyFlowOverlay(juce::Graphics& g, juce::Rectangle<float> area, float cornerRadius)
{
    juce::ignoreUnused(cornerRadius);

    const float bandWidth = area.getWidth() * 0.24f;
    const float bandStart = area.getX() - bandWidth + (area.getWidth() + bandWidth * 2.0f) * energyFlowPhase;
    const float flowAlpha = 0.030f + smoothedUiEnergy * 0.050f;

    juce::ColourGradient flow(
        juce::Colours::transparentBlack,
        bandStart,
        area.getCentreY(),
        juce::Colours::transparentBlack,
        bandStart + bandWidth,
        area.getCentreY(),
        false
    );
    flow.addColour(0.28, juce::Colour(90, 255, 142).withAlpha(0.0f));
    flow.addColour(0.50, juce::Colour(205, 255, 202).withAlpha(flowAlpha));
    flow.addColour(0.72, juce::Colour(90, 255, 142).withAlpha(0.0f));

    g.setGradientFill(flow);
    g.fillRect(area);

    g.setColour(juce::Colour(185, 255, 194).withAlpha(flowAlpha * 0.45f));
    g.fillRect(juce::Rectangle<float>(bandStart + bandWidth * 0.44f, area.getY(), 1.8f, area.getHeight()));
}

void RootFlowAudioProcessorEditor::drawSectionPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    const auto accent = getSectionAccent(title);
    auto sectionBody = area.toFloat().reduced(5.0f, 10.0f);
    sectionBody.removeFromTop(18.0f);
    const float panelBreathe = getPanelBreathe(sectionBody, 1.08f);
    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.fillRoundedRectangle(sectionBody.translated(0.0f, 2.0f), 12.0f);
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillRoundedRectangle(sectionBody.translated(0.0f, 4.5f), 12.0f);

    juce::ColourGradient sectionTint(
        getPanelBreathingColour(accent.bodyTint, sectionBody, 0.010f, 0.028f, 1.08f).withAlpha(0.26f),
        sectionBody.getCentreX(),
        sectionBody.getY(),
        getPanelBreathingColour(accent.bodyTint.darker(0.35f), sectionBody.translated(0.0f, 26.0f), 0.006f, 0.018f, 0.96f).withAlpha(0.12f),
        sectionBody.getCentreX(),
        sectionBody.getBottom(),
        false
    );
    sectionTint.addColour(0.35, accent.panelGlow.withAlpha(0.050f + panelBreathe * 0.018f));
    g.setGradientFill(sectionTint);
    g.fillRoundedRectangle(sectionBody, 12.0f);

    drawPanelDepthOverlay(g, sectionBody, 12.0f,
                          accent.panelGlow.withAlpha(0.050f),
                          juce::Colours::black.withAlpha(0.13f));
    drawPanelFinishOverlay(g,
                           sectionBody.reduced(0.6f),
                           11.4f,
                           accent.panelGlow,
                           0.030f,
                           0.014f);
    g.setColour(accent.lineCore.withAlpha(0.085f));
    g.drawRoundedRectangle(sectionBody.reduced(1.0f), 12.0f, 0.9f);
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.drawRoundedRectangle(sectionBody, 12.0f, 1.1f);

    auto topLine = area.removeFromTop(22).toFloat();
    juce::Font f(juce::FontOptions(17.0f).withStyle("Bold"));
    g.setFont(f);

    juce::GlyphArrangement ga;
    ga.addFittedText(f, title, 0, 0, 1000.0f, 100.0f, juce::Justification::left, 1);
    float textW = ga.getBoundingBox(0, title.length(), true).getWidth() + 22.0f;
    float cy = topLine.getCentreY();
    float cx = topLine.getCentreX();

    const float leftX = topLine.getX() + 8.0f;
    const float rightX = topLine.getRight() - 8.0f;
    const float leftEnd = cx - textW * 0.5f;
    const float rightStart = cx + textW * 0.5f;

    g.setColour(accent.lineGlow.withAlpha(0.24f));
    g.drawLine(leftX, cy, leftEnd, cy, 4.0f);
    g.drawLine(rightStart, cy, rightX, cy, 4.0f);
    g.drawLine(leftX, cy, leftX, cy + 7.0f, 4.0f);
    g.drawLine(rightX, cy, rightX, cy + 7.0f, 4.0f);

    g.setColour(accent.lineCore.withAlpha(0.96f));
    g.drawLine(leftX, cy, leftEnd, cy, 1.45f);
    g.drawLine(rightStart, cy, rightX, cy, 1.45f);
    g.drawLine(leftX, cy, leftX, cy + 6.0f, 1.45f);
    g.drawLine(rightX, cy, rightX, cy + 6.0f, 1.45f);

    drawGlowText(g, title, topLine.toNearestInt(),
                 f,
                 accent.textCore.withAlpha(0.98f),
                 accent.textGlow.withAlpha(0.28f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centred);
}

void RootFlowAudioProcessorEditor::drawCanopyPod(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();

    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), 13.0f);
    g.setColour(juce::Colours::black.withAlpha(0.42f));
    g.fillRoundedRectangle(r.translated(0.0f, 5.5f), 13.0f);
    drawTexturedWoodPanel(g, r, 12.0f,
                          juce::Colour(62, 38, 22),
                          juce::Colour(26, 15, 9),
                          0.42f, 0.12f, 0.18f);
    drawPanelDepthOverlay(g, r.reduced(0.8f), 12.0f,
                          juce::Colour(188, 164, 124).withAlpha(0.050f),
                          juce::Colours::black.withAlpha(0.18f));
    g.setColour(juce::Colours::black.withAlpha(0.68f));
    g.drawRoundedRectangle(r.reduced(0.5f), 12.0f, 1.8f);
    g.setColour(juce::Colour(146, 112, 76).withAlpha(0.10f));
    g.drawRoundedRectangle(r.reduced(2.4f), 11.0f, 0.9f);

    auto titleArea = area.removeFromTop(22);
    drawGlowText(g, "CANOPY", titleArea,
                 juce::Font(juce::FontOptions(15.0f).withStyle("Bold")),
                 juce::Colour(230, 255, 226).withAlpha(0.96f),
                 juce::Colour(140, 255, 170).withAlpha(0.16f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centred);

    auto valueArea = juce::Rectangle<int>(area.getX() + 12,
                                          area.getBottom() - 24,
                                          area.getWidth() - 24,
                                          18);
    drawValueReadout(g, canopySlider.getValue() * 100.0f, valueArea, "%", 0);

    drawScrew(g, { r.getX() + 10.0f, r.getY() + 10.0f }, 4.2f);
    drawScrew(g, { r.getRight() - 10.0f, r.getY() + 10.0f }, 4.2f);
}

void RootFlowAudioProcessorEditor::drawInstabilityPod(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (area.isEmpty())
        return;

    const auto instabilityInfo = getInstabilityDisplayInfo((float) instabilitySlider.getValue());
    auto r = area.toFloat();

    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), 13.0f);
    g.setColour(juce::Colours::black.withAlpha(0.42f));
    g.fillRoundedRectangle(r.translated(0.0f, 5.0f), 13.0f);
    drawTexturedWoodPanel(g, r, 12.0f,
                          instabilityInfo.panelTop.withAlpha(0.94f),
                          instabilityInfo.panelBottom.withAlpha(0.96f),
                          0.34f, 0.12f, 0.18f);
    drawPanelDepthOverlay(g, r.reduced(0.8f), 12.0f,
                          instabilityInfo.glow.withAlpha(0.046f),
                          juce::Colours::black.withAlpha(0.18f));
    g.setColour(instabilityInfo.rim.withAlpha(0.12f));
    g.drawRoundedRectangle(r.reduced(1.2f), 11.0f, 0.9f);
    g.setColour(juce::Colours::black.withAlpha(0.68f));
    g.drawRoundedRectangle(r.reduced(0.5f), 12.0f, 1.8f);

    const float scale = juce::jmin(area.getWidth() / 185.0f, area.getHeight() / 88.0f);

    auto titleArea = juce::Rectangle<int>(area.getX() + 10, area.getY() + 6, area.getWidth() - 20, 18);
    drawGlowText(g, "INSTABILITY", titleArea,
                 juce::Font(juce::FontOptions(11.0f * scale).withStyle("Bold")),
                 juce::Colour(245, 250, 235).withAlpha(0.96f),
                 instabilityInfo.glow.withAlpha(0.12f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    const int knobRight = instabilitySlider.getRight();
    auto nameArea = juce::Rectangle<int>(knobRight + juce::roundToInt(12.0f * scale),
                                         area.getY() + 28,
                                         area.getRight() - knobRight - juce::roundToInt(16.0f * scale),
                                         20);
    drawGlowText(g, instabilityInfo.name, nameArea,
                 juce::Font(juce::FontOptions(16.0f * scale).withStyle("Bold")),
                 instabilityInfo.core.withAlpha(0.98f),
                 instabilityInfo.glow.withAlpha(0.18f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    auto descriptorArea = juce::Rectangle<int>(knobRight + juce::roundToInt(12.0f * scale),
                                               nameArea.getBottom() - 1,
                                               area.getRight() - knobRight - juce::roundToInt(16.0f * scale),
                                               14);
    drawGlowText(g, instabilityInfo.descriptor, descriptorArea,
                 juce::Font(juce::FontOptions(10.0f * scale).withStyle("Bold")),
                 instabilityInfo.rim.withAlpha(0.94f),
                 instabilityInfo.glow.withAlpha(0.10f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    auto valueArea = juce::Rectangle<int>(area.getRight() - 65,
                                          area.getY() + 6,
                                          55,
                                          18);
    drawGlowText(g, formatReadoutText(instabilitySlider.getValue() * 100.0f, 0, "%"), valueArea,
                 juce::Font(juce::FontOptions(11.0f).withStyle("Bold")),
                 instabilityInfo.rim.withAlpha(0.96f),
                 instabilityInfo.glow.withAlpha(0.08f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredRight);

    const float railLeft = r.getX() + 12.0f;
    const float railRight = r.getRight() - 12.0f;
    const float railY = r.getBottom() - 11.0f;
    g.setColour(instabilityInfo.rim.withAlpha(0.10f));
    g.drawLine(railLeft, railY, railRight, railY, 4.0f);
    g.setColour(instabilityInfo.rim.withAlpha(0.32f));
    g.drawLine(railLeft, railY, railRight, railY, 1.1f);

    const std::array<float, 4> stops { 0.0f, 0.33f, 0.66f, 1.0f };
    for (float stop : stops)
    {
        const auto stopInfo = getInstabilityDisplayInfo(stop);
        const float x = railLeft + stop * (railRight - railLeft);
        g.setColour(stopInfo.rim.withAlpha(0.28f));
        g.fillEllipse(x - 2.0f, railY - 2.0f, 4.0f, 4.0f);
    }

    const float markerX = railLeft + (float) instabilitySlider.getValue() * (railRight - railLeft);
    g.setColour(instabilityInfo.glow.withAlpha(0.28f));
    g.fillEllipse(markerX - 5.5f, railY - 5.5f, 11.0f, 11.0f);
    g.setColour(instabilityInfo.core.withAlpha(0.96f));
    g.fillEllipse(markerX - 3.1f, railY - 3.1f, 6.2f, 6.2f);

    drawScrew(g, { r.getX() + 10.0f, r.getY() + 10.0f }, 4.2f);
    drawScrew(g, { r.getRight() - 10.0f, r.getY() + 10.0f }, 4.2f);
}

void RootFlowAudioProcessorEditor::drawAtmosPod(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (area.isEmpty())
        return;

    const auto atmos = getAtmosDisplayInfo((float) atmosSlider.getValue());
    auto r = area.toFloat();

    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.fillRoundedRectangle(r.translated(0.0f, 1.5f), 11.0f);
    g.setColour(juce::Colours::black.withAlpha(0.36f));
    g.fillRoundedRectangle(r.translated(0.0f, 4.2f), 11.0f);
    drawTexturedWoodPanel(g, r, 10.0f,
                          atmos.panelTop.withAlpha(0.94f),
                          atmos.panelBottom.withAlpha(0.94f),
                          0.28f, 0.10f, 0.14f);
    drawPanelDepthOverlay(g, r.reduced(0.7f), 10.0f,
                          atmos.core.withAlpha(0.040f),
                          juce::Colours::black.withAlpha(0.16f));
    g.setColour(atmos.rim.withAlpha(0.16f));
    g.drawRoundedRectangle(r.reduced(0.8f), 10.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.56f));
    g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.5f);

    const float scale = juce::jmin(area.getWidth() / 195.0f, area.getHeight() / 64.0f);

    auto titleArea = juce::Rectangle<int>(area.getX() + 10, area.getY() + 4, area.getWidth() - 80, 18);
    drawGlowText(g, "ATMOS", titleArea,
                 juce::Font(juce::FontOptions(11.5f * scale).withStyle("Bold")),
                 juce::Colour(245, 250, 235).withAlpha(0.98f),
                 atmos.glow.withAlpha(0.14f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    auto atmosValueArea = juce::Rectangle<int>(area.getRight() - 60,
                                               area.getY() + 4,
                                               50,
                                               18);
    drawGlowText(g, formatReadoutText(atmosSlider.getValue() * 100.0f, 0, "%"), atmosValueArea,
                 juce::Font(juce::FontOptions(11.0f * scale).withStyle("Bold")),
                 atmos.rim.withAlpha(0.96f),
                 atmos.glow.withAlpha(0.14f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredRight);

    const int knobLeft = atmosSlider.getX();
    auto atmosNameArea = juce::Rectangle<int>(area.getX() + 10,
                                              area.getY() + 26,
                                              knobLeft - area.getX() - 10,
                                              22);
    drawGlowText(g, atmos.name, atmosNameArea,
                 juce::Font(juce::FontOptions(16.5f * scale).withStyle("Bold")),
                 atmos.core.withAlpha(0.98f),
                 atmos.glow.withAlpha(0.20f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    auto descriptorArea = juce::Rectangle<int>(area.getX() + 10,
                                               atmosNameArea.getBottom() - 1,
                                               knobLeft - area.getX() - 10,
                                               14);
    drawGlowText(g, atmos.descriptor, descriptorArea,
                 juce::Font(juce::FontOptions(10.0f * scale).withStyle("Bold")),
                 atmos.rim.withAlpha(0.94f),
                 atmos.glow.withAlpha(0.12f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    const float railLeft = r.getX() + 10.0f;
    const float railRight = r.getRight() - 12.0f;
    const float railY = r.getBottom() - 11.0f;
    g.setColour(atmos.rim.withAlpha(0.10f));
    g.drawLine(railLeft, railY, railRight, railY, 4.0f);
    g.setColour(atmos.rim.withAlpha(0.32f));
    g.drawLine(railLeft, railY, railRight, railY, 1.1f);

    const std::array<float, 4> stops { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f };
    for (float stop : stops)
    {
        const auto stopAtmos = getAtmosDisplayInfo(stop);
        const float x = railLeft + stop * (railRight - railLeft);
        g.setColour(stopAtmos.rim.withAlpha(0.28f));
        g.fillEllipse(x - 2.0f, railY - 2.0f, 4.0f, 4.0f);
    }

    const float markerX = railLeft + (float) atmosSlider.getValue() * (railRight - railLeft);
    g.setColour(atmos.glow.withAlpha(0.28f));
    g.fillEllipse(markerX - 5.5f, railY - 5.5f, 11.0f, 11.0f);
    g.setColour(atmos.core.withAlpha(0.96f));
    g.fillEllipse(markerX - 3.1f, railY - 3.1f, 6.2f, 6.2f);
}

void RootFlowAudioProcessorEditor::drawSeasonPod(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (area.isEmpty())
        return;

    const auto season = getSeasonDisplayInfo((float) seasonsSlider.getValue());
    auto r = area.toFloat();

    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.fillRoundedRectangle(r.translated(0.0f, 1.5f), 11.0f);
    g.setColour(juce::Colours::black.withAlpha(0.36f));
    g.fillRoundedRectangle(r.translated(0.0f, 4.2f), 11.0f);
    drawTexturedWoodPanel(g, r, 10.0f,
                          season.panelTop.withAlpha(0.94f),
                          season.panelBottom.withAlpha(0.94f),
                          0.28f, 0.10f, 0.14f);
    drawPanelDepthOverlay(g, r.reduced(0.7f), 10.0f,
                          season.core.withAlpha(0.040f),
                          juce::Colours::black.withAlpha(0.16f));
    g.setColour(season.rim.withAlpha(0.16f));
    g.drawRoundedRectangle(r.reduced(0.8f), 10.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.56f));
    g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.5f);

    const float scale = juce::jmin(area.getWidth() / 195.0f, area.getHeight() / 64.0f);

    auto titleArea = juce::Rectangle<int>(area.getX() + 10, area.getY() + 4, area.getWidth() - 80, 18);
    drawGlowText(g, "SEASONS", titleArea,
                 juce::Font(juce::FontOptions(11.5f * scale).withStyle("Bold")),
                 juce::Colour(245, 250, 235).withAlpha(0.98f),
                 season.glow.withAlpha(0.14f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    auto seasonValueArea = juce::Rectangle<int>(area.getRight() - 60,
                                                area.getY() + 4,
                                                50,
                                                18);
    drawGlowText(g, formatReadoutText(seasonsSlider.getValue() * 100.0f, 0, "%"), seasonValueArea,
                 juce::Font(juce::FontOptions(11.0f * scale).withStyle("Bold")),
                 season.rim.withAlpha(0.96f),
                 season.glow.withAlpha(0.14f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredRight);

    const int knobRight = seasonsSlider.getRight();
    auto seasonNameArea = juce::Rectangle<int>(knobRight + 10,
                                               area.getY() + 26,
                                               area.getRight() - knobRight - 15,
                                               22);
    drawGlowText(g, season.name, seasonNameArea,
                 juce::Font(juce::FontOptions(16.5f * scale).withStyle("Bold")),
                 season.core.withAlpha(0.98f),
                 season.glow.withAlpha(0.20f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    auto descriptorArea = juce::Rectangle<int>(knobRight + 10,
                                               seasonNameArea.getBottom() - 1,
                                               area.getRight() - knobRight - 15,
                                               14);
    drawGlowText(g, season.descriptor, descriptorArea,
                 juce::Font(juce::FontOptions(10.0f * scale).withStyle("Bold")),
                 season.rim.withAlpha(0.94f),
                 season.glow.withAlpha(0.12f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    const float railLeft = r.getX() + 10.0f;
    const float railRight = r.getRight() - 12.0f;
    const float railY = r.getBottom() - 11.0f;
    g.setColour(season.rim.withAlpha(0.10f));
    g.drawLine(railLeft, railY, railRight, railY, 4.0f);
    g.setColour(season.rim.withAlpha(0.32f));
    g.drawLine(railLeft, railY, railRight, railY, 1.1f);

    const std::array<float, 4> stops { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f };
    for (float stop : stops)
    {
        const auto stopSeason = getSeasonDisplayInfo(stop);
        const float x = railLeft + stop * (railRight - railLeft);
        g.setColour(stopSeason.rim.withAlpha(0.28f));
        g.fillEllipse(x - 2.0f, railY - 2.0f, 4.0f, 4.0f);
    }

    const float markerX = railLeft + (float) seasonsSlider.getValue() * (railRight - railLeft);
    g.setColour(season.glow.withAlpha(0.28f));
    g.fillEllipse(markerX - 5.5f, railY - 5.5f, 11.0f, 11.0f);
    g.setColour(season.core.withAlpha(0.96f));
    g.fillEllipse(markerX - 3.1f, railY - 3.1f, 6.2f, 6.2f);
}

void RootFlowAudioProcessorEditor::drawFxModule(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    auto r = area.toFloat();

    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.48f));
    g.fillRoundedRectangle(r.translated(0.0f, 7.5f), 14.0f);
    drawProceduralWoodPanel(g, r, 12.0f,
                            juce::Colour(48, 28, 16),
                            juce::Colour(88, 58, 34),
                            0.30f);
    drawPanelDepthOverlay(g, r.reduced(0.8f), 12.0f,
                          juce::Colour(196, 150, 100).withAlpha(0.060f),
                          juce::Colours::black.withAlpha(0.20f));
    g.setColour(juce::Colour(136, 102, 66).withAlpha(0.10f));
    g.drawRoundedRectangle(r.reduced(1.4f), 12.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.68f));
    g.drawRoundedRectangle(r.reduced(0.5f), 12.0f, 1.8f);

    auto plateOuter = r.reduced(7.5f);
    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.fillRoundedRectangle(plateOuter.translated(0.0f, 2.2f), 9.5f);
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.drawRoundedRectangle(plateOuter, 9.5f, 3.0f);
    auto plateFace = plateOuter.reduced(2.8f);
    drawProceduralWoodPanel(g, plateFace, 8.0f,
                            juce::Colour(44, 25, 15),
                            juce::Colour(98, 66, 40),
                            0.18f);
    drawPanelDepthOverlay(g, plateFace, 8.0f,
                          juce::Colour(214, 176, 130).withAlpha(0.035f),
                          juce::Colours::black.withAlpha(0.18f));
    g.setColour(juce::Colour(138, 102, 66).withAlpha(0.08f));
    g.drawRoundedRectangle(plateFace.reduced(0.8f), 8.0f, 0.9f);
    g.setColour(juce::Colours::black.withAlpha(0.50f));
    g.drawRoundedRectangle(plateFace, 8.0f, 1.2f);

    auto topArea = area.removeFromTop(34);
    auto titleStrip = topArea.toFloat().reduced(14.0f, 3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.fillRoundedRectangle(titleStrip.translated(0.0f, 1.5f), 7.0f);
    drawProceduralWoodPanel(g, titleStrip, 7.0f,
                            juce::Colour(52, 30, 18),
                            juce::Colour(94, 64, 38),
                            0.16f);
    drawPanelDepthOverlay(g, titleStrip, 7.0f,
                          juce::Colour(210, 178, 132).withAlpha(0.040f),
                          juce::Colours::black.withAlpha(0.16f));
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.drawRoundedRectangle(titleStrip, 7.0f, 1.0f);

    drawGlowText(g, title, topArea,
                 juce::Font(juce::FontOptions(17.0f).withStyle("Bold")),
                 juce::Colour(232, 255, 236),
                 juce::Colour(155, 255, 170).withAlpha(0.16f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centred);

    juce::Slider* fxSlider = nullptr;
    if (title == "BLOOM")      fxSlider = &bloomSlider;
    else if (title == "RAIN")  fxSlider = &rainSlider;
    else if (title == "SUN")   fxSlider = &sunSlider;

    if (fxSlider != nullptr)
    {
        auto recess = fxSlider->getBounds().toFloat().expanded(11.0f, 11.0f);
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        g.fillEllipse(recess.translated(0.0f, 1.0f));
        g.setColour(juce::Colours::black.withAlpha(0.36f));
        g.fillEllipse(recess.translated(0.0f, 4.0f));
        g.setColour(juce::Colour(14, 14, 14).withAlpha(0.96f));
        g.fillEllipse(recess);
        g.setColour(juce::Colour(234, 214, 186).withAlpha(0.030f));
        g.drawEllipse(recess.translated(0.0f, -0.6f).reduced(3.0f), 0.8f);
        g.setColour(juce::Colour(138, 106, 72).withAlpha(0.12f));
        g.drawEllipse(recess.reduced(2.2f), 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.84f));
        g.drawEllipse(recess, 2.2f);
    }

    const float modScale = juce::jmin(area.getWidth() / 250.0f, area.getHeight() / 120.0f);

    auto iconArea = juce::Rectangle<float>(r.getX() + 18.0f * modScale, r.getBottom() - 40.0f * modScale, 28.0f * modScale, 28.0f * modScale);
    drawFxIcon(g, iconArea, title);

    auto valueArea = juce::Rectangle<int>(area.getRight() - juce::roundToInt(90.0f * modScale), 
                                         area.getBottom() - juce::roundToInt(38.0f * modScale), 
                                         juce::roundToInt(84.0f * modScale), 
                                         juce::roundToInt(30.0f * modScale));
    float val = 0.0f;
    juce::String suffix;
    int decimals = 0;

    if (title == "BLOOM")      { val = bloomSlider.getValue() * 100.0f; suffix = "%"; }
    else if (title == "RAIN")  { val = getRainDelayTimeSeconds((float) rainSlider.getValue()) * 1000.0f; suffix = " ms"; decimals = 0; }
    else if (title == "SUN")   { val = sunSlider.getValue() * 100.0f; suffix = "%"; }

    drawGlowText(g, formatReadoutText(val, decimals, suffix), valueArea,
                 juce::Font(juce::FontOptions(17.0f * modScale).withStyle("Bold")),
                 juce::Colour(245, 250, 235).withAlpha(0.98f),
                 juce::Colour(118, 255, 138).withAlpha(0.12f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredRight);

    drawScrew(g, { r.getX() + 12.0f, r.getY() + 12.0f }, 4.8f);
    drawScrew(g, { r.getRight() - 12.0f, r.getY() + 12.0f }, 4.8f);
    drawScrew(g, { r.getX() + 12.0f, r.getBottom() - 12.0f }, 4.8f);
    drawScrew(g, { r.getRight() - 12.0f, r.getBottom() - 12.0f }, 4.8f);
}

void RootFlowAudioProcessorEditor::drawVisualizerHousing(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();
    const float panelBreathe = getPanelBreathe(r, 0.78f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), 16.0f);
    g.setColour(juce::Colours::black.withAlpha(0.48f));
    g.fillRoundedRectangle(r.translated(0.0f, 7.5f), 16.0f);
    g.setColour(getPanelBreathingColour(juce::Colour(18, 13, 11), r, 0.015f, 0.028f, 0.78f).withAlpha(0.95f));
    g.fillRoundedRectangle(r, 14.0f);
    drawPanelDepthOverlay(g, r.reduced(0.8f), 14.0f,
                          juce::Colour(198, 156, 110).withAlpha(0.055f),
                          juce::Colours::black.withAlpha(0.20f));
    drawPanelFinishOverlay(g,
                           r.reduced(1.0f),
                           13.2f,
                           juce::Colour(126, 255, 156),
                           0.022f,
                           0.011f);
    g.setColour(getPanelBreathingColour(juce::Colour(85, 62, 44), r.reduced(2.0f), 0.010f, 0.018f, 0.74f).withAlpha(0.20f + panelBreathe * 0.07f));
    g.fillRoundedRectangle(r.reduced(2.0f), 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.70f));
    g.drawRoundedRectangle(r, 14.0f, 2.0f);
    g.setColour(juce::Colour(180, 140, 88).withAlpha(0.12f));
    g.drawRoundedRectangle(r.reduced(4.0f), 12.0f, 1.2f);
    g.setColour(juce::Colours::black.withAlpha(0.32f));
    g.drawRoundedRectangle(r.reduced(8.0f), 10.0f, 1.4f);

    auto displayWell = r.reduced(11.0f, 10.0f);
    displayWell.removeFromTop(28.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(displayWell.translated(0.0f, 2.2f), 10.0f);
    g.setColour(juce::Colours::black.withAlpha(0.26f));
    g.fillRoundedRectangle(displayWell.translated(0.0f, 4.6f), 10.0f);
    g.setColour(getPanelBreathingColour(juce::Colour(10, 8, 7), displayWell, 0.012f, 0.020f, 0.72f).withAlpha(0.88f));
    g.fillRoundedRectangle(displayWell, 10.0f);
    drawPanelDepthOverlay(g, displayWell, 10.0f,
                          juce::Colour(165, 132, 96).withAlpha(0.040f),
                          juce::Colours::black.withAlpha(0.16f));
    drawPanelFinishOverlay(g,
                           displayWell.reduced(0.6f),
                           9.4f,
                           juce::Colour(110, 255, 142),
                           0.018f,
                           0.010f);
    g.setColour(juce::Colour(148, 114, 80).withAlpha(0.08f));
    g.drawRoundedRectangle(displayWell.reduced(1.2f), 10.0f, 0.9f);
    g.setColour(juce::Colours::black.withAlpha(0.76f));
    g.drawRoundedRectangle(displayWell, 10.0f, 1.3f);


    auto titleArea = area.removeFromTop(30);
    drawGlowText(g, "PLANT ENERGY", titleArea,
                 juce::Font(juce::FontOptions(17.0f).withStyle("Bold")),
                 juce::Colour(220, 255, 214).withAlpha(0.96f),
                 juce::Colour(120, 255, 128).withAlpha(0.18f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centred);

    const auto species = visualizer.getCurrentSpecies();
    auto speciesCol = (species == PlantEnergyVisualizer::SpeciesMode::lightFlow) ? juce::Colour(140, 255, 180) :
                     (species == PlantEnergyVisualizer::SpeciesMode::classicSolid) ? juce::Colour(255, 190, 110) :
                     juce::Colour(160, 200, 255);

    const auto gColor = visualizer.getCurrentColor();

    auto growthCol = (gColor == PlantEnergyVisualizer::GrowthColor::emerald) ? juce::Colour(80, 255, 160) :
                    (gColor == PlantEnergyVisualizer::GrowthColor::neon) ? juce::Colour(180, 255, 40) :
                    (gColor == PlantEnergyVisualizer::GrowthColor::moss) ? juce::Colour(120, 180, 80) :
                    juce::Colour(60, 140, 60);

    const float pulse = 0.85f + 0.15f * std::sin((float) juce::Time::getMillisecondCounterHiRes() * 0.006f);

    
    drawStatusLight(g, { r.getRight() - 24.0f, r.getY() + 18.0f }, speciesCol.withMultipliedAlpha(pulse), 5.2f);
    drawStatusLight(g, { r.getRight() - 46.0f, r.getY() + 18.0f }, growthCol.withMultipliedAlpha(pulse), 3.5f);

}

void RootFlowAudioProcessorEditor::drawKeyboardDrawer(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (area.isEmpty())
        return;

    auto r = area.toFloat();
    const float openAlpha = juce::jlimit(0.0f, 1.0f, keyboardDrawerOpenAmount);

    g.setColour(juce::Colours::black.withAlpha(0.16f * openAlpha));
    g.fillRect(innerRect.withY(area.getY()).reduced(2, 0));

    g.setColour(juce::Colours::black.withAlpha(0.18f + 0.14f * openAlpha));
    g.fillRoundedRectangle(r.translated(0.0f, 2.2f), 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.44f + 0.18f * openAlpha));
    g.fillRoundedRectangle(r.translated(0.0f, 7.0f), 14.0f);
    drawTexturedWoodPanel(g, r, 14.0f,
                          juce::Colour(62, 38, 22),
                          juce::Colour(24, 14, 9),
                          0.42f, 0.14f, 0.20f);
    drawPanelDepthOverlay(g, r.reduced(0.8f), 14.0f,
                          juce::Colour(194, 168, 126).withAlpha(0.05f + openAlpha * 0.03f),
                          juce::Colours::black.withAlpha(0.20f));
    drawPanelFinishOverlay(g,
                           r.reduced(0.8f),
                           13.2f,
                           juce::Colour(136, 255, 172),
                           0.020f + openAlpha * 0.010f,
                           0.010f);
    g.setColour(juce::Colours::black.withAlpha(0.72f));
    g.drawRoundedRectangle(r.reduced(0.5f), 14.0f, 1.8f);

    auto titleArea = area.removeFromTop(24);
    drawGlowText(g, "PLAY SURFACE", titleArea.removeFromLeft(180),
                 juce::Font(juce::FontOptions(15.0f).withStyle("Bold")),
                 juce::Colour(230, 255, 224).withAlpha(0.96f),
                 juce::Colour(132, 255, 180).withAlpha(0.18f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredLeft);

    drawGlowText(g, "CLICK OR USE A W S E D F T G Y H U J K", titleArea.reduced(0, 1),
                 juce::Font(juce::FontOptions(11.0f).withStyle("Bold")),
                 juce::Colour(214, 228, 198).withAlpha(0.72f),
                 juce::Colour(118, 255, 142).withAlpha(0.08f),
                 juce::Colours::transparentBlack,
                 juce::Justification::centredRight);
}

void RootFlowAudioProcessorEditor::drawFxIcon(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& type)
{
    g.setColour(juce::Colour(110, 255, 160).withAlpha(0.8f));
    
    if (type == "BLOOM")
    {
        juce::Path p;
        p.addCentredArc(area.getCentreX(), area.getCentreY() - 2, 6, 6, 0.0f, -juce::MathConstants<float>::pi * 0.75f, juce::MathConstants<float>::pi * 0.75f, true);
        p.lineTo(area.getCentreX() + 2, area.getCentreY() + 4);
        p.lineTo(area.getCentreX() - 2, area.getCentreY() + 4);
        p.closeSubPath();
        g.strokePath(p, juce::PathStrokeType(1.5f));
        g.fillRect (area.getCentreX() - 2.0f, area.getCentreY() + 5.0f, 4.0f, 1.5f);
    }
    else if (type == "RAIN")
    {
        g.setColour(juce::Colour(120, 220, 255).withAlpha(0.92f));
        juce::Path p;
        p.addEllipse(area.getX() + 2, area.getY() + 6, 8, 8);
        p.addEllipse(area.getX() + 8, area.getY() + 4, 10, 10);
        p.addEllipse(area.getX() + 14, area.getY() + 7, 7, 7);
        g.fillPath(p);
        // drops
        g.drawLine(area.getX() + 6, area.getBottom() - 4, area.getX() + 4, area.getBottom(), 1.2f);
        g.drawLine(area.getX() + 12, area.getBottom() - 4, area.getX() + 10, area.getBottom(), 1.2f);
        g.drawLine(area.getX() + 18, area.getBottom() - 4, area.getX() + 16, area.getBottom(), 1.2f);
    }
    else if (type == "SUN")
    {
        g.setColour(juce::Colour(255, 220, 110).withAlpha(0.95f));
        g.drawEllipse(area.reduced(6), 2.0f);
        for (int i = 0; i < 8; ++i) {
            auto angle = (float)i * juce::MathConstants<float>::halfPi * 0.5f;
            auto p1 = area.getCentre().getPointOnCircumference(6.5f, angle);
            auto p2 = area.getCentre().getPointOnCircumference(9.5f, angle);
            g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.5f);
        }
    }
}


void RootFlowAudioProcessorEditor::drawScrew(juce::Graphics& g, juce::Point<float> c, float radius)
{
    juce::ColourGradient screw(juce::Colour(90, 92, 95), c, juce::Colour(50, 52, 55), c.translated(radius, radius), true);
    g.setGradientFill(screw);
    g.fillEllipse(c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawEllipse(c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, 0.8f);
    g.setColour(juce::Colour(30, 30, 30).withAlpha(0.9f));
    g.drawLine(c.x - radius * 0.65f, c.y, c.x + radius * 0.65f, c.y, 1.5f);
}

void RootFlowAudioProcessorEditor::drawStatusLight(juce::Graphics& g, juce::Point<float> centre, juce::Colour colour, float radius)
{
    g.setColour(colour.withAlpha(0.16f));
    g.fillEllipse(centre.x - radius * 2.0f, centre.y - radius * 2.0f, radius * 4.0f, radius * 4.0f);
    g.setColour(colour.withAlpha(0.35f));
    g.fillEllipse(centre.x - radius * 1.25f, centre.y - radius * 1.25f, radius * 2.5f, radius * 2.5f);
    g.setColour(colour);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
}

void RootFlowAudioProcessorEditor::drawValueReadout(juce::Graphics& g, float value, juce::Rectangle<int> area,
                                                    const juce::String& suffix, int decimals,
                                                    juce::Colour coreColour, juce::Colour glowColour)
{
    drawGlowText(g, formatReadoutText(value, decimals, suffix), area,
                 juce::Font(juce::FontOptions(15.0f).withStyle("Bold")),
                 coreColour,
                 glowColour,
                 juce::Colours::transparentBlack,
                 juce::Justification::centred);
}

void RootFlowAudioProcessorEditor::setupKnob(juce::Slider& s, juce::Label& l, const juce::String& labelText, bool showVal)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    if (showVal)
        s.setNumDecimalPlacesToDisplay(0);
    s.setRotaryParameters(juce::degreesToRadians(135.0f), juce::degreesToRadians(405.0f), true);
    s.getProperties().set("rfInteractionBoost", 1.0);
    s.getProperties().set("rfInteractionActive", false);
    s.onDragStart = [this, &s]
    {
        if (audioProcessor.isMidiLearnArmed())
        {
            const auto paramID = s.getProperties()["rfParamID"].toString();
            if (paramID.isNotEmpty())
            {
                audioProcessor.setMidiLearnTargetParameterID(paramID);
                setMidiStatusMessage("MAP " + audioProcessor.getParameterDisplayName(paramID).toUpperCase() + " - TURN A MIDI KNOB",
                                     getStatusMapColour());
                midiIndicatorLevel = juce::jmax(midiIndicatorLevel, 0.56f);
                refreshHeaderControlState();
            }
        }

        s.getProperties().set("rfInteractionActive", true);
        s.getProperties().set("rfInteractionBoost", 1.42);
    };
    s.onDragEnd = [&s]
    {
        s.getProperties().set("rfInteractionActive", false);
        const auto boostVar = s.getProperties()["rfInteractionBoost"];
        const double currentBoost = boostVar.isVoid() ? 1.0 : (double) boostVar;
        s.getProperties().set("rfInteractionBoost", juce::jmax(currentBoost, 1.18));
    };
    s.onValueChange = [&s]
    {
        if (! s.isMouseOverOrDragging())
            return;

        const auto boostVar = s.getProperties()["rfInteractionBoost"];
        const double currentBoost = boostVar.isVoid() ? 1.0 : (double) boostVar;
        s.getProperties().set("rfInteractionBoost", juce::jmax(currentBoost, 1.30));
    };
    addAndMakeVisible(s);
    l.setText(labelText, juce::dontSendNotification);
    setupLabel(l, true);
    addAndMakeVisible(l);
}

void RootFlowAudioProcessorEditor::setupLabel(juce::Label& l, bool primary)
{
    if (primary)
    {
        l.setColour(juce::Label::textColourId, juce::Colour(238, 226, 188));
        l.setFont(juce::FontOptions(13.5f).withStyle("Bold"));
    }
    else
    {
        l.setColour(juce::Label::textColourId, juce::Colour(160, 255, 200).withAlpha(0.7f));
        l.setFont(juce::FontOptions(12.0f));
    }
    l.setJustificationType(juce::Justification::centred);
}


