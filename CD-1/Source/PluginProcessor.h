/*
  ==============================================================================

    PluginProcessor.h
    CD-1 Cinematic Drums — multi-voice synth plugin. Fires up to 8 drum
    voices polyphonically, mixes them into a stereo bus, applies a tilt
    EQ + soft saturation + room reverb, and routes the result to the host.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "CinematicDrumEngine.h"
#include "RoomReverb.h"
#include "PresetManager.h"

class CD1AudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    CD1AudioProcessor();
    ~CD1AudioProcessor() override;

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
    PresetManager                      presetManager;

    //==============================================================================
    // Editor interface

    // Schedule a UI-driven trigger for a particular drum.
    void requestAudition (int drumIdx, float velocity = 0.95f) noexcept;

    // Editor pulls trigger events that just happened (for the visualizer).
    struct TriggerEvent { int drumIdx; float velocity; };
    bool consumeTriggerEvent (TriggerEvent& outEvent) noexcept;

    // Editor pulls audio buffer for waveform display.
    void copyDisplayAudio (float* destL, float* destR, int& numOut, int maxSamples) noexcept;

    // Master peak for the editor's meters.
    void   getMasterPeak (float& L, float& R) noexcept;

    // Build a snapshot of the current macros for tooltip / preview.
    cd1::MasterMacros buildMasterSnapshot() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    static constexpr int kMaxVoices = 8;

    cd1::DrumVoice voices[kMaxVoices];
    cd1::RoomReverb reverb;

    // master tilt-EQ + air shelf state
    float tiltLP_L = 0.0f, tiltLP_R = 0.0f, tiltCoeff = 0.0f;
    float airHP_L  = 0.0f, airHP_R  = 0.0f, airCoeff  = 0.0f;

    // peak meters
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };

    // ---- per-drum APVTS pointers ----
    struct DrumPtrs { std::atomic<float>* tune; std::atomic<float>* decay; std::atomic<float>* level; std::atomic<float>* pan; };
    DrumPtrs drumP[cd1::NumDrums] {};

    // ---- master macro APVTS pointers ----
    std::atomic<float>* depthP   = nullptr;
    std::atomic<float>* impactP  = nullptr;
    std::atomic<float>* airP     = nullptr;
    std::atomic<float>* driveP   = nullptr;
    std::atomic<float>* widthP   = nullptr;
    std::atomic<float>* sizeP    = nullptr;
    std::atomic<float>* toneP    = nullptr;
    std::atomic<float>* outputP  = nullptr;

    // reverb internals
    std::atomic<float>* rvSizeP  = nullptr;
    std::atomic<float>* rvDampP  = nullptr;
    std::atomic<float>* rvLowP   = nullptr;

    // ---- audition request queue (UI -> audio) ----
    std::atomic<int>   auditionPending { 0 };
    std::atomic<int>   auditionDrum    { 0 };
    std::atomic<float> auditionVelocity { 1.0f };

    // ---- trigger event queue (audio -> UI) ----
    static constexpr int kTriggerQueueSize = 32;
    struct QueuedEvent { int drumIdx; float velocity; };
    QueuedEvent eventQueue[kTriggerQueueSize] {};
    std::atomic<int> eventWrite { 0 };
    std::atomic<int> eventRead  { 0 };

    // ---- display ring buffer (audio -> UI) ----
    static constexpr int kDispSize = 16384;
    std::vector<float> dispL, dispR;
    std::atomic<int>   dispWrite { 0 };
    int                dispLastRead = 0;

    // ---- helpers ----
    int findFreeVoice() noexcept;
    void triggerDrum (int drumIdx, float velocity) noexcept;
    void pushTriggerEvent (int drumIdx, float velocity) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CD1AudioProcessor)
};
