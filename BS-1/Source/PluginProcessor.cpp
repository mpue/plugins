/*
  ==============================================================================

    PluginProcessor.cpp
    BS-1 Luxury Bass Synthesizer

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
BS1AudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using IntParam   = juce::AudioParameterInt;
    using Attr       = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ----- Tone / Oscillators -----
    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "tone", 1 }, "Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f,
        Attr().withStringFromValueFunction (
            [] (float v, int)
            {
                if (v < 0.08f)        return juce::String ("Sine");
                if (v < 0.30f)        return juce::String ("Sine→Tri");
                if (v < 0.40f)        return juce::String ("Tri");
                if (v < 0.60f)        return juce::String ("Tri→Saw");
                if (v < 0.72f)        return juce::String ("Saw");
                if (v < 0.92f)        return juce::String ("Saw→Sqr");
                return juce::String ("Square");
            })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.30f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "subLevel", 1 }, "Sub",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "noiseLevel", 1 }, "Noise",
        juce::NormalisableRange<float> (0.0f, 0.40f, 0.001f), 0.02f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 250.0f)) + " %"; })));

    layout.add (std::make_unique<IntParam> (
        juce::ParameterID { "octave", 1 }, "Octave", -2, 2, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [](int v, int) { return (v >= 0 ? juce::String ("+") : juce::String()) + juce::String (v); })));

    // ----- Filter -----
    {
        juce::NormalisableRange<float> r (30.0f, 14000.0f, 1.0f);
        r.setSkewForCentre (500.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "cutoff", 1 }, "Cutoff", r, 500.0f,
            Attr().withStringFromValueFunction (
                [](float v, int)
                {
                    if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                    return juce::String ((int) std::round (v)) + " Hz";
                })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "resonance", 1 }, "Resonance",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "envAmount", 1 }, "Env Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.65f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    {
        juce::NormalisableRange<float> r (5.0f, 2000.0f, 0.1f);
        r.setSkewForCentre (200.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "filterDecay", 1 }, "Decay", r, 280.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String ((int) std::round (v)) + " ms"; })));
    }

    // ----- Amp envelope -----
    {
        juce::NormalisableRange<float> r (0.5f, 500.0f, 0.1f);
        r.setSkewForCentre (10.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "ampAttack", 1 }, "Attack", r, 6.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "ampSustain", 1 }, "Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

    {
        juce::NormalisableRange<float> r (5.0f, 2500.0f, 0.1f);
        r.setSkewForCentre (250.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "ampRelease", 1 }, "Release", r, 220.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String ((int) std::round (v)) + " ms"; })));
    }

    // ----- Voice -----
    {
        juce::NormalisableRange<float> r (0.0f, 1500.0f, 0.1f);
        r.setSkewForCentre (80.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "glide", 1 }, "Glide", r, 60.0f,
            Attr().withStringFromValueFunction (
                [](float v, int)
                {
                    if (v < 0.5f) return juce::String ("Off");
                    return juce::String ((int) std::round (v)) + " ms";
                })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "warmth", 1 }, "Warmth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.40f,
        Attr().withStringFromValueFunction (
            [](float v, int)
            {
                const float dB = (v - 0.5f) * 2.0f * 6.0f;
                if (std::abs (dB) < 0.1f) return juce::String ("Flat");
                return (dB > 0.0f ? juce::String ("+") : juce::String()) + juce::String (dB, 1) + " dB";
            })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "output", 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String (v, 1) + " dB"; })));

    return layout;
}

//==============================================================================
BS1AudioProcessor::BS1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "BS1State", createParameterLayout()),
       presetManager (apvts)
#else
     : apvts (*this, nullptr, "BS1State", createParameterLayout()),
       presetManager (apvts)
#endif
{
    toneParam        = apvts.getRawParameterValue ("tone");
    driveParam       = apvts.getRawParameterValue ("drive");
    subLevelParam    = apvts.getRawParameterValue ("subLevel");
    noiseLevelParam  = apvts.getRawParameterValue ("noiseLevel");
    octaveParam      = apvts.getRawParameterValue ("octave");

    cutoffParam      = apvts.getRawParameterValue ("cutoff");
    resonanceParam   = apvts.getRawParameterValue ("resonance");
    envAmountParam   = apvts.getRawParameterValue ("envAmount");
    filterDecayParam = apvts.getRawParameterValue ("filterDecay");

    ampAttackParam   = apvts.getRawParameterValue ("ampAttack");
    ampSustainParam  = apvts.getRawParameterValue ("ampSustain");
    ampReleaseParam  = apvts.getRawParameterValue ("ampRelease");

    glideParam       = apvts.getRawParameterValue ("glide");
    warmthParam      = apvts.getRawParameterValue ("warmth");
    outputParam      = apvts.getRawParameterValue ("output");
}

BS1AudioProcessor::~BS1AudioProcessor() = default;

//==============================================================================
const juce::String BS1AudioProcessor::getName() const            { return JucePlugin_Name; }
bool   BS1AudioProcessor::acceptsMidi() const                    { return true;  }
bool   BS1AudioProcessor::producesMidi() const                   { return false; }
bool   BS1AudioProcessor::isMidiEffect() const                   { return false; }
double BS1AudioProcessor::getTailLengthSeconds() const           { return 3.0; }

int    BS1AudioProcessor::getNumPrograms()                                   { return 1; }
int    BS1AudioProcessor::getCurrentProgram()                                { return 0; }
void   BS1AudioProcessor::setCurrentProgram (int)                            {}
const  juce::String BS1AudioProcessor::getProgramName (int)                  { return {}; }
void   BS1AudioProcessor::changeProgramName (int, const juce::String&)       {}

//==============================================================================
void BS1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    voice.prepare (sampleRate, samplesPerBlock);
    voice.reset();

    visRing.fill (0.0f);
    visWritePos.store (0);

    keyboardState.reset();
}

void BS1AudioProcessor::releaseResources()
{
    voice.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BS1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

//==============================================================================
BassVoice::Params BS1AudioProcessor::buildParamsSnapshot() const noexcept
{
    BassVoice::Params p;
    if (toneParam)        p.tone        = toneParam->load();
    if (driveParam)       p.drive       = driveParam->load();
    if (subLevelParam)    p.subLevel    = subLevelParam->load();
    if (noiseLevelParam)  p.noiseLevel  = noiseLevelParam->load();
    if (octaveParam)      p.octaveShift = (int) octaveParam->load();

    if (cutoffParam)      p.cutoffHz    = cutoffParam->load();
    if (resonanceParam)   p.resonance   = resonanceParam->load();
    if (envAmountParam)   p.envAmount   = envAmountParam->load();
    if (filterDecayParam) p.filterDecay = filterDecayParam->load();

    if (ampAttackParam)   p.ampAttack   = ampAttackParam->load();
    if (ampSustainParam)  p.ampSustain  = ampSustainParam->load();
    if (ampReleaseParam)  p.ampRelease  = ampReleaseParam->load();

    if (glideParam)       p.glideMs     = glideParam->load();
    if (warmthParam)      p.warmth      = warmthParam->load();
    if (outputParam)      p.outputGainLin = juce::Decibels::decibelsToGain (outputParam->load());
    return p;
}

void BS1AudioProcessor::requestAudition (float velocity) noexcept
{
    auditionVelocity.store (juce::jlimit (0.05f, 1.0f, velocity));
    auditionPending.fetch_add (1, std::memory_order_release);
}

void BS1AudioProcessor::requestAuditionRelease() noexcept
{
    auditionReleasePending.store (true, std::memory_order_release);
}

int BS1AudioProcessor::consumeTriggerEvent (float& velocityOut) noexcept
{
    const int now    = triggerCounter.load (std::memory_order_acquire);
    const int delta  = now - lastSeenTrigger;
    lastSeenTrigger  = now;
    velocityOut      = lastTriggerVel.load (std::memory_order_relaxed);
    return delta;
}

void BS1AudioProcessor::pullVisAudio (float* dest, int numSamples) noexcept
{
    const int wp = visWritePos.load (std::memory_order_acquire);
    const int n  = juce::jmin (numSamples, kVisRingSize);
    for (int i = 0; i < n; ++i)
    {
        const int idx = (wp - n + i + kVisRingSize) % kVisRingSize;
        dest[i] = visRing[(size_t) idx];
    }
}

//==============================================================================
void BS1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0)
        return;

    // merge MIDI from on-screen keyboard
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // refresh DSP params from APVTS once per block
    voice.setParams (buildParamsSnapshot());

    auto fireNoteOn = [this] (int note, float vel)
    {
        voice.noteOn (note, vel);
        lastTriggerVel.store (vel, std::memory_order_relaxed);
        triggerCounter.fetch_add (1, std::memory_order_release);
    };

    // ---- consume audition requests from UI ----
    int pending = auditionPending.exchange (0, std::memory_order_acquire);
    if (pending > 0)
    {
        fireNoteOn (kAuditionMidiNote, auditionVelocity.load (std::memory_order_relaxed));
        auditionGateOn = true;
    }
    if (auditionReleasePending.exchange (false, std::memory_order_acquire))
    {
        if (auditionGateOn)
        {
            voice.noteOff (kAuditionMidiNote);
            auditionGateOn = false;
        }
    }

    // ---- process MIDI events with sample-accurate gating ----
    int sampleIdx = 0;
    for (auto meta : midi)
    {
        const int evPos = juce::jlimit (0, numSamples, meta.samplePosition);
        const int len   = evPos - sampleIdx;
        if (len > 0)
        {
            voice.renderBlock (buffer, sampleIdx, len);
            // also push to visual ring buffer (mono mix)
            for (int i = 0; i < len; ++i)
            {
                float s = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                    s += buffer.getSample (ch, sampleIdx + i);
                s /= juce::jmax (1, numCh);
                const int wp = (visWritePos.load (std::memory_order_relaxed) + 1) % kVisRingSize;
                visRing[(size_t) wp] = s;
                visWritePos.store (wp, std::memory_order_release);
            }
            sampleIdx = evPos;
        }

        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const float vel = juce::jlimit (0.05f, 1.0f, m.getFloatVelocity());
            fireNoteOn (m.getNoteNumber(), vel);
        }
        else if (m.isNoteOff())
        {
            voice.noteOff (m.getNoteNumber());
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            voice.allNotesOff();
        }
    }
    if (sampleIdx < numSamples)
    {
        const int len = numSamples - sampleIdx;
        voice.renderBlock (buffer, sampleIdx, len);
        for (int i = 0; i < len; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                s += buffer.getSample (ch, sampleIdx + i);
            s /= juce::jmax (1, numCh);
            const int wp = (visWritePos.load (std::memory_order_relaxed) + 1) % kVisRingSize;
            visRing[(size_t) wp] = s;
            visWritePos.store (wp, std::memory_order_release);
        }
    }
}

//==============================================================================
bool BS1AudioProcessor::hasEditor() const                        { return true; }
juce::AudioProcessorEditor* BS1AudioProcessor::createEditor()    { return new BS1AudioProcessorEditor (*this); }

//==============================================================================
void BS1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", presetManager.getCurrentPresetName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BS1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new BS1AudioProcessor();
}
