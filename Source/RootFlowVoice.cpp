#include "RootFlowVoice.h"
#include "RootFlowModulators.h"
#include <cmath>

RootFlowVoice::RootFlowVoice()
{
    auto sine = [](float x) { return std::sin(x); };
    lfo.initialise(sine);
    driftLfo.initialise(sine);
    oscSub.initialise(sine);

    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setResonance(0.707f); 

    adsrParams.attack = 0.015f;  // 15ms Attack
    adsrParams.decay = 0.18f;
    adsrParams.sustain = 0.72f;
    adsrParams.release = 0.25f;  // 250ms Release
    adsr.setParameters(adsrParams);
}

bool RootFlowVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<RootFlowSound*>(sound) != nullptr;
}

void RootFlowVoice::setSampleRate(double sr, int blockSize)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec{ sr, (juce::uint32) juce::jmax(blockSize, 1), 1 };
    osc1.prepare(sr);
    osc2.prepare(sr);
    oscSub.prepare(spec);
    lfo.prepare(spec);
    driftLfo.prepare(spec);
    filter.prepare(spec);
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setResonance(0.707f);
    
    // Noise Filter (Lowpass to keep it organic)
    *noiseFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 1200.0f);
    noiseFilter.prepare(spec);
    adsr.setSampleRate(sr);

    smoothedFlow.reset(sr, 0.02);
    smoothedVitality.reset(sr, 0.02);
    smoothedTexture.reset(sr, 0.02);
    smoothedDepth.reset(sr, 0.02);
    smoothedPulseRate.reset(sr, 0.02);
    smoothedPulseAmount.reset(sr, 0.02);
    smoothedGrowth.reset(sr, 0.02);
    smoothedEnergy.reset(sr, 0.05);
    
    // Bio-Filter states
    lastLeftFilterOut = 0.0f;
    lastRightFilterOut = 0.0f;
}

void RootFlowVoice::startNote(int midiNoteNumber, float velocity,
                              juce::SynthesiserSound*, int)
{
    baseFrequency = (float)juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    
    // personality per voice
    auto& r = juce::Random::getSystemRandom();
    voicePan = r.nextFloat() * 2.0f - 1.0f;
    voiceDetuneRatio = r.nextFloat() * 2.0f - 1.0f;
    
    // random drift speed
    driftLfo.setFrequency(0.05f + r.nextFloat() * 0.25f);
    
    level = velocity;
    adsr.noteOn();
}

void RootFlowVoice::stopNote(float, bool allowTailOff)
{
    adsr.noteOff();

    if (!allowTailOff || !adsr.isActive())
        clearCurrentNote();
}

void RootFlowVoice::pitchWheelMoved(int) {}
void RootFlowVoice::controllerMoved(int, int) {}
void RootFlowVoice::aftertouchChanged(int) {}
void RootFlowVoice::channelPressureChanged(int) {}

void RootFlowVoice::reset()
{
    adsr.reset();
    filter.reset();
}

void RootFlowVoice::recoverFromSilentState()
{
    reset();
}

