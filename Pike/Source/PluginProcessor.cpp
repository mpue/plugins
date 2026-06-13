/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterLayout.h"
#include "params/ParameterIDs.h"
#include "synth/PikeSound.h"

//==============================================================================
PikeAudioProcessor::PikeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMETERS", pike::createParameterLayout())
#else
     : apvts (*this, nullptr, "PARAMETERS", pike::createParameterLayout())
#endif
{
    cacheParameterPointers();

    presetManager.onResetNonParamState = [this] { msegStore.resetAll(); };

    synth.addSound (new pike::PikeSound());
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new pike::PikeVoice (voiceParameters));

    synth.setNoteStealingEnabled (true);
}

void PikeAudioProcessor::cacheParameterPointers()
{
    voiceParameters.ampAttack  = apvts.getRawParameterValue (pid::ampAttack);
    voiceParameters.ampDecay   = apvts.getRawParameterValue (pid::ampDecay);
    voiceParameters.ampSustain = apvts.getRawParameterValue (pid::ampSustain);
    voiceParameters.ampRelease = apvts.getRawParameterValue (pid::ampRelease);

    for (int n = 0; n < pike::PikeVoice::numOscillators; ++n)
    {
        auto& o = voiceParameters.osc[n];
        o.wave       = apvts.getRawParameterValue (pid::oscWave[n]);
        o.octave     = apvts.getRawParameterValue (pid::oscOctave[n]);
        o.semi       = apvts.getRawParameterValue (pid::oscSemi[n]);
        o.fine       = apvts.getRawParameterValue (pid::oscFine[n]);
        o.level      = apvts.getRawParameterValue (pid::oscLevel[n]);
        o.pulseWidth = apvts.getRawParameterValue (pid::oscPW[n]);
        o.wtPos      = apvts.getRawParameterValue (pid::oscWtPos[n]);
    }

    voiceParameters.osc2Sync     = apvts.getRawParameterValue (pid::osc2Sync);
    voiceParameters.osc3Sync     = apvts.getRawParameterValue (pid::osc3Sync);
    voiceParameters.fmAmount     = apvts.getRawParameterValue (pid::fmAmount);
    voiceParameters.ringModLevel = apvts.getRawParameterValue (pid::ringModLevel);
    voiceParameters.noiseLevel   = apvts.getRawParameterValue (pid::noiseLevel);

    voiceParameters.filterType      = apvts.getRawParameterValue (pid::filterType);
    voiceParameters.filterSlope     = apvts.getRawParameterValue (pid::filterSlope);
    voiceParameters.filterCutoff    = apvts.getRawParameterValue (pid::filterCutoff);
    voiceParameters.filterResonance = apvts.getRawParameterValue (pid::filterResonance);
    voiceParameters.filterKeyTrack  = apvts.getRawParameterValue (pid::filterKeyTrack);
    voiceParameters.filterDrive     = apvts.getRawParameterValue (pid::filterDrive);
    voiceParameters.filterEnvAmount = apvts.getRawParameterValue (pid::filterEnvAmount);

    voiceParameters.filtAttack  = apvts.getRawParameterValue (pid::filtAttack);
    voiceParameters.filtDecay   = apvts.getRawParameterValue (pid::filtDecay);
    voiceParameters.filtSustain = apvts.getRawParameterValue (pid::filtSustain);
    voiceParameters.filtRelease = apvts.getRawParameterValue (pid::filtRelease);

    voiceParameters.auxAttack  = apvts.getRawParameterValue (pid::auxAttack);
    voiceParameters.auxDecay   = apvts.getRawParameterValue (pid::auxDecay);
    voiceParameters.auxSustain = apvts.getRawParameterValue (pid::auxSustain);
    voiceParameters.auxRelease = apvts.getRawParameterValue (pid::auxRelease);

    for (int l = 0; l < pike::PikeVoice::numLfos; ++l)
    {
        auto& lp = voiceParameters.lfo[l];
        lp.shape   = apvts.getRawParameterValue (pid::lfoShape[l]);
        lp.keySync = apvts.getRawParameterValue (pid::lfoKeySync[l]);
        lp.mono    = apvts.getRawParameterValue (pid::lfoMono[l]);
        lp.fadeIn  = apvts.getRawParameterValue (pid::lfoFade[l]);
        lp.inc       = &lfoIncShared[l];
        lp.monoPhase = &lfoMonoPhaseShared[l];
    }

    voiceParameters.modWheel   = &modWheelShared;
    voiceParameters.aftertouch = &aftertouchShared;
    voiceParameters.glideTime  = apvts.getRawParameterValue (pid::glideTime);

    for (int s = 0; s < pike::mod::numSlots; ++s)
    {
        auto& slot = voiceParameters.matrix[s];
        slot.source = apvts.getRawParameterValue (pid::modSourceId (s));
        slot.dest   = apvts.getRawParameterValue (pid::modDestId (s));
        slot.depth  = apvts.getRawParameterValue (pid::modDepthId (s));
    }

    for (int m = 0; m < pike::mseg::maxMsegs; ++m)
    {
        auto& mp = voiceParameters.msegs[m];
        mp.data = &msegAudio[m];
        mp.inc  = &msegIncShared[m];
        mp.loop = apvts.getRawParameterValue (pid::msegLoop[m]);
    }

    voiceParameters.wavetable = &wavetable;
}

