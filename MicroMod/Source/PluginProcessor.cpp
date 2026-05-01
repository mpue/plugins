/*
  ==============================================================================

    PluginProcessor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

MicroModAudioProcessor::MicroModAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Default starter chain: Oscillator -> Filter -> AmpEnv -> Reverb
    addModule (mm::ModuleType::Oscillator);
    addModule (mm::ModuleType::Filter);
    addModule (mm::ModuleType::AmpEnv);
    addModule (mm::ModuleType::Reverb);
}

MicroModAudioProcessor::~MicroModAudioProcessor() = default;

const juce::String MicroModAudioProcessor::getName() const { return JucePlugin_Name; }

void MicroModAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    const juce::ScopedLock sl (chainLock);
    for (auto& s : chain)
    {
        s.module->prepare (sampleRate, samplesPerBlock);
        s.module->reset();
    }
    voices.reset();
}

void MicroModAudioProcessor::releaseResources()
{
    const juce::ScopedLock sl (chainLock);
    for (auto& s : chain)
        s.module->reset();
    voices.reset();
}

bool MicroModAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void MicroModAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Synth: clear the buffer, modules ADD into it.
    buffer.clear();

    // Process MIDI: note on / off / all-notes-off.
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
            voices.allocateVoice (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())
            voices.releaseNote (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            voices.releaseAll();
    }

    mm::ProcessContext ctx;
    ctx.sampleRate = currentSampleRate;
    ctx.numSamples = buffer.getNumSamples();
    ctx.voices     = &voices;
    ctx.hasPendingMod = false;
    ctx.pendingModValue = 0.0f;

    const juce::ScopedLock sl (chainLock);
    for (auto& s : chain)
        s.module->process (buffer, ctx);
}

bool MicroModAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* MicroModAudioProcessor::createEditor()
{
    return new MicroModAudioProcessorEditor (*this);
}

//==============================================================================
// State serialization

void MicroModAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock sl (chainLock);
    juce::ValueTree state ("MicroMod");
    state.setProperty ("version", 1, nullptr);
    auto chainTree = juce::ValueTree ("chain");
    for (auto& s : chain)
    {
        juce::ValueTree m ("module");
        m.setProperty ("type", (int) s.module->getType(), nullptr);
        m.setProperty ("enabled", s.module->isEnabled(), nullptr);
        for (int i = 0; i < s.module->getNumParams(); ++i)
            m.setProperty ("p" + juce::String (i), s.module->getParam (i), nullptr);
        chainTree.appendChild (m, nullptr);
    }
    state.appendChild (chainTree, nullptr);
    juce::MemoryOutputStream mos (destData, false);
    state.writeToStream (mos);
}

void MicroModAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! state.isValid() || ! state.hasType ("MicroMod"))
        return;

    {
        const juce::ScopedLock sl (chainLock);
        chain.clear();
        auto chainTree = state.getChildWithName ("chain");
        for (int i = 0; i < chainTree.getNumChildren(); ++i)
        {
            auto m = chainTree.getChild (i);
            const int t = (int) m.getProperty ("type", -1);
            if (t < 0 || t >= (int) mm::ModuleType::NumTypes) continue;
            auto mod = mm::createModule ((mm::ModuleType) t);
            if (mod == nullptr) continue;
            mod->setEnabled ((bool) m.getProperty ("enabled", true));
            for (int p = 0; p < mod->getNumParams(); ++p)
            {
                auto v = m.getProperty ("p" + juce::String (p));
                if (! v.isVoid())
                    mod->setParam (p, (float) v);
            }
            mod->prepare (currentSampleRate, currentBlockSize);
            mod->reset();
            chain.push_back ({ std::move (mod), nextId++ });
        }
    }
    notifyChainChanged();
}

//==============================================================================
// Chain ops

std::vector<MicroModAudioProcessor::ChainEntry> MicroModAudioProcessor::getChainSnapshot() const
{
    const juce::ScopedLock sl (chainLock);
    std::vector<ChainEntry> out;
    out.reserve (chain.size());
    for (auto& s : chain)
        out.push_back ({ s.module->getType(), s.module.get(), s.id });
    return out;
}

int MicroModAudioProcessor::getInstanceCount (mm::ModuleType t) const
{
    const juce::ScopedLock sl (chainLock);
    int n = 0;
    for (auto& s : chain)
        if (s.module->getType() == t) ++n;
    return n;
}

bool MicroModAudioProcessor::canAdd (mm::ModuleType t) const
{
    return getInstanceCount (t) < kMaxInstancesPerType;
}

void MicroModAudioProcessor::addModule (mm::ModuleType t)
{
    if (! canAdd (t)) return;
    auto m = mm::createModule (t);
    if (m == nullptr) return;
    m->prepare (currentSampleRate, currentBlockSize);
    m->reset();
    {
        const juce::ScopedLock sl (chainLock);
        chain.push_back ({ std::move (m), nextId++ });
    }
    notifyChainChanged();
}

void MicroModAudioProcessor::removeModule (int id)
{
    {
        const juce::ScopedLock sl (chainLock);
        chain.erase (std::remove_if (chain.begin(), chain.end(),
                                     [id] (const ChainSlot& s) { return s.id == id; }),
                     chain.end());
    }
    notifyChainChanged();
}

void MicroModAudioProcessor::moveModule (int id, int newIndex)
{
    {
        const juce::ScopedLock sl (chainLock);
        auto it = std::find_if (chain.begin(), chain.end(),
                                [id] (const ChainSlot& s) { return s.id == id; });
        if (it == chain.end()) return;
        ChainSlot moved = std::move (*it);
        chain.erase (it);
        newIndex = juce::jlimit (0, (int) chain.size(), newIndex);
        chain.insert (chain.begin() + newIndex, std::move (moved));
    }
    notifyChainChanged();
}

void MicroModAudioProcessor::setModuleEnabled (int id, bool enabled)
{
    const juce::ScopedLock sl (chainLock);
    for (auto& s : chain)
        if (s.id == id) { s.module->setEnabled (enabled); return; }
}

mm::Module* MicroModAudioProcessor::findModuleById (int id) const
{
    const juce::ScopedLock sl (chainLock);
    for (auto& s : chain)
        if (s.id == id) return s.module.get();
    return nullptr;
}

void MicroModAudioProcessor::notifyChainChanged()
{
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        listeners.call ([] (ChainListener& l) { l.chainChanged(); });
    }
    else
    {
        juce::MessageManager::callAsync ([this]
        {
            listeners.call ([] (ChainListener& l) { l.chainChanged(); });
        });
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MicroModAudioProcessor();
}
