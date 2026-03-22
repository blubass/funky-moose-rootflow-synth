#pragma once

#include <JuceHeader.h>
#include "RootFlowModulators.h"
#include "RootFlowSynthSound.h"
#include "RootFlowOscillators.h"

class RootFlowSynthVoice : public juce::SynthesiserVoice
{
public:
    struct MidiExpressionState
    {
        std::array<float, 16> pitchBendRangeSemitones {};
    };

    RootFlowSynthVoice() = default;

    void setMidiExpressionState(const MidiExpressionState* state) noexcept
    {
        midiExpressionState = state;
    }

    void setBioFeedbackSnapshot(const RootFlowBioFeedbackSnapshot& snapshot) noexcept
    {
        bioFeedback = snapshot;
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<RootFlowSynthSound*> (sound) != nullptr;
    }

    void startNote(int midiNoteNumber,
                   float velocity,
                   juce::SynthesiserSound*,
                   int currentPitchWheelPosition) override
    {
        noteVelocity = juce::jlimit(0.0f, 1.0f, velocity);
        level = 0.34f + std::sqrt(noteVelocity) * 0.34f;
        baseFrequency = (float) juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

        oscA.prepare(sr);
        oscB.prepare(sr);
        subOsc.prepare(sr);
        shimmerOsc.prepare(sr);
        atmosphereOsc.prepare(sr);
        atmosphereShimmerOsc.prepare(sr);
        pitchWander.prepare(sr);
        filterWander.prepare(sr);
        shapeWander.prepare(sr);

        pulsePhase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        driftPhase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        sapPhase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        panPhase = random.nextFloat() * juce::MathConstants<float>::twoPi;

        driftRate = 0.00008f + random.nextFloat() * 0.00022f;
        sapRate = 0.00018f + random.nextFloat() * 0.00035f;

        pulseValue = 0.0f;
        sapValue = 0.0f;
        currentMidiChannel = detectPlayingMidiChannel();
        notePressure = 0.0f;
        channelPressure = 0.0f;
        timbre = 0.5f;
        pitchBendSemitones = pitchWheelToSemitones(currentPitchWheelPosition, getPitchBendRangeSemitones());
        pitchBendSmoothed.setCurrentAndTargetValue(pitchBendSemitones);
        pressureSmoothed.setCurrentAndTargetValue(0.0f);
        timbreSmoothed.setCurrentAndTargetValue(timbre);
        instabilitySmoothed.setCurrentAndTargetValue(instability);
        organicFilter.reset();
        currentCutoff = cutoffSmoothed.getCurrentValue();
        atmosphereNoiseLowpass = 0.0f;
        atmosphereBodyLowpass = 0.0f;
        noteInstabilityBias = 0.88f + random.nextFloat() * 0.28f;

        updateEnvelopeParams();
        ampEnv.noteOn();
        filterEnv.noteOn();
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampEnv.noteOff();
            filterEnv.noteOff();
            return;
        }

