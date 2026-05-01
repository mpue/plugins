/*
  ==============================================================================
    CP-1 Compressor — Preset Manager
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);

    int getNumPresets() const;
    juce::String getPresetName (int index) const;
    juce::StringArray getAllPresetNames() const;

    void loadPreset (int index);
    void loadNextPreset();
    void loadPreviousPreset();

    int saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);
    bool isFactoryPreset (int index) const;
    int getNumFactoryPresets() const { return numFactoryPresets; }

    int getCurrentPresetIndex() const { return currentIndex; }
    void setCurrentPresetIndex (int index);

    void refreshUserPresets();

private:
    juce::AudioProcessorValueTreeState& apvts;

    struct Preset
    {
        juce::String name;
        bool isFactory = false;
        juce::File file;
    };

    std::vector<Preset> presets;
    int currentIndex = 0;
    int numFactoryPresets = 0;

    struct FactoryData
    {
        const char* name;
        float threshold, ratio, attack, release, knee, makeup, mix, scHpf;
        int detector;
        bool stereoLink, autoRelease;
    };

    static const std::vector<FactoryData>& getFactoryData();
    void buildPresetList();
    juce::File getUserPresetDirectory() const;

    juce::ValueTree buildState (float threshold, float ratio, float attack,
                                float release, float knee, float makeup,
                                float mix, float scHpf, int detector,
                                bool stereoLink, bool autoRelease) const;
};
