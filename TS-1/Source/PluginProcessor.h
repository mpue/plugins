/*
  ==============================================================================

    TS-1 Transient Shaper – Processor

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

class TS1AudioProcessor  : public juce::AudioProcessor
{
public:
    TS1AudioProcessor();
    ~TS1AudioProcessor() override;

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

    //========================================================================
    // Parameters
    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //========================================================================
    // Visualisation accessors (thread-safe)
    float getInputLevel()      const { return inputLevel.load(); }
    float getOutputLevel()     const { return outputLevel.load(); }
    float getGainChangeDb()    const { return gainChangeDb.load(); }
    float getTransientActivity() const { return transientActivity.load(); }

    static constexpr int waveformSize = 384;
    void copyWaveformSnapshot (float* envOut, float* gainDbOut, int numSamples) const;

    //========================================================================
    // Preset system
    juce::File         getPresetDirectory() const;
    juce::StringArray  getFactoryPresetNames() const;
    juce::StringArray  getUserPresetNames() const;
    bool               loadPreset (const juce::String& name, bool isFactory);
    bool               savePreset (const juce::String& name);
    bool               deletePreset (const juce::String& name);
    juce::String       getCurrentPresetName() const { return currentPresetName; }
    void               setCurrentPresetName (const juce::String& n) { currentPresetName = n; }

private:
    //========================================================================
    // DSP state
    struct ChannelState
    {
        float fastEnv = 0.0f;
        float slowEnv = 0.0f;
    };
    std::array<ChannelState, 2> envState{};

    float fastAttCoeff = 0.0f, fastRelCoeff = 0.0f;
    float slowAttCoeff = 0.0f, slowRelCoeff = 0.0f;

    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> mixSmoothed;

    // Atomic params (audio thread reads)
    std::atomic<float>* attackParam      = nullptr;
    std::atomic<float>* sustainParam     = nullptr;
    std::atomic<float>* outputParam      = nullptr;
    std::atomic<float>* mixParam         = nullptr;
    std::atomic<float>* sensitivityParam = nullptr;
    std::atomic<float>* bypassParam      = nullptr;

    // Visualisation atomics
    std::atomic<float> inputLevel       { 0.0f };
    std::atomic<float> outputLevel      { 0.0f };
    std::atomic<float> gainChangeDb     { 0.0f };
    std::atomic<float> transientActivity{ 0.0f };

    // Lock-free ring buffers for visualiser
    std::array<float, waveformSize> envHistory{};
    std::array<float, waveformSize> gainHistory{};
    std::atomic<int> wfWriteIdx { 0 };
    int wfDecimator = 0;
    int wfDecimateN = 8;

    juce::String currentPresetName { "Init" };

    void computeEnvelopeCoeffs (double sampleRate);
    juce::String getFactoryPresetXml (const juce::String& name) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TS1AudioProcessor)
};
