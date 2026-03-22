#pragma once
#include <JuceHeader.h>

/**
 * Snapshot for UI Visualizers.
 */
struct RootFlowBioFeedbackSnapshot
{
    float plantEnergy = 0.0f;
    float growthCycle = 0.0f;
    float leafJitter = 0.0f;
    float tension = 0.0f;
};

/**
 * The 'Brain' of RootFlow.
 * Fuses ROOT (stability), SAP (vitality), and PULSE (interaction)
 * into a single organic Energy value.
 */
class RootFlowModulationEngine
{
public:
    void prepare(double sr, int)
    {
        sampleRate = sr;
        smoothedPlantEnergy.reset(sr, 0.1); 
        
        phase = 0.0f;
        currentSap = 0.0f;
        targetSap = 0.0f;
        pulseFollower = 0.0f;
        plantEnergy = 0.0f;
        bioFeedback = {};
    }

    void setParams(float depth, float soil, float anchor, 
                    float flow, float vitality, float texture,
                    float rate, float breath, float growth)
    {
        // ROOT (Stability): Slow, deep movements
        rootAmt = depth;
        rootAnchorAmt = anchor;
        // Quadratic scaling for finer control over ultra-slow drift
        rootSpeed = 0.00015f + (soil * soil * 0.0125f); 
        
        // SAP (Vitality): Fluid Drift and Organic 'Juice' flow
        sapAmt = flow;
        sapSpeed = 0.001f + vitality * 0.035f;
        sapTextureAmt = texture;
        
        // PULSE (Interaction): Reaction to Audio Input / Envelopes
        pulseAmt = rate;
        pulseSens = breath;
        pulseGrowthAmt = growth;
    }

    void update(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0) return;

        // 1. ROOT (The Foundation)
        // Very slow organic sine wave.
        phase += rootSpeed;
        if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
        
        float root = (std::sin(phase) * 0.5f + 0.5f) * rootAmt;
        // Mix in Anchor (static bias)
        root = root * (1.0f - rootAnchorAmt * 0.75f) + (rootAnchorAmt * 0.45f);

        // 2. SAP (The Vitality Loop)
        // Random walk for unpredictable but smooth changes.
        if (random.nextFloat() > (1.0f - sapSpeed)) 
            targetSap = random.nextFloat();
        
        // Approx. 30ms smoothing for the sap drift
        currentSap += (targetSap - currentSap) * 0.0055f;
        float sap = currentSap * sapAmt;
        
        // Texture creates micro-nervousness
        float textureNoise = (random.nextFloat() * 2.0f - 1.0f) * sapTextureAmt * 0.12f;

        // 3. PULSE (The Dynamic Reaction)
        // Envelope Follower tracking the input magnitude.
        float env = 0.0f;
        if (buffer.getNumChannels() > 0)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                env = juce::jmax(env, buffer.getMagnitude(ch, 0, numSamples));
        }

        // Sensitivity scaling
        env = juce::jlimit(0.0f, 1.0f, env * (1.15f + pulseSens * 14.0f));

        // Attack/Release characteristic
        float attack = 0.42f;
        float release = 0.024f;
        pulseFollower += (env - pulseFollower) * (env > pulseFollower ? attack : release) * (0.15f + pulseAmt * 0.85f);
        pulseFollower = juce::jlimit(0.0f, 1.0f, pulseFollower);

        // 4. SYNERGY (Combining into beautiful creature)
        // Base energy ensures heartbeat even when silent.
        float rawEnergy = 0.12f 
                        + (root * 0.22f) 
                        + (sap * 0.28f) 
                        + (pulseFollower * 0.46f) 
                        + (textureNoise * 0.05f);
        
        plantEnergy = juce::jlimit(0.0f, 1.0f, rawEnergy);
        smoothedPlantEnergy.setTargetValue(plantEnergy);

        // Map internal states to the Snapshot for the Visualizer
        bioFeedback.plantEnergy = smoothedPlantEnergy.getNextValue();
        bioFeedback.growthCycle = juce::jlimit(0.0f, 1.0f, root * 1.15f + pulseFollower * 0.25f);
        bioFeedback.leafJitter = juce::jlimit(0.0f, 1.0f, currentSap + std::abs(textureNoise) * 4.0f);
        bioFeedback.tension = juce::jlimit(0.0f, 1.0f, pulseFollower * 0.95f + (1.0f - rootAnchorAmt) * 0.15f);
    }

    float getPlantEnergy() const { return bioFeedback.plantEnergy; }
    const RootFlowBioFeedbackSnapshot& getBioFeedbackSnapshot() const noexcept { return bioFeedback; }

private:
    double sampleRate = 44100.0;
    juce::Random random;
    juce::LinearSmoothedValue<float> smoothedPlantEnergy;
    
    float phase = 0;
    float rootAmt = 0.5f, rootSpeed = 0.005f, rootAnchorAmt = 0.5f;
    float sapAmt = 0.5f, sapSpeed = 0.01f, sapTextureAmt = 0.5f;
    float pulseAmt = 0.5f, pulseSens = 0.5f, pulseGrowthAmt = 0.5f;
    float currentSap = 0, targetSap = 0;
    float pulseFollower = 0;
    float plantEnergy = 0;
    
    RootFlowBioFeedbackSnapshot bioFeedback;
};
