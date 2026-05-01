/*
  ==============================================================================
    ST-1  -  Luxury Saturation
    PluginProcessor.cpp
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
const juce::String ST1AudioProcessor::pidDrive        = "drive";
const juce::String ST1AudioProcessor::pidMode         = "mode";
const juce::String ST1AudioProcessor::pidBias         = "bias";
const juce::String ST1AudioProcessor::pidTone         = "tone";
const juce::String ST1AudioProcessor::pidMix          = "mix";
const juce::String ST1AudioProcessor::pidOutput       = "output";
const juce::String ST1AudioProcessor::pidOversampling = "oversampling";
const juce::String ST1AudioProcessor::pidBypass       = "bypass";

//==============================================================================
ST1AudioProcessor::ST1AudioProcessor()
   : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
     apvts (*this, nullptr, "STATE", createLayout())
{
    pDrive     = apvts.getRawParameterValue (pidDrive);
    pMode      = apvts.getRawParameterValue (pidMode);
    pBias      = apvts.getRawParameterValue (pidBias);
    pTone      = apvts.getRawParameterValue (pidTone);
    pMix       = apvts.getRawParameterValue (pidMix);
    pOutput    = apvts.getRawParameterValue (pidOutput);
    pOSampling = apvts.getRawParameterValue (pidOversampling);
    pBypass    = apvts.getRawParameterValue (pidBypass);
}

ST1AudioProcessor::~ST1AudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ST1AudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<P> (juce::ParameterID { pidDrive,  1 },
                                     "Drive",
                                     juce::NormalisableRange<float> (0.0f, 36.0f, 0.01f), 6.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<C> (juce::ParameterID { pidMode, 1 },
                                     "Mode",
                                     SaturationEngine::getModeNames(),
                                     0));

    layout.add (std::make_unique<P> (juce::ParameterID { pidBias, 1 },
                                     "Bias",
                                     juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<P> (juce::ParameterID { pidTone, 1 },
                                     "Tone",
                                     juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<P> (juce::ParameterID { pidMix, 1 },
                                     "Mix",
                                     juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("%")
                                         .withStringFromValueFunction ([] (float v, int) {
                                             return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<P> (juce::ParameterID { pidOutput, 1 },
                                     "Output",
                                     juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<C> (juce::ParameterID { pidOversampling, 1 },
                                     "Oversampling",
                                     juce::StringArray { "Off", "2x", "4x" },
                                     1));

    layout.add (std::make_unique<B> (juce::ParameterID { pidBypass, 1 },
                                     "Bypass",
                                     false));

    return layout;
}

//==============================================================================
const juce::String ST1AudioProcessor::getName() const            { return JucePlugin_Name; }
bool ST1AudioProcessor::acceptsMidi()  const                     { return false; }
bool ST1AudioProcessor::producesMidi() const                     { return false; }
bool ST1AudioProcessor::isMidiEffect() const                     { return false; }
double ST1AudioProcessor::getTailLengthSeconds() const           { return 0.0; }
int ST1AudioProcessor::getNumPrograms()                          { return 1; }
int ST1AudioProcessor::getCurrentProgram()                       { return 0; }
void ST1AudioProcessor::setCurrentProgram (int)                  {}
const juce::String ST1AudioProcessor::getProgramName (int)       { return {}; }
void ST1AudioProcessor::changeProgramName (int, const juce::String&) {}

int   ST1AudioProcessor::getCurrentMode()    const noexcept { return juce::roundToInt (pMode->load()); }
float ST1AudioProcessor::getCurrentBias()    const noexcept { return pBias->load(); }
float ST1AudioProcessor::getCurrentDriveDb() const noexcept { return pDrive->load(); }

//==============================================================================
void ST1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    dryBuffer.setSize ((int) spec.numChannels, samplesPerBlock, false, false, true);

    drvSmooth .reset (sampleRate, 0.02);
    biasSmooth.reset (sampleRate, 0.05);
    mixSmooth .reset (sampleRate, 0.02);
    outSmooth .reset (sampleRate, 0.02);

    drvSmooth .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDrive->load()));
    biasSmooth.setCurrentAndTargetValue (pBias->load());
    mixSmooth .setCurrentAndTargetValue (pMix->load());
    outSmooth .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    tiltLow .prepare (spec);
    tiltHigh.prepare (spec);
    updateToneCoefficients (sampleRate, pTone->load());

    os2x = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);

    os4x = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);

    os2x->initProcessing ((size_t) samplesPerBlock);
    os4x->initProcessing ((size_t) samplesPerBlock);

    // 300 ms release per block.
    meterReleaseCoeff = std::exp (-1.0f / (float) (sampleRate * 0.30));

    envInL = envInR = envOutL = envOutR = 0.0f;

    // Try to capture about a 25 ms window in the scope buffer.
    const int targetSamples = juce::jmax (1, (int) (sampleRate * 0.025));
    scopeDecimateRate = juce::jmax (1, targetSamples / scopeSize);
    scopeDecimator    = 0;

    scopeIn.fill (0.0f);
    scopeOut.fill (0.0f);
    scopeWritePos.store (0);
}

void ST1AudioProcessor::releaseResources()
{
    if (os2x) os2x->reset();
    if (os4x) os4x->reset();
    tiltLow .reset();
    tiltHigh.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ST1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainOut != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

//==============================================================================
void ST1AudioProcessor::updateToneCoefficients (double sampleRate, float toneVal)
{
    const float maxGainDb = 8.0f;
    const float gHi = juce::Decibels::decibelsToGain (toneVal *  maxGainDb);
    const float gLo = juce::Decibels::decibelsToGain (toneVal * -maxGainDb);

    *tiltHigh.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 4500.0,  0.6f, gHi);
    *tiltLow .state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, 250.0,   0.6f, gLo);

    lastTone = toneVal;
}

//==============================================================================
void ST1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumIn  = getTotalNumInputChannels();
    const int totalNumOut = getTotalNumOutputChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int i = totalNumIn; i < totalNumOut; ++i)
        buffer.clear (i, 0, numSamples);

    if (numSamples == 0)
        return;

    // ---- Input metering -----------------------------------------------------
    auto computeEnv = [this, numSamples] (const float* data, float& env) -> float
    {
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax (peak, std::abs (data[i]));

        if (peak > env)            env = peak;
        else                       env = peak + (env - peak) * std::pow (meterReleaseCoeff, (float) numSamples);
        return env;
    };

    if (totalNumOut > 0)
        inLevelDbL.store (juce::Decibels::gainToDecibels (
            computeEnv (buffer.getReadPointer (0), envInL), -100.0f));
    if (totalNumOut > 1)
        inLevelDbR.store (juce::Decibels::gainToDecibels (
            computeEnv (buffer.getReadPointer (1), envInR), -100.0f));
    else
        inLevelDbR.store (inLevelDbL.load());

    // ---- Snapshot dry buffer for mix ----------------------------------------
    dryBuffer.makeCopyOf (buffer, true);

    // ---- Update smoothed parameter targets ----------------------------------
    const float driveDb = pDrive->load();
    const float bias    = pBias->load();
    const float tone    = pTone->load();
    const float mix     = pMix->load();
    const float outDb   = pOutput->load();
    const int   mode    = juce::roundToInt (pMode->load());
    const int   osIdx   = juce::roundToInt (pOSampling->load());
    const bool  bypass  = pBypass->load() > 0.5f;

    drvSmooth .setTargetValue (juce::Decibels::decibelsToGain (driveDb));
    biasSmooth.setTargetValue (bias);
    mixSmooth .setTargetValue (mix);
    outSmooth .setTargetValue (juce::Decibels::decibelsToGain (outDb));

    if (std::abs (tone - lastTone) > 1.0e-4f)
        updateToneCoefficients (currentSampleRate, tone);

    if (! bypass)
    {
        // ---- Saturation core ------------------------------------------------
        // We process the wet path in `buffer` (which currently holds the input).
        auto processSaturation = [this, mode] (juce::dsp::AudioBlock<float>& block, int hostBlockSamples)
        {
            const int numCh   = (int) block.getNumChannels();
            const int numS    = (int) block.getNumSamples();
            const int ratio   = juce::jmax (1, numS / juce::jmax (1, hostBlockSamples));

            for (int i = 0; i < numS; ++i)
            {
                // Per-host-sample: advance smoothed values once every `ratio` upsampled samples.
                if ((i % ratio) == 0)
                {
                    drvSmooth .getNextValue();
                    biasSmooth.getNextValue();
                }
                const float drv  = drvSmooth .getCurrentValue();
                const float bias = biasSmooth.getCurrentValue();

                for (int ch = 0; ch < numCh; ++ch)
                {
                    float* d = block.getChannelPointer ((size_t) ch);
                    const float xn = d[i] * drv;
                    d[i] = SaturationEngine::shape (xn, mode, bias);
                }
            }
        };

        if (osIdx == 0)
        {
            juce::dsp::AudioBlock<float> block (buffer);
            processSaturation (block, numSamples);
        }
        else
        {
            auto& osr = (osIdx == 1) ? *os2x : *os4x;
            juce::dsp::AudioBlock<float> block (buffer);
            auto upBlock = osr.processSamplesUp (block);
            processSaturation (upBlock, numSamples);
            osr.processSamplesDown (block);
        }

        // ---- Tone (post-saturation tilt) ------------------------------------
        {
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            tiltLow .process (ctx);
            tiltHigh.process (ctx);
        }

        // ---- Mix + output gain (per-sample, smoothed) -----------------------
        for (int i = 0; i < numSamples; ++i)
        {
            const float m   = mixSmooth.getNextValue();
            const float og  = outSmooth.getNextValue();

            for (int ch = 0; ch < totalNumOut; ++ch)
            {
                float* w = buffer.getWritePointer (ch);
                const float dry = dryBuffer.getReadPointer (ch)[i];
                const float wet = w[i];
                w[i] = (dry * (1.0f - m) + wet * m) * og;
            }
        }
    }
    else
    {
        // Bypassed: still drain smoothed values to keep state consistent.
        for (int i = 0; i < numSamples; ++i)
        {
            drvSmooth .getNextValue();
            biasSmooth.getNextValue();
            mixSmooth .getNextValue();
            outSmooth .getNextValue();
        }
    }

    // ---- Output metering ----------------------------------------------------
    if (totalNumOut > 0)
        outLevelDbL.store (juce::Decibels::gainToDecibels (
            computeEnv (buffer.getReadPointer (0), envOutL), -100.0f));
    if (totalNumOut > 1)
        outLevelDbR.store (juce::Decibels::gainToDecibels (
            computeEnv (buffer.getReadPointer (1), envOutR), -100.0f));
    else
        outLevelDbR.store (outLevelDbL.load());

    // ---- Scope: write decimated samples ------------------------------------
    {
        const float* inLPtr  = dryBuffer.getReadPointer (0);
        const float* inRPtr  = totalNumOut > 1 ? dryBuffer.getReadPointer (1) : inLPtr;
        const float* outLPtr = buffer.getReadPointer (0);
        const float* outRPtr = totalNumOut > 1 ? buffer.getReadPointer (1) : outLPtr;

        int wp = scopeWritePos.load();
        for (int i = 0; i < numSamples; ++i)
        {
            if (++scopeDecimator >= scopeDecimateRate)
            {
                scopeDecimator = 0;
                scopeIn [wp] = 0.5f * (inLPtr [i] + inRPtr [i]);
                scopeOut[wp] = 0.5f * (outLPtr[i] + outRPtr[i]);
                wp = (wp + 1) % scopeSize;
            }
        }
        scopeWritePos.store (wp);
    }
}

//==============================================================================
bool ST1AudioProcessor::hasEditor() const                       { return true; }
juce::AudioProcessorEditor* ST1AudioProcessor::createEditor()   { return new ST1AudioProcessorEditor (*this); }

//==============================================================================
void ST1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("ST1");
    root.setProperty ("version", 1, nullptr);
    root.appendChild (apvts.copyState(), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void ST1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        juce::ValueTree root = juce::ValueTree::fromXml (*xml);
        if (root.isValid())
        {
            // Accept either the wrapped form (root "ST1" with APVTS child) or the
            // raw APVTS state, for backwards compatibility.
            juce::ValueTree apvtsState = root.getChildWithName (apvts.state.getType());
            if (! apvtsState.isValid())
                apvtsState = root;

            if (apvtsState.isValid())
                apvts.replaceState (apvtsState);
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ST1AudioProcessor();
}