        ampEnv.reset();
        filterEnv.reset();
        clearCurrentNote();
    }

    void pitchWheelMoved(int newPitchWheelValue) override
    {
        pitchBendSemitones = pitchWheelToSemitones(newPitchWheelValue, getPitchBendRangeSemitones());
        pitchBendSmoothed.setTargetValue(pitchBendSemitones);
    }

    void controllerMoved(int controllerNumber, int newControllerValue) override
    {
        if (controllerNumber == 74)
        {
            timbre = juce::jlimit(0.0f, 1.0f, (float) newControllerValue / 127.0f);
            timbreSmoothed.setTargetValue(timbre);
        }
    }

    void aftertouchChanged(int newAftertouchValue) override
    {
        notePressure = juce::jlimit(0.0f, 1.0f, (float) newAftertouchValue / 127.0f);
        updatePressureTarget();
    }

    void channelPressureChanged(int newChannelPressureValue) override
    {
        channelPressure = juce::jlimit(0.0f, 1.0f, (float) newChannelPressureValue / 127.0f);
        updatePressureTarget();
    }

    void prepare(double sampleRate, int samplesPerBlock)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;

        ampEnv.setSampleRate(sr);
        filterEnv.setSampleRate(sr);

        organicFilter.prepare(sr);

        cutoffSmoothed.reset(sr, 0.04);
        cutoffSmoothed.setCurrentAndTargetValue(1200.0f);
        currentCutoff = 1200.0f;
        pitchBendSmoothed.reset(sr, 0.012);
        pitchBendSmoothed.setCurrentAndTargetValue(0.0f);
        pressureSmoothed.reset(sr, 0.020);
        pressureSmoothed.setCurrentAndTargetValue(0.0f);
        timbreSmoothed.reset(sr, 0.028);
        timbreSmoothed.setCurrentAndTargetValue(0.5f);
        instabilitySmoothed.reset(sr, 0.060);
        instabilitySmoothed.setCurrentAndTargetValue(instability);

        oscA.prepare(sr);
        oscB.prepare(sr);
        subOsc.prepare(sr);
        shimmerOsc.prepare(sr);
        atmosphereOsc.prepare(sr);
        atmosphereShimmerOsc.prepare(sr);
        pitchWander.prepare(sr);
        filterWander.prepare(sr);
        shapeWander.prepare(sr);

        random.setSeedRandomly();
        updateEnvelopeParams();
    }

    void reset()
    {
        ampEnv.reset();
        filterEnv.reset();
        organicFilter.reset();

        float c = cutoffSmoothed.getTargetValue();
        cutoffSmoothed.setCurrentAndTargetValue(juce::jlimit(20.0f, 20000.0f, c));
        currentCutoff = c;
        
        pitchBendSmoothed.setCurrentAndTargetValue(pitchBendSmoothed.getTargetValue());
        pressureSmoothed.setCurrentAndTargetValue(0.0f);
        
        oscA.prepare(sr);
        oscB.prepare(sr);
        subOsc.prepare(sr);
        shimmerOsc.prepare(sr);
        atmosphereOsc.prepare(sr);
        atmosphereShimmerOsc.prepare(sr);
        
        clearCurrentNote();
    }

    void recoverFromSilentState() noexcept
    {
        if (! isVoiceActive())
            return;

        auto sanitizeWrappedPhase = [this](float phaseValue)
        {
            if (! std::isfinite(phaseValue))
                return random.nextFloat() * juce::MathConstants<float>::twoPi;

            while (phaseValue >= juce::MathConstants<float>::twoPi)
                phaseValue -= juce::MathConstants<float>::twoPi;

            while (phaseValue < 0.0f)
                phaseValue += juce::MathConstants<float>::twoPi;

            return phaseValue;
        };

        organicFilter.reset();
        oscA.prepare(sr);
        oscB.prepare(sr);
        subOsc.prepare(sr);
        shimmerOsc.prepare(sr);
        atmosphereOsc.prepare(sr);
        atmosphereShimmerOsc.prepare(sr);
        pitchWander.prepare(sr);
        filterWander.prepare(sr);
        shapeWander.prepare(sr);

        pulsePhase = sanitizeWrappedPhase(pulsePhase);
        driftPhase = sanitizeWrappedPhase(driftPhase);
        sapPhase = sanitizeWrappedPhase(sapPhase);
        panPhase = sanitizeWrappedPhase(panPhase);
        pulseValue = std::isfinite(pulseValue) ? juce::jlimit(-1.0f, 1.0f, pulseValue) : 0.0f;
        sapValue = std::isfinite(sapValue) ? juce::jlimit(-1.0f, 1.0f, sapValue) : 0.0f;
        atmosphereNoiseLowpass = 0.0f;
        atmosphereBodyLowpass = 0.0f;
        noteInstabilityBias = std::isfinite(noteInstabilityBias)
                            ? juce::jlimit(0.5f, 1.5f, noteInstabilityBias)
                            : 1.0f;

        pitchBendSemitones = std::isfinite(pitchBendSemitones) ? pitchBendSemitones : 0.0f;
        pitchBendSmoothed.setCurrentAndTargetValue(pitchBendSemitones);

        timbre = std::isfinite(timbre) ? juce::jlimit(0.0f, 1.0f, timbre) : 0.5f;
        timbreSmoothed.setCurrentAndTargetValue(timbre);

        instability = std::isfinite(instability) ? juce::jlimit(0.0f, 1.0f, instability) : 0.28f;
        instabilitySmoothed.setCurrentAndTargetValue(instability);

        const float safePressure = std::isfinite(pressureSmoothed.getTargetValue())
                                 ? juce::jlimit(0.0f, 1.0f, pressureSmoothed.getTargetValue())
                                 : 0.0f;
        pressureSmoothed.setCurrentAndTargetValue(safePressure);

        const float cutoffTarget = cutoffSmoothed.getTargetValue();
        const float safeCutoff = std::isfinite(cutoffTarget)
                               ? juce::jlimit(80.0f, (float) sr * 0.45f, cutoffTarget)
                               : 1200.0f;
        cutoffSmoothed.setCurrentAndTargetValue(safeCutoff);
        currentCutoff = safeCutoff;
    }

    void setParams(float newRootDepth,
                   float newRootSoil,
                   float newRootAnchor,
                   float newSapFlow,
                   float newSapVitality,
                   float newSapTexture,
                   float newPulseRate,
                   float newPulseBreath,
                   float newPulseGrowth,
                   float newCanopy,
                   float newAtmosphere,
                   float newInstability)
    {
        rootDepth = newRootDepth;
        rootSoil = newRootSoil;
        rootAnchor = newRootAnchor;

        sapFlow = newSapFlow;
        sapVitality = newSapVitality;
        sapTexture = newSapTexture;

        pulseRate = newPulseRate;
        pulseBreath = newPulseBreath;
        pulseGrowth = newPulseGrowth;

        canopy = newCanopy;
        atmosphere = newAtmosphere;
        instability = newInstability;
        instabilitySmoothed.setTargetValue(instability);

        // Reconfiguring the ADSR while a note is actively sounding can cause
        // voices to collapse mid-gesture. Keep envelope changes for the next note.
        if (! ampEnv.isActive())
            updateEnvelopeParams();

        const float baseCutoff = 180.0f + canopy * 5800.0f + atmosphere * 820.0f
                               + (1.0f - rootSoil) * 1800.0f
                               - rootDepth * 920.0f
                               - rootAnchor * 260.0f;
        cutoffSmoothed.setTargetValue(baseCutoff);
        currentCutoff = juce::jlimit(80.0f, (float) sr * 0.45f, currentCutoff);
    }

