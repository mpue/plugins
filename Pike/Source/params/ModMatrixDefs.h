/*
  ==============================================================================

    ModMatrixDefs.h
    Shared definitions for the modulation matrix: source/destination enums, their
    display names, per-destination scaling and the slot count. Used by both the
    parameter layout (choice lists) and the voice (routing), so they never drift.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace pike::mod
{
    enum class Source
    {
        None = 0,
        Env1,        // amp envelope
        Env2,        // filter envelope
        Env3,        // aux envelope
        Lfo1,
        Lfo2,
        Velocity,
        ModWheel,
        Aftertouch,
        KeyTrack,
        Count
    };

    enum class Dest
    {
        None = 0,
        Osc1Pitch,
        Osc2Pitch,
        Osc3Pitch,
        PulseWidth,
        WavetablePos,
        FmAmount,
        Cutoff,
        Resonance,
        Osc1Level,
        Osc2Level,
        Osc3Level,
        Lfo1Rate,
        Lfo2Rate,
        Count
    };

    inline constexpr int numSources = (int) Source::Count;
    inline constexpr int numDests   = (int) Dest::Count;
    inline constexpr int numSlots   = 16;

    inline juce::StringArray sourceNames()
    {
        return { "None", "Env 1 (Amp)", "Env 2 (Filter)", "Env 3 (Aux)",
                 "LFO 1", "LFO 2", "Velocity", "Mod Wheel", "Aftertouch", "Key Track" };
    }

    inline juce::StringArray destNames()
    {
        return { "None", "Osc1 Pitch", "Osc2 Pitch", "Osc3 Pitch", "Pulse Width",
                 "Wavetable Pos", "FM Amount", "Cutoff", "Resonance",
                 "Osc1 Level", "Osc2 Level", "Osc3 Level", "LFO1 Rate", "LFO2 Rate" };
    }

    // Full-scale amount applied when source*depth == 1, per destination, in that
    // destination's natural unit (semitones, octaves or normalised 0..1).
    inline constexpr float destScale[numDests] =
    {
        0.0f,   // None
        24.0f,  // Osc1Pitch  (semitones)
        24.0f,  // Osc2Pitch
        24.0f,  // Osc3Pitch
        0.5f,   // PulseWidth
        1.0f,   // WavetablePos
        1.0f,   // FmAmount
        8.0f,   // Cutoff      (octaves)
        1.0f,   // Resonance
        1.0f,   // Osc1Level
        1.0f,   // Osc2Level
        1.0f,   // Osc3Level
        4.0f,   // Lfo1Rate    (octaves)
        4.0f    // Lfo2Rate
    };
}
