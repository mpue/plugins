/*
  ==============================================================================

    VisualState.h
    Realtime-safe data the audio thread publishes for the editor's animated
    visualisations (level meter, oscilloscope, envelope playheads). The processor
    owns one instance and writes it from processBlock; the editor polls it from a
    Timer. All access is lock-free; benign races on the scope ring are acceptable
    for a display.

  ==============================================================================
*/

#pragma once

#include <atomic>
#include "../dsp/Mseg.h"

namespace pike
{
    struct VisualState
    {
        static constexpr int scopeSize = 1024;          // power of two

        VisualState()
        {
            for (auto& p : msegPhase)
                p.store (-1.0f, std::memory_order_relaxed);
        }

        std::atomic<float> meterL { 0.0f };             // block peak, 0..1
        std::atomic<float> meterR { 0.0f };

        std::atomic<int>   triggerId { 0 };             // bumps on each note-on
        std::atomic<bool>  gate { false };              // true while a note is held

        // MSEG playheads: 0..1 phase of the most recently triggered sounding
        // voice per MSEG, or < 0 when that MSEG has no active playhead.
        std::atomic<float> msegPhase[mseg::maxMsegs];

        float              scope[scopeSize] = {};       // mono output ring
        std::atomic<int>   scopeWrite { 0 };

        void pushScope (float s) noexcept
        {
            const int w = scopeWrite.load (std::memory_order_relaxed);
            scope[w] = s;
            scopeWrite.store ((w + 1) & (scopeSize - 1), std::memory_order_relaxed);
        }
    };
}
