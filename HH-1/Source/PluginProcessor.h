/*
  ==============================================================================

    PluginProcessor.h
    HH-1 Luxury Hi-Hat Machine — synth plugin. Hosts a single-voice hi-hat
    engine driven from MIDI note-ons (or a UI audition trigger), with an
    APVTS parameter set and a code-defined factory preset library.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "HiHatEngine.h"
#include "PresetManager.h"

class HH1AudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    HH1AudioProcessor();
    ~HH1AudioProcessor() override;

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
    PresetManager presetManager;

    //==============================================================================
    HiHatVoice::Params buildParamsSnapshot() const noexcept;

    void requestAudition (float velocity = 1.0f) noexcept;

    int  consumeTriggerEvent (float& velocityOut) noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    HiHatVoice voice;

    std::atomic<int>   auditionPending  { 0 };
    std::atomic<float> auditionVelocity { 1.0f };

    std::atomic<int>   triggerCounter   { 0 };
    std::atomic<float> lastTriggerVel   { 1.0f };
    int                lastSeenTrigger  = 0;

    std::atomic<float>* tuneParam       = nullptr;
    std::atomic<float>* metalParam      = nullptr;
    std::atomic<float>* harmonicsParam  = nullptr;
    std::atomic<float>* hpCutParam      = nullptr;
    std::atomic<float>* bpCutParam      = nullptr;
    std::atomic<float>* shimmerQParam   = nullptr;
    std::atomic<float>* decayParam      = nullptr;
    std::atomic<float>* holdParam       = nullptr;
    std::atomic<float>* noiseParam      = nullptr;
    std::atomic<float>* colorParam      = nullptr;
    std::atomic<float>* driveParam      = nullptr;
    std::atomic<float>* toneParam       = nullptr;
    std::atomic<float>* widthParam      = nullptr;
    std::atomic<float>* outputParam     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HH1AudioProcessor)
};
