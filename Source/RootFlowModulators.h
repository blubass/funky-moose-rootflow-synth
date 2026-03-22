#pragma once
#include <JuceHeader.h>

struct RootFlowBioFeedbackSnapshot
{
    float plantEnergy = 0.0f;
    float growthCycle = 0.0f;
    float leafJitter = 0.0f;
    float tension = 0.0f;
};

class RootFlowModulationEngine
{
public:
    void prepare(double sr, int)
    {
        sampleRate = sr;
        phase = 0.0f;
        currentSap = 0.0f;
        targetSap = 0.0f;
        pulseFollower = 0.0f;
        plantEnergy = 0.0f;
        growthCycle = 0.0f;
        leafJitter = 0.0f;
        tension = 0.0f;
        bioFeedback = {};
    }

    void setParams(float depth, float soil, float anchor, 
                    float flow, float vitality, float texture,
                    float rate, float breath, float growth)
    {
        constexpr float referenceBlocksPerSecond = referenceSampleRate / referenceBlockSize;

        const float shapedDepth = std::pow(juce::jlimit(0.0f, 1.0f, depth), 0.42f);
        const float shapedSoil = std::pow(juce::jlimit(0.0f, 1.0f, soil), 0.44f);
        const float shapedAnchor = std::pow(juce::jlimit(0.0f, 1.0f, anchor), 0.48f);
        const float shapedFlow = std::pow(juce::jlimit(0.0f, 1.0f, flow), 0.56f);
        const float shapedVitality = std::pow(juce::jlimit(0.0f, 1.0f, vitality), 0.56f);
        const float shapedTexture = std::pow(juce::jlimit(0.0f, 1.0f, texture), 0.54f);
        const float shapedRate = std::pow(juce::jlimit(0.0f, 1.0f, rate), 0.18f);
        const float shapedBreath = std::pow(juce::jlimit(0.0f, 1.0f, breath), 0.20f);
        const float shapedGrowth = std::pow(juce::jlimit(0.0f, 1.0f, growth), 0.18f);

        rootAmt = shapedDepth;
        // Retuned from the previous block-based motion so the feel stays close to the 44.1k/512 baseline.
        rootRateHz = (0.0010f + shapedSoil * 0.120f) * (referenceBlocksPerSecond / juce::MathConstants<float>::twoPi);
        rootAnchorAmt = shapedAnchor;
        
        sapAmt = shapedFlow;
        sapDriftRateHz = (0.005f + shapedVitality * 0.05f) * referenceBlocksPerSecond;
        sapTextureAmt = shapedTexture;
        
        pulseAmt = shapedRate;
        pulseSens = shapedBreath;
        pulseGrowthAmt = shapedGrowth;
    }

    void update(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0)
            return;

        const float safeSampleRate = (float) juce::jmax(1.0, sampleRate);
        const float deltaTime = (float) numSamples / safeSampleRate;

        // 1. ROOT: Slow Organic Sine, integrated in real time instead of block-size steps.
        phase += rootRateHz * deltaTime * juce::MathConstants<float>::twoPi;
        while (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
        
        float root = (std::sin(phase) * 0.5f + 0.5f) * (0.02f + rootAmt * 1.18f);
        root = root * (1.0f - rootAnchorAmt * 0.90f) + (rootAnchorAmt * 0.14f);

        // 2. SAP: Fluid Drift
        const float sapDriftProbability = 1.0f - std::exp(-sapDriftRateHz * deltaTime);
        if (random.nextFloat() < sapDriftProbability)
            targetSap = random.nextFloat();
        
        // Roughly 20ms smoothing, independent of sample rate and buffer size.
        float sapSmoothing = 1.0f - std::exp(-(float) numSamples / (safeSampleRate * 0.02f));
        currentSap += (targetSap - currentSap) * sapSmoothing * sapAmt;
        
        float textureNoise = (random.nextFloat() - 0.5f) * sapTextureAmt * 0.25f;

        // 3. PULSE: Energy Follower
        float env = 0.0f;
        if (buffer.getNumChannels() > 0)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                env = juce::jmax(env, buffer.getMagnitude(ch, 0, numSamples));
        }
            
        env = juce::jlimit(0.0f, 1.0f, env * (1.2f + pulseSens * 16.0f));
        const float activity = juce::jlimit(0.0f, 1.0f, env + pulseFollower * 0.45f);

        // Block-size independent attack/release (roughly 10ms and 200ms)
        float attackCoeff = 1.0f - std::exp(-(float) numSamples / (safeSampleRate * (0.008f / (0.16f + pulseAmt * 3.0f))));
        float releaseCoeff = 1.0f - std::exp(-(float) numSamples / (safeSampleRate * (0.18f / (0.10f + pulseAmt * 2.8f))));

        pulseFollower += (env - pulseFollower) * (env > pulseFollower ? attackCoeff : releaseCoeff);
        pulseFollower = juce::jlimit(0.0f, 1.0f, pulseFollower);

