/*
  ==============================================================================

    ParameterIDs.h
    Central, stable string IDs for all APVTS parameters. IDs are append-only:
    once shipped, never rename or reuse an ID (it would break saved presets and
    DAW automation). New sections add their IDs here as the synth grows.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

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

    //==============================================================================
    // Oscillators 1..3 (index 0..2). Per-oscillator parameter IDs.
    inline constexpr const char* oscWave[3]   = { "osc1Wave",   "osc2Wave",   "osc3Wave"   }; // choice
    inline constexpr const char* oscOctave[3] = { "osc1Octave", "osc2Octave", "osc3Octave" }; // -3..+3
    inline constexpr const char* oscSemi[3]   = { "osc1Semi",   "osc2Semi",   "osc3Semi"   }; // -12..+12
    inline constexpr const char* oscFine[3]   = { "osc1Fine",   "osc2Fine",   "osc3Fine"   }; // cents
    inline constexpr const char* oscLevel[3]  = { "osc1Level",  "osc2Level",  "osc3Level"  }; // 0..1
    inline constexpr const char* oscPW[3]     = { "osc1PW",     "osc2PW",     "osc3PW"     }; // pulse width
    inline constexpr const char* oscWtPos[3]  = { "osc1WtPos",  "osc2WtPos",  "osc3WtPos"  }; // wavetable pos

    //==============================================================================
    // Oscillator routing / mixer
    inline constexpr auto osc2Sync     = "osc2Sync";      // hard-sync osc2 to osc1
    inline constexpr auto osc3Sync     = "osc3Sync";      // hard-sync osc3 to osc1
    inline constexpr auto fmAmount     = "fmAmount";      // osc3 -> osc1 FM depth
    inline constexpr auto ringModLevel = "ringModLevel";  // osc1 x osc2 into mix
    inline constexpr auto noiseLevel   = "noiseLevel";    // noise into mix

    //==============================================================================
    // Filter
    inline constexpr auto filterType      = "filterType";      // choice LP/BP/HP
    inline constexpr auto filterSlope     = "filterSlope";     // choice 12/24 dB
    inline constexpr auto filterCutoff    = "filterCutoff";    // Hz
    inline constexpr auto filterResonance = "filterResonance"; // 0..1
    inline constexpr auto filterKeyTrack  = "filterKeyTrack";  // 0..1
    inline constexpr auto filterDrive     = "filterDrive";     // 0..1 pre-filter overdrive
    inline constexpr auto filterEnvAmount = "filterEnvAmount"; // -1..1 (octaves)

    //==============================================================================
    // Filter envelope (ADSR)
    inline constexpr auto filtAttack  = "filtAttack";
    inline constexpr auto filtDecay   = "filtDecay";
    inline constexpr auto filtSustain = "filtSustain";
    inline constexpr auto filtRelease = "filtRelease";

    //==============================================================================
    // Aux envelope (ADSR) — the 3rd, freely-assignable envelope
    inline constexpr auto auxAttack  = "auxAttack";
    inline constexpr auto auxDecay   = "auxDecay";
    inline constexpr auto auxSustain = "auxSustain";
    inline constexpr auto auxRelease = "auxRelease";

    //==============================================================================
    // LFOs 1..2 (index 0..1)
    inline constexpr const char* lfoShape[2]   = { "lfo1Shape",   "lfo2Shape"   }; // choice
    inline constexpr const char* lfoSync[2]    = { "lfo1Sync",    "lfo2Sync"    }; // bool tempo-sync
    inline constexpr const char* lfoRate[2]    = { "lfo1Rate",    "lfo2Rate"    }; // free Hz
    inline constexpr const char* lfoDiv[2]     = { "lfo1Div",     "lfo2Div"     }; // sync division
    inline constexpr const char* lfoKeySync[2] = { "lfo1KeySync", "lfo2KeySync" }; // retrigger
    inline constexpr const char* lfoMono[2]    = { "lfo1Mono",    "lfo2Mono"    }; // mono vs poly
    inline constexpr const char* lfoFade[2]    = { "lfo1Fade",    "lfo2Fade"    }; // fade-in seconds

    //==============================================================================
    // Modulation matrix (16 slots). IDs built deterministically per slot.
    inline juce::String modSourceId (int slot) { return "mod" + juce::String (slot) + "Src"; }
    inline juce::String modDestId    (int slot) { return "mod" + juce::String (slot) + "Dst"; }
    inline juce::String modDepthId   (int slot) { return "mod" + juce::String (slot) + "Depth"; }
}
