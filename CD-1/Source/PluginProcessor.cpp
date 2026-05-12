/*
  ==============================================================================

    PluginProcessor.cpp
    CD-1 Cinematic Drums

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
CD1AudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using Attr       = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- per-drum parameters ----
    const char* prefixes[] = { "boom", "hit", "crack", "sub" };
    const char* names[]    = { "Boom", "Hit", "Crack", "Sub" };

    for (int d = 0; d < cd1::NumDrums; ++d)
    {
        const juce::String pfx (prefixes[d]);
        const juce::String nm  (names[d]);

        // Tune in semitones around home freq.  ±12 st = ±1 octave.
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { pfx + "Tune", 1 }, nm + " Tune",
            juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
            Attr().withStringFromValueFunction (
                [](float v, int)
                {
                    if (std::abs (v) < 0.05f) return juce::String ("0 st");
                    return juce::String (v >= 0.0f ? "+" : "") + juce::String (v, 1) + " st";
                })));

        // Decay scale (1.0 = factory default)
        {
            juce::NormalisableRange<float> r (0.25f, 3.0f, 0.001f);
            r.setSkewForCentre (1.0f);
            layout.add (std::make_unique<FloatParam> (
                juce::ParameterID { pfx + "Decay", 1 }, nm + " Decay", r, 1.0f,
                Attr().withStringFromValueFunction (
                    [](float v, int) { return juce::String (v, 2) + "x"; })));
        }

        // Level (0..1.5)
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { pfx + "Level", 1 }, nm + " Level",
            juce::NormalisableRange<float> (0.0f, 1.5f, 0.001f), 1.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; })));

        // Pan (-1..1)
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { pfx + "Pan", 1 }, nm + " Pan",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
            Attr().withStringFromValueFunction (
                [](float v, int)
                {
                    if (std::abs (v) < 0.02f) return juce::String ("C");
                    return (v < 0.0f ? "L" : "R")
                            + juce::String ((int) std::round (std::abs (v) * 100.0f));
                })));
    }

    // ---- master macros ----
    auto pct = [](float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; };

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "depth",  1 }, "Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
        Attr().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "impact", 1 }, "Impact",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
        Attr().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "air",    1 }, "Air",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
        Attr().withStringFromValueFunction (
            [](float v, int)
            {
                if (std::abs (v) < 0.01f) return juce::String ("Flat");
                return juce::String (v >= 0.0f ? "+" : "") + juce::String (v * 9.0f, 1) + " dB";
            })));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "drive",  1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.20f,
        Attr().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "width",  1 }, "Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.70f,
        Attr().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "size",   1 }, "Size",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.40f,
        Attr().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "tone",   1 }, "Tone",
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

    // ---- reverb internals ----
    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "rvSize", 1 }, "Reverb Size",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.45f,
        Attr().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<FloatParam> (
        juce::ParameterID { "rvDamp", 1 }, "Reverb Damp",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.50f,
        Attr().withStringFromValueFunction (pct)));

    {
        juce::NormalisableRange<float> r (20.0f, 600.0f, 1.0f);
        r.setSkewForCentre (90.0f);
        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { "rvLow",  1 }, "Reverb Low Cut", r, 90.0f,
            Attr().withStringFromValueFunction (
                [](float v, int) { return juce::String ((int) v) + " Hz"; })));
    }

    return layout;
}

//==============================================================================
CD1AudioProcessor::CD1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "CD1State", createParameterLayout()),
       presetManager (apvts)
#else
     : apvts (*this, nullptr, "CD1State", createParameterLayout()),
       presetManager (apvts)
#endif
{
    const char* prefixes[] = { "boom", "hit", "crack", "sub" };
    for (int d = 0; d < cd1::NumDrums; ++d)
    {
        const juce::String pfx (prefixes[d]);
        drumP[d].tune  = apvts.getRawParameterValue (pfx + "Tune");
        drumP[d].decay = apvts.getRawParameterValue (pfx + "Decay");
        drumP[d].level = apvts.getRawParameterValue (pfx + "Level");
        drumP[d].pan   = apvts.getRawParameterValue (pfx + "Pan");
    }

    depthP   = apvts.getRawParameterValue ("depth");
    impactP  = apvts.getRawParameterValue ("impact");
    airP     = apvts.getRawParameterValue ("air");
    driveP   = apvts.getRawParameterValue ("drive");
    widthP   = apvts.getRawParameterValue ("width");
    sizeP    = apvts.getRawParameterValue ("size");
    toneP    = apvts.getRawParameterValue ("tone");
    outputP  = apvts.getRawParameterValue ("output");

    rvSizeP  = apvts.getRawParameterValue ("rvSize");
    rvDampP  = apvts.getRawParameterValue ("rvDamp");
    rvLowP   = apvts.getRawParameterValue ("rvLow");

    dispL.assign (kDispSize, 0.0f);
    dispR.assign (kDispSize, 0.0f);
}

CD1AudioProcessor::~CD1AudioProcessor() = default;

//==============================================================================
const juce::String CD1AudioProcessor::getName() const            { return JucePlugin_Name; }
bool   CD1AudioProcessor::acceptsMidi() const                    { return true;  }
bool   CD1AudioProcessor::producesMidi() const                   { return false; }
bool   CD1AudioProcessor::isMidiEffect() const                   { return false; }
double CD1AudioProcessor::getTailLengthSeconds() const           { return 4.0; }

int    CD1AudioProcessor::getNumPrograms()                                  { return 1; }
int    CD1AudioProcessor::getCurrentProgram()                               { return 0; }
void   CD1AudioProcessor::setCurrentProgram (int)                           {}
const  juce::String CD1AudioProcessor::getProgramName (int)                 { return {}; }
void   CD1AudioProcessor::changeProgramName (int, const juce::String&)      {}

//==============================================================================
void CD1AudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    for (auto& v : voices) v.prepare (sampleRate);
    reverb.prepare (sampleRate);

    // tilt EQ pivot ~700 Hz
    const float fc = 700.0f;
    tiltCoeff = std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sampleRate);
    tiltLP_L  = tiltLP_R = 0.0f;

    // air shelf split (HP for "high" component)
    const float airFc = 4500.0f;
    airCoeff = std::exp (-juce::MathConstants<float>::twoPi * airFc / (float) sampleRate);
    airHP_L  = airHP_R = 0.0f;

    dispWrite.store (0);
    dispLastRead = 0;
}

void CD1AudioProcessor::releaseResources()
{
    for (auto& v : voices) v.reset();
    reverb.reset();
    tiltLP_L = tiltLP_R = 0.0f;
    airHP_L  = airHP_R  = 0.0f;
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CD1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}
#endif

//==============================================================================
cd1::MasterMacros CD1AudioProcessor::buildMasterSnapshot() const noexcept
{
    cd1::MasterMacros m;
    if (depthP)  m.depth   = depthP ->load();
    if (impactP) m.impact  = impactP->load();
    if (airP)    m.air     = airP   ->load();
    if (driveP)  m.drive   = driveP ->load();
    if (widthP)  m.width   = widthP ->load();
    if (sizeP)   m.size    = sizeP  ->load();
    if (toneP)   m.tone    = toneP  ->load();
    if (outputP) m.outputDb = outputP->load();
    return m;
}

int CD1AudioProcessor::findFreeVoice() noexcept
{
    int best = 0;
    int bestAge = -1;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (! voices[i].isActive())
            return i;
        const int age = voices[i].getAgeSamples();
        if (age > bestAge) { bestAge = age; best = i; }
    }
    return best; // steal oldest
}

void CD1AudioProcessor::triggerDrum (int drumIdx, float velocity) noexcept
{
    if (! juce::isPositiveAndBelow (drumIdx, (int) cd1::NumDrums))
        return;

    const auto m = buildMasterSnapshot();

    const float tune  = drumP[drumIdx].tune  ? drumP[drumIdx].tune ->load() : 0.0f;
    const float decay = drumP[drumIdx].decay ? drumP[drumIdx].decay->load() : 1.0f;
    const float lvl   = drumP[drumIdx].level ? drumP[drumIdx].level->load() : 1.0f;
    const float pan   = drumP[drumIdx].pan   ? drumP[drumIdx].pan  ->load() : 0.0f;

    const auto p = cd1::buildVoiceParams (drumIdx, tune, decay, lvl, pan, m);

    const int v = findFreeVoice();
    voices[v].trigger (p, velocity);

    pushTriggerEvent (drumIdx, velocity);
}

void CD1AudioProcessor::pushTriggerEvent (int drumIdx, float velocity) noexcept
{
    const int w = eventWrite.load (std::memory_order_relaxed);
    eventQueue[w % kTriggerQueueSize] = { drumIdx, velocity };
    eventWrite.store (w + 1, std::memory_order_release);
}

bool CD1AudioProcessor::consumeTriggerEvent (TriggerEvent& outEvent) noexcept
{
    const int w = eventWrite.load (std::memory_order_acquire);
    int r = eventRead .load (std::memory_order_relaxed);
    if (r == w) return false;
    const auto& q = eventQueue[r % kTriggerQueueSize];
    outEvent.drumIdx  = q.drumIdx;
    outEvent.velocity = q.velocity;
    eventRead.store (r + 1, std::memory_order_release);
    return true;
}

void CD1AudioProcessor::requestAudition (int drumIdx, float velocity) noexcept
{
    auditionDrum     .store (juce::jlimit (0, (int) cd1::NumDrums - 1, drumIdx));
    auditionVelocity .store (juce::jlimit (0.05f, 1.0f, velocity));
    auditionPending  .fetch_add (1, std::memory_order_release);
}

void CD1AudioProcessor::copyDisplayAudio (float* destL, float* destR, int& numOut, int maxSamples) noexcept
{
    const int w = dispWrite.load (std::memory_order_acquire);
    int avail = w - dispLastRead;
    if (avail < 0) avail += kDispSize;  // wrapped, treat conservatively
    if (avail > kDispSize) avail = kDispSize;

    const int n = juce::jmin (avail, maxSamples);
    for (int i = 0; i < n; ++i)
    {
        const int idx = (dispLastRead + i) % kDispSize;
        destL[i] = dispL[(size_t) idx];
        destR[i] = dispR[(size_t) idx];
    }
    dispLastRead = (dispLastRead + n) % kDispSize;
    numOut = n;
}

void CD1AudioProcessor::getMasterPeak (float& L, float& R) noexcept
{
    L = peakL.load();  R = peakR.load();
    peakL.store (L * 0.6f);
    peakR.store (R * 0.6f);
}

//==============================================================================
void CD1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0)
        return;

    // ---- consume audition request from UI ----
    int pending = auditionPending.exchange (0, std::memory_order_acquire);
    if (pending > 0)
        triggerDrum (auditionDrum.load (std::memory_order_relaxed),
                     auditionVelocity.load (std::memory_order_relaxed));

    auto* outL = buffer.getWritePointer (0);
    auto* outR = numCh > 1 ? buffer.getWritePointer (1) : outL;

    // master macros (block-rate snapshot)
    const float driveAmt   = driveP   ? driveP   ->load() : 0.2f;
    const float toneAmt    = toneP    ? toneP    ->load() : 0.0f;
    const float airAmt     = airP     ? airP     ->load() : 0.0f;
    const float outDb      = outputP  ? outputP  ->load() : 0.0f;
    const float widthAmt   = widthP   ? widthP   ->load() : 0.7f;
    const float sizeAmt    = sizeP    ? sizeP    ->load() : 0.4f;
    const float rvSizeRaw  = rvSizeP  ? rvSizeP  ->load() : 0.45f;
    const float rvDampRaw  = rvDampP  ? rvDampP  ->load() : 0.50f;
    const float rvLowHz    = rvLowP   ? rvLowP   ->load() : 90.0f;

    reverb.setSize  (rvSizeRaw);
    reverb.setDamping (rvDampRaw);
    reverb.setLowCut (rvLowHz);
    reverb.setWidth  (juce::jlimit (0.0f, 1.0f, widthAmt));
    reverb.setMix    (juce::jlimit (0.0f, 1.0f, sizeAmt));

    const float outGain = juce::Decibels::decibelsToGain (outDb);

    // mid/side widening coefficient
    const float msWide = juce::jlimit (0.5f, 1.5f, 1.0f + (widthAmt - 0.5f) * 1.2f);

    // sample loop with sample-accurate MIDI
    auto midiIt = midi.begin();
    float pkL = peakL.load(), pkR = peakR.load();

    for (int n = 0; n < numSamples; ++n)
    {
        // ---- handle MIDI note-ons at this sample position ----
        while (midiIt != midi.end())
        {
            const auto meta = *midiIt;
            if (meta.samplePosition > n)
                break;

            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                const int  noteN = m.getNoteNumber();
                const float vel  = juce::jlimit (0.05f, 1.0f, (float) m.getFloatVelocity());

                // Map MIDI note → drum index.
                //   C1=36 BOOM, D1=38 HIT, E1=40 CRACK, F1=41 SUB
                //   plus convenient secondary range C2..F2 (48..53) and C3..F3 (60..65).
                int drum = -1;
                switch (noteN % 12)
                {
                    case 0:  drum = cd1::Boom;  break;  // C
                    case 2:  drum = cd1::Hit;   break;  // D
                    case 4:  drum = cd1::Crack; break;  // E
                    case 5:  drum = cd1::Sub;   break;  // F
                    default: break;
                }
                if (drum < 0)
                {
                    // any other note: mod-cycle through the drums
                    drum = (noteN - 36) % cd1::NumDrums;
                    if (drum < 0) drum += cd1::NumDrums;
                }
                triggerDrum (drum, vel);
            }
            ++midiIt;
        }

        // ---- mix all active voices ----
        float L = 0.0f, R = 0.0f;
        for (auto& v : voices)
        {
            if (v.isActive())
            {
                float vL, vR;
                v.renderSample (vL, vR);
                L += vL;
                R += vR;
            }
        }

        // ---- master drive (soft tanh) ----
        if (driveAmt > 0.001f)
        {
            const float k = 1.0f + driveAmt * 4.0f;
            L = std::tanh (L * k) * (1.0f + driveAmt * 0.5f);
            R = std::tanh (R * k) * (1.0f + driveAmt * 0.5f);
        }

        // ---- tilt EQ around 700 Hz ----
        if (std::abs (toneAmt) > 0.005f)
        {
            tiltLP_L = tiltLP_L * tiltCoeff + L * (1.0f - tiltCoeff);
            tiltLP_R = tiltLP_R * tiltCoeff + R * (1.0f - tiltCoeff);
            const float lowL = tiltLP_L, highL = L - tiltLP_L;
            const float lowR = tiltLP_R, highR = R - tiltLP_R;
            L += lowL * toneAmt * 0.7f - highL * toneAmt * 0.7f;
            R += lowR * toneAmt * 0.7f - highR * toneAmt * 0.7f;
        }

        // ---- "Air" high shelf (high boost / cut) ----
        if (std::abs (airAmt) > 0.005f)
        {
            airHP_L = airHP_L * airCoeff + L * (1.0f - airCoeff);
            airHP_R = airHP_R * airCoeff + R * (1.0f - airCoeff);
            const float hiL = L - airHP_L;
            const float hiR = R - airHP_R;
            L += hiL * airAmt * 0.9f;
            R += hiR * airAmt * 0.9f;
        }

        // ---- mid/side stereo widener ----
        {
            const float mid  = 0.5f * (L + R);
            const float side = 0.5f * (L - R) * msWide;
            L = mid + side;
            R = mid - side;
        }

        // ---- room reverb (additive wet) ----
        reverb.process (L, R, L, R);

        // ---- output gain ----
        L *= outGain;
        R *= outGain;

        // ---- write to host ----
        outL[n] = L;
        outR[n] = R;

        // ---- meters ----
        const float aL = std::abs (L);
        const float aR = std::abs (R);
        if (aL > pkL) pkL = aL;
        if (aR > pkR) pkR = aR;

        // ---- display ring buffer ----
        const int dw = dispWrite.load (std::memory_order_relaxed);
        dispL[(size_t)(dw % kDispSize)] = L;
        dispR[(size_t)(dw % kDispSize)] = R;
        dispWrite.store ((dw + 1) % kDispSize, std::memory_order_release);
    }

    peakL.store (pkL);
    peakR.store (pkR);
}

//==============================================================================
bool CD1AudioProcessor::hasEditor() const                        { return true; }
juce::AudioProcessorEditor* CD1AudioProcessor::createEditor()    { return new CD1AudioProcessorEditor (*this); }

//==============================================================================
void CD1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", presetManager.getCurrentPresetName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CD1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new CD1AudioProcessor();
}
