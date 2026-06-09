/*
  ==============================================================================

    ParameterLayout.h
    Builds the APVTS parameter layout for Pike. Each phase extends this with
    its section's parameters.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace pike
{
    /** Creates the full APVTS parameter layout for the processor. */
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
