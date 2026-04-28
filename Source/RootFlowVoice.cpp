#include "RootFlowVoice.h"
#include "RootFlowModulators.h"
#include <cmath>

namespace
{
float clampStateVariableCutoff(float cutoffHz, double sampleRate) noexcept
{
    const auto safeSampleRate = juce::jmax(1.0, sampleRate);
    const auto maxCutoff = juce::jmax(10.0f, (float) safeSampleRate * 0.49f);
    const auto minCutoff = juce::jmin(20.0f, maxCutoff * 0.5f);
    return juce::jlimit(minCutoff, maxCutoff, cutoffHz);
}
}

RootFlowVoice::RootFlowVoice()
{
    auto sine = [](float x) { return std::sin(x); };
    lfo.initialise(sine);
    driftLfo.initialise(sine);
    oscSub.initialise(sine);
    vibratoLfo.initialise(sine);

    filterL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filterL.setResonance(0.707f);
    filterR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filterR.setResonance(0.707f);
    adsrParams.attack = 0.015f;  // 15ms Attack
    adsrParams.decay = 0.18f;
    adsrParams.sustain = 0.72f;
    adsrParams.release = 0.68f;  // Increased from 0.25f for softer stopping
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
    vibratoLfo.prepare(spec);
    filterL.prepare(spec);
    filterL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filterL.setResonance(0.707f);
    filterR.prepare(spec);
    filterR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filterR.setResonance(0.707f);

    // Noise Filter (Lowpass for spectral texture)
    noiseFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 1200.0f);
    noiseFilter.prepare(spec);
    adsr.setSampleRate(sr);

    smoothedFlowRate.reset(sr, 0.02);
    smoothedFlowEnergy.reset(sr, 0.02);
    smoothedFlowTexture.reset(sr, 0.02);
    smoothedSourceDepth.reset(sr, 0.02);
    smoothedPulseFrequency.reset(sr, 0.02);
    smoothedPulseWidth.reset(sr, 0.02);
    smoothedPulseEnergy.reset(sr, 0.02);
    smoothedFieldComplexity.reset(sr, 0.02);
    smoothedInstability.reset(sr, 0.02);
    smoothedEnergy.reset(sr, 0.05);
    smoothedPitchBendSemitones.reset(sr, 0.01);
    smoothedModWheel.reset(sr, 0.04);

    // Matrix-Filter states
    lastLeftFilterOut = 0.0f;
    lastRightFilterOut = 0.0f;
}

void RootFlowVoice::startNote(int midiNoteNumber, float velocity,
                              juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    const int midiChannel = getActiveMidiChannel();
    lastPitchWheelValue = currentPitchWheelPosition;
    baseFrequency = (float)juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    smoothedPitchBendSemitones.setCurrentAndTargetValue(pitchWheelToSemitones(midiChannel, currentPitchWheelPosition));
    smoothedModWheel.setCurrentAndTargetValue(getModWheelValueForChannel(midiChannel));

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
    vibratoLfo.setFrequency(4.6f + r.nextFloat() * 0.9f);

    // Determine Unison count at note-on to avoid clicks
    if (engine != nullptr)
    {
        float energy = engine->getSystemEnergy();
        int voiceVoices = 3;
        if (energy > 0.45f) voiceVoices = 6;
        if (energy > 0.85f) voiceVoices = 8;
        setUnison(voiceVoices);
    }

    level = velocity;
    adsr.noteOn();
}

void RootFlowVoice::stopNote(float, bool allowTailOff)
{
    adsr.noteOff();

    if (!allowTailOff || !adsr.isActive())
        clearCurrentNote();
}

void RootFlowVoice::pitchWheelMoved(int newPitchWheelValue)
{
    lastPitchWheelValue = newPitchWheelValue;
    smoothedPitchBendSemitones.setTargetValue(pitchWheelToSemitones(getActiveMidiChannel(), newPitchWheelValue));
}

void RootFlowVoice::controllerMoved(int controllerNumber, int newControllerValue)
{
    if (controllerNumber == 1)
        smoothedModWheel.setTargetValue(juce::jlimit(0.0f, 1.0f, (float) newControllerValue / 127.0f));
}
void RootFlowVoice::aftertouchChanged(int) {}
void RootFlowVoice::channelPressureChanged(int) {}

