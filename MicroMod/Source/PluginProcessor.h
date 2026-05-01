/*
  ==============================================================================

    PluginProcessor.h
    MicroMod — modular synth audio processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Modules.h"

constexpr int kMaxInstancesPerType = 4;

/** Listener so the editor can react when the chain changes from outside. */
class ChainListener
{
public:
    virtual ~ChainListener() = default;
    virtual void chainChanged() = 0;
};

class MicroModAudioProcessor  : public juce::AudioProcessor
{
public:
    MicroModAudioProcessor();
    ~MicroModAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override   { return true; }
    bool producesMidi() const override  { return false; }
    bool isMidiEffect() const override  { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override                  { return 1; }
    int getCurrentProgram() override               { return 0; }
    void setCurrentProgram (int) override          {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Chain management — called from the UI thread.

    /** Snapshot of the current chain for the UI. Each entry references a Module
        owned by the processor (lifetime guaranteed for the lifetime of the
        processor or until the chain is mutated). */
    struct ChainEntry
    {
        mm::ModuleType type;
        mm::Module*    module;   // not owning
        int            id;       // stable id within this session
    };

    std::vector<ChainEntry> getChainSnapshot() const;
    int  getInstanceCount (mm::ModuleType t) const;
    bool canAdd (mm::ModuleType t) const;

    void addModule (mm::ModuleType t);
    void removeModule (int id);
    void moveModule (int id, int newIndex);
    void setModuleEnabled (int id, bool enabled);

    mm::Module* findModuleById (int id) const;

    void addChainListener (ChainListener* l)    { listeners.add (l); }
    void removeChainListener (ChainListener* l) { listeners.remove (l); }

private:
    struct ChainSlot
    {
        std::unique_ptr<mm::Module> module;
        int id = 0;
    };

    void rebuildChain();
    void notifyChainChanged();

    mutable juce::CriticalSection chainLock;
    std::vector<ChainSlot> chain;
    int nextId = 1;

    mm::VoiceManager voices;

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    juce::ListenerList<ChainListener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroModAudioProcessor)
};
