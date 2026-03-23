#include "RootFlowVoice.h"
#include "RootFlowModulators.h"
#include <cmath>

RootFlowVoice::RootFlowVoice()
{
    auto sine = [](float x) { return std::sin(x); };
    lfo.initialise(sine);
    driftLfo.initialise(sine);
    oscSub.initialise(sine);

    filterL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filterL.setResonance(0.707f); 
    filterR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filterR.setResonance(0.707f);
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
    for (int i = 0; i < maxUnison; ++i)
        unisonOscillators[i].prepare(sr);
    
    oscSub.prepare(spec);
    lfo.prepare(spec);
    driftLfo.prepare(spec);
    filterL.prepare(spec);
    filterL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filterL.setResonance(0.707f);
    filterR.prepare(spec);
    filterR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filterR.setResonance(0.707f);
    
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
    smoothedCanopy.reset(sr, 0.02);
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

    // Initialize Unison Offsets & Phases
    for (int i = 0; i < unisonVoices; ++i)
    {
        if (unisonVoices > 1)
            unisonOffsets[i] = (float(i) / float(unisonVoices - 1)) - 0.5f;
        else
            unisonOffsets[i] = 0.0f;

        unisonOscillators[i].setPhase(r.nextFloat());
    }
    
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
    filterL.reset();
    filterR.reset();
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

    float masterScale = level * 0.088f * (0.58f + currentEnergy * 0.42f);
    
    // Extreme 'BITE' Scaling (Parabolic Mapping) - STABILIZED
    float macroDetuneAmt = (currentDepth * currentDepth * 0.165f) + (growth * growth * 0.225f);
    float macroDriftAmt = (0.0030f) + (growth * 0.0085f);
    float macroSpreadAmt = (currentDepth * 0.94f) + (growth * 0.82f);
    
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
        float currentCanopy = smoothedCanopy.getNextValue();
        currentEnergy = smoothedEnergy.getNextValue();

        // 2. Sub-Sample Parameter Updates (Every 16 samples)
        if ((subSampleCount++ & 15) == 0)
        {
            // Filter Update
            // Sicherer Cutoff-Bereich: 20Hz bis 18kHz
            float baseCutoff = 52.0f + (growth * growth * 3600.0f) + (currentEnergy * 5800.0f);
            float velMod = juce::jmap(level, 0.0f, 1.0f, 0.0f, 8200.0f);
            float finalCutoff = juce::jlimit(20.0f, 18000.0f, baseCutoff + velMod);
            
            // Resonanz-Limitierung: 
            // Wir begrenzen 'res' auf maximal 0.95f, um Instabilität zu vermeiden.
            float res = juce::jlimit(0.707f, 0.95f, 0.707f + (currentEnergy * 0.25f)); 
            
            filterL.setCutoffFrequency(finalCutoff);
            filterL.setResonance(res);
            filterR.setCutoffFrequency(finalCutoff);
            filterR.setResonance(res);
        }     // Osc Frequency Update (Heavy Math)
        float lfoVal = lfo.processSample(0.0f);
        float env = adsr.getNextSample();
        
        float leftSum = 0.0f;
        float rightSum = 0.0f;
        
        float detuneBase = (voiceDetuneRatio * macroDetuneAmt * 1.15f) + (currentVital * 0.12f);
        float driftInSemis = driftLfo.processSample(0.0f) * macroDriftAmt * 4.0f;

        // Phase/Frequency Agitation (Vitality Instability) - RE-TUNED FOR STABILITY
        float agitation = (currentVital * currentVital * 0.35f) + (growth * 0.12f);

        // Bio-Dynamic Unison Params
        float dynamicDetune = 0.008f + (currentEnergy * 0.042f);
        float dynamicSpread = 0.22f + (currentEnergy * 0.78f);
        
        // Basis-PWM durch sapFlow (currentFlow) und LFO moduliert
        float basePw = 0.5f + (lfoVal * currentFlow * 0.45f);

        for (int v = 0; v < unisonVoices; ++v)
        {
            // Detune per Unison Voice (with unique micro-drift per voice)
            float unisonDetuneInSemis = unisonOffsets[v] * dynamicDetune * 12.0f; 
            float voiceDrift = driftInSemis * (1.0f + (float)v * 0.12f);
            
            // Apply agitation jitter for "Electric Life"
            float jitter = (noiseGen.nextFloat() - 0.5f) * agitation;
            float voiceFreq = baseFrequency * std::exp2((detuneBase + voiceDrift + unisonDetuneInSemis + jitter) / 12.0f);
            
            // Individuelle Pulse Width per Stimmer für fetteren lebendigen Sound (via sapFlow)
            float voicePw = basePw + (std::sin(voiceDrift * 15.0f) * currentFlow * 0.15f);
            
            unisonOscillators[v].setFrequency(voiceFreq);
            unisonOscillators[v].setPulseWidth(voicePw);
            float osc = unisonOscillators[v].nextSample();

            // Panning per Unison Voice (with randomized movement/bloom)
            float panMovement = (noiseGen.nextFloat() - 0.5f) * 0.004f;
            float voiceSpreadPan = 0.5f + (unisonOffsets[v] + panMovement) * dynamicSpread * 0.5f;
            
            leftSum += osc * (1.0f - voiceSpreadPan);
            rightSum += osc * voiceSpreadPan;
        }

        float sSub = oscSub.processSample(0.0f);
        oscSub.setFrequency(baseFrequency * 0.5f);
        
        float subAmt = (currentDepth * currentDepth * 0.74f) + (currentEnergy * 0.18f);
        
        // Normalize by voices (Power preservation factor)
        float normalization = 1.0f / std::sqrt((float)unisonVoices);
        float leftOut = (leftSum * 0.42f * normalization) + (sSub * subAmt * 0.72f);
        float rightOut = (rightSum * 0.42f * normalization) + (sSub * subAmt * 0.72f);
        
        // Anti-Denormal
        float noise = (noiseGen.nextFloat() * 2.0f - 1.0f) * 1e-18f;
        leftOut += noise;
        rightOut += noise;

        // 4. Resonant Filter (SVF) - Independent per channel for stereo
        float L = filterL.processSample(0, leftOut);
        float R = filterR.processSample(0, rightOut); 
        
        // --- NEU: ORGANIC PROTECTION (Soft Clipping) ---
        // Anstatt hartem Clipping nutzen wir eine Sättigungskurve.
        // Das bändigt die Resonanzspitzen des Filters.
        auto safeSaturate = [](float& sample) {
            // Schützt vor Extremwerten und fügt Obertöne hinzu
            sample = std::tanh(sample * 1.2f); 
        };
        
        safeSaturate(L);
        safeSaturate(R);
        
        // 5. TEXTURE (Smooth Saturation)
        if (currentText > 0.02f)
        {
            float drive = (1.0f + (currentText * currentText * 10.5f)) * (1.2f + currentEnergy * 0.85f);
            L = std::tanh(L * drive);
            R = std::tanh(R * drive);
        }

        // 6. CANOPY AIR (One-Pole High Shelf) - STABILIZED SHIMMER
        if (currentCanopy > 0.05f)
        {
            float airGain = currentCanopy * 0.65f;
            float dL = L - lastLeftFilterOut;
            float dR = R - lastRightFilterOut;
            L = L + std::tanh(dL * airGain);
            R = R + std::tanh(dR * airGain);
        }
        lastLeftFilterOut = L;
        lastRightFilterOut = R;

        float amMod = 1.0f - (std::abs(lfoVal) * currentPulseA * 0.18f);
        float gain = env * masterScale * amMod;
        
        L *= gain;
        R *= gain;

        // Final Safe Guard
        L = L / (1.0f + std::abs(L * 0.12f));
        R = R / (1.0f + std::abs(R * 0.12f));

        // 6. Stereo Master Panning (Global shift)
        float spreadPan = 0.5f + (voicePan * juce::jlimit(0.0f, 0.98f, macroSpreadAmt));
        buffer.addSample(0, startSample + i, L * (1.0f - spreadPan));
        if (buffer.getNumChannels() > 1)
            buffer.addSample(1, startSample + i, R * spreadPan);

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
void RootFlowVoice::setCanopy(float v) { smoothedCanopy.setTargetValue(v); }

void RootFlowVoice::setWaveform(int typeIndex)
{
    for (int i = 0; i < maxUnison; ++i)
        unisonOscillators[i].setWaveform(typeIndex);
}
