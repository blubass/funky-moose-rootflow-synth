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
        smoothedPlantEnergy.reset(sr, 0.05); 
        
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
        plantEnergy = 0.0f;
        bioFeedback = {};
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
        if (numSamples <= 0) return;

        // Fetch Smoothed Parameters for this block
        float rootAmt = s_rootAmt.getNextValue();
        float rootAnchorAmt = s_rootAnchorAmt.getNextValue();
        float rootSpeed = s_rootSpeed.getNextValue();
        float sapAmt = s_sapAmt.getNextValue();
        float sapSpeed = s_sapSpeed.getNextValue();
        float sapTextureAmt = s_sapTextureAmt.getNextValue();
        float pulseAmt = s_pulseAmt.getNextValue();
        float pulseSens = s_pulseSens.getNextValue();
        float pulseGrowthAmt = s_pulseGrowthAmt.getNextValue();

        // 1. ROOT (The Foundation)
        phase += rootSpeed;
        if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
        
        float root = (std::sin(phase) * 0.5f + 0.5f) * rootAmt;
        root = root * (1.0f - rootAnchorAmt * 0.75f) + (rootAnchorAmt * 0.45f);

        // 2. SAP (The Vitality Loop)
        if (random.nextFloat() > (1.0f - sapSpeed)) 
            targetSap = random.nextFloat();
        
        currentSap += (targetSap - currentSap) * 0.0055f;
        float sap = currentSap * sapAmt;
        float textureNoise = (random.nextFloat() * 2.0f - 1.0f) * sapTextureAmt * 0.12f;

        // 3. PULSE (The Dynamic Reaction)
        float env = 0.0f;
        if (buffer.getNumChannels() > 0)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                env = juce::jmax(env, buffer.getMagnitude(ch, 0, numSamples));
        }

        env = juce::jlimit(0.0f, 1.0f, env * (1.15f + pulseSens * 14.0f));

        float attack = 0.42f;
        float release = 0.024f;
        pulseFollower += (env - pulseFollower) * (env > pulseFollower ? attack : release) * (0.15f + pulseAmt * 0.85f);
        pulseFollower = juce::jlimit(0.0f, 1.0f, pulseFollower);

        // 4. SYNERGY
        float rawEnergy = 0.12f 
                        + (root * 0.22f) 
                        + (sap * 0.28f) 
                        + (pulseFollower * 0.46f) 
                        + (textureNoise * 0.05f);
        
        plantEnergy = juce::jlimit(0.0f, 1.0f, rawEnergy);
        smoothedPlantEnergy.setTargetValue(plantEnergy);

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
    
    // Parameter Smoothing
    juce::LinearSmoothedValue<float> s_rootAmt, s_rootAnchorAmt, s_rootSpeed;
    juce::LinearSmoothedValue<float> s_sapAmt, s_sapSpeed, s_sapTextureAmt;
    juce::LinearSmoothedValue<float> s_pulseAmt, s_pulseSens, s_pulseGrowthAmt;

    float phase = 0;
    float currentSap = 0, targetSap = 0;
    float pulseFollower = 0;
    float plantEnergy = 0;
    
    RootFlowBioFeedbackSnapshot bioFeedback;
};
