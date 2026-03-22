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

/**
 * Deep, Gritty Forest-Chorus BloomProcessor.
 * Zero-clicking using LinearSmoothedValue and Cross-fade ramping.
 */
class BloomProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        
        chorus.prepare(spec);
        chorus.setCentreDelay(20.0f);
        chorus.setFeedback(0.34f);
        chorus.setMix(1.0f); // Ramping manually

        smoothedMix.reset(sampleRate, 0.05);
        smoothedDepth.reset(sampleRate, 0.05);
        smoothedRate.reset(sampleRate, 0.05);
    }

    void reset()
    {
        chorus.reset();
        smoothedMix.setCurrentAndTargetValue(0.0f);
    }

    void setParams(float blossomAmount,
                   float plantEnergy,
                   float sapVitality,
                   float pulseBreath,
                   float canopyOpen,
                   float rootAnchor)
    {
        const float amount = clamp01(blossomAmount);
        const float energyMod = clamp01(sapVitality * 0.45f + plantEnergy * 0.55f);
        
        smoothedMix.setTargetValue(amount);
        // BOLDER DEPTH: Hit 100% modulation depth faster
        smoothedDepth.setTargetValue(amount * (0.44f + energyMod * 0.68f + canopyOpen * 0.14f));
        smoothedRate.setTargetValue(0.048f + energyMod * 1.08f + pulseBreath * 0.16f);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const float startMix = smoothedMix.getCurrentValue();
        smoothedMix.skip(numSamples);
        const float endMix = smoothedMix.getCurrentValue();

        chorus.setDepth(smoothedDepth.getNextValue());
        chorus.setRate(smoothedRate.getNextValue());
        smoothedDepth.skip(numSamples - 1);
        smoothedRate.skip(numSamples - 1);

        if (startMix < 0.0001f && endMix < 0.0001f) return;

        juce::AudioBuffer<float> dry;
        dry.makeCopyOf(buffer, true);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        chorus.process(context);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.applyGainRamp(ch, 0, numSamples, startMix, endMix);
            buffer.addFromWithRamp(ch, 0, dry.getReadPointer(ch), numSamples, 1.0f - startMix, 1.0f - endMix);
        }
    }

private:
    juce::dsp::Chorus<float> chorus;
    double sampleRate = 44100.0;
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedDepth;
    juce::LinearSmoothedValue<float> smoothedRate;
};

/**
 * Color-Filter Tape-Delay RainProcessor.
 * Synchronized Stereo-Phasing FIX.
 */
class RainProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0 ? spec.sampleRate : 44100.0;
        
        for (int i = 0; i < 2; ++i)
        {
            delay[i].prepare(spec);
            delay[i].setMaximumDelayInSamples(sampleRate * 2.1);
            colorFilters[i].prepare(spec);
            colorFilters[i].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            colorFilters[i].setResonance(0.42f);
        }

        smoothedDelayTime.reset(sampleRate, 0.05);
        smoothedMix.reset(sampleRate, 0.05);
        smoothedFeedback.reset(sampleRate, 0.05);
    }

    void reset()
    {
        for (int i = 0; i < 2; ++i)
        {
            delay[i].reset();
            colorFilters[i].reset();
        }
        smoothedMix.setCurrentAndTargetValue(0.0f);
    }

    void setParams(float rainAmount,
                   float plantEnergy,
                   float sapFlow,
                   float sapTexture,
                   float pulseBreath,
                   float canopy)
    {
        const float amount = clamp01(rainAmount);
        const float modulation = clamp01(plantEnergy * 0.6f + sapFlow * 0.4f);
        
        smoothedMix.setTargetValue(amount);
        float timeScale = 0.20f + (1.0f - sapFlow) * 0.60f + canopy * 0.10f;
        smoothedDelayTime.setTargetValue(timeScale * (float)sampleRate);
        smoothedFeedback.setTargetValue(0.35f + modulation * 0.52f + sapTexture * 0.12f);

        // SHARPER RAIN: Higher resonance makes the echoes "cut" better
        float cutoff = 440.0f + (modulation * 3200.0f) + pulseBreath * 500.0f;
        for (auto& filter : colorFilters) {
            filter.setCutoffFrequency(cutoff);
            filter.setResonance(0.55f);
        }
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        const float startMix = smoothedMix.getCurrentValue();
        smoothedMix.skip(numSamples);
        const float endMix = smoothedMix.getCurrentValue();

        if (startMix < 0.001f && endMix < 0.001f) return;

        for (int i = 0; i < numSamples; ++i)
        {
            // Sync parameter update for both channels (Fixes stereo phasing distortion)
            const float delaySamples = smoothedDelayTime.getNextValue();
            const float fb = smoothedFeedback.getNextValue();
            const float currentMix = smoothedMix.getCurrentValue() - (endMix - startMix) * ((float)(numSamples - 1 - i) / (float)numSamples);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* samples = buffer.getWritePointer(ch);
                float dry = samples[i];
                float delayed = delay[ch].popSample(0, delaySamples, false);
                
                float filtered = colorFilters[ch].processSample(0, delayed);
                float fbSignal = std::tanh(dry + filtered * fb);
                delay[ch].pushSample(0, fbSignal);
                
                samples[i] = dry * (1.0f - currentMix) + filtered * currentMix;
            }
        }
    }

private:
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>, 2> delay;
    juce::LinearSmoothedValue<float> smoothedDelayTime;
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedFeedback;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> colorFilters;
    double sampleRate = 44100.0;
};

/**
 * Organic Cave-Reverb SunProcessor.
 */
class SunProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        reverb.prepare(spec);
        smoothedMix.reset(sampleRate, 0.05);
        smoothedSize.reset(sampleRate, 0.05);
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
        const float amount = clamp01(sunAmount);
        const float energy = clamp01(plantEnergy);
        
        smoothedMix.setTargetValue(amount);
        // POWER REVERB: Bigger range and wider image
        smoothedSize.setTargetValue(0.52f + amount * 0.38f + energy * 0.20f);

        auto params = reverb.getParameters();
        params.damping = 0.40f - sapVitality * 0.24f;
        params.width = 0.75f + canopy * 0.35f;
        params.wetLevel = 1.0f;
        params.dryLevel = 0.0f;
        reverb.setParameters(params);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const float startMix = smoothedMix.getCurrentValue();
        smoothedMix.skip(numSamples);
        const float endMix = smoothedMix.getCurrentValue();

        auto params = reverb.getParameters();
        params.roomSize = smoothedSize.getNextValue();
        smoothedSize.skip(numSamples - 1);
        reverb.setParameters(params);

        if (startMix < 0.0001f && endMix < 0.0001f) return;

        juce::AudioBuffer<float> dry;
        dry.makeCopyOf(buffer, true);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.applyGainRamp(ch, 0, numSamples, startMix, endMix);
            buffer.addFromWithRamp(ch, 0, dry.getReadPointer(ch), numSamples, 1.0f - startMix, 1.0f - endMix);
        }
    }

private:
    juce::dsp::Reverb reverb;
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedSize;
    double sampleRate = 44100.0;
};

} // namespace RootFlowDSP
