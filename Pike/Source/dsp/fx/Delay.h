/*
  ==============================================================================

    Delay.h
    Stereo delay with fractional (interpolated) taps, feedback, optional
    ping-pong and a gentle high-cut in the feedback path. Pure DSP, no JUCE.

  ==============================================================================
*/

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace pike
{
    class Delay
    {
    public:
        void prepare (double newSampleRate, double maxSeconds = 2.0)
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            size = (int) (maxSeconds * sampleRate) + 4;
            for (auto& b : line)
                b.assign ((size_t) size, 0.0f);

            // ~6 kHz one-pole high-cut for the feedback path.
            const double fc = 6000.0;
            damp = (float) (1.0 - std::exp (-2.0 * 3.14159265358979323846 * fc / sampleRate));

            reset();
        }

        void reset() noexcept
        {
            for (auto& b : line)
                std::fill (b.begin(), b.end(), 0.0f);
            writePos = 0;
            lp[0] = lp[1] = 0.0f;
        }

        void setParams (float delaySamplesL, float delaySamplesR,
                        float feedback, float mix, bool pingpong) noexcept
        {
            const float maxD = (float) (size - 4);
            dL   = std::clamp (delaySamplesL, 1.0f, maxD);
            dR   = std::clamp (delaySamplesR, 1.0f, maxD);
            fb   = std::clamp (feedback, 0.0f, 0.98f);
            wet  = std::clamp (mix, 0.0f, 1.0f);
            ping = pingpong;
        }

        void processBlock (float* L, float* R, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                const float inL = L[i];
                const float inR = R[i];

                const float dlyL = read (0, dL);
                const float dlyR = read (1, dR);

                float wL, wR;
                if (ping)
                {
                    const float inMono = 0.5f * (inL + inR);
                    wL = inMono + dlyR * fb;
                    wR = dlyL * fb;
                }
                else
                {
                    wL = inL + dlyL * fb;
                    wR = inR + dlyR * fb;
                }

                // High-cut in the feedback loop.
                lp[0] += damp * (wL - lp[0]);
                lp[1] += damp * (wR - lp[1]);

                line[0][(size_t) writePos] = lp[0];
                line[1][(size_t) writePos] = lp[1];

                L[i] = inL + wet * dlyL;
                R[i] = inR + wet * dlyR;

                if (++writePos >= size)
                    writePos = 0;
            }
        }

    private:
        float read (int ch, float delaySamples) const noexcept
        {
            double rp = (double) writePos - (double) delaySamples;
            while (rp < 0.0) rp += size;

            const int    i0   = (int) rp;
            const double frac = rp - (double) i0;
            const int    i1   = (i0 + 1) % size;

            const float a = line[(size_t) ch][(size_t) i0];
            const float b = line[(size_t) ch][(size_t) i1];
            return a + (float) frac * (b - a);
        }

        std::vector<float> line[2];
        double sampleRate = 44100.0;
        int    size       = 4;
        int    writePos   = 0;
        float  dL = 1.0f, dR = 1.0f, fb = 0.0f, wet = 0.0f, damp = 0.5f;
        float  lp[2] { 0.0f, 0.0f };
        bool   ping = false;
    };
}
