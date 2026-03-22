#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace RootFlowDSP
{
inline float clamp01(float x) noexcept
{
    return juce::jlimit(0.0f, 1.0f, x);
}

inline float softSat(float x) noexcept
{
    return std::tanh(x);
}

inline float msToSamples(float ms, double sampleRate) noexcept
{
    return ms * (float) sampleRate / 1000.0f;
}

inline float sanitizeSample(float x, float limit = 1.5f) noexcept
{
    if (! std::isfinite(x))
        return 0.0f;

    return juce::jlimit(-limit, limit, x);
}

inline float sanitizeDelayInput(float x) noexcept
{
    return sanitizeSample(x, 1.75f);
}

inline float clampFeedbackGain(float x) noexcept
{
    return juce::jlimit(0.0f, 0.82f, x);
}

class BloomProcessor
{
public:
    static constexpr int maxDelaySamples = 8192;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

        mix.prepare(spec);
        mix.setWetLatency(0.0f);

        juce::dsp::ProcessSpec monoSpec { sampleRate, spec.maximumBlockSize, 1 };
        delayL.prepare(monoSpec);
        delayR.prepare(monoSpec);

        for (auto& filter : toneFilters)
        {
            filter.reset();
            filter.prepare(monoSpec);
            filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
            filter.setResonance(0.62f);
            filter.setCutoffFrequency(2400.0f);
        }

        wetSmoothed = 0.0f;
        wetGainSmoothed = 1.0f;
        haloSmoothed = 0.0f;
        widthSmoothed = 0.0f;
        cloudL = 0.0f;
        cloudR = 0.0f;
        lfoPhase = 0.0f;
        blossomPhase = 0.0f;
        pollenBurst = 0.0f;
        amount = 0.0f;
        energy = 0.0f;
        vitality = 0.5f;
        breath = 0.5f;
        canopyOpen = 0.5f;
        anchor = 0.5f;
        reset();
    }

    void reset()
    {
        delayL.reset();
        delayR.reset();
        mix.reset();
        for (auto& filter : toneFilters)
            filter.reset();
        cloudL = 0.0f;
        cloudR = 0.0f;
        pollenBurst = 0.0f;
    }

    void setParams(float bloomAmount,
                   float plantEnergy,
                   float sapVitality,
                   float pulseBreath,
                   float canopy,
                   float rootAnchor)
    {
        amount = clamp01(bloomAmount);
        energy = clamp01(plantEnergy);
        vitality = clamp01(sapVitality);
        breath = clamp01(pulseBreath);
        canopyOpen = clamp01(canopy);
        anchor = clamp01(rootAnchor);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (amount < 0.0001f && wetSmoothed < 0.0002f)
            return;

        auto block = juce::dsp::AudioBlock<float>(buffer);
        mix.pushDrySamples(block);

        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
        const bool stereo = right != nullptr;
        const int numSamples = buffer.getNumSamples();

        const float shapedAmount = std::pow(amount, 0.20f);
        const float baseDelayL = 3.0f + shapedAmount * 14.0f + canopyOpen * 3.2f;
        const float baseDelayR = 4.2f + shapedAmount * 16.0f + canopyOpen * 3.6f;
        const float spreadMs = 3.2f + shapedAmount * 15.0f + vitality * 2.8f + canopyOpen * 1.6f;
        const float microDepthMs = 1.0f + shapedAmount * 6.6f + energy * 1.8f;
        const float rateHz = 0.040f + shapedAmount * 0.30f + energy * 0.10f + vitality * 0.12f;
        const float blossomRateHz = 0.075f + breath * 0.26f + vitality * 0.12f;
        const float targetWet = juce::jlimit(0.0f, 0.84f,
                                             shapedAmount * (0.50f + energy * 0.08f + breath * 0.07f + vitality * 0.16f));
        const float targetWetGain = 1.0f + shapedAmount * (0.90f + vitality * 0.34f + canopyOpen * 0.18f);
        const float targetHalo = juce::jlimit(0.0f, 0.98f,
                                              shapedAmount * (0.36f + vitality * 0.34f + energy * 0.20f + (1.0f - anchor) * 0.12f));
        const float targetWidth = shapedAmount * (0.34f + canopyOpen * 0.34f + vitality * 0.12f);
        const float targetCutoff = 2200.0f + vitality * 3400.0f + canopyOpen * 1400.0f + energy * 1200.0f - anchor * 180.0f;
        const float targetResonance = juce::jlimit(0.44f, 0.90f,
                                                   0.62f + vitality * 0.16f + shapedAmount * 0.08f + (1.0f - anchor) * 0.08f);
        const float cloudCoeff = 0.036f + shapedAmount * 0.036f + breath * 0.014f;

        wetSmoothed += 0.03f * (targetWet - wetSmoothed);
        wetGainSmoothed += 0.03f * (targetWetGain - wetGainSmoothed);
        haloSmoothed += 0.03f * (targetHalo - haloSmoothed);
        widthSmoothed += 0.03f * (targetWidth - widthSmoothed);
        mix.setWetMixProportion(wetSmoothed);

        for (int i = 0; i < numSamples; ++i)
        {
            const float modA = std::sin(lfoPhase);
            const float modB = std::sin(lfoPhase * 1.17f + 1.4f);
            const float blossom = std::sin(blossomPhase);

            lfoPhase += juce::MathConstants<float>::twoPi * rateHz / (float) sampleRate;
            blossomPhase += juce::MathConstants<float>::twoPi * blossomRateHz / (float) sampleRate;
            if (lfoPhase > juce::MathConstants<float>::twoPi)
                lfoPhase -= juce::MathConstants<float>::twoPi;
            if (blossomPhase > juce::MathConstants<float>::twoPi)
                blossomPhase -= juce::MathConstants<float>::twoPi;

            const float blossomImpulse = juce::jmax(0.0f, blossom) * (0.24f + energy * 0.26f + vitality * 0.34f);
            pollenBurst += 0.028f * (blossomImpulse - pollenBurst);

            const float d0L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayL + modA * microDepthMs, sampleRate));
            const float d1L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayL + spreadMs * 0.58f - modB * microDepthMs * 0.6f, sampleRate));
            const float d2L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayL + spreadMs * 1.18f + modA * microDepthMs * 0.8f + pollenBurst * 2.2f, sampleRate));
            const float d3L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayL + spreadMs * 1.88f - modB * microDepthMs, sampleRate));
            const float d4L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayL + spreadMs * 2.68f + modA * microDepthMs * 1.2f + pollenBurst * 4.2f, sampleRate));

            const float d0R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayR - modB * microDepthMs, sampleRate));
            const float d1R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayR + spreadMs * 0.62f + modA * microDepthMs * 0.5f, sampleRate));
            const float d2R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayR + spreadMs * 1.26f - modB * microDepthMs * 0.7f + pollenBurst * 2.6f, sampleRate));
            const float d3R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayR + spreadMs * 1.96f + modA * microDepthMs, sampleRate));
            const float d4R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayR + spreadMs * 2.82f - modB * microDepthMs * 1.1f + pollenBurst * 4.8f, sampleRate));

            const float inL = sanitizeSample(left[i], 1.25f);
            const float inR = stereo ? sanitizeSample(right[i], 1.25f) : inL;

            const float tap0L = delayL.popSample(0, d0L, false);
            const float tap1L = delayL.popSample(0, d1L, false);
            const float tap2L = delayL.popSample(0, d2L, false);
            const float tap3L = delayL.popSample(0, d3L, true);
            const float tap4L = delayL.popSample(0, d4L, true);
            const float grainL = tap0L * 0.34f + tap1L * 0.28f + tap2L * 0.22f + tap3L * 0.14f + tap4L * 0.10f;
            const float sporeEdgeL = (tap1L - tap2L) * 0.34f + (tap0L - tap3L) * 0.18f + (tap4L - tap1L) * 0.12f;
            cloudL += cloudCoeff * ((grainL + sporeEdgeL * 0.72f + cloudR * 0.14f) - cloudL);
            cloudL = sanitizeSample(cloudL, 1.2f);

            toneFilters[0].setResonance(targetResonance);
            toneFilters[0].setCutoffFrequency(targetCutoff * (0.96f + pollenBurst * 0.18f));
            const float resonantL = sanitizeSample(toneFilters[0].processSample(0,
                                                                                grainL * (0.72f + haloSmoothed * 0.06f)
                                                                                + cloudL * (0.24f + haloSmoothed * 0.12f)
                                                                                + inL * 0.12f
                                                                                + inR * (0.04f + widthSmoothed * 0.06f)), 1.3f);
            const float haloL = std::tanh((sporeEdgeL * 0.72f + resonantL * (0.52f + haloSmoothed * 0.34f))
                                          * (1.0f + pollenBurst * 1.4f));
            const float sporeShineL = softSat((tap2L - tap4L * 0.52f + sporeEdgeL) * (0.78f + vitality * 0.34f + pollenBurst * 1.8f));
            float wetL = grainL * 0.34f
                       + resonantL * 0.82f
                       + cloudL * (0.22f + vitality * 0.10f)
                       + haloL * (0.10f + haloSmoothed * 0.30f)
                       + sporeShineL * (0.18f + haloSmoothed * 0.26f);
            wetL = sanitizeSample(softSat(wetL * wetGainSmoothed) * 0.92f, 1.2f);
            left[i] = wetL;

            if (stereo)
            {
                const float tap0R = delayR.popSample(0, d0R, false);
                const float tap1R = delayR.popSample(0, d1R, false);
                const float tap2R = delayR.popSample(0, d2R, false);
                const float tap3R = delayR.popSample(0, d3R, true);
                const float tap4R = delayR.popSample(0, d4R, true);
                const float grainR = tap0R * 0.34f + tap1R * 0.28f + tap2R * 0.22f + tap3R * 0.14f + tap4R * 0.10f;
                const float sporeEdgeR = (tap1R - tap2R) * 0.34f + (tap0R - tap3R) * 0.18f + (tap4R - tap1R) * 0.12f;
                cloudR += cloudCoeff * ((grainR + sporeEdgeR * 0.72f + cloudL * 0.14f) - cloudR);
                cloudR = sanitizeSample(cloudR, 1.2f);

                toneFilters[1].setResonance(targetResonance);
                toneFilters[1].setCutoffFrequency(targetCutoff * (1.04f + pollenBurst * 0.16f));
                const float resonantR = sanitizeSample(toneFilters[1].processSample(0,
                                                                                    grainR * (0.72f + haloSmoothed * 0.06f)
                                                                                    + cloudR * (0.24f + haloSmoothed * 0.12f)
                                                                                    + inR * 0.12f
                                                                                    + inL * (0.04f + widthSmoothed * 0.06f)), 1.3f);
                const float haloR = std::tanh((sporeEdgeR * 0.72f + resonantR * (0.52f + haloSmoothed * 0.34f))
                                              * (1.0f + pollenBurst * 1.4f));
                const float sporeShineR = softSat((tap2R - tap4R * 0.52f + sporeEdgeR) * (0.78f + vitality * 0.34f + pollenBurst * 1.8f));
                float wetR = grainR * 0.34f
                           + resonantR * 0.82f
                           + cloudR * (0.22f + vitality * 0.10f)
                           + haloR * (0.10f + haloSmoothed * 0.30f)
                           + sporeShineR * (0.18f + haloSmoothed * 0.26f);
                wetR = sanitizeSample(softSat(wetR * wetGainSmoothed) * 0.92f, 1.2f);
                right[i] = wetR;

                delayL.pushSample(0, sanitizeDelayInput(inL
                                                        + resonantL * 0.14f
                                                        + cloudR * (0.06f + widthSmoothed * 0.08f)
                                                        + haloL * 0.08f
                                                        + sporeShineL * 0.10f));
                delayR.pushSample(0, sanitizeDelayInput(inR
                                                        + resonantR * 0.14f
                                                        + cloudL * (0.06f + widthSmoothed * 0.08f)
                                                        + haloR * 0.08f
                                                        + sporeShineR * 0.10f));
            }
            else
            {
                cloudR = cloudL;
                delayL.pushSample(0, sanitizeDelayInput(inL + resonantL * 0.16f + cloudL * 0.08f + haloL * 0.08f + sporeShineL * 0.10f));
            }
        }

        mix.mixWetSamples(block);
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayL { maxDelaySamples };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayR { maxDelaySamples };
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> toneFilters;
    juce::dsp::DryWetMixer<float> mix;
    double sampleRate = 44100.0;
    float amount = 0.0f;
    float energy = 0.0f;
    float vitality = 0.5f;
    float breath = 0.5f;
    float canopyOpen = 0.5f;
    float anchor = 0.5f;
    float lfoPhase = 0.0f;
    float blossomPhase = 0.0f;
    float pollenBurst = 0.0f;
    float wetSmoothed = 0.0f;
    float wetGainSmoothed = 1.0f;
    float haloSmoothed = 0.0f;
    float widthSmoothed = 0.0f;
    float cloudL = 0.0f;
    float cloudR = 0.0f;
};

