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
    // MSEGs 1..4 (index 0..3) — multi-segment envelopes. Point data lives in a
    // non-parameter ValueTree (see MsegStore.h); only the time base is automatable.
    inline constexpr int numMsegs = 4;
    inline constexpr const char* msegSync[numMsegs] = { "mseg1Sync", "mseg2Sync", "mseg3Sync", "mseg4Sync" }; // bool tempo-sync
    inline constexpr const char* msegRate[numMsegs] = { "mseg1Rate", "mseg2Rate", "mseg3Rate", "mseg4Rate" }; // total length, seconds
    inline constexpr const char* msegDiv[numMsegs]  = { "mseg1Div",  "mseg2Div",  "mseg3Div",  "mseg4Div"  }; // total length, beats (choice)
    inline constexpr const char* msegLoop[numMsegs] = { "mseg1Loop", "mseg2Loop", "mseg3Loop", "mseg4Loop" }; // bool loop enable

    //==============================================================================
    // Modulation matrix (16 slots). IDs built deterministically per slot.
    inline juce::String modSourceId (int slot) { return "mod" + juce::String (slot) + "Src"; }
    inline juce::String modDestId    (int slot) { return "mod" + juce::String (slot) + "Dst"; }
    inline juce::String modDepthId   (int slot) { return "mod" + juce::String (slot) + "Depth"; }

    //==============================================================================
    // FX chain: Distortion -> Chorus -> Delay -> Reverb
    inline constexpr auto distOn    = "distOn";
    inline constexpr auto distType  = "distType";    // choice Soft/Hard/Fold
    inline constexpr auto distDrive = "distDrive";
    inline constexpr auto distMix   = "distMix";

    inline constexpr auto chorusOn       = "chorusOn";
    inline constexpr auto chorusRate     = "chorusRate";
    inline constexpr auto chorusDepth    = "chorusDepth";
    inline constexpr auto chorusFeedback = "chorusFeedback";
    inline constexpr auto chorusMix      = "chorusMix";

    inline constexpr auto delayOn       = "delayOn";
    inline constexpr auto delaySync     = "delaySync";
    inline constexpr auto delayTime     = "delayTime";   // ms (free)
    inline constexpr auto delayDiv      = "delayDiv";    // sync division
    inline constexpr auto delayFeedback = "delayFeedback";
    inline constexpr auto delayMix      = "delayMix";
    inline constexpr auto delayPingpong = "delayPingpong";

    inline constexpr auto reverbOn      = "reverbOn";
    inline constexpr auto reverbSize    = "reverbSize";
    inline constexpr auto reverbDamping = "reverbDamping";
    inline constexpr auto reverbWidth   = "reverbWidth";
    inline constexpr auto reverbMix     = "reverbMix";

    //==============================================================================
    // Arpeggiator
    inline constexpr auto arpOn      = "arpOn";
    inline constexpr auto arpMode    = "arpMode";     // Up/Down/UpDown/Random/AsPlayed
    inline constexpr auto arpRate    = "arpRate";     // sync division
    inline constexpr auto arpGate    = "arpGate";     // 0..1
    inline constexpr auto arpOctaves = "arpOctaves";  // 1..4
    inline constexpr auto arpLatch   = "arpLatch";

    //==============================================================================
    // Voice mode / unison / glide
    inline constexpr auto voiceMode    = "voiceMode";    // Poly/Mono/Legato
    inline constexpr auto unisonCount  = "unisonCount";  // 1..7
    inline constexpr auto unisonDetune = "unisonDetune"; // cents
    inline constexpr auto glideTime    = "glideTime";    // seconds (0 = off)
    inline constexpr auto stereoWidth  = "stereoWidth";  // 0=mono .. 1=normal .. 2=wide

    //==============================================================================
    // Mix / EQ — 8-band parametric EQ (low shelf / 6 peaks / high shelf)
    inline constexpr auto eqOn = "eqOn";
    inline constexpr const char* eqFreq[8] =
        { "eq1Freq", "eq2Freq", "eq3Freq", "eq4Freq", "eq5Freq", "eq6Freq", "eq7Freq", "eq8Freq" };
    inline constexpr const char* eqGain[8] =
        { "eq1Gain", "eq2Gain", "eq3Gain", "eq4Gain", "eq5Gain", "eq6Gain", "eq7Gain", "eq8Gain" };
    inline constexpr const char* eqQ[8] =
        { "eq1Q", "eq2Q", "eq3Q", "eq4Q", "eq5Q", "eq6Q", "eq7Q", "eq8Q" };
}
