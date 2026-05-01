/*
  ==============================================================================

    PluginProcessor.h
    BC-1 Luxury Bitcrusher – AudioProcessor with APVTS, click-free bypass,
    parallel dry/wet, and a lock-free scope tap for the visualizer.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Bitcrusher.h"
#include "PresetManager.h"

class BC1AudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    BC1AudioProcessor();
    ~BC1AudioProcessor() override;

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

    juce::AudioProcessorParameter* getBypassParameter() const override;

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
    PresetManager presetManager;

    //==============================================================================
    // Lock-free scope tap. Channel 0 only. The editor pulls the latest
    // samples from this ring buffer for visualization.
    static constexpr int scopeBufferSize = 4096;
    void copyScope (float* dryOut, float* wetOut, int numSamples) const noexcept;

    float getCurrentBits()  const noexcept;
    float getCurrentMix()   const noexcept;
    float getCurrentRate()  const noexcept;
    bool  getCurrentBypass() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    LuxuryBitcrusher bitcrusher;

    juce::AudioBuffer<float> dryCopy;

    std::array<float, scopeBufferSize> scopeIn  {};
    std::array<float, scopeBufferSize> scopeOut {};
    std::atomic<int> scopeWritePos { 0 };

    juce::SmoothedValue<float> mixSm, outputSm, processSm;

    std::atomic<float>* driveParam  = nullptr;
    std::atomic<float>* bitsParam   = nullptr;
    std::atomic<float>* rateParam   = nullptr;
    std::atomic<float>* ditherParam = nullptr;
    std::atomic<float>* toneParam   = nullptr;
    std::atomic<float>* mixParam    = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BC1AudioProcessor)
};
