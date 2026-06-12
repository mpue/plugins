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

        //======================================================================
        // Showcase presets

        // 11. PWM Strings — pulse oscillators with LFO pulse-width modulation.
        presets.push_back ({ "PWM Strings", V {
            { pid::oscWave[0], 3 }, { pid::oscLevel[0], 0.7f }, { pid::oscPW[0], 0.5f },
            { pid::oscWave[1], 3 }, { pid::oscLevel[1], 0.6f }, { pid::oscFine[1], 7.0f }, { pid::oscPW[1], 0.5f },
            { pid::filterCutoff, 3200.0f }, { pid::filterResonance, 0.12f },
            { pid::filterEnvAmount, 0.2f }, { pid::filtAttack, 0.4f }, { pid::filtSustain, 0.7f },
            { pid::ampAttack, 0.35f }, { pid::ampSustain, 0.9f }, { pid::ampRelease, 1.1f },
            { pid::lfoShape[0], 1 }, { pid::lfoRate[0], 0.35f }, { pid::lfoFade[0], 0.6f },
            { pid::modSourceId (0), (float) (int) mod::Source::Lfo1 },
            { pid::modDestId (0),   (float) (int) mod::Dest::PulseWidth }, { pid::modDepthId (0), 0.30f },
            { pid::lfoShape[1], 0 }, { pid::lfoRate[1], 5.0f }, { pid::lfoMono[1], 1 },
            { pid::modSourceId (1), (float) (int) mod::Source::Lfo2 },
            { pid::modDestId (1),   (float) (int) mod::Dest::Osc2Pitch }, { pid::modDepthId (1), 0.012f },
            { pid::chorusOn, 1 }, { pid::chorusMix, 0.45f }, { pid::chorusDepth, 0.4f },
            { pid::reverbOn, 1 }, { pid::reverbSize, 0.7f }, { pid::reverbMix, 0.35f },
            { pid::eqOn, 1 }, { pid::eqGain[7], 4.0f } } });

        // 12. Tempo Wobble — tempo-synced LFO sweeping a resonant 24 dB low-pass.
        presets.push_back ({ "Tempo Wobble", V {
            { pid::voiceMode, 1 }, { pid::glideTime, 0.02f },
            { pid::oscWave[0], 2 }, { pid::oscLevel[0], 0.9f }, { pid::oscOctave[0], -1 },
            { pid::oscWave[1], 3 }, { pid::oscLevel[1], 0.5f }, { pid::oscOctave[1], -1 }, { pid::oscFine[1], 6.0f },
            { pid::filterType, 0 }, { pid::filterSlope, 1 }, { pid::filterCutoff, 180.0f },
            { pid::filterResonance, 0.55f }, { pid::filterDrive, 0.35f },
            { pid::ampAttack, 0.002f }, { pid::ampSustain, 1.0f }, { pid::ampRelease, 0.1f },
            { pid::lfoShape[0], 0 }, { pid::lfoSync[0], 1 }, { pid::lfoDiv[0], 3 }, { pid::lfoMono[0], 1 },
            { pid::modSourceId (0), (float) (int) mod::Source::Lfo1 },
            { pid::modDestId (0),   (float) (int) mod::Dest::Cutoff }, { pid::modDepthId (0), 0.45f },
            { pid::distOn, 1 }, { pid::distType, 0 }, { pid::distDrive, 0.25f }, { pid::distMix, 0.5f },
            { pid::eqOn, 1 }, { pid::eqGain[0], 4.0f } } });

        // 13. FM E-Piano — Rhodes-ish: osc3 FMs osc1, aux env fades the FM out.
        presets.push_back ({ "FM E-Piano", V {
            { pid::oscWave[0], 0 }, { pid::oscLevel[0], 0.85f },
            { pid::oscWave[2], 0 }, { pid::oscOctave[2], 1 }, { pid::oscLevel[2], 0.0f },
            { pid::fmAmount, 0.12f },
            { pid::modSourceId (0), (float) (int) mod::Source::Env3 },
            { pid::modDestId (0),   (float) (int) mod::Dest::FmAmount }, { pid::modDepthId (0), 0.40f },
            { pid::auxAttack, 0.001f }, { pid::auxDecay, 0.4f }, { pid::auxSustain, 0.15f }, { pid::auxRelease, 0.3f },
            { pid::filterCutoff, 7000.0f },
            { pid::ampAttack, 0.002f }, { pid::ampDecay, 1.6f }, { pid::ampSustain, 0.25f }, { pid::ampRelease, 0.7f },
            { pid::chorusOn, 1 }, { pid::chorusMix, 0.3f }, { pid::chorusRate, 0.6f },
            { pid::reverbOn, 1 }, { pid::reverbSize, 0.5f }, { pid::reverbMix, 0.25f },
            { pid::eqOn, 1 }, { pid::eqFreq[3], 500.0f }, { pid::eqGain[3], -3.0f }, { pid::eqGain[7], 3.0f } } });

        // 14. Evolving WT Pad — morphing wavetables driven by two LFOs + lush FX.
        presets.push_back ({ "Evolving WT Pad", V {
            { pid::oscWave[0], 4 }, { pid::oscWtPos[0], 0.2f }, { pid::oscLevel[0], 0.7f },
            { pid::oscWave[1], 4 }, { pid::oscWtPos[1], 0.55f }, { pid::oscLevel[1], 0.5f }, { pid::oscFine[1], -7.0f },
            { pid::filterCutoff, 2600.0f }, { pid::filterEnvAmount, 0.3f }, { pid::filtAttack, 1.0f }, { pid::filtSustain, 0.6f },
            { pid::ampAttack, 0.9f }, { pid::ampSustain, 0.9f }, { pid::ampRelease, 1.8f },
            { pid::lfoShape[0], 0 }, { pid::lfoRate[0], 0.25f },
            { pid::modSourceId (0), (float) (int) mod::Source::Lfo1 },
            { pid::modDestId (0),   (float) (int) mod::Dest::WavetablePos }, { pid::modDepthId (0), 0.40f },
            { pid::lfoShape[1], 1 }, { pid::lfoRate[1], 0.15f },
            { pid::modSourceId (1), (float) (int) mod::Source::Lfo2 },
            { pid::modDestId (1),   (float) (int) mod::Dest::Cutoff }, { pid::modDepthId (1), 0.25f },
            { pid::chorusOn, 1 }, { pid::chorusMix, 0.4f },
            { pid::delayOn, 1 }, { pid::delaySync, 1 }, { pid::delayMix, 0.25f }, { pid::delayFeedback, 0.4f },
            { pid::reverbOn, 1 }, { pid::reverbSize, 0.85f }, { pid::reverbMix, 0.45f },
            { pid::eqOn, 1 }, { pid::eqGain[7], 3.0f } } });

        // 15. Hypersaw Anthem — 7-voice unison, wide stereo, ping-pong + reverb.
        presets.push_back ({ "Hypersaw Anthem", V {
            { pid::oscWave[0], 2 }, { pid::oscLevel[0], 0.8f },
            { pid::oscWave[1], 2 }, { pid::oscLevel[1], 0.5f }, { pid::oscOctave[1], 1 },
            { pid::unisonCount, 7 }, { pid::unisonDetune, 26.0f }, { pid::stereoWidth, 1.6f },
            { pid::filterCutoff, 7500.0f }, { pid::filterEnvAmount, 0.3f }, { pid::filtDecay, 0.6f }, { pid::filtSustain, 0.6f },
            { pid::ampAttack, 0.01f }, { pid::ampRelease, 0.4f },
            { pid::delayOn, 1 }, { pid::delaySync, 1 }, { pid::delayDiv, 3 }, { pid::delayPingpong, 1 },
            { pid::delayMix, 0.3f }, { pid::delayFeedback, 0.35f },
            { pid::reverbOn, 1 }, { pid::reverbSize, 0.7f }, { pid::reverbMix, 0.3f },
            { pid::eqOn, 1 }, { pid::eqGain[0], 3.0f }, { pid::eqGain[7], 4.0f } } });

        // 16. Sub Boom 808 — sine sub with a fast pitch-drop (aux env -> osc1 pitch).
        presets.push_back ({ "Sub Boom 808", V {
            { pid::oscWave[0], 0 }, { pid::oscOctave[0], -1 }, { pid::oscLevel[0], 1.0f },
            { pid::modSourceId (0), (float) (int) mod::Source::Env3 },
            { pid::modDestId (0),   (float) (int) mod::Dest::Osc1Pitch }, { pid::modDepthId (0), 0.50f },
            { pid::auxAttack, 0.001f }, { pid::auxDecay, 0.06f }, { pid::auxSustain, 0.0f },
            { pid::ampAttack, 0.002f }, { pid::ampDecay, 0.9f }, { pid::ampSustain, 0.0f }, { pid::ampRelease, 0.3f },
            { pid::filterDrive, 0.2f },
            { pid::distOn, 1 }, { pid::distType, 0 }, { pid::distDrive, 0.2f }, { pid::distMix, 0.4f },
            { pid::eqOn, 1 }, { pid::eqFreq[0], 50.0f }, { pid::eqGain[0], 6.0f } } });

        // 17. Cinematic Riser — noise + saw through a slow 4 s filter-env sweep.
        presets.push_back ({ "Cinematic Riser", V {
            { pid::oscWave[0], 2 }, { pid::oscLevel[0], 0.4f },
            { pid::noiseLevel, 0.5f },
            { pid::filterType, 0 }, { pid::filterSlope, 1 }, { pid::filterCutoff, 100.0f }, { pid::filterResonance, 0.5f },
            { pid::filterEnvAmount, 0.9f }, { pid::filtAttack, 4.0f }, { pid::filtSustain, 1.0f },
            { pid::ampAttack, 0.5f }, { pid::ampSustain, 1.0f }, { pid::ampRelease, 1.0f },
            { pid::lfoShape[0], 0 }, { pid::lfoRate[0], 5.0f },
            { pid::modSourceId (0), (float) (int) mod::Source::Lfo1 },
            { pid::modDestId (0),   (float) (int) mod::Dest::Osc1Pitch }, { pid::modDepthId (0), 0.06f },
            { pid::delayOn, 1 }, { pid::delaySync, 1 }, { pid::delayMix, 0.2f },
            { pid::reverbOn, 1 }, { pid::reverbSize, 0.9f }, { pid::reverbMix, 0.5f } } });

        //======================================================================
        // Sound-designer presets — converted from user patches (diffs against
        // defaults, generated from the original .pikepreset XML snapshots).

        // 18. Fatter Unison Lead — 5-voice unison saw, brighter EQ, hot output.
        presets.push_back ({ "Fatter Unison Lead", V {
            { pid::masterGain, 6 }, { pid::ampAttack, 0.01f }, { pid::ampRelease, 0.3f },
            { pid::noiseLevel, 0.013f }, { pid::filterCutoff, 6000.0f }, { pid::filterResonance, 0.2f },
            { pid::filterEnvAmount, 0.3f }, { pid::filtDecay, 0.6f }, { pid::filtSustain, 0.5f },
            { pid::chorusOn, 1 }, { pid::chorusMix, 0.35f }, { pid::unisonCount, 5 },
            { pid::unisonDetune, 22 }, { pid::glideTime, 0.06f }, { pid::eqOn, 1 },
            { pid::oscWave[0], 2 }, { pid::eqFreq[0], 2520.944f }, { pid::eqGain[0], 5.7f },
            { pid::eqFreq[5], 4522.075f }, { pid::eqGain[5], 8 }, { pid::eqFreq[7], 8556.34f },
            { pid::eqGain[7], 5.6f } } });

        // 19. Long Voyage Pad — slow triple-saw swell, driven 24 dB filter, wide.
        presets.push_back ({ "Long Voyage Pad", V {
            { pid::ampAttack, 1.47187f }, { pid::ampDecay, 0.29141f }, { pid::ampSustain, 1 },
            { pid::ampRelease, 4.61177f }, { pid::noiseLevel, 0.111f }, { pid::filterSlope, 1 },
            { pid::filterCutoff, 79.4915f }, { pid::filterResonance, 0.379f }, { pid::filterKeyTrack, 0.896f },
            { pid::filterDrive, 0.585f }, { pid::filterEnvAmount, 1 }, { pid::filtAttack, 10 },
            { pid::filtDecay, 4.18308f }, { pid::filtSustain, 0.178f }, { pid::filtRelease, 7.17617f },
            { pid::auxAttack, 0.00106f }, { pid::unisonCount, 3 }, { pid::stereoWidth, 2 },
            { pid::oscWave[0], 2 }, { pid::oscWave[1], 2 }, { pid::oscFine[1], 11.9f },
            { pid::oscLevel[1], 1 }, { pid::oscWave[2], 2 }, { pid::oscFine[2], -5.5f },
            { pid::oscLevel[2], 0.805f } } });

        // 20. PikaBass — wavetable bass, hard distortion, LFO-modulated, punchy EQ.
        presets.push_back ({ "PikaBass", V {
            { pid::ampRelease, 0.06637f }, { pid::filterCutoff, 236.2072f }, { pid::filterResonance, 0.358f },
            { pid::filterDrive, 0.792f }, { pid::filterEnvAmount, 1 }, { pid::filtAttack, 0.52547f },
            { pid::filtDecay, 0.94554f }, { pid::filtSustain, 0 }, { pid::filtRelease, 0.23457f },
            { pid::distOn, 1 }, { pid::distType, 1 }, { pid::distDrive, 0.481f },
            { pid::chorusOn, 1 }, { pid::chorusDepth, 0.562f }, { pid::chorusFeedback, -0.242f },
            { pid::chorusMix, 0.405f }, { pid::reverbOn, 1 }, { pid::reverbSize, 0.098f },
            { pid::reverbWidth, 0.719f }, { pid::arpOctaves, 2 }, { pid::unisonCount, 4 },
            { pid::unisonDetune, 34.6f }, { pid::glideTime, 0.00215f }, { pid::eqOn, 1 },
            { pid::oscWave[0], 4 }, { pid::oscWtPos[0], 0.282f }, { pid::lfoRate[0], 0.38644f },
            { pid::modSourceId (0), 3 }, { pid::modDestId (0), 5 }, { pid::modDepthId (0), 0.616f },
            { pid::modSourceId (1), 5 }, { pid::modDestId (1), 12 }, { pid::modDepthId (1), 0.64f },
            { pid::modDepthId (2), -0.144f }, { pid::modDepthId (5), -0.211f }, { pid::modDepthId (8), -0.275f },
            { pid::eqFreq[3], 60.46467f }, { pid::eqGain[3], 8.9f }, { pid::eqFreq[6], 6579.75f },
            { pid::eqGain[6], 2 }, { pid::eqFreq[7], 14492.13f }, { pid::eqGain[7], 15.4f } } });

        // 21. Spinner's Glocke — synced bell, long decays, latched up/down arp.
        presets.push_back ({ "Spinner's Glocke", V {
            { pid::ampAttack, 0.01f }, { pid::ampDecay, 8.24462f }, { pid::ampSustain, 1 },
            { pid::ampRelease, 2.10139f }, { pid::osc2Sync, 1 }, { pid::filterCutoff, 1687.938f },
            { pid::filterResonance, 0.435f }, { pid::filterKeyTrack, 0.86f }, { pid::filterDrive, 0.838f },
            { pid::filterEnvAmount, 0.3f }, { pid::filtDecay, 8.40314f }, { pid::filtSustain, 0.867f },
            { pid::auxAttack, 0.001f }, { pid::auxDecay, 0.01294f }, { pid::auxSustain, 0 },
            { pid::distType, 1 }, { pid::distDrive, 0.213f }, { pid::chorusOn, 1 },
            { pid::chorusMix, 0.35f }, { pid::delayTime, 499.1984f }, { pid::delayFeedback, 0.626f },
            { pid::delayPingpong, 1 }, { pid::reverbOn, 1 }, { pid::reverbSize, 0.897f },
            { pid::reverbDamping, 0.152f }, { pid::reverbMix, 0.097f }, { pid::arpMode, 2 },
            { pid::arpOctaves, 2 }, { pid::arpLatch, 1 }, { pid::unisonCount, 3 },
            { pid::unisonDetune, 15.2f }, { pid::stereoWidth, 2 }, { pid::eqOn, 1 },
            { pid::oscWave[0], 2 }, { pid::oscOctave[0], 1 }, { pid::oscWave[1], 2 },
            { pid::oscOctave[1], 1 }, { pid::oscLevel[1], 0.917f }, { pid::oscPW[1], 0.598f },
            { pid::modSourceId (0), 3 }, { pid::modDestId (0), 2 }, { pid::modDepthId (0), 1 },
            { pid::eqFreq[3], 906.027f }, { pid::eqGain[3], 0.4f }, { pid::eqFreq[6], 6579.26f },
            { pid::eqGain[6], 17.4f } } });

        return presets;
    }
}