void RootFlowVoice::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                    int startSample, int numSamples)
{
    // Sub-sample smoothing counter to save CPU on heavy math
    int subSampleCount = 0;
    
    // Initial block-level cached values
    float currentFlow = smoothedFlow.getNextValue();
    float currentVital = smoothedVitality.getNextValue();
    float currentText = smoothedTexture.getNextValue();
    float currentDepth = smoothedDepth.getNextValue();
    float currentPulseR = smoothedPulseRate.getNextValue();
    float currentPulseA = smoothedPulseAmount.getNextValue();
    float growth = smoothedGrowth.getNextValue();
    float targetEnergy = (engine != nullptr) ? engine->getPlantEnergy() : 0.5f;
    smoothedEnergy.setTargetValue(targetEnergy);
    float currentEnergy = smoothedEnergy.getNextValue();

    float masterScale = level * 0.058f * (0.65f + currentEnergy * 0.35f);
    
    // Pre-calculate block-level constants
    float macroDetuneAmt = (currentDepth * 0.045f) + (growth * 0.08f);
    float macroDriftAmt = (0.0018f) + (growth * 0.0045f);
    float macroSpreadAmt = (currentDepth * 0.55f) + (growth * 0.65f);
    
    lfo.setFrequency(juce::jmap(currentPulseR, 0.05f, 15.0f));

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Per-Sample Smoothing Advance
        currentFlow = smoothedFlow.getNextValue();
        currentVital = smoothedVitality.getNextValue();
        currentText = smoothedTexture.getNextValue();
        currentDepth = smoothedDepth.getNextValue();
        currentPulseR = smoothedPulseRate.getNextValue();
        currentPulseA = smoothedPulseAmount.getNextValue();
        growth = smoothedGrowth.getNextValue();
        currentEnergy = smoothedEnergy.getNextValue();

        // 2. Sub-Sample Parameter Updates (Every 16 samples)
        if ((subSampleCount++ & 15) == 0)
        {
            // Filter Update
            float baseCutoff = 110.0f + (growth * 1400.0f) + (currentEnergy * 2600.0f);
            float velMod = juce::jmap(level, 0.0f, 1.0f, 0.0f, 4200.0f);
            filter.setCutoffFrequency(juce::jlimit(20.0f, 20000.0f, baseCutoff + velMod));
            filter.setResonance(0.707f);

            // Osc Frequency Update (Heavy Math)
            float detuneInSemis = (voiceDetuneRatio * macroDetuneAmt * 1.15f) + (currentVital * 0.12f);
            float driftInSemis = driftLfo.processSample(0.0f) * macroDriftAmt * 4.0f;
            
            osc1.setFrequency(baseFrequency * std::exp2((detuneInSemis + driftInSemis) / 12.0f));
            osc2.setFrequency(baseFrequency * std::exp2((-detuneInSemis - driftInSemis) / 12.0f));
            oscSub.setFrequency(baseFrequency * 0.5f);
            
            masterScale = level * 0.058f * (0.65f + currentEnergy * 0.35f);
        }

        // 3. Audio Processing
        float lfoVal = lfo.processSample(0.0f);
        float env = adsr.getNextSample();
        
        float s1 = osc1.nextSample();
        float s2 = osc2.nextSample();
        float sSub = oscSub.processSample(0.0f);
        
        float subAmt = currentDepth * 0.44f;
        float sample = (s1 + s2) * 0.28f + sSub * subAmt;
        
        // Anti-Denormal
        sample += (noiseGen.nextFloat() * 2.0f - 1.0f) * 1e-18f;

        // 4. Resonant Filter (SVF, Q=0.707)
        sample = filter.processSample(0, sample);

        // 5. TEXTURE (Smooth Saturation)
        if (currentText > 0.12f)
        {
            // Halve input for dynamics (User request)
            float drive = (1.0f + (currentText - 0.12f) * 2.15f) * (1.0f + currentEnergy * 0.35f);
            float x = sample * 0.5f * drive; 
            sample = x / (1.0f + std::abs(x)); 
        }

        float amMod = 1.0f - (std::abs(lfoVal) * currentPulseA * 0.18f);
        sample *= env * masterScale * amMod;
        
        // Final Safe Guard
        sample = sample / (1.0f + std::abs(sample * 0.12f));

        // 6. Stereo Panning
        float spreadPan = 0.5f + (voicePan * juce::jlimit(0.0f, 0.98f, macroSpreadAmt));
        buffer.addSample(0, startSample + i, sample * (1.0f - spreadPan));
        if (buffer.getNumChannels() > 1)
            buffer.addSample(1, startSample + i, sample * spreadPan);

        if (!adsr.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

// --- Parameter Mappings ---
void RootFlowVoice::setFlow(float v)    { smoothedFlow.setTargetValue(v); }
void RootFlowVoice::setVitality(float v) { smoothedVitality.setTargetValue(v); }
void RootFlowVoice::setTexture(float v)  { smoothedTexture.setTargetValue(v); }
void RootFlowVoice::setDepth(float v)    { smoothedDepth.setTargetValue(v); }
void RootFlowVoice::setGrowth(float v)      { smoothedGrowth.setTargetValue(v); }
void RootFlowVoice::setPulseRate(float v) { smoothedPulseRate.setTargetValue(v); }
void RootFlowVoice::setPulseAmount(float v) { smoothedPulseAmount.setTargetValue(v); }
