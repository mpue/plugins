/*
  ==============================================================================

    PresetManager.h
    Preset management for CRV-1: factory presets baked into the binary, user
    presets stored as XML in the user's application data folder. Each preset
    captures every APVTS parameter — including the selected IR — so saved
    sounds recall exactly.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <map>
#include <vector>

class PresetManager : public juce::ChangeBroadcaster
{
public:
    struct Preset
    {
        juce::String name;
        bool         isFactory = false;
        juce::String irName; // display-name of the IR (so presets can pick a factory or user IR)
        std::map<juce::String, float> values;
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& s);

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

    bool saveUserPreset (const juce::String& name, const juce::String& irName);
    bool deleteUserPreset (const juce::String& name);

    void nextPreset();
    void previousPreset();

    void rescanUserPresets();

    // The IR selection sits outside APVTS (it's a string, not a parameter).
    // Callers wire a callback to get notified when a preset wants to switch IR.
    std::function<void (const juce::String& irName)> onIRChangeRequest;

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
