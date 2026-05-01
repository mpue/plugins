/*
  ==============================================================================

    PluginProcessor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamIDs {
    constexpr auto width      = "width";
    constexpr auto lowWidth   = "lowWidth";
    constexpr auto midWidth   = "midWidth";
    constexpr auto highWidth  = "highWidth";
    constexpr auto xLow       = "xLow";
    constexpr auto xHigh      = "xHigh";
    constexpr auto bassMonoHz = "bassMonoHz";
    constexpr auto bassMonoOn = "bassMonoOn";
    constexpr auto shimmer    = "shimmer";
    constexpr auto haas       = "haas";
    constexpr auto rotation   = "rotation";
    constexpr auto output     = "output";
    constexpr auto mix        = "mix";
    constexpr auto bypass     = "bypass";
    constexpr auto monoCheck  = "monoCheck";
}

juce::AudioProcessorValueTreeState::ParameterLayout SW1AudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using BoolParam  = juce::AudioParameterBool;
    using Range      = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::width, 1 },     "Width",
        Range { 0.0f, 200.0f, 0.1f }, 100.0f, "%"));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::lowWidth, 1 },  "Low Width",
        Range { 0.0f, 200.0f, 0.1f }, 100.0f, "%"));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::midWidth, 1 },  "Mid Width",
        Range { 0.0f, 200.0f, 0.1f }, 100.0f, "%"));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::highWidth, 1 }, "High Width",
        Range { 0.0f, 200.0f, 0.1f }, 100.0f, "%"));

    Range xLowRange  (80.0f, 1500.0f, 1.0f);  xLowRange.setSkewForCentre  (300.0f);
    Range xHighRange (1000.0f, 12000.0f, 1.0f); xHighRange.setSkewForCentre (3500.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::xLow, 1 },     "Low / Mid Split",
        xLowRange, 250.0f, "Hz"));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::xHigh, 1 },    "Mid / High Split",
        xHighRange, 3500.0f, "Hz"));

    Range bassRange (40.0f, 500.0f, 1.0f);  bassRange.setSkewForCentre (150.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::bassMonoHz, 1 }, "Bass Mono",
        bassRange, 120.0f, "Hz"));
    p.push_back (std::make_unique<BoolParam>  (
        juce::ParameterID { ParamIDs::bassMonoOn, 1 }, "Bass Mono On", true));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::shimmer, 1 },   "Shimmer",
        Range { 0.0f, 100.0f, 0.1f }, 0.0f, "%"));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::haas, 1 },      "Haas",
        Range { -20.0f, 20.0f, 0.01f }, 0.0f, "ms"));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::rotation, 1 },  "Rotation",
        Range { -45.0f, 45.0f, 0.1f }, 0.0f, juce::CharPointer_UTF8 ("\xc2\xb0"))); // °

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::output, 1 },    "Output",
        Range { -24.0f, 24.0f, 0.1f }, 0.0f, "dB"));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::mix, 1 },       "Mix",
        Range { 0.0f, 100.0f, 0.1f }, 100.0f, "%"));

    p.push_back (std::make_unique<BoolParam> (
        juce::ParameterID { ParamIDs::bypass, 1 },    "Bypass", false));
    p.push_back (std::make_unique<BoolParam> (
        juce::ParameterID { ParamIDs::monoCheck, 1 }, "Mono Check", false));

    return { p.begin(), p.end() };
}

//==============================================================================
SW1AudioProcessor::SW1AudioProcessor()
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
    static const std::array<const char*, 15> ids {
        ParamIDs::width, ParamIDs::lowWidth, ParamIDs::midWidth, ParamIDs::highWidth,
        ParamIDs::xLow, ParamIDs::xHigh, ParamIDs::bassMonoHz, ParamIDs::bassMonoOn,
        ParamIDs::shimmer, ParamIDs::haas, ParamIDs::rotation, ParamIDs::output,
        ParamIDs::mix, ParamIDs::bypass, ParamIDs::monoCheck
    };
    for (auto id : ids)
        apvts.addParameterListener (id, this);
}

SW1AudioProcessor::~SW1AudioProcessor()
{
    static const std::array<const char*, 15> ids {
        ParamIDs::width, ParamIDs::lowWidth, ParamIDs::midWidth, ParamIDs::highWidth,
        ParamIDs::xLow, ParamIDs::xHigh, ParamIDs::bassMonoHz, ParamIDs::bassMonoOn,
        ParamIDs::shimmer, ParamIDs::haas, ParamIDs::rotation, ParamIDs::output,
        ParamIDs::mix, ParamIDs::bypass, ParamIDs::monoCheck
    };
    for (auto id : ids)
        apvts.removeParameterListener (id, this);
}

void SW1AudioProcessor::parameterChanged (const juce::String&, float)
{
    parametersDirty.store (true);
}

void SW1AudioProcessor::updateAllParameters()
{
    LuxuryStereoWidener::Parameters p;
    p.widthPct      = apvts.getRawParameterValue (ParamIDs::width)->load();
    p.lowWidthPct   = apvts.getRawParameterValue (ParamIDs::lowWidth)->load();
    p.midWidthPct   = apvts.getRawParameterValue (ParamIDs::midWidth)->load();
    p.highWidthPct  = apvts.getRawParameterValue (ParamIDs::highWidth)->load();
    p.xLowHz        = apvts.getRawParameterValue (ParamIDs::xLow)->load();
    p.xHighHz       = apvts.getRawParameterValue (ParamIDs::xHigh)->load();
    p.bassMonoHz    = apvts.getRawParameterValue (ParamIDs::bassMonoHz)->load();
    p.bassMonoOn    = apvts.getRawParameterValue (ParamIDs::bassMonoOn)->load() > 0.5f;
    p.shimmer       = apvts.getRawParameterValue (ParamIDs::shimmer)->load() * 0.01f;
    p.haasMs        = apvts.getRawParameterValue (ParamIDs::haas)->load();
    p.rotationDeg   = apvts.getRawParameterValue (ParamIDs::rotation)->load();
    p.outputDb      = apvts.getRawParameterValue (ParamIDs::output)->load();
    p.mix           = apvts.getRawParameterValue (ParamIDs::mix)->load() * 0.01f;
    p.bypass        = apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;
    p.monoCheck     = apvts.getRawParameterValue (ParamIDs::monoCheck)->load() > 0.5f;
    widener.setParameters (p);
}

//==============================================================================
const juce::String SW1AudioProcessor::getName() const                { return JucePlugin_Name; }
bool SW1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}
bool SW1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}
bool SW1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}
double SW1AudioProcessor::getTailLengthSeconds() const               { return 0.05; }

int SW1AudioProcessor::getNumPrograms()                              { return 1; }
int SW1AudioProcessor::getCurrentProgram()                           { return 0; }
void SW1AudioProcessor::setCurrentProgram (int)                       {}
const juce::String SW1AudioProcessor::getProgramName (int)            { return {}; }
void SW1AudioProcessor::changeProgramName (int, const juce::String&)  {}

//==============================================================================
void SW1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    widener.prepare (sampleRate, samplesPerBlock);
    updateAllParameters();
    parametersDirty.store (false);
    presetManager.initialise();
}

void SW1AudioProcessor::releaseResources()
{
    widener.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SW1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SW1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (parametersDirty.exchange (false))
        updateAllParameters();

    widener.process (buffer);
}

//==============================================================================
bool SW1AudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SW1AudioProcessor::createEditor()
{
    return new SW1AudioProcessorEditor (*this);
}

//==============================================================================
void SW1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void SW1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new SW1AudioProcessor();
}
