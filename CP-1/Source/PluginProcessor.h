/*
  ==============================================================================
    CP-1 Compressor — Processor
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "PresetManager.h"

class CP1AudioProcessor  : public juce::AudioProcessor
{
public:
    CP1AudioProcessor();
    ~CP1AudioProcessor() override;

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

    // Public state for GUI
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    PresetManager presetManager;

    // Live metering values (atomic, lock-free) read by the editor
    std::atomic<float> grDb       { 0.0f };       // current gain reduction in dB (positive)
    std::atomic<float> inLevelDb  { -60.0f };
    std::atomic<float> outLevelDb { -60.0f };
    std::atomic<bool>  scExternalConnected { false };

private:
    // ---------------- DSP helpers ----------------

    // Second-order high-pass biquad (RBJ cookbook style)
    struct Biquad
    {
        float b0 { 1 }, b1 { 0 }, b2 { 0 }, a1 { 0 }, a2 { 0 };
        float z1 { 0 }, z2 { 0 };

        void reset() noexcept { z1 = z2 = 0.0f; }

        void setHighPass (double sampleRate, double freqHz, double Q) noexcept
        {
            const double f  = juce::jlimit (10.0, sampleRate * 0.45, freqHz);
            const double w0 = 2.0 * juce::MathConstants<double>::pi * f / sampleRate;
            const double cw = std::cos (w0);
            const double sw = std::sin (w0);
            const double alpha = sw / (2.0 * juce::jmax (0.0001, Q));

            const double b0n =  (1.0 + cw) * 0.5;
            const double b1n = -(1.0 + cw);
            const double b2n =  (1.0 + cw) * 0.5;
            const double a0n =  1.0 + alpha;
            const double a1n = -2.0 * cw;
            const double a2n =  1.0 - alpha;

            const double inv = 1.0 / a0n;
            b0 = (float) (b0n * inv);
            b1 = (float) (b1n * inv);
            b2 = (float) (b2n * inv);
            a1 = (float) (a1n * inv);
            a2 = (float) (a2n * inv);
        }

        // Transposed Direct Form II
        forcedinline float processSample (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    // Soft-knee gain computer. Returns positive dB of gain reduction.
    static forcedinline float computeReductionDb (float xDb, float threshDb,
                                                  float ratio, float kneeDb) noexcept
    {
        const float overshoot = xDb - threshDb;
        const float halfKnee  = kneeDb * 0.5f;
        const float slope     = 1.0f - 1.0f / juce::jmax (1.0f, ratio);

        if (overshoot <= -halfKnee)
            return 0.0f;
        if (overshoot >= halfKnee || kneeDb <= 0.0001f)
            return overshoot * slope;

        const float k = overshoot + halfKnee;
        return slope * (k * k) / (2.0f * kneeDb);
    }

    // ---------------- DSP state ----------------
    double sampleRate { 44100.0 };

    Biquad scHpfL, scHpfR;

    // RMS smoothers (per channel)
    float rmsStateL { 0.0f };
    float rmsStateR { 0.0f };

    // Gain-reduction envelope (positive dB), per channel
    float grEnvL { 0.0f };
    float grEnvR { 0.0f };

    // Adaptive release: smoothed average of GR (used to slow down release on sustained signals)
    float grSlowL { 0.0f };
    float grSlowR { 0.0f };

    // For meter smoothing
    float meterInDb  { -60.0f };
    float meterOutDb { -60.0f };
    float meterGrDb  {   0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CP1AudioProcessor)
};