private:
    struct VoiceRenderBlockContext
    {
        float sampleRate = 44100.0f;
        float rateScale = 1.0f;
        float pulseSmoothCoeff = 0.0f;
        float sapSmoothCoeff = 0.0f;
        float depthT = 0.5f;
        float soilT = 0.5f;
        float anchorT = 0.5f;
        float flowT = 0.5f;
        float vitalT = 0.5f;
        float textureT = 0.5f;
        float rateT = 0.5f;
        float breathT = 0.5f;
        float growthT = 0.5f;
        float canopyT = 0.5f;
        float atmosphereT = 0.0f;
        float velocityT = 0.8f;
        float noteT = 0.5f;
        float stability = 0.5f;
        float bioEnergy = 0.0f;
        float bioGrowth = 0.0f;
        float bioJitter = 0.0f;
        float bioTension = 0.0f;
        float cutoffFollowCoeff = 0.0f;
    };

    struct VoiceModulationFrame
    {
        float pressureT = 0.0f;
        float timbreBipolar = 0.0f;
        float instabilityT = 0.0f;
        float pitchBendRatio = 1.0f;
        float sapWarp = 0.0f;
        float pulseAmp = 1.0f;
        float pulseFilter = 0.0f;
        float pulseNoise = 0.0f;
        float pitchWander = 0.0f;
        float filterWander = 0.0f;
        float shapeWander = 0.0f;
        float freqBase = 440.0f;
    };

    struct VoiceSourceStageFrame
    {
        float sample = 0.0f;
        float env = 0.0f;
        float atmosphereAirLift = 0.0f;
        float atmosphereStereoLayer = 0.0f;
    };

    struct VoiceToneStageFrame
    {
        float sample = 0.0f;
        float atmosphereStereoLayer = 0.0f;
    };

    struct VoiceSpatialStageFrame
    {
        float left = 0.0f;
        float right = 0.0f;
    };