        const float rootMotion = root * (0.10f + activity * 0.90f);
        const float sapMotion = currentSap * (0.08f + activity * 0.92f);

        const float growthTarget = juce::jlimit(0.0f, 1.0f,
                                                0.01f
                                                + rootMotion * 0.18f
                                                + sapMotion * 0.10f
                                                + pulseFollower * (0.14f + pulseGrowthAmt * 0.34f));
        const float growthRiseCoeff = 1.0f - std::exp(-(float) numSamples / (safeSampleRate * (0.09f + (1.0f - pulseGrowthAmt) * 0.10f)));
        const float growthFallCoeff = 1.0f - std::exp(-(float) numSamples / (safeSampleRate * (0.16f + rootAnchorAmt * 0.12f)));
        growthCycle += (growthTarget - growthCycle) * (growthTarget > growthCycle ? growthRiseCoeff : growthFallCoeff);
        growthCycle = juce::jlimit(0.0f, 1.0f, growthCycle);

        const float jitterImpulse = sapDriftProbability * activity * (0.16f + sapTextureAmt * 0.34f + pulseFollower * 0.12f);
        const float jitterTarget = juce::jlimit(0.0f, 1.0f,
                                                0.01f
                                                + std::abs(textureNoise) * 2.7f
                                                + sapTextureAmt * 0.10f * activity
                                                + pulseFollower * 0.08f
                                                + (random.nextFloat() < jitterImpulse ? random.nextFloat() * 0.36f : 0.0f));
        const float jitterCoeff = 1.0f - std::exp(-(float) numSamples / (safeSampleRate * (0.055f + 0.06f * (1.0f - sapTextureAmt))));
        leafJitter += (jitterTarget - leafJitter) * jitterCoeff;
        leafJitter = juce::jlimit(0.0f, 1.0f, leafJitter);

        const float tensionTarget = juce::jlimit(0.0f, 1.0f,
                                                 0.01f
                                                 + pulseFollower * (0.18f + pulseAmt * 0.28f)
                                                 + (1.0f - rootAnchorAmt) * 0.12f * activity
                                                 + sapTextureAmt * 0.06f * activity
                                                 + rootAmt * 0.10f * activity);
        const float tensionCoeff = 1.0f - std::exp(-(float) numSamples / (safeSampleRate * 0.11f));
        tension += (tensionTarget - tension) * tensionCoeff;
        tension = juce::jlimit(0.0f, 1.0f, tension);

        if (env < 0.0005f)
        {
            const float idleSapDecay = std::exp(-(float) numSamples / (safeSampleRate * 0.10f));
            const float idlePulseDecay = std::exp(-(float) numSamples / (safeSampleRate * 0.12f));
            const float idleGrowthDecay = std::exp(-(float) numSamples / (safeSampleRate * 0.14f));
            const float idleJitterDecay = std::exp(-(float) numSamples / (safeSampleRate * 0.12f));
            const float idleTensionDecay = std::exp(-(float) numSamples / (safeSampleRate * 0.12f));
            currentSap *= idleSapDecay;
            pulseFollower *= idlePulseDecay;
            growthCycle *= idleGrowthDecay;
            leafJitter *= idleJitterDecay;
            tension *= idleTensionDecay;
        }
        
        // Combine to Plant Energy
        float combinedSap = juce::jlimit(0.0f, 1.0f, currentSap + textureNoise);
        plantEnergy = 0.01f + rootMotion * 0.08f
                            + combinedSap * 0.08f * activity
                            + pulseFollower * (0.12f + pulseGrowthAmt * 0.24f)
                            + growthCycle * 0.10f
                            + tension * 0.08f;
        plantEnergy = juce::jlimit(0.0f, 1.0f, plantEnergy);

        bioFeedback.plantEnergy = plantEnergy;
        bioFeedback.growthCycle = juce::jlimit(0.0f, 1.0f, growthCycle);
        bioFeedback.leafJitter = juce::jlimit(0.0f, 1.0f, leafJitter);
        bioFeedback.tension = juce::jlimit(0.0f, 1.0f, tension);
    }

    float getPlantEnergy() const { return plantEnergy; }
    const RootFlowBioFeedbackSnapshot& getBioFeedbackSnapshot() const noexcept { return bioFeedback; }
	private:
    static constexpr float referenceSampleRate = 44100.0f;
    static constexpr float referenceBlockSize = 512.0f;

    double sampleRate = 44100.0;
    juce::Random random;
    float phase = 0;
    float rootAmt = 0.5f, rootRateHz = 0.068f, rootAnchorAmt = 0.5f;
    float sapAmt = 0.5f, sapDriftRateHz = 0.86f, sapTextureAmt = 0.5f;
    float pulseAmt = 0.5f, pulseSens = 0.5f, pulseGrowthAmt = 0.5f;
    float currentSap = 0, targetSap = 0;
    float pulseFollower = 0;
    float plantEnergy = 0;
    float growthCycle = 0;
    float leafJitter = 0;
    float tension = 0;
    RootFlowBioFeedbackSnapshot bioFeedback;
}; 
