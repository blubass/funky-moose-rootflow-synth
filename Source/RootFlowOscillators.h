#pragma once
#include <JuceHeader.h>
#include <cmath>

class RootFlowOscillator
{
public:
    enum class Waveform { Sine, Saw, Pulse };

    void prepare(double sr) {
        sampleRate = sr;
    }

    void setFrequency(float freq) {
        phaseDelta = freq / (float)sampleRate;
    }

    void setPhase(float p) {
        phase = std::fmod(p, 1.0f);
    }

    void setWaveform(int type) {
        targetWave = static_cast<Waveform>(juce::jlimit(0, 2, type));
    }

    void setPulseWidth(float pw) {
        pulseWidth = juce::jlimit(0.05f, 0.95f, pw);
    }

    float nextSample() {
        phase += phaseDelta;
        while (phase >= 1.0f) phase -= 1.0f;

        float out = 0.0f;

        // Wir berechnen alle drei und könnten sie theoretisch sogar mischen
        if (targetWave == Waveform::Sine) {
            out = std::sin(phase * juce::MathConstants<float>::twoPi);
        }
        else if (targetWave == Waveform::Saw) {
            out = 2.0f * phase - 1.0f;
            out -= polyBlep(phase); // Anti-Aliasing
        }
        else if (targetWave == Waveform::Pulse) {
            // PWM Moduliert
            out = (phase < pulseWidth) ? 0.5f : -0.5f;
            
            // Anti-Aliasing für aufsteigende Flanke (bei phase = 0)
            out += polyBlep(phase); 
            
            // Anti-Aliasing für absteigende Flanke (bei phase = pulseWidth)
            float phase2 = phase - pulseWidth;
            if (phase2 < 0.0f) phase2 += 1.0f;
            out -= polyBlep(phase2);
        }

        return out;
    }

private:
    float polyBlep(float t) {
        float dt = phaseDelta;
        if (t < dt) {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        else if (t > 1.0f - dt) {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    double sampleRate = 44100.0;
    float phase = 0.0f, phaseDelta = 0.0f;
    float pulseWidth = 0.5f;
    Waveform targetWave = Waveform::Sine;
};