public:
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample,
                         int numSamples) override
    {
        if (! isVoiceActive())
            return;

        auto* outL = outputBuffer.getWritePointer(0);
        auto* outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1) : nullptr;
        const auto context = makeRenderBlockContext();

        for (int i = 0; i < numSamples; ++i)
        {
            const auto modulation = advanceModulationFrame(context);
            const auto sourceStage = renderSourceStage(context, modulation);
            const auto toneStage = applyToneStage(context, modulation, sourceStage);
            const auto spatialStage = renderSpatialStage(context, modulation, toneStage);

            outL[startSample + i] += sanitizeSample(spatialStage.left, 1.2f);
            if (outR != nullptr)
                outR[startSample + i] += sanitizeSample(spatialStage.right, 1.2f);

            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    VoiceRenderBlockContext makeRenderBlockContext() const noexcept
    {
        VoiceRenderBlockContext context;
        context.sampleRate = (float) juce::jmax(1.0, sr);
        context.rateScale = 44100.0f / context.sampleRate;
        context.pulseSmoothCoeff = getSmoothingCoefficient(context.sampleRate, 8.0f);
        context.sapSmoothCoeff = getSmoothingCoefficient(context.sampleRate, 46.0f);
        context.depthT = std::pow(juce::jlimit(0.0f, 1.0f, rootDepth), 0.18f);
        context.soilT = std::pow(juce::jlimit(0.0f, 1.0f, rootSoil), 0.20f);
        context.anchorT = std::pow(juce::jlimit(0.0f, 1.0f, rootAnchor), 0.22f);
        context.flowT = std::pow(juce::jlimit(0.0f, 1.0f, sapFlow), 0.52f);
        context.vitalT = std::pow(juce::jlimit(0.0f, 1.0f, sapVitality), 0.56f);
        context.textureT = std::pow(juce::jlimit(0.0f, 1.0f, sapTexture), 0.54f);
        context.rateT = std::pow(juce::jlimit(0.0f, 1.0f, pulseRate), 0.05f);
        context.breathT = std::pow(juce::jlimit(0.0f, 1.0f, pulseBreath), 0.08f);
        context.growthT = std::pow(juce::jlimit(0.0f, 1.0f, pulseGrowth), 0.05f);
        context.canopyT = std::pow(juce::jlimit(0.0f, 1.0f, canopy), 0.90f);
        context.atmosphereT = std::pow(juce::jlimit(0.0f, 1.0f, atmosphere), 1.15f);
        context.velocityT = noteVelocity;
        context.noteT = juce::jlimit(0.0f, 1.0f,
                                     (std::log2(juce::jmax(27.5f, baseFrequency) / 55.0f) + 1.0f) / 5.0f);
        context.stability = 1.0f - context.anchorT;
        context.bioEnergy = juce::jlimit(0.0f, 1.0f, bioFeedback.plantEnergy);
        context.bioGrowth = juce::jlimit(0.0f, 1.0f, bioFeedback.growthCycle);
        context.bioJitter = juce::jlimit(0.0f, 1.0f, bioFeedback.leafJitter);
        context.bioTension = juce::jlimit(0.0f, 1.0f, bioFeedback.tension);
        context.cutoffFollowCoeff = juce::jlimit(0.0f, 1.0f, 0.05f * context.rateScale);
        return context;
    }

    VoiceModulationFrame advanceModulationFrame(const VoiceRenderBlockContext& context) noexcept
    {
        VoiceModulationFrame frame;
        frame.pressureT = pressureSmoothed.getNextValue();
        const float timbreT = timbreSmoothed.getNextValue();
        frame.timbreBipolar = std::copysign(std::pow(std::abs(timbreT * 2.0f - 1.0f), 0.85f), timbreT * 2.0f - 1.0f);
        frame.instabilityT = std::pow(juce::jlimit(0.0f, 1.0f, instabilitySmoothed.getNextValue() * noteInstabilityBias), 0.28f);
        frame.pitchBendRatio = std::exp2(pitchBendSmoothed.getNextValue() / 12.0f);
        const float instabilityCurve = std::pow(frame.instabilityT, 0.70f);
        const float driftDepthScale = 0.12f + instabilityCurve * 3.40f;
        const float driftRateScale = 0.70f + instabilityCurve * 2.60f;
        const float wanderRateScale = 0.90f + instabilityCurve * 2.90f;
        const float pitchWanderScale = 0.20f + instabilityCurve * 2.20f;
        const float filterWanderScale = 0.28f + instabilityCurve * 2.00f;
        const float shapeWanderScale = 0.24f + instabilityCurve * 1.90f;

        const float driftDepth = (0.0022f + 0.0058f * context.flowT + context.bioGrowth * 0.0020f + context.bioJitter * 0.0014f)
                               * driftDepthScale;
        float drift = std::sin(driftPhase) * driftDepth;
        drift *= context.stability * context.stability * (0.85f + context.bioTension * 0.25f);
        driftPhase += driftRate * context.rateScale * driftRateScale * (1.0f + context.bioJitter * 0.35f);
        wrapPhase(driftPhase);

        const float rawSapMotion = std::sin(sapPhase);
        frame.sapWarp = std::tanh(rawSapMotion * (0.5f + context.vitalT));
        const float sapMotion = frame.sapWarp * (0.05f + 0.28f * context.vitalT + context.flowT * 0.10f + context.bioGrowth * 0.10f)
                              * (0.4f + 0.6f * context.stability)
                              * (0.92f + context.bioEnergy * 0.30f);
        sapPhase += (sapRate + context.flowT * 0.0014f + context.bioJitter * 0.00045f) * context.rateScale;
        wrapPhase(sapPhase);
        sapValue += context.sapSmoothCoeff * (sapMotion - sapValue);

        const float pulseSpeed = (0.00005f + context.rateT * 0.0100f + context.bioGrowth * 0.00120f) * context.rateScale;
        pulsePhase += pulseSpeed;
        wrapPhase(pulsePhase);
        const float rawPulse = std::sin(pulsePhase);
        const float pulseAccent = std::sin(pulsePhase * 2.0f + 0.65f);
        const float shapedPulse = juce::jlimit(-1.0f, 1.0f,
                                               std::copysign(std::pow(std::abs(rawPulse), 0.32f), rawPulse) * 0.82f
                                               + pulseAccent * 0.18f);
        pulseValue += context.pulseSmoothCoeff * (shapedPulse - pulseValue);
        frame.pulseAmp = juce::jlimit(0.12f, 3.40f,
                                      1.0f + pulseValue * (0.26f + 1.45f * context.breathT + context.bioEnergy * 0.46f + context.growthT * 0.78f));
        frame.pulseFilter = juce::jlimit(-6200.0f, 6200.0f,
                                         pulseValue * (480.0f + 3600.0f * context.breathT + context.bioGrowth * 1400.0f + context.growthT * 2400.0f));
        frame.pulseNoise = juce::jlimit(-1.15f, 1.15f,
                                        pulseValue * (0.62f * context.growthT + context.bioJitter * 0.32f));

        frame.pitchWander = (pitchWander.getNextValue((0.10f + context.flowT * 0.18f + context.bioTension * 0.08f) * wanderRateScale) * 2.0f - 1.0f)
                          * pitchWanderScale;
        frame.filterWander = (filterWander.getNextValue((0.16f + context.vitalT * 0.28f + context.bioGrowth * 0.10f) * wanderRateScale) * 2.0f - 1.0f)
                           * filterWanderScale;
        frame.shapeWander = (shapeWander.getNextValue((0.06f + context.growthT * 0.16f + context.bioJitter * 0.08f) * wanderRateScale) * 2.0f - 1.0f)
                          * shapeWanderScale;

        frame.freqBase = sanitizeSample(baseFrequency * frame.pitchBendRatio
                                      * (1.0f + drift + frame.pitchWander * (0.0018f + context.vitalT * 0.0060f + context.bioTension * 0.0028f)),
                                        context.sampleRate * 0.45f);
        frame.freqBase = juce::jlimit(20.0f, context.sampleRate * 0.45f, frame.freqBase);
        return frame;
    }

    void renderAtmosphereStage(const VoiceRenderBlockContext& context,
                               const VoiceModulationFrame& modulation,
                               float env,
                               float& sample,
                               float& atmosphereAirLift,
                               float& atmosphereStereoLayer) noexcept
    {
        if (context.atmosphereT <= 0.0001f)
            return;

        const float tunedAtmosFrequency = baseFrequency * modulation.pitchBendRatio;
        const float atmosphereBodyT = std::pow(context.atmosphereT, 0.85f);
        const float atmosphereNoiseT = std::pow(context.atmosphereT, 1.55f);
        const float atmosphereCharacter = juce::jlimit(0.0f, 1.0f,
                                                       0.04f
                                                       + context.textureT * 0.18f
                                                       + context.canopyT * 0.05f
                                                       + atmosphereNoiseT * 0.10f
                                                       + context.bioGrowth * 0.08f
                                                       + modulation.pressureT * 0.05f);
        const float atmosphereNoiseSample = atmosphereNoise.getNextSample(atmosphereCharacter);
        atmosphereNoiseLowpass += (atmosphereNoiseSample - atmosphereNoiseLowpass)
                                * juce::jlimit(0.001f, 0.45f,
                                               0.006f + atmosphereNoiseT * 0.022f + context.canopyT * 0.008f);
        const float airyNoise = atmosphereNoiseSample
                              - atmosphereNoiseLowpass * (0.92f - context.soilT * 0.10f);
        const float silkNoise = atmosphereNoiseLowpass * (0.92f + context.canopyT * 0.10f)
                              + airyNoise * (0.18f + context.textureT * 0.08f);
        const float windMotion = 0.55f + 0.45f * std::sin(panPhase * 0.38f + sapPhase * 0.72f + driftPhase * 0.44f);
        const float windNoise = silkNoise * (0.74f + windMotion * 0.30f)
                              + airyNoise * (0.06f + context.bioJitter * 0.04f);

        const float mistShape = juce::jlimit(0.0f, 1.0f,
                                             0.12f
                                             + context.textureT * 0.34f
                                             + context.canopyT * 0.10f
                                             + juce::jmax(0.0f, modulation.timbreBipolar) * 0.14f
                                             + context.bioJitter * 0.06f);
        const float mistGrowth = juce::jlimit(0.0f, 1.0f,
                                              atmosphereBodyT * 0.54f
                                              + context.bioEnergy * 0.20f
                                              + modulation.pressureT * 0.12f
                                              + context.growthT * 0.10f);

        const float mistBodyFreq = juce::jlimit(55.0f,
                                                880.0f,
                                                tunedAtmosFrequency * 0.5f);
        const float mistBodyShape = juce::jlimit(0.0f, 1.0f,
                                                 0.02f + context.textureT * 0.08f + context.canopyT * 0.03f);
        float mistBody = atmosphereOsc.getNextSample(mistBodyFreq,
                                                     mistBodyShape,
                                                     mistGrowth * 0.12f);
        atmosphereBodyLowpass += (mistBody - atmosphereBodyLowpass)
                               * juce::jlimit(0.001f, 0.45f,
                                              0.028f + atmosphereBodyT * 0.120f + context.canopyT * 0.014f);
        mistBody = atmosphereBodyLowpass;

        const float mistLiftFreq = juce::jlimit(110.0f,
                                                context.sampleRate * 0.22f,
                                                tunedAtmosFrequency * 2.0f);
        const float mistLiftShape = juce::jlimit(0.0f, 1.0f,
                                                 0.06f + mistShape * 0.12f + context.canopyT * 0.03f);
        const float mistLift = atmosphereShimmerOsc.getNextSample(mistLiftFreq,
                                                                  mistLiftShape,
                                                                  juce::jlimit(0.0f, 1.0f,
                                                                               mistGrowth * 0.14f + context.canopyT * 0.020f));

        float atmosphereLayer = windNoise * (0.12f + atmosphereNoiseT * (0.46f + context.textureT * 0.12f))
                              + mistBody * (0.20f + atmosphereBodyT * 0.16f + context.bioEnergy * 0.06f)
                              + mistLift * (0.10f + atmosphereBodyT * 0.08f + context.canopyT * 0.05f + juce::jmax(0.0f, modulation.timbreBipolar) * 0.02f);
        atmosphereLayer *= atmosphereBodyT
                         * (0.12f + context.canopyT * 0.05f + context.bioEnergy * 0.05f)
                         * (0.58f + env * 0.42f)
                         * (1.00f + modulation.pressureT * 0.28f + context.bioGrowth * 0.20f);
        atmosphereLayer *= 1.0f + pulseValue * (0.10f + context.breathT * 0.10f) + sapValue * 0.32f;
        sample += atmosphereLayer * (0.42f + atmosphereBodyT * 0.10f + context.canopyT * 0.05f);

        atmosphereAirLift = (windNoise * (0.10f + atmosphereNoiseT * (0.24f + context.textureT * 0.08f))
                           + mistLift * (0.12f + atmosphereBodyT * 0.08f + context.canopyT * 0.04f + juce::jmax(0.0f, modulation.timbreBipolar) * 0.02f))
                           * atmosphereBodyT
                           * (0.05f + context.canopyT * 0.022f + context.bioEnergy * 0.016f)
                           * (0.30f + env * 0.70f);

        atmosphereStereoLayer = (windNoise * (0.12f + atmosphereNoiseT * (0.40f + context.textureT * 0.10f))
                               + mistBody * (0.14f + atmosphereBodyT * 0.10f + context.bioGrowth * 0.04f)
                               + mistLift * (0.12f + atmosphereBodyT * 0.08f + context.canopyT * 0.04f + juce::jmax(0.0f, modulation.timbreBipolar) * 0.02f))
                               * atmosphereBodyT
                               * (0.11f + context.canopyT * 0.06f + context.bioEnergy * 0.05f)
                               * (0.40f + env * 0.60f)
                               * (1.0f + modulation.pressureT * 0.22f + context.bioJitter * 0.16f);
    }

    VoiceSourceStageFrame renderSourceStage(const VoiceRenderBlockContext& context,
                                            const VoiceModulationFrame& modulation) noexcept
    {
        VoiceSourceStageFrame frame;
        const float textureCurve = std::pow(context.textureT, 0.86f);
        const float growthCurve = std::pow(context.growthT, 0.82f);
        const float sapLift = juce::jmax(0.0f, sapValue);
        const float sapShadow = juce::jmax(0.0f, -sapValue);
        const float detune = 1.0f
                           + (0.0024f + 0.020f * std::pow(context.flowT, 1.02f))
                           + modulation.instabilityT * (0.0024f + context.flowT * 0.0060f)
                           + sapValue * 0.012f;

        const float shape = juce::jlimit(0.0f, 1.0f,
                                         0.05f
                                         + textureCurve * 0.88f
                                         + modulation.timbreBipolar * 0.24f
                                         + modulation.shapeWander * 0.10f
                                         + sapLift * (0.10f + context.flowT * 0.12f)
                                         + context.bioGrowth * 0.14f
                                         + modulation.instabilityT * 0.08f
                                         - sapShadow * 0.04f
                                         - context.bioTension * 0.04f);
        const float growth = juce::jlimit(0.0f, 1.0f,
                                          growthCurve * 0.68f
                                          + modulation.pressureT * 0.28f
                                          + juce::jmax(0.0f, modulation.timbreBipolar) * 0.10f
                                          + modulation.shapeWander * 0.08f
                                          + sapLift * (0.12f + context.vitalT * 0.10f)
                                          + std::abs(sapValue) * 0.04f
                                          + context.bioGrowth * 0.28f
                                          + context.bioEnergy * 0.16f);

        const float sA = oscA.getNextSample(modulation.freqBase, shape, growth);
        const float sB = oscB.getNextSample(modulation.freqBase * detune, shape * 0.8f, growth * 0.9f)
                       * (1.0f + modulation.sapWarp * 0.10f) * (1.0f + sapValue * 0.28f);
        const float sSub = subOsc.getNextSample(modulation.freqBase * 0.5f, 0.0f, growth * 0.2f);
        const float sShim = shimmerOsc.getNextSample(modulation.freqBase * 2.0f, shape * 0.4f, growth * 1.2f);

        const float harmonicMix = 0.02f + 0.40f * std::pow(context.vitalT, 0.78f) + context.noteT * 0.06f
                                + (1.0f - context.depthT) * 0.46f + (1.0f - context.soilT) * 0.14f
                                + sapLift * (0.12f + context.flowT * 0.08f);
        const float bodyMix = 0.38f + 1.20f * context.depthT + context.soilT * 0.46f + context.anchorT * 0.12f
                            + sapShadow * 0.10f - context.noteT * 0.08f;
        const float subMix = (0.06f + 1.34f * context.depthT + context.soilT * 0.62f + (1.0f - context.anchorT) * 0.10f
                            + sapShadow * 0.08f)
                           * (1.0f - context.noteT * 0.82f);
        const float shimmerMix = 0.012f + context.canopyT * 0.032f + context.vitalT * 0.024f + context.noteT * 0.024f
                               + juce::jmax(0.0f, modulation.timbreBipolar) * 0.05f
                               + sapLift * 0.06f + std::abs(sapValue) * 0.02f;

        const float weightedBody = sA * bodyMix;
        const float weightedHarmonics = sB * (harmonicMix + modulation.pressureT * 0.08f + sapLift * 0.06f);
        const float weightedSub = sSub * subMix;
        const float weightedShimmer = sShim * shimmerMix;
        const float mixCompensation = 1.0f / (0.70f
                                              + bodyMix * 0.32f
                                              + harmonicMix * 0.38f
                                              + subMix * 0.40f
                                              + shimmerMix * 0.24f);

        frame.sample = sanitizeSample((weightedBody + weightedHarmonics + weightedSub + weightedShimmer)
                                      * mixCompensation
                                      * (1.02f + context.depthT * 0.22f + context.soilT * 0.10f + context.growthT * 0.06f), 1.5f);

        frame.env = ampEnv.getNextSample();
        renderAtmosphereStage(context,
                              modulation,
                              frame.env,
                              frame.sample,
                              frame.atmosphereAirLift,
                              frame.atmosphereStereoLayer);

        const float noiseVitality = juce::jlimit(0.0f, 1.0f, context.vitalT * 0.72f + context.bioJitter * 0.28f);
        float plantNoise = organicNoise.getNextSample(noiseVitality);
        plantNoise *= 0.0008f + 0.0068f * textureCurve + 0.0025f * context.vitalT + context.bioJitter * 0.0026f;
        plantNoise *= 0.46f + frame.env * 0.34f + context.flowT * 0.08f + sapLift * 0.12f;
        plantNoise *= 1.0f + modulation.pulseNoise * 0.38f + modulation.pressureT * 0.16f
                    + context.bioTension * 0.08f + modulation.instabilityT * 0.14f;
        frame.sample = sanitizeSample(frame.sample + plantNoise, 1.4f);
        return frame;
    }

    VoiceToneStageFrame applyToneStage(const VoiceRenderBlockContext& context,
                                       const VoiceModulationFrame& modulation,
                                       const VoiceSourceStageFrame& sourceStage) noexcept
    {
        VoiceToneStageFrame frame;
        frame.sample = sanitizeSample(sourceStage.sample, 1.4f);
        frame.sample = sanitizeSample(frame.sample * modulation.pulseAmp * (1.0f + modulation.pressureT * 0.12f + context.rateT * 0.16f), 1.6f);

        const float driveAmount = 0.02f + context.depthT * 0.64f + context.soilT * 0.26f + context.vitalT * 0.10f + context.velocityT * 0.04f
                                + modulation.pressureT * 0.08f + context.bioTension * 0.10f + context.bioEnergy * 0.06f + context.growthT * 0.08f;
        const float drive = 1.0f + driveAmount * 0.42f;
        const float preDrivenSample = softSat(frame.sample * drive) / drive;
        frame.sample = sanitizeSample(juce::jmap(juce::jlimit(0.0f, 0.18f, driveAmount * 1.7f),
                                                 frame.sample,
                                                 preDrivenSample), 1.3f);

        const float res = juce::jlimit(0.18f, 0.90f,
                                       0.44f + (1.0f - context.anchorT) * 0.54f + context.depthT * 0.22f + context.canopyT * 0.04f + context.bioTension * 0.08f);
        const float filterDrive = juce::jlimit(0.0f, 0.14f,
                                               0.004f + context.depthT * 0.200f + context.soilT * 0.080f + context.vitalT * 0.035f
                                             + modulation.pressureT * 0.030f + context.bioEnergy * 0.030f);
        const float timbreLift = juce::jmax(0.0f, modulation.timbreBipolar);
        const float timbreShade = juce::jmax(0.0f, -modulation.timbreBipolar);

        const float filterEnvValue = filterEnv.getNextSample();
        const float targetCutoff = cutoffSmoothed.getNextValue()
                                 + filterEnvValue * (300.0f + 2400.0f * context.growthT)
                                 + modulation.filterWander * (80.0f + context.canopyT * 150.0f)
                                 + modulation.pulseFilter
                                 + sapValue * (120.0f + context.flowT * 360.0f + context.vitalT * 220.0f)
                                 + context.bioGrowth * (120.0f + context.canopyT * 420.0f)
                                 - context.depthT * (560.0f + context.soilT * 2200.0f)
                                 - context.soilT * 980.0f
                                 - context.bioTension * (50.0f + context.depthT * 180.0f)
                                 + timbreLift * (260.0f + context.canopyT * 700.0f)
                                 - timbreShade * (120.0f + context.depthT * 240.0f)
                                 + modulation.pressureT * (160.0f + context.growthT * 380.0f)
                                 + context.noteT * (220.0f + context.canopyT * 420.0f)
                                 + context.velocityT * (180.0f + context.growthT * 320.0f);
        const float musicalCutoffFloor = juce::jlimit(110.0f,
                                                      context.sampleRate * 0.30f,
                                                      modulation.freqBase * (0.58f + context.canopyT * 0.18f)
                                                      + 110.0f
                                                      + context.bioEnergy * 90.0f
                                                      + context.velocityT * 60.0f);
        currentCutoff += context.cutoffFollowCoeff * (targetCutoff - currentCutoff);
        currentCutoff = juce::jlimit(musicalCutoffFloor,
                                     context.sampleRate * 0.45f,
                                     sanitizeSample(currentCutoff, context.sampleRate * 0.45f));

        frame.sample = sanitizeSample(organicFilter.processSample(frame.sample,
                                                                  juce::jlimit(musicalCutoffFloor, context.sampleRate * 0.45f, currentCutoff),
                                                                  res,
                                                                  filterDrive), 1.3f);

        frame.sample = sanitizeSample(frame.sample + sourceStage.atmosphereAirLift, 1.35f);
        const float postDrive = 0.008f + context.canopyT * 0.008f + modulation.pressureT * 0.010f
                              + context.bioGrowth * 0.010f + context.growthT * 0.014f + context.breathT * 0.010f;
        const float postDrivenSample = softSat(frame.sample * (1.0f + postDrive)) / (1.0f + postDrive);
        frame.sample = sanitizeSample(juce::jmap(juce::jlimit(0.0f, 0.08f, postDrive * 3.0f),
                                                 frame.sample,
                                                 postDrivenSample), 1.25f);

        const float canopyHeadroom = 1.0f - context.canopyT * 0.10f;
        const float voiceGain = sourceStage.env * level
                              * (0.58f + context.depthT * 0.24f + context.canopyT * 0.04f + context.growthT * 0.08f)
                              * (0.98f + modulation.pressureT * 0.18f);
        frame.sample = sanitizeSample(frame.sample * voiceGain * canopyHeadroom, 1.2f);
        frame.atmosphereStereoLayer = sourceStage.atmosphereStereoLayer
                                    * level
                                    * (0.50f + context.canopyT * 0.10f + context.atmosphereT * 0.14f)
                                    * canopyHeadroom;
        frame.atmosphereStereoLayer = sanitizeSample(frame.atmosphereStereoLayer, 0.9f);
        return frame;
    }

    VoiceSpatialStageFrame renderSpatialStage(const VoiceRenderBlockContext& context,
                                              const VoiceModulationFrame& modulation,
                                              const VoiceToneStageFrame& toneStage) noexcept
    {
        VoiceSpatialStageFrame frame;

        panPhase += (0.0003f + context.flowT * 0.0005f) * context.rateScale;
        wrapPhase(panPhase);
        const float swayDepth = (0.04f + 0.10f * context.flowT + context.bioJitter * 0.05f
                               + context.breathT * 0.08f + context.growthT * 0.06f) * (1.0f - context.anchorT * 0.5f);
        float sway = (std::sin(panPhase) * swayDepth
                    + pulseValue * (0.03f + context.breathT * 0.18f + context.growthT * 0.10f)
                    + sapValue * (0.02f + context.flowT * 0.05f + context.vitalT * 0.03f))
                   * (0.72f + 0.28f * (0.5f + 0.5f * pulseValue));
        sway *= context.stability;
        const float leftGain = juce::jlimit(0.82f, 1.18f, 1.0f - sway);
        const float rightGain = juce::jlimit(0.82f, 1.18f, 1.0f + sway);

        const float atmosphereSpread = std::sin(panPhase * 0.57f + driftPhase * 0.41f + sapPhase * 0.25f);
        const float atmosphereWidth = juce::jlimit(0.0f, 0.94f,
                                                   0.24f + context.atmosphereT * 0.54f + context.canopyT * 0.14f + context.bioJitter * 0.08f);
        const float atmosphereLeft = toneStage.atmosphereStereoLayer * (1.0f - atmosphereSpread * atmosphereWidth);
        const float atmosphereRight = toneStage.atmosphereStereoLayer * (1.0f + atmosphereSpread * atmosphereWidth);

        frame.left = sanitizeSample(toneStage.sample * leftGain + atmosphereLeft, 1.1f);
        frame.right = sanitizeSample(toneStage.sample * rightGain + atmosphereRight, 1.1f);
        juce::ignoreUnused(modulation);
        return frame;
    }

    int detectPlayingMidiChannel() const noexcept
    {
        for (int channel = 1; channel <= 16; ++channel)
            if (isPlayingChannel(channel))
                return channel;

        return 1;
    }

    float getPitchBendRangeSemitones() const noexcept
    {
        if (midiExpressionState != nullptr && currentMidiChannel >= 1 && currentMidiChannel <= 16)
            return juce::jmax(0.0f, midiExpressionState->pitchBendRangeSemitones[(size_t) currentMidiChannel - 1]);

        return 2.0f;
    }

    void updatePressureTarget()
    {
        const float rawPressure = juce::jmax(notePressure, channelPressure);
        pressureSmoothed.setTargetValue(std::pow(juce::jlimit(0.0f, 1.0f, rawPressure), 0.82f));
    }

    void updateEnvelopeParams()
    {
        const float anchorT = juce::jlimit(0.0f, 1.0f, rootAnchor);
        const float soilT = juce::jlimit(0.0f, 1.0f, rootSoil);
        const float pulseT = juce::jlimit(0.0f, 1.0f, pulseBreath);

        ampEnvParams.attack = 0.002f + 0.55f * anchorT;
        ampEnvParams.decay = 0.03f + 0.82f * soilT;
        ampEnvParams.sustain = 0.46f + 0.46f * (1.0f - soilT * 0.48f);
        ampEnvParams.release = 0.08f + 2.10f * anchorT + 1.05f * pulseT;
        ampEnv.setParameters(ampEnvParams);

        filterEnvParams.attack = 0.002f + 0.28f * pulseT;
        filterEnvParams.decay = 0.04f + 0.70f * soilT;
        filterEnvParams.sustain = 0.28f;
        filterEnvParams.release = 0.08f + 1.10f * anchorT + 0.36f * pulseT;
        filterEnv.setParameters(filterEnvParams);
    }

    static void wrapPhase(float& phaseValue)
    {
        while (phaseValue >= juce::MathConstants<float>::twoPi)
            phaseValue -= juce::MathConstants<float>::twoPi;
    }

    static float softSat(float x)
    {
        return std::tanh(x);
    }

    static float sanitizeSample(float x, float limit = 1.5f) noexcept
    {
        if (! std::isfinite(x))
            return 0.0f;

        return juce::jlimit(-limit, limit, x);
    }

    static float pitchWheelToSemitones(int pitchWheelValue, float bendRangeSemitones) noexcept
    {
        const float normalized = juce::jlimit(-1.0f, 1.0f, ((float) pitchWheelValue - 8192.0f) / 8192.0f);
        return normalized * bendRangeSemitones;
    }

    static float getSmoothingCoefficient(float sampleRate, float timeMs)
    {
        const float timeSeconds = juce::jmax(0.001f, timeMs) * 0.001f;
        return 1.0f - std::exp(-1.0f / (juce::jmax(1.0f, sampleRate) * timeSeconds));
    }

    double sr = 44100.0;

    RootFlow::OrganicOscillator oscA, oscB, subOsc, shimmerOsc, atmosphereOsc, atmosphereShimmerOsc;
    RootFlow::OrganicNoise organicNoise, atmosphereNoise;
    RootFlow::WanderingOrganicModulation pitchWander, filterWander, shapeWander;

    float pulsePhase = 0.0f;
    float driftPhase = 0.0f;
    float driftRate = 0.00015f;
    float sapPhase = 0.0f;
    float sapRate = 0.0003f;
    float pulseValue = 0.0f;
    float sapValue = 0.0f;
    float panPhase = 0.0f;
    float atmosphereNoiseLowpass = 0.0f;
    float atmosphereBodyLowpass = 0.0f;

    float baseFrequency = 440.0f;
    float level = 0.0f;
    float noteVelocity = 0.8f;
    float currentCutoff = 1200.0f;
    int currentMidiChannel = 1;
    float pitchBendSemitones = 0.0f;
    float notePressure = 0.0f;
    float channelPressure = 0.0f;
    float timbre = 0.5f;
    juce::SmoothedValue<float> pitchBendSmoothed;
    juce::SmoothedValue<float> pressureSmoothed;
    juce::SmoothedValue<float> timbreSmoothed;
    juce::SmoothedValue<float> instabilitySmoothed;
    const MidiExpressionState* midiExpressionState = nullptr;
    RootFlowBioFeedbackSnapshot bioFeedback;

    RootFlow::OrganicLadderFilter organicFilter;
    juce::SmoothedValue<float> cutoffSmoothed;

    juce::ADSR ampEnv;
    juce::ADSR filterEnv;
    juce::ADSR::Parameters ampEnvParams;
    juce::ADSR::Parameters filterEnvParams;

    juce::Random random;

    float rootDepth = 0.5f;
    float rootSoil = 0.5f;
    float rootAnchor = 0.5f;

    float sapFlow = 0.5f;
    float sapVitality = 0.5f;
    float sapTexture = 0.5f;

    float pulseRate = 0.5f;
    float pulseBreath = 0.5f;
    float pulseGrowth = 0.5f;

    float canopy = 0.5f;
    float atmosphere = 0.18f;
    float instability = 0.28f;
    float noteInstabilityBias = 1.0f;
};
