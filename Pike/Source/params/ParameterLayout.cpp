/*
  ==============================================================================

    ParameterLayout.cpp

  ==============================================================================
*/

#include "ParameterLayout.h"
#include "ParameterIDs.h"
#include "ModMatrixDefs.h"

namespace pike
{
    using APF   = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;
    using ID    = juce::ParameterID;

    namespace
    {
        // A time range (seconds) with a musical skew so short times get more
        // resolution near the bottom of the knob.
        Range timeRange (float minS, float maxS, float defaultForSkew)
        {
            Range r (minS, maxS);
            r.setSkewForCentre (defaultForSkew);
            return r;
        }

        juce::String secondsText (float v, int)
        {
            return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms"
                            : juce::String (v, 2) + " s";
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        //======================================================================
        // Master
        {
            Range gainRange (-60.0f, 6.0f, 0.1f);
            layout.add (std::make_unique<APF> (
                ID { pid::masterGain, pid::version }, "Master Gain",
                gainRange, 0.0f,
                juce::AudioParameterFloatAttributes()
                    .withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })));
        }

        //======================================================================
        // Amp envelope
        {
            auto attrSeconds = juce::AudioParameterFloatAttributes()
                                   .withStringFromValueFunction (secondsText);

            layout.add (std::make_unique<APF> (
                ID { pid::ampAttack, pid::version }, "Amp Attack",
                timeRange (0.001f, 10.0f, 0.05f), 0.005f, attrSeconds));

            layout.add (std::make_unique<APF> (
                ID { pid::ampDecay, pid::version }, "Amp Decay",
                timeRange (0.001f, 10.0f, 0.2f), 0.15f, attrSeconds));

            layout.add (std::make_unique<APF> (
                ID { pid::ampSustain, pid::version }, "Amp Sustain",
                Range (0.0f, 1.0f, 0.001f), 0.8f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));

            layout.add (std::make_unique<APF> (
                ID { pid::ampRelease, pid::version }, "Amp Release",
                timeRange (0.001f, 12.0f, 0.3f), 0.2f, attrSeconds));
        }

        //======================================================================
        // Oscillators 1..3
        {
            const juce::StringArray waveforms { "Sine", "Triangle", "Saw", "Pulse", "Wavetable" };

            // Only osc 1 sounds by default; 2 and 3 start silent.
            const float defaultLevel[3] = { 0.8f, 0.0f, 0.0f };

            auto pctText = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };

            for (int n = 0; n < 3; ++n)
            {
                const auto label = "Osc " + juce::String (n + 1) + " ";

                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    ID { pid::oscWave[n], pid::version }, label + "Wave", waveforms, 0));

                layout.add (std::make_unique<juce::AudioParameterInt> (
                    ID { pid::oscOctave[n], pid::version }, label + "Octave", -3, 3, 0));

                layout.add (std::make_unique<juce::AudioParameterInt> (
                    ID { pid::oscSemi[n], pid::version }, label + "Semitone", -12, 12, 0));

                layout.add (std::make_unique<APF> (
                    ID { pid::oscFine[n], pid::version }, label + "Fine",
                    Range (-100.0f, 100.0f, 0.1f), 0.0f,
                    juce::AudioParameterFloatAttributes()
                        .withLabel ("ct")
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ct"; })));

                layout.add (std::make_unique<APF> (
                    ID { pid::oscLevel[n], pid::version }, label + "Level",
                    Range (0.0f, 1.0f, 0.001f), defaultLevel[n],
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

                layout.add (std::make_unique<APF> (
                    ID { pid::oscPW[n], pid::version }, label + "Pulse Width",
                    Range (0.02f, 0.98f, 0.001f), 0.5f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

                layout.add (std::make_unique<APF> (
                    ID { pid::oscWtPos[n], pid::version }, label + "WT Pos",
                    Range (0.0f, 1.0f, 0.001f), 0.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
            }
        }

        //======================================================================
        // Oscillator routing / mixer
        {
            auto pctText = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };

            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::osc2Sync, pid::version }, "Osc2 Sync", false));
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::osc3Sync, pid::version }, "Osc3 Sync", false));

            layout.add (std::make_unique<APF> (
                ID { pid::fmAmount, pid::version }, "FM Amount (3>1)",
                Range (0.0f, 1.0f, 0.001f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

            layout.add (std::make_unique<APF> (
                ID { pid::ringModLevel, pid::version }, "Ring Mod (1x2)",
                Range (0.0f, 1.0f, 0.001f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

            layout.add (std::make_unique<APF> (
                ID { pid::noiseLevel, pid::version }, "Noise Level",
                Range (0.0f, 1.0f, 0.001f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
        }

        //======================================================================
        // Filter
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                ID { pid::filterType, pid::version }, "Filter Type",
                juce::StringArray { "Low Pass", "Band Pass", "High Pass" }, 0));

            layout.add (std::make_unique<juce::AudioParameterChoice> (
                ID { pid::filterSlope, pid::version }, "Filter Slope",
                juce::StringArray { "12 dB/oct", "24 dB/oct" }, 0));

            Range cutoffRange (20.0f, 20000.0f);
            cutoffRange.setSkewForCentre (1000.0f);
            layout.add (std::make_unique<APF> (
                ID { pid::filterCutoff, pid::version }, "Cutoff",
                cutoffRange, 20000.0f,
                juce::AudioParameterFloatAttributes()
                    .withLabel ("Hz")
                    .withStringFromValueFunction ([] (float v, int)
                    {
                        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                            : juce::String (v, 0) + " Hz";
                    })));

            auto pctText = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };

            layout.add (std::make_unique<APF> (
                ID { pid::filterResonance, pid::version }, "Resonance",
                Range (0.0f, 1.0f, 0.001f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

            layout.add (std::make_unique<APF> (
                ID { pid::filterKeyTrack, pid::version }, "Key Track",
                Range (0.0f, 1.0f, 0.001f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

            layout.add (std::make_unique<APF> (
                ID { pid::filterDrive, pid::version }, "Drive",
                Range (0.0f, 1.0f, 0.001f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

            layout.add (std::make_unique<APF> (
                ID { pid::filterEnvAmount, pid::version }, "Filter Env Amount",
                Range (-1.0f, 1.0f, 0.001f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
        }

        //======================================================================
        // Filter envelope
        {
            auto attrSeconds = juce::AudioParameterFloatAttributes()
                                   .withStringFromValueFunction (secondsText);

            layout.add (std::make_unique<APF> (
                ID { pid::filtAttack, pid::version }, "Filter Attack",
                timeRange (0.001f, 10.0f, 0.05f), 0.005f, attrSeconds));

            layout.add (std::make_unique<APF> (
                ID { pid::filtDecay, pid::version }, "Filter Decay",
                timeRange (0.001f, 10.0f, 0.2f), 0.2f, attrSeconds));

            layout.add (std::make_unique<APF> (
                ID { pid::filtSustain, pid::version }, "Filter Sustain",
                Range (0.0f, 1.0f, 0.001f), 0.6f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));

            layout.add (std::make_unique<APF> (
                ID { pid::filtRelease, pid::version }, "Filter Release",
                timeRange (0.001f, 12.0f, 0.3f), 0.3f, attrSeconds));
        }

        //======================================================================
        // Aux envelope
        {
            auto attrSeconds = juce::AudioParameterFloatAttributes()
                                   .withStringFromValueFunction (secondsText);

            layout.add (std::make_unique<APF> (
                ID { pid::auxAttack, pid::version }, "Aux Attack",
                timeRange (0.001f, 10.0f, 0.05f), 0.005f, attrSeconds));
            layout.add (std::make_unique<APF> (
                ID { pid::auxDecay, pid::version }, "Aux Decay",
                timeRange (0.001f, 10.0f, 0.2f), 0.2f, attrSeconds));
            layout.add (std::make_unique<APF> (
                ID { pid::auxSustain, pid::version }, "Aux Sustain",
                Range (0.0f, 1.0f, 0.001f), 0.5f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
            layout.add (std::make_unique<APF> (
                ID { pid::auxRelease, pid::version }, "Aux Release",
                timeRange (0.001f, 12.0f, 0.3f), 0.3f, attrSeconds));
        }

        //======================================================================
        // LFOs
        {
            const juce::StringArray shapes   { "Sine", "Triangle", "Saw", "Square", "S&H" };
            const juce::StringArray divisions { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/32" };

            for (int n = 0; n < 2; ++n)
            {
                const auto label = "LFO " + juce::String (n + 1) + " ";

                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    ID { pid::lfoShape[n], pid::version }, label + "Shape", shapes, 0));

                layout.add (std::make_unique<juce::AudioParameterBool> (
                    ID { pid::lfoSync[n], pid::version }, label + "Sync", false));

                Range rateRange (0.01f, 40.0f);
                rateRange.setSkewForCentre (2.0f);
                layout.add (std::make_unique<APF> (
                    ID { pid::lfoRate[n], pid::version }, label + "Rate",
                    rateRange, 1.0f,
                    juce::AudioParameterFloatAttributes()
                        .withLabel ("Hz")
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })));

                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    ID { pid::lfoDiv[n], pid::version }, label + "Division", divisions, 2));

                layout.add (std::make_unique<juce::AudioParameterBool> (
                    ID { pid::lfoKeySync[n], pid::version }, label + "Key Sync", true));

                layout.add (std::make_unique<juce::AudioParameterBool> (
                    ID { pid::lfoMono[n], pid::version }, label + "Mono", false));

                layout.add (std::make_unique<APF> (
                    ID { pid::lfoFade[n], pid::version }, label + "Fade In",
                    timeRange (0.0f, 10.0f, 0.5f), 0.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (secondsText)));
            }
        }

        //======================================================================
        // MSEGs — multi-segment envelopes (time base only; points live in
        // MsegStore's ValueTree). Version hint 2: added after first release.
        {
            const juce::StringArray msegDivisions { "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars" };

            for (int n = 0; n < pid::numMsegs; ++n)
            {
                const auto label = "MSEG " + juce::String (n + 1) + " ";

                layout.add (std::make_unique<juce::AudioParameterBool> (
                    ID { pid::msegSync[n], 2 }, label + "Sync", false));

                layout.add (std::make_unique<APF> (
                    ID { pid::msegRate[n], 2 }, label + "Time",
                    timeRange (0.05f, 20.0f, 1.0f), 1.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (secondsText)));

                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    ID { pid::msegDiv[n], 2 }, label + "Length", msegDivisions, 4));

                layout.add (std::make_unique<juce::AudioParameterBool> (
                    ID { pid::msegLoop[n], 2 }, label + "Loop", false));
            }
        }

        //======================================================================
        // Modulation matrix
        {
            const auto sources = mod::sourceNames();
            const auto dests   = mod::destNames();

            for (int s = 0; s < mod::numSlots; ++s)
            {
                const auto label = "Mod " + juce::String (s + 1) + " ";

                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    ID { pid::modSourceId (s), pid::version }, label + "Source", sources, 0));

                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    ID { pid::modDestId (s), pid::version }, label + "Dest", dests, 0));

                layout.add (std::make_unique<APF> (
                    ID { pid::modDepthId (s), pid::version }, label + "Depth",
                    Range (-1.0f, 1.0f, 0.001f), 0.0f,
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
            }
        }

        //======================================================================
        // FX chain
        {
            auto pctText = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };

            // Distortion
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::distOn, pid::version }, "Distortion On", false));
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                ID { pid::distType, pid::version }, "Distortion Type",
                juce::StringArray { "Soft", "Hard", "Fold" }, 0));
            layout.add (std::make_unique<APF> (
                ID { pid::distDrive, pid::version }, "Distortion Drive",
                Range (0.0f, 1.0f, 0.001f), 0.3f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
            layout.add (std::make_unique<APF> (
                ID { pid::distMix, pid::version }, "Distortion Mix",
                Range (0.0f, 1.0f, 0.001f), 1.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

            // Chorus
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::chorusOn, pid::version }, "Chorus On", false));
            {
                Range chorusRateRange (0.05f, 10.0f);
                chorusRateRange.setSkewForCentre (1.0f);
                layout.add (std::make_unique<APF> (
                    ID { pid::chorusRate, pid::version }, "Chorus Rate",
                    chorusRateRange, 1.0f,
                    juce::AudioParameterFloatAttributes()
                        .withLabel ("Hz")
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })));
            }
            layout.add (std::make_unique<APF> (
                ID { pid::chorusDepth, pid::version }, "Chorus Depth",
                Range (0.0f, 1.0f, 0.001f), 0.25f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
            layout.add (std::make_unique<APF> (
                ID { pid::chorusFeedback, pid::version }, "Chorus Feedback",
                Range (-0.95f, 0.95f, 0.001f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
            layout.add (std::make_unique<APF> (
                ID { pid::chorusMix, pid::version }, "Chorus Mix",
                Range (0.0f, 1.0f, 0.001f), 0.5f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

            // Delay
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::delayOn, pid::version }, "Delay On", false));
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::delaySync, pid::version }, "Delay Sync", true));
            {
                Range delayTimeRange (1.0f, 2000.0f);
                delayTimeRange.setSkewForCentre (300.0f);
                layout.add (std::make_unique<APF> (
                    ID { pid::delayTime, pid::version }, "Delay Time",
                    delayTimeRange, 300.0f,
                    juce::AudioParameterFloatAttributes()
                        .withLabel ("ms")
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0) + " ms"; })));
            }
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                ID { pid::delayDiv, pid::version }, "Delay Division",
                juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/32" }, 2));
            layout.add (std::make_unique<APF> (
                ID { pid::delayFeedback, pid::version }, "Delay Feedback",
                Range (0.0f, 0.98f, 0.001f), 0.4f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
            layout.add (std::make_unique<APF> (
                ID { pid::delayMix, pid::version }, "Delay Mix",
                Range (0.0f, 1.0f, 0.001f), 0.3f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::delayPingpong, pid::version }, "Delay Ping-Pong", false));

            // Reverb
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::reverbOn, pid::version }, "Reverb On", false));
            layout.add (std::make_unique<APF> (
                ID { pid::reverbSize, pid::version }, "Reverb Size",
                Range (0.0f, 1.0f, 0.001f), 0.5f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
            layout.add (std::make_unique<APF> (
                ID { pid::reverbDamping, pid::version }, "Reverb Damping",
                Range (0.0f, 1.0f, 0.001f), 0.5f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
            layout.add (std::make_unique<APF> (
                ID { pid::reverbWidth, pid::version }, "Reverb Width",
                Range (0.0f, 1.0f, 0.001f), 1.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
            layout.add (std::make_unique<APF> (
                ID { pid::reverbMix, pid::version }, "Reverb Mix",
                Range (0.0f, 1.0f, 0.001f), 0.3f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
        }

        //======================================================================
        // Arpeggiator
        {
            auto pctText = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };

            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::arpOn, pid::version }, "Arp On", false));
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                ID { pid::arpMode, pid::version }, "Arp Mode",
                juce::StringArray { "Up", "Down", "Up/Down", "Random", "As Played" }, 0));
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                ID { pid::arpRate, pid::version }, "Arp Rate",
                juce::StringArray { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 3));
            layout.add (std::make_unique<APF> (
                ID { pid::arpGate, pid::version }, "Arp Gate",
                Range (0.05f, 1.0f, 0.001f), 0.5f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
            layout.add (std::make_unique<juce::AudioParameterInt> (
                ID { pid::arpOctaves, pid::version }, "Arp Octaves", 1, 4, 1));
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::arpLatch, pid::version }, "Arp Latch", false));
        }

        //======================================================================
        // Voice mode / unison / glide
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                ID { pid::voiceMode, pid::version }, "Voice Mode",
                juce::StringArray { "Poly", "Mono", "Legato" }, 0));
            layout.add (std::make_unique<juce::AudioParameterInt> (
                ID { pid::unisonCount, pid::version }, "Unison Voices", 1, 7, 1));
            layout.add (std::make_unique<APF> (
                ID { pid::unisonDetune, pid::version }, "Unison Detune",
                Range (0.0f, 50.0f, 0.1f), 15.0f,
                juce::AudioParameterFloatAttributes()
                    .withLabel ("ct")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ct"; })));
            layout.add (std::make_unique<APF> (
                ID { pid::glideTime, pid::version }, "Glide Time",
                timeRange (0.0f, 2.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (secondsText)));

            layout.add (std::make_unique<APF> (
                ID { pid::stereoWidth, pid::version }, "Stereo Width",
                Range (0.0f, 2.0f, 0.001f), 1.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
        }

        //======================================================================
        // Mix / EQ — 8-band parametric EQ
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ID { pid::eqOn, pid::version }, "EQ On", false));

            const float defFreq[8] = { 60.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 14000.0f };
            const float defQ[8]    = { 0.7f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f };

            Range freqRange (20.0f, 20000.0f);  freqRange.setSkewForCentre (1000.0f);
            Range qRange    (0.1f, 10.0f);      qRange.setSkewForCentre (1.0f);

            auto hzText = [] (float v, int)
            {
                return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, 0) + " Hz";
            };

            for (int b = 0; b < 8; ++b)
            {
                const auto label = "EQ " + juce::String (b + 1) + " ";

                layout.add (std::make_unique<APF> (
                    ID { pid::eqFreq[b], pid::version }, label + "Freq",
                    freqRange, defFreq[b],
                    juce::AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (hzText)));

                layout.add (std::make_unique<APF> (
                    ID { pid::eqGain[b], pid::version }, label + "Gain",
                    Range (-24.0f, 24.0f, 0.1f), 0.0f,
                    juce::AudioParameterFloatAttributes()
                        .withLabel ("dB")
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })));

                layout.add (std::make_unique<APF> (
                    ID { pid::eqQ[b], pid::version }, label + "Q",
                    qRange, defQ[b],
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })));
            }
        }

        return layout;
    }
}
