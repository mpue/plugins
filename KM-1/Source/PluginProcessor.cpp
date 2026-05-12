/*
  ==============================================================================

    PluginProcessor.cpp
    KM-1 Luxury Kick Machine

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
KM1AudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using Attr       = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    {
        juce::NormalisableRange<float> r (30.0f, 150.0f, 0.1f);
        r.setSkewForCentre (60.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "tune", 1 }, "Tune", r, 50.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String (v, 1) + " Hz"; })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "pitchAmt", 1 }, "Pitch Amount",
        juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 36.0f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return "+" + juce::String (v, 1) + " st"; })));

    {
        juce::NormalisableRange<float> r (1.0f, 200.0f, 0.1f);
        r.setSkewForCentre (30.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "pitchTime", 1 }, "Pitch Time", r, 35.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String (v, 1) + " ms"; })));
    }

    {
        juce::NormalisableRange<float> r (30.0f, 2000.0f, 1.0f);
        r.setSkewForCentre (400.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "bodyDecay", 1 }, "Decay", r, 500.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String ((int) v) + " ms"; })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "bodyShape", 1 }, "Shape",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int)
            {
                if (v < 0.05f) return juce::String ("Sine");
                if (v > 0.95f) return juce::String ("Tri");
                return juce::String ((int) std::round (v * 100.0f)) + " %";
            })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "clickLevel", 1 }, "Click",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    {
        juce::NormalisableRange<float> r (200.0f, 8000.0f, 1.0f);
        r.setSkewForCentre (1500.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "clickTone", 1 }, "Click Tone", r, 2200.0f,
            Attr().withStringFromValueFunction (
                [](float v, int)
                {
                    if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                    return juce::String ((int) std::round (v)) + " Hz";
                })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "subLevel", 1 }, "Sub",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.45f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.20f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "punch", 1 }, "Punch",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.30f,
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
KM1AudioProcessor::KM1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "KM1State", createParameterLayout()),
       presetManager (apvts)
#else
     : apvts (*this, nullptr, "KM1State", createParameterLayout()),
       presetManager (apvts)
#endif
{
    tuneParam       = apvts.getRawParameterValue ("tune");
    pitchAmtParam   = apvts.getRawParameterValue ("pitchAmt");
    pitchTimeParam  = apvts.getRawParameterValue ("pitchTime");
    bodyDecayParam  = apvts.getRawParameterValue ("bodyDecay");
    bodyShapeParam  = apvts.getRawParameterValue ("bodyShape");
    clickLevelParam = apvts.getRawParameterValue ("clickLevel");
    clickToneParam  = apvts.getRawParameterValue ("clickTone");
    subLevelParam   = apvts.getRawParameterValue ("subLevel");
    driveParam      = apvts.getRawParameterValue ("drive");
    punchParam      = apvts.getRawParameterValue ("punch");
    toneParam       = apvts.getRawParameterValue ("tone");
    outputParam     = apvts.getRawParameterValue ("output");
}

KM1AudioProcessor::~KM1AudioProcessor() = default;

//==============================================================================
const juce::String KM1AudioProcessor::getName() const            { return JucePlugin_Name; }
bool   KM1AudioProcessor::acceptsMidi() const                    { return true;  }
bool   KM1AudioProcessor::producesMidi() const                   { return false; }
bool   KM1AudioProcessor::isMidiEffect() const                   { return false; }
double KM1AudioProcessor::getTailLengthSeconds() const           { return 2.0; }

int    KM1AudioProcessor::getNumPrograms()                                           { return 1; }
int    KM1AudioProcessor::getCurrentProgram()                                        { return 0; }
void   KM1AudioProcessor::setCurrentProgram (int)                                    {}
const  juce::String KM1AudioProcessor::getProgramName (int)                          { return {}; }
void   KM1AudioProcessor::changeProgramName (int, const juce::String&)               {}

//==============================================================================
void KM1AudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    voice.prepare (sampleRate);
    voice.reset();
}

void KM1AudioProcessor::releaseResources()
{
    voice.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KM1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

//==============================================================================
KickVoice::Params KM1AudioProcessor::buildParamsSnapshot() const noexcept
{
    KickVoice::Params p;
    if (tuneParam)       p.tuneHz        = tuneParam->load();
    if (pitchAmtParam)   p.pitchAmtSemis = pitchAmtParam->load();
    if (pitchTimeParam)  p.pitchTimeMs   = pitchTimeParam->load();
    if (bodyDecayParam)  p.bodyDecayMs   = bodyDecayParam->load();
    if (bodyShapeParam)  p.bodyShape     = bodyShapeParam->load();
    if (clickLevelParam) p.clickLevel    = clickLevelParam->load();
    if (clickToneParam)  p.clickToneHz   = clickToneParam->load();
    if (subLevelParam)   p.subLevel      = subLevelParam->load();
    if (driveParam)      p.drive         = driveParam->load();
    if (punchParam)      p.punch         = punchParam->load();
    if (toneParam)       p.tone          = toneParam->load();
    if (outputParam)     p.outputGainLin = juce::Decibels::decibelsToGain (outputParam->load());
    return p;
}

void KM1AudioProcessor::requestAudition (float velocity) noexcept
{
    auditionVelocity.store (juce::jlimit (0.05f, 1.0f, velocity));
    auditionPending.fetch_add (1, std::memory_order_release);
}

int KM1AudioProcessor::consumeTriggerEvent (float& velocityOut) noexcept
{
    const int now    = triggerCounter.load (std::memory_order_acquire);
    const int delta  = now - lastSeenTrigger;
    lastSeenTrigger  = now;
    velocityOut      = lastTriggerVel.load (std::memory_order_relaxed);
    return delta;
}

//==============================================================================
void KM1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    // ---- consume audition requests from UI ----
    int pending = auditionPending.exchange (0, std::memory_order_acquire);
    if (pending > 0)
        fireTrigger (auditionVelocity.load (std::memory_order_relaxed));

    // ---- render with sample-accurate MIDI note-on triggers ----
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
bool KM1AudioProcessor::hasEditor() const                        { return true; }
juce::AudioProcessorEditor* KM1AudioProcessor::createEditor()    { return new KM1AudioProcessorEditor (*this); }

//==============================================================================
void KM1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", presetManager.getCurrentPresetName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void KM1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new KM1AudioProcessor();
}
