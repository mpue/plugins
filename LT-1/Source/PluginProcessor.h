/*
  ==============================================================================

    LT-1 — Luxury Limiter
    PluginProcessor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

class PresetManager;

//==============================================================================
class LT1AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    LT1AudioProcessor();
    ~LT1AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    PresetManager& getPresetManager() noexcept { return *presetManager; }

    // ====== Atomic meter feeds for the editor ======
    std::atomic<float> meterInL  { 0.0f };
    std::atomic<float> meterInR  { 0.0f };
    std::atomic<float> meterOutL { 0.0f };
    std::atomic<float> meterOutR { 0.0f };
    std::atomic<float> meterGR   { 0.0f }; // current gain reduction in dB (positive number = reduction)
    std::atomic<float> meterGRPeak { 0.0f };

    // ====== Visualisation: ring buffer for input/output samples & gain envelope ======
    static constexpr int scopeSize = 1024;
    std::array<std::atomic<float>, scopeSize> scopeIn   { };
    std::array<std::atomic<float>, scopeSize> scopeOut  { };
    std::array<std::atomic<float>, scopeSize> scopeGain { }; // 0..1, 1.0 = no reduction
    std::atomic<int> scopeWriteIndex { 0 };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    std::unique_ptr<PresetManager> presetManager;

    // ====== DSP state ======
    double currentSampleRate = 44100.0;

    // Lookahead delay line (per channel)
    std::vector<std::vector<float>> delayLines;
    int  delayWrite = 0;
    int  lookaheadSamples = 0;

    // Gain envelope (smoothed gain factor, 1.0 = unity)
    float envelope = 1.0f;

    // Sliding-window peak detector for the lookahead buffer
    // Stores the maximum absolute sample over the lookahead window so attack
    // can be effectively zero (i.e. the limiter "sees" the peak before it arrives).
    std::vector<float> peakWindow;   // ring of |x| samples (mono, max over channels)
    int   peakWriteIdx = 0;
    int   peakWindowSize = 0;

    // Cached parameter atomic pointers
    std::atomic<float>* pThreshold   = nullptr;
    std::atomic<float>* pCeiling     = nullptr;
    std::atomic<float>* pRelease     = nullptr;
    std::atomic<float>* pKnee        = nullptr;
    std::atomic<float>* pInGain      = nullptr;
    std::atomic<float>* pOutGain     = nullptr;
    std::atomic<float>* pLookahead   = nullptr;
    std::atomic<float>* pAutoRelease = nullptr;
    std::atomic<float>* pStereoLink  = nullptr;
    std::atomic<float>* pBypass      = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LT1AudioProcessor)
};
