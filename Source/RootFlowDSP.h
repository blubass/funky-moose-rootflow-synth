#pragma once

#include <JuceHeader.h>

namespace RootFlowDSP
{

static inline float clamp01(float v) { return juce::jlimit(0.0f, 1.0f, v); }

/**
 * A gentle safety net to prevent harsh digital clipping.
 */
static inline float softLimit(float x) noexcept
{
    return std::tanh(x * 1.1f) / 0.8005f; // std::tanh(1.1f) is approx 0.8005
}

/**
 * CORE (Resonator & Body)
 * The tonal center and physical presence of the sound.
 */
class CoreResonator
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0 ? spec.sampleRate : 44100.0;
        
        for (int i = 0; i < 2; ++i)
        {
            filter[i].reset();
            filter[i].setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        }
        
        updateFilter(spec.sampleRate);
        
        smoothedMix.reset(sampleRate, 0.05);
        smoothedRes.reset(sampleRate, 0.05);
        smoothedFreq.reset(sampleRate, 0.05);
    }

    void setParams(float coreAmount, float systemEnergy, float sourceDepth, float sourceAnchor)
    {
        smoothedMix.setTargetValue(clamp01(coreAmount));
        
        float baseFreq = 220.0f + (sourceAnchor * 880.0f);
        float modFreq  = baseFreq * (1.0f + systemEnergy * 0.5f);
        smoothedFreq.setTargetValue(juce::jlimit(20.0f, 18000.0f, modFreq));
        
        smoothedRes.setTargetValue(0.1f + (sourceDepth * 0.85f) + (systemEnergy * 0.15f));
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        if (smoothedMix.getCurrentValue() < 0.001f && smoothedMix.getTargetValue() < 0.001f)
        {
            smoothedMix.skip(numSamples);
            smoothedFreq.skip(numSamples);
            smoothedRes.skip(numSamples);
            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float mix  = smoothedMix.getNextValue();
            float freq = smoothedFreq.getNextValue();
            float res  = smoothedRes.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                filter[ch % 2].setCutoffFrequency(freq);
                filter[ch % 2].setResonance(res);

                auto* samples = buffer.getWritePointer(ch);
                float dry = samples[i];
                float wet = filter[ch % 2].processSample(ch % 2, dry);
                
                samples[i] = dry * (1.0f - mix) + wet * mix;
            }
        }
    }

private:
    void updateFilter(double sr)
    {
        for (int i = 0; i < 2; ++i)
        {
            filter[i].prepare({ sr, 512, 1 });
        }
    }

    juce::dsp::StateVariableTPTFilter<float> filter[2];
    double sampleRate = 44100.0;
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedFreq;
    juce::LinearSmoothedValue<float> smoothedRes;
};

/**
 * RADIANCE (Ambient & Space)
 * Shimmers, air and ground presence.
 */
class RadianceFinisher
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0 ? spec.sampleRate : 44100.0;
        reverb.prepare(spec);
        
        wetBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
        smoothedMix.reset(sampleRate, 0.05);
    }

    void reset()
    {
        reverb.reset();
        smoothedMix.setCurrentAndTargetValue(0.0f);
    }

    void setParams(float radianceAmount, float systemEnergy, float sourceDepth,
                   float flowEnergy, float pulseWidth, float fieldComplexity)
    {
        smoothedMix.setTargetValue(clamp01(radianceAmount));

        auto params = reverb.getParameters();
        params.roomSize = 0.75f + fieldComplexity * 0.22f;
        params.damping  = 0.92f - (systemEnergy * 0.35f);
        params.width    = 0.85f + (sourceDepth * 0.14f);
        params.wetLevel = 1.0f;
        params.dryLevel = 0.0f;
        reverb.setParameters(params);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        
        float startMix = smoothedMix.getNextValue();
        smoothedMix.skip(numSamples - 2);
        float endMix = smoothedMix.getNextValue();

        if (startMix < 0.001f && endMix < 0.001f && smoothedMix.getTargetValue() < 0.001f)
            return;

        jassert(wetBuffer.getNumChannels() >= numChannels);
        jassert(wetBuffer.getNumSamples() >= numSamples);

        for (int ch = 0; ch < numChannels; ++ch)
            wetBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        auto block = juce::dsp::AudioBlock<float>(wetBuffer)
                         .getSubsetChannelBlock(0, (size_t)numChannels)
                         .getSubBlock(0, (size_t)numSamples);
                         
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* samples = buffer.getWritePointer(ch);
            auto* wetSamples = wetBuffer.getReadPointer(ch);
            
            float mixStep = (endMix - startMix) / (float)numSamples;
            float currentMix = startMix;
            
            for (int i = 0; i < numSamples; ++i)
            {
                float dry = samples[i];
                float wet = wetSamples[i];
                
                // Moose with safety belt
                samples[i] = juce::jlimit(-1.0f, 1.0f, dry * (1.0f - currentMix) + wet * currentMix);
                currentMix += mixStep;
            }
        }
    }

