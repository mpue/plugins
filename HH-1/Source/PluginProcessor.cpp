/*
  ==============================================================================

    PluginProcessor.cpp
    HH-1 Luxury Hi-Hat Machine

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
HH1AudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using Attr       = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---------- METAL / BODY ----------
    {
        juce::NormalisableRange<float> r (200.0f, 2000.0f, 0.1f);
        r.setSkewForCentre (800.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "tune", 1 }, "Tune", r, 800.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String ((int) v) + " Hz"; })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "metal", 1 }, "Metal",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "harmonics", 1 }, "Spread",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    {
        juce::NormalisableRange<float> r (1500.0f, 12000.0f, 1.0f);
        r.setSkewForCentre (6500.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "hpCut", 1 }, "HP Cut", r, 6500.0f,
            Attr().withStringFromValueFunction (
                [](float v, int)
                {
                    if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                    return juce::String ((int) std::round (v)) + " Hz";
                })));
    }

    {
        juce::NormalisableRange<float> r (3000.0f, 16000.0f, 1.0f);
        r.setSkewForCentre (9000.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "bpCut", 1 }, "Bright", r, 9000.0f,
            Attr().withStringFromValueFunction (
                [](float v, int)
                {
                    if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                    return juce::String ((int) std::round (v)) + " Hz";
                })));
    }

    {
        juce::NormalisableRange<float> r (0.5f, 12.0f, 0.01f);
        r.setSkewForCentre (4.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "shimmerQ", 1 }, "Shimmer", r, 4.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String (v, 2) + " Q"; })));
    }

    // ---------- DECAY / TEXTURE ----------
    {
        juce::NormalisableRange<float> r (15.0f, 1500.0f, 0.5f);
        r.setSkewForCentre (110.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "decay", 1 }, "Decay", r, 90.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String ((int) v) + " ms"; })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "hold", 1 }, "Hold",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int)
            {
                if (v < 0.01f) return juce::String ("Closed");
                if (v > 0.95f) return juce::String ("Open");
                return juce::String ((int) std::round (v * 100.0f)) + " %";
            })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "noise", 1 }, "Air",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    {
        juce::NormalisableRange<float> r (300.0f, 14000.0f, 1.0f);
        r.setSkewForCentre (4500.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "color", 1 }, "Color", r, 7000.0f,
            Attr().withStringFromValueFunction (
                [](float v, int)
                {
                    if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                    return juce::String ((int) std::round (v)) + " Hz";
                })));
    }

    // ---------- MASTER ----------
    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.20f,
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
        juce::ParameterID { "width", 1 }, "Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "output", 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String (v, 1) + " dB"; })));

    return layout;
}

//==============================================================================
HH1AudioProcessor::HH1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "HH1State", createParameterLayout()),
       presetManager (apvts)
#else
     : apvts (*this, nullptr, "HH1State", createParameterLayout()),
       presetManager (apvts)
#endif
{
    tuneParam      = apvts.getRawParameterValue ("tune");
    metalParam     = apvts.getRawParameterValue ("metal");
    harmonicsParam = apvts.getRawParameterValue ("harmonics");
    hpCutParam     = apvts.getRawParameterValue ("hpCut");
    bpCutParam     = apvts.getRawParameterValue ("bpCut");
    shimmerQParam  = apvts.getRawParameterValue ("shimmerQ");
    decayParam     = apvts.getRawParameterValue ("decay");
    holdParam      = apvts.getRawParameterValue ("hold");
    noiseParam     = apvts.getRawParameterValue ("noise");
    colorParam     = apvts.getRawParameterValue ("color");
    driveParam     = apvts.getRawParameterValue ("drive");
    toneParam      = apvts.getRawParameterValue ("tone");
    widthParam     = apvts.getRawParameterValue ("width");
    outputParam    = apvts.getRawParameterValue ("output");
}

HH1AudioProcessor::~HH1AudioProcessor() = default;

//==============================================================================
const juce::String HH1AudioProcessor::getName() const            { return JucePlugin_Name; }
bool   HH1AudioProcessor::acceptsMidi() const                    { return true;  }
bool   HH1AudioProcessor::producesMidi() const                   { return false; }
bool   HH1AudioProcessor::isMidiEffect() const                   { return false; }
double HH1AudioProcessor::getTailLengthSeconds() const           { return 2.0; }

int    HH1AudioProcessor::getNumPrograms()                                           { return 1; }
int    HH1AudioProcessor::getCurrentProgram()                                        { return 0; }
void   HH1AudioProcessor::setCurrentProgram (int)                                    {}
const  juce::String HH1AudioProcessor::getProgramName (int)                          { return {}; }
void   HH1AudioProcessor::changeProgramName (int, const juce::String&)               {}

//==============================================================================
void HH1AudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    voice.prepare (sampleRate);
    voice.reset();
}

void HH1AudioProcessor::releaseResources()
{
    voice.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool HH1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

//==============================================================================
HiHatVoice::Params HH1AudioProcessor::buildParamsSnapshot() const noexcept
{
    HiHatVoice::Params p;
    if (tuneParam)      p.tuneHz        = tuneParam->load();
    if (metalParam)     p.metalLevel    = metalParam->load();
    if (harmonicsParam) p.harmonics     = harmonicsParam->load();
    if (hpCutParam)     p.hpCutoffHz    = hpCutParam->load();
    if (bpCutParam)     p.bpCutoffHz    = bpCutParam->load();
    if (shimmerQParam)  p.shimmerQ      = shimmerQParam->load();
    if (decayParam)     p.decayMs       = decayParam->load();
    if (holdParam)      p.holdLevel     = holdParam->load();
    if (noiseParam)     p.noiseLevel    = noiseParam->load();
    if (colorParam)     p.noiseColorHz  = colorParam->load();
    if (driveParam)     p.drive         = driveParam->load();
    if (toneParam)      p.tone          = toneParam->load();
    if (widthParam)     p.width         = widthParam->load();
    if (outputParam)    p.outputGainLin = juce::Decibels::decibelsToGain (outputParam->load());
    return p;
}

void HH1AudioProcessor::requestAudition (float velocity) noexcept
{
    auditionVelocity.store (juce::jlimit (0.05f, 1.0f, velocity));
    auditionPending.fetch_add (1, std::memory_order_release);
}

int HH1AudioProcessor::consumeTriggerEvent (float& velocityOut) noexcept
{
    const int now    = triggerCounter.load (std::memory_order_acquire);
    const int delta  = now - lastSeenTrigger;
    lastSeenTrigger  = now;
    velocityOut      = lastTriggerVel.load (std::memory_order_relaxed);
    return delta;
}

//==============================================================================
void HH1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    auto* outL = buffer.getWritePointer (0);
    auto* outR = (numCh > 1) ? buffer.getWritePointer (1) : nullptr;

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

        float l = 0.0f, r = 0.0f;
        voice.renderStereo (l, r);
        outL[n] = l;
        if (outR != nullptr) outR[n] = r;
    }
}

//==============================================================================
bool HH1AudioProcessor::hasEditor() const                        { return true; }
juce::AudioProcessorEditor* HH1AudioProcessor::createEditor()    { return new HH1AudioProcessorEditor (*this); }

//==============================================================================
void HH1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", presetManager.getCurrentPresetName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HH1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new HH1AudioProcessor();
}
