/*
  ==============================================================================

    PikeVoice.h
    One polyphonic voice. Signal path:
      3 oscillators (PolyBLEP + wavetable) -> mixer (+ring mod + noise)
      -> drive -> multimode filter -> amp VCA
    Modulation: 3 ADSR envelopes (amp/filter/aux), 2 LFOs, 16-slot mod matrix
    (per sample). Phase 7 adds glide (portamento) and per-voice unison detune;
    note dispatch (poly/mono/legato/unison) is handled by the VoiceManager,
    which drives voices directly.

    Per-block: parameters cached into plain members (one atomic read each).
    Per-sample: only cached values are used — no atomic loads in the inner loop.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PikeSound.h"
#include "../dsp/Oscillator.h"
#include "../dsp/Wavetable.h"
#include "../dsp/Noise.h"
#include "../dsp/Filter.h"
#include "../dsp/Lfo.h"
#include "../dsp/Mseg.h"
#include "../params/ModMatrixDefs.h"

namespace pike
{
    class PikeVoice : public juce::SynthesiserVoice
    {
    public:
        static constexpr int numOscillators = 3;
        static constexpr int numLfos        = 2;

        struct Parameters
        {
            // Amp / Filter / Aux envelopes
            std::atomic<float>* ampAttack  = nullptr;
            std::atomic<float>* ampDecay   = nullptr;
            std::atomic<float>* ampSustain = nullptr;
            std::atomic<float>* ampRelease = nullptr;

            std::atomic<float>* filtAttack  = nullptr;
            std::atomic<float>* filtDecay   = nullptr;
            std::atomic<float>* filtSustain = nullptr;
            std::atomic<float>* filtRelease = nullptr;

            std::atomic<float>* auxAttack  = nullptr;
            std::atomic<float>* auxDecay   = nullptr;
            std::atomic<float>* auxSustain = nullptr;
            std::atomic<float>* auxRelease = nullptr;

            struct Osc
            {
                std::atomic<float>* wave       = nullptr;
                std::atomic<float>* octave     = nullptr;
                std::atomic<float>* semi       = nullptr;
                std::atomic<float>* fine       = nullptr;
                std::atomic<float>* level      = nullptr;
                std::atomic<float>* pulseWidth = nullptr;
                std::atomic<float>* wtPos      = nullptr;
            } osc[numOscillators];

            std::atomic<float>* osc2Sync     = nullptr;
            std::atomic<float>* osc3Sync     = nullptr;
            std::atomic<float>* fmAmount     = nullptr;
            std::atomic<float>* ringModLevel = nullptr;
            std::atomic<float>* noiseLevel   = nullptr;

            std::atomic<float>* filterType      = nullptr;
            std::atomic<float>* filterSlope     = nullptr;
            std::atomic<float>* filterCutoff    = nullptr;
            std::atomic<float>* filterResonance = nullptr;
            std::atomic<float>* filterKeyTrack  = nullptr;
            std::atomic<float>* filterDrive     = nullptr;
            std::atomic<float>* filterEnvAmount = nullptr;

            struct LfoParams
            {
                std::atomic<float>* shape     = nullptr;
                std::atomic<float>* keySync   = nullptr;
                std::atomic<float>* mono      = nullptr;
                std::atomic<float>* fadeIn    = nullptr;
                std::atomic<float>* inc       = nullptr;
                std::atomic<float>* monoPhase = nullptr;
            } lfo[numLfos];

            std::atomic<float>* modWheel   = nullptr;
            std::atomic<float>* aftertouch = nullptr;

            std::atomic<float>* glideTime = nullptr;   // seconds, 0 = off

            struct Slot
            {
                std::atomic<float>* source = nullptr;
                std::atomic<float>* dest   = nullptr;
                std::atomic<float>* depth  = nullptr;
            } matrix[mod::numSlots];

            struct MsegParams
            {
                const mseg::Data*   data = nullptr;   // processor-owned audio-side snapshot
                std::atomic<float>* inc  = nullptr;   // phase increment per sample (block rate)
                std::atomic<float>* loop = nullptr;   // loop enable
            } msegs[mseg::maxMsegs];

            const Wavetable* wavetable = nullptr;
        };

        explicit PikeVoice (const Parameters& params) : parameters (params)
        {
            for (auto& o : oscillators)
                o.setWavetable (parameters.wavetable);
        }

        //======================================================================
        bool canPlaySound (juce::SynthesiserSound* sound) override
        {
            return dynamic_cast<PikeSound*> (sound) != nullptr;
        }

        void setCurrentPlaybackSampleRate (double newRate) override
        {
            juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

            if (newRate > 0.0)
            {
                for (auto& o : oscillators) o.setSampleRate (newRate);
                for (auto& l : lfos)        l.setSampleRate (newRate);

                ampEnvelope.setSampleRate (newRate);
                filterEnvelope.setSampleRate (newRate);
                auxEnvelope.setSampleRate (newRate);
                filter.setSampleRate (newRate);
            }
        }

        //======================================================================
        /** Per-voice unison settings: detune (cents), output gain and stereo pan
            (0 = left, 0.5 = centre, 1 = right). Centre keeps full level in both
            channels so a single voice stays mono-compatible. */
        void setUnison (float detuneCents, float gain, float pan) noexcept
        {
            unisonMult = (float) std::exp2 (detuneCents / 1200.0);
            unisonGain = gain;
            panL = juce::jmin (1.0f, 2.0f * (1.0f - pan));
            panR = juce::jmin (1.0f, 2.0f * pan);
        }

        int  getCurrentNote() const noexcept { return currentNote; }
        bool isSounding()     const noexcept { return ampEnvelope.isActive(); }

        /** Legato pitch change: glide to the new note without retriggering. */
        void retune (int midiNoteNumber) noexcept
        {
            currentNote = midiNoteNumber;
            midiNote    = midiNoteNumber;
            targetHz    = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        }

        void startNote (int midiNoteNumber, float velocity,
                        juce::SynthesiserSound*, int) override
        {
            level       = velocity;
            midiNote    = midiNoteNumber;
            currentNote = midiNoteNumber;
            targetHz    = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

            // Glide starts from the previous pitch if we have one and glide is on.
            if (! hasPlayed || glideCoef >= 1.0f)
                currentHz = targetHz;
            hasPlayed = true;

            lastOsc3 = 0.0f;
            samplesSinceNoteOn = 0;
            prevRateMod[0] = prevRateMod[1] = 0.0;

            voiceSeed = (uint32_t) (midiNoteNumber * 2654435761u) ^ 0x9e3779b9u;
            noise.seed (voiceSeed);

            for (auto& o : oscillators)
                o.resetPhase();

            filter.reset();

            for (int l = 0; l < numLfos; ++l)
            {
                const bool keySync = parameters.lfo[l].keySync != nullptr
                                       && parameters.lfo[l].keySync->load() > 0.5f;
                lfos[l].noteOn (keySync);
            }

            for (auto& m : msegPlayers)
                m.noteOn();

            updateBlockParameters();
            ampEnvelope.noteOn();
            filterEnvelope.noteOn();
            auxEnvelope.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                ampEnvelope.noteOff();
                filterEnvelope.noteOff();
                auxEnvelope.noteOff();
                for (auto& m : msegPlayers)
                    m.noteOff();
            }
            else
            {
                ampEnvelope.reset();
                filterEnvelope.reset();
                auxEnvelope.reset();
                clearCurrentNote();
                currentNote = -1;
            }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        //======================================================================
        void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                              int startSample, int numSamples) override
        {
            if (! ampEnvelope.isActive())
                return;

            updateBlockParameters();

            const int numChannels = outputBuffer.getNumChannels();

            for (int i = 0; i < numSamples; ++i)
            {
                // Glide the base pitch toward the target.
                currentHz += (targetHz - currentHz) * glideCoef;

                // ----- envelopes -----
                const float ampVal  = ampEnvelope.getNextSample();
                const float filtVal = filterEnvelope.getNextSample();
                const float auxVal  = auxEnvelope.getNextSample();

                // ----- LFOs (rate modulated by previous sample's value) -----
                const int absPos = startSample + i;   // position within the block
                float lfoVal[numLfos];
                for (int l = 0; l < numLfos; ++l)
                {
                    const float fade = lfoFadeSamples[l] > 0.0f
                                         ? juce::jlimit (0.0f, 1.0f, (float) samplesSinceNoteOn / lfoFadeSamples[l])
                                         : 1.0f;

                    if (lfoMono[l])
                    {
                        const double tp = lfoMonoPhase[l] + (double) absPos * lfoInc[l];
                        lfoVal[l] = lfos[l].renderMono (tp, 0x51ed3000u + (uint32_t) l) * fade;
                    }
                    else
                    {
                        const double inc = lfoInc[l] * std::exp2 (prevRateMod[l]);
                        lfoVal[l] = lfos[l].renderPoly (inc, voiceSeed + (uint32_t) l) * fade;
                    }
                }

                // ----- modulation sources -----
                float src[mod::numSources];
                src[(int) mod::Source::None]       = 0.0f;
                src[(int) mod::Source::Env1]       = ampVal;
                src[(int) mod::Source::Env2]       = filtVal;
                src[(int) mod::Source::Env3]       = auxVal;
                src[(int) mod::Source::Lfo1]       = lfoVal[0];
                src[(int) mod::Source::Lfo2]       = lfoVal[1];
                src[(int) mod::Source::Velocity]   = level;
                src[(int) mod::Source::ModWheel]   = modWheelVal;
                src[(int) mod::Source::Aftertouch] = aftertouchVal;
                src[(int) mod::Source::KeyTrack]   = keyTrackSource;

                for (int m = 0; m < mseg::maxMsegs; ++m)
                    src[(int) mod::Source::Mseg1 + m] =
                        msegActive[m] ? msegPlayers[m].process (*parameters.msegs[m].data,
                                                                msegInc[m], msegLoopOn[m])
                                      : 0.0f;

                // ----- evaluate matrix -----
                float dst[mod::numDests] = {};
                for (int s = 0; s < mod::numSlots; ++s)
                {
                    const int   si = slotSource[s];
                    const int   di = slotDest[s];
                    const float dp = slotDepth[s];
                    if (si > 0 && di > 0 && dp != 0.0f)
                        dst[di] += src[si] * dp;
                }

                prevRateMod[0] = (double) (dst[(int) mod::Dest::Lfo1Rate] * mod::destScale[(int) mod::Dest::Lfo1Rate]);
                prevRateMod[1] = (double) (dst[(int) mod::Dest::Lfo2Rate] * mod::destScale[(int) mod::Dest::Lfo2Rate]);

                // ----- per-oscillator modulation -----
                const float pwMod = dst[(int) mod::Dest::PulseWidth]   * mod::destScale[(int) mod::Dest::PulseWidth];
                const float wtMod = dst[(int) mod::Dest::WavetablePos] * mod::destScale[(int) mod::Dest::WavetablePos];

                double o1Freq = currentHz;
                for (int n = 0; n < numOscillators; ++n)
                {
                    const int   pitchDest = (int) mod::Dest::Osc1Pitch + n;
                    const float semis     = dst[pitchDest] * mod::destScale[pitchDest];
                    const double f = currentHz * oscMult[n] * unisonMult * std::exp2 ((double) semis / 12.0);

                    oscillators[n].setFrequency ((float) f);
                    oscillators[n].setPulseWidth       (juce::jlimit (0.02f, 0.98f, basePulseWidth[n] + pwMod));
                    oscillators[n].setWavetablePosition (juce::jlimit (0.0f, 1.0f, baseWavetablePos[n] + wtMod));

                    if (n == 0)
                        o1Freq = f;
                }

                // ----- oscillators with FM + sync -----
                const float fm       = fmBase + dst[(int) mod::Dest::FmAmount] * mod::destScale[(int) mod::Dest::FmAmount];
                const float fmOffset = fm * lastOsc3 * (float) o1Freq * fmDepth;

                const float o1 = oscillators[0].processSample (fmOffset);
                if (oscillators[0].didWrap())
                {
                    if (sync2) oscillators[1].hardSync();
                    if (sync3) oscillators[2].hardSync();
                }
                const float o2 = oscillators[1].processSample();
                const float o3 = oscillators[2].processSample();
                lastOsc3 = o3;

                // ----- mixer -----
                const float l0 = juce::jmax (0.0f, oscLevel[0] + dst[(int) mod::Dest::Osc1Level]);
                const float l1 = juce::jmax (0.0f, oscLevel[1] + dst[(int) mod::Dest::Osc2Level]);
                const float l2 = juce::jmax (0.0f, oscLevel[2] + dst[(int) mod::Dest::Osc3Level]);

                float mix = o1 * l0 + o2 * l1 + o3 * l2
                          + ringModLevel * (o1 * o2)
                          + noiseLevel * noise.processSample();

                // ----- drive -----
                if (drive > 0.0001f)
                    mix += drive * (std::tanh (mix * drivePreGain) - mix);

                // ----- filter -----
                if (resonanceModActive)
                {
                    const float resMod = dst[(int) mod::Dest::Resonance] * mod::destScale[(int) mod::Dest::Resonance];
                    filter.setResonance (juce::jlimit (0.0f, 1.0f, baseResonance + resMod));
                }

                const double cutoffOct = keyTrackOctaves
                                       + (double) (filterEnvAmount * filtVal) * filterEnvRangeOct
                                       + (double) (dst[(int) mod::Dest::Cutoff] * mod::destScale[(int) mod::Dest::Cutoff]);
                filter.setCutoff ((float) (baseCutoff * std::exp2 (cutoffOct)));

                const float filtered = filter.process (mix);

                // ----- amp VCA + unison gain + stereo pan -----
                const float sample = filtered * ampVal * level * unisonGain;
                if (numChannels >= 2)
                {
                    outputBuffer.addSample (0, startSample + i, sample * panL);
                    outputBuffer.addSample (1, startSample + i, sample * panR);
                    for (int ch = 2; ch < numChannels; ++ch)
                        outputBuffer.addSample (ch, startSample + i, sample);
                }
                else
                {
                    outputBuffer.addSample (0, startSample + i, sample);
                }

                ++samplesSinceNoteOn;

                if (! ampEnvelope.isActive())
                {
                    clearCurrentNote();
                    currentNote = -1;
                    break;
                }
            }
        }

    private:
        void updateBlockParameters() noexcept
        {
            ampEnvelope.setParameters (readADSR (parameters.ampAttack, parameters.ampDecay,
                                                 parameters.ampSustain, parameters.ampRelease, 0.005f, 0.15f, 0.8f, 0.2f));
            filterEnvelope.setParameters (readADSR (parameters.filtAttack, parameters.filtDecay,
                                                    parameters.filtSustain, parameters.filtRelease, 0.005f, 0.2f, 0.6f, 0.3f));
            auxEnvelope.setParameters (readADSR (parameters.auxAttack, parameters.auxDecay,
                                                 parameters.auxSustain, parameters.auxRelease, 0.005f, 0.2f, 0.5f, 0.3f));

            for (int n = 0; n < numOscillators; ++n)
            {
                const auto& P = parameters.osc[n];

                oscillators[n].setWaveform (static_cast<Waveform> (P.wave != nullptr ? (int) P.wave->load() : 0));

                const double octave = P.octave != nullptr ? P.octave->load() : 0.0;
                const double semi   = P.semi   != nullptr ? P.semi->load()   : 0.0;
                const double fine   = P.fine   != nullptr ? P.fine->load()   : 0.0;
                oscMult[n]          = std::pow (2.0, octave + semi / 12.0 + fine / 1200.0);
                basePulseWidth[n]   = P.pulseWidth != nullptr ? P.pulseWidth->load() : 0.5f;
                baseWavetablePos[n] = P.wtPos      != nullptr ? P.wtPos->load()      : 0.0f;
                oscLevel[n]         = P.level      != nullptr ? P.level->load()      : 0.0f;
            }

            sync2        = loadBool (parameters.osc2Sync);
            sync3        = loadBool (parameters.osc3Sync);
            fmBase       = loadFloat (parameters.fmAmount);
            ringModLevel = loadFloat (parameters.ringModLevel);
            noiseLevel   = loadFloat (parameters.noiseLevel);

            filter.setType (static_cast<FilterType> (parameters.filterType != nullptr ? (int) parameters.filterType->load() : 0));
            filter.setSlope24 (loadBool (parameters.filterSlope));
            baseResonance = loadFloat (parameters.filterResonance);
            filter.setResonance (baseResonance);

            baseCutoff      = parameters.filterCutoff != nullptr ? parameters.filterCutoff->load() : 20000.0f;
            drive           = loadFloat (parameters.filterDrive);
            drivePreGain    = 1.0f + drive * 15.0f;
            filterEnvAmount = loadFloat (parameters.filterEnvAmount);

            const float keyTrack = loadFloat (parameters.filterKeyTrack);
            keyTrackOctaves = keyTrack * (double) (midiNote - 60) / 12.0;

            for (int l = 0; l < numLfos; ++l)
            {
                lfos[l].setShape (static_cast<LfoShape> (parameters.lfo[l].shape != nullptr ? (int) parameters.lfo[l].shape->load() : 0));
                lfoMono[l]        = loadBool (parameters.lfo[l].mono);
                lfoFadeSamples[l] = loadFloat (parameters.lfo[l].fadeIn) * (float) getSampleRate();
                lfoInc[l]         = parameters.lfo[l].inc       != nullptr ? parameters.lfo[l].inc->load()       : 0.0;
                lfoMonoPhase[l]   = parameters.lfo[l].monoPhase != nullptr ? parameters.lfo[l].monoPhase->load() : 0.0;
            }

            for (int m = 0; m < mseg::maxMsegs; ++m)
            {
                const auto& mp = parameters.msegs[m];
                msegInc[m]    = mp.inc != nullptr ? mp.inc->load() : 0.0;
                msegLoopOn[m] = loadBool (mp.loop);
                msegActive[m] = mp.data != nullptr && mp.data->numPoints >= 2;
            }

            modWheelVal    = loadFloat (parameters.modWheel);
            aftertouchVal  = loadFloat (parameters.aftertouch);
            keyTrackSource = juce::jlimit (-1.0f, 1.0f, (float) (midiNote - 60) / 60.0f);

            // Glide coefficient (one-pole) from glide time.
            const float glideTime = loadFloat (parameters.glideTime);
            glideCoef = glideTime > 0.0001f
                          ? 1.0f - std::exp (-1.0f / (glideTime * (float) getSampleRate()))
                          : 1.0f;

            resonanceModActive = false;
            for (int s = 0; s < mod::numSlots; ++s)
            {
                slotSource[s] = parameters.matrix[s].source != nullptr ? (int) parameters.matrix[s].source->load() : 0;
                slotDest[s]   = parameters.matrix[s].dest   != nullptr ? (int) parameters.matrix[s].dest->load()   : 0;
                slotDepth[s]  = parameters.matrix[s].depth  != nullptr ? parameters.matrix[s].depth->load()        : 0.0f;

                if (slotSource[s] > 0 && slotDepth[s] != 0.0f && slotDest[s] == (int) mod::Dest::Resonance)
                    resonanceModActive = true;
            }
        }

        static float loadFloat (std::atomic<float>* p) noexcept { return p != nullptr ? p->load() : 0.0f; }
        static bool  loadBool  (std::atomic<float>* p) noexcept { return p != nullptr && p->load() > 0.5f; }

        static juce::ADSR::Parameters readADSR (std::atomic<float>* a, std::atomic<float>* d,
                                                std::atomic<float>* s, std::atomic<float>* r,
                                                float da, float dd, float ds, float dr) noexcept
        {
            juce::ADSR::Parameters p;
            p.attack  = a != nullptr ? a->load() : da;
            p.decay   = d != nullptr ? d->load() : dd;
            p.sustain = s != nullptr ? s->load() : ds;
            p.release = r != nullptr ? r->load() : dr;
            return p;
        }

        static constexpr float  fmDepth           = 4.0f;
        static constexpr double filterEnvRangeOct = 6.0;

        Parameters parameters;
        Oscillator oscillators[numOscillators];
        Lfo        lfos[numLfos];
        Noise      noise;
        juce::ADSR ampEnvelope, filterEnvelope, auxEnvelope;
        StateVariableFilter filter;

        int      midiNote   = 60;
        int      currentNote = -1;
        double   targetHz   = 440.0;
        double   currentHz  = 440.0;
        float    glideCoef  = 1.0f;
        bool     hasPlayed  = false;
        float    level      = 0.0f;
        float    lastOsc3   = 0.0f;
        uint32_t voiceSeed  = 0x9e3779b9u;
        int      samplesSinceNoteOn = 0;
        double   prevRateMod[numLfos] { 0.0, 0.0 };

        // Unison (set by the VoiceManager per allocated voice).
        float unisonMult = 1.0f;
        float unisonGain = 1.0f;
        float panL = 1.0f, panR = 1.0f;

        // Cached per-block oscillator / mixer state.
        double oscMult[numOscillators]          { 1.0, 1.0, 1.0 };
        float  basePulseWidth[numOscillators]   { 0.5f, 0.5f, 0.5f };
        float  baseWavetablePos[numOscillators] { 0.0f, 0.0f, 0.0f };
        float  oscLevel[numOscillators]         { 0.0f, 0.0f, 0.0f };
        bool   sync2 = false, sync3 = false;
        float  fmBase = 0.0f, ringModLevel = 0.0f, noiseLevel = 0.0f;

        // Cached per-block filter state.
        double baseCutoff      = 20000.0;
        double keyTrackOctaves = 0.0;
        float  baseResonance   = 0.0f;
        float  drive           = 0.0f;
        float  drivePreGain    = 1.0f;
        float  filterEnvAmount = 0.0f;
        bool   resonanceModActive = false;

        // Cached per-block LFO state.
        bool   lfoMono[numLfos]        { false, false };
        float  lfoFadeSamples[numLfos] { 0.0f, 0.0f };
        double lfoInc[numLfos]         { 0.0, 0.0 };
        double lfoMonoPhase[numLfos]   { 0.0, 0.0 };

        // Per-voice MSEG players + cached per-block state.
        mseg::Player msegPlayers[mseg::maxMsegs];
        double msegInc[mseg::maxMsegs]    {};
        bool   msegLoopOn[mseg::maxMsegs] {};
        bool   msegActive[mseg::maxMsegs] {};

        // Cached per-block MIDI/matrix state.
        float modWheelVal = 0.0f, aftertouchVal = 0.0f, keyTrackSource = 0.0f;
        int   slotSource[mod::numSlots] {};
        int   slotDest[mod::numSlots]   {};
        float slotDepth[mod::numSlots]  {};
    };
}
