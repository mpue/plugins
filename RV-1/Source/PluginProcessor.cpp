/*
  ==============================================================================

    PluginProcessor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamIDs {
    constexpr auto size       = "size";
    constexpr auto decay      = "decay";
    constexpr auto damping    = "damping";
    constexpr auto predelay   = "predelay";
    constexpr auto modulation = "modulation";
    constexpr auto diffusion  = "diffusion";
    constexpr auto width      = "width";
    constexpr auto mix        = "mix";
    constexpr auto lowcut     = "lowcut";
    constexpr auto highcut    = "highcut";
    constexpr auto character  = "character";
}

juce::AudioProcessorValueTreeState::ParameterLayout RV1AudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using ChoiceParam = juce::AudioParameterChoice;
    using Range = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::size, 1 },        "Size",
        Range { 0.0f, 1.0f, 0.001f }, 0.55f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::decay, 1 },       "Decay",
        Range { 0.0f, 1.0f, 0.001f }, 0.55f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::damping, 1 },     "Damping",
        Range { 0.0f, 1.0f, 0.001f }, 0.45f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::predelay, 1 },    "Pre-Delay",
        Range { 0.0f, 250.0f, 0.1f }, 20.0f, "ms"));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::modulation, 1 },  "Modulation",
        Range { 0.0f, 1.0f, 0.001f }, 0.30f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::diffusion, 1 },   "Diffusion",
        Range { 0.0f, 1.0f, 0.001f }, 0.72f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::width, 1 },       "Width",
        Range { 0.0f, 1.0f, 0.001f }, 1.00f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::mix, 1 },         "Mix",
        Range { 0.0f, 1.0f, 0.001f }, 0.30f));

    juce::NormalisableRange<float> lowRange (20.0f, 1000.0f, 1.0f);
    lowRange.setSkewForCentre (200.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::lowcut, 1 },      "Low Cut",
        lowRange, 90.0f, "Hz"));

    juce::NormalisableRange<float> hiRange (500.0f, 20000.0f, 1.0f);
    hiRange.setSkewForCentre (3000.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::highcut, 1 },     "High Cut",
        hiRange, 9000.0f, "Hz"));

    juce::StringArray characters { "Hall", "Plate", "Room", "Chamber", "Cathedral", "Spring" };
    p.push_back (std::make_unique<ChoiceParam> (
        juce::ParameterID { ParamIDs::character, 1 },   "Character",
        characters, 0));

    return { p.begin(), p.end() };
}

//==============================================================================
RV1AudioProcessor::RV1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
       presetManager (apvts)
#else
     : apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
       presetManager (apvts)
#endif
{
    for (auto id : { ParamIDs::size, ParamIDs::decay, ParamIDs::damping,
                     ParamIDs::predelay, ParamIDs::modulation, ParamIDs::diffusion,
                     ParamIDs::width, ParamIDs::mix, ParamIDs::lowcut,
                     ParamIDs::highcut, ParamIDs::character })
    {
        apvts.addParameterListener (id, this);
    }
}

RV1AudioProcessor::~RV1AudioProcessor()
{
    for (auto id : { ParamIDs::size, ParamIDs::decay, ParamIDs::damping,
                     ParamIDs::predelay, ParamIDs::modulation, ParamIDs::diffusion,
                     ParamIDs::width, ParamIDs::mix, ParamIDs::lowcut,
                     ParamIDs::highcut, ParamIDs::character })
    {
        apvts.removeParameterListener (id, this);
    }
}

void RV1AudioProcessor::parameterChanged (const juce::String&, float)
{
    parametersDirty.store (true);
}

void RV1AudioProcessor::updateAllParameters()
{
    LuxuryReverb::Parameters p;
    p.size       = apvts.getRawParameterValue (ParamIDs::size)->load();
    p.decay      = apvts.getRawParameterValue (ParamIDs::decay)->load();
    p.damping    = apvts.getRawParameterValue (ParamIDs::damping)->load();
    p.predelayMs = apvts.getRawParameterValue (ParamIDs::predelay)->load();
    p.modulation = apvts.getRawParameterValue (ParamIDs::modulation)->load();
    p.diffusion  = apvts.getRawParameterValue (ParamIDs::diffusion)->load();
    p.width      = apvts.getRawParameterValue (ParamIDs::width)->load();
    p.mix        = apvts.getRawParameterValue (ParamIDs::mix)->load();
    p.lowCutHz   = apvts.getRawParameterValue (ParamIDs::lowcut)->load();
    p.highCutHz  = apvts.getRawParameterValue (ParamIDs::highcut)->load();
    p.character  = (int) apvts.getRawParameterValue (ParamIDs::character)->load();
    reverb.setParameters (p);
}

//==============================================================================
const juce::String RV1AudioProcessor::getName() const                { return JucePlugin_Name; }
bool RV1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}
bool RV1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}
bool RV1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}
double RV1AudioProcessor::getTailLengthSeconds() const               { return 25.0; }

int RV1AudioProcessor::getNumPrograms()                              { return 1; }
int RV1AudioProcessor::getCurrentProgram()                           { return 0; }
void RV1AudioProcessor::setCurrentProgram (int)                       {}
const juce::String RV1AudioProcessor::getProgramName (int)            { return {}; }
void RV1AudioProcessor::changeProgramName (int, const juce::String&)  {}

//==============================================================================
void RV1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    reverb.prepare (sampleRate, samplesPerBlock);
    updateAllParameters();
    parametersDirty.store (false);
    presetManager.initialise();
}

void RV1AudioProcessor::releaseResources()
{
    reverb.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool RV1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void RV1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (parametersDirty.exchange (false))
        updateAllParameters();

    reverb.process (buffer);
}

//==============================================================================
bool RV1AudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* RV1AudioProcessor::createEditor()
{
    return new RV1AudioProcessorEditor (*this);
}

//==============================================================================
void RV1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void RV1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        parametersDirty.store (true);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RV1AudioProcessor();
}
