/*
  ==============================================================================

    PluginProcessor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr const char* idMode     = "mode";
    constexpr const char* idLfoShape = "lfoShape";
    constexpr const char* idStages   = "stages";
    constexpr const char* idRate     = "rate";
    constexpr const char* idDepth    = "depth";
    constexpr const char* idManual   = "manual";
    constexpr const char* idFeedback = "feedback";
    constexpr const char* idMix      = "mix";
    constexpr const char* idWidth    = "width";
    constexpr const char* idTone     = "tone";
    constexpr const char* idGain     = "gain";
    constexpr const char* idBypass   = "bypass";

    constexpr int kStagesValues[] = { 4, 6, 8, 12 };
}

juce::AudioProcessorValueTreeState::ParameterLayout
FP1AudioProcessor::createParameterLayout()
{
    using AP = juce::AudioParameterFloat;
    using AC = juce::AudioParameterChoice;
    using AB = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto rateRange = juce::NormalisableRange<float> (0.01f, 8.0f, 0.0f, 0.35f);
    auto toneRange = juce::NormalisableRange<float> (1000.0f, 18000.0f, 0.0f, 0.4f);

    layout.add (std::make_unique<AC> (juce::ParameterID { idMode, 1 }, "Mode",
                                      juce::StringArray { "Flanger", "Phaser", "Hybrid" }, 0));
    layout.add (std::make_unique<AC> (juce::ParameterID { idLfoShape, 1 }, "LFO Shape",
                                      juce::StringArray { "Sine", "Triangle", "Drift" }, 0));
    layout.add (std::make_unique<AC> (juce::ParameterID { idStages, 1 }, "Stages",
                                      juce::StringArray { "4", "6", "8", "12" }, 1));

    layout.add (std::make_unique<AP> (juce::ParameterID { idRate, 1 },     "Rate",     rateRange, 0.4f,
                                      juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idDepth, 1 },    "Depth",
                                      juce::NormalisableRange<float> (0.0f, 100.0f, 0.0f), 60.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idManual, 1 },   "Manual",
                                      juce::NormalisableRange<float> (0.0f, 100.0f, 0.0f), 40.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idFeedback, 1 }, "Feedback",
                                      juce::NormalisableRange<float> (-95.0f, 95.0f, 0.0f), 30.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idMix, 1 },      "Mix",
                                      juce::NormalisableRange<float> (0.0f, 100.0f, 0.0f), 50.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idWidth, 1 },    "Width",
                                      juce::NormalisableRange<float> (0.0f, 100.0f, 0.0f), 70.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idTone, 1 },     "Tone", toneRange, 9000.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AP> (juce::ParameterID { idGain, 1 },     "Output",
                                      juce::NormalisableRange<float> (-24.0f, 12.0f, 0.0f), 0.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AB> (juce::ParameterID { idBypass, 1 },   "Bypass", false));

    return layout;
}

//==============================================================================
FP1AudioProcessor::FP1AudioProcessor()
   #ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
   #else
    :
   #endif
      apvts (*this, nullptr, "FP1", createParameterLayout()),
      presetManager (apvts, "FP-1")
{
    pMode     = apvts.getRawParameterValue (idMode);
    pLfoShape = apvts.getRawParameterValue (idLfoShape);
    pStages   = apvts.getRawParameterValue (idStages);
    pRate     = apvts.getRawParameterValue (idRate);
    pDepth    = apvts.getRawParameterValue (idDepth);
    pManual   = apvts.getRawParameterValue (idManual);
    pFeedback = apvts.getRawParameterValue (idFeedback);
    pMix      = apvts.getRawParameterValue (idMix);
    pWidth    = apvts.getRawParameterValue (idWidth);
    pTone     = apvts.getRawParameterValue (idTone);
    pGain     = apvts.getRawParameterValue (idGain);
    pBypass   = apvts.getRawParameterValue (idBypass);
}

FP1AudioProcessor::~FP1AudioProcessor() = default;

//==============================================================================
const juce::String FP1AudioProcessor::getName() const          { return JucePlugin_Name; }
bool FP1AudioProcessor::acceptsMidi() const                    { return false; }
bool FP1AudioProcessor::producesMidi() const                   { return false; }
bool FP1AudioProcessor::isMidiEffect() const                   { return false; }
double FP1AudioProcessor::getTailLengthSeconds() const         { return 0.1; }

int FP1AudioProcessor::getNumPrograms()                        { return 1; }
int FP1AudioProcessor::getCurrentProgram()                     { return 0; }
void FP1AudioProcessor::setCurrentProgram (int)                {}
const juce::String FP1AudioProcessor::getProgramName (int)     { return {}; }
void FP1AudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void FP1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
    pullParametersToEngine();
}

void FP1AudioProcessor::releaseResources()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FP1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void FP1AudioProcessor::pullParametersToEngine()
{
    if (pMode     != nullptr) engine.setMode      (static_cast<FlangerPhaserEngine::Mode> ((int) pMode->load()));
    if (pLfoShape != nullptr) engine.setLfoShape  (static_cast<FlangerPhaserEngine::LfoShape> ((int) pLfoShape->load()));
    if (pStages   != nullptr)
    {
        const int idx = juce::jlimit (0, 3, (int) pStages->load());
        engine.setNumStages (kStagesValues[idx]);
    }
    if (pRate     != nullptr) engine.setRate         (pRate->load());
    if (pDepth    != nullptr) engine.setDepthPct     (pDepth->load());
    if (pManual   != nullptr) engine.setManualPct    (pManual->load());
    if (pFeedback != nullptr) engine.setFeedbackPct  (pFeedback->load());
    if (pMix      != nullptr) engine.setMixPct       (pMix->load());
    if (pWidth    != nullptr) engine.setWidthPct     (pWidth->load());
    if (pTone     != nullptr) engine.setToneHz       (pTone->load());
    if (pGain     != nullptr) engine.setOutputDb     (pGain->load());
}

void FP1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
bool FP1AudioProcessor::hasEditor() const                       { return true; }
juce::AudioProcessorEditor* FP1AudioProcessor::createEditor()   { return new FP1AudioProcessorEditor (*this); }

//==============================================================================
void FP1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FP1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new FP1AudioProcessor();
}