class RainProcessor
{
public:
    static constexpr int maxDelaySamples = 262144;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

        mix.prepare(spec);
        mix.setWetLatency(0.0f);

        juce::dsp::ProcessSpec monoSpec { sampleRate, spec.maximumBlockSize, 1 };
        delayL.prepare(monoSpec);
        delayR.prepare(monoSpec);

        for (auto& filter : feedbackFilters)
        {
            filter.reset();
            filter.prepare(monoSpec);
            filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            filter.setResonance(0.38f);
            filter.setCutoffFrequency(4300.0f);
        }

        wetSmoothed = 0.0f;
        wetGainSmoothed = 1.0f;
        feedbackSmoothed = 0.0f;
        widthSmoothed = 0.0f;
        amount = 0.0f;
        energy = 0.0f;
        flow = 0.5f;
        texture = 0.5f;
        breath = 0.5f;
        canopyOpen = 0.5f;
        wanderPhase = 0.0f;
        shimmerPhase = 0.0f;
        diffuseMemoryL = 0.0f;
        diffuseMemoryR = 0.0f;
        canopyBloomL = 0.0f;
        canopyBloomR = 0.0f;
        canopyLeavesL.fill(0.0f);
        canopyLeavesR.fill(0.0f);
        reset();
    }

    void reset()
    {
        delayL.reset();
        delayR.reset();
        mix.reset();
        for (auto& filter : feedbackFilters)
            filter.reset();
        wetSmoothed = 0.0f;
        wetGainSmoothed = 1.0f;
        feedbackSmoothed = 0.0f;
        widthSmoothed = 0.0f;
        wanderPhase = 0.0f;
        shimmerPhase = 0.0f;
        diffuseMemoryL = 0.0f;
        diffuseMemoryR = 0.0f;
        canopyBloomL = 0.0f;
        canopyBloomR = 0.0f;
        canopyLeavesL.fill(0.0f);
        canopyLeavesR.fill(0.0f);
    }

    void setParams(float rainAmount,
                   float plantEnergy,
                   float sapFlow,
                   float sapTexture,
                   float pulseBreath,
                   float canopy)
    {
        amount = clamp01(rainAmount);
        energy = clamp01(plantEnergy);
        flow = clamp01(sapFlow);
        texture = clamp01(sapTexture);
        breath = clamp01(pulseBreath);
        canopyOpen = clamp01(canopy);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (amount < 0.0001f && wetSmoothed < 0.0002f)
        {
            reset();
            return;
        }

        auto block = juce::dsp::AudioBlock<float>(buffer);
        mix.pushDrySamples(block);

        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
        const bool stereo = right != nullptr;
        const int numSamples = buffer.getNumSamples();

        constexpr std::array<float, 6> tapWeights { 0.28f, 0.23f, 0.18f, 0.13f, 0.10f, 0.08f };
        constexpr std::array<float, 6> stageCoefficients { 0.24f, 0.18f, 0.14f, 0.10f, 0.075f, 0.055f };

        const float shapedAmount = std::pow(amount, 0.13f);
        const float baseMs = 78.0f + shapedAmount * 1480.0f + flow * 92.0f + canopyOpen * 120.0f;
        const float targetFeedback = juce::jlimit(0.0f, 0.86f,
                                                  shapedAmount * (0.78f + energy * 0.10f + texture * 0.10f + canopyOpen * 0.12f));
        const float targetWet = juce::jlimit(0.0f, 0.90f,
                                             shapedAmount * (0.54f + energy * 0.08f + breath * 0.10f + canopyOpen * 0.10f));
        const float targetWetGain = 1.0f + shapedAmount * (0.40f + canopyOpen * 0.18f + flow * 0.14f);
        const float targetWidth = shapedAmount * (0.46f + flow * 0.22f + canopyOpen * 0.20f);
        const float targetCutoff = 1180.0f + canopyOpen * 1180.0f + texture * 920.0f + energy * 640.0f;

        const float smoothingRate = amount < 0.0008f ? 0.070f : 0.030f;
        wetSmoothed += smoothingRate * (targetWet - wetSmoothed);
        wetGainSmoothed += smoothingRate * (targetWetGain - wetGainSmoothed);
        feedbackSmoothed += smoothingRate * (targetFeedback - feedbackSmoothed);
        widthSmoothed += smoothingRate * (targetWidth - widthSmoothed);
        mix.setWetMixProportion(wetSmoothed);

        const float wanderRateHz = 0.018f + shapedAmount * 0.100f + canopyOpen * 0.032f;
        const float shimmerRateHz = 0.033f + texture * 0.074f + breath * 0.052f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float wander = std::sin(wanderPhase);
            const float shimmer = std::sin(shimmerPhase * 1.19f + 0.8f);
            wanderPhase += juce::MathConstants<float>::twoPi * wanderRateHz / (float) sampleRate;
            shimmerPhase += juce::MathConstants<float>::twoPi * shimmerRateHz / (float) sampleRate;
            if (wanderPhase > juce::MathConstants<float>::twoPi)
                wanderPhase -= juce::MathConstants<float>::twoPi;
            if (shimmerPhase > juce::MathConstants<float>::twoPi)
                shimmerPhase -= juce::MathConstants<float>::twoPi;

            const float driftMs = wander * (7.0f + shapedAmount * 16.0f) + shimmer * (2.0f + texture * 4.8f);
            const float d0L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 0.24f + driftMs * 0.26f + 12.0f, sampleRate));
            const float d1L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 0.54f + driftMs * 0.58f + flow * 10.0f, sampleRate));
            const float d2L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 0.92f - driftMs * 0.10f + energy * 16.0f, sampleRate));
            const float d3L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 1.34f + flow * 24.0f + shimmer * 6.0f, sampleRate));
            const float d4L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 1.86f + canopyOpen * 34.0f - wander * 6.0f, sampleRate));
            const float d5L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 2.52f + canopyOpen * 54.0f + shimmer * 8.0f, sampleRate));

            const float d0R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 0.28f - driftMs * 0.18f + 15.0f, sampleRate));
            const float d1R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 0.60f - driftMs * 0.16f + flow * 12.0f, sampleRate));
            const float d2R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 1.00f + driftMs * 0.16f + energy * 14.0f, sampleRate));
            const float d3R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 1.46f + flow * 28.0f - shimmer * 5.0f, sampleRate));
            const float d4R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 2.06f + canopyOpen * 38.0f + wander * 7.0f, sampleRate));
            const float d5R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseMs * 2.76f + canopyOpen * 58.0f - shimmer * 7.0f, sampleRate));

            const float inL = sanitizeSample(left[i], 1.25f);
            const float inR = stereo ? sanitizeSample(right[i], 1.25f) : inL;

            feedbackFilters[0].setCutoffFrequency(targetCutoff * 0.98f);
            const float tapL0 = delayL.popSample(0, d0L, false);
            const float tapL1 = delayL.popSample(0, d1L, false);
            const float tapL2 = delayL.popSample(0, d2L, false);
            const float tapL3 = delayL.popSample(0, d3L, false);
            const float tapL4 = delayL.popSample(0, d4L, false);
            const float tapL5 = delayL.popSample(0, d5L, true);
            const std::array<float, 6> tapsL { tapL0, tapL1, tapL2, tapL3, tapL4, tapL5 };
            const float earlyMistL = tapL0 * 0.64f + tapL1 * 0.36f + tapL2 * 0.12f;
            const float leafRustleL = softSat((tapL1 - tapL3 * 0.72f + tapL5 * 0.36f)
                                              * (0.28f + texture * 0.24f + canopyOpen * 0.18f + std::abs(shimmer) * 0.08f));

            float branchSumL = 0.0f;
            float lateShadowL = 0.0f;
            float previousLeafL = 0.0f;
            for (size_t stage = 0; stage < tapsL.size(); ++stage)
            {
                const float darkness = juce::jlimit(0.22f, 1.0f, 1.0f - (float) stage * (0.14f + canopyOpen * 0.015f));
                const float coeff = juce::jlimit(0.060f, 0.26f,
                                                 stageCoefficients[stage] + texture * 0.012f + canopyOpen * 0.008f);
                const float branchFeed = previousLeafL * (0.08f + (float) stage * 0.01f);
                const float rustleSend = leafRustleL * (0.04f + (float) stage * 0.02f);
                const float stageTarget = tapsL[stage] * darkness + branchFeed + rustleSend;
                canopyLeavesL[stage] += coeff * (stageTarget - canopyLeavesL[stage]);
                const float leaf = sanitizeSample(softSat(canopyLeavesL[stage] * (0.92f - (float) stage * 0.05f
                                                                                 + canopyOpen * 0.10f + energy * 0.04f)), 1.25f);
                branchSumL += leaf * tapWeights[stage];
                if (stage >= 2)
                    lateShadowL += leaf * (0.10f + 0.03f * (float) (stage - 2));
                previousLeafL = leaf;
            }

            float wetL = branchSumL + diffuseMemoryR * (0.12f + widthSmoothed * 0.06f) + canopyBloomR * (0.10f + canopyOpen * 0.06f);
            wetL = sanitizeSample(feedbackFilters[0].processSample(0, wetL), 1.35f);
            canopyBloomL += 0.032f * ((lateShadowL * (0.24f + canopyOpen * 0.09f) + leafRustleL * 0.16f + earlyMistL * 0.20f) - canopyBloomL);
            canopyBloomL = sanitizeSample(canopyBloomL, 1.15f);
            const float forestAirL = earlyMistL * (0.22f + texture * 0.08f)
                                   + diffuseMemoryL * (0.12f + energy * 0.05f)
                                   + canopyBloomL * (0.16f + canopyOpen * 0.10f);
            const float wetLPre = (wetL + lateShadowL * (0.24f + canopyOpen * 0.10f) + forestAirL) * wetGainSmoothed * 0.96f;
            wetL = sanitizeSample(wetLPre * 0.72f + softSat(wetLPre * 0.82f) * 0.28f, 1.2f);
            diffuseMemoryL += 0.040f * ((lateShadowL * 0.44f + wetL * 0.18f + leafRustleL * 0.10f) - diffuseMemoryL);
            diffuseMemoryL = sanitizeSample(diffuseMemoryL, 1.2f);

            float wetR = wetL;
            if (stereo)
            {
                feedbackFilters[1].setCutoffFrequency(targetCutoff * 1.02f);
                const float tapR0 = delayR.popSample(0, d0R, false);
                const float tapR1 = delayR.popSample(0, d1R, false);
                const float tapR2 = delayR.popSample(0, d2R, false);
                const float tapR3 = delayR.popSample(0, d3R, false);
                const float tapR4 = delayR.popSample(0, d4R, false);
                const float tapR5 = delayR.popSample(0, d5R, true);
                const std::array<float, 6> tapsR { tapR0, tapR1, tapR2, tapR3, tapR4, tapR5 };
                const float earlyMistR = tapR0 * 0.64f + tapR1 * 0.36f + tapR2 * 0.12f;
                const float leafRustleR = softSat((tapR1 - tapR3 * 0.72f + tapR5 * 0.36f)
                                                  * (0.28f + texture * 0.24f + canopyOpen * 0.18f + std::abs(shimmer) * 0.08f));

                float branchSumR = 0.0f;
                float lateShadowR = 0.0f;
                float previousLeafR = 0.0f;
                for (size_t stage = 0; stage < tapsR.size(); ++stage)
                {
                    const float darkness = juce::jlimit(0.22f, 1.0f, 1.0f - (float) stage * (0.14f + canopyOpen * 0.015f));
                    const float coeff = juce::jlimit(0.060f, 0.26f,
                                                     stageCoefficients[stage] + texture * 0.012f + canopyOpen * 0.008f);
                    const float branchFeed = previousLeafR * (0.08f + (float) stage * 0.01f);
                    const float rustleSend = leafRustleR * (0.04f + (float) stage * 0.02f);
                    const float stageTarget = tapsR[stage] * darkness + branchFeed + rustleSend;
                    canopyLeavesR[stage] += coeff * (stageTarget - canopyLeavesR[stage]);
                    const float leaf = sanitizeSample(softSat(canopyLeavesR[stage] * (0.92f - (float) stage * 0.05f
                                                                                     + canopyOpen * 0.10f + energy * 0.04f)), 1.25f);
                    branchSumR += leaf * tapWeights[stage];
                    if (stage >= 2)
                        lateShadowR += leaf * (0.10f + 0.03f * (float) (stage - 2));
                    previousLeafR = leaf;
                }

                wetR = branchSumR + diffuseMemoryL * (0.12f + widthSmoothed * 0.06f) + canopyBloomL * (0.10f + canopyOpen * 0.06f);
                wetR = sanitizeSample(feedbackFilters[1].processSample(0, wetR), 1.35f);
                canopyBloomR += 0.032f * ((lateShadowR * (0.24f + canopyOpen * 0.09f) + leafRustleR * 0.16f + earlyMistR * 0.20f) - canopyBloomR);
                canopyBloomR = sanitizeSample(canopyBloomR, 1.15f);
                const float forestAirR = earlyMistR * (0.22f + texture * 0.08f)
                                       + diffuseMemoryR * (0.12f + energy * 0.05f)
                                       + canopyBloomR * (0.16f + canopyOpen * 0.10f);
                const float wetRPre = (wetR + lateShadowR * (0.24f + canopyOpen * 0.10f) + forestAirR) * wetGainSmoothed * 0.96f;
                wetR = sanitizeSample(wetRPre * 0.72f + softSat(wetRPre * 0.82f) * 0.28f, 1.2f);
                diffuseMemoryR += 0.040f * ((lateShadowR * 0.44f + wetR * 0.18f + leafRustleR * 0.10f) - diffuseMemoryR);
                diffuseMemoryR = sanitizeSample(diffuseMemoryR, 1.2f);
            }
            else
            {
                diffuseMemoryR = diffuseMemoryL;
                canopyBloomR = canopyBloomL;
                canopyLeavesR = canopyLeavesL;
            }

            const float width = widthSmoothed;
            const float crossL = wetR * (0.18f + width * 0.20f) + canopyBloomR * (0.08f + width * 0.10f) + diffuseMemoryR * 0.04f;
            const float crossR = wetL * (0.18f + width * 0.20f) + canopyBloomL * (0.08f + width * 0.10f) + diffuseMemoryL * 0.04f;
            const float refeedL = diffuseMemoryL * 0.16f + canopyBloomL * 0.12f + wetL * 0.07f;
            const float refeedR = diffuseMemoryR * 0.16f + canopyBloomR * 0.12f + wetR * 0.07f;
            const float feedbackGain = clampFeedbackGain(feedbackSmoothed * (0.58f + width * 0.12f));

            delayL.pushSample(0, sanitizeDelayInput(inL + crossL + refeedL * feedbackGain));
            if (stereo)
                delayR.pushSample(0, sanitizeDelayInput(inR + crossR + refeedR * feedbackGain));

            left[i] = sanitizeSample(wetL, 1.2f);
            if (stereo)
                right[i] = sanitizeSample(wetR, 1.2f);
        }

        mix.mixWetSamples(block);
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayL { maxDelaySamples };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayR { maxDelaySamples };
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> feedbackFilters;
    juce::dsp::DryWetMixer<float> mix;
    double sampleRate = 44100.0;
    float amount = 0.0f;
    float energy = 0.0f;
    float flow = 0.5f;
    float texture = 0.5f;
    float breath = 0.5f;
    float canopyOpen = 0.5f;
    float wetSmoothed = 0.0f;
    float wetGainSmoothed = 1.0f;
    float feedbackSmoothed = 0.30f;
    float widthSmoothed = 0.0f;
    float wanderPhase = 0.0f;
    float shimmerPhase = 0.0f;
    float diffuseMemoryL = 0.0f;
    float diffuseMemoryR = 0.0f;
    float canopyBloomL = 0.0f;
    float canopyBloomR = 0.0f;
    std::array<float, 6> canopyLeavesL {};
    std::array<float, 6> canopyLeavesR {};
};

