/*
  ==============================================================================

    PluginProcessor.h
    BS-1 Luxury Bass Synthesizer — monophonic bass voice driven by MIDI
    (and a UI audition trigger). Code-defined factory preset library.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "BassEngine.h"
#include "PresetManager.h"

class BS1AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    BS1AudioProcessor();
    ~BS1AudioProcessor() override;

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
    PresetManager  presetManager;

    juce::MidiKeyboardState keyboardState; // for the editor's on-screen keyboard

    //==============================================================================
    BassVoice::Params buildParamsSnapshot() const noexcept;

    // Audition: editor calls this to request a one-shot audition note on next block.
    void requestAudition (float velocity = 0.95f) noexcept;
    void requestAuditionRelease() noexcept;

    int  consumeTriggerEvent (float& velocityOut) noexcept;

    // Editor reads recently rendered samples for the scope.
    void pullVisAudio (float* dest, int numSamples) noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    BassVoice voice;

    // ---- audition state ----
    std::atomic<int>   auditionPending  { 0 };       // counter
    std::atomic<float> auditionVelocity { 0.95f };
    std::atomic<bool>  auditionReleasePending { false };
    bool               auditionGateOn = false;
    static constexpr int kAuditionMidiNote = 36; // C2

    // visualisation: ring buffer for the editor to read
    static constexpr int kVisRingSize = 8192;
    std::array<float, kVisRingSize> visRing {};
    std::atomic<int>   visWritePos { 0 };
    std::atomic<int>   triggerCounter { 0 };
    std::atomic<float> lastTriggerVel { 1.0f };
    int                lastSeenTrigger = 0;

    // raw APVTS pointers
    std::atomic<float>* toneParam       = nullptr;
    std::atomic<float>* driveParam      = nullptr;
    std::atomic<float>* subLevelParam   = nullptr;
    std::atomic<float>* noiseLevelParam = nullptr;
    std::atomic<float>* octaveParam     = nullptr;

    std::atomic<float>* cutoffParam     = nullptr;
    std::atomic<float>* resonanceParam  = nullptr;
    std::atomic<float>* envAmountParam  = nullptr;
    std::atomic<float>* filterDecayParam= nullptr;

    std::atomic<float>* ampAttackParam  = nullptr;
    std::atomic<float>* ampSustainParam = nullptr;
    std::atomic<float>* ampReleaseParam = nullptr;

    std::atomic<float>* glideParam      = nullptr;
    std::atomic<float>* warmthParam     = nullptr;
    std::atomic<float>* outputParam     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BS1AudioProcessor)
};