private:
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wetBuffer;
    juce::LinearSmoothedValue<float> smoothedMix;
    double sampleRate = 44100.0;
};

/**
 * FIELD (Modulated Aether-Delay)
 * Moving energy field rather than static echoes.
 */
class FieldDelay
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0 ? spec.sampleRate : 44100.0;

        for (int i = 0; i < 2; ++i)
        {
            delay[i].prepare(spec);
            delay[i].setMaximumDelayInSamples(sampleRate * 2.0);
        }

        smoothedMix.reset(sampleRate, 0.08);
        smoothedDelayMs.reset(sampleRate, 0.1);
        smoothedFeedback.reset(sampleRate, 0.05);
        smoothedModDepth.reset(sampleRate, 0.05);
        smoothedModSpeed.reset(sampleRate, 0.05);
        
        modPhase = 0.0f;
    }

    void reset()
    {
        for (int i = 0; i < 2; ++i) delay[i].reset();
        smoothedMix.setCurrentAndTargetValue(0.0f);
    }

    void setParams(float fieldAmount, float systemEnergy, float flowRate,
                   float flowTexture, float pulseWidth, float fieldComplexity)
    {
        smoothedMix.setTargetValue(clamp01(fieldAmount));
        smoothedModSpeed.setTargetValue(0.0001f + (pulseWidth * 0.0008f) + (systemEnergy * 0.0004f));
        smoothedDelayMs.setTargetValue(220.0f + (flowRate * 380.0f));
        smoothedFeedback.setTargetValue(0.18f + (flowTexture * 0.32f));
        smoothedModDepth.setTargetValue(35.0f + (fieldComplexity * 65.0f));
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        
        if (smoothedMix.getCurrentValue() < 0.001f && smoothedMix.getTargetValue() < 0.001f)
        {
            smoothedMix.skip(numSamples);
            smoothedDelayMs.skip(numSamples);
            smoothedFeedback.skip(numSamples);
            smoothedModDepth.skip(numSamples);
            smoothedModSpeed.skip(numSamples);
            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float mix        = smoothedMix.getNextValue();
            float feedback   = smoothedFeedback.getNextValue();
            float baseDelayMs = smoothedDelayMs.getNextValue();
            float modDepth   = smoothedModDepth.getNextValue();
            float modSpeed   = smoothedModSpeed.getNextValue();

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

                // Feedback loop with saturation & damping
                float fbRaw = dry + delayed * feedback;
                tapeLp[ch % 2] = tapeLp[ch % 2] * 0.65f + fbRaw * 0.35f;
                float fbSignal = std::tanh(tapeLp[ch % 2] * 1.15f);
                delay[ch % 2].pushSample(0, fbSignal);

                // Moose with safety belt
                float wet = delayed * mix;
                samples[i] = juce::jlimit(-1.0f, 1.0f, dry * (1.0f - mix) + wet);
            }
        }
    }

private:
    juce::dsp::DelayLine<float> delay[2];
    float tapeLp[2] = { 0.0f, 0.0f };
    double sampleRate = 44100.0;
    float modPhase = 0.0f;
    
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedDelayMs;
    juce::LinearSmoothedValue<float> smoothedFeedback;
    juce::LinearSmoothedValue<float> smoothedModDepth;
    juce::LinearSmoothedValue<float> smoothedModSpeed;
};

struct MidiExpressionState
{
    float pitchBendCents = 0.0f;
    float timbre = 0.0f;
    float pressure = 0.0f;
};

} // namespace RootFlowDSP