class SunProcessor
{
public:
    static constexpr int maxDelaySamples = 65536;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

        juce::dsp::ProcessSpec monoSpec { sampleRate, spec.maximumBlockSize, 1 };
        delayL.prepare(monoSpec);
        delayR.prepare(monoSpec);
        for (auto& filter : toneFilters)
        {
            filter.reset();
            filter.prepare(monoSpec);
            filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            filter.setResonance(0.18f);
            filter.setCutoffFrequency(2600.0f);
        }

        amount = 0.0f;
        energy = 0.0f;
        depth = 0.5f;
        vitality = 0.5f;
        breath = 0.5f;
        canopyOpen = 0.5f;
        driveSmoothed = 1.0f;
        trimSmoothed = 1.0f;
        cutoffSmoothed = 2600.0f;
        wetSmoothed = 0.0f;
        feedbackSmoothed = 0.12f;
        widthSmoothed = 0.0f;
        modPhase = 0.0f;
        stormPhase = 0.0f;
        stormLevel = 0.0f;
        breathLevel = 0.0f;
        humusL = 0.0f;
        humusR = 0.0f;
        mycelNodesL.fill(0.0f);
        mycelNodesR.fill(0.0f);
        reset();
    }

    void reset()
    {
        delayL.reset();
        delayR.reset();
        for (auto& filter : toneFilters)
            filter.reset();
        driveSmoothed = 1.0f;
        trimSmoothed = 1.0f;
        cutoffSmoothed = 2600.0f;
        wetSmoothed = 0.0f;
        feedbackSmoothed = 0.0f;
        widthSmoothed = 0.0f;
        humusL = 0.0f;
        humusR = 0.0f;
        breathLevel = 0.0f;
        modPhase = 0.0f;
        stormPhase = 0.0f;
        stormLevel = 0.0f;
        mycelNodesL.fill(0.0f);
        mycelNodesR.fill(0.0f);
    }

    void setParams(float sunAmount,
                   float plantEnergy,
                   float rootDepth,
                   float sapVitality,
                   float pulseBreath,
                   float canopy)
    {
        amount = clamp01(sunAmount);
        energy = clamp01(plantEnergy);
        depth = clamp01(rootDepth);
        vitality = clamp01(sapVitality);
        breath = clamp01(pulseBreath);
        canopyOpen = clamp01(canopy);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (amount < 0.0001f && wetSmoothed < 0.0002f)
        {
            reset();
            return;
        }

        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
        const bool stereo = right != nullptr;
        const int numSamples = buffer.getNumSamples();

        constexpr std::array<float, 5> nodeWeights { 0.12f, 0.18f, 0.24f, 0.24f, 0.22f };
        constexpr std::array<float, 5> nodeCoefficients { 0.18f, 0.14f, 0.105f, 0.078f, 0.058f };

        const float shapedAmount = std::pow(amount, 0.07f);
        const float targetDrive = 1.0f + shapedAmount * (1.80f + depth * 0.28f + energy * 0.24f);
        const float targetTrim = 1.0f + shapedAmount * (0.20f + depth * 0.05f + breath * 0.05f);
        const float targetCutoff = 1260.0f + canopyOpen * 620.0f + vitality * 420.0f + breath * 320.0f + (1.0f - shapedAmount) * 220.0f;
        const float targetWet = juce::jlimit(0.0f, 0.84f,
                                             shapedAmount * (0.62f + vitality * 0.12f + breath * 0.12f + depth * 0.10f));
        const float targetFeedback = juce::jlimit(0.0f, 0.68f,
                                                  shapedAmount * (0.54f + vitality * 0.12f + depth * 0.12f + canopyOpen * 0.08f));
        const float targetWidth = shapedAmount * (0.32f + canopyOpen * 0.26f + vitality * 0.12f);
        const float baseDelayMs = 18.0f + shapedAmount * 44.0f + depth * 18.0f;
        const float networkDepthMs = 220.0f + shapedAmount * 920.0f + depth * 320.0f + canopyOpen * 140.0f;
        const float modRateHz = 0.032f + shapedAmount * 0.075f + breath * 0.11f;
        const float stormRateHz = 0.050f + energy * 0.11f + vitality * 0.05f;

        for (int i = 0; i < numSamples; ++i)
        {
            driveSmoothed += 0.0048f * (targetDrive - driveSmoothed);
            trimSmoothed += 0.0048f * (targetTrim - trimSmoothed);
            cutoffSmoothed += 0.0048f * (targetCutoff - cutoffSmoothed);
            wetSmoothed += 0.0048f * (targetWet - wetSmoothed);
            feedbackSmoothed += 0.0048f * (targetFeedback - feedbackSmoothed);
            widthSmoothed += 0.0048f * (targetWidth - widthSmoothed);

            const float lfoA = std::sin(modPhase);
            const float lfoB = std::sin(modPhase * 1.31f + 1.8f);
            modPhase += juce::MathConstants<float>::twoPi * modRateHz / (float) sampleRate;
            if (modPhase > juce::MathConstants<float>::twoPi)
                modPhase -= juce::MathConstants<float>::twoPi;

            const float mycelPulse = juce::jmax(0.0f,
                                                std::sin(stormPhase) * 0.56f
                                                + std::sin(stormPhase * 1.73f + 0.7f) * 0.28f);
            stormPhase += juce::MathConstants<float>::twoPi * stormRateHz / (float) sampleRate;
            if (stormPhase > juce::MathConstants<float>::twoPi)
                stormPhase -= juce::MathConstants<float>::twoPi;

            const float dryL = sanitizeSample(left[i], 1.25f);
            const float dryR = stereo ? sanitizeSample(right[i], 1.25f) : dryL;
            const float excite = clamp01((std::abs(dryL) + std::abs(dryR)) * 0.70f + energy * 0.24f + breath * 0.18f);
            breathLevel += 0.0072f * (excite - breathLevel);

            // Keep the internal storm motion present, but avoid the old re-triggering surge.
            const float stormTarget = std::pow(mycelPulse, 2.4f) * (0.24f + breathLevel * 0.34f);
            stormLevel += 0.0080f * (stormTarget - stormLevel);
            const float breathBloom = 0.74f + breathLevel * 0.36f + (0.5f + 0.5f * lfoA) * (0.08f + breath * 0.08f);

            const float d0L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayMs * 0.85f + lfoA * 3.0f + breathLevel * 8.0f, sampleRate));
            const float d1L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayMs + networkDepthMs * 0.28f + lfoB * 8.0f + depth * 10.0f, sampleRate));
            const float d2L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayMs + networkDepthMs * 0.62f - lfoA * 11.0f + canopyOpen * 14.0f, sampleRate));
            const float d3L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayMs + networkDepthMs * 1.08f + lfoB * 18.0f + depth * 22.0f, sampleRate));
            const float d4L = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayMs + networkDepthMs * 1.72f - lfoA * 24.0f + canopyOpen * 34.0f, sampleRate));

            const float d0R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayMs * 0.92f - lfoB * 2.5f + breathLevel * 7.0f, sampleRate));
            const float d1R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayMs * 1.08f + networkDepthMs * 0.32f - lfoA * 7.0f + depth * 12.0f, sampleRate));
            const float d2R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayMs * 1.12f + networkDepthMs * 0.68f + lfoB * 10.0f + canopyOpen * 16.0f, sampleRate));
            const float d3R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayMs * 1.18f + networkDepthMs * 1.18f - lfoA * 16.0f + depth * 24.0f, sampleRate));
            const float d4R = juce::jlimit(1.0f, (float) (maxDelaySamples - 2),
                                           msToSamples(baseDelayMs * 1.24f + networkDepthMs * 1.84f + lfoB * 22.0f + canopyOpen * 38.0f, sampleRate));

            const float tapL0 = sanitizeSample(delayL.popSample(0, d0L, false), 1.35f);
            const float tapL1 = sanitizeSample(delayL.popSample(0, d1L, false), 1.35f);
            const float tapL2 = sanitizeSample(delayL.popSample(0, d2L, false), 1.35f);
            const float tapL3 = sanitizeSample(delayL.popSample(0, d3L, false), 1.35f);
            const float tapL4 = sanitizeSample(delayL.popSample(0, d4L, true), 1.35f);
            const std::array<float, 5> tapsL { tapL0, tapL1, tapL2, tapL3, tapL4 };
            const float rootDustL = tapL0 * 0.32f + tapL1 * 0.24f + tapL2 * 0.14f;

            float networkL = 0.0f;
            float deepTailL = 0.0f;
            float previousNodeL = 0.0f;
            for (size_t stage = 0; stage < tapsL.size(); ++stage)
            {
                const float darkness = juce::jlimit(0.22f, 1.0f, 1.0f - (float) stage * (0.14f + depth * 0.01f));
                const float coeff = juce::jlimit(0.050f, 0.22f,
                                                 nodeCoefficients[stage] + breathLevel * 0.014f + stormLevel * 0.024f);
                const float networkFeed = previousNodeL * (0.08f + (float) stage * 0.015f);
                const float humusFeed = humusR * (0.04f + (float) stage * 0.012f);
                const float stageTarget = tapsL[stage] * darkness + networkFeed + humusFeed + rootDustL * (0.08f + (float) stage * 0.018f);
                mycelNodesL[stage] += coeff * (stageTarget - mycelNodesL[stage]);
                const float node = sanitizeSample(softSat(mycelNodesL[stage] * (0.92f - (float) stage * 0.06f
                                                                                + depth * 0.14f + stormLevel * 0.24f)), 1.2f);
                networkL += node * nodeWeights[stage];
                if (stage >= 2)
                    deepTailL += node * (0.18f + 0.05f * (float) (stage - 2));
                previousNodeL = node;
            }

            toneFilters[0].setCutoffFrequency(cutoffSmoothed * (0.92f + breathLevel * 0.12f + stormLevel * 0.04f));
            humusL += 0.036f * ((deepTailL * (0.58f + depth * 0.18f) + networkL * 0.22f + rootDustL * 0.18f) - humusL);
            humusL = sanitizeSample(humusL, 1.15f);

            float wetL = networkL + humusR * (0.42f + widthSmoothed * 0.16f) + deepTailL * 0.38f + rootDustL * 0.12f;
            wetL = sanitizeSample(toneFilters[0].processSample(0, wetL), 1.25f);
            const float wetLPre = (wetL + humusL * (0.66f + depth * 0.18f) + rootDustL * 0.18f) * (1.22f + breathBloom * 0.38f);
            wetL = sanitizeSample(wetLPre * 0.48f + softSat(wetLPre * (1.36f + stormLevel * 0.36f)) * 0.52f, 1.2f);
            const float warmL = dryL * 0.28f + softSat(dryL * driveSmoothed * (0.80f + breathLevel * 0.08f)) * 0.72f;
            const float dryDuck = juce::jlimit(0.0f, 0.64f,
                                               wetSmoothed * shapedAmount * (0.50f + breathLevel * 0.10f + stormLevel * 0.05f));
            // Preserve note articulation even when the network blooms heavily.
            const float dryFloor = 0.24f + (1.0f - wetSmoothed) * 0.10f + (1.0f - depth) * 0.04f;
            const float dryMix = juce::jmax(dryFloor, 1.0f - dryDuck);
            const float wetProjection = wetSmoothed * (0.72f + shapedAmount * 0.40f + breathLevel * 0.10f + stormLevel * 0.06f) * trimSmoothed;
            const float bodySupport = 0.14f + wetSmoothed * 0.06f + breathLevel * 0.03f;

            float wetR = wetL;
            float rootDustR = rootDustL;
            float deepTailR = deepTailL;
            float networkR = networkL;

            if (stereo)
            {
                const float tapR0 = sanitizeSample(delayR.popSample(0, d0R, false), 1.35f);
                const float tapR1 = sanitizeSample(delayR.popSample(0, d1R, false), 1.35f);
                const float tapR2 = sanitizeSample(delayR.popSample(0, d2R, false), 1.35f);
                const float tapR3 = sanitizeSample(delayR.popSample(0, d3R, false), 1.35f);
                const float tapR4 = sanitizeSample(delayR.popSample(0, d4R, true), 1.35f);
                const std::array<float, 5> tapsR { tapR0, tapR1, tapR2, tapR3, tapR4 };
                rootDustR = tapR0 * 0.32f + tapR1 * 0.24f + tapR2 * 0.14f;

                networkR = 0.0f;
                deepTailR = 0.0f;
                float previousNodeR = 0.0f;
                for (size_t stage = 0; stage < tapsR.size(); ++stage)
                {
                    const float darkness = juce::jlimit(0.22f, 1.0f, 1.0f - (float) stage * (0.14f + depth * 0.01f));
                    const float coeff = juce::jlimit(0.050f, 0.22f,
                                                     nodeCoefficients[stage] + breathLevel * 0.014f + stormLevel * 0.024f);
                    const float networkFeed = previousNodeR * (0.08f + (float) stage * 0.015f);
                    const float humusFeed = humusL * (0.04f + (float) stage * 0.012f);
                    const float stageTarget = tapsR[stage] * darkness + networkFeed + humusFeed + rootDustR * (0.08f + (float) stage * 0.018f);
                    mycelNodesR[stage] += coeff * (stageTarget - mycelNodesR[stage]);
                    const float node = sanitizeSample(softSat(mycelNodesR[stage] * (0.92f - (float) stage * 0.06f
                                                                                    + depth * 0.14f + stormLevel * 0.24f)), 1.2f);
                    networkR += node * nodeWeights[stage];
                    if (stage >= 2)
                        deepTailR += node * (0.18f + 0.05f * (float) (stage - 2));
                    previousNodeR = node;
                }

                toneFilters[1].setCutoffFrequency(cutoffSmoothed * (0.96f + breathLevel * 0.10f + stormLevel * 0.04f));
                humusR += 0.036f * ((deepTailR * (0.58f + depth * 0.18f) + networkR * 0.22f + rootDustR * 0.18f) - humusR);
                humusR = sanitizeSample(humusR, 1.15f);

                wetR = networkR + humusL * (0.42f + widthSmoothed * 0.16f) + deepTailR * 0.38f + rootDustR * 0.12f;
                wetR = sanitizeSample(toneFilters[1].processSample(0, wetR), 1.25f);
                const float wetRPre = (wetR + humusR * (0.66f + depth * 0.18f) + rootDustR * 0.18f) * (1.22f + breathBloom * 0.38f);
                wetR = sanitizeSample(wetRPre * 0.48f + softSat(wetRPre * (1.36f + stormLevel * 0.36f)) * 0.52f, 1.2f);
                const float warmR = dryR * 0.28f + softSat(dryR * driveSmoothed * (0.80f + breathLevel * 0.08f)) * 0.72f;

                left[i] = sanitizeSample(dryL * dryMix
                                       + wetL * wetProjection
                                       + warmL * bodySupport, 1.2f);
                right[i] = sanitizeSample(dryR * dryMix
                                        + wetR * wetProjection
                                        + warmR * bodySupport, 1.2f);

                const float feedbackGain = clampFeedbackGain(feedbackSmoothed * (0.40f + widthSmoothed * 0.10f));
                delayL.pushSample(0, sanitizeDelayInput(dryL * 0.04f + networkL * 0.14f + humusL * 0.22f + wetR * 0.10f + deepTailR * feedbackGain));
                delayR.pushSample(0, sanitizeDelayInput(dryR * 0.04f + networkR * 0.14f + humusR * 0.22f + wetL * 0.10f + deepTailL * feedbackGain));
            }
            else
            {
                humusR = humusL;
                mycelNodesR = mycelNodesL;
                left[i] = sanitizeSample(dryL * dryMix
                                       + wetL * wetProjection
                                       + warmL * bodySupport, 1.2f);

                const float feedbackGain = clampFeedbackGain(feedbackSmoothed * 0.40f);
                delayL.pushSample(0, sanitizeDelayInput(dryL * 0.04f + networkL * 0.16f + humusL * 0.24f + deepTailL * feedbackGain));
            }
        }
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayL { maxDelaySamples };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayR { maxDelaySamples };
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> toneFilters;
    double sampleRate = 44100.0;
    float amount = 0.0f;
    float energy = 0.0f;
    float depth = 0.5f;
    float vitality = 0.5f;
    float breath = 0.5f;
    float canopyOpen = 0.5f;
    float driveSmoothed = 1.0f;
    float trimSmoothed = 1.0f;
    float cutoffSmoothed = 12000.0f;
    float wetSmoothed = 0.0f;
    float feedbackSmoothed = 0.10f;
    float widthSmoothed = 0.0f;
    float modPhase = 0.0f;
    float stormPhase = 0.0f;
    float stormLevel = 0.0f;
    float breathLevel = 0.0f;
    float humusL = 0.0f;
    float humusR = 0.0f;
    std::array<float, 5> mycelNodesL {};
    std::array<float, 5> mycelNodesR {};
};
} // namespace RootFlowDSP
