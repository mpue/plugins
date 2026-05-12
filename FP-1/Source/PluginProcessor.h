/*
  ==============================================================================

    PluginProcessor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "FlangerPhaserEngine.h"
#include "PresetManager.h"

class FP1AudioProcessor  : public juce::AudioProcessor
{
public:
    FP1AudioProcessor();
    ~FP1AudioProcessor() override;

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

    bool acceptsMidi()  const override;
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
    juce::AudioProcessorValueTreeState& getAPVTS()             noexcept { return apvts; }
    PresetManager&                      getPresetManager()     noexcept { return presetManager; }
    FlangerPhaserEngine&                getEngine()            noexcept { return engine; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void pullParametersToEngine();

    juce::AudioProcessorValueTreeState apvts;
    FlangerPhaserEngine                engine;
    PresetManager                      presetManager;

    std::atomic<float>* pMode     = nullptr;
    std::atomic<float>* pLfoShape = nullptr;
    std::atomic<float>* pStages   = nullptr;
    std::atomic<float>* pRate     = nullptr;
    std::atomic<float>* pDepth    = nullptr;
    std::atomic<float>* pManual   = nullptr;
    std::atomic<float>* pFeedback = nullptr;
    std::atomic<float>* pMix      = nullptr;
    std::atomic<float>* pWidth    = nullptr;
    std::atomic<float>* pTone     = nullptr;
    std::atomic<float>* pGain     = nullptr;
    std::atomic<float>* pBypass   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FP1AudioProcessor)
};
