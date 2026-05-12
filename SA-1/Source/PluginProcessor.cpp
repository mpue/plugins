/*
  ==============================================================================

    PluginProcessor.cpp
    SA-1 Luxury Quick Sampler

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SA1AudioProcessor::SA1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       presetManager (engine)
#else
     : presetManager (engine)
#endif
{
    for (auto& a : auditionPending)  a.store (0);
    for (auto& v : auditionVelocity) v.store (1.0f);
}

SA1AudioProcessor::~SA1AudioProcessor() = default;

//==============================================================================
const juce::String SA1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SA1AudioProcessor::acceptsMidi()        const { return true; }
bool SA1AudioProcessor::producesMidi()       const { return false; }
bool SA1AudioProcessor::isMidiEffect()       const { return false; }
double SA1AudioProcessor::getTailLengthSeconds() const { return 5.0; }

int SA1AudioProcessor::getNumPrograms()                                    { return 1; }
int SA1AudioProcessor::getCurrentProgram()                                 { return 0; }
void SA1AudioProcessor::setCurrentProgram (int)                            {}
const juce::String SA1AudioProcessor::getProgramName (int)                 { return {}; }
void SA1AudioProcessor::changeProgramName (int, const juce::String&)       {}

//==============================================================================
void SA1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engine.reset();
}

void SA1AudioProcessor::releaseResources()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SA1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

//==============================================================================
void SA1AudioProcessor::requestAudition (int padIndex, float velocity) noexcept
{
    if (! juce::isPositiveAndBelow (padIndex, SA1::kNumPads)) return;
    auditionVelocity[(size_t) padIndex].store (juce::jlimit (0.05f, 1.0f, velocity));
    auditionPending [(size_t) padIndex].fetch_add (1, std::memory_order_release);
}

//==============================================================================
void SA1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0) return;

    // Drain queued auditions from the UI
    for (int p = 0; p < SA1::kNumPads; ++p)
    {
        const int pending = auditionPending[(size_t) p].exchange (0, std::memory_order_acquire);
        if (pending > 0)
            engine.triggerPad (p, auditionVelocity[(size_t) p].load (std::memory_order_relaxed));
    }

    // Process MIDI events sample-accurately by chunks
    int offset = 0;
    auto it = midi.cbegin();
    while (offset < numSamples)
    {
        int nextEventSample = numSamples;
        // peek at next event
        auto temp = it;
        if (temp != midi.cend())
            nextEventSample = juce::jlimit (offset, numSamples,
                                            (*temp).samplePosition);

        // Render up to next event
        const int subBlock = nextEventSample - offset;
        if (subBlock > 0)
            engine.renderBlock (buffer, offset, subBlock);

        offset = nextEventSample;

        // Apply all events occurring at this offset
        while (it != midi.cend() && (*it).samplePosition <= offset)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn())
            {
                const int  note = m.getNoteNumber();
                const float vel = juce::jlimit (0.05f, 1.0f, (float) m.getFloatVelocity());
                const int   pad = engine.padForMidiNote (note);
                if (pad >= 0)
                    engine.triggerPad (pad, vel);
            }
            else if (m.isNoteOff())
            {
                const int note = m.getNoteNumber();
                const int pad  = engine.padForMidiNote (note);
                if (pad >= 0)
                    engine.releasePad (pad);
            }
            ++it;
        }
    }
}

//==============================================================================
bool SA1AudioProcessor::hasEditor() const                       { return true; }
juce::AudioProcessorEditor* SA1AudioProcessor::createEditor()   { return new SA1AudioProcessorEditor (*this); }

//==============================================================================
void SA1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = presetManager.captureState();
    state.setProperty ("currentPreset", presetManager.getCurrentPresetName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SA1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            const auto presetName = state.getProperty ("currentPreset", "Init").toString();
            presetManager.applyState (state);
            presetManager.setCurrentPresetNameForRestore (presetName);
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SA1AudioProcessor();
}
