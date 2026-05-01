/*
  ==============================================================================

    PluginProcessor.h
    DL-1 — Luxury Stereo Delay

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DelayEngine.h"

class PresetManager;

class DL1AudioProcessor  : public juce::AudioProcessor
{
public:
    DL1AudioProcessor();
    ~DL1AudioProcessor() override;

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

    // ==== DL-1 specific ====
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    DelayEngine::VisualState& getVisualState() noexcept { return engine.getVisualState(); }
    PresetManager& getPresetManager() noexcept { return *presetManager; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static const juce::StringArray& getSyncDivisionNames();
    static double getSyncDivisionBeats(int index);

private:
    juce::AudioProcessorValueTreeState apvts;
    DelayEngine engine;
    std::unique_ptr<PresetManager> presetManager;

    double currentBpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DL1AudioProcessor)
};
