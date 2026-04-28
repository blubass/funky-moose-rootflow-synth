#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace RootFlowDSP
{
inline float clamp01(float x) noexcept { return juce::jlimit(0.0f, 1.0f, x); }

struct MidiExpressionState
{
    std::array<float, 16> pitchBendNormalized {};
    std::array<float, 16> pitchBendRangeSemitones {};
    std::array<float, 16> modWheelNormalized {};
    std::array<float, 16> pressureNormalized {};
    std::array<float, 16> timbreNormalized {};
};

struct SeasonMorph
{
    float spring = 0.0f;
    float summer = 0.0f;
    float autumn = 0.0f;
    float winter = 0.0f;
};

inline SeasonMorph getSeasonMorph(float seasonMacro)
{
    SeasonMorph m;
    const float s = seasonMacro;
    
    // Spring: Peaks at 0.25 (range 0.0 to 0.5)
    m.spring = std::max(0.0f, 1.0f - std::abs(s - 0.25f) * 4.0f);
    // Summer: Peaks at 0.5 (range 0.25 to 0.75)
    m.summer = std::max(0.0f, 1.0f - std::abs(s - 0.5f) * 4.0f);
    // Autumn: Peaks at 0.75 (range 0.5 to 1.0)
    m.autumn = std::max(0.0f, 1.0f - std::abs(s - 0.75f) * 4.0f);
    // Winter: Peaks at 0.0 and 1.0 (Cyclic)
    m.winter = std::max(0.0f, 1.0f - std::abs(s - (s > 0.5f ? 1.0f : 0.0f)) * 4.0f);
    
    return m;
}

/**
 * CORE (Matrix Resonator)
 * Dark, resonant environment based on system energy.
 */
class CoreResonator
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
        smoothedDamp.reset(sampleRate, 0.05);

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

    void setParams(float resonanceAmount,
                   float systemEnergy,
                   float flowEnergy,
                   float pulseWidth,
                   float fieldComplexity,
                   float sourceAnchor)
    {
        smoothedMix.setTargetValue(clamp01(resonanceAmount));
        smoothedSize.setTargetValue(0.45f + systemEnergy * 0.45f);

        // Lowpass damping factor based on flow energy
        float targetD = 0.82f - (flowEnergy * 0.28f);
        smoothedDamp.setTargetValue(targetD);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        if (smoothedMix.getCurrentValue() < 0.001f && smoothedMix.getTargetValue() < 0.001f)
        {
            smoothedMix.skip(numSamples);
            smoothedSize.skip(numSamples);
            smoothedDamp.skip(numSamples);
            return;
        }

        jassert(wetBuffer.getNumChannels() >= buffer.getNumChannels());
        jassert(wetBuffer.getNumSamples() >= numSamples);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            wetBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        juce::dsp::Reverb::Parameters params;
        params.roomSize = smoothedSize.getNextValue();
        smoothedSize.skip(numSamples - 1);
        params.damping  = 0.72f; 
        params.width    = 0.65f;
        params.wetLevel = 1.0f;
        params.dryLevel = 0.0f;
        reverb.setParameters(params);

        auto block = juce::dsp::AudioBlock<float>(wetBuffer)
                         .getSubsetChannelBlock(0, (size_t) buffer.getNumChannels())
                         .getSubBlock(0, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);

        // handle both channels and smoothing correctly
        auto* leftDry = buffer.getWritePointer(0);
        auto* rightDry = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
        auto* leftWet = wetBuffer.getReadPointer(0);
        auto* rightWet = buffer.getNumChannels() > 1 ? wetBuffer.getReadPointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float mix = smoothedMix.getNextValue();
            float damp = smoothedDamp.getNextValue();

            // Left
            float sL = leftWet[i];
            sL = sL * (1.0f - damp) + prevL * damp;
            prevL = sL;
            leftDry[i] = leftDry[i] * (1.0f - mix) + sL * mix;

            // Right
            if (rightDry)
            {
                float sR = rightWet[i];
                sR = sR * (1.0f - damp) + prevR * damp;
                prevR = sR;
                rightDry[i] = rightDry[i] * (1.0f - mix) + sR * mix;
            }
        }
    }

private:
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wetBuffer;
    double sampleRate = 44100.0;
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedSize;
    juce::LinearSmoothedValue<float> smoothedDamp;
    float prevL = 0.0f, prevR = 0.0f;
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
        specInternal = spec;

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

    void setParams(float fieldAmount,
                   float systemEnergy,
                   float flowRate,
                   float flowTexture,
                   float pulseWidth,
                   float fieldComplexity)
    {
        smoothedMix.setTargetValue(clamp01(fieldAmount));

        // Speed of field movement linked to Pulse & Energy
        smoothedModSpeed.setTargetValue(0.0001f + (pulseWidth * 0.0008f) + (systemEnergy * 0.0004f));

        // Base delay time
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
            float mix = smoothedMix.getNextValue();
            float feedback = smoothedFeedback.getNextValue();
            float baseDelayMs = smoothedDelayMs.getNextValue();
            float modDepth = smoothedModDepth.getNextValue();
            float modSpeed = smoothedModSpeed.getNextValue();

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
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>, 2> delay;
    double sampleRate = 44100.0;
    juce::dsp::ProcessSpec specInternal;
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedDelayMs;
    juce::LinearSmoothedValue<float> smoothedFeedback;
    juce::LinearSmoothedValue<float> smoothedModDepth;
    juce::LinearSmoothedValue<float> smoothedModSpeed;

    float modPhase = 0.0f;
    float tapeLp[2] = {0.0f, 0.0f};
};

/**
 * RADIANCE (Quantum Finisher)
 * Combining depth and shimmer for a wide finish.
 */
class RadianceFinisher
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

    void setParams(float radianceAmount,
                   float systemEnergy,
                   float sourceDepth,
                   float flowEnergy,
                   float pulseWidth,
                   float fieldComplexity)
    {
        smoothedMix.setTargetValue(clamp01(radianceAmount));

        // Update reverb parameters based on latest values.
        // These will be applied at the start of the next process block.
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
                         .getSubsetChannelBlock(0, (size_t) numChannels)
                         .getSubBlock(0, (size_t) numSamples);
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

} // namespace RootFlowDSP
