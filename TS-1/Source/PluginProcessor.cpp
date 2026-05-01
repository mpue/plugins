/*
  ==============================================================================

    TS-1 Transient Shaper – Processor implementation

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace IDs
{
    static const juce::String attack       { "attack" };
    static const juce::String sustain      { "sustain" };
    static const juce::String output       { "output" };
    static const juce::String mix          { "mix" };
    static const juce::String sensitivity  { "sensitivity" };
    static const juce::String bypass       { "bypass" };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TS1AudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { IDs::attack, 1 }, "Attack",
        NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { IDs::sustain, 1 }, "Sustain",
        NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { IDs::sensitivity, 1 }, "Sensitivity",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { IDs::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { IDs::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterBool>(
        ParameterID { IDs::bypass, 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

//==============================================================================
TS1AudioProcessor::TS1AudioProcessor()
   #ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        #if ! JucePlugin_IsMidiEffect
                         #if ! JucePlugin_IsSynth
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                         #endif
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        #endif
                          ),
   #endif
      parameters (*this, nullptr, juce::Identifier ("TS1State"), createParameterLayout())
{
    attackParam      = parameters.getRawParameterValue (IDs::attack);
    sustainParam     = parameters.getRawParameterValue (IDs::sustain);
    outputParam      = parameters.getRawParameterValue (IDs::output);
    mixParam         = parameters.getRawParameterValue (IDs::mix);
    sensitivityParam = parameters.getRawParameterValue (IDs::sensitivity);
    bypassParam      = parameters.getRawParameterValue (IDs::bypass);

    envHistory.fill (0.0f);
    gainHistory.fill (0.0f);
}

TS1AudioProcessor::~TS1AudioProcessor() = default;

//==============================================================================
const juce::String TS1AudioProcessor::getName() const  { return JucePlugin_Name; }
bool   TS1AudioProcessor::acceptsMidi() const          { return false; }
bool   TS1AudioProcessor::producesMidi() const         { return false; }
bool   TS1AudioProcessor::isMidiEffect() const         { return false; }
double TS1AudioProcessor::getTailLengthSeconds() const { return 0.0; }
int    TS1AudioProcessor::getNumPrograms()             { return 1; }
int    TS1AudioProcessor::getCurrentProgram()          { return 0; }
void   TS1AudioProcessor::setCurrentProgram (int)      {}
const juce::String TS1AudioProcessor::getProgramName (int)              { return {}; }
void   TS1AudioProcessor::changeProgramName (int, const juce::String&)  {}

//==============================================================================
void TS1AudioProcessor::computeEnvelopeCoeffs (double sampleRate)
{
    auto coeff = [sampleRate] (float ms)
    {
        return std::exp (-1.0f / ((float) sampleRate * (ms * 0.001f)));
    };

    fastAttCoeff = coeff (0.5f);
    fastRelCoeff = coeff (40.0f);
    slowAttCoeff = coeff (20.0f);
    slowRelCoeff = coeff (180.0f);

    wfDecimateN = juce::jmax (1, (int) std::round (sampleRate / 11000.0));
}

void TS1AudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    computeEnvelopeCoeffs (sampleRate);
    for (auto& s : envState) { s.fastEnv = 0.0f; s.slowEnv = 0.0f; }

    outputGainSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() * 0.01f);

    inputLevel.store (0.0f);
    outputLevel.store (0.0f);
    gainChangeDb.store (0.0f);
    transientActivity.store (0.0f);
}

void TS1AudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TS1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

//==============================================================================
void TS1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    if (numChannels == 0 || numSamples == 0) return;

    const bool  bypassed   = bypassParam->load() > 0.5f;
    const float attack01   = attackParam->load()  * 0.01f;             // -1..+1
    const float sustain01  = sustainParam->load() * 0.01f;             // -1..+1
    const float sensitivity = juce::jmax (0.05f,
                                          sensitivityParam->load() * 0.04f); // 0..4

    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    mixSmoothed.setTargetValue (mixParam->load() * 0.01f);

    const int detectChannels = juce::jmin (numChannels, 2);
    auto& s = envState[0];

    double inAcc = 0.0, outAcc = 0.0;
    int meterCount = 0;
    float maxAbsGainBlock = 0.0f;
    float maxTransBlock   = 0.0f;
    float lastGainDb      = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // -------- Detection signal (mono sum of |x|) ----------
        float detect = 0.0f;
        for (int c = 0; c < detectChannels; ++c)
            detect += std::abs (buffer.getReadPointer (c)[i]);
        detect /= (float) detectChannels;

        // -------- Dual envelope follower (attack/release) ----
        if (detect > s.fastEnv) s.fastEnv = fastAttCoeff * (s.fastEnv - detect) + detect;
        else                    s.fastEnv = fastRelCoeff * (s.fastEnv - detect) + detect;

        if (detect > s.slowEnv) s.slowEnv = slowAttCoeff * (s.slowEnv - detect) + detect;
        else                    s.slowEnv = slowRelCoeff * (s.slowEnv - detect) + detect;

        const float fastE = s.fastEnv;
        const float slowE = juce::jmax (s.slowEnv, 1.0e-6f);
        const float ratio = fastE / slowE;

        // Transient = fastEnv exceeds slowEnv
        const float transientFactor = juce::jlimit (0.0f, 1.0f, (ratio - 1.0f) * sensitivity);
        // Sustain = slow envelope active and not transient
        const float sustainFactor   = (1.0f - transientFactor)
                                      * juce::jlimit (0.0f, 1.0f, slowE * 8.0f);

        const float attackDb  = transientFactor * attack01  * 18.0f;
        const float sustainDb = sustainFactor   * sustain01 * 12.0f;
        const float gainDb    = attackDb + sustainDb;
        const float gainLin   = std::pow (10.0f, gainDb * (1.0f / 20.0f));

        if (std::abs (gainDb) > std::abs (maxAbsGainBlock)) maxAbsGainBlock = gainDb;
        if (transientFactor > maxTransBlock) maxTransBlock = transientFactor;
        lastGainDb = gainDb;

        const float outGain = outputGainSmoothed.getNextValue();
        const float mix     = mixSmoothed.getNextValue();

        for (int c = 0; c < numChannels; ++c)
        {
            float* d = buffer.getWritePointer (c);
            const float dry = d[i];

            if (! bypassed)
            {
                const float wet = dry * gainLin;
                const float out = ((1.0f - mix) * dry + mix * wet) * outGain;
                d[i] = out;
                inAcc  += (double) dry * dry;
                outAcc += (double) out * out;
            }
            else
            {
                inAcc  += (double) dry * dry;
                outAcc += (double) dry * dry;
            }
        }
        meterCount += numChannels;

        // -------- Waveform / gain history (decimated) --------
        if (++wfDecimator >= wfDecimateN)
        {
            wfDecimator = 0;
            const int idx = wfWriteIdx.load (std::memory_order_relaxed);
            envHistory[(size_t) idx]  = fastE;
            gainHistory[(size_t) idx] = bypassed ? 0.0f : gainDb;
            wfWriteIdx.store ((idx + 1) % waveformSize, std::memory_order_release);
        }
    }

    if (meterCount > 0)
    {
        const float inRMS  = (float) std::sqrt (inAcc  / (double) meterCount);
        const float outRMS = (float) std::sqrt (outAcc / (double) meterCount);

        // Asymmetric smoothing for nice meter feel (fast attack, slow release)
        auto smoothMeter = [] (std::atomic<float>& a, float target)
        {
            const float prev = a.load();
            const float alpha = (target > prev) ? 0.6f : 0.12f;
            a.store (alpha * target + (1.0f - alpha) * prev);
        };

        smoothMeter (inputLevel,  inRMS);
        smoothMeter (outputLevel, outRMS);
        smoothMeter (transientActivity, maxTransBlock);

        // Gain change: slightly slower so the meter stays readable
        const float prev = gainChangeDb.load();
        gainChangeDb.store (0.5f * maxAbsGainBlock + 0.5f * prev);

        juce::ignoreUnused (lastGainDb);
    }
}

//==============================================================================
bool TS1AudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* TS1AudioProcessor::createEditor()
{
    return new TS1AudioProcessorEditor (*this);
}

//==============================================================================
void TS1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("currentPresetName", currentPresetName, nullptr);
    if (auto xml = std::unique_ptr<juce::XmlElement> (state.createXml()))
        copyXmlToBinary (*xml, destData);
}

void TS1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = std::unique_ptr<juce::XmlElement> (getXmlFromBinary (data, sizeInBytes)))
    {
        if (xml->hasTagName (parameters.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            if (state.hasProperty ("currentPresetName"))
                currentPresetName = state.getProperty ("currentPresetName").toString();
            parameters.replaceState (state);
        }
    }
}

//==============================================================================
void TS1AudioProcessor::copyWaveformSnapshot (float* envOut, float* gainDbOut, int numSamples) const
{
    const int idx = wfWriteIdx.load (std::memory_order_acquire);
    const int n   = juce::jmin (numSamples, (int) waveformSize);

    for (int i = 0; i < n; ++i)
    {
        const int srcIdx = (idx + i) % waveformSize;
        if (envOut)    envOut[i]    = envHistory[(size_t) srcIdx];
        if (gainDbOut) gainDbOut[i] = gainHistory[(size_t) srcIdx];
    }
}

//==============================================================================
juce::File TS1AudioProcessor::getPresetDirectory() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("TS-1").getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::StringArray TS1AudioProcessor::getFactoryPresetNames() const
{
    return { "Init",
             "Punchy Drums",
             "Soft Snare",
             "Tight Kick",
             "Acoustic Body",
             "Electric Pluck",
             "Drum Bus Glue",
             "Vocal Air",
             "Aggressive Stab",
             "Smooth Tame" };
}

juce::StringArray TS1AudioProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    auto dir = getPresetDirectory();
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, "*.xml");
    for (auto& f : files)
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

juce::String TS1AudioProcessor::getFactoryPresetXml (const juce::String& name) const
{
    auto build = [] (float a, float s, float o, float m, float sens) -> juce::String
    {
        return juce::String ("<TS1State>")
             + "<PARAM id=\"attack\" value=\""      + juce::String (a, 2)    + "\"/>"
             + "<PARAM id=\"sustain\" value=\""     + juce::String (s, 2)    + "\"/>"
             + "<PARAM id=\"sensitivity\" value=\"" + juce::String (sens, 2) + "\"/>"
             + "<PARAM id=\"output\" value=\""      + juce::String (o, 2)    + "\"/>"
             + "<PARAM id=\"mix\" value=\""         + juce::String (m, 2)    + "\"/>"
             + "<PARAM id=\"bypass\" value=\"0\"/>"
             + "</TS1State>";
    };

    if (name == "Init")             return build (  0.0f,   0.0f,  0.0f, 100.0f, 50.0f);
    if (name == "Punchy Drums")     return build ( 55.0f, -25.0f,  0.0f, 100.0f, 65.0f);
    if (name == "Soft Snare")       return build (-30.0f,  35.0f,  0.0f, 100.0f, 55.0f);
    if (name == "Tight Kick")       return build ( 70.0f, -55.0f, -2.0f, 100.0f, 75.0f);
    if (name == "Acoustic Body")    return build (-15.0f,  35.0f,  0.0f,  85.0f, 50.0f);
    if (name == "Electric Pluck")   return build ( 50.0f, -10.0f,  0.0f, 100.0f, 65.0f);
    if (name == "Drum Bus Glue")    return build ( 20.0f,  15.0f, -1.0f,  90.0f, 45.0f);
    if (name == "Vocal Air")        return build ( 15.0f,  25.0f,  0.0f, 100.0f, 40.0f);
    if (name == "Aggressive Stab")  return build ( 80.0f, -40.0f,  0.0f, 100.0f, 80.0f);
    if (name == "Smooth Tame")      return build (-50.0f,  20.0f,  1.0f, 100.0f, 45.0f);
    return build (0.0f, 0.0f, 0.0f, 100.0f, 50.0f);
}

bool TS1AudioProcessor::loadPreset (const juce::String& name, bool isFactory)
{
    juce::String xmlText;
    if (isFactory)
    {
        xmlText = getFactoryPresetXml (name);
    }
    else
    {
        auto file = getPresetDirectory().getChildFile (name + ".xml");
        if (! file.existsAsFile()) return false;
        xmlText = file.loadFileAsString();
    }

    auto xml = juce::parseXML (xmlText);
    if (xml == nullptr) return false;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return false;
    if (state.getType() != parameters.state.getType()) return false;

    parameters.replaceState (state);
    currentPresetName = name;
    return true;
}

bool TS1AudioProcessor::savePreset (const juce::String& name)
{
    if (name.isEmpty()) return false;
    auto file = getPresetDirectory().getChildFile (name + ".xml");

    auto state = parameters.copyState();
    if (auto xml = std::unique_ptr<juce::XmlElement> (state.createXml()))
    {
        if (xml->writeTo (file))
        {
            currentPresetName = name;
            return true;
        }
    }
    return false;
}

bool TS1AudioProcessor::deletePreset (const juce::String& name)
{
    auto file = getPresetDirectory().getChildFile (name + ".xml");
    return file.deleteFile();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TS1AudioProcessor();
}
