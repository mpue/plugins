/*
  ==============================================================================

    PluginProcessor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamIDs {
    constexpr auto source       = "source";
    constexpr auto position     = "position";
    constexpr auto spray        = "spray";
    constexpr auto grainsize    = "grainsize";
    constexpr auto density      = "density";
    constexpr auto pitch        = "pitch";
    constexpr auto pitchspray   = "pitchspray";
    constexpr auto reverse      = "reverse";
    constexpr auto panspread    = "panspread";
    constexpr auto movement     = "movement";
    constexpr auto attack       = "attack";
    constexpr auto release      = "release";
    constexpr auto tone         = "tone";
    constexpr auto drive        = "drive";
    constexpr auto lushness     = "lushness";
    constexpr auto space        = "space";
    constexpr auto width        = "width";
    constexpr auto volume       = "volume";
    constexpr auto octave       = "octave";
    constexpr auto lforate      = "lforate";

    static const juce::StringArray all
    {
        source, position, spray, grainsize, density,
        pitch, pitchspray, reverse, panspread, movement,
        attack, release, tone, drive,
        lushness, space, width, volume, octave, lforate
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout GS1AudioProcessor::createParameterLayout()
{
    using FloatParam  = juce::AudioParameterFloat;
    using IntParam    = juce::AudioParameterInt;
    using ChoiceParam = juce::AudioParameterChoice;
    using Range       = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    juce::StringArray sources { "Vocal", "Strings", "Choir", "Bell", "Glass", "Air" };
    p.push_back (std::make_unique<ChoiceParam> (
        juce::ParameterID { ParamIDs::source, 1 },     "Source",
        sources, 0));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::position, 1 },   "Position",
        Range { 0.0f, 1.0f, 0.001f }, 0.30f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::spray, 1 },      "Spray",
        Range { 0.0f, 1.0f, 0.001f }, 0.20f));

    Range grainRange (20.0f, 400.0f, 0.5f);
    grainRange.setSkewForCentre (120.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::grainsize, 1 },  "Grain Size",
        grainRange, 120.0f, "ms"));

    Range densityRange (4.0f, 160.0f, 0.5f);
    densityRange.setSkewForCentre (35.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::density, 1 },    "Density",
        densityRange, 30.0f, "/s"));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::pitch, 1 },      "Pitch",
        Range { -24.0f, 24.0f, 1.0f }, 0.0f, "st"));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::pitchspray, 1 }, "Pitch Spray",
        Range { 0.0f, 1.0f, 0.001f }, 0.04f));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::reverse, 1 },    "Reverse",
        Range { 0.0f, 1.0f, 0.001f }, 0.10f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::panspread, 1 },  "Pan Spread",
        Range { 0.0f, 1.0f, 0.001f }, 0.65f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::movement, 1 },   "Movement",
        Range { 0.0f, 1.0f, 0.001f }, 0.30f));

    Range attackRange (0.05f, 8.0f, 0.01f);
    attackRange.setSkewForCentre (1.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::attack, 1 },     "Attack",
        attackRange, 0.50f, "s"));

    Range releaseRange (0.10f, 12.0f, 0.01f);
    releaseRange.setSkewForCentre (2.5f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::release, 1 },    "Release",
        releaseRange, 1.80f, "s"));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::tone, 1 },       "Tone",
        Range { 0.0f, 1.0f, 0.001f }, 0.50f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::drive, 1 },      "Drive",
        Range { 0.0f, 1.0f, 0.001f }, 0.15f));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::lushness, 1 },   "Lushness",
        Range { 0.0f, 1.0f, 0.001f }, 0.55f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::space, 1 },      "Space",
        Range { 0.0f, 1.0f, 0.001f }, 0.55f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::width, 1 },      "Width",
        Range { 0.0f, 1.0f, 0.001f }, 0.85f));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::volume, 1 },     "Volume",
        Range { -36.0f, 6.0f, 0.1f }, -6.0f, "dB"));

    p.push_back (std::make_unique<IntParam> (
        juce::ParameterID { ParamIDs::octave, 1 },     "Octave",
        -2, 2, 0));

    Range lfoRange (0.05f, 4.0f, 0.001f);
    lfoRange.setSkewForCentre (0.5f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::lforate, 1 },    "LFO Rate",
        lfoRange, 0.30f, "Hz"));

    return { p.begin(), p.end() };
}

//==============================================================================
GS1AudioProcessor::GS1AudioProcessor()
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
    for (auto& id : ParamIDs::all)
        apvts.addParameterListener (id, this);
}

GS1AudioProcessor::~GS1AudioProcessor()
{
    for (auto& id : ParamIDs::all)
        apvts.removeParameterListener (id, this);
}

void GS1AudioProcessor::parameterChanged (const juce::String&, float)
{
    parametersDirty.store (true);
}

void GS1AudioProcessor::updateAllParameters()
{
    GranularEngine::Parameters p;
    p.sourceIdx     = (int) apvts.getRawParameterValue (ParamIDs::source)->load();
    p.position      = apvts.getRawParameterValue (ParamIDs::position)->load();
    p.spray         = apvts.getRawParameterValue (ParamIDs::spray)->load();
    p.grainSizeMs   = apvts.getRawParameterValue (ParamIDs::grainsize)->load();
    p.density       = apvts.getRawParameterValue (ParamIDs::density)->load();
    p.pitchSemis    = apvts.getRawParameterValue (ParamIDs::pitch)->load();
    p.pitchSpray    = apvts.getRawParameterValue (ParamIDs::pitchspray)->load();
    p.reverseProb   = apvts.getRawParameterValue (ParamIDs::reverse)->load();
    p.panSpread     = apvts.getRawParameterValue (ParamIDs::panspread)->load();
    p.movement      = apvts.getRawParameterValue (ParamIDs::movement)->load();
    p.attackSec     = apvts.getRawParameterValue (ParamIDs::attack)->load();
    p.releaseSec    = apvts.getRawParameterValue (ParamIDs::release)->load();
    p.tone          = apvts.getRawParameterValue (ParamIDs::tone)->load();
    p.drive         = apvts.getRawParameterValue (ParamIDs::drive)->load();
    p.lushness      = apvts.getRawParameterValue (ParamIDs::lushness)->load();
    p.space         = apvts.getRawParameterValue (ParamIDs::space)->load();
    p.width         = apvts.getRawParameterValue (ParamIDs::width)->load();
    p.volumeDb      = apvts.getRawParameterValue (ParamIDs::volume)->load();
    p.octaveShift   = (int) apvts.getRawParameterValue (ParamIDs::octave)->load();
    p.lfoRateHz     = apvts.getRawParameterValue (ParamIDs::lforate)->load();
    engine.setParameters (p);
}

//==============================================================================
const juce::String GS1AudioProcessor::getName() const                { return JucePlugin_Name; }
bool GS1AudioProcessor::acceptsMidi() const                          { return true; }
bool GS1AudioProcessor::producesMidi() const                         { return false; }
bool GS1AudioProcessor::isMidiEffect() const                         { return false; }
double GS1AudioProcessor::getTailLengthSeconds() const               { return 12.0; }

int GS1AudioProcessor::getNumPrograms()                              { return 1; }
int GS1AudioProcessor::getCurrentProgram()                           { return 0; }
void GS1AudioProcessor::setCurrentProgram (int)                       {}
const juce::String GS1AudioProcessor::getProgramName (int)            { return {}; }
void GS1AudioProcessor::changeProgramName (int, const juce::String&)  {}

//==============================================================================
void GS1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    updateAllParameters();
    parametersDirty.store (false);
    keyboardState.reset();

    if (! presetManagerInitialised)
    {
        presetManager.initialise();
        presetManagerInitialised = true;
    }
}

void GS1AudioProcessor::releaseResources()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GS1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

void GS1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (parametersDirty.exchange (false))
        updateAllParameters();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    engine.processMidi (midi, numSamples);
    engine.process (buffer);
}

//==============================================================================
bool GS1AudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* GS1AudioProcessor::createEditor()
{
    return new GS1AudioProcessorEditor (*this);
}

//==============================================================================
void GS1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void GS1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new GS1AudioProcessor();
}
