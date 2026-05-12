/*
  ==============================================================================

    PluginProcessor.cpp
    SN-1 Luxury Snare Machine

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
SN1AudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using Attr       = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---------- TONE / BODY ----------
    {
        juce::NormalisableRange<float> r (90.0f, 400.0f, 0.1f);
        r.setSkewForCentre (200.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "tune", 1 }, "Tune", r, 200.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String (v, 1) + " Hz"; })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "pitchAmt", 1 }, "Pitch",
        juce::NormalisableRange<float> (0.0f, 36.0f, 0.1f), 8.0f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return "+" + juce::String (v, 1) + " st"; })));

    {
        juce::NormalisableRange<float> r (1.0f, 80.0f, 0.1f);
        r.setSkewForCentre (15.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "pitchTime", 1 }, "Pitch Time", r, 12.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String (v, 1) + " ms"; })));
    }

    {
        juce::NormalisableRange<float> r (20.0f, 600.0f, 1.0f);
        r.setSkewForCentre (130.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "bodyDecay", 1 }, "Decay", r, 110.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String ((int) v) + " ms"; })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "bodyLevel", 1 }, "Body",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.65f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    // ---------- SNARES / NOISE ----------
    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "noiseLevel", 1 }, "Sizzle",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    {
        juce::NormalisableRange<float> r (600.0f, 12000.0f, 1.0f);
        r.setSkewForCentre (3500.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "noiseColor", 1 }, "Color", r, 4500.0f,
            Attr().withStringFromValueFunction (
                [](float v, int)
                {
                    if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                    return juce::String ((int) std::round (v)) + " Hz";
                })));
    }

    {
        juce::NormalisableRange<float> r (40.0f, 900.0f, 1.0f);
        r.setSkewForCentre (220.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "noiseDecay", 1 }, "Tail", r, 220.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String ((int) v) + " ms"; })));
    }

    {
        juce::NormalisableRange<float> r (0.4f, 8.0f, 0.01f);
        r.setSkewForCentre (1.6f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "buzzQ", 1 }, "Buzz", r, 1.6f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String (v, 2) + " Q"; })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "snapLevel", 1 }, "Snap",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    // ---------- MASTER ----------
    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.18f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "tone", 1 }, "Tone",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int)
            {
                if (std::abs (v) < 0.01f) return juce::String ("Flat");
                return juce::String (v >= 0.0f ? "+" : "") + juce::String (v * 9.0f, 1) + " dB";
            })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "output", 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String (v, 1) + " dB"; })));

    return layout;
}

//==============================================================================
SN1AudioProcessor::SN1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "SN1State", createParameterLayout()),
       presetManager (apvts)
#else
     : apvts (*this, nullptr, "SN1State", createParameterLayout()),
       presetManager (apvts)
#endif
{
    tuneParam       = apvts.getRawParameterValue ("tune");
    pitchAmtParam   = apvts.getRawParameterValue ("pitchAmt");
    pitchTimeParam  = apvts.getRawParameterValue ("pitchTime");
    bodyDecayParam  = apvts.getRawParameterValue ("bodyDecay");
    bodyLevelParam  = apvts.getRawParameterValue ("bodyLevel");
    noiseLevelParam = apvts.getRawParameterValue ("noiseLevel");
    noiseColorParam = apvts.getRawParameterValue ("noiseColor");
    noiseDecayParam = apvts.getRawParameterValue ("noiseDecay");
    buzzQParam      = apvts.getRawParameterValue ("buzzQ");
    snapLevelParam  = apvts.getRawParameterValue ("snapLevel");
    driveParam      = apvts.getRawParameterValue ("drive");
    toneParam       = apvts.getRawParameterValue ("tone");
    outputParam     = apvts.getRawParameterValue ("output");
}

SN1AudioProcessor::~SN1AudioProcessor() = default;

//==============================================================================
const juce::String SN1AudioProcessor::getName() const            { return JucePlugin_Name; }
bool   SN1AudioProcessor::acceptsMidi() const                    { return true;  }
bool   SN1AudioProcessor::producesMidi() const                   { return false; }
bool   SN1AudioProcessor::isMidiEffect() const                   { return false; }
double SN1AudioProcessor::getTailLengthSeconds() const           { return 2.0; }

int    SN1AudioProcessor::getNumPrograms()                                           { return 1; }
int    SN1AudioProcessor::getCurrentProgram()                                        { return 0; }
void   SN1AudioProcessor::setCurrentProgram (int)                                    {}
const  juce::String SN1AudioProcessor::getProgramName (int)                          { return {}; }
void   SN1AudioProcessor::changeProgramName (int, const juce::String&)               {}

//==============================================================================
void SN1AudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    voice.prepare (sampleRate);
    voice.reset();
}

void SN1AudioProcessor::releaseResources()
{
    voice.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SN1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

//==============================================================================
SnareVoice::Params SN1AudioProcessor::buildParamsSnapshot() const noexcept
{
    SnareVoice::Params p;
    if (tuneParam)       p.tuneHz        = tuneParam->load();
    if (pitchAmtParam)   p.pitchAmtSemis = pitchAmtParam->load();
    if (pitchTimeParam)  p.pitchTimeMs   = pitchTimeParam->load();
    if (bodyDecayParam)  p.bodyDecayMs   = bodyDecayParam->load();
    if (bodyLevelParam)  p.bodyLevel     = bodyLevelParam->load();
    if (noiseLevelParam) p.noiseLevel    = noiseLevelParam->load();
    if (noiseColorParam) p.noiseColorHz  = noiseColorParam->load();
    if (noiseDecayParam) p.noiseDecayMs  = noiseDecayParam->load();
    if (buzzQParam)      p.buzzQ         = buzzQParam->load();
    if (snapLevelParam)  p.snapLevel     = snapLevelParam->load();
    if (driveParam)      p.drive         = driveParam->load();
    if (toneParam)       p.tone          = toneParam->load();
    if (outputParam)     p.outputGainLin = juce::Decibels::decibelsToGain (outputParam->load());
    return p;
}

void SN1AudioProcessor::requestAudition (float velocity) noexcept
{
    auditionVelocity.store (juce::jlimit (0.05f, 1.0f, velocity));
    auditionPending.fetch_add (1, std::memory_order_release);
}

int SN1AudioProcessor::consumeTriggerEvent (float& velocityOut) noexcept
{
    const int now    = triggerCounter.load (std::memory_order_acquire);
    const int delta  = now - lastSeenTrigger;
    lastSeenTrigger  = now;
    velocityOut      = lastTriggerVel.load (std::memory_order_relaxed);
    return delta;
}

//==============================================================================
void SN1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0)
        return;

    auto fireTrigger = [this] (float vel)
    {
        const auto p = buildParamsSnapshot();
        voice.trigger (p, vel);
        lastTriggerVel.store (vel, std::memory_order_relaxed);
        triggerCounter.fetch_add (1, std::memory_order_release);
    };

    int pending = auditionPending.exchange (0, std::memory_order_acquire);
    if (pending > 0)
        fireTrigger (auditionVelocity.load (std::memory_order_relaxed));

    auto midiIt = midi.begin();
    for (int n = 0; n < numSamples; ++n)
    {
        while (midiIt != midi.end())
        {
            const auto meta = *midiIt;
            if (meta.samplePosition > n)
                break;

            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                const float vel = juce::jlimit (0.05f, 1.0f,
                                                 (float) m.getFloatVelocity());
                fireTrigger (vel);
            }
            ++midiIt;
        }

        const float s = voice.renderSample();
        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, n, s);
    }
}

//==============================================================================
bool SN1AudioProcessor::hasEditor() const                        { return true; }
juce::AudioProcessorEditor* SN1AudioProcessor::createEditor()    { return new SN1AudioProcessorEditor (*this); }

//==============================================================================
void SN1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", presetManager.getCurrentPresetName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SN1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            const auto presetName = state.getProperty ("currentPreset", "Init").toString();
            apvts.replaceState (state);
            presetManager.setCurrentPresetNameForRestore (presetName);
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SN1AudioProcessor();
}
