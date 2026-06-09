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

        return layout;
    }
}
