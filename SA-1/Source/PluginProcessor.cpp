/*
  ==============================================================================

    PluginProcessor.cpp
    SA-1 Luxury Quick Sampler

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <array>

//==============================================================================
SA1AudioProcessor::SA1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       presetManager (engine, sequencer)
#else
     : presetManager (engine, sequencer)
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
    sequencer.prepare (sampleRate);
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

    const bool seqMode = sequencer.isEnabled();

    // ---- Merged event timeline for this block -------------------------------
    // Trigger / release events are sorted by sample offset and applied between
    // sub-block renders so every voice starts sample-accurately, whether it
    // came from a live note, a UI audition, or a sequencer step.
    struct Ev { int offset; int type; int pad; float vel; };   // type: 0 = trigger, 1 = release
    std::array<Ev, 1024> events;
    int numEvents = 0;
    auto addEvent = [&] (int off, int type, int pad, float vel)
    {
        if (numEvents < (int) events.size())
            events[(size_t) numEvents++] = { off, type, pad, vel };
    };

    // Queued auditions from the UI fire at the top of the block.
    for (int p = 0; p < SA1::kNumPads; ++p)
    {
        const int pending = auditionPending[(size_t) p].exchange (0, std::memory_order_acquire);
        if (pending > 0)
            addEvent (0, 0, p, auditionVelocity[(size_t) p].load (std::memory_order_relaxed));
    }

    // ---- MIDI ---------------------------------------------------------------
    // In sequencer mode notes drive the sequencer gate instead of pads.
    std::array<SA1::SeqNoteEvent, 256> seqNotes;
    int numSeqNotes = 0;

    for (const auto meta : midi)
    {
        const auto m   = meta.getMessage();
        const int  off = juce::jlimit (0, numSamples - 1, meta.samplePosition);

        if (seqMode)
        {
            if (m.isNoteOn() && numSeqNotes < (int) seqNotes.size())
                seqNotes[(size_t) numSeqNotes++] = { off, true };
            else if (m.isNoteOff() && numSeqNotes < (int) seqNotes.size())
                seqNotes[(size_t) numSeqNotes++] = { off, false };
        }
        else
        {
            if (m.isNoteOn())
            {
                const float vel = juce::jlimit (0.05f, 1.0f, (float) m.getFloatVelocity());
                const int   pad = engine.padForMidiNote (m.getNoteNumber());
                if (pad >= 0) addEvent (off, 0, pad, vel);
            }
            else if (m.isNoteOff())
            {
                const int pad = engine.padForMidiNote (m.getNoteNumber());
                if (pad >= 0) addEvent (off, 1, pad, 0.0f);
            }
        }
    }

    // ---- Host transport snapshot -------------------------------------------
    SA1::SeqHostInfo host;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            host.isPlaying = pos->getIsPlaying();
            if (auto bpm = pos->getBpm())         host.bpm             = *bpm;
            if (auto ppq = pos->getPpqPosition()) host.ppqAtBlockStart = *ppq;
        }
    }

    // ---- Sequencer: collect sample-accurate step fires ----------------------
    if (seqMode)
    {
        sequencer.renderBlock (numSamples, host, seqNotes.data(), numSeqNotes,
                               [&] (int off, int pad, float vel) { addEvent (off, 0, pad, vel); });
    }

    // ---- Sort & render between events ---------------------------------------
    std::stable_sort (events.begin(), events.begin() + numEvents,
                      [] (const Ev& a, const Ev& b) { return a.offset < b.offset; });

    int offset = 0, idx = 0;
    while (offset < numSamples)
    {
        const int nextEventSample = idx < numEvents
                                      ? juce::jlimit (offset, numSamples, events[(size_t) idx].offset)
                                      : numSamples;

        const int subBlock = nextEventSample - offset;
        if (subBlock > 0)
            engine.renderBlock (buffer, offset, subBlock);

        offset = nextEventSample;

        while (idx < numEvents && events[(size_t) idx].offset <= offset)
        {
            const auto& e = events[(size_t) idx];
            if (e.type == 0) engine.triggerPad (e.pad, e.vel);
            else             engine.releasePad (e.pad);
            ++idx;
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
