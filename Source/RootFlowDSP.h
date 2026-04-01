#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace RootFlowDSP
{
inline float clamp01(float x) noexcept { return juce::jlimit(0.0f, 1.0f, x); }

/**
 * SOIL (Earthy Resonator)
 * Dark, resonant environment based on plant energy.
 */
class BloomProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        reverb.prepare(spec);
        wetBuffer.setSize((int) spec.numChannels,
                          (int) juce::jmax<juce::uint32>(1u, spec.maximumBlockSize),
                          false,
                          false,
                          true);

        smoothedMix.reset(sampleRate, 0.05);
        smoothedSize.reset(sampleRate, 0.05);

        prevL = 0.0f;
        prevR = 0.0f;
    }

    void reset()
    {
        reverb.reset();
        smoothedMix.setCurrentAndTargetValue(0.0f);
        prevL = 0.0f;
        prevR = 0.0f;
    }

    void setParams(float blossomAmount,
                   float plantEnergy,
                   float sapVitality,
                   float pulseBreath,
                   float canopyOpen,
                   float rootAnchor)
    {
        smoothedMix.setTargetValue(clamp01(blossomAmount));

        // Size linked to Bio-Energy
        smoothedSize.setTargetValue(0.45f + plantEnergy * 0.45f);

        juce::dsp::Reverb::Parameters params;
        params.roomSize = smoothedSize.getTargetValue();
        params.damping  = 0.72f; // Very dark
        params.width    = 0.65f;
        params.wetLevel = 1.0f;
        params.dryLevel = 0.0f;
        reverb.setParameters(params);

        // Lowpass damping factor based on vitality
        targetDamp = 0.82f - (sapVitality * 0.28f); // More damping (softer)
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const float mix = smoothedMix.getNextValue();
        smoothedMix.skip(numSamples - 1);

        if (mix < 0.001f)
            return;

        jassert(wetBuffer.getNumChannels() >= buffer.getNumChannels());
        jassert(wetBuffer.getNumSamples() >= numSamples);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            wetBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        auto block = juce::dsp::AudioBlock<float>(wetBuffer)
                         .getSubsetChannelBlock(0, (size_t) buffer.getNumChannels())
                         .getSubBlock(0, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* dryPtr = buffer.getWritePointer(ch);
            auto* wetPtr = wetBuffer.getReadPointer(ch);
            float& last = (ch == 0) ? prevL : prevR;

            for (int i = 0; i < numSamples; ++i)
            {
                // Simple Low-Pass on the Reverb Tail (Soil effect)
                // Heavier darkening for RootFlow organic feel
                float s = wetPtr[i];
                s = s * (1.0f - targetDamp) + last * targetDamp;
                last = s;

                dryPtr[i] = dryPtr[i] * (1.0f - mix) + s * mix;
            }
        }
    }

private:
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wetBuffer;
    double sampleRate = 44100.0;
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedSize;
    float prevL = 0.0f, prevR = 0.0f;
    float targetDamp = 0.94f; // Deep analog darkening of reverb tail
};

/**
 * AIR (Modulated Wind-Delay)
 * Moving air feeling rather than static echoes.
 */
class RainProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0 ? spec.sampleRate : 44100.0;
        specInternal = spec;

        for (int i = 0; i < 2; ++i)
        {
            delay[i].prepare(spec);
            delay[i].setMaximumDelayInSamples(sampleRate * 2.0);
        }

        smoothedMix.reset(sampleRate, 0.08);
        modPhase = 0.0f;
    }

    void reset()
    {
        for (int i = 0; i < 2; ++i) delay[i].reset();
        smoothedMix.setCurrentAndTargetValue(0.0f);
    }

    void setParams(float rainAmount,
                   float plantEnergy,
                   float sapFlow,
                   float sapTexture,
                   float pulseBreath,
                   float canopy)
    {
        smoothedMix.setTargetValue(clamp01(rainAmount));

        // Speed of air movement linked to Pulse & Energy
        modSpeed = 0.0001f + (pulseBreath * 0.0008f) + (plantEnergy * 0.0004f);

        // Base delay time
        baseDelayMs = 220.0f + (sapFlow * 380.0f);
        feedback = 0.18f + (sapTexture * 0.32f); // Max 0.50, softer feedback loop
        modDepth = 35.0f + (canopy * 65.0f);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        const float mix = smoothedMix.getNextValue();
        smoothedMix.skip(numSamples - 1);

        if (mix < 0.001f) return;

        for (int i = 0; i < numSamples; ++i)
        {
            modPhase += modSpeed;
            if (modPhase > 1.0f) modPhase -= 1.0f;

            float mod = std::sin(modPhase * juce::MathConstants<float>::twoPi);
            float currentDelayMs = baseDelayMs + mod * modDepth;
            float delaySamples = currentDelayMs * (float)sampleRate / 1000.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* samples = buffer.getWritePointer(ch);
                float dry = samples[i];
                float delayed = delay[ch % 2].popSample(0, delaySamples, true);

                // Feedback loop with organic tape saturation & frequency damping
                float fbRaw = dry + delayed * feedback;
                tapeLp[ch % 2] = tapeLp[ch % 2] * 0.65f + fbRaw * 0.35f; // Softer one-pole lowpass
                float fbSignal = std::tanh(tapeLp[ch % 2] * 1.15f); // Slightly saturated for warmth
                delay[ch % 2].pushSample(0, fbSignal);

                samples[i] = dry * (1.0f - mix) + delayed * mix;
            }
        }
    }

private:
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>, 2> delay;
    double sampleRate = 44100.0;
    juce::dsp::ProcessSpec specInternal;
    juce::LinearSmoothedValue<float> smoothedMix;

    float modPhase = 0.0f;
    float modSpeed = 0.0005f;
    float modDepth = 50.0f;
    float baseDelayMs = 200.0f;
    float feedback = 0.5f;
    float tapeLp[2] = {0.0f, 0.0f};
};

/**
 * SPACE (Cosmic Bloom)
 * Combining depth and shimmer for a wide finish.
 */
class SunProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        reverb.prepare(spec);
        wetBuffer.setSize((int) spec.numChannels,
                          (int) juce::jmax<juce::uint32>(1u, spec.maximumBlockSize),
                          false,
                          false,
                          true);
        smoothedMix.reset(sampleRate, 0.05);
    }

    void reset()
    {
        reverb.reset();
        smoothedMix.setCurrentAndTargetValue(0.0f);
    }

    void setParams(float sunAmount,
                   float plantEnergy,
                   float rootDepth,
                   float sapVitality,
                   float pulseBreath,
                   float canopy)
    {
        smoothedMix.setTargetValue(clamp01(sunAmount));

        auto params = reverb.getParameters();
        params.roomSize = 0.75f + canopy * 0.22f;
        params.damping  = 0.92f - (plantEnergy * 0.35f); // Deeply damped (dark, warm analog tail)
        params.width    = 0.85f + (rootDepth * 0.14f);
        params.wetLevel = 1.0f;
        params.dryLevel = 0.0f;
        reverb.setParameters(params);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const float mix = smoothedMix.getNextValue();
        smoothedMix.skip(numSamples - 1);

        if (mix < 0.001f)
            return;

        jassert(wetBuffer.getNumChannels() >= buffer.getNumChannels());
        jassert(wetBuffer.getNumSamples() >= numSamples);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            wetBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        auto block = juce::dsp::AudioBlock<float>(wetBuffer)
                         .getSubsetChannelBlock(0, (size_t) buffer.getNumChannels())
                         .getSubBlock(0, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.applyGainRamp(ch, 0, numSamples, 1.0f - mix, 1.0f - mix);
            buffer.addFrom(ch, 0, wetBuffer.getReadPointer(ch), numSamples, mix);
        }
    }

private:
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wetBuffer;
    juce::LinearSmoothedValue<float> smoothedMix;
    double sampleRate = 44100.0;
};

} // namespace RootFlowDSP
