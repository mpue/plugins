/*
  ==============================================================================
    ST-1  -  Luxury Saturation
    PluginProcessor.h
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
// SaturationEngine - static math + per-instance helpers.
//==============================================================================
class SaturationEngine
{
public:
    enum Mode
    {
        Tube = 0,
        Tape,
        Transistor,
        Diode,
        Foldback,
        Soft,
        Hard,
        NumModes
    };

    static juce::StringArray getModeNames()
    {
        return { "Tube", "Tape", "Transistor", "Diode", "Foldback", "Soft", "Hard" };
    }

    // The pure waveshaping function.  x is expected in roughly [-1.5 .. 1.5].
    // Returns the shaped sample. bias is the asymmetry term applied pre-shape.
    static inline float shape (float x, int mode, float bias) noexcept
    {
        const float xi = x + bias;
        switch (mode)
        {
            case Tube:
            {
                // Soft knee tanh with subtle even-harmonic colouring.
                const float t = std::tanh (xi * 1.2f);
                return 0.92f * t + 0.08f * std::sin (xi * 1.4f);
            }
            case Tape:
            {
                // Compressive soft saturation, normalised so |y|<1.
                const float k = 0.8509181f; // 1 / tanh(1) ≈ 1.313, scaled
                return std::tanh (xi * 1.0f) * k;
            }
            case Transistor:
            {
                // Cubic soft clip with hard knees beyond ±1.5.
                const float a = juce::jlimit (-1.5f, 1.5f, xi);
                return (a - (a * a * a) / 3.0f) * 0.85f;
            }
            case Diode:
            {
                // Strongly asymmetric, like a single-diode clipper.
                return (xi >= 0.0f)
                    ? std::tanh (xi * 1.4f)
                    : std::tanh (xi * 0.55f) * 0.6f;
            }
            case Foldback:
            {
                // Sinusoidal foldback - rich high-order harmonics at high drive.
                const float a = juce::jlimit (-3.0f, 3.0f, xi);
                return std::sin (a * juce::MathConstants<float>::halfPi * 0.7f);
            }
            case Soft:
            {
                // Gentle atan curve.
                const float k = 1.0f / std::atan (1.5f);
                return std::atan (xi * 1.5f) * k * 0.92f;
            }
            case Hard:
            {
                return juce::jlimit (-1.0f, 1.0f, xi);
            }
        }
        return xi;
    }
};

//==============================================================================
class ST1AudioProcessor  : public juce::AudioProcessor
{
public:
    //==========================================================================
    ST1AudioProcessor();
    ~ST1AudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Public state for the editor
    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    // Smoothed metering values (in dB, -100..+6).
    std::atomic<float> inLevelDbL  { -100.0f };
    std::atomic<float> inLevelDbR  { -100.0f };
    std::atomic<float> outLevelDbL { -100.0f };
    std::atomic<float> outLevelDbR { -100.0f };

    // Lock-free oscilloscope ring buffer (mono sums of L/R for visualisation).
    static constexpr int scopeSize = 1024;
    std::array<float, scopeSize> scopeIn  {};
    std::array<float, scopeSize> scopeOut {};
    std::atomic<int> scopeWritePos { 0 };

    int  getCurrentMode() const noexcept;
    float getCurrentBias() const noexcept;
    float getCurrentDriveDb() const noexcept;

    // String IDs of all parameters - referenced by editor & preset manager.
    static const juce::String pidDrive;
    static const juce::String pidMode;
    static const juce::String pidBias;
    static const juce::String pidTone;
    static const juce::String pidMix;
    static const juce::String pidOutput;
    static const juce::String pidOversampling;
    static const juce::String pidBypass;

private:
    //==========================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void updateToneCoefficients (double sampleRate, float toneVal);

    // Cached parameter pointers
    std::atomic<float>* pDrive     = nullptr;
    std::atomic<float>* pMode      = nullptr;
    std::atomic<float>* pBias      = nullptr;
    std::atomic<float>* pTone      = nullptr;
    std::atomic<float>* pMix       = nullptr;
    std::atomic<float>* pOutput    = nullptr;
    std::atomic<float>* pOSampling = nullptr;
    std::atomic<float>* pBypass    = nullptr;

    // Parameter smoothing
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> drvSmooth,
                                                                  biasSmooth,
                                                                  mixSmooth,
                                                                  outSmooth;

    float lastTone = 0.0f;

    // Tone tilt (post-shaper): one low-shelf + one high-shelf duplicator.
    using FilterDuplicator = juce::dsp::ProcessorDuplicator<
                                juce::dsp::IIR::Filter<float>,
                                juce::dsp::IIR::Coefficients<float>>;
    FilterDuplicator tiltLow, tiltHigh;

    // Two oversamplers: 2x (1 stage) and 4x (2 stages).
    std::unique_ptr<juce::dsp::Oversampling<float>> os2x, os4x;

    // Dry copy for mix
    juce::AudioBuffer<float> dryBuffer;

    // Metering envelopes
    float envInL = 0.0f, envInR = 0.0f, envOutL = 0.0f, envOutR = 0.0f;
    float meterReleaseCoeff = 0.0f;

    int scopeDecimator = 0;
    int scopeDecimateRate = 1;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ST1AudioProcessor)
};
