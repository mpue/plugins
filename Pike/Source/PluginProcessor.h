/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "synth/PikeVoice.h"
#include "dsp/fx/FxChain.h"

//==============================================================================
/**
*/
class PikeAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    PikeAudioProcessor();
    ~PikeAudioProcessor() override;

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
    /** The plugin's parameter tree. The editor binds its controls to this. */
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }

    static constexpr int numVoices = 8;

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    juce::Synthesiser synth;
    pike::PikeVoice::Parameters voiceParameters;

    // Shared, read-only wavetable bank built in prepareToPlay.
    pike::Wavetable wavetable;

    // Runtime modulation values the voices read (computed each block).
    std::atomic<float> lfoIncShared[2]       { 0.0f, 0.0f };
    std::atomic<float> lfoMonoPhaseShared[2] { 0.0f, 0.0f };
    std::atomic<float> modWheelShared        { 0.0f };
    std::atomic<float> aftertouchShared      { 0.0f };
    double lfoMonoPhaseAccum[2] { 0.0, 0.0 };
    double currentBpm = 120.0;

    // Global post-mix effects.
    pike::FxChain fxChain;
    pike::FxChain::Params readFxParams() const;

    /** Caches the APVTS atomics the voices read each block. */
    void cacheParameterPointers();

    /** Updates LFO rates/phases and captures MIDI mod sources for this block. */
    void updateModulationRuntime (const juce::MidiBuffer& midi, int numSamples);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PikeAudioProcessor)
};
