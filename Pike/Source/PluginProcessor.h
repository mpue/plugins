/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "synth/PikeVoice.h"

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

    /** Caches the APVTS atomics the voices read each block. */
    void cacheParameterPointers();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PikeAudioProcessor)
};
