/*
  ==============================================================================

    PluginProcessor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
BC1AudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using BoolParam  = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    using Attr = juce::AudioParameterFloatAttributes;

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (-12.0f, 24.0f, 0.01f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String (v, 1) + " dB"; })
              .withValueFromStringFunction (
            [](const juce::String& s) { return s.getFloatValue(); })));

    {
        juce::NormalisableRange<float> bitsRange (1.0f, 16.0f, 0.01f);
        bitsRange.setSkewForCentre (6.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "bits", 1 }, "Bits",
            bitsRange, 16.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String (v, 2) + " bit"; })
                  .withValueFromStringFunction (
                [](const juce::String& s) { return s.getFloatValue(); })));
    }

    {
        juce::NormalisableRange<float> rateRange (200.0f, 50000.0f, 1.0f);
        rateRange.setSkewForCentre (2000.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "rate", 1 }, "Rate",
            rateRange, 50000.0f,
            Attr().withStringFromValueFunction (
                [](float v, int)
                {
                    if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                    return juce::String ((int) std::round (v)) + " Hz";
                })
                  .withValueFromStringFunction (
                [](const juce::String& s) { return s.getFloatValue(); })));
    }

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "dither", 1 }, "Dither",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
              .withValueFromStringFunction (
            [](const juce::String& s) { return s.getFloatValue() * 0.01f; })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "tone", 1 }, "Tone",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int)
            {
                if (std::abs (v) < 0.005f) return juce::String ("Flat");
                return juce::String (v >= 0.0f ? "+" : "") + juce::String (v * 9.0f, 1) + " dB";
            })
              .withValueFromStringFunction (
            [](const juce::String& s) { return s.getFloatValue(); })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
              .withValueFromStringFunction (
            [](const juce::String& s) { return s.getFloatValue() * 0.01f; })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "output", 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int) { return juce::String (v, 1) + " dB"; })
              .withValueFromStringFunction (
            [](const juce::String& s) { return s.getFloatValue(); })));

    layout.add (std::make_unique<BoolParam> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

//==============================================================================
BC1AudioProcessor::BC1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "BC1State", createParameterLayout()),
       presetManager (apvts)
#else
     : apvts (*this, nullptr, "BC1State", createParameterLayout()),
       presetManager (apvts)
#endif
{
    driveParam  = apvts.getRawParameterValue ("drive");
    bitsParam   = apvts.getRawParameterValue ("bits");
    rateParam   = apvts.getRawParameterValue ("rate");
    ditherParam = apvts.getRawParameterValue ("dither");
    toneParam   = apvts.getRawParameterValue ("tone");
    mixParam    = apvts.getRawParameterValue ("mix");
    outputParam = apvts.getRawParameterValue ("output");
    bypassParam = apvts.getRawParameterValue ("bypass");
}

BC1AudioProcessor::~BC1AudioProcessor() = default;

//==============================================================================
const juce::String BC1AudioProcessor::getName() const            { return JucePlugin_Name; }
bool   BC1AudioProcessor::acceptsMidi()        const             { return false; }
bool   BC1AudioProcessor::producesMidi()       const             { return false; }
bool   BC1AudioProcessor::isMidiEffect()       const             { return false; }
double BC1AudioProcessor::getTailLengthSeconds() const           { return 0.0; }

juce::AudioProcessorParameter* BC1AudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

int  BC1AudioProcessor::getNumPrograms()                                          { return 1; }
int  BC1AudioProcessor::getCurrentProgram()                                       { return 0; }
void BC1AudioProcessor::setCurrentProgram (int)                                   {}
const juce::String BC1AudioProcessor::getProgramName (int)                        { return {}; }
void BC1AudioProcessor::changeProgramName (int, const juce::String&)              {}

//==============================================================================
void BC1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels(), 2);

    bitcrusher.prepare (sampleRate, samplesPerBlock, numCh);
    dryCopy.setSize (numCh, samplesPerBlock, false, false, true);

    mixSm    .reset (sampleRate, 0.02);
    outputSm .reset (sampleRate, 0.02);
    processSm.reset (sampleRate, 0.005);

    mixSm    .setCurrentAndTargetValue (mixParam    != nullptr ? mixParam->load() : 1.0f);
    outputSm .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
                                          outputParam != nullptr ? outputParam->load() : 0.0f));
    processSm.setCurrentAndTargetValue (1.0f);

    scopeIn .fill (0.0f);
    scopeOut.fill (0.0f);
    scopeWritePos.store (0);

    setLatencySamples (bitcrusher.getLatencyInSamples());
}

void BC1AudioProcessor::releaseResources()
{
    bitcrusher.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BC1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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
void BC1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumIn  = getTotalNumInputChannels();
    const int totalNumOut = getTotalNumOutputChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = totalNumIn; ch < totalNumOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0)
        return;

    // ---------- save dry ----------
    if (dryCopy.getNumChannels() < totalNumOut || dryCopy.getNumSamples() < numSamples)
        dryCopy.setSize (totalNumOut, numSamples, false, false, true);

    for (int ch = 0; ch < totalNumOut; ++ch)
        dryCopy.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // ---------- update DSP parameters ----------
    bitcrusher.setParameters (driveParam ->load(),
                              bitsParam  ->load(),
                              rateParam  ->load(),
                              ditherParam->load(),
                              toneParam  ->load());

    const bool bypassed = bypassParam != nullptr && bypassParam->load() > 0.5f;
    processSm.setTargetValue (bypassed ? 0.0f : 1.0f);
    mixSm   .setTargetValue (mixParam != nullptr ? mixParam->load() : 1.0f);
    outputSm.setTargetValue (juce::Decibels::decibelsToGain (
                              outputParam != nullptr ? outputParam->load() : 0.0f));

    // ---------- crush wet ----------
    bitcrusher.process (buffer);

    // ---------- mix dry + wet, apply output gain ----------
    for (int n = 0; n < numSamples; ++n)
    {
        const float ramp     = processSm.getNextValue();
        const float mix      = mixSm    .getNextValue() * ramp;
        const float gain     = outputSm .getNextValue();

        for (int ch = 0; ch < totalNumOut; ++ch)
        {
            const float dry = (ch < dryCopy.getNumChannels() ? dryCopy.getSample (ch, n) : 0.0f);
            const float wet = buffer.getSample (ch, n);
            const float mixed = dry + (wet - dry) * mix;
            buffer.setSample (ch, n, mixed * gain);
        }
    }

    // ---------- scope tap (channel 0) ----------
    {
        const float* dryCh = (totalNumIn  > 0 ? dryCopy.getReadPointer (0)  : nullptr);
        const float* wetCh = (totalNumOut > 0 ? buffer .getReadPointer (0)  : nullptr);

        int wp = scopeWritePos.load (std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            scopeIn [(size_t) wp] = dryCh != nullptr ? dryCh[i] : 0.0f;
            scopeOut[(size_t) wp] = wetCh != nullptr ? wetCh[i] : 0.0f;
            wp = (wp + 1) & (scopeBufferSize - 1);
        }
        scopeWritePos.store (wp, std::memory_order_release);
    }
}

//==============================================================================
void BC1AudioProcessor::copyScope (float* dryOut, float* wetOut, int numSamples) const noexcept
{
    static_assert ((scopeBufferSize & (scopeBufferSize - 1)) == 0,
                   "scopeBufferSize must be a power of two");

    const int N  = scopeBufferSize;
    const int wp = scopeWritePos.load (std::memory_order_acquire);
    int start = (wp - numSamples + N) & (N - 1);

    for (int i = 0; i < numSamples; ++i)
    {
        const int idx = (start + i) & (N - 1);
        dryOut[i] = scopeIn [(size_t) idx];
        wetOut[i] = scopeOut[(size_t) idx];
    }
}

float BC1AudioProcessor::getCurrentBits()   const noexcept { return bitsParam   ? bitsParam->load()   : 16.0f; }
float BC1AudioProcessor::getCurrentMix()    const noexcept { return mixParam    ? mixParam->load()    : 1.0f;  }
float BC1AudioProcessor::getCurrentRate()   const noexcept { return rateParam   ? rateParam->load()   : 50000.0f; }
bool  BC1AudioProcessor::getCurrentBypass() const noexcept { return bypassParam && bypassParam->load() > 0.5f; }

//==============================================================================
bool BC1AudioProcessor::hasEditor() const                        { return true; }
juce::AudioProcessorEditor* BC1AudioProcessor::createEditor()    { return new BC1AudioProcessorEditor (*this); }

//==============================================================================
void BC1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", presetManager.getCurrentPresetName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BC1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new BC1AudioProcessor();
}
