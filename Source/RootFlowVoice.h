#include <JuceHeader.h>
#include <array>
#include "RootFlowOscillators.h"

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
    struct MidiExpressionState
    {
        std::array<float, 16> pitchBendRangeSemitones {};
    };

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
    void setFlow(float v);    // Filter Cutoff
    void setVitality(float v); // Resonance / Drive
    void setTexture(float v);  // Waveshaping / Char
    void setDepth(float v);    // Detune / Sub
    void setPulseRate(float v);
    void setPulseAmount(float v);
    void setGrowth(float v);
    void setCanopy(float v);
    void setWaveform(int typeIndex);
    
    void setEngine(const RootFlowModulationEngine* e) { engine = e; }

private:
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
    
    // NOISE (Organic Roots Texture)
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

    // --- Smoothed Parameters ---
    juce::LinearSmoothedValue<float> smoothedFlow { 0.5f };
    juce::LinearSmoothedValue<float> smoothedVitality { 0.5f };
    juce::LinearSmoothedValue<float> smoothedTexture { 0.5f };
    juce::LinearSmoothedValue<float> smoothedDepth { 0.5f };
    juce::LinearSmoothedValue<float> smoothedPulseRate { 0.5f };
    juce::LinearSmoothedValue<float> smoothedPulseAmount { 0.5f };
    juce::LinearSmoothedValue<float> smoothedGrowth { 0.5f };
    juce::LinearSmoothedValue<float> smoothedCanopy { 0.5f };
    
    // Bio-Feedback Energy & Filter
    const RootFlowModulationEngine* engine = nullptr;
    juce::LinearSmoothedValue<float> smoothedEnergy { 0.05f };
    float lastLeftFilterOut = 0.0f;
    float lastRightFilterOut = 0.0f;

};
