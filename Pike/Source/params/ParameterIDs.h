/*
  ==============================================================================

    ParameterIDs.h
    Central, stable string IDs for all APVTS parameters. IDs are append-only:
    once shipped, never rename or reuse an ID (it would break saved presets and
    DAW automation). New sections add their IDs here as the synth grows.

  ==============================================================================
*/

#pragma once

namespace pid
{
    // Version hint for juce::ParameterID. Bump only on incompatible reshuffles.
    inline constexpr int version = 1;

    //==============================================================================
    // Master
    inline constexpr auto masterGain = "masterGain";   // output level (dB)

    //==============================================================================
    // Amp envelope (ADSR)
    inline constexpr auto ampAttack  = "ampAttack";    // seconds
    inline constexpr auto ampDecay   = "ampDecay";     // seconds
    inline constexpr auto ampSustain = "ampSustain";   // 0..1
    inline constexpr auto ampRelease = "ampRelease";   // seconds
}
