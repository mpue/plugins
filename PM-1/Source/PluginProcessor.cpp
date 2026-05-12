/*
  ==============================================================================

    PluginProcessor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamIDs {
    constexpr auto texture    = "texture";
    constexpr auto warmth     = "warmth";
    constexpr auto brightness = "brightness";
    constexpr auto movement   = "movement";
    constexpr auto lushness   = "lushness";
    constexpr auto space      = "space";
    constexpr auto delay      = "delay";
    constexpr auto width      = "width";
    constexpr auto drive      = "drive";
    constexpr auto attack     = "attack";
    constexpr auto release    = "release";
    constexpr auto volume     = "volume";
    constexpr auto octave     = "octave";
    constexpr auto character  = "character";
    constexpr auto lforate    = "lforate";
}

juce::AudioProcessorValueTreeState::ParameterLayout PM1AudioProcessor::createParameterLayout()
{
    using FloatParam  = juce::AudioParameterFloat;
    using IntParam    = juce::AudioParameterInt;
    using ChoiceParam = juce::AudioParameterChoice;
    using Range       = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::texture, 1 },     "Texture",
        Range { 0.0f, 1.0f, 0.001f }, 0.50f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::warmth, 1 },      "Warmth",
        Range { 0.0f, 1.0f, 0.001f }, 0.55f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::brightness, 1 },  "Brightness",
        Range { 0.0f, 1.0f, 0.001f }, 0.55f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::movement, 1 },    "Movement",
        Range { 0.0f, 1.0f, 0.001f }, 0.35f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::lushness, 1 },    "Lushness",
        Range { 0.0f, 1.0f, 0.001f }, 0.55f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::space, 1 },       "Space",
        Range { 0.0f, 1.0f, 0.001f }, 0.45f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::delay, 1 },       "Delay",
        Range { 0.0f, 1.0f, 0.001f }, 0.18f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::width, 1 },       "Width",
        Range { 0.0f, 1.0f, 0.001f }, 0.85f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::drive, 1 },       "Drive",
        Range { 0.0f, 1.0f, 0.001f }, 0.20f));

    Range attackRange (0.05f, 8.0f, 0.01f);
    attackRange.setSkewForCentre (1.5f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::attack, 1 },      "Attack",
        attackRange, 1.20f, "s"));

    Range releaseRange (0.10f, 12.0f, 0.01f);
    releaseRange.setSkewForCentre (3.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::release, 1 },     "Release",
        releaseRange, 2.20f, "s"));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::volume, 1 },      "Volume",
        Range { -36.0f, 6.0f, 0.1f }, -6.0f, "dB"));

    p.push_back (std::make_unique<IntParam> (
        juce::ParameterID { ParamIDs::octave, 1 },      "Octave",
        -2, 2, 0));

    juce::StringArray characters { "Warm Pad", "Bright Pad", "Strings", "Choir", "Glass", "Air" };
    p.push_back (std::make_unique<ChoiceParam> (
        juce::ParameterID { ParamIDs::character, 1 },   "Character",
        characters, 0));

    Range lfoRange (0.05f, 5.0f, 0.001f);
    lfoRange.setSkewForCentre (0.5f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::lforate, 1 },     "LFO Rate",
        lfoRange, 0.35f, "Hz"));

    return { p.begin(), p.end() };
}

//==============================================================================
PM1AudioProcessor::PM1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
       presetManager (apvts)
#else
     : apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
       presetManager (apvts)
#endif
{
    for (auto id : { ParamIDs::texture, ParamIDs::warmth, ParamIDs::brightness,
                     ParamIDs::movement, ParamIDs::lushness, ParamIDs::space,
                     ParamIDs::delay, ParamIDs::width, ParamIDs::drive,
                     ParamIDs::attack, ParamIDs::release, ParamIDs::volume,
                     ParamIDs::octave, ParamIDs::character, ParamIDs::lforate })
    {
        apvts.addParameterListener (id, this);
    }
}

PM1AudioProcessor::~PM1AudioProcessor()
{
    for (auto id : { ParamIDs::texture, ParamIDs::warmth, ParamIDs::brightness,
                     ParamIDs::movement, ParamIDs::lushness, ParamIDs::space,
                     ParamIDs::delay, ParamIDs::width, ParamIDs::drive,
                     ParamIDs::attack, ParamIDs::release, ParamIDs::volume,
                     ParamIDs::octave, ParamIDs::character, ParamIDs::lforate })
    {
        apvts.removeParameterListener (id, this);
    }
}

void PM1AudioProcessor::parameterChanged (const juce::String&, float)
{
    parametersDirty.store (true);
}

void PM1AudioProcessor::updateAllParameters()
{
    LuxuryPadSynth::Parameters p;
    p.texture     = apvts.getRawParameterValue (ParamIDs::texture)->load();
    p.warmth      = apvts.getRawParameterValue (ParamIDs::warmth)->load();
    p.brightness  = apvts.getRawParameterValue (ParamIDs::brightness)->load();
    p.movement    = apvts.getRawParameterValue (ParamIDs::movement)->load();
    p.lushness    = apvts.getRawParameterValue (ParamIDs::lushness)->load();
    p.space       = apvts.getRawParameterValue (ParamIDs::space)->load();
    p.delaySend   = apvts.getRawParameterValue (ParamIDs::delay)->load();
    p.width       = apvts.getRawParameterValue (ParamIDs::width)->load();
    p.drive       = apvts.getRawParameterValue (ParamIDs::drive)->load();
    p.attackSec   = apvts.getRawParameterValue (ParamIDs::attack)->load();
    p.releaseSec  = apvts.getRawParameterValue (ParamIDs::release)->load();
    p.volumeDb    = apvts.getRawParameterValue (ParamIDs::volume)->load();
    p.octaveShift = (int) apvts.getRawParameterValue (ParamIDs::octave)->load();
    p.characterIdx = (int) apvts.getRawParameterValue (ParamIDs::character)->load();
    p.lfoRateHz   = apvts.getRawParameterValue (ParamIDs::lforate)->load();
    synth.setParameters (p);
}

//==============================================================================
const juce::String PM1AudioProcessor::getName() const                { return JucePlugin_Name; }
bool PM1AudioProcessor::acceptsMidi() const                          { return true; }
bool PM1AudioProcessor::producesMidi() const                         { return false; }
bool PM1AudioProcessor::isMidiEffect() const                         { return false; }
double PM1AudioProcessor::getTailLengthSeconds() const               { return 12.0; }

int PM1AudioProcessor::getNumPrograms()                              { return 1; }
int PM1AudioProcessor::getCurrentProgram()                           { return 0; }
void PM1AudioProcessor::setCurrentProgram (int)                       {}
const juce::String PM1AudioProcessor::getProgramName (int)            { return {}; }
void PM1AudioProcessor::changeProgramName (int, const juce::String&)  {}

//==============================================================================
void PM1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate, samplesPerBlock);
    updateAllParameters();
    parametersDirty.store (false);
    keyboardState.reset();
    presetManager.initialise();
}

void PM1AudioProcessor::releaseResources()
{
    synth.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PM1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

void PM1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Clear all output channels first (synth output)
    for (int i = 0; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (parametersDirty.exchange (false))
        updateAllParameters();

    // Inject on-screen keyboard notes into the MIDI stream
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    synth.processMidi (midi, numSamples);
    synth.process (buffer);
}

//==============================================================================
bool PM1AudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* PM1AudioProcessor::createEditor()
{
    return new PM1AudioProcessorEditor (*this);
}

//==============================================================================
void PM1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void PM1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new PM1AudioProcessor();
}
