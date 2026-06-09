/*
  ==============================================================================

    PikeSound.h
    The SynthesiserSound for Pike. A single sound that responds to all notes and
    channels; per-note behaviour lives entirely in PikeVoice.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace pike
{
    struct PikeSound : public juce::SynthesiserSound
    {
        bool appliesToNote    (int /*midiNoteNumber*/) override { return true; }
        bool appliesToChannel (int /*midiChannel*/)    override { return true; }
    };
}
