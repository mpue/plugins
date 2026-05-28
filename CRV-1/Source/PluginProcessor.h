/*
  ==============================================================================

    PluginProcessor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "ConvolutionEngine.h"
#include "IRLibrary.h"
#include "PresetManager.h"

class CRV1AudioProcessor  : public juce::AudioProcessor,
                            public juce::AsyncUpdater,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    CRV1AudioProcessor();
    ~CRV1AudioProcessor() override;

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

    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    ConvolutionEngine& getEngine() noexcept { return engine; }
    IRLibrary&         getIRLibrary() noexcept { return irLibrary; }
    PresetManager&     getPresetManager() noexcept { return presetManager; }

    // Selected IR name (lives outside APVTS because it is a string).
    juce::String getSelectedIRName() const;
    void setSelectedIRName (const juce::String& name);

    // Forces a rebuild + emission of the current IR (used by the editor when
    // opening, so that the visualizer immediately shows the live waveform).
    void requestIRRefresh();

    // Notifies the editor whenever the IR has been (re)rendered. Editor wires
    // a callback to update its waveform display. Use the setter so access is
    // serialised through the internal critical section.
    using IRRenderedFn = std::function<void (const juce::AudioBuffer<float>&, double, const juce::String&)>;
    void setOnIRRendered (IRRenderedFn fn);

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void rebuildAndLoadIR();
    void updateEngineParameters();

    IRRenderedFn onIRRendered;
    juce::CriticalSection callbackLock;

    ConvolutionEngine engine;
    IRLibrary         irLibrary;
    PresetManager     presetManager;

    juce::String selectedIRName;
    juce::CriticalSection irNameLock;

    std::atomic<bool> engineParamsDirty { true };
    std::atomic<bool> irDirty           { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CRV1AudioProcessor)
};
