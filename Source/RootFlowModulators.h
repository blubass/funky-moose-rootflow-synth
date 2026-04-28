#pragma once

#include <JuceHeader.h>

/**
 * Snapshot for UI Visualizers.
 */
struct RootFlowSystemFeedbackSnapshot
{
    float systemEnergy = 0.0f;
    float cycle = 0.0f;
    float jitter = 0.0f;
    float load = 0.0f;
};

/**
 * The 'Core' of RootFlow Modulation.
 * Fuses ROOT (stability), CORE (vitality), and PULSE (interaction)
 * into a single cybernetic System Energy value.
 */
class RootFlowModulationEngine
{
public:
    void prepare(double sr, int)
    {
        sampleRate = sr;
        smoothedSystemEnergy.reset(sr, 0.05);

        // Initialize Parameter Smoothing
        s_rootAmt.reset(sr, 0.02);
        s_rootAnchorAmt.reset(sr, 0.02);
        s_rootSpeed.reset(sr, 0.05);
        s_sapAmt.reset(sr, 0.02);
        s_sapSpeed.reset(sr, 0.05);
        s_sapTextureAmt.reset(sr, 0.02);
        s_pulseAmt.reset(sr, 0.02);
        s_pulseSens.reset(sr, 0.02);
        s_pulseGrowthAmt.reset(sr, 0.02);

        phase = 0.0f;
        currentSap = 0.0f;
        targetSap = 0.0f;
        pulseFollower = 0.0f;
        systemEnergy = 0.0f;
        systemFeedback = {};
    }

    void setParams(float depth, float soil, float anchor,
                   float flow, float vitality, float texture,
                   float rate, float breath, float growth)
    {
        s_rootAmt.setTargetValue(depth);
        s_rootAnchorAmt.setTargetValue(anchor);
        s_rootSpeed.setTargetValue(0.00015f + (soil * soil * 0.0125f));

        s_sapAmt.setTargetValue(flow);
        s_sapSpeed.setTargetValue(0.001f + vitality * 0.035f);
        s_sapTextureAmt.setTargetValue(texture);

        s_pulseAmt.setTargetValue(rate);
        s_pulseSens.setTargetValue(breath);
        s_pulseGrowthAmt.setTargetValue(growth);
    }

    void update(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0)
            return;

        // Fetch Smoothed Parameters for this block
        float rootAmt        = s_rootAmt.getNextValue();
        float rootAnchorAmt  = s_rootAnchorAmt.getNextValue();
        float rootSpeed      = s_rootSpeed.getNextValue();
        float sapAmt         = s_sapAmt.getNextValue();
        float sapSpeed       = s_sapSpeed.getNextValue();
        float sapTextureAmt  = s_sapTextureAmt.getNextValue();
        float pulseAmt       = s_pulseAmt.getNextValue();
        float pulseSens      = s_pulseSens.getNextValue();
        float pulseGrowthAmt = s_pulseGrowthAmt.getNextValue();

        // Calculate block-size independent alpha for smoothing (approx. 100ms time constant)
        const float tau = 0.10f;
        const float blockDuration = (float)numSamples / (float)sampleRate;
        const float alpha = 1.0f - std::exp(-blockDuration / tau);

        // 1. ROOT (The Foundation)
        phase += rootSpeed * (float)numSamples;
        if (phase > juce::MathConstants<float>::twoPi)
            phase = std::fmod(phase, juce::MathConstants<float>::twoPi);

        float root = (std::sin(phase) * 0.5f + 0.5f) * rootAmt;
        root = root * (1.0f - rootAnchorAmt * 0.75f) + (rootAnchorAmt * 0.45f);

        // 2. SAP (The Vitality Loop)
        float sapProb = 1.0f - std::pow(1.0f - sapSpeed, (float)numSamples / 128.0f);
        if (random.nextFloat() < sapProb)
            targetSap = random.nextFloat();

        currentSap += (targetSap - currentSap) * alpha;
        float sap = currentSap * sapAmt;
        float textureNoise = (random.nextFloat() * 2.0f - 1.0f) * sapTextureAmt * 0.12f;

        // 3. PULSE (The Dynamic Reaction)
        float env = 0.0f;
        if (buffer.getNumChannels() > 0)
        {
            env = buffer.getMagnitude(0, numSamples);
            if (buffer.getNumChannels() > 1)
                env = juce::jmax(env, buffer.getMagnitude(1, numSamples));
        }

        env = juce::jlimit(0.0f, 1.0f, env * (1.15f + pulseSens * 14.0f));

        // Block-independent envelope following
        float attackTau = 0.01f;  // 10ms
        float releaseTau = 0.25f; // 250ms
        float currentTau = (env > pulseFollower) ? attackTau : releaseTau;
        float envAlpha = 1.0f - std::exp(-blockDuration / currentTau);

        pulseFollower += (env - pulseFollower) * envAlpha * (0.15f + pulseAmt * 0.85f);
        pulseFollower = juce::jlimit(0.0f, 1.0f, pulseFollower);

        // 4. SYNERGY (System Energy Calculation)
        float rawEnergy = 0.12f
                        + (root * 0.22f)
                        + (sap * 0.28f)
                        + (pulseFollower * 0.46f)
                        + (textureNoise * 0.05f);

        systemEnergy = juce::jlimit(0.0f, 1.0f, rawEnergy);
        smoothedSystemEnergy.setTargetValue(systemEnergy);

        systemFeedback.systemEnergy = smoothedSystemEnergy.getNextValue();
        smoothedSystemEnergy.skip(numSamples - 1);

        systemFeedback.cycle  = juce::jlimit(0.0f, 1.0f, root * 1.15f + pulseFollower * 0.25f);
        systemFeedback.jitter = juce::jlimit(0.0f, 1.0f, currentSap + std::abs(textureNoise) * 4.0f);
        systemFeedback.load   = juce::jlimit(0.0f, 1.0f, pulseFollower * 0.95f + (1.0f - rootAnchorAmt) * 0.15f);
    }

    float getSystemEnergy() const { return systemFeedback.systemEnergy; }
    const RootFlowSystemFeedbackSnapshot& getSystemFeedbackSnapshot() const noexcept { return systemFeedback; }

private:
    double sampleRate = 44100.0;
    juce::Random random;
    juce::LinearSmoothedValue<float> smoothedSystemEnergy;

    // Parameter Smoothing
    juce::LinearSmoothedValue<float> s_rootAmt, s_rootAnchorAmt, s_rootSpeed;
    juce::LinearSmoothedValue<float> s_sapAmt, s_sapSpeed, s_sapTextureAmt;
    juce::LinearSmoothedValue<float> s_pulseAmt, s_pulseSens, s_pulseGrowthAmt;

    float phase = 0;
    float currentSap = 0, targetSap = 0;
    float pulseFollower = 0;
    float systemEnergy = 0;

    RootFlowSystemFeedbackSnapshot systemFeedback;
};
