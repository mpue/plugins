/*
  ==============================================================================

    FactoryPresets.h
    Built-in presets defined as parameter overrides (plain values). Loading a
    preset first resets all parameters to their defaults, then applies these.

    Value conventions: choices = 0-based index, bools = 0/1, ints = plain value,
    floats = plain value in the parameter's range.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../params/ParameterIDs.h"
#include "../params/ModMatrixDefs.h"

namespace pike
{
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    inline std::vector<Preset> buildFactoryPresets()
    {
        using V = std::vector<std::pair<juce::String, float>>;

        std::vector<Preset> presets;

        // 1. Init — pure defaults (single sine).
        presets.push_back ({ "Init", {} });

        // 2. Classic Poly — two detuned saws through a gentle low-pass.
        presets.push_back ({ "Classic Poly", V {
            { pid::oscWave[0], 2 }, { pid::oscLevel[0], 0.7f },
            { pid::oscWave[1], 2 }, { pid::oscLevel[1], 0.6f }, { pid::oscFine[1], 7.0f },
            { pid::filterCutoff, 4000.0f }, { pid::filterResonance, 0.15f },
            { pid::filterEnvAmount, 0.35f }, { pid::filtDecay, 0.8f }, { pid::filtSustain, 0.4f },
            { pid::ampAttack, 0.01f }, { pid::ampRelease, 0.4f } } });

        // 3. Fat Unison Lead — 5-voice unison saw with glide.
        presets.push_back ({ "Fat Unison Lead", V {
            { pid::oscWave[0], 2 }, { pid::oscLevel[0], 0.8f },
            { pid::unisonCount, 5 }, { pid::unisonDetune, 22.0f },
            { pid::glideTime, 0.06f },
            { pid::filterCutoff, 6000.0f }, { pid::filterResonance, 0.2f },
            { pid::filterEnvAmount, 0.3f }, { pid::filtDecay, 0.6f }, { pid::filtSustain, 0.5f },
            { pid::ampAttack, 0.01f }, { pid::ampRelease, 0.3f },
            { pid::chorusOn, 1 }, { pid::chorusMix, 0.35f } } });

        // 4. FM Bell — osc3 FMs osc1; quick metallic decay.
        presets.push_back ({ "FM Bell", V {
            { pid::oscWave[0], 0 }, { pid::oscLevel[0], 0.85f },
            { pid::oscWave[2], 0 }, { pid::oscOctave[2], 1 },
            { pid::fmAmount, 0.5f },
            { pid::filterCutoff, 18000.0f },
            { pid::ampAttack, 0.001f }, { pid::ampDecay, 1.6f }, { pid::ampSustain, 0.0f }, { pid::ampRelease, 1.2f },
            { pid::reverbOn, 1 }, { pid::reverbMix, 0.3f } } });

        // 5. Wavetable Pad — morphing wavetables, slow swell, lush FX.
        presets.push_back ({ "Wavetable Pad", V {
            { pid::oscWave[0], 4 }, { pid::oscWtPos[0], 0.35f }, { pid::oscLevel[0], 0.7f },
            { pid::oscWave[1], 4 }, { pid::oscWtPos[1], 0.6f }, { pid::oscLevel[1], 0.5f }, { pid::oscFine[1], -6.0f },
            { pid::filterCutoff, 2500.0f }, { pid::filterResonance, 0.1f },
            { pid::filterEnvAmount, 0.4f }, { pid::filtAttack, 0.9f }, { pid::filtSustain, 0.6f },
            { pid::ampAttack, 0.8f }, { pid::ampRelease, 1.8f }, { pid::ampSustain, 0.9f },
            { pid::lfoShape[0], 0 }, { pid::lfoRate[0], 0.3f },
            { pid::modSourceId (0), (float) (int) mod::Source::Lfo1 },
            { pid::modDestId (0),   (float) (int) mod::Dest::WavetablePos },
            { pid::modDepthId (0),  0.3f },
            { pid::chorusOn, 1 }, { pid::chorusMix, 0.4f },
            { pid::reverbOn, 1 }, { pid::reverbSize, 0.8f }, { pid::reverbMix, 0.45f } } });

        // 6. Acid Bass — mono, resonant 24 dB low-pass with envelope + drive.
        presets.push_back ({ "Acid Bass", V {
            { pid::voiceMode, 1 }, { pid::glideTime, 0.04f },
            { pid::oscWave[0], 2 }, { pid::oscLevel[0], 0.9f }, { pid::oscOctave[0], -1 },
            { pid::filterType, 0 }, { pid::filterSlope, 1 },
            { pid::filterCutoff, 250.0f }, { pid::filterResonance, 0.75f },
            { pid::filterEnvAmount, 0.6f }, { pid::filterDrive, 0.4f },
            { pid::filtAttack, 0.001f }, { pid::filtDecay, 0.35f }, { pid::filtSustain, 0.1f },
            { pid::ampAttack, 0.001f }, { pid::ampDecay, 0.6f }, { pid::ampSustain, 0.7f }, { pid::ampRelease, 0.15f } } });

        // 7. Sync Lead — osc2 hard-synced and detuned upward.
        presets.push_back ({ "Sync Lead", V {
            { pid::voiceMode, 1 },
            { pid::oscWave[0], 2 }, { pid::oscLevel[0], 0.5f },
            { pid::oscWave[1], 2 }, { pid::oscLevel[1], 0.8f }, { pid::oscSemi[1], 7 },
            { pid::osc2Sync, 1 },
            { pid::filterCutoff, 9000.0f }, { pid::filterEnvAmount, 0.25f },
            { pid::modSourceId (0), (float) (int) mod::Source::Env3 },
            { pid::modDestId (0),   (float) (int) mod::Dest::Osc2Pitch },
            { pid::modDepthId (0),  0.4f },
            { pid::auxDecay, 0.5f }, { pid::auxSustain, 0.2f },
            { pid::ampAttack, 0.005f }, { pid::ampRelease, 0.25f } } });

        // 8. Arp Pluck — tempo-synced arpeggio with delay.
        presets.push_back ({ "Arp Pluck", V {
            { pid::oscWave[0], 3 }, { pid::oscLevel[0], 0.8f },
            { pid::filterCutoff, 5000.0f }, { pid::filterResonance, 0.2f },
            { pid::filterEnvAmount, 0.4f }, { pid::filtDecay, 0.25f }, { pid::filtSustain, 0.0f },
            { pid::ampAttack, 0.001f }, { pid::ampDecay, 0.3f }, { pid::ampSustain, 0.0f }, { pid::ampRelease, 0.2f },
            { pid::arpOn, 1 }, { pid::arpMode, 0 }, { pid::arpRate, 3 }, { pid::arpOctaves, 2 }, { pid::arpGate, 0.6f },
            { pid::delayOn, 1 }, { pid::delaySync, 1 }, { pid::delayDiv, 3 }, { pid::delayMix, 0.3f }, { pid::delayFeedback, 0.35f } } });

        // 9. Ambient Keys — soft pad with vibrato and big space.
        presets.push_back ({ "Ambient Keys", V {
            { pid::oscWave[0], 4 }, { pid::oscWtPos[0], 0.2f }, { pid::oscLevel[0], 0.7f },
            { pid::oscWave[1], 0 }, { pid::oscLevel[1], 0.3f }, { pid::oscOctave[1], 1 },
            { pid::filterCutoff, 4000.0f }, { pid::filterEnvAmount, 0.2f },
            { pid::ampAttack, 0.25f }, { pid::ampRelease, 1.5f }, { pid::ampSustain, 0.85f },
            { pid::lfoShape[0], 0 }, { pid::lfoRate[0], 5.0f }, { pid::lfoFade[0], 0.6f },
            { pid::modSourceId (0), (float) (int) mod::Source::Lfo1 },
            { pid::modDestId (0),   (float) (int) mod::Dest::Osc1Pitch },
            { pid::modDepthId (0),  0.04f },
            { pid::delayOn, 1 }, { pid::delaySync, 1 }, { pid::delayMix, 0.25f },
            { pid::reverbOn, 1 }, { pid::reverbSize, 0.85f }, { pid::reverbMix, 0.5f } } });

        // 10. Ring Stab — ring modulation hit.
        presets.push_back ({ "Ring Stab", V {
            { pid::oscWave[0], 2 }, { pid::oscLevel[0], 0.5f },
            { pid::oscWave[1], 0 }, { pid::oscLevel[1], 0.0f }, { pid::oscSemi[1], 5 },
            { pid::ringModLevel, 0.7f },
            { pid::filterCutoff, 7000.0f }, { pid::filterEnvAmount, 0.3f },
            { pid::filtDecay, 0.3f }, { pid::filtSustain, 0.0f },
            { pid::ampAttack, 0.001f }, { pid::ampDecay, 0.4f }, { pid::ampSustain, 0.0f }, { pid::ampRelease, 0.2f },
            { pid::distOn, 1 }, { pid::distType, 0 }, { pid::distDrive, 0.3f }, { pid::distMix, 0.5f } } });

        return presets;
    }
}