void PikeAudioProcessor::updateModulationRuntime (const juce::MidiBuffer& midi, int numSamples)
{
    // Capture the latest mod-wheel (CC1), channel pressure and poly aftertouch.
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (m.isController() && m.getControllerNumber() == 1)
            modWheelShared.store ((float) m.getControllerValue() / 127.0f);
        else if (m.isChannelPressure())
            aftertouchShared.store ((float) m.getChannelPressureValue() / 127.0f);
        else if (m.isAftertouch())
            aftertouchShared.store ((float) m.getAfterTouchValue() / 127.0f);
    }

    // Tempo for synced LFO rates and delay.
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;
    currentBpm = bpm;

    // LFO sync divisions: cycles per beat for "1/1".."1/32".
    static constexpr float divFactor[] = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 8.0f };

    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;

    for (int l = 0; l < pike::PikeVoice::numLfos; ++l)
    {
        const bool  sync = apvts.getRawParameterValue (pid::lfoSync[l])->load() > 0.5f;
        double hz;
        if (sync)
        {
            const int div = juce::jlimit (0, (int) std::size (divFactor) - 1,
                                          (int) apvts.getRawParameterValue (pid::lfoDiv[l])->load());
            hz = bpm / 60.0 * divFactor[div];
        }
        else
        {
            hz = apvts.getRawParameterValue (pid::lfoRate[l])->load();
        }

        const double inc = hz / sr;
        lfoIncShared[l].store ((float) inc);
        lfoMonoPhaseShared[l].store ((float) lfoMonoPhaseAccum[l]);

        lfoMonoPhaseAccum[l] += inc * numSamples;
        lfoMonoPhaseAccum[l] -= std::floor (lfoMonoPhaseAccum[l]);
    }

    // MSEG total lengths: "1/16".."4 Bars" in beats when synced, else seconds.
    static constexpr double msegBeats[] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };

    for (int m = 0; m < pike::mseg::maxMsegs; ++m)
    {
        msegStore.shared[m].readIfChanged (msegAudio[m], msegSeqSeen[m]);

        const bool sync = apvts.getRawParameterValue (pid::msegSync[m])->load() > 0.5f;
        double seconds;
        if (sync)
        {
            const int div = juce::jlimit (0, (int) std::size (msegBeats) - 1,
                                          (int) apvts.getRawParameterValue (pid::msegDiv[m])->load());
            seconds = msegBeats[div] * 60.0 / bpm;
        }
        else
        {
            seconds = apvts.getRawParameterValue (pid::msegRate[m])->load();
        }

        msegIncShared[m].store ((float) (1.0 / juce::jmax (1.0e-3, seconds * sr)));
    }
}

PikeAudioProcessor::~PikeAudioProcessor()
{
}

//==============================================================================
const juce::String PikeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PikeAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PikeAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PikeAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PikeAudioProcessor::getTailLengthSeconds() const
{
    // Reverb and feedback delay can ring after notes end; report a generous tail.
    return 5.0;
}

int PikeAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PikeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PikeAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String PikeAudioProcessor::getProgramName (int index)
{
    return {};
}

void PikeAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void PikeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    // Build the shared wavetable bank for this sample rate (not realtime).
    wavetable.prepare (sampleRate);

    fxChain.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    eq.prepare (sampleRate, samplesPerBlock);

    synth.setCurrentPlaybackSampleRate (sampleRate);

    // Propagate the sample rate to every voice's DSP.
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = synth.getVoice (i))
            voice->setCurrentPlaybackSampleRate (sampleRate);

    arpeggiator.prepare (sampleRate);
    voiceManager.prepare (synth);
    emptyMidi.ensureSize (16);
}

void PikeAudioProcessor::releaseResources()
{
    // Nothing allocated outside prepareToPlay; voices reset themselves on note-off.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PikeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void PikeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Synth instrument: no audio input. Start from silence, then let the voices
    // render the active notes into the buffer.
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    // MIDI Learn: bind/apply CC -> parameter mappings.
    midiLearn.processMidi (midiMessages);

    // Tempo + MIDI mod sources (also sets currentBpm).
    updateModulationRuntime (midiMessages, numSamples);

    // Arpeggiator transforms the note stream (passes other messages through).
    arpeggiator.process (midiMessages, readArpParams(), currentBpm, numSamples);

    // Voice-mode dispatch + sample-accurate rendering.
    voiceManager.setConfig ((int) apvts.getRawParameterValue (pid::voiceMode)->load(),
                            (int) apvts.getRawParameterValue (pid::unisonCount)->load(),
                            apvts.getRawParameterValue (pid::unisonDetune)->load());
    renderVoices (buffer, midiMessages);

    // Global FX chain (Distortion -> Chorus -> Delay -> Reverb).
    fxChain.process (buffer, readFxParams(), currentBpm);

    // Mix / EQ (8-band parametric).
    updateEqFromParams();
    if (eq.isEnabled() && buffer.getNumChannels() >= 1)
    {
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
        eq.processStereo (L, R, numSamples);
    }

    // Master gain (dB -> linear), applied to the whole block.
    const float gainDb     = apvts.getRawParameterValue (pid::masterGain)->load();
    const float gainLinear = juce::Decibels::decibelsToGain (gainDb);
    buffer.applyGain (gainLinear);

    // Stereo width (mid/side): 0 = mono, 1 = normal, 2 = wide.
    const float width = apvts.getRawParameterValue (pid::stereoWidth)->load();
    if (buffer.getNumChannels() >= 2 && std::abs (width - 1.0f) > 1.0e-4f)
    {
        auto* wL = buffer.getWritePointer (0);
        auto* wR = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = 0.5f * (wL[i] + wR[i]);
            const float side = 0.5f * (wL[i] - wR[i]) * width;
            wL[i] = mid + side;
            wR[i] = mid - side;
        }
    }

    // ----- publish visualisation data (realtime-safe) -----
    const int   numCh = buffer.getNumChannels();
    const float peakL = numCh > 0 ? buffer.getMagnitude (0, 0, numSamples) : 0.0f;
    const float peakR = numCh > 1 ? buffer.getMagnitude (1, 0, numSamples) : peakL;
    visualState.meterL.store (peakL, std::memory_order_relaxed);
    visualState.meterR.store (peakR, std::memory_order_relaxed);

    const float* L = numCh > 0 ? buffer.getReadPointer (0) : nullptr;
    const float* R = numCh > 1 ? buffer.getReadPointer (1) : L;
    if (L != nullptr)
        for (int i = 0; i < numSamples; ++i)
            visualState.pushScope (0.5f * (L[i] + R[i]));

    publishMsegPhases();
}

void PikeAudioProcessor::publishMsegPhases()
{
    // Representative voice: the most recently triggered one still sounding.
    const pike::PikeVoice* best = nullptr;
    int bestAge = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<pike::PikeVoice*> (synth.getVoice (i)))
            if (v->isSounding())
            {
                const int age = v->samplesSinceNoteOnCount();
                if (best == nullptr || age < bestAge) { bestAge = age; best = v; }
            }

    for (int m = 0; m < pike::mseg::maxMsegs; ++m)
        visualState.msegPhase[m].store (
            best != nullptr && best->msegIsActive (m) ? best->msegPosition (m) : -1.0f,
            std::memory_order_relaxed);
}

