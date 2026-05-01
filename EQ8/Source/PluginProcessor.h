/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "EQBand.h"
#include "SpectrumAnalyzer.h"
#include "PresetManager.h"

//==============================================================================
/**
*/
class EQ8AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    EQ8AudioProcessor();
    ~EQ8AudioProcessor() override;

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
    EQBand* getEQBand(int index) 
    { 
        if (index >= 0 && index < 8)
            return &eqBands[index];
        return nullptr;
    }

    std::array<EQBand, 8>& getEQBands() { return eqBands; }
    SpectrumAnalyzer& getSpectrumAnalyzer() { return spectrumAnalyzer; }
    PresetManager& getPresetManager() { return *presetManager; }

private:
    //==============================================================================
    std::array<EQBand, 8> eqBands;
    SpectrumAnalyzer spectrumAnalyzer;
    std::unique_ptr<PresetManager> presetManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQ8AudioProcessor)
};
