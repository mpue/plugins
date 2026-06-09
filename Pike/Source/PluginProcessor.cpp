/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterLayout.h"
#include "params/ParameterIDs.h"
#include "synth/PikeSound.h"

//==============================================================================
PikeAudioProcessor::PikeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMETERS", pike::createParameterLayout())
#else
     : apvts (*this, nullptr, "PARAMETERS", pike::createParameterLayout())
#endif
{
    cacheParameterPointers();

    synth.addSound (new pike::PikeSound());
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new pike::PikeVoice (voiceParameters));

    synth.setNoteStealingEnabled (true);
}

void PikeAudioProcessor::cacheParameterPointers()
{
    voiceParameters.ampAttack  = apvts.getRawParameterValue (pid::ampAttack);
    voiceParameters.ampDecay   = apvts.getRawParameterValue (pid::ampDecay);
    voiceParameters.ampSustain = apvts.getRawParameterValue (pid::ampSustain);
    voiceParameters.ampRelease = apvts.getRawParameterValue (pid::ampRelease);
}

PikeAudioProcessor::~PikeAudioProcessor()
{
}

//==============================================================================
const juce::String PikeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PikeAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PikeAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PikeAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PikeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PikeAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PikeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PikeAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String PikeAudioProcessor::getProgramName (int index)
{
    return {};
}

void PikeAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void PikeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    synth.setCurrentPlaybackSampleRate (sampleRate);

    // Propagate the sample rate to every voice's DSP.
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = synth.getVoice (i))
            voice->setCurrentPlaybackSampleRate (sampleRate);
}

void PikeAudioProcessor::releaseResources()
{
    // Nothing allocated outside prepareToPlay; voices reset themselves on note-off.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PikeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void PikeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Synth instrument: no audio input. Start from silence, then let the voices
    // render the active notes into the buffer.
    buffer.clear();

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // Master gain (dB -> linear), applied to the whole block.
    const float gainDb     = apvts.getRawParameterValue (pid::masterGain)->load();
    const float gainLinear = juce::Decibels::decibelsToGain (gainDb);
    buffer.applyGain (gainLinear);
}

//==============================================================================
bool PikeAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PikeAudioProcessor::createEditor()
{
    return new PikeAudioProcessorEditor (*this);
}

//==============================================================================
void PikeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Serialise the whole parameter tree to XML (ValueTree-based state).
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PikeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PikeAudioProcessor();
}
