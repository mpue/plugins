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
    constexpr auto predelay   = "predelay";
    constexpr auto lowcut     = "lowcut";
    constexpr auto highcut    = "highcut";
    constexpr auto modulation = "modulation";
    constexpr auto width      = "width";
    constexpr auto mix        = "mix";
    constexpr auto output     = "output";
}

juce::AudioProcessorValueTreeState::ParameterLayout CRV1AudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::size, 1 },     "Size",
        Range { 0.0f, 1.0f, 0.001f }, 0.50f));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::decay, 1 },    "Decay",
        Range { 0.0f, 1.0f, 0.001f }, 0.50f));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::predelay, 1 }, "Pre-Delay",
        Range { 0.0f, 500.0f, 0.1f }, 20.0f, "ms"));

    juce::NormalisableRange<float> lowRange (20.0f, 1000.0f, 1.0f);
    lowRange.setSkewForCentre (200.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::lowcut, 1 },   "Low Cut",
        lowRange, 90.0f, "Hz"));

    juce::NormalisableRange<float> hiRange (500.0f, 20000.0f, 1.0f);
    hiRange.setSkewForCentre (3000.0f);
    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::highcut, 1 },  "High Cut",
        hiRange, 10000.0f, "Hz"));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::modulation, 1 },"Modulation",
        Range { 0.0f, 1.0f, 0.001f }, 0.25f));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::width, 1 },    "Width",
        Range { 0.0f, 1.5f, 0.001f }, 1.0f));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::mix, 1 },      "Mix",
        Range { 0.0f, 1.0f, 0.001f }, 0.30f));

    p.push_back (std::make_unique<FloatParam> (
        juce::ParameterID { ParamIDs::output, 1 },   "Output",
        Range { -24.0f, 24.0f, 0.1f }, 0.0f, "dB"));

    return { p.begin(), p.end() };
}

//==============================================================================
CRV1AudioProcessor::CRV1AudioProcessor()
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
    for (auto id : { ParamIDs::size, ParamIDs::decay, ParamIDs::predelay,
                     ParamIDs::lowcut, ParamIDs::highcut, ParamIDs::modulation,
                     ParamIDs::width, ParamIDs::mix, ParamIDs::output })
    {
        apvts.addParameterListener (id, this);
    }

    presetManager.onIRChangeRequest = [this] (const juce::String& irName)
    {
        setSelectedIRName (irName);
    };

    selectedIRName = "Concert Hall";
}

CRV1AudioProcessor::~CRV1AudioProcessor()
{
    for (auto id : { ParamIDs::size, ParamIDs::decay, ParamIDs::predelay,
                     ParamIDs::lowcut, ParamIDs::highcut, ParamIDs::modulation,
                     ParamIDs::width, ParamIDs::mix, ParamIDs::output })
    {
        apvts.removeParameterListener (id, this);
    }

    cancelPendingUpdate();
}

void CRV1AudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == ParamIDs::size || parameterID == ParamIDs::decay)
    {
        irDirty.store (true);
        triggerAsyncUpdate();
    }
    else
    {
        engineParamsDirty.store (true);
    }
}

void CRV1AudioProcessor::updateEngineParameters()
{
    ConvolutionEngine::Parameters p;
    p.predelayMs = apvts.getRawParameterValue (ParamIDs::predelay)->load();
    p.lowCutHz   = apvts.getRawParameterValue (ParamIDs::lowcut)->load();
    p.highCutHz  = apvts.getRawParameterValue (ParamIDs::highcut)->load();
    p.modulation = apvts.getRawParameterValue (ParamIDs::modulation)->load();
    p.width      = apvts.getRawParameterValue (ParamIDs::width)->load();
    p.mix        = apvts.getRawParameterValue (ParamIDs::mix)->load();
    p.outputDb   = apvts.getRawParameterValue (ParamIDs::output)->load();
    engine.setParameters (p);
}

