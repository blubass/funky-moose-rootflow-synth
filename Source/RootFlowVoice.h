#include <JuceHeader.h>
#include <array>
#include "RootFlowOscillators.h"
#include "RootFlowDSP.h"

class RootFlowModulationEngine;

/**
 * Basic sound class for RootFlow. 
 * Allows the synthesiser to identify which voices can play which sounds.
 */
class RootFlowSound : public juce::SynthesiserSound
{
public:
    RootFlowSound() : juce::SynthesiserSound() {}
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

/**
 * RootFlowVoice - Modern Polyphonic Synth Voice.
 * Implements the core synthesis logic with FLOW, VITALITY, TEXTURE, and DEPTH.
 */
class RootFlowVoice : public juce::SynthesiserVoice
{
public:
    RootFlowVoice();

    bool canPlaySound(juce::SynthesiserSound* sound) override;

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int) override;

    void stopNote(float velocity, bool allowTailOff) override;

    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void aftertouchChanged(int newAftertouchValue) override;
    void channelPressureChanged(int newChannelPressureValue) override;

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, 
                         int startSample, int numSamples) override;

    void setSampleRate(double sr, int blockSize);
    void reset();
    void recoverFromSilentState();

    // --- Parameter Mapping ---
    void setFlow(float v);          // Filter Cutoff (Flow Rate)
    void setEnergy(float v);        // Resonance / Drive (Flow Energy)
    void setTexture(float v);       // Waveshaping / Char (Flow Texture)
    void setDepth(float v);         // Detune / Sub (Source Depth)
    void setPulseFrequency(float v);
    void setPulseWidth(float v);
    void setPulseEnergy(float v);
    void setFieldComplexity(float v);
    void setInstability(float v);
    void setUnison(int numVoices) { unisonVoices = juce::jlimit(1, 8, numVoices); }
    void setWaveform(int typeIndex);
    void setMidiExpressionState(const RootFlowDSP::MidiExpressionState* state) { midiExpressionState = state; }
    void setEngine(const RootFlowModulationEngine* e) { engine = e; }

private:
    int getActiveMidiChannel() const noexcept;
    float getPitchBendRangeForChannel(int midiChannel) const noexcept;
    float getModWheelValueForChannel(int midiChannel) const noexcept;
    float pitchWheelToSemitones(int midiChannel, int pitchWheelValue) const noexcept;

    double sampleRate = 44100.0;

    // ROOT (Unison Bank)
    static constexpr int maxUnison = 8;
    int unisonVoices = 4;
    float unisonDetune = 0.012f;
    float stereoSpread = 0.5f;

    RootFlowOscillator unisonOscillators[maxUnison];
    float unisonOffsets[maxUnison]{ 0 };

    juce::dsp::Oscillator<float> oscSub;
    juce::dsp::Oscillator<float> lfo;      // PULSE LFO
    juce::dsp::Oscillator<float> driftLfo; // Slow Analog Drift
    juce::dsp::Oscillator<float> vibratoLfo;
    
    // NOISE (Spectral Core Texture)
    juce::Random noiseGen;
    juce::dsp::IIR::Filter<float> noiseFilter;

    // SAP (Filter)
    juce::dsp::StateVariableTPTFilter<float> filterL;
    juce::dsp::StateVariableTPTFilter<float> filterR;

    // AMP (Envelope)
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    float level = 0.0f;
    float baseFrequency = 440.0f;
    float voicePan = 0.0f;
    float voiceDetuneRatio = 0.0f;
    int lastPitchWheelValue = 8192;
    const RootFlowDSP::MidiExpressionState* midiExpressionState = nullptr;
    juce::LinearSmoothedValue<float> smoothedPitchBendSemitones { 0.0f };
    juce::LinearSmoothedValue<float> smoothedModWheel { 0.0f };

    // --- Smoothed Parameters ---
    juce::LinearSmoothedValue<float> smoothedFlowRate { 0.5f };
    juce::LinearSmoothedValue<float> smoothedFlowEnergy { 0.5f };
    juce::LinearSmoothedValue<float> smoothedFlowTexture { 0.5f };
    juce::LinearSmoothedValue<float> smoothedSourceDepth { 0.5f };
    juce::LinearSmoothedValue<float> smoothedPulseFrequency { 0.5f };
    juce::LinearSmoothedValue<float> smoothedPulseWidth { 0.5f };
    juce::LinearSmoothedValue<float> smoothedPulseEnergy { 0.5f };
    juce::LinearSmoothedValue<float> smoothedFieldComplexity { 0.5f };
    juce::LinearSmoothedValue<float> smoothedInstability { 0.0f };
    
    // System-Feedback Energy & Filter
    const RootFlowModulationEngine* engine = nullptr;
    juce::LinearSmoothedValue<float> smoothedEnergy { 0.05f };
    float lastLeftFilterOut = 0.0f;
    float lastRightFilterOut = 0.0f;

};
