/*
  ==============================================================================

    Wavetable.h
    A morphable, mip-mapped wavetable bank. Pure DSP (no JUCE), built once and
    shared read-only across all oscillators/voices.

    - Several harmonic "frames" (sine -> triangle -> saw -> square) that the
      oscillator morphs between continuously.
    - For each frame, a set of band-limited mip levels (one per octave) built by
      additive synthesis, so a played note never contains harmonics above
      Nyquist -> no wavetable aliasing.

  ==============================================================================
*/

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace pike
{
    class Wavetable
    {
    public:
        static constexpr int    tableSize = 2048;        // samples per frame/mip
        static constexpr int    numMips   = 11;          // octave bands from baseFreq
        static constexpr int    numFrames = 4;           // sine, tri, saw, square
        static constexpr double baseFreq  = 20.0;        // lowest mip fundamental

        /** Builds the bank for the given sample rate. Call from prepareToPlay. */
        void prepare (double newSampleRate)
        {
            if (newSampleRate == sampleRate && built)
                return;

            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            build();
            built = true;
        }

        int getNumFrames() const noexcept { return numFrames; }

        /** Reads the bank. frame01 morphs between frames, phase01 in [0,1). */
        float getSample (float frame01, double phase01, double freq) const noexcept
        {
            if (! built)
                return 0.0f;

            const int mip = mipForFreq (freq);

            const float fpos = std::clamp (frame01, 0.0f, 1.0f) * (numFrames - 1);
            const int   f0   = (int) fpos;
            const int   f1   = std::min (f0 + 1, numFrames - 1);
            const float ff   = fpos - (float) f0;

            const auto& t0 = tables[(size_t) (f0 * numMips + mip)];
            const auto& t1 = tables[(size_t) (f1 * numMips + mip)];

            const double idx  = phase01 * tableSize;
            const int    i0   = (int) idx & (tableSize - 1);
            const int    i1   = (i0 + 1) & (tableSize - 1);
            const float  frac = (float) (idx - (double) (int) idx);

            const float s0 = t0[(size_t) i0] + frac * (t0[(size_t) i1] - t0[(size_t) i0]);
            const float s1 = t1[(size_t) i0] + frac * (t1[(size_t) i1] - t1[(size_t) i0]);

            return s0 + ff * (s1 - s0);
        }

    private:
        int mipForFreq (double freq) const noexcept
        {
            if (freq <= baseFreq)
                return 0;

            const int m = (int) std::floor (std::log2 (freq / baseFreq));
            return std::clamp (m, 0, numMips - 1);
        }

        // Harmonic amplitude for a given frame's target spectrum.
        static double harmonicAmp (int h, int frame) noexcept
        {
            switch (frame)
            {
                case 0:  return h == 1 ? 1.0 : 0.0;                                   // sine
                case 1:  return (h % 2 == 1) ? (((h - 1) / 2) % 2 == 0 ?  1.0 : -1.0) // triangle
                                               / (double) (h * h) : 0.0;
                case 2:  return 1.0 / (double) h;                                     // saw
                default: return (h % 2 == 1) ? 1.0 / (double) h : 0.0;                // square
            }
        }

        void build()
        {
            constexpr double twoPi   = 6.283185307179586476925286766559;
            const double     nyquist = sampleRate * 0.5;

            tables.assign ((size_t) (numFrames * numMips), {});

            for (int f = 0; f < numFrames; ++f)
            {
                for (int m = 0; m < numMips; ++m)
                {
                    const double topFreq = baseFreq * std::pow (2.0, m + 1);
                    const int    maxH    = std::max (1, (int) std::floor (nyquist / topFreq));

                    auto& tbl = tables[(size_t) (f * numMips + m)];
                    tbl.assign (tableSize, 0.0f);

                    for (int h = 1; h <= maxH; ++h)
                    {
                        const double amp = harmonicAmp (h, f);
                        if (amp == 0.0)
                            continue;

                        for (int i = 0; i < tableSize; ++i)
                            tbl[(size_t) i] += (float) (amp * std::sin (twoPi * h * i / tableSize));
                    }

                    normalisePeak (tbl);
                }
            }
        }

        static void normalisePeak (std::vector<float>& tbl) noexcept
        {
            float peak = 0.0f;
            for (auto v : tbl)
                peak = std::max (peak, std::abs (v));

            if (peak > 1.0e-6f)
            {
                const float g = 1.0f / peak;
                for (auto& v : tbl)
                    v *= g;
            }
        }

        std::vector<std::vector<float>> tables;   // [frame*numMips + mip][tableSize]
        double sampleRate = 0.0;
        bool   built      = false;
    };
}