void RootFlowVoice::reset()
{
    adsr.reset();
    level = 0.0f;
    baseFrequency = 440.0f;
    voicePan = 0.0f;
    voiceDetuneRatio = 0.0f;
    lastPitchWheelValue = 8192;

    for (int i = 0; i < maxUnison; ++i)
        unisonOffsets[i] = 0.0f;

    lastLeftFilterOut = 0.0f;
    lastRightFilterOut = 0.0f;

    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    smoothedFlowRate.reset(sr, 0.02);
    smoothedFlowEnergy.reset(sr, 0.02);
    smoothedFlowTexture.reset(sr, 0.02);
    smoothedSourceDepth.reset(sr, 0.02);
    smoothedPulseFrequency.reset(sr, 0.02);
    smoothedPulseWidth.reset(sr, 0.02);
    smoothedPulseEnergy.reset(sr, 0.02);
    smoothedFieldComplexity.reset(sr, 0.02);
    smoothedInstability.reset(sr, 0.02);
    smoothedEnergy.reset(sr, 0.05);
    smoothedPitchBendSemitones.reset(sr, 0.01);
    smoothedModWheel.reset(sr, 0.01);

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
    float flowRate = smoothedFlowRate.getNextValue();
    float flowEnergy = smoothedFlowEnergy.getNextValue();
    float flowTexture = smoothedFlowTexture.getNextValue();
    float sourceDepth = smoothedSourceDepth.getNextValue();
    float pulseFreq = smoothedPulseFrequency.getNextValue();
    float pulseWidth = smoothedPulseWidth.getNextValue();
    float pulseEnergy = smoothedPulseEnergy.getNextValue();
    float currentInstability = smoothedInstability.getNextValue();
    float targetEnergy = (engine != nullptr) ? engine->getSystemEnergy() : 0.5f;
    smoothedEnergy.setTargetValue(targetEnergy);
    float currentEnergy = smoothedEnergy.getNextValue();
    float currentPitchBendSemitones = smoothedPitchBendSemitones.getNextValue();
    float currentModWheel = smoothedModWheel.getNextValue();

    float masterScale = level * 0.088f * (0.58f + currentEnergy * 0.42f);

    // Extreme 'BITE' Scaling (Parabolic Mapping) - STABILIZED
    float macroDetuneAmt = (sourceDepth * sourceDepth * 0.165f) + (pulseEnergy * pulseEnergy * 0.225f);
    float macroDriftAmt = (0.0030f) + (pulseEnergy * 0.0085f);
    float macroSpreadAmt = (sourceDepth * 0.94f) + (pulseEnergy * 0.82f);
    float envCutoffMod = 0.0f;
    float env = 0.0f;

    lfo.setFrequency(juce::jmap(pulseFreq, 0.05f, 15.0f));

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Per-Sample Smoothing Advance
        flowRate = smoothedFlowRate.getNextValue();
        flowEnergy = smoothedFlowEnergy.getNextValue();
        flowTexture = smoothedFlowTexture.getNextValue();
        sourceDepth = smoothedSourceDepth.getNextValue();
        pulseFreq = smoothedPulseFrequency.getNextValue();
        pulseWidth = smoothedPulseWidth.getNextValue();
        pulseEnergy = smoothedPulseEnergy.getNextValue();
        currentInstability = smoothedInstability.getNextValue();
        float fieldComplexity = smoothedFieldComplexity.getNextValue();
        currentEnergy = smoothedEnergy.getNextValue();
        currentPitchBendSemitones = smoothedPitchBendSemitones.getNextValue();
        currentModWheel = smoothedModWheel.getNextValue();

        // 2. Sub-Sample Parameter Updates (Every 16 samples)
        if ((subSampleCount++ & 15) == 0)
        {
            // Filter Update
            // Quantum Touch Filter Dynamics (Velocity mapping to Timbre)
            float baseCutoff = 32.0f + (pulseEnergy * pulseEnergy * 2400.0f) + (currentEnergy * 3200.0f);
            const float modWheelLift = currentModWheel * (900.0f + flowEnergy * 1800.0f);
            float velMod = juce::jmap(level * level, 0.0f, 1.0f, 0.0f, 12800.0f); 
            // Analog Filter Instability Jitter (VCF Drift)
            float filterJitter = (noiseGen.nextFloat() - 0.5f) * 15.0f * currentInstability;
            
            // Add the new Envelope Modulation (envCutoffMod is calculated in the sample loop)
            // Note: Since this is in the 16-sample block, we use the value from the previous sample loop iteration
            // which is perfectly fine for filter modulation.
            float finalCutoff = clampStateVariableCutoff(baseCutoff + modWheelLift + velMod + filterJitter + envCutoffMod, sampleRate);

            // Resonance Limitation (Slightly dampened for analog warmth)
            float maxRes = 0.92f - (pulseEnergy * 0.1f);
            float res = juce::jlimit(0.707f, maxRes, 0.70f + (currentEnergy * 0.22f));

            filterL.setCutoffFrequency(finalCutoff);
            filterL.setResonance(res);
            filterR.setCutoffFrequency(finalCutoff);
            filterR.setResonance(res);

            // Update LFO frequencies smoothly
            lfo.setFrequency(juce::jmap(pulseFreq, 0.05f, 15.0f));
            const float vibratoRateHz = 4.2f + pulseFreq * 2.4f;
            vibratoLfo.setFrequency(vibratoRateHz);
        }     // Osc Frequency Update (Heavy Math)
        float lfoVal = lfo.processSample(0.0f);
        const float vibratoLfoValue = vibratoLfo.processSample(0.0f);
        const float vibratoDepthSemitones = currentModWheel * (0.08f + flowEnergy * 0.42f + pulseEnergy * 0.18f);
        const float expressivePitchSemitones = currentPitchBendSemitones + vibratoLfoValue * vibratoDepthSemitones;
        
        // --- ADSR SHAPING (Musical Snapping) ---
        // We apply a power curve to the linear ADSR to get exponential-like decay/release.
        float rawEnv = adsr.getNextSample();
        float env = std::pow(rawEnv, 1.55f); 
        
        // Filter Envelope Modulation (makes the 'bite' much more musical)
        float envCutoffMod = std::pow(rawEnv, 2.2f) * (flowEnergy * 6500.0f);

        float leftSum = 0.0f;
        float rightSum = 0.0f;

        float detuneBase = (voiceDetuneRatio * macroDetuneAmt * 1.15f) + (flowEnergy * 0.12f);
        float driftInSemis = driftLfo.processSample(0.0f) * macroDriftAmt * 4.0f;

        // Phase/Frequency Agitation (Vitality Instability) - RE-TUNED FOR STABILITY
        float agitation = (flowEnergy * flowEnergy * 0.35f) + (pulseEnergy * 0.12f);

        // Matrix-Dynamic Unison Params
        float dynamicDetune = 0.008f + (currentEnergy * 0.042f);
        float dynamicDetuneSpread = 0.22f + (currentEnergy * 0.78f);

        // Basis-PWM durch flowRate und LFO moduliert
        float basePw = 0.5f + (lfoVal * flowRate * 0.45f);

        for (int v = 0; v < unisonVoices; ++v)
        {
            // Detune per Unison Voice (with unique micro-drift per voice)
            float unisonDetuneInSemis = unisonOffsets[v] * dynamicDetune * 12.0f;
            float voiceDrift = driftInSemis * (1.0f + (float)v * 0.12f);

            // Apply analog component drift and instability for "VCO Life"
            float jitter = (noiseGen.nextFloat() - 0.5f) * agitation;
            float analogWow = std::sin((float)i * 0.0001f + unisonOffsets[v]) * 0.005f; // Slow tape-like wow
            float flutter = (noiseGen.nextFloat() - 0.5f) * 0.002f; // Fast flutter

            float voiceFreq = baseFrequency * std::exp2((expressivePitchSemitones + detuneBase + voiceDrift
                                                         + unisonDetuneInSemis + jitter + analogWow + flutter) / 12.0f);

            // Individuelle Pulse Width per Stimmer für fetteren lebendigen Sound (via flowRate)
            float voicePw = basePw + (std::sin(voiceDrift * 15.0f) * flowRate * 0.15f);

            unisonOscillators[v].setFrequency(voiceFreq);
            unisonOscillators[v].setPulseWidth(voicePw);
            float osc = unisonOscillators[v].nextSample();

            // Panning per Unison Voice (with randomized movement/bloom)
            float panMovement = (noiseGen.nextFloat() - 0.5f) * 0.004f;
            float voiceSpreadPan = 0.5f + (unisonOffsets[v] + panMovement) * dynamicDetuneSpread * 0.5f;

            leftSum += osc * (1.0f - voiceSpreadPan);
            rightSum += osc * voiceSpreadPan;
        }

        float sSub = oscSub.processSample(0.0f);
        oscSub.setFrequency(baseFrequency * 0.5f * std::exp2(expressivePitchSemitones / 12.0f));

        float subAmt = (sourceDepth * sourceDepth * 0.74f) + (currentEnergy * 0.18f);

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

        // --- ANALOG WARMTH (Asymmetric Tube Saturation) ---
        // Instead of hard-clipping or symmetric tanh, we use an asymmetric curve
        // to generate pleasing even-order harmonics (warmth).
        auto tubeSaturate = [](float sample, float drive) {
            float x = sample * drive;
            // Asymmetric clipping:
            // Positives driven slightly differently than negatives
            float x2 = x * x;
            return (x + 0.18f * x2) / (1.0f + std::abs(x) + 0.18f * x2);
        };

        // First Stage: dynamic velocity "System Touch" Saturation
        float touchDrive = 0.85f + (level * level * 2.8f); // 0.85 to 3.65 based on strike force
        L = tubeSaturate(L, touchDrive);
        R = tubeSaturate(R, touchDrive);

        // 5. TEXTURE (Drive Stage)
        if (flowTexture > 0.02f)
        {
            float drive = (1.0f + (flowTexture * flowTexture * 9.5f)) * (1.1f + currentEnergy * 0.65f) * (0.6f + level * 0.8f);
            L = tubeSaturate(L, drive);
            R = tubeSaturate(R, drive);
        }

        // 6. FIELD COMPLEXITY (One-Pole High Shelf) - STABILIZED SHIMMER
        if (fieldComplexity > 0.05f)
        {
            float airGain = fieldComplexity * 0.65f;
            float dL = L - lastLeftFilterOut;
            float dR = R - lastRightFilterOut;
            L = L + std::tanh(dL * airGain);
            R = R + std::tanh(dR * airGain);
        }
        lastLeftFilterOut = L;
        lastRightFilterOut = R;

        float amMod = 1.0f - (std::abs(lfoVal) * pulseWidth * 0.18f);
        float gain = env * masterScale * amMod;

        L *= gain;
        R *= gain;

        // Final Safe Guard
        L = L / (1.0f + std::abs(L * 0.12f));
        R = R / (1.0f + std::abs(R * 0.12f));

        // 6. Stereo Master Panning (Global shift based on instability)
        // Normal state: preserves the wide stereophonic spread from unison.
        // Extreme jumps: only happen when Instability parameter is high.
        float extremePanShift = voicePan * currentInstability * currentInstability; // -1 to 1, strongly attenuated below 1.0

        // True stereo balance (preserves volume/width better than discarding a channel)
        float leftBalance = 1.0f - std::max(0.0f, extremePanShift);
        float rightBalance = 1.0f - std::max(0.0f, -extremePanShift);

        buffer.addSample(0, startSample + i, L * leftBalance);
        if (buffer.getNumChannels() > 1)
            buffer.addSample(1, startSample + i, R * rightBalance);

        if (!adsr.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

// --- Parameter Mappings ---
void RootFlowVoice::setFlow(float v)             { smoothedFlowRate.setTargetValue(v); }
void RootFlowVoice::setEnergy(float v)           { smoothedFlowEnergy.setTargetValue(v); }
void RootFlowVoice::setTexture(float v)          { smoothedFlowTexture.setTargetValue(v); }
void RootFlowVoice::setDepth(float v)            { smoothedSourceDepth.setTargetValue(v); }
void RootFlowVoice::setPulseFrequency(float v)   { smoothedPulseFrequency.setTargetValue(v); }
void RootFlowVoice::setPulseWidth(float v)       { smoothedPulseWidth.setTargetValue(v); }
void RootFlowVoice::setPulseEnergy(float v)      { smoothedPulseEnergy.setTargetValue(v); }
void RootFlowVoice::setFieldComplexity(float v)  { smoothedFieldComplexity.setTargetValue(v); }
void RootFlowVoice::setInstability(float v)      { smoothedInstability.setTargetValue(v); }

void RootFlowVoice::setWaveform(int typeIndex)
{
    for (int i = 0; i < maxUnison; ++i)
        unisonOscillators[i].setWaveform(typeIndex);
}

int RootFlowVoice::getActiveMidiChannel() const noexcept
{
    for (int channel = 1; channel <= 16; ++channel)
    {
        if (isPlayingChannel(channel))
            return channel;
    }

    return 1;
}

float RootFlowVoice::getPitchBendRangeForChannel(int midiChannel) const noexcept
{
    const auto index = (size_t) juce::jlimit(1, 16, midiChannel) - 1;
    if (midiExpressionState == nullptr)
        return 2.0f;

    return juce::jlimit(0.0f, 96.0f, midiExpressionState->pitchBendRangeSemitones[index]);
}

float RootFlowVoice::getModWheelValueForChannel(int midiChannel) const noexcept
{
    const auto index = (size_t) juce::jlimit(1, 16, midiChannel) - 1;
    if (midiExpressionState == nullptr)
        return 0.0f;

    return juce::jlimit(0.0f, 1.0f, midiExpressionState->modWheelNormalized[index]);
}

float RootFlowVoice::pitchWheelToSemitones(int midiChannel, int pitchWheelValue) const noexcept
{
    const float normalized = juce::jlimit(-1.0f, 1.0f, ((float) pitchWheelValue - 8192.0f) / 8192.0f);
    return normalized * getPitchBendRangeForChannel(midiChannel);
}
