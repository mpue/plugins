/*
  ==============================================================================

    PresetManager.h
    Preset management for PS-1: factory presets baked in code, user presets
    saved as XML in the user's application data folder.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PresetManager : public juce::ChangeBroadcaster
{
public:
    struct Preset
    {
        juce::String name;
        bool         isFactory = false;
        std::map<juce::String, float> values;
    };

    PresetManager (juce::AudioProcessorValueTreeState& s);

    juce::File getUserPresetsFolder() const;

    void initialise();

    const std::vector<Preset>& getFactoryPresets() const noexcept { return factoryPresets; }
    const std::vector<Preset>& getUserPresets()    const noexcept { return userPresets; }

    int getNumFactoryPresets() const noexcept { return (int) factoryPresets.size(); }
    int getNumUserPresets()    const noexcept { return (int) userPresets.size(); }

    juce::String getCurrentPresetName() const { return currentPresetName; }
    bool         currentPresetIsFactory() const { return currentIsFactory; }

    void loadFactoryPreset (int index);
    void loadUserPreset    (int index);
    void loadPresetByName  (const juce::String& name);

    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);

    void nextPreset();
    void previousPreset();

    void rescanUserPresets();

private:
    juce::AudioProcessorValueTreeState& apvts;

    std::vector<Preset> factoryPresets;
    std::vector<Preset> userPresets;

    juce::String currentPresetName;
    bool         currentIsFactory = true;

    void buildFactoryPresets();
    void applyPreset (const Preset& p);
    static juce::String sanitizeFilename (const juce::String& name);
};
