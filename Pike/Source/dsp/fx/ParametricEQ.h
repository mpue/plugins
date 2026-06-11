/*
  ==============================================================================

    ParametricEQ.h
    8-band parametric EQ (low shelf / 6 peaks / high shelf), ported from the
    Lupo synth. Uses juce::dsp::IIR. Header-only, in namespace pike.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace pike
{
    class ParametricEQ
    {
    public:
        static constexpr int numBands = 8;

        enum class BandType { LowShelf, Peak, HighShelf };

        struct Band
        {
            float    frequency = 1000.0f;
            float    gainDb    = 0.0f;
            float    q         = 1.0f;
            BandType type      = BandType::Peak;
        };

        ParametricEQ()
        {
            bands[0] = { 60.0f,    0.0f, 0.7f, BandType::LowShelf  };
            bands[1] = { 200.0f,   0.0f, 1.0f, BandType::Peak      };
            bands[2] = { 500.0f,   0.0f, 1.0f, BandType::Peak      };
            bands[3] = { 1000.0f,  0.0f, 1.0f, BandType::Peak      };
            bands[4] = { 2000.0f,  0.0f, 1.0f, BandType::Peak      };
            bands[5] = { 4000.0f,  0.0f, 1.0f, BandType::Peak      };
            bands[6] = { 8000.0f,  0.0f, 1.0f, BandType::Peak      };
            bands[7] = { 14000.0f, 0.0f, 0.7f, BandType::HighShelf };
        }

        void prepare (double sr, int /*blockSize*/)
        {
            sampleRate = sr;
            prepared   = true;

            juce::dsp::ProcessSpec spec;
            spec.sampleRate       = sr;
            spec.maximumBlockSize = 4096;
            spec.numChannels      = 1;

            for (int i = 0; i < numBands; ++i)
            {
                updateCoefficients (i);
                filtersL[i].prepare (spec);
                filtersR[i].prepare (spec);
            }
        }

        void reset()
        {
            for (int i = 0; i < numBands; ++i) { filtersL[i].reset(); filtersR[i].reset(); }
        }

        void processStereo (float* left, float* right, int numSamples)
        {
            if (! enabled || ! prepared)
                return;

            for (int i = 0; i < numBands; ++i)
                for (int n = 0; n < numSamples; ++n)
                {
                    left[n]  = filtersL[i].processSample (left[n]);
                    right[n] = filtersR[i].processSample (right[n]);
                }
        }

        void setBandFrequency (int band, float hz)  { if (valid (band)) { bands[band].frequency = hz; updateCoefficients (band); } }
        void setBandGain      (int band, float dB)  { if (valid (band)) { bands[band].gainDb    = dB; updateCoefficients (band); } }
        void setBandQ         (int band, float q)   { if (valid (band)) { bands[band].q         = q;  updateCoefficients (band); } }

        void setEnabled (bool e)
        {
            if (! e && enabled)
                reset();
            enabled = e;
        }

        bool        isEnabled() const               { return enabled; }
        const Band& getBand (int i) const           { return bands[i]; }

    private:
        using CoeffsPtr = juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>;

        static bool valid (int b) { return b >= 0 && b < numBands; }

        void updateCoefficients (int i)
        {
            const auto& b   = bands[i];
            const float freq = juce::jlimit (20.0f, 20000.0f, b.frequency);
            const float q    = juce::jlimit (0.1f, 10.0f, b.q);
            const float lin  = juce::Decibels::decibelsToGain (b.gainDb);

            CoeffsPtr c;
            switch (b.type)
            {
                case BandType::LowShelf:  c = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, freq, q, lin); break;
                case BandType::HighShelf: c = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, freq, q, lin); break;
                default:                  c = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, freq, q, lin); break;
            }

            coefficients[i] = c;
            if (prepared)
            {
                *filtersL[i].coefficients = *c;
                *filtersR[i].coefficients = *c;
            }
        }

        std::array<Band, numBands>                          bands;
        std::array<juce::dsp::IIR::Filter<float>, numBands> filtersL, filtersR;
        std::array<CoeffsPtr, numBands>                     coefficients;

        double sampleRate = 44100.0;
        bool   enabled    = false;
        bool   prepared   = false;
    };
}
