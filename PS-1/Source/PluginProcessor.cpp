/*
  ==============================================================================

    PluginProcessor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamIDs {
    constexpr auto pitch     = "pitch";
    constexpr auto fine      = "fine";
    constexpr auto mix       = "mix";
    constexpr auto feedback  = "feedback";
    constexpr auto formant   = "formant";
    constexpr auto width     = "width";
    constexpr auto drive     = "drive";
    constexpr auto lowcut    = "lowcut";
    constexpr auto highcut   = "highcut";
    constexpr auto character = "character";
    constexpr auto quality   = "quality";
}

juce::AudioProcessorValueTreeState::ParameterLayout PS1AudioProcessor::createParameterLayout()
{
    using FloatParam  = juce::AudioParameterFloat;
    using ChoiceParam = juce::AudioParameterChoice;
    using Range       = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::pitch, 1 },     "Pitch",
        Range { -24.0f, 24.0f, 1.0f }, 0.0f, "st"));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::fine, 1 },      "Fine",
        Range { -50.0f, 50.0f, 0.1f }, 0.0f, "ct"));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::mix, 1 },       "Mix",
        Range { 0.0f, 1.0f, 0.001f }, 1.0f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::feedback, 1 },  "Feedback",
        Range { 0.0f, 0.85f, 0.001f }, 0.0f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::formant, 1 },   "Formant",
        Range { 0.0f, 1.0f, 0.001f }, 0.0f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::width, 1 },     "Width",
        Range { 0.0f, 1.0f, 0.001f }, 0.5f));
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::drive, 1 },     "Drive",
        Range { 0.0f, 1.0f, 0.001f }, 0.0f));

    juce::NormalisableRange<float> lowRange (20.0f, 1000.0f, 1.0f);
    lowRange.setSkewForCentre (200.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::lowcut, 1 },    "Low Cut",
        lowRange, 30.0f, "Hz"));

    juce::NormalisableRange<float> hiRange (2000.0f, 20000.0f, 1.0f);
    hiRange.setSkewForCentre (5000.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::highcut, 1 },   "High Cut",
        hiRange, 18000.0f, "Hz"));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::quality, 1 },   "Quality",
        Range { 0.0f, 1.0f, 0.001f }, 0.6f));

    juce::StringArray characters { "Smooth", "Tight", "Wide", "Shimmer", "Crystal" };
    p.push_back (std::make_unique<ChoiceParam> (
        juce::ParameterID { ParamIDs::character, 1 }, "Character",
        characters, 0));

    return { p.begin(), p.end() };
}

//==============================================================================
PS1AudioProcessor::PS1AudioProcessor()
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
    for (auto id : { ParamIDs::pitch, ParamIDs::fine, ParamIDs::mix,
                     ParamIDs::feedback, ParamIDs::formant, ParamIDs::width,
                     ParamIDs::drive, ParamIDs::lowcut, ParamIDs::highcut,
                     ParamIDs::character, ParamIDs::quality })
    {
        apvts.addParameterListener (id, this);
    }
}

PS1AudioProcessor::~PS1AudioProcessor()
{
    for (auto id : { ParamIDs::pitch, ParamIDs::fine, ParamIDs::mix,
                     ParamIDs::feedback, ParamIDs::formant, ParamIDs::width,
                     ParamIDs::drive, ParamIDs::lowcut, ParamIDs::highcut,
                     ParamIDs::character, ParamIDs::quality })
    {
        apvts.removeParameterListener (id, this);
    }
}

void PS1AudioProcessor::parameterChanged (const juce::String&, float)
{
    parametersDirty.store (true);
}

void PS1AudioProcessor::updateAllParameters()
{
    LuxuryPitchShifter::Parameters p;
    p.pitchSemitones = apvts.getRawParameterValue (ParamIDs::pitch)->load();
    p.fineCents      = apvts.getRawParameterValue (ParamIDs::fine)->load();
    p.mix            = apvts.getRawParameterValue (ParamIDs::mix)->load();
    p.feedback       = apvts.getRawParameterValue (ParamIDs::feedback)->load();
    p.formant        = apvts.getRawParameterValue (ParamIDs::formant)->load();
    p.width          = apvts.getRawParameterValue (ParamIDs::width)->load();
    p.drive          = apvts.getRawParameterValue (ParamIDs::drive)->load();
    p.lowCutHz       = apvts.getRawParameterValue (ParamIDs::lowcut)->load();
    p.highCutHz      = apvts.getRawParameterValue (ParamIDs::highcut)->load();
    p.quality        = apvts.getRawParameterValue (ParamIDs::quality)->load();
    p.character      = (int) apvts.getRawParameterValue (ParamIDs::character)->load();
    pitchShifter.setParameters (p);
}

//==============================================================================
const juce::String PS1AudioProcessor::getName() const                { return JucePlugin_Name; }
bool PS1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}
bool PS1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}
bool PS1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}
double PS1AudioProcessor::getTailLengthSeconds() const               { return 4.0; }

int PS1AudioProcessor::getNumPrograms()                              { return 1; }
int PS1AudioProcessor::getCurrentProgram()                           { return 0; }
void PS1AudioProcessor::setCurrentProgram (int)                       {}
const juce::String PS1AudioProcessor::getProgramName (int)            { return {}; }
void PS1AudioProcessor::changeProgramName (int, const juce::String&)  {}

//==============================================================================
void PS1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    pitchShifter.prepare (sampleRate, samplesPerBlock);
    updateAllParameters();
    parametersDirty.store (false);
    presetManager.initialise();
}

void PS1AudioProcessor::releaseResources()
{
    pitchShifter.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PS1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void PS1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (parametersDirty.exchange (false))
        updateAllParameters();

    pitchShifter.process (buffer);
}

//==============================================================================
bool PS1AudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* PS1AudioProcessor::createEditor()
{
    return new PS1AudioProcessorEditor (*this);
}

//==============================================================================
void PS1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void PS1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new PS1AudioProcessor();
}
