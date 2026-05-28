/*
  ==============================================================================

    IRLibrary.h
    Manages factory (procedurally generated) and user impulse responses for
    the CRV-1 luxury convolution reverb.

    Factory IRs are synthesised to sound large, smooth and three-dimensional:
    sparse early-reflection patterns followed by a dense decorrelated stereo
    tail with frequency-dependent decay (low end lingers, high end absorbs
    over time). The result is a "huge" but musical reverberation that avoids
    the metallic ring of simple noise-burst tails.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

class IRLibrary
{
public:
    struct Entry
    {
        juce::String name;
        bool         isFactory = true;
        int          factoryType = 0;   // index into factory generator switch
        juce::File   userFile;          // empty for factory entries
        float        baseLengthSec = 3.0f;
    };

    IRLibrary();

    void initialise (double sampleRate);

    void setSampleRate (double sampleRate);
    double getSampleRate() const noexcept { return currentSampleRate; }

    void rescanUserIRs();
    juce::File getUserIRFolder() const;
    bool importUserIR (const juce::File& source);
    bool deleteUserIR (const juce::String& displayName);

    const std::vector<Entry>& getEntries() const noexcept { return entries; }
    int  getNumEntries() const noexcept { return (int) entries.size(); }

    int  findEntryByName (const juce::String& name) const;

    // Build an AudioBuffer for the given entry, applying size (stretch) and
    // decay (extra envelope) modifiers. Both modifiers are 0..1, mapped to
    // 0.4..2.2 stretch and 0.5..2.0 decay multipliers internally.
    juce::AudioBuffer<float> renderIR (int entryIndex,
                                       float sizeNorm,
                                       float decayNorm,
                                       juce::String& outDisplayInfo);

private:
    static constexpr int kNumFactoryTypes = 12;

    std::vector<Entry> entries;
    double currentSampleRate = 48000.0;

    void buildFactoryEntries();

    // Procedural factory IR rendering
    juce::AudioBuffer<float> renderFactoryIR (int factoryType,
                                              float lengthSec,
                                              float decayMul,
                                              double sampleRate);

    // User IR loading
    juce::AudioBuffer<float> loadUserIRFile (const juce::File& f,
                                             double targetSampleRate,
                                             float& outOriginalLengthSec);

    // Helpers
    static void applyExtraEnvelope (juce::AudioBuffer<float>& buf, float decayMul);
    static void stretchInPlace (juce::AudioBuffer<float>& src, float ratio);
    static void resampleTo (const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out,
                            double srIn, double srOut);
    static void normaliseRMS (juce::AudioBuffer<float>& buf, float targetPeak = 0.8f);
};
