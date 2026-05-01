/*
  ==============================================================================

    LT-1 — Luxury Limiter
    PluginProcessor.cpp

    A look-ahead peak limiter with a smooth, program-dependent release. The
    DSP path:
      1. Apply input gain.
      2. Push every sample into a per-channel delay line of length = lookahead.
      3. Feed the linked peak (max of |L|, |R|) into a sliding-window peak
         tracker covering the same lookahead. The output of that tracker is
         the peak the limiter is *about* to see.
      4. Compute a target gain that keeps that peak below the ceiling, with
         an optional soft knee.
      5. Smooth the gain (instant attack, exponential release) and apply it
         to the delayed samples.
      6. Apply output gain. Done.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetManager.h"

namespace ParamIDs
{
    static constexpr const char* threshold   = "threshold";
    static constexpr const char* ceiling     = "ceiling";
    static constexpr const char* release     = "release";
    static constexpr const char* knee        = "knee";
    static constexpr const char* inGain      = "inGain";
    static constexpr const char* outGain     = "outGain";
    static constexpr const char* lookahead   = "lookahead";
    static constexpr const char* autoRelease = "autoRelease";
    static constexpr const char* stereoLink  = "stereoLink";
    static constexpr const char* bypass      = "bypass";
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout LT1AudioProcessor::createParameterLayout()
{
    using AP   = juce::AudioParameterFloat;
    using APB  = juce::AudioParameterBool;
    using Range = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<AP> (juce::ParameterID { ParamIDs::threshold,  1 },
                                       "Threshold", Range (-30.0f, 0.0f, 0.01f), -1.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<AP> (juce::ParameterID { ParamIDs::ceiling,    1 },
                                       "Ceiling", Range (-3.0f, 0.0f, 0.01f), -0.3f,
                                       juce::AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<AP> (juce::ParameterID { ParamIDs::release,    1 },
                                       "Release", Range (1.0f, 1000.0f, 0.1f, 0.4f), 120.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("ms")));

    p.push_back (std::make_unique<AP> (juce::ParameterID { ParamIDs::knee,       1 },
                                       "Knee", Range (0.0f, 12.0f, 0.01f), 2.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<AP> (juce::ParameterID { ParamIDs::inGain,     1 },
                                       "Input Gain", Range (-12.0f, 24.0f, 0.01f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<AP> (juce::ParameterID { ParamIDs::outGain,    1 },
                                       "Output Gain", Range (-24.0f, 12.0f, 0.01f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<APB> (juce::ParameterID { ParamIDs::lookahead, 1 },
                                        "Lookahead", true));
    p.push_back (std::make_unique<APB> (juce::ParameterID { ParamIDs::autoRelease, 1 },
                                        "Auto Release", true));
    p.push_back (std::make_unique<APB> (juce::ParameterID { ParamIDs::stereoLink, 1 },
                                        "Stereo Link", true));
    p.push_back (std::make_unique<APB> (juce::ParameterID { ParamIDs::bypass,    1 },
                                        "Bypass", false));

    return { p.begin(), p.end() };
}

//==============================================================================
LT1AudioProcessor::LT1AudioProcessor()
   #ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
   #else
     :
   #endif
       apvts (*this, nullptr, "LT1State", createParameterLayout())
{
    pThreshold   = apvts.getRawParameterValue (ParamIDs::threshold);
    pCeiling     = apvts.getRawParameterValue (ParamIDs::ceiling);
    pRelease     = apvts.getRawParameterValue (ParamIDs::release);
    pKnee        = apvts.getRawParameterValue (ParamIDs::knee);
    pInGain      = apvts.getRawParameterValue (ParamIDs::inGain);
    pOutGain     = apvts.getRawParameterValue (ParamIDs::outGain);
    pLookahead   = apvts.getRawParameterValue (ParamIDs::lookahead);
    pAutoRelease = apvts.getRawParameterValue (ParamIDs::autoRelease);
    pStereoLink  = apvts.getRawParameterValue (ParamIDs::stereoLink);
    pBypass      = apvts.getRawParameterValue (ParamIDs::bypass);

    presetManager = std::make_unique<PresetManager> (apvts);

    for (auto& a : scopeIn)   a.store (0.0f);
    for (auto& a : scopeOut)  a.store (0.0f);
    for (auto& a : scopeGain) a.store (1.0f);
}

LT1AudioProcessor::~LT1AudioProcessor() = default;

//==============================================================================
const juce::String LT1AudioProcessor::getName() const          { return JucePlugin_Name; }
bool LT1AudioProcessor::acceptsMidi() const                    { return false; }
bool LT1AudioProcessor::producesMidi() const                   { return false; }
bool LT1AudioProcessor::isMidiEffect() const                   { return false; }
double LT1AudioProcessor::getTailLengthSeconds() const         { return 0.0; }

int LT1AudioProcessor::getNumPrograms()                        { return 1; }
int LT1AudioProcessor::getCurrentProgram()                     { return 0; }
void LT1AudioProcessor::setCurrentProgram (int)                {}
const juce::String LT1AudioProcessor::getProgramName (int)     { return {}; }
void LT1AudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void LT1AudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // 5 ms lookahead — fixed length, the bool flag toggles whether we use it.
    lookaheadSamples = juce::jmax (1, (int) std::round (sampleRate * 0.005));
    peakWindowSize   = lookaheadSamples;

    delayLines.assign (2, std::vector<float> (lookaheadSamples, 0.0f));
    peakWindow.assign (peakWindowSize, 0.0f);

    delayWrite   = 0;
    peakWriteIdx = 0;
    envelope     = 1.0f;

    setLatencySamples (lookaheadSamples);
}

void LT1AudioProcessor::releaseResources()
{
    delayLines.clear();
    peakWindow.clear();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LT1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}
#endif

//==============================================================================
void LT1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh      = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    // Clear any extra output channels.
    for (int i = numCh; i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    if (numCh == 0 || numSamples == 0)
        return;

    const float thresholdDb = pThreshold->load();
    const float ceilingDb   = pCeiling->load();
    const float releaseMs   = pRelease->load();
    const float kneeDb      = juce::jmax (0.0f, pKnee->load());
    const float inGainLin   = juce::Decibels::decibelsToGain (pInGain->load());
    const float outGainLin  = juce::Decibels::decibelsToGain (pOutGain->load());
    const bool  lookaheadOn = pLookahead->load() > 0.5f;
    const bool  autoRelease = pAutoRelease->load() > 0.5f;
    const bool  stereoLink  = pStereoLink->load() > 0.5f;
    const bool  bypass      = pBypass->load() > 0.5f;

    const float ceilingLin   = juce::Decibels::decibelsToGain (ceilingDb);
    const float thresholdLin = juce::Decibels::decibelsToGain (thresholdDb);

    // Per-sample release coefficient (one-pole, time = -1/ln(0.001) constants).
    const float releaseSec  = juce::jmax (0.001f, releaseMs * 0.001f);
    float relCoeff = std::exp (-1.0f / (float) (releaseSec * currentSampleRate));

    // Peak hold for the GR meter (slow falloff).
    float currentGRPeak = meterGRPeak.load();
    const float grHoldCoeff = std::exp (-1.0f / (float) (1.5 * currentSampleRate)); // 1.5 s

    // Block-wise peak meters.
    float inPeakL = 0.0f, inPeakR = 0.0f, outPeakL = 0.0f, outPeakR = 0.0f;

    auto* chL = buffer.getWritePointer (0);
    auto* chR = numCh > 1 ? buffer.getWritePointer (1) : chL;

    auto& dlL = delayLines[0];
    auto& dlR = delayLines[numCh > 1 ? 1 : 0];

    int scopeIdx = scopeWriteIndex.load();

    for (int n = 0; n < numSamples; ++n)
    {
        // 1) Input gain
        float inL = chL[n] * inGainLin;
        float inR = (numCh > 1 ? chR[n] : inL) * inGainLin;

        inPeakL = juce::jmax (inPeakL, std::abs (inL));
        inPeakR = juce::jmax (inPeakR, std::abs (inR));

        // 2) Push into the lookahead delay (write current, read delayed).
        const float delayedL = dlL[delayWrite];
        const float delayedR = dlR[delayWrite];
        dlL[delayWrite] = inL;
        if (numCh > 1) dlR[delayWrite] = inR;

        // 3) Sliding peak: feed the *new* sample(s) into the peak window.
        const float linkedNow = stereoLink
            ? juce::jmax (std::abs (inL), std::abs (inR))
            : std::abs (inL);                       // simple for the linked case
        peakWindow[peakWriteIdx] = linkedNow;

        // Brute-force max over the window — window is small (5 ms ≈ 240 samples
        // at 48 kHz) so this stays cheap and gives a true peak rather than an
        // approximation.
        float windowPeak = 0.0f;
        for (float v : peakWindow)
            if (v > windowPeak) windowPeak = v;

        // 4) Compute target gain for the upcoming peak.
        float targetGain = 1.0f;
        if (windowPeak > thresholdLin && windowPeak > 1.0e-9f)
        {
            // Map the over-threshold amount to a reduction. Soft knee blends
            // between unity and the hard-limit gain across `knee` dB above
            // the threshold.
            const float overDb = juce::Decibels::gainToDecibels (windowPeak) - thresholdDb;
            float reductionDb;

            if (kneeDb > 0.0f && overDb < kneeDb)
            {
                const float t = overDb / kneeDb;       // 0..1
                const float smooth = t * t * (3.0f - 2.0f * t);  // smoothstep
                reductionDb = -overDb * smooth;
            }
            else
            {
                reductionDb = -overDb;
            }

            // Hard guarantee: never let the held peak exceed the ceiling.
            const float gainAfter = juce::Decibels::decibelsToGain (
                juce::Decibels::gainToDecibels (windowPeak) + reductionDb);

            float effectiveGain = juce::Decibels::decibelsToGain (reductionDb);
            if (gainAfter > ceilingLin)
                effectiveGain *= ceilingLin / gainAfter;

            targetGain = effectiveGain;
        }

        // Auto-release: blend faster release when the input briefly spikes,
        // slower release when sustained loudness is present.
        float effectiveRelCoeff = relCoeff;
        if (autoRelease)
        {
            const float headroom = juce::jlimit (0.0f, 1.0f,
                (juce::Decibels::gainToDecibels (juce::jmax (windowPeak, 1.0e-9f)) - thresholdDb) / 12.0f);
            const float adaptiveSec = juce::jmap (headroom, 0.0f, 1.0f,
                                                   releaseSec * 0.4f, releaseSec * 1.4f);
            effectiveRelCoeff = std::exp (-1.0f / (float) (adaptiveSec * currentSampleRate));
        }

        // 5) Smooth: instant attack, exponential release.
        if (targetGain < envelope)
            envelope = targetGain;                                 // instant attack
        else
            envelope = effectiveRelCoeff * envelope + (1.0f - effectiveRelCoeff) * targetGain;

        envelope = juce::jlimit (0.0f, 1.0f, envelope);

        // 6) Apply gain to the delayed signal (lookahead) or to the live
        //    signal (no lookahead → small overshoot is allowed).
        const float gainToApply = envelope;
        const float xL = lookaheadOn ? delayedL : inL;
        const float xR = lookaheadOn ? delayedR : inR;

        float yL = xL * gainToApply;
        float yR = xR * gainToApply;

        // Brick-wall clamp — protects against any residual overshoot.
        yL = juce::jlimit (-ceilingLin, ceilingLin, yL);
        yR = juce::jlimit (-ceilingLin, ceilingLin, yR);

        // Output gain
        yL *= outGainLin;
        yR *= outGainLin;

        if (bypass)
        {
            yL = chL[n];
            yR = numCh > 1 ? chR[n] : chL[n];
        }

        chL[n] = yL;
        if (numCh > 1) chR[n] = yR;

        outPeakL = juce::jmax (outPeakL, std::abs (yL));
        outPeakR = juce::jmax (outPeakR, std::abs (yR));

        // GR peak hold (positive number = dB of reduction)
        const float grNowDb = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (juce::jmax (envelope, 1.0e-6f)));
        if (grNowDb > currentGRPeak) currentGRPeak = grNowDb;
        else                         currentGRPeak *= grHoldCoeff;

        // Feed the visualisation buffers (decimate to keep traffic light).
        if ((n & 3) == 0)
        {
            scopeIn  [scopeIdx].store (lookaheadOn ? delayedL : inL);
            scopeOut [scopeIdx].store (yL);
            scopeGain[scopeIdx].store (envelope);
            scopeIdx = (scopeIdx + 1) % scopeSize;
        }

        // Advance ring positions.
        delayWrite   = (delayWrite + 1) % lookaheadSamples;
        peakWriteIdx = (peakWriteIdx + 1) % peakWindowSize;
    }

    scopeWriteIndex.store (scopeIdx);

    // ====== Update meters ======
    auto smoothMeter = [] (std::atomic<float>& a, float target)
    {
        // Quick attack, slow decay so the bar feels like a real VU.
        const float prev = a.load();
        const float coeff = (target > prev) ? 0.5f : 0.05f;
        a.store (prev + coeff * (target - prev));
    };
    smoothMeter (meterInL,  inPeakL);
    smoothMeter (meterInR,  inPeakR);
    smoothMeter (meterOutL, outPeakL);
    smoothMeter (meterOutR, outPeakR);

    const float blockGRDb = juce::jmax (0.0f, -juce::Decibels::gainToDecibels (juce::jmax (envelope, 1.0e-6f)));
    smoothMeter (meterGR, blockGRDb);
    meterGRPeak.store (currentGRPeak);
}

//==============================================================================
bool LT1AudioProcessor::hasEditor() const                            { return true; }
juce::AudioProcessorEditor* LT1AudioProcessor::createEditor()        { return new LT1AudioProcessorEditor (*this); }

//==============================================================================
void LT1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LT1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LT1AudioProcessor();
}