juce::String CRV1AudioProcessor::getSelectedIRName() const
{
    const juce::ScopedLock sl (const_cast<juce::CriticalSection&> (irNameLock));
    return selectedIRName;
}

void CRV1AudioProcessor::setSelectedIRName (const juce::String& name)
{
    {
        const juce::ScopedLock sl (irNameLock);
        if (selectedIRName == name) return;
        selectedIRName = name;
    }
    irDirty.store (true);
    triggerAsyncUpdate();
}

void CRV1AudioProcessor::rebuildAndLoadIR()
{
    juce::String irName = getSelectedIRName();
    int idx = irLibrary.findEntryByName (irName);
    if (idx < 0 && irLibrary.getNumEntries() > 0)
    {
        // Fall back to the first available entry
        idx = 0;
        setSelectedIRName (irLibrary.getEntries()[0].name);
    }
    if (idx < 0) return;

    const float sizeNorm  = apvts.getRawParameterValue (ParamIDs::size)->load();
    const float decayNorm = apvts.getRawParameterValue (ParamIDs::decay)->load();

    juce::String displayInfo;
    auto ir = irLibrary.renderIR (idx, sizeNorm, decayNorm, displayInfo);

    // Snapshot the callback under the lock so we can invoke it outside
    // (an editor-destruction tearing the std::function out from under us
    // mid-call would otherwise be a data race).
    IRRenderedFn fn;
    {
        const juce::ScopedLock sl (callbackLock);
        fn = onIRRendered;
    }
    if (fn) fn (ir, irLibrary.getSampleRate(), displayInfo);

    engine.loadImpulseResponse (std::move (ir), irLibrary.getSampleRate());
}

void CRV1AudioProcessor::setOnIRRendered (IRRenderedFn fn)
{
    const juce::ScopedLock sl (callbackLock);
    onIRRendered = std::move (fn);
}

void CRV1AudioProcessor::requestIRRefresh()
{
    irDirty.store (true);
    triggerAsyncUpdate();
}

void CRV1AudioProcessor::handleAsyncUpdate()
{
    if (irDirty.exchange (false))
        rebuildAndLoadIR();
}

//==============================================================================
const juce::String CRV1AudioProcessor::getName() const                { return JucePlugin_Name; }

bool CRV1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool CRV1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool CRV1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double CRV1AudioProcessor::getTailLengthSeconds() const               { return 25.0; }

int CRV1AudioProcessor::getNumPrograms()                              { return 1; }
int CRV1AudioProcessor::getCurrentProgram()                           { return 0; }
void CRV1AudioProcessor::setCurrentProgram (int)                       {}
const juce::String CRV1AudioProcessor::getProgramName (int)            { return {}; }
void CRV1AudioProcessor::changeProgramName (int, const juce::String&)  {}

//==============================================================================
void CRV1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    irLibrary.initialise (sampleRate);

    updateEngineParameters();
    engineParamsDirty.store (false);

    rebuildAndLoadIR();
    irDirty.store (false);

    presetManager.initialise();
}

void CRV1AudioProcessor::releaseResources()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CRV1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void CRV1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (engineParamsDirty.exchange (false))
        updateEngineParameters();

    engine.process (buffer);
}

//==============================================================================
bool CRV1AudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* CRV1AudioProcessor::createEditor()
{
    return new CRV1AudioProcessorEditor (*this);
}

//==============================================================================
void CRV1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("crv1_ir", getSelectedIRName(), nullptr);
    state.setProperty ("crv1_preset", presetManager.getCurrentPresetName(), nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void CRV1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        juce::String irName = state.getProperty ("crv1_ir", "Concert Hall").toString();
        apvts.replaceState (state);

        setSelectedIRName (irName);
        engineParamsDirty.store (true);
        irDirty.store (true);
        triggerAsyncUpdate();
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CRV1AudioProcessor();
}
