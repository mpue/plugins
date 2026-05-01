/*
  ==============================================================================
    CP-1 Compressor — Processor implementation
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
CP1AudioProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using C = juce::AudioParameterChoice;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<P>(juce::ParameterID { "threshold", 1 }, "Threshold",
                                     juce::NormalisableRange<float>(-60.0f, 0.0f, 0.01f), -18.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<P>(juce::ParameterID { "ratio", 1 }, "Ratio",
                                     juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f, 0.4f), 4.0f,
                                     juce::AudioParameterFloatAttributes().withLabel (": 1")));

    layout.add (std::make_unique<P>(juce::ParameterID { "attack", 1 }, "Attack",
                                     juce::NormalisableRange<float>(0.1f, 200.0f, 0.01f, 0.3f), 10.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<P>(juce::ParameterID { "release", 1 }, "Release",
                                     juce::NormalisableRange<float>(5.0f, 2000.0f, 0.01f, 0.3f), 120.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<P>(juce::ParameterID { "knee", 1 }, "Knee",
                                     juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f), 6.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<P>(juce::ParameterID { "makeup", 1 }, "Makeup",
                                     juce::NormalisableRange<float>(-12.0f, 24.0f, 0.01f), 0.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<P>(juce::ParameterID { "mix", 1 }, "Mix",
                                     juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<C>(juce::ParameterID { "detector", 1 }, "Detector",
                                     juce::StringArray { "Peak", "RMS" }, 0));

    layout.add (std::make_unique<B>(juce::ParameterID { "stereoLink", 1 }, "Stereo Link", true));
    layout.add (std::make_unique<B>(juce::ParameterID { "autoRelease", 1 }, "Auto Release", false));

    layout.add (std::make_unique<B>(juce::ParameterID { "extSc", 1 }, "External SC", false));
    layout.add (std::make_unique<P>(juce::ParameterID { "scHpf", 1 }, "SC HPF",
                                     juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.4f), 20.0f,
                                     juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<B>(juce::ParameterID { "scListen", 1 }, "SC Listen", false));

    layout.add (std::make_unique<B>(juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

//==============================================================================
CP1AudioProcessor::CP1AudioProcessor()
     : AudioProcessor (BusesProperties()
                         .withInput  ("Input",      juce::AudioChannelSet::stereo(), true)
                         .withInput  ("Sidechain",  juce::AudioChannelSet::stereo(), false)
                         .withOutput ("Output",     juce::AudioChannelSet::stereo(), true)),
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
       presetManager (apvts)
{
}

CP1AudioProcessor::~CP1AudioProcessor() = default;

//==============================================================================
const juce::String CP1AudioProcessor::getName() const   { return JucePlugin_Name; }
bool   CP1AudioProcessor::acceptsMidi() const            { return false; }
bool   CP1AudioProcessor::producesMidi() const           { return false; }
bool   CP1AudioProcessor::isMidiEffect() const           { return false; }
double CP1AudioProcessor::getTailLengthSeconds() const   { return 0.0; }

int    CP1AudioProcessor::getNumPrograms()               { return 1; }
int    CP1AudioProcessor::getCurrentProgram()            { return 0; }
void   CP1AudioProcessor::setCurrentProgram (int)        {}
const juce::String CP1AudioProcessor::getProgramName (int)               { return {}; }
void   CP1AudioProcessor::changeProgramName (int, const juce::String&)   {}

//==============================================================================
void CP1AudioProcessor::prepareToPlay (double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;

    scHpfL.reset();
    scHpfR.reset();
    scHpfL.setHighPass (sr, 20.0, 0.707);
    scHpfR.setHighPass (sr, 20.0, 0.707);

    rmsStateL = rmsStateR = 0.0f;
    grEnvL = grEnvR = 0.0f;
    grSlowL = grSlowR = 0.0f;
    meterInDb = meterOutDb = -60.0f;
    meterGrDb = 0.0f;
}

void CP1AudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CP1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled())
        return false;

    if (mainIn != mainOut)
        return false;

    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    // Sidechain: disabled, mono or stereo all OK
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled()
            && sc != juce::AudioChannelSet::mono()
            && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}
#endif

//==============================================================================
void CP1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainOutBus = getBusBuffer (buffer, false, 0);
    auto mainInBus  = getBusBuffer (buffer, true,  0);

    const int numSamples = buffer.getNumSamples();
    const int numCh      = juce::jmin (2, mainOutBus.getNumChannels());

    // Sidechain bus availability
    const auto* scBus = getBus (true, 1);
    const bool scBusAvailable = (scBus != nullptr && scBus->isEnabled());
    scExternalConnected.store (scBusAvailable);

    // Read parameters
    const float threshold   = apvts.getRawParameterValue ("threshold")->load();
    const float ratio       = apvts.getRawParameterValue ("ratio")->load();
    const float attackMs    = apvts.getRawParameterValue ("attack")->load();
    const float releaseMs   = apvts.getRawParameterValue ("release")->load();
    const float kneeDb      = apvts.getRawParameterValue ("knee")->load();
    const float makeupDb    = apvts.getRawParameterValue ("makeup")->load();
    const float mix         = apvts.getRawParameterValue ("mix")->load() * 0.01f;
    const int   detector    = (int) apvts.getRawParameterValue ("detector")->load();
    const bool  stereoLink  = apvts.getRawParameterValue ("stereoLink")->load() > 0.5f;
    const bool  autoRel     = apvts.getRawParameterValue ("autoRelease")->load() > 0.5f;
    const bool  extSc       = apvts.getRawParameterValue ("extSc")->load() > 0.5f;
    const float scHpfHz     = apvts.getRawParameterValue ("scHpf")->load();
    const bool  scListen    = apvts.getRawParameterValue ("scListen")->load() > 0.5f;
    const bool  bypass      = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    // Update HPF coefficients (cheap, called once per block)
    scHpfL.setHighPass (sampleRate, (double) scHpfHz, 0.707);
    scHpfR.setHighPass (sampleRate, (double) scHpfHz, 0.707);

    // Envelope coefficients (one-pole, time = "time to reach (1 - 1/e)")
    auto onePoleCoeff = [this] (float ms) noexcept
    {
        const float t = juce::jmax (0.05f, ms) * 0.001f;
        return 1.0f - std::exp (-1.0f / (t * (float) sampleRate));
    };
    const float attackCoeff  = onePoleCoeff (attackMs);
    const float releaseCoeff = onePoleCoeff (releaseMs);

    // Slow envelope used for adaptive release: longer constant
    const float slowCoeff    = onePoleCoeff (releaseMs * 4.0f);

    // RMS smoother coefficient (~10 ms window)
    const float rmsCoeff     = onePoleCoeff (10.0f);

    // Use the external sidechain only when both requested AND connected
    const bool useExtSc = extSc && scBusAvailable;

    // Make a dry copy of the main input for parallel mix and SC fallback
    juce::AudioBuffer<float> dryCopy;
    dryCopy.setSize (numCh, numSamples, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        dryCopy.copyFrom (ch, 0, mainInBus, ch, 0, numSamples);

    // Choose sidechain source
    juce::AudioBuffer<float> scBuf;
    if (useExtSc)
    {
        auto extBus = getBusBuffer (buffer, true, 1);
        const int scCh = juce::jmin (2, extBus.getNumChannels());
        scBuf.setSize (juce::jmax (1, scCh), numSamples, false, false, true);
        for (int ch = 0; ch < scCh; ++ch)
            scBuf.copyFrom (ch, 0, extBus, ch, 0, numSamples);
    }
    else
    {
        scBuf.setSize (numCh, numSamples, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            scBuf.copyFrom (ch, 0, dryCopy, ch, 0, numSamples);
    }

    const int scNumCh = scBuf.getNumChannels();

    // Bypass: just copy input to output, update meters from input
    if (bypass)
    {
        for (int ch = 0; ch < numCh; ++ch)
            mainOutBus.copyFrom (ch, 0, dryCopy, ch, 0, numSamples);

        float inPeak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            inPeak = juce::jmax (inPeak, dryCopy.getMagnitude (ch, 0, numSamples));

        const float inDbNow = juce::Decibels::gainToDecibels (inPeak, -60.0f);
        meterInDb  += (inDbNow - meterInDb)  * 0.4f;
        meterOutDb += (inDbNow - meterOutDb) * 0.4f;
        meterGrDb  += (0.0f    - meterGrDb)  * 0.4f;

        inLevelDb.store (meterInDb);
        outLevelDb.store (meterOutDb);
        grDb.store (meterGrDb);
        return;
    }

    const float makeupLin = juce::Decibels::decibelsToGain (makeupDb);

    // For peak metering this block
    float blockInPeak  = 0.0f;
    float blockOutPeak = 0.0f;
    float blockGrMax   = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = dryCopy.getSample (0, i);
        const float inR = numCh > 1 ? dryCopy.getSample (1, i) : inL;

        blockInPeak = juce::jmax (blockInPeak, std::abs (inL), std::abs (inR));

        // ---- sidechain detection signal (HPF'ed) ----
        float scL = scBuf.getSample (0, i);
        float scR = scNumCh > 1 ? scBuf.getSample (1, i) : scL;
        scL = scHpfL.processSample (scL);
        scR = scHpfR.processSample (scR);

        // ---- detector (peak or RMS) ----
        float detL, detR;
        if (detector == 0)
        {
            detL = std::abs (scL);
            detR = std::abs (scR);
        }
        else
        {
            rmsStateL += (scL * scL - rmsStateL) * rmsCoeff;
            rmsStateR += (scR * scR - rmsStateR) * rmsCoeff;
            detL = std::sqrt (juce::jmax (0.0f, rmsStateL));
            detR = std::sqrt (juce::jmax (0.0f, rmsStateR));
        }

        float detDbL = juce::Decibels::gainToDecibels (detL, -100.0f);
        float detDbR = juce::Decibels::gainToDecibels (detR, -100.0f);

        if (stereoLink)
        {
            const float linked = juce::jmax (detDbL, detDbR);
            detDbL = detDbR = linked;
        }

        // ---- gain computer ----
        const float redDbL = computeReductionDb (detDbL, threshold, ratio, kneeDb);
        const float redDbR = computeReductionDb (detDbR, threshold, ratio, kneeDb);

        // ---- attack/release smoothing (in dB domain) ----
        // Adaptive release: blend release coefficient towards a slower one
        // when the running-average GR is large (program-dependent release).
        grSlowL += (redDbL - grSlowL) * slowCoeff;
        grSlowR += (redDbR - grSlowR) * slowCoeff;

        auto applyEnv = [&] (float& env, float target, float slowGr) noexcept
        {
            float relC = releaseCoeff;
            if (autoRel)
            {
                // ratio of slow GR to threshold-of-noticeable (~6 dB).
                // Blends release smoothly toward slowCoeff (4× slower) when GR is sustained.
                const float blend = juce::jlimit (0.0f, 1.0f, slowGr / 6.0f);
                relC = juce::jmap (blend, releaseCoeff, slowCoeff);
            }
            const float c = (target > env) ? attackCoeff : relC;
            env += (target - env) * c;
        };

        applyEnv (grEnvL, redDbL, grSlowL);
        applyEnv (grEnvR, redDbR, grSlowR);

        // Track maximum GR for the meter
        blockGrMax = juce::jmax (blockGrMax, grEnvL, grEnvR);

        // ---- apply gain ----
        const float gainL = juce::Decibels::decibelsToGain (-grEnvL) * makeupLin;
        const float gainR = juce::Decibels::decibelsToGain (-grEnvR) * makeupLin;

        const float wetL = inL * gainL;
        const float wetR = inR * gainR;

        // Dry/wet (parallel) mix
        float outL = inL * (1.0f - mix) + wetL * mix;
        float outR = inR * (1.0f - mix) + wetR * mix;

        // SC Listen — replaces the output with the post-HPF sidechain signal
        if (scListen)
        {
            outL = scL;
            outR = scR;
        }

        mainOutBus.setSample (0, i, outL);
        if (numCh > 1)
            mainOutBus.setSample (1, i, outR);

        blockOutPeak = juce::jmax (blockOutPeak, std::abs (outL), std::abs (outR));
    }

    // ---- update meters (smoothed for nice GUI motion) ----
    const float inDbNow  = juce::Decibels::gainToDecibels (blockInPeak,  -60.0f);
    const float outDbNow = juce::Decibels::gainToDecibels (blockOutPeak, -60.0f);

    // Asymmetric ballistics for VU-like behaviour: snap up, fall slowly
    auto smoothMeter = [] (float& current, float target, float upCoeff, float downCoeff)
    {
        const float c = (target > current) ? upCoeff : downCoeff;
        current += (target - current) * c;
    };

    smoothMeter (meterInDb,  inDbNow,  0.6f, 0.10f);
    smoothMeter (meterOutDb, outDbNow, 0.6f, 0.10f);
    smoothMeter (meterGrDb,  blockGrMax, 0.6f, 0.10f);

    inLevelDb.store (meterInDb);
    outLevelDb.store (meterOutDb);
    grDb.store (meterGrDb);
}

//==============================================================================
bool CP1AudioProcessor::hasEditor() const                       { return true; }
juce::AudioProcessorEditor* CP1AudioProcessor::createEditor()   { return new CP1AudioProcessorEditor (*this); }

//==============================================================================
void CP1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void CP1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CP1AudioProcessor();
}
