/*
  ==============================================================================

    PluginProcessor.h
    ARP-1 Luxury Arpeggiator

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ArpEngine.h"
#include "PatternManager.h"
#include "PresetManager.h"

//==============================================================================
class ARP1AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ARP1AudioProcessor();
    ~ARP1AudioProcessor() override;

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
    ARP1::ArpEngine&      getEngine()         { return engine; }
    ARP1::PatternManager& getPatternManager() { return patternManager; }
    ARP1::PresetManager&  getPresetManager()  { return presetManager; }

private:
    //==============================================================================
    ARP1::ArpEngine      engine;
    ARP1::PatternManager patternManager { engine };
    ARP1::PresetManager  presetManager { engine, patternManager };

    juce::MidiBuffer    arpOutput;   // scratch buffer reused each block

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARP1AudioProcessor)
};
