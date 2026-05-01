/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EQ8AudioProcessor::EQ8AudioProcessor()
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
    eqBands[0].setType(EQBand::LowShelf);
    eqBands[0].setFrequency(80.0f);
    eqBands[0].setQ(0.707f);
    eqBands[0].setGain(0.0f);

    eqBands[1].setType(EQBand::Peak);
    eqBands[1].setFrequency(150.0f);
    eqBands[1].setQ(1.0f);
    eqBands[1].setGain(0.0f);

    eqBands[2].setType(EQBand::Peak);
    eqBands[2].setFrequency(400.0f);
    eqBands[2].setQ(1.0f);
    eqBands[2].setGain(0.0f);

    eqBands[3].setType(EQBand::Peak);
    eqBands[3].setFrequency(1000.0f);
    eqBands[3].setQ(1.0f);
    eqBands[3].setGain(0.0f);

    eqBands[4].setType(EQBand::Peak);
    eqBands[4].setFrequency(2500.0f);
    eqBands[4].setQ(1.0f);
    eqBands[4].setGain(0.0f);

    eqBands[5].setType(EQBand::Peak);
    eqBands[5].setFrequency(5000.0f);
    eqBands[5].setQ(1.0f);
    eqBands[5].setGain(0.0f);

    eqBands[6].setType(EQBand::Peak);
    eqBands[6].setFrequency(10000.0f);
    eqBands[6].setQ(1.0f);
    eqBands[6].setGain(0.0f);

    eqBands[7].setType(EQBand::HighShelf);
    eqBands[7].setFrequency(12000.0f);
    eqBands[7].setQ(0.707f);
    eqBands[7].setGain(0.0f);

    presetManager = std::make_unique<PresetManager>(eqBands);
}

EQ8AudioProcessor::~EQ8AudioProcessor()
{
}

//==============================================================================
const juce::String EQ8AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EQ8AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EQ8AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EQ8AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EQ8AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EQ8AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EQ8AudioProcessor::getCurrentProgram()
{
    return 0;
}

void EQ8AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EQ8AudioProcessor::getProgramName (int index)
{
    return {};
}

void EQ8AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EQ8AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (auto& band : eqBands)
        band.prepare(sampleRate, samplesPerBlock);

    spectrumAnalyzer.prepare(sampleRate);
}

void EQ8AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EQ8AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void EQ8AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float leftSample = buffer.getSample(0, sample);
        float rightSample = totalNumInputChannels > 1 ? buffer.getSample(1, sample) : leftSample;

        spectrumAnalyzer.pushSample((leftSample + rightSample) * 0.5f);

        for (auto& band : eqBands)
            band.processSample(leftSample, rightSample);

        buffer.setSample(0, sample, leftSample);
        if (totalNumOutputChannels > 1)
            buffer.setSample(1, sample, rightSample);
    }
}

//==============================================================================
bool EQ8AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EQ8AudioProcessor::createEditor()
{
    return new EQ8AudioProcessorEditor (*this);
}

//==============================================================================
void EQ8AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = presetManager->createStateXml();
    copyXmlToBinary(*xml, destData);
}

void EQ8AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml != nullptr)
        presetManager->restoreFromStateXml(*xml);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQ8AudioProcessor();
}
