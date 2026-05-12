/*
  ==============================================================================

    PluginProcessor.h
    KM-1 Luxury Kick Machine — synth plugin. Hosts a single-voice kick
    engine driven from MIDI note-ons (or a UI audition trigger), with an
    APVTS parameter set and a code-defined factory preset library.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "KickEngine.h"
#include "PresetManager.h"

class KM1AudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    KM1AudioProcessor();
    ~KM1AudioProcessor() override;

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
    // Build a Params snapshot from current APVTS values.
    KickVoice::Params buildParamsSnapshot() const noexcept;

    // Audition: editor calls this to schedule a trigger on the next block.
    void requestAudition (float velocity = 1.0f) noexcept;

    // Editor reads these to drive its visualiser playhead.
    int  consumeTriggerEvent (float& velocityOut) noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    KickVoice voice;

    std::atomic<int>   auditionPending  { 0 };
    std::atomic<float> auditionVelocity { 1.0f };

    std::atomic<int>   triggerCounter   { 0 };
    std::atomic<float> lastTriggerVel   { 1.0f };
    int                lastSeenTrigger  = 0;

    // raw APVTS pointers
    std::atomic<float>* tuneParam       = nullptr;
    std::atomic<float>* pitchAmtParam   = nullptr;
    std::atomic<float>* pitchTimeParam  = nullptr;
    std::atomic<float>* bodyDecayParam  = nullptr;
    std::atomic<float>* bodyShapeParam  = nullptr;
    std::atomic<float>* clickLevelParam = nullptr;
    std::atomic<float>* clickToneParam  = nullptr;
    std::atomic<float>* subLevelParam   = nullptr;
    std::atomic<float>* driveParam      = nullptr;
    std::atomic<float>* punchParam      = nullptr;
    std::atomic<float>* toneParam       = nullptr;
    std::atomic<float>* outputParam     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KM1AudioProcessor)
};
