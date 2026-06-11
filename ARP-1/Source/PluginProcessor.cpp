/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ARP1AudioProcessor::ARP1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

ARP1AudioProcessor::~ARP1AudioProcessor()
{
}

//==============================================================================
const juce::String ARP1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ARP1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ARP1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ARP1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ARP1AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ARP1AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ARP1AudioProcessor::getCurrentProgram()
{
    return 0;
}

void ARP1AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ARP1AudioProcessor::getProgramName (int index)
{
    return {};
}

void ARP1AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ARP1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    arpOutput.ensureSize (4096);
}

void ARP1AudioProcessor::releaseResources()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ARP1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ARP1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // Audio effect with MIDI out: pass incoming audio straight through, only
    // clearing any output channels that have no corresponding input.
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // ---- Host transport snapshot ----
    ARP1::ArpHostInfo host;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            host.isPlaying = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) host.bpm = *bpm;
        }
    }

    // ---- Arpeggiate: read input MIDI, write generated MIDI ----
    engine.renderBlock (numSamples, host, midiMessages, arpOutput);
    midiMessages.swapWith (arpOutput);
}

//==============================================================================
bool ARP1AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ARP1AudioProcessor::createEditor()
{
    return new ARP1AudioProcessorEditor (*this);
}

//==============================================================================
void ARP1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    patternManager.flush();   // persist any pending pattern edit before saving
    auto state = presetManager.captureState();
    state.setProperty ("currentPreset", presetManager.getCurrentPresetName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ARP1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ARP1AudioProcessor();
}
