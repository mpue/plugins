/*
  ==============================================================================

    ParameterLayout.cpp

  ==============================================================================
*/

#include "ParameterLayout.h"
#include "ParameterIDs.h"

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
            return v < 1.0f ? juce::String (v * 1000.0f, 0) + " ms"
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
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v * 100.0f, 0) + " %"; })));

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

            auto pctText = [] (float v, int) { return juce::String (v * 100.0f, 0) + " %"; };

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
            auto pctText = [] (float v, int) { return juce::String (v * 100.0f, 0) + " %"; };

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

            auto pctText = [] (float v, int) { return juce::String (v * 100.0f, 0) + " %"; };

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
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v * 100.0f, 0) + " %"; })));

            layout.add (std::make_unique<APF> (
                ID { pid::filtRelease, pid::version }, "Filter Release",
                timeRange (0.001f, 12.0f, 0.3f), 0.3f, attrSeconds));
        }

        return layout;
    }
}
