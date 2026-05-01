/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace Cloudseed;


//==============================================================================
UltraVerbAudioProcessor::UltraVerbAudioProcessor() : fft(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hann)
#ifndef JucePlugin_PreferredChannelConfigurations
     , AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ) 
#endif
{
    

    ProgramDarkPlate[Parameter::DryOut] = 0.8705999851226807;
    ProgramDarkPlate[Parameter::EarlyDiffuseCount] = 0.2960000038146973;
    ProgramDarkPlate[Parameter::EarlyDiffuseDelay] = 0.3066999912261963;
    ProgramDarkPlate[Parameter::EarlyDiffuseEnabled] = 0.0;
    ProgramDarkPlate[Parameter::EarlyDiffuseFeedback] = 0.7706999778747559;
    ProgramDarkPlate[Parameter::EarlyDiffuseModAmount] = 0.143899992108345;
    ProgramDarkPlate[Parameter::EarlyDiffuseModRate] = 0.2466999888420105;
    ProgramDarkPlate[Parameter::EarlyOut] = 0.0;
    ProgramDarkPlate[Parameter::EqCrossSeed] = 0.0;
    ProgramDarkPlate[Parameter::EqCutoff] = 0.9759999513626099;
    ProgramDarkPlate[Parameter::EqHighFreq] = 0.5133999586105347;
    ProgramDarkPlate[Parameter::EqHighGain] = 0.7680000066757202;
    ProgramDarkPlate[Parameter::EqHighShelfEnabled] = 1.0;
    ProgramDarkPlate[Parameter::EqLowFreq] = 0.3879999816417694;
    ProgramDarkPlate[Parameter::EqLowGain] = 0.5559999942779541;
    ProgramDarkPlate[Parameter::EqLowShelfEnabled] = 1.0;
    ProgramDarkPlate[Parameter::EqLowpassEnabled] = 1.0;
    ProgramDarkPlate[Parameter::HighCut] = 0.2933000028133392;
    ProgramDarkPlate[Parameter::HighCutEnabled] = 1.0;
    ProgramDarkPlate[Parameter::InputMix] = 0.2346999943256378;
    ProgramDarkPlate[Parameter::Interpolation] = 1.0;
    ProgramDarkPlate[Parameter::LateDiffuseCount] = 0.4879999756813049;
    ProgramDarkPlate[Parameter::LateDiffuseDelay] = 0.239999994635582;
    ProgramDarkPlate[Parameter::LateDiffuseEnabled] = 1.0;
    ProgramDarkPlate[Parameter::LateDiffuseFeedback] = 0.8506999611854553;
    ProgramDarkPlate[Parameter::LateDiffuseModAmount] = 0.1467999964952469;
    ProgramDarkPlate[Parameter::LateDiffuseModRate] = 0.1666999906301498;
    ProgramDarkPlate[Parameter::LateLineCount] = 1.0;
    ProgramDarkPlate[Parameter::LateLineDecay] = 0.6345999836921692;
    ProgramDarkPlate[Parameter::LateLineModAmount] = 0.2719999849796295;
    ProgramDarkPlate[Parameter::LateLineModRate] = 0.2292999923229218;
    ProgramDarkPlate[Parameter::LateLineSize] = 0.4693999886512756;
    ProgramDarkPlate[Parameter::LateMode] = 1.0;
    ProgramDarkPlate[Parameter::LateOut] = 0.6613999605178833;
    ProgramDarkPlate[Parameter::LowCut] = 0.6399999856948853;
    ProgramDarkPlate[Parameter::LowCutEnabled] = 1.0;
    ProgramDarkPlate[Parameter::SeedDelay] = 0.2180999964475632;
    ProgramDarkPlate[Parameter::SeedDiffusion] = 0.1850000023841858;
    ProgramDarkPlate[Parameter::SeedPostDiffusion] = 0.3652999997138977;
    ProgramDarkPlate[Parameter::SeedTap] = 0.3339999914169312;
    ProgramDarkPlate[Parameter::TapDecay] = 1.0;
    ProgramDarkPlate[Parameter::TapLength] = 0.9866999983787537;
    ProgramDarkPlate[Parameter::TapPredelay] = 0.0;
    ProgramDarkPlate[Parameter::TapCount] = 0.1959999948740005;
    ProgramDarkPlate[Parameter::TapEnabled] = 1.0;
 
    parameters = new AudioProcessorValueTreeState(*this, nullptr);

    getValueTreeState()->state = ValueTree(Identifier("default"));

}

UltraVerbAudioProcessor::~UltraVerbAudioProcessor()
{
    delete parameters;
}

//==============================================================================
const juce::String UltraVerbAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool UltraVerbAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool UltraVerbAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool UltraVerbAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double UltraVerbAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int UltraVerbAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int UltraVerbAudioProcessor::getCurrentProgram()
{
    return 0;
}

void UltraVerbAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String UltraVerbAudioProcessor::getProgramName (int index)
{
    return {};
}

void UltraVerbAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void UltraVerbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    buffer.setSize(2, samplesPerBlock);
    tempBuffer.setSize(1, fftSize);

    reverb = std::make_unique<Cloudseed::ReverbController>(sampleRate);

    for (int i = 0; i < Cloudseed::Parameter::COUNT; i++)
    {
        reverb->SetParameter(i, ProgramDarkPlate[i] );
    }

    reverb->SetSamplerate(sampleRate);
    reverb->ClearBuffers();

}

void UltraVerbAudioProcessor::releaseResources()
{
    reverb = nullptr;
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool UltraVerbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void UltraVerbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    float* channelDataInL = const_cast<float*>(buffer.getReadPointer(0));
    float* channelDataInR = const_cast<float*>(buffer.getReadPointer(1));

    float* channelDataOutL = buffer.getWritePointer(0);
    float* channelDataOutR = buffer.getWritePointer(1);


    reverb-> Process(channelDataInL , channelDataInR, channelDataOutL, channelDataOutR, buffer.getNumSamples());
        

    if (buffer.getNumChannels() > 0) {
        tempBuffer.clear();

        auto* channelData = buffer.getReadPointer(0);
        
        if (post) {
            channelData = channelDataOutL;
        }
        tempBuffer.copyFrom(0, 0, channelData, buffer.getNumSamples());
        performFFT();

    }

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        analyzer.pushNextSampleIntoFifo(channelDataOutL[i]);
    }

}

//==============================================================================
bool UltraVerbAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* UltraVerbAudioProcessor::createEditor()
{
    return new UltraVerbAudioProcessorEditor (*this);
}

//==============================================================================
void UltraVerbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void UltraVerbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UltraVerbAudioProcessor();
}


juce::AudioProcessorValueTreeState* UltraVerbAudioProcessor::getValueTreeState() {
    return this->parameters;
}

void UltraVerbAudioProcessor::parameterChanged(const String& parameterID, float newValue)
{
}

void UltraVerbAudioProcessor::parameterValueChanged(int parameterIndex, float newValue)
{
}

void UltraVerbAudioProcessor::parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
}
