/*
  ==============================================================================

    PresetManager.h
    BS-1 factory + user preset system. Factory presets are defined in code;
    user presets are stored as XML files in the user data directory and
    survive across sessions.

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
        return { "tone", "drive", "subLevel", "noiseLevel", "octave",
                 "cutoff", "resonance", "envAmount", "filterDecay",
                 "ampAttack", "ampSustain", "ampRelease",
                 "glide", "warmth", "output" };
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
                       .getChildFile ("BS-1").getChildFile ("Presets");
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

        auto file = getUserPresetDirectory().getChildFile (name + ".bs1preset");
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

        auto file = getUserPresetDirectory().getChildFile (trimmed + ".bs1preset");
        auto state = apvts.copyState();
        if (auto xml = state.createXml())
            xml->writeTo (file, {});

        rescanUserPresets();
        setCurrentPresetName (trimmed, false);
        if (onPresetListChanged) onPresetListChanged();
    }

    void deleteUserPreset (const juce::String& name)
    {
        auto file = getUserPresetDirectory().getChildFile (name + ".bs1preset");
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
                           float tone, float drive, float subLevel, float noiseLevel,
                           float octave,
                           float cutoff, float resonance, float envAmount, float filterDecay,
                           float ampAttack, float ampSustain, float ampRelease,
                           float glide, float warmth, float output)
    {
        FactoryPreset p;
        p.name = name;
        p.values["tone"]        = tone;
        p.values["drive"]       = drive;
        p.values["subLevel"]    = subLevel;
        p.values["noiseLevel"]  = noiseLevel;
        p.values["octave"]      = octave;
        p.values["cutoff"]      = cutoff;
        p.values["resonance"]   = resonance;
        p.values["envAmount"]   = envAmount;
        p.values["filterDecay"] = filterDecay;
        p.values["ampAttack"]   = ampAttack;
        p.values["ampSustain"]  = ampSustain;
        p.values["ampRelease"]  = ampRelease;
        p.values["glide"]       = glide;
        p.values["warmth"]      = warmth;
        p.values["output"]      = output;
        factoryPresets.add (std::move (p));
    }

    void initFactoryPresets()
    {
        factoryPresets.clear();

        //                  name                tone  drv   sub  noise oct   cut   reso  env   fdec   atk   sus   rel    gld  warm  out
        addFactoryPreset ("Init",                0.55f, 0.30f, 0.55f, 0.02f, 0.f,  500.f, 0.55f, 0.65f, 280.f, 6.f, 0.85f, 220.f, 60.f, 0.40f, 0.0f);
        addFactoryPreset ("Deep Sub",            0.05f, 0.10f, 0.85f, 0.00f, -1.f, 220.f, 0.20f, 0.20f, 200.f, 8.f, 0.95f, 320.f, 30.f, 0.65f, 0.0f);
        addFactoryPreset ("Round Reese",         0.85f, 0.45f, 0.40f, 0.00f, 0.f,  650.f, 0.65f, 0.55f, 380.f, 10.f, 0.85f, 280.f, 80.f, 0.45f, -1.0f);
        addFactoryPreset ("Acid 303",            0.95f, 0.55f, 0.10f, 0.00f, 0.f,  280.f, 0.85f, 0.95f, 240.f, 4.f, 0.30f, 140.f, 50.f, 0.30f, 0.0f);
        addFactoryPreset ("Pluck Bass",          0.70f, 0.35f, 0.45f, 0.01f, 0.f,  420.f, 0.55f, 0.85f, 180.f, 3.f, 0.20f, 160.f, 20.f, 0.40f, 0.0f);
        addFactoryPreset ("808 Glide",           0.05f, 0.20f, 0.90f, 0.00f, -1.f, 240.f, 0.20f, 0.10f, 600.f, 5.f, 0.95f, 380.f, 180.f, 0.55f, 1.0f);
        addFactoryPreset ("Dub Wob",             0.85f, 0.40f, 0.50f, 0.00f, 0.f,  320.f, 0.75f, 0.65f, 520.f, 12.f, 0.80f, 320.f, 100.f, 0.50f, -0.5f);
        addFactoryPreset ("Funk Pick",           0.65f, 0.45f, 0.30f, 0.05f, 0.f,  900.f, 0.40f, 0.95f, 140.f, 2.f, 0.55f, 200.f, 5.f, 0.30f, 1.0f);
        addFactoryPreset ("Synth Wave Pad",      0.55f, 0.20f, 0.55f, 0.00f, 0.f,  800.f, 0.45f, 0.30f, 400.f, 80.f, 0.95f, 600.f, 40.f, 0.45f, -1.0f);
        addFactoryPreset ("Hard Sync Stab",      0.95f, 0.65f, 0.20f, 0.05f, 0.f,  550.f, 0.85f, 0.85f, 220.f, 2.f, 0.20f, 120.f, 0.f, 0.30f, 0.0f);
        addFactoryPreset ("Drum & Bass Reese",   0.70f, 0.55f, 0.45f, 0.00f, 0.f,  500.f, 0.70f, 0.45f, 360.f, 6.f, 0.85f, 280.f, 60.f, 0.40f, 0.0f);
        addFactoryPreset ("Latex Funk",          0.75f, 0.40f, 0.40f, 0.04f, 0.f,  650.f, 0.55f, 0.90f, 180.f, 3.f, 0.40f, 160.f, 10.f, 0.30f, 0.0f);
        addFactoryPreset ("Sub Drone",           0.05f, 0.05f, 0.95f, 0.00f, -2.f, 180.f, 0.10f, 0.10f, 1000.f, 200.f, 1.0f, 1500.f, 0.f, 0.55f, 0.0f);
        addFactoryPreset ("FM Snap",             0.85f, 0.60f, 0.30f, 0.02f, 0.f,  420.f, 0.80f, 0.95f, 160.f, 2.f, 0.10f, 100.f, 0.f, 0.30f, 0.0f);
        addFactoryPreset ("Cinematic Sub Hit",   0.10f, 0.30f, 0.90f, 0.05f, -1.f, 260.f, 0.45f, 0.60f, 800.f, 5.f, 0.30f, 1200.f, 60.f, 0.65f, 1.5f);
        addFactoryPreset ("Velvet Round",        0.40f, 0.20f, 0.65f, 0.00f, 0.f,  380.f, 0.30f, 0.40f, 320.f, 12.f, 0.85f, 260.f, 80.f, 0.55f, 0.0f);
    }

    void rescanUserPresets()
    {
        userPresetNames.clear();
        auto dir = getUserPresetDirectory();
        if (! dir.exists()) return;

        for (auto file : dir.findChildFiles (juce::File::findFiles, false, "*.bs1preset"))
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