void PikeAudioProcessor::updateEqFromParams()
{
    eq.setEnabled (apvts.getRawParameterValue (pid::eqOn)->load() > 0.5f);
    for (int b = 0; b < pike::ParametricEQ::numBands; ++b)
    {
        eq.setBandFrequency (b, apvts.getRawParameterValue (pid::eqFreq[b])->load());
        eq.setBandGain      (b, apvts.getRawParameterValue (pid::eqGain[b])->load());
        eq.setBandQ         (b, apvts.getRawParameterValue (pid::eqQ[b])->load());
    }
}

pike::FxChain::Params PikeAudioProcessor::readFxParams() const
{
    auto val  = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto flag = [this] (const char* id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };

    pike::FxChain::Params p;

    p.distOn    = flag (pid::distOn);
    p.distType  = (int) val (pid::distType);
    p.distDrive = val (pid::distDrive);
    p.distMix   = val (pid::distMix);

    p.chorusOn       = flag (pid::chorusOn);
    p.chorusRate     = val (pid::chorusRate);
    p.chorusDepth    = val (pid::chorusDepth);
    p.chorusFeedback = val (pid::chorusFeedback);
    p.chorusMix      = val (pid::chorusMix);

    p.delayOn       = flag (pid::delayOn);
    p.delaySync     = flag (pid::delaySync);
    p.delayTimeMs   = val (pid::delayTime);
    p.delayDiv      = (int) val (pid::delayDiv);
    p.delayFeedback = val (pid::delayFeedback);
    p.delayMix      = val (pid::delayMix);
    p.delayPingpong = flag (pid::delayPingpong);

    p.reverbOn      = flag (pid::reverbOn);
    p.reverbSize    = val (pid::reverbSize);
    p.reverbDamping = val (pid::reverbDamping);
    p.reverbWidth   = val (pid::reverbWidth);
    p.reverbMix     = val (pid::reverbMix);

    return p;
}

pike::Arpeggiator::Params PikeAudioProcessor::readArpParams() const
{
    pike::Arpeggiator::Params p;
    p.enabled = apvts.getRawParameterValue (pid::arpOn)->load()    > 0.5f;
    p.mode    = (int) apvts.getRawParameterValue (pid::arpMode)->load();
    p.rateDiv = (int) apvts.getRawParameterValue (pid::arpRate)->load();
    p.gate    = apvts.getRawParameterValue (pid::arpGate)->load();
    p.octaves = (int) apvts.getRawParameterValue (pid::arpOctaves)->load();
    p.latch   = apvts.getRawParameterValue (pid::arpLatch)->load()  > 0.5f;
    return p;
}

void PikeAudioProcessor::renderVoices (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
{
    // Drive the VoiceManager from MIDI events, rendering the synth in segments
    // between events so note starts/stops are sample-accurate.
    const int numSamples = buffer.getNumSamples();
    int lastPos = 0;

    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, numSamples, meta.samplePosition);

        if (pos > lastPos)
        {
            synth.renderNextBlock (buffer, emptyMidi, lastPos, pos - lastPos);
            lastPos = pos;
        }

        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            voiceManager.noteOn (m.getNoteNumber(), m.getFloatVelocity());
            ++heldNoteCount;
            visualState.triggerId.fetch_add (1, std::memory_order_relaxed);
            visualState.gate.store (true, std::memory_order_relaxed);
        }
        else if (m.isNoteOff())
        {
            voiceManager.noteOff (m.getNoteNumber());
            heldNoteCount = juce::jmax (0, heldNoteCount - 1);
            visualState.gate.store (heldNoteCount > 0, std::memory_order_relaxed);
        }
    }

    if (lastPos < numSamples)
        synth.renderNextBlock (buffer, emptyMidi, lastPos, numSamples - lastPos);
}

//==============================================================================
bool PikeAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PikeAudioProcessor::createEditor()
{
    return new PikeAudioProcessorEditor (*this);
}

//==============================================================================
void PikeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Wrapper root: <PikeState> [ APVTS state ] [ MidiLearn ].
    juce::XmlElement root ("PikeState");
    if (auto pxml = apvts.copyState().createXml())
        root.addChildElement (pxml.release());
    midiLearn.writeTo (root);
    copyXmlToBinary (root, destData);
}

void PikeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    if (xml->hasTagName ("PikeState"))
    {
        midiLearn.readFrom (*xml);
        if (auto* pxml = xml->getChildByName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*pxml));
    }
    else if (xml->hasTagName (apvts.state.getType()))   // legacy (pre-wrapper) state
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PikeAudioProcessor();
}
