/*
  ==============================================================================

    PikeVoice.h
    One polyphonic voice. Phase 2 scope: a single oscillator through an amp
    ADSR envelope. The voice reads its envelope parameters straight from the
    APVTS atomics (realtime-safe loads) so the processor never reaches in.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PikeSound.h"
#include "../dsp/Oscillator.h"

namespace pike
{
    class PikeVoice : public juce::SynthesiserVoice
    {
    public:
        /** Atomic parameter pointers the voice reads each block. Owned by APVTS. */
        struct Parameters
        {
            std::atomic<float>* ampAttack  = nullptr;
            std::atomic<float>* ampDecay   = nullptr;
            std::atomic<float>* ampSustain = nullptr;
            std::atomic<float>* ampRelease = nullptr;
        };

        explicit PikeVoice (const Parameters& params) : parameters (params) {}

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
                oscillator.setSampleRate (newRate);
                ampEnvelope.setSampleRate (newRate);
            }
        }

        //======================================================================
        void startNote (int midiNoteNumber, float velocity,
                        juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
        {
            level = velocity;
            oscillator.setFrequency ((float) juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber));
            oscillator.resetPhase();

            updateEnvelopeParameters();
            ampEnvelope.noteOn();
        }

        void stopNote (float /*velocity*/, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                ampEnvelope.noteOff();
            }
            else
            {
                ampEnvelope.reset();
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

            const int numChannels = outputBuffer.getNumChannels();

            for (int i = 0; i < numSamples; ++i)
            {
                const float env    = ampEnvelope.getNextSample();
                const float sample = oscillator.processSample() * env * level;

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

        Parameters  parameters;
        Oscillator  oscillator;
        juce::ADSR  ampEnvelope;
        float       level = 0.0f;   // velocity-scaled amplitude
    };
}
