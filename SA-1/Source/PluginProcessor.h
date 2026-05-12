/*
  ==============================================================================

    PluginProcessor.h
    SA-1 Luxury Quick Sampler — drum sampler plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SamplerEngine.h"
#include "PresetManager.h"

class SA1AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SA1AudioProcessor();
    ~SA1AudioProcessor() override;

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
    SA1::SamplerEngine& getEngine()        noexcept { return engine; }
    SA1::PresetManager& getPresetManager() noexcept { return presetManager; }

    /** Audition request from UI (RT-safe). */
    void requestAudition (int padIndex, float velocity = 1.0f) noexcept;

private:
    SA1::SamplerEngine engine;
    SA1::PresetManager presetManager;

    struct PendingAudition { int padIndex; float velocity; };

    std::array<std::atomic<int>,   SA1::kNumPads> auditionPending {};
    std::array<std::atomic<float>, SA1::kNumPads> auditionVelocity {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SA1AudioProcessor)
};
