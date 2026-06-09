/*
  ==============================================================================

    Oscillator.h
    Anti-aliased oscillator: PolyBLEP for the analogue waveforms (saw, pulse,
    triangle), an analytic sine, and a mip-mapped wavetable mode. Supports a
    per-sample FM frequency offset and hard sync (master/slave).

    Pure DSP: no JUCE dependencies, unit-testable.

  ==============================================================================
*/

#pragma once

#include <cmath>
#include <algorithm>
#include "Wavetable.h"

namespace pike
{
    enum class Waveform
    {
        Sine = 0,
        Triangle,
        Saw,
        Pulse,
        Wavetable
    };

    class Oscillator
    {
    public:
        void setSampleRate (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        }

        void setFrequency (float newFrequencyHz) noexcept { frequency = newFrequencyHz; }
        void setWaveform  (Waveform w)           noexcept { waveform = w; }
        void setPulseWidth (float pw)            noexcept { pulseWidth = std::clamp (pw, 0.02f, 0.98f); }
        void setWavetablePosition (float p)      noexcept { wavetablePos = std::clamp (p, 0.0f, 1.0f); }
        void setWavetable (const Wavetable* wt)  noexcept { wavetable = wt; }

        /** Full reset on note-on for a clean, repeatable attack. */
        void resetPhase() noexcept
        {
            phase    = 0.0;
            triState = triStart;   // start at the triangle minimum, no overshoot
            wrapped  = false;
        }

        /** Hard-sync this (slave) oscillator: restart its cycle. */
        void hardSync() noexcept
        {
            phase    = 0.0;
            triState = triStart;
        }

        /** True if the phase wrapped on the most recent processSample(). */
        bool didWrap() const noexcept { return wrapped; }

        /** Advances one sample. fmOffsetHz adds to the instantaneous frequency. */
        float processSample (float fmOffsetHz = 0.0f) noexcept
        {
            const double inc = (double) (frequency + fmOffsetHz) / sampleRate;
            const double dt  = inc;

            float out = 0.0f;

            switch (waveform)
            {
                case Waveform::Sine:
                    out = (float) std::sin (phase * twoPi);
                    break;

                case Waveform::Saw:
                {
                    double v = 2.0 * phase - 1.0;
                    v -= polyBlep (phase, dt);
                    out = (float) v;
                    break;
                }

                case Waveform::Pulse:
                {
                    double v = phase < pulseWidth ? 1.0 : -1.0;
                    v += polyBlep (phase, dt);
                    double t2 = phase - pulseWidth;
                    if (t2 < 0.0) t2 += 1.0;
                    v -= polyBlep (t2, dt);
                    out = (float) v;
                    break;
                }

                case Waveform::Triangle:
                {
                    // Band-limited square, then leaky-integrated into a triangle.
                    double sq = phase < 0.5 ? 1.0 : -1.0;
                    sq += polyBlep (phase, dt);
                    double t2 = phase - 0.5;
                    if (t2 < 0.0) t2 += 1.0;
                    sq -= polyBlep (t2, dt);

                    triState += 4.0 * std::abs (dt) * sq;
                    triState *= 0.9995;            // tiny leak removes DC drift
                    out = (float) triState;
                    break;
                }

                case Waveform::Wavetable:
                    out = wavetable != nullptr
                            ? wavetable->getSample (wavetablePos, phase, (double) (frequency + fmOffsetHz))
                            : (float) std::sin (phase * twoPi);
                    break;
            }

            // Advance and track wrap (for sync masters).
            phase  += dt;
            wrapped = false;
            if (phase >= 1.0)      { phase -= 1.0; wrapped = true; }
            else if (phase < 0.0)  { phase += 1.0; wrapped = true; }

            return out;
        }

    private:
        // PolyBLEP residual for a discontinuity at phase t with step dt.
        static double polyBlep (double t, double dt) noexcept
        {
            if (dt <= 0.0)
                return 0.0;

            if (t < dt)            { t /= dt;          return t + t - t * t - 1.0; }
            if (t > 1.0 - dt)      { t = (t - 1.0)/dt; return t * t + t + t + 1.0; }
            return 0.0;
        }

        static constexpr double twoPi = 6.283185307179586476925286766559;

        const Wavetable* wavetable = nullptr;

        double   sampleRate   = 44100.0;
        float    frequency    = 440.0f;
        Waveform waveform     = Waveform::Sine;
        float    pulseWidth   = 0.5f;
        float    wavetablePos = 0.0f;

        static constexpr double triStart = -1.0;  // triangle minimum at phase 0

        double phase    = 0.0;
        double triState = triStart;
        bool   wrapped  = false;
    };
}
