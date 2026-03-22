#include "RootFlowVoice.h"
#include <cmath>

RootFlowVoice::RootFlowVoice()
{
    auto sine = [](float x) { return std::sin(x); };
    osc1.initialise(sine);
    osc2.initialise(sine);
    oscSub.initialise(sine);
    lfo.initialise(sine);
    driftLfo.initialise(sine);

    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    adsrParams.attack = 0.015f;
    adsrParams.decay = 0.18f;
    adsrParams.sustain = 0.72f;
    adsrParams.release = 0.45f;
    adsr.setParameters(adsrParams);
}

bool RootFlowVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<RootFlowSound*>(sound) != nullptr;
}

void RootFlowVoice::setSampleRate(double sr)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec{ sr, 512, 1 };
    osc1.prepare(spec);
    osc2.prepare(spec);
    oscSub.prepare(spec);
    lfo.prepare(spec);
    driftLfo.prepare(spec);
    filter.prepare(spec);
    adsr.setSampleRate(sr);

    smoothedFlow.reset(sr, 0.02);
    smoothedVitality.reset(sr, 0.02);
    smoothedTexture.reset(sr, 0.02);
    smoothedDepth.reset(sr, 0.02);
    smoothedPulseRate.reset(sr, 0.02);
    smoothedPulseAmount.reset(sr, 0.02);
    smoothedGrowth.reset(sr, 0.02);
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
    if (!isVoiceActive())
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Get smoothed parameters for this sample
        float currentFlow = smoothedFlow.getNextValue();
        float currentVital = smoothedVitality.getNextValue();
        float currentText = smoothedTexture.getNextValue();
        float currentDepth = smoothedDepth.getNextValue();
        float currentPulseR = smoothedPulseRate.getNextValue();
        float currentPulseA = smoothedPulseAmount.getNextValue();

        // 2. Update DSP parameters (Block-based smoothing for Filter)
        float growth = smoothedGrowth.getNextValue();
        float macroDetuneAmt = (currentDepth * 0.045f) + (growth * 0.08f);
        float macroDriftAmt = (0.0018f) + (growth * 0.0045f);
        float macroSpreadAmt = (currentDepth * 0.55f) + (growth * 0.65f);

        float drift = driftLfo.processSample(0.0f) * macroDriftAmt;
        float depthDetune = voiceDetuneRatio * macroDetuneAmt; 
        
        // OSC FREQ (Update if changed meaningfully or use per-sample if needed)
        osc1.setFrequency(baseFrequency * (1.1f + depthDetune + drift));
        osc2.setFrequency(baseFrequency * (0.9f - depthDetune - drift));
        oscSub.setFrequency(baseFrequency * 0.5f);

        // PULSE LFO
        lfo.setFrequency(juce::jmap(currentPulseR, 0.05f, 15.0f));
        float lfoVal = lfo.processSample(0.0f);
        
        float cutoffMod = (lfoVal * currentPulseA * 0.45f);
        float targetCutoff = juce::jmap(juce::jlimit(0.0f, 1.0f, currentFlow + cutoffMod), 120.0f, 13500.0f);
        float targetRes = juce::jmap(currentVital, 0.05f, 1.8f);
        
        // Zippering Fix: Soft filter smoothing
        filter.setCutoffFrequency(targetCutoff);
        filter.setResonance(targetRes);

        // 3. Oscillators Process
        float s1 = osc1.processSample(0.0f);
        float s2 = osc2.processSample(0.0f);
        float sSub = oscSub.processSample(0.0f);
        
        // MIX (Even beefier oscillator mix)
        float sample = (s1 + s2) * 0.55f + sSub * (currentDepth * 0.72f);

        // 4. TEXTURE (More aggressive Soft Clipping)
        if (currentText > 0.05f)
        {
            float drive = 1.0f + currentText * 9.5f;
            sample = std::tanh(sample * drive);
        }

        // 5. Filter
        sample = filter.processSample(0, sample);

        // 6. Envelope & Gain & PULSE Amplitude Mod
        float env = adsr.getNextSample();
        float amMod = 1.0f + (lfoVal * currentPulseA * 0.55f);
        sample *= env * level * 2.45f * amMod;

        // 7. Stereo Panning (Randomized per voice + Depth spread + GROWTH spread)
        float spreadPan = 0.5f + (voicePan * juce::jlimit(0.0f, 0.98f, macroSpreadAmt));
        buffer.addSample(0, startSample + i, sample * (1.0f - spreadPan));
        if (buffer.getNumChannels() > 1)
            buffer.addSample(1, startSample + i, sample * spreadPan);

        // Clear voice if envelope is done
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
