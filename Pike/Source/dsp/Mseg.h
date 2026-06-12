/*
  ==============================================================================

    Mseg.h
    Multi-segment envelope (MSEG) modulation source.
    Pure DSP (no JUCE).

    - Data: fixed-size, trivially copyable snapshot of one envelope
      (sorted breakpoints with per-segment curvature, optional loop region).
      Invariants (sorted times, first point at t=0, last at t=1, valid loop
      indices) are enforced by the snapshot builder (MsegStore), never here.
    - SharedData: single-writer seqlock so the audio thread can pull fresh
      snapshots wait-free and allocation-free once per block.
    - Player: per-voice playhead. Phase runs 0..1 over the whole envelope
      (the caller scales time via the per-sample increment). While the gate
      is held and looping is enabled, the playhead cycles loopStart..loopEnd;
      after note-off it runs past the loop to the final point and holds.

  ==============================================================================
*/

#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>

namespace pike::mseg
{
    inline constexpr int maxMsegs  = 4;
    inline constexpr int maxPoints = 32;

    /** Curve shaping shared by DSP and GUI: u in 0..1, curve in -1..+1.
        curve > 0 bends towards a late rise (slow start), curve < 0 towards
        an early rise. curve == 0 is exactly linear. */
    inline float shapeU (float u, float curve) noexcept
    {
        if (curve == 0.0f)
            return u;
        return std::pow (u, std::exp2 (3.0f * curve));
    }

    struct Point
    {
        float t = 0.0f;       // 0..1 normalized time
        float v = 0.0f;       // 0..1 value
        float curve = 0.0f;   // -1..+1 curvature of the segment starting here
    };

    struct Data
    {
        int   numPoints = 0;             // < 2 => inactive (source outputs 0)
        int   loopStart = -1;            // point indices; -1 = no loop
        int   loopEnd   = -1;
        Point points[maxPoints];

        bool hasLoop() const noexcept
        {
            return loopStart >= 0 && loopEnd > loopStart && loopEnd < numPoints
                && points[loopEnd].t - points[loopStart].t > 1.0e-6f;
        }
    };

    /** Single-writer seqlock. Writers must be externally serialized (MsegStore
        holds a mutex); the audio-thread reader never blocks. */
    struct SharedData
    {
        void write (const Data& d) noexcept
        {
            const uint32_t s = seq.load (std::memory_order_relaxed);
            seq.store (s + 1, std::memory_order_relaxed);          // odd: write in progress
            std::atomic_thread_fence (std::memory_order_release);
            data = d;
            std::atomic_thread_fence (std::memory_order_release);
            seq.store (s + 2, std::memory_order_release);          // even: stable
        }

        /** Copies into out if a new stable snapshot is available; returns true
            on success and updates lastSeen. On a torn read the previous copy
            in out stays valid and we simply retry next block. */
        bool readIfChanged (Data& out, uint32_t& lastSeen) const noexcept
        {
            const uint32_t s1 = seq.load (std::memory_order_acquire);
            if ((s1 & 1u) != 0u || s1 == lastSeen)
                return false;

            Data tmp = data;
            std::atomic_thread_fence (std::memory_order_acquire);
            if (seq.load (std::memory_order_relaxed) != s1)
                return false;

            out = tmp;
            lastSeen = s1;
            return true;
        }

        std::atomic<uint32_t> seq { 0 };
        Data data;
    };

    class Player
    {
    public:
        void noteOn() noexcept
        {
            phase = 0.0;
            seg = 0;
            gate = true;
            cachedSeg = -1;
        }

        void noteOff() noexcept { gate = false; }

        /** Advance one sample. inc is the phase increment per sample
            (1 / totalLengthInSamples). Returns the envelope value 0..1. */
        float process (const Data& d, double inc, bool loopOn) noexcept
        {
            if (d.numPoints < 2)
                return 0.0f;

            const int last = d.numPoints - 1;

            // Loop wrap while the note is held.
            if (gate && loopOn && d.hasLoop())
            {
                const double tEnd  = d.points[d.loopEnd].t;
                const double span  = tEnd - d.points[d.loopStart].t;
                if (phase >= tEnd)
                {
                    do { phase -= span; } while (phase >= tEnd);
                    seg = d.loopStart;
                    cachedSeg = -1;
                }
            }

            if (seg > last)            // data shrank under us (snapshot swap)
            {
                seg = last;
                cachedSeg = -1;
            }

            // Walk forward to the segment containing the current phase.
            while (seg < last && phase >= d.points[seg + 1].t)
            {
                ++seg;
                cachedSeg = -1;
            }
            if (phase < d.points[seg].t)   // snapshot swap moved times around
            {
                seg = 0;
                while (seg < last && phase >= d.points[seg + 1].t)
                    ++seg;
                cachedSeg = -1;
            }

            float value;
            if (seg >= last)
            {
                value = d.points[last].v;  // end hold
            }
            else
            {
                const Point& p0 = d.points[seg];
                const Point& p1 = d.points[seg + 1];
                const double dt = (double) p1.t - (double) p0.t;
                float u = dt > 1.0e-9 ? (float) ((phase - p0.t) / dt) : 1.0f;
                u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);

                if (p0.curve != 0.0f)
                {
                    if (seg != cachedSeg)
                    {
                        curveExp = std::exp2 (3.0f * p0.curve);
                        cachedSeg = seg;
                    }
                    u = std::pow (u, curveExp);
                }
                value = p0.v + (p1.v - p0.v) * u;
            }

            phase += inc;
            return value;
        }

        double position() const noexcept { return phase; }
        bool   isHeld()  const noexcept { return gate; }

    private:
        double phase = 0.0;
        int    seg = 0;
        bool   gate = false;
        int    cachedSeg = -1;
        float  curveExp = 1.0f;
    };
}
