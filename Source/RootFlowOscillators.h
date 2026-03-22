#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace RootFlow
{
    /**
     * A smooth, naturally wandering modulator using fractal noise.
     * Perfect for subtle pitch drift, filter motion, or level flux.
     */
    class WanderingOrganicModulation
    {
    public:
        void prepare(double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 44100.0;
            time = 0.0f;

            juce::Random r;
            for (auto& seed : octaveSeeds)
                seed = (juce::uint32) r.nextInt();
        }

        /**
         * Returns a value between 0.0 and 1.0 that wanders smoothly.
         * rate: Base speed of movement.
         */
        float getNextValue(float rate)
        {
            const float dt = 1.0f / (float) sr;
            time += juce::jmax(0.0001f, rate) * dt;

            float value = 0.0f;
            float amplitude = 0.62f;
            float amplitudeSum = 0.0f;
            float frequency = 0.42f;

            for (size_t octave = 0; octave < octaveSeeds.size(); ++octave)
            {
                value += smoothValueNoise(time * frequency, octaveSeeds[octave]) * amplitude;
                amplitudeSum += amplitude;
                amplitude *= 0.5f;
                frequency *= 2.17f;
            }

            if (amplitudeSum > 0.0f)
                value /= amplitudeSum;

            return juce::jlimit(0.0f, 1.0f, value * 0.5f + 0.5f);
        }

    private:
        static float smoothstep(float t) noexcept
        {
            return t * t * (3.0f - 2.0f * t);
        }

        static float hashToSignedFloat(int index, juce::uint32 seed) noexcept
        {
            auto x = (juce::uint32) index;
            x ^= seed + 0x9e3779b9u + (x << 6) + (x >> 2);
            x ^= x >> 16;
            x *= 0x7feb352du;
            x ^= x >> 15;
            x *= 0x846ca68bu;
            x ^= x >> 16;
            return ((float) (x & 0x00ffffffu) / (float) 0x00800000u) - 1.0f;
        }

        static float smoothValueNoise(float x, juce::uint32 seed) noexcept
        {
            const auto index0 = (int) std::floor(x);
            const auto index1 = index0 + 1;
            const float frac = x - (float) index0;
            const float t = smoothstep(frac);
            const float v0 = hashToSignedFloat(index0, seed);
            const float v1 = hashToSignedFloat(index1, seed);
            return juce::jmap(t, v0, v1);
        }

        double sr = 44100.0;
        float time = 0.0f;
        std::array<juce::uint32, 4> octaveSeeds {};
    };

    /**
     * A collection of anti-aliased oscillators with organic character.
     */
    class OrganicOscillator
    {
    public:
        void prepare(double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 44100.0;
            phase = 0.0f;
        }

        /**
         * Returns a sample from a morphing organic waveform.
         * shape: 0.0=Sine, 0.5=Saw, 1.0=Square
         * growth: Increases harmonic complexity and "bite"
         */
        float getNextSample(float frequency, float shape, float growth)
        {
            const float clampedShape = juce::jlimit(0.0f, 1.0f, shape);
            const float clampedGrowth = juce::jlimit(0.0f, 1.0f, growth);
            const float safeFrequency = juce::jlimit(0.0f, (float) sr * 0.475f, frequency);
            const float increment = safeFrequency / (float) sr;
            
            // Basic phase increment
            phase += increment;
            while (phase >= 1.0f) phase -= 1.0f;

            // Basic Sine
            const float sine = std::sin(phase * juce::MathConstants<float>::twoPi);
            
            // Anti-aliased waveforms using PolyBLEP
            float saw = (2.0f * phase - 1.0f);
            saw -= polyBlep(increment, phase);
            
            float pulse = (phase < 0.5f) ? 1.0f : -1.0f;
            pulse += polyBlep(increment, phase);
            pulse -= polyBlep(increment, std::fmod(phase + 0.5f, 1.0f));

            // Morphing logic
            float output = 0.0f;
            if (clampedShape < 0.5f)
            {
                // Sine to Saw
                const float t = clampedShape * 2.0f;
                output = (1.0f - t) * sine + t * saw;
            }
            else
            {
                // Saw to Pulse
                const float t = (clampedShape - 0.5f) * 2.0f;
                output = (1.0f - t) * saw + t * pulse;
            }

            // Growth adds extra "overtones" and complexity (organic bite)
            if (clampedGrowth > 0.01f)
            {
                const float harmonicRatio = 2.0f + clampedGrowth * 3.0f;
                const float harmonicFreq = safeFrequency * harmonicRatio;
                const float harmonicFade = juce::jlimit(0.0f, 1.0f,
                                                        1.0f - harmonicFreq / ((float) sr * 0.5f));
                const float harmPhase = std::fmod(phase * harmonicRatio, 1.0f);
                const float harmonic = std::sin(harmPhase * juce::MathConstants<float>::twoPi)
                                     * clampedGrowth * 0.35f * harmonicFade;
                output = (output + harmonic) / (1.0f + clampedGrowth * 0.25f);
            }

            return output;
        }

    private:
        float polyBlep(float dt, float t)
        {
            if (t < dt)
            {
                float t_dt = t / dt;
                return t_dt + t_dt - t_dt * t_dt - 1.0f;
            }
            else if (t > 1.0f - dt)
            {
                float t_dt = (t - 1.0f) / dt;
                return t_dt * t_dt + t_dt + t_dt + 1.0f;
            }
            return 0.0f;
        }

        double sr = 44100.0;
        float phase = 0.0f;
    };

    /**
     * A noise generator with organic "rustle" character.
     */
    class OrganicNoise
    {
    public:
        float getNextSample(float character)
        {
            const float clampedCharacter = juce::jlimit(0.0f, 1.0f, character);

            // Pink-ish noise filter
            float raw = random.nextFloat() * 2.0f - 1.0f;
            
            b0 = 0.99765f * b0 + raw * 0.0990460f;
            b1 = 0.96300f * b1 + raw * 0.2965164f;
            b2 = 0.57000f * b2 + raw * 1.0526913f;
            float pink = b0 + b1 + b2 + raw * 0.1848f;
            pink *= 0.05f; // Normalise
            
            // Character adds crackle/rustle
            crackle *= 0.92f;

            if (clampedCharacter > 0.1f)
            {
                if (random.nextFloat() < (clampedCharacter * 0.02f))
                    crackle = (random.nextFloat() * 2.0f - 1.0f) * clampedCharacter;
            }
            
            return pink + crackle;
        }

    private:
        juce::Random random;
        float b0 = 0, b1 = 0, b2 = 0;
        float crackle = 0;
    };

    /**
     * A non-linear 4-pole ladder filter with organic saturation.
     * This provides a much warmer and more "alive" character than standard SVFs.
     */
    class OrganicLadderFilter
    {
    public:
        void prepare(double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 44100.0;
            reset();
        }

        void reset()
        {
            for (int i = 0; i < 4; ++i) s[i] = 0.0f;
        }

        float processSample(float input, float cutoffHz, float resonance, float drive)
        {
            // Catch NaNs from inputs immediately
            if (! std::isfinite(input)) input = 0.0f;

            const float safeCutoff = juce::jlimit(20.0f, (float) sr * 0.45f, cutoffHz);
            const float g = std::tan(juce::MathConstants<float>::pi * safeCutoff / (float)sr);
            const float G = g / (1.0f + g); // Proper TPT G
            const float r = juce::jlimit(0.0f, 0.98f, resonance);
            
            const float sat = 1.0f + juce::jmax(0.0f, drive) * 2.5f;
            const float feedback = r * 4.0f;
            
            // The previous math bug was g*g/(1+g) instead of G*G. This caused huge overshoots!
            const float sigma = s[0] * (G * G * G) + s[1] * (G * G) + s[2] * G + s[3];
            
            float u = (input * sat - feedback * std::tanh(sigma)) / (1.0f + feedback * G * G * G * G);
            
            for (int i = 0; i < 4; ++i)
            {
                float v = (u - s[i]) * G;
                float y = v + s[i];
                s[i] = y + v;
                
                if (! std::isfinite(s[i])) s[i] = 0.0f;
                
                u = y;
            }
            
            return u / (1.0f + juce::jmax(0.0f, drive) * 0.5f);
        }

    private:
        double sr = 44100.0;
        float s[4] = { 0, 0, 0, 0 };
    };
}
