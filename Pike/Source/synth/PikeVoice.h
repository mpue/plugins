/*
  ==============================================================================

    PikeVoice.h
    One polyphonic voice. Phase 3 scope: three oscillators (PolyBLEP analogue
    waveforms + mip-mapped wavetable) -> mixer (with ring mod + noise) -> amp
    ADSR. Osc1 is the sync master and the FM carrier (Osc3 -> Osc1).

    The voice reads its parameters straight from the APVTS atomics (realtime-safe
    loads); block-rate updates set per-oscillator tuning/shape, the sample loop
    runs the DSP.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PikeSound.h"
#include "../dsp/Oscillator.h"
#include "../dsp/Wavetable.h"
#include "../dsp/Noise.h"
#include "../dsp/Filter.h"

namespace pike
{
    class PikeVoice : public juce::SynthesiserVoice
    {
    public:
        static constexpr int numOscillators = 3;

        /** Atomic parameter pointers the voice reads each block. Owned by APVTS. */
        struct Parameters
        {
            // Amp envelope
            std::atomic<float>* ampAttack  = nullptr;
            std::atomic<float>* ampDecay   = nullptr;
            std::atomic<float>* ampSustain = nullptr;
            std::atomic<float>* ampRelease = nullptr;

            // Per oscillator
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

            // Routing / mixer
            std::atomic<float>* osc2Sync     = nullptr;
            std::atomic<float>* osc3Sync     = nullptr;
            std::atomic<float>* fmAmount     = nullptr;
            std::atomic<float>* ringModLevel = nullptr;
            std::atomic<float>* noiseLevel   = nullptr;

            // Filter
            std::atomic<float>* filterType      = nullptr;
            std::atomic<float>* filterSlope     = nullptr;
            std::atomic<float>* filterCutoff    = nullptr;
            std::atomic<float>* filterResonance = nullptr;
            std::atomic<float>* filterKeyTrack  = nullptr;
            std::atomic<float>* filterDrive     = nullptr;
            std::atomic<float>* filterEnvAmount = nullptr;

            // Filter envelope
            std::atomic<float>* filtAttack  = nullptr;
            std::atomic<float>* filtDecay   = nullptr;
            std::atomic<float>* filtSustain = nullptr;
            std::atomic<float>* filtRelease = nullptr;

            // Shared, read-only wavetable bank (owned by the processor).
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
                for (auto& o : oscillators)
                    o.setSampleRate (newRate);

                ampEnvelope.setSampleRate (newRate);
                filterEnvelope.setSampleRate (newRate);
                filter.setSampleRate (newRate);
            }
        }

        //======================================================================
        void startNote (int midiNoteNumber, float velocity,
                        juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
        {
            level    = velocity;
            midiNote = midiNoteNumber;
            noteHz   = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
            lastOsc3 = 0.0f;

            noise.seed ((uint32_t) (midiNoteNumber * 2654435761u) ^ 0x9e3779b9u);

            for (auto& o : oscillators)
                o.resetPhase();

            filter.reset();

            updateEnvelopeParameters();
            updateOscillatorParameters();
            updateFilterParameters();
            ampEnvelope.noteOn();
            filterEnvelope.noteOn();
        }

        void stopNote (float /*velocity*/, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                ampEnvelope.noteOff();
                filterEnvelope.noteOff();
            }
            else
            {
                ampEnvelope.reset();
                filterEnvelope.reset();
                clearCurrentNote();
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

            updateEnvelopeParameters();
            updateOscillatorParameters();
            updateFilterParameters();

            const int numChannels = outputBuffer.getNumChannels();

            for (int i = 0; i < numSamples; ++i)
            {
                // Osc3 -> Osc1 FM (one-sample-delayed carrier feedback).
                const float fmOffset = fmAmount * lastOsc3 * (float) osc1Hz * fmDepth;

                const float o1 = oscillators[0].processSample (fmOffset);

                // Hard sync: when the master (osc1) wraps, restart enabled slaves.
                if (oscillators[0].didWrap())
                {
                    if (sync2) oscillators[1].hardSync();
                    if (sync3) oscillators[2].hardSync();
                }

                const float o2 = oscillators[1].processSample();
                const float o3 = oscillators[2].processSample();
                lastOsc3 = o3;

                float mix = o1 * oscLevel[0]
                          + o2 * oscLevel[1]
                          + o3 * oscLevel[2]
                          + ringModLevel * (o1 * o2)
                          + noiseLevel * noise.processSample();

                // Pre-filter drive (clean at 0, soft overdrive as it opens up).
                if (drive > 0.0001f)
                    mix += drive * (std::tanh (mix * drivePreGain) - mix);

                // Filter envelope modulates cutoff (in octaves), plus key-track.
                const float fenv   = filterEnvelope.getNextSample();
                const double octs  = keyTrackOctaves + (double) (filterEnvAmount * fenv) * filterEnvRangeOct;
                const float cutoff = (float) (baseCutoff * std::exp2 (octs));
                filter.setCutoff (cutoff);

                const float filtered = filter.process (mix);

                const float env    = ampEnvelope.getNextSample();
                const float sample = filtered * env * level;

                for (int ch = 0; ch < numChannels; ++ch)
                    outputBuffer.addSample (ch, startSample + i, sample);

                if (! ampEnvelope.isActive())
                {
                    clearCurrentNote();
                    break;
                }
            }
        }

    private:
        void updateEnvelopeParameters() noexcept
        {
            juce::ADSR::Parameters p;
            p.attack  = parameters.ampAttack  != nullptr ? parameters.ampAttack ->load() : 0.005f;
            p.decay   = parameters.ampDecay   != nullptr ? parameters.ampDecay  ->load() : 0.15f;
            p.sustain = parameters.ampSustain != nullptr ? parameters.ampSustain->load() : 0.8f;
            p.release = parameters.ampRelease != nullptr ? parameters.ampRelease->load() : 0.2f;
            ampEnvelope.setParameters (p);
        }

        void updateOscillatorParameters() noexcept
        {
            for (int n = 0; n < numOscillators; ++n)
            {
                const auto& P = parameters.osc[n];

                const int waveIndex = P.wave != nullptr ? (int) P.wave->load() : 0;
                oscillators[n].setWaveform (static_cast<Waveform> (waveIndex));

                const double octave = P.octave != nullptr ? P.octave->load() : 0.0;
                const double semi   = P.semi   != nullptr ? P.semi->load()   : 0.0;
                const double fine   = P.fine   != nullptr ? P.fine->load()   : 0.0;
                const double mult   = std::pow (2.0, octave + semi / 12.0 + fine / 1200.0);
                const double freq   = noteHz * mult;

                oscillators[n].setFrequency ((float) freq);
                oscillators[n].setPulseWidth       (P.pulseWidth != nullptr ? P.pulseWidth->load() : 0.5f);
                oscillators[n].setWavetablePosition (P.wtPos     != nullptr ? P.wtPos->load()      : 0.0f);

                oscLevel[n] = P.level != nullptr ? P.level->load() : 0.0f;

                if (n == 0)
                    osc1Hz = freq;
            }

            sync2        = parameters.osc2Sync     != nullptr && parameters.osc2Sync->load() > 0.5f;
            sync3        = parameters.osc3Sync     != nullptr && parameters.osc3Sync->load() > 0.5f;
            fmAmount     = parameters.fmAmount     != nullptr ? parameters.fmAmount->load()     : 0.0f;
            ringModLevel = parameters.ringModLevel != nullptr ? parameters.ringModLevel->load() : 0.0f;
            noiseLevel   = parameters.noiseLevel   != nullptr ? parameters.noiseLevel->load()   : 0.0f;
        }

        void updateFilterParameters() noexcept
        {
            const int typeIndex = parameters.filterType != nullptr ? (int) parameters.filterType->load() : 0;
            filter.setType (static_cast<FilterType> (typeIndex));
            filter.setSlope24 (parameters.filterSlope != nullptr && parameters.filterSlope->load() > 0.5f);
            filter.setResonance (parameters.filterResonance != nullptr ? parameters.filterResonance->load() : 0.0f);

            baseCutoff      = parameters.filterCutoff    != nullptr ? parameters.filterCutoff->load()    : 20000.0f;
            drive           = parameters.filterDrive     != nullptr ? parameters.filterDrive->load()     : 0.0f;
            filterEnvAmount = parameters.filterEnvAmount != nullptr ? parameters.filterEnvAmount->load() : 0.0f;
            drivePreGain    = 1.0f + drive * 15.0f;

            const float keyTrack = parameters.filterKeyTrack != nullptr ? parameters.filterKeyTrack->load() : 0.0f;
            keyTrackOctaves = keyTrack * (double) (midiNote - 60) / 12.0;

            juce::ADSR::Parameters p;
            p.attack  = parameters.filtAttack  != nullptr ? parameters.filtAttack ->load() : 0.005f;
            p.decay   = parameters.filtDecay   != nullptr ? parameters.filtDecay  ->load() : 0.2f;
            p.sustain = parameters.filtSustain != nullptr ? parameters.filtSustain->load() : 0.6f;
            p.release = parameters.filtRelease != nullptr ? parameters.filtRelease->load() : 0.3f;
            filterEnvelope.setParameters (p);
        }

        static constexpr float  fmDepth            = 4.0f;   // maps fmAmount 0..1 to a musical index
        static constexpr double filterEnvRangeOct  = 6.0;    // filter env full-scale range (octaves)

        Parameters parameters;
        Oscillator oscillators[numOscillators];
        Noise      noise;
        juce::ADSR ampEnvelope;
        juce::ADSR filterEnvelope;
        StateVariableFilter filter;

        int    midiNote = 60;
        double noteHz   = 440.0;
        double osc1Hz   = 440.0;
        float  level    = 0.0f;   // velocity-scaled amplitude
        float  lastOsc3 = 0.0f;   // FM carrier feedback

        // Cached per-block routing/mixer state.
        float oscLevel[numOscillators] { 0.0f, 0.0f, 0.0f };
        bool  sync2 = false, sync3 = false;
        float fmAmount = 0.0f, ringModLevel = 0.0f, noiseLevel = 0.0f;

        // Cached per-block filter state.
        double baseCutoff      = 20000.0;
        double keyTrackOctaves = 0.0;
        float  drive           = 0.0f;
        float  drivePreGain    = 1.0f;
        float  filterEnvAmount = 0.0f;
    };
}
