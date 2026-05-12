/*
  ==============================================================================

    PluginProcessor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamIDs {
    constexpr auto drive      = "drive";
    constexpr auto crunch     = "crunch";
    constexpr auto tone       = "tone";
    constexpr auto body       = "body";
    constexpr auto texture    = "texture";
    constexpr auto motion     = "motion";
    constexpr auto motionRate = "motionRate";
    constexpr auto age        = "age";
    constexpr auto width      = "width";
    constexpr auto mix        = "mix";
    constexpr auto output     = "output";
    constexpr auto character  = "character";
}

juce::AudioProcessorValueTreeState::ParameterLayout TR1AudioProcessor::createParameterLayout()
{
    using FloatParam  = juce::AudioParameterFloat;
    using ChoiceParam = juce::AudioParameterChoice;
    using Range       = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::drive, 1 },      "Drive",
        Range { 0.0f, 1.0f, 0.001f }, 0.45f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::crunch, 1 },     "Crunch",
        Range { 0.0f, 1.0f, 0.001f }, 0.0f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::tone, 1 },       "Tone",
        Range { 0.0f, 1.0f, 0.001f }, 0.5f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::body, 1 },       "Body",
        Range { 0.0f, 1.0f, 0.001f }, 0.5f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::texture, 1 },    "Texture",
        Range { 0.0f, 1.0f, 0.001f }, 0.0f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::motion, 1 },     "Motion",
        Range { 0.0f, 1.0f, 0.001f }, 0.0f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::motionRate, 1 }, "Rate",
        Range { 0.0f, 1.0f, 0.001f }, 0.5f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::age, 1 },        "Age",
        Range { 0.0f, 1.0f, 0.001f }, 0.20f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::width, 1 },      "Width",
        Range { 0.0f, 1.0f, 0.001f }, 1.0f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::mix, 1 },        "Mix",
        Range { 0.0f, 1.0f, 0.001f }, 1.0f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::output, 1 },     "Output",
        Range { -24.0f, 12.0f, 0.1f }, -2.0f, "dB"));

    juce::StringArray characters {
        "Tube", "Tape", "Fuzz", "Crush", "Telephone", "Radio", "Mangler", "Vintage Amp"
    };
    p.push_back (std::make_unique<ChoiceParam> (
        juce::ParameterID { ParamIDs::character, 1 },  "Character",
        characters, 0));

    return { p.begin(), p.end() };
}

//==============================================================================
TR1AudioProcessor::TR1AudioProcessor()
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
    for (auto id : { ParamIDs::drive, ParamIDs::crunch, ParamIDs::tone, ParamIDs::body,
                     ParamIDs::texture, ParamIDs::motion, ParamIDs::motionRate,
                     ParamIDs::age, ParamIDs::width, ParamIDs::mix,
                     ParamIDs::output, ParamIDs::character })
    {
        apvts.addParameterListener (id, this);
    }
}

TR1AudioProcessor::~TR1AudioProcessor()
{
    for (auto id : { ParamIDs::drive, ParamIDs::crunch, ParamIDs::tone, ParamIDs::body,
                     ParamIDs::texture, ParamIDs::motion, ParamIDs::motionRate,
                     ParamIDs::age, ParamIDs::width, ParamIDs::mix,
                     ParamIDs::output, ParamIDs::character })
    {
        apvts.removeParameterListener (id, this);
    }
}

void TR1AudioProcessor::parameterChanged (const juce::String&, float)
{
    parametersDirty.store (true);
}

void TR1AudioProcessor::updateAllParameters()
{
    TrashEngine::Parameters p;
    p.drive       = apvts.getRawParameterValue (ParamIDs::drive)->load();
    p.crunch      = apvts.getRawParameterValue (ParamIDs::crunch)->load();
    p.tone        = apvts.getRawParameterValue (ParamIDs::tone)->load();
    p.body        = apvts.getRawParameterValue (ParamIDs::body)->load();
    p.texture     = apvts.getRawParameterValue (ParamIDs::texture)->load();
    p.motion      = apvts.getRawParameterValue (ParamIDs::motion)->load();
    p.motionRate  = apvts.getRawParameterValue (ParamIDs::motionRate)->load();
    p.age         = apvts.getRawParameterValue (ParamIDs::age)->load();
    p.width       = apvts.getRawParameterValue (ParamIDs::width)->load();
    p.mix         = apvts.getRawParameterValue (ParamIDs::mix)->load();
    p.outputDb    = apvts.getRawParameterValue (ParamIDs::output)->load();
    p.character   = (int) apvts.getRawParameterValue (ParamIDs::character)->load();
    engine.setParameters (p);
}

//==============================================================================
const juce::String TR1AudioProcessor::getName() const                { return JucePlugin_Name; }

bool TR1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}
bool TR1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}
bool TR1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}
double TR1AudioProcessor::getTailLengthSeconds() const               { return 0.5; }

int TR1AudioProcessor::getNumPrograms()                              { return 1; }
int TR1AudioProcessor::getCurrentProgram()                           { return 0; }
void TR1AudioProcessor::setCurrentProgram (int)                       {}
const juce::String TR1AudioProcessor::getProgramName (int)            { return {}; }
void TR1AudioProcessor::changeProgramName (int, const juce::String&)  {}

//==============================================================================
void TR1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    updateAllParameters();
    parametersDirty.store (false);
    presetManager.initialise();
}

void TR1AudioProcessor::releaseResources()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TR1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void TR1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (parametersDirty.exchange (false))
        updateAllParameters();

    engine.process (buffer);
}

//==============================================================================
bool TR1AudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* TR1AudioProcessor::createEditor()
{
    return new TR1AudioProcessorEditor (*this);
}

//==============================================================================
void TR1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void TR1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new TR1AudioProcessor();
}
