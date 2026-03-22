#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * Pure Sine Oscillator for 100% Artifact-free sound.
 * Delivers the purest waveform of nature with zero aliasing.
 */
class RootFlowOscillator
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
    }

    void setFrequency(float freq)
    {
        // Calculate the phase increment per sample in radians
        phaseDelta = freq * juce::MathConstants<float>::twoPi / (float)sampleRate;
    }

    float nextSample()
    {
        // 1. Phase Update
        phase += phaseDelta;
        
        // 2. Wrap-Around (prevents overflow and calculation errors)
        if (phase >= juce::MathConstants<float>::twoPi) 
            phase -= juce::MathConstants<float>::twoPi;

        // 3. Nature's purest wave
        return std::sin(phase);
    }

private:
    double sampleRate = 44100.0;
    float phase = 0.0f;
    float phaseDelta = 0.0f;
};
