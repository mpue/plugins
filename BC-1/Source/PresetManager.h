/*
  ==============================================================================

    PresetManager.h
    Factory + user preset system for BC-1. Factory presets are defined in
    code; user presets are stored as XML state files in the application
    user-data directory and survive across sessions.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& valueTree)
        : apvts (valueTree)
    {
        initFactoryPresets();
        rescanUserPresets();
        currentPresetName = "Init";
    }

    static juce::StringArray getTrackedParameterIds()
    {
        return { "drive", "bits", "rate", "dither", "tone", "mix", "output" };
    }

    juce::StringArray getFactoryPresetNames() const
    {
        juce::StringArray names;
        for (auto& p : factoryPresets) names.add (p.name);
        return names;
    }

    juce::StringArray getUserPresetNames() const { return userPresetNames; }

    juce::StringArray getAllPresetNames() const
    {
        auto names = getFactoryPresetNames();
        names.addArray (userPresetNames);
        return names;
    }

    juce::String getCurrentPresetName() const { return currentPresetName; }

    int getCurrentIndex() const
    {
        return getAllPresetNames().indexOf (currentPresetName);
    }

    bool isFactoryPreset (const juce::String& name) const
    {
        for (auto& p : factoryPresets)
            if (p.name == name) return true;
        return false;
    }

    bool isUserPreset (const juce::String& name) const
    {
        return userPresetNames.contains (name);
    }

    juce::File getUserPresetDirectory() const
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("BC-1").getChildFile ("Presets");
        if (! dir.exists()) dir.createDirectory();
        return dir;
    }

    void loadPreset (const juce::String& name)
    {
        for (auto& p : factoryPresets)
        {
            if (p.name == name)
            {
                applyValues (p.values);
                setCurrentPresetName (name);
                return;
            }
        }

        auto file = getUserPresetDirectory().getChildFile (name + ".bc1preset");
        if (file.existsAsFile())
        {
            if (auto xml = juce::XmlDocument::parse (file))
            {
                auto state = juce::ValueTree::fromXml (*xml);
                if (state.isValid())
                {
                    apvts.replaceState (state);
                    setCurrentPresetName (name);
                    return;
                }
            }
        }
    }

    void loadPresetByIndex (int index)
    {
        auto names = getAllPresetNames();
        if (juce::isPositiveAndBelow (index, names.size()))
            loadPreset (names[index]);
    }

    void saveUserPreset (const juce::String& name)
    {
        auto trimmed = name.trim();
        if (trimmed.isEmpty() || isFactoryPreset (trimmed))
            return;

        auto file = getUserPresetDirectory().getChildFile (trimmed + ".bc1preset");
        auto state = apvts.copyState();
        if (auto xml = state.createXml())
            xml->writeTo (file, {});

        rescanUserPresets();
        setCurrentPresetName (trimmed, false);
        if (onPresetListChanged) onPresetListChanged();
    }

    void deleteUserPreset (const juce::String& name)
    {
        auto file = getUserPresetDirectory().getChildFile (name + ".bc1preset");
        if (file.existsAsFile()) file.deleteFile();

        rescanUserPresets();
        if (currentPresetName == name)
            currentPresetName = "Init";

        if (onPresetListChanged) onPresetListChanged();
        if (onCurrentPresetChanged) onCurrentPresetChanged();
    }

    void selectPrevious()
    {
        auto names = getAllPresetNames();
        if (names.isEmpty()) return;
        int idx = getCurrentIndex();
        idx = (idx <= 0) ? names.size() - 1 : idx - 1;
        loadPresetByIndex (idx);
    }

    void selectNext()
    {
        auto names = getAllPresetNames();
        if (names.isEmpty()) return;
        int idx = getCurrentIndex();
        idx = (idx + 1) % names.size();
        loadPresetByIndex (idx);
    }

    void resetToInit() { loadPreset ("Init"); }

    void setCurrentPresetNameForRestore (const juce::String& name)
    {
        setCurrentPresetName (name.isEmpty() ? juce::String ("Init") : name);
    }

    std::function<void()> onPresetListChanged;
    std::function<void()> onCurrentPresetChanged;

private:
    struct FactoryPreset
    {
        juce::String name;
        std::map<juce::String, float> values;
    };

    void addFactoryPreset (const juce::String& name,
                           float drive, float bits, float rate, float dither,
                           float tone,  float mix,  float output)
    {
        FactoryPreset p;
        p.name = name;
        p.values["drive"]  = drive;
        p.values["bits"]   = bits;
        p.values["rate"]   = rate;
        p.values["dither"] = dither;
        p.values["tone"]   = tone;
        p.values["mix"]    = mix;
        p.values["output"] = output;
        factoryPresets.add (std::move (p));
    }

    void initFactoryPresets()
    {
        factoryPresets.clear();

        //                  name              drive  bits  rate     dither tone   mix    out
        addFactoryPreset ("Init",              0.0f, 16.0f, 50000.f, 0.0f,  0.00f, 1.00f,  0.0f);
        addFactoryPreset ("Subtle Glue",       1.5f, 15.0f, 44100.f, 0.4f, -0.05f, 0.30f,  0.0f);
        addFactoryPreset ("Vintage Warmth",    3.0f, 14.0f, 32000.f, 0.3f, -0.15f, 0.55f, -1.0f);
        addFactoryPreset ("Lo-Fi Tape",        4.5f, 11.0f, 18000.f, 0.5f, -0.35f, 1.00f, -2.0f);
        addFactoryPreset ("Crunchy Drums",     8.0f,  8.0f, 12000.f, 0.2f, -0.10f, 0.75f, -3.0f);
        addFactoryPreset ("Aliased Air",       2.0f, 12.0f,  6000.f, 0.1f,  0.45f, 1.00f, -2.0f);
        addFactoryPreset ("Telephone",         6.0f, 10.0f,  6500.f, 0.0f,  0.55f, 1.00f, -1.0f);
        addFactoryPreset ("Retro Synth",       4.0f,  9.0f, 11025.f, 0.15f,-0.05f, 0.85f, -2.0f);
        addFactoryPreset ("8-Bit Hero",        6.0f,  6.0f,  8000.f, 0.0f,  0.20f, 1.00f, -3.0f);
        addFactoryPreset ("Crushed Bass",     12.0f,  4.0f,  6000.f, 0.0f, -0.55f, 1.00f, -4.0f);
        addFactoryPreset ("Destroyed",        18.0f,  3.0f,  3500.f, 0.0f, -0.20f, 1.00f, -6.0f);
    }

    void rescanUserPresets()
    {
        userPresetNames.clear();
        auto dir = getUserPresetDirectory();
        if (! dir.exists()) return;

        for (auto file : dir.findChildFiles (juce::File::findFiles, false, "*.bc1preset"))
            userPresetNames.add (file.getFileNameWithoutExtension());

        userPresetNames.sortNatural();
    }

    void applyValues (const std::map<juce::String, float>& values)
    {
        for (auto& kv : values)
        {
            if (auto* p = apvts.getParameter (kv.first))
            {
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    const float norm = ranged->convertTo0to1 (kv.second);
                    ranged->setValueNotifyingHost (norm);
                }
            }
        }
    }

    void setCurrentPresetName (const juce::String& name, bool notify = true)
    {
        currentPresetName = name;
        if (notify && onCurrentPresetChanged)
            onCurrentPresetChanged();
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<FactoryPreset> factoryPresets;
    juce::StringArray          userPresetNames;
    juce::String               currentPresetName;
};
