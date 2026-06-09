/*
  ==============================================================================

    FxChain.h
    Global post-mix effects chain: Distortion -> Chorus -> Delay -> Reverb.
    Uses juce::dsp::Chorus and juce::Reverb; Distortion/Delay are own DSP.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Distortion.h"
#include "Delay.h"

namespace pike
{
    class FxChain
    {
    public:
        struct Params
        {
            bool  distOn = false;  int distType = 0;  float distDrive = 0.0f, distMix = 1.0f;

            bool  chorusOn = false; float chorusRate = 1.0f, chorusDepth = 0.25f,
                                          chorusFeedback = 0.0f, chorusMix = 0.5f;

            bool  delayOn = false; bool delaySync = true; float delayTimeMs = 300.0f;
            int   delayDiv = 2;    float delayFeedback = 0.4f, delayMix = 0.3f; bool delayPingpong = false;

            bool  reverbOn = false; float reverbSize = 0.5f, reverbDamping = 0.5f,
                                          reverbWidth = 1.0f, reverbMix = 0.3f;
        };

        void prepare (double sampleRate, int maxBlock, int numChannels)
        {
            sr = sampleRate;

            juce::dsp::ProcessSpec spec;
            spec.sampleRate       = sampleRate;
            spec.maximumBlockSize = (juce::uint32) juce::jmax (1, maxBlock);
            spec.numChannels      = (juce::uint32) juce::jmax (1, numChannels);

            chorus.prepare (spec);
            chorus.reset();

            delay.prepare (sampleRate, 2.0);
            reverb.setSampleRate (sampleRate);
            reverb.reset();
        }

        void reset()
        {
            chorus.reset();
            delay.reset();
            reverb.reset();
        }

        void process (juce::AudioBuffer<float>& buffer, const Params& p, double bpm)
        {
            const int n  = buffer.getNumSamples();
            const int ch = buffer.getNumChannels();

            // --- Distortion ---
            if (p.distOn)
            {
                distortion.setParams (static_cast<DistortionType> (p.distType), p.distDrive, p.distMix);
                for (int c = 0; c < ch; ++c)
                {
                    auto* d = buffer.getWritePointer (c);
                    for (int i = 0; i < n; ++i)
                        d[i] = distortion.processSample (d[i]);
                }
            }

            // --- Chorus ---
            if (p.chorusOn)
            {
                chorus.setRate (p.chorusRate);
                chorus.setDepth (juce::jlimit (0.0f, 1.0f, p.chorusDepth));
                chorus.setCentreDelay (7.0f);
                chorus.setFeedback (juce::jlimit (-0.95f, 0.95f, p.chorusFeedback));
                chorus.setMix (juce::jlimit (0.0f, 1.0f, p.chorusMix));

                juce::dsp::AudioBlock<float> block (buffer);
                juce::dsp::ProcessContextReplacing<float> ctx (block);
                chorus.process (ctx);
            }

            // --- Delay ---
            if (p.delayOn)
            {
                const float samples = p.delaySync ? syncSamples (bpm, p.delayDiv)
                                                  : (float) (p.delayTimeMs * 0.001 * sr);
                delay.setParams (samples, samples, p.delayFeedback, p.delayMix, p.delayPingpong);

                auto* L = buffer.getWritePointer (0);
                auto* R = ch > 1 ? buffer.getWritePointer (1) : L;
                delay.processBlock (L, R, n);
            }

            // --- Reverb ---
            if (p.reverbOn)
            {
                juce::Reverb::Parameters rp;
                rp.roomSize = juce::jlimit (0.0f, 1.0f, p.reverbSize);
                rp.damping  = juce::jlimit (0.0f, 1.0f, p.reverbDamping);
                rp.width    = juce::jlimit (0.0f, 1.0f, p.reverbWidth);
                rp.wetLevel = juce::jlimit (0.0f, 1.0f, p.reverbMix);
                rp.dryLevel = 1.0f - juce::jlimit (0.0f, 1.0f, p.reverbMix);
                reverb.setParameters (rp);

                auto* L = buffer.getWritePointer (0);
                if (ch > 1) reverb.processStereo (L, buffer.getWritePointer (1), n);
                else        reverb.processMono (L, n);
            }
        }

    private:
        // Delay sync division -> samples. Index maps "1/1".."1/32" (with 1/8T).
        float syncSamples (double bpm, int div) const noexcept
        {
            static constexpr double beatsPerRepeat[] =
                { 4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 0.125 };
            const int idx = juce::jlimit (0, (int) std::size (beatsPerRepeat) - 1, div);
            const double seconds = beatsPerRepeat[idx] * 60.0 / (bpm > 0.0 ? bpm : 120.0);
            return (float) (seconds * sr);
        }

        double sr = 44100.0;
        Distortion distortion;
        Delay      delay;
        juce::dsp::Chorus<float> chorus;
        juce::Reverb reverb;
    };
}
