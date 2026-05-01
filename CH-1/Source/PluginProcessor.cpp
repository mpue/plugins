/*
  ==============================================================================

    PluginProcessor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr const char* idRate     = "rate";
    constexpr const char* idDepth    = "depth";
    constexpr const char* idDelay    = "delay";
    constexpr const char* idFeedback = "feedback";
    constexpr const char* idMix      = "mix";
    constexpr const char* idWidth    = "width";
    constexpr const char* idVoices   = "voices";
    constexpr const char* idTone     = "tone";
    constexpr const char* idGain     = "gain";
    constexpr const char* idBypass   = "bypass";
}

juce::AudioProcessorValueTreeState::ParameterLayout
CH1AudioProcessor::createParameterLayout()
{
    using AP = juce::AudioParameterFloat;
    using AC = juce::AudioParameterChoice;
    using AB = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto rateRange  = juce::NormalisableRange<float> (0.05f, 8.0f, 0.0f, 0.35f);
    auto delayRange = juce::NormalisableRange<float> (1.0f, 25.0f, 0.0f, 0.7f);
    auto toneRange  = juce::NormalisableRange<float> (1000.0f, 18000.0f, 0.0f, 0.4f);

    layout.add (std::make_unique<AP> (juce::ParameterID { idRate, 1 },     "Rate",     rateRange,  0.5f,
                                      juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idDepth, 1 },    "Depth",
                                      juce::NormalisableRange<float> (0.0f, 100.0f, 0.0f), 35.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idDelay, 1 },    "Delay",    delayRange, 9.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idFeedback, 1 }, "Feedback",
                                      juce::NormalisableRange<float> (-95.0f, 95.0f, 0.0f), 0.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idMix, 1 },      "Mix",
                                      juce::NormalisableRange<float> (0.0f, 100.0f, 0.0f), 50.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idWidth, 1 },    "Width",
                                      juce::NormalisableRange<float> (0.0f, 100.0f, 0.0f), 70.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AC> (juce::ParameterID { idVoices, 1 },   "Voices",
                                      juce::StringArray { "1", "2", "3", "4" }, 2));

    layout.add (std::make_unique<AP> (juce::ParameterID { idTone, 1 },     "Tone",     toneRange, 8000.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idGain, 1 },     "Output",
                                      juce::NormalisableRange<float> (-24.0f, 12.0f, 0.0f), 0.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AB> (juce::ParameterID { idBypass, 1 },   "Bypass", false));

    return layout;
}

//==============================================================================
CH1AudioProcessor::CH1AudioProcessor()
   #ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
   #else
    :
   #endif
      apvts (*this, nullptr, "CH1", createParameterLayout()),
      presetManager (apvts, "CH-1")
{
    pRate     = apvts.getRawParameterValue (idRate);
    pDepth    = apvts.getRawParameterValue (idDepth);
    pDelay    = apvts.getRawParameterValue (idDelay);
    pFeedback = apvts.getRawParameterValue (idFeedback);
    pMix      = apvts.getRawParameterValue (idMix);
    pWidth    = apvts.getRawParameterValue (idWidth);
    pVoices   = apvts.getRawParameterValue (idVoices);
    pTone     = apvts.getRawParameterValue (idTone);
    pGain     = apvts.getRawParameterValue (idGain);
    pBypass   = apvts.getRawParameterValue (idBypass);
}

CH1AudioProcessor::~CH1AudioProcessor() = default;

//==============================================================================
const juce::String CH1AudioProcessor::getName() const          { return JucePlugin_Name; }
bool CH1AudioProcessor::acceptsMidi() const                    { return false; }
bool CH1AudioProcessor::producesMidi() const                   { return false; }
bool CH1AudioProcessor::isMidiEffect() const                   { return false; }
double CH1AudioProcessor::getTailLengthSeconds() const         { return 0.1; }

int CH1AudioProcessor::getNumPrograms()                        { return 1; }
int CH1AudioProcessor::getCurrentProgram()                     { return 0; }
void CH1AudioProcessor::setCurrentProgram (int)                {}
const juce::String CH1AudioProcessor::getProgramName (int)     { return {}; }
void CH1AudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void CH1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
    pullParametersToEngine();
}

void CH1AudioProcessor::releaseResources()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CH1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void CH1AudioProcessor::pullParametersToEngine()
{
    if (pRate     != nullptr) engine.setRate        (pRate->load());
    if (pDepth    != nullptr) engine.setDepthMs     ((pDepth->load() * 0.01f) * 15.0f); // 0..100% -> 0..15 ms
    if (pDelay    != nullptr) engine.setBaseDelayMs (pDelay->load());
    if (pFeedback != nullptr) engine.setFeedback    (pFeedback->load());
    if (pMix      != nullptr) engine.setMix         (pMix->load());
    if (pWidth    != nullptr) engine.setWidth       (pWidth->load());
    if (pTone     != nullptr) engine.setToneHz      (pTone->load());
    if (pGain     != nullptr) engine.setOutputDb    (pGain->load());
    if (pVoices   != nullptr) engine.setNumVoices   ((int) pVoices->load() + 1);
}

void CH1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    pullParametersToEngine();

    const bool bypass = pBypass != nullptr && pBypass->load() > 0.5f;
    if (bypass) return;

    engine.process (buffer);
}

//==============================================================================
bool CH1AudioProcessor::hasEditor() const                            { return true; }
juce::AudioProcessorEditor* CH1AudioProcessor::createEditor()        { return new CH1AudioProcessorEditor (*this); }

//==============================================================================
void CH1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CH1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
            apvts.replaceState (state);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CH1AudioProcessor();
}
