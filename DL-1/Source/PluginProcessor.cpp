/*
  ==============================================================================

    PluginProcessor.cpp
    DL-1 — Luxury Stereo Delay

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetManager.h"

namespace
{
    static const juce::StringArray syncDivisionNames
    {
        "1/64", "1/32", "1/16T", "1/16", "1/16D",
        "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D",
        "1/2", "1/2D", "1/1"
    };

    static const std::array<double, 14> syncDivisionBeats
    {
        0.0625, 0.125, 1.0/6.0, 0.25, 0.375,
        1.0/3.0, 0.5, 0.75, 2.0/3.0, 1.0, 1.5,
        2.0, 3.0, 4.0
    };
}

const juce::StringArray& DL1AudioProcessor::getSyncDivisionNames()
{
    return syncDivisionNames;
}

double DL1AudioProcessor::getSyncDivisionBeats(int index)
{
    if (index < 0 || index >= (int)syncDivisionBeats.size()) return 1.0;
    return syncDivisionBeats[(size_t)index];
}

juce::AudioProcessorValueTreeState::ParameterLayout DL1AudioProcessor::createParameterLayout()
{
    using F = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using C = juce::AudioParameterChoice;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back(std::make_unique<F>("inGain",  "Input Gain",  juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));
    p.push_back(std::make_unique<F>("outGain", "Output Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));

    p.push_back(std::make_unique<F>("timeL",   "Time L",      juce::NormalisableRange<float>(1.0f, 2000.0f, 0.01f, 0.4f), 350.0f));
    p.push_back(std::make_unique<F>("timeR",   "Time R",      juce::NormalisableRange<float>(1.0f, 2000.0f, 0.01f, 0.4f), 525.0f));

    p.push_back(std::make_unique<B>("linkTimes", "Link Times", false));
    p.push_back(std::make_unique<B>("sync",      "Tempo Sync", false));

    p.push_back(std::make_unique<C>("divisionL", "Division L", syncDivisionNames, 6));
    p.push_back(std::make_unique<C>("divisionR", "Division R", syncDivisionNames, 6));

    p.push_back(std::make_unique<F>("feedback",  "Feedback",   juce::NormalisableRange<float>(0.0f, 1.10f, 0.0001f), 0.40f));
    p.push_back(std::make_unique<F>("crossfeed", "Crossfeed",  juce::NormalisableRange<float>(0.0f, 1.0f,  0.0001f), 0.0f));

    p.push_back(std::make_unique<F>("highCut",   "High Cut",   juce::NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.3f), 10000.0f));
    p.push_back(std::make_unique<F>("lowCut",    "Low Cut",    juce::NormalisableRange<float>(20.0f,  2000.0f,  1.0f, 0.3f), 80.0f));

    p.push_back(std::make_unique<F>("modRate",   "Mod Rate",   juce::NormalisableRange<float>(0.05f, 8.0f, 0.001f, 0.4f), 0.30f));
    p.push_back(std::make_unique<F>("modDepth",  "Mod Depth",  juce::NormalisableRange<float>(0.0f,  1.0f, 0.0001f),       0.10f));

    p.push_back(std::make_unique<F>("drive",     "Drive",      juce::NormalisableRange<float>(0.0f,  1.0f, 0.0001f), 0.10f));
    p.push_back(std::make_unique<F>("width",     "Width",      juce::NormalisableRange<float>(0.0f,  2.0f, 0.0001f), 1.0f));
    p.push_back(std::make_unique<F>("ducking",   "Ducking",    juce::NormalisableRange<float>(0.0f,  1.0f, 0.0001f), 0.0f));
    p.push_back(std::make_unique<F>("mix",       "Mix",        juce::NormalisableRange<float>(0.0f,  1.0f, 0.0001f), 0.35f));

    return { p.begin(), p.end() };
}

//==============================================================================
DL1AudioProcessor::DL1AudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts (*this, nullptr, "DL1", createParameterLayout())
{
    presetManager = std::make_unique<PresetManager>(apvts);
}

DL1AudioProcessor::~DL1AudioProcessor() = default;

const juce::String DL1AudioProcessor::getName() const                 { return JucePlugin_Name; }
bool DL1AudioProcessor::acceptsMidi() const                           { return false; }
bool DL1AudioProcessor::producesMidi() const                          { return false; }
bool DL1AudioProcessor::isMidiEffect() const                          { return false; }
double DL1AudioProcessor::getTailLengthSeconds() const                { return 4.0; }
int DL1AudioProcessor::getNumPrograms()                               { return 1; }
int DL1AudioProcessor::getCurrentProgram()                            { return 0; }
void DL1AudioProcessor::setCurrentProgram (int)                       { }
const juce::String DL1AudioProcessor::getProgramName (int)            { return {}; }
void DL1AudioProcessor::changeProgramName (int, const juce::String&)  { }

//==============================================================================
void DL1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
}

void DL1AudioProcessor::releaseResources()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DL1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void DL1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Tempo from host
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                currentBpm = *bpm;
    }

    auto getF = [&](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    auto getB = [&](const char* id) { return apvts.getRawParameterValue(id)->load() > 0.5f; };
    auto getI = [&](const char* id) { return (int)apvts.getRawParameterValue(id)->load(); };

    const bool  sync      = getB("sync");
    const bool  link      = getB("linkTimes");
    float       timeL     = getF("timeL");
    float       timeR     = getF("timeR");

    if (sync)
    {
        const double beatsL = getSyncDivisionBeats(getI("divisionL"));
        const double beatsR = getSyncDivisionBeats(getI("divisionR"));
        const double msPerBeat = 60000.0 / juce::jmax(20.0, currentBpm);
        timeL = (float)(beatsL * msPerBeat);
        timeR = (float)(beatsR * msPerBeat);
    }

    if (link)
        timeR = timeL;

    timeL = juce::jlimit(1.0f, 2500.0f, timeL);
    timeR = juce::jlimit(1.0f, 2500.0f, timeR);

    engine.setParameters(
        getF("inGain"),  getF("outGain"),
        timeL,           timeR,
        getF("feedback"),getF("crossfeed"),
        getF("lowCut"),  getF("highCut"),
        getF("modRate"), getF("modDepth"),
        getF("drive"),   getF("width"),
        getF("ducking"), getF("mix"));

    engine.process(buffer);
}

//==============================================================================
bool DL1AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* DL1AudioProcessor::createEditor()
{
    return new DL1AudioProcessorEditor (*this);
}

//==============================================================================
void DL1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    if (xml != nullptr)
    {
        xml->setAttribute("dl1_currentPreset", presetManager->getCurrentPresetName());
        copyXmlToBinary(*xml, destData);
    }
}

void DL1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        juce::String name = xml->getStringAttribute("dl1_currentPreset", "Init");
        xml->removeAttribute("dl1_currentPreset");

        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));

        if (presetManager)
            presetManager->setCurrentNameAfterStateRestore(name);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DL1AudioProcessor();
}
