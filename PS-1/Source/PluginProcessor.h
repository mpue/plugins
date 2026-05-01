/*
  ==============================================================================

    PluginProcessor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "LuxuryPitchShifter.h"
#include "PresetManager.h"

class PS1AudioProcessor  : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    PS1AudioProcessor();
    ~PS1AudioProcessor() override;

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

    juce::AudioProcessorValueTreeState apvts;
    PresetManager&       getPresetManager()  noexcept { return presetManager; }
    LuxuryPitchShifter&  getPitchShifter()   noexcept { return pitchShifter; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void updateAllParameters();

    LuxuryPitchShifter pitchShifter;
    PresetManager      presetManager;

    std::atomic<bool> parametersDirty { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PS1AudioProcessor)
};
