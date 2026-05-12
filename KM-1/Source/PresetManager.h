/*
  ==============================================================================

    PresetManager.h
    KM-1 factory + user preset system. Factory presets are defined in code;
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
        return { "tune", "pitchAmt", "pitchTime", "bodyDecay", "bodyShape",
                 "clickLevel", "clickTone", "subLevel",
                 "drive", "punch", "tone", "output" };
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
                       .getChildFile ("KM-1").getChildFile ("Presets");
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

        auto file = getUserPresetDirectory().getChildFile (name + ".km1preset");
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

        auto file = getUserPresetDirectory().getChildFile (trimmed + ".km1preset");
        auto state = apvts.copyState();
        if (auto xml = state.createXml())
            xml->writeTo (file, {});

        rescanUserPresets();
        setCurrentPresetName (trimmed, false);
        if (onPresetListChanged) onPresetListChanged();
    }

    void deleteUserPreset (const juce::String& name)
    {
        auto file = getUserPresetDirectory().getChildFile (name + ".km1preset");
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
                           float tune, float pitchAmt, float pitchTime, float bodyDecay,
                           float bodyShape, float clickLevel, float clickTone, float subLevel,
                           float drive, float punch, float tone, float output)
    {
        FactoryPreset p;
        p.name = name;
        p.values["tune"]       = tune;
        p.values["pitchAmt"]   = pitchAmt;
        p.values["pitchTime"]  = pitchTime;
        p.values["bodyDecay"]  = bodyDecay;
        p.values["bodyShape"]  = bodyShape;
        p.values["clickLevel"] = clickLevel;
        p.values["clickTone"]  = clickTone;
        p.values["subLevel"]   = subLevel;
        p.values["drive"]      = drive;
        p.values["punch"]      = punch;
        p.values["tone"]       = tone;
        p.values["output"]     = output;
        factoryPresets.add (std::move (p));
    }

    void initFactoryPresets()
    {
        factoryPresets.clear();

        //                  name              tune  pAmt  pTime  decay  shape clkL  clkT    sub  drive punch tone   out
        addFactoryPreset ("Init",              50.f, 36.f, 35.f,  500.f, 0.0f, 0.35f, 2200.f, 0.45f, 0.20f, 0.30f,  0.0f,  0.0f);
        addFactoryPreset ("Deep House",        45.f, 28.f, 45.f,  650.f, 0.0f, 0.20f, 1500.f, 0.55f, 0.15f, 0.25f, -0.10f, 0.0f);
        addFactoryPreset ("Techno Pillar",     52.f, 32.f, 30.f,  420.f, 0.0f, 0.30f, 2400.f, 0.40f, 0.35f, 0.45f,  0.05f, 1.5f);
        addFactoryPreset ("808 Sub",           38.f, 18.f, 60.f,  900.f, 0.0f, 0.10f, 1200.f, 0.70f, 0.10f, 0.10f, -0.30f, 0.0f);
        addFactoryPreset ("909 Punch",         60.f, 24.f, 18.f,  280.f, 0.0f, 0.55f, 3000.f, 0.30f, 0.25f, 0.55f,  0.10f, 1.0f);
        addFactoryPreset ("Trap Boom",         42.f, 40.f, 50.f, 1000.f, 0.0f, 0.25f, 1800.f, 0.65f, 0.30f, 0.40f, -0.20f, 1.0f);
        addFactoryPreset ("Hard Stomp",        58.f, 30.f, 22.f,  340.f, 0.4f, 0.45f, 2800.f, 0.35f, 0.55f, 0.65f,  0.15f, 2.0f);
        addFactoryPreset ("Lo-Fi Thump",       48.f, 26.f, 40.f,  520.f, 0.2f, 0.18f, 1100.f, 0.40f, 0.40f, 0.30f, -0.40f, 0.0f);
        addFactoryPreset ("Industrial",        55.f, 38.f, 25.f,  380.f, 0.6f, 0.60f, 3500.f, 0.30f, 0.65f, 0.55f,  0.25f, 1.0f);
        addFactoryPreset ("Dubstep Wob",       40.f, 20.f, 70.f, 1100.f, 0.0f, 0.15f, 1400.f, 0.75f, 0.20f, 0.20f, -0.25f, 0.0f);
        addFactoryPreset ("Minimal Click",     65.f, 22.f, 16.f,  220.f, 0.0f, 0.65f, 4000.f, 0.20f, 0.10f, 0.50f,  0.20f,-1.0f);
        addFactoryPreset ("Cinematic Hit",     35.f, 44.f, 80.f, 1500.f, 0.3f, 0.30f, 1800.f, 0.65f, 0.45f, 0.50f, -0.10f, 2.0f);
        addFactoryPreset ("Acid Pump",         70.f, 26.f, 20.f,  300.f, 0.5f, 0.50f, 3200.f, 0.25f, 0.50f, 0.60f,  0.30f, 1.5f);
        addFactoryPreset ("Soft Round",        46.f, 30.f, 55.f,  720.f, 0.0f, 0.20f, 1300.f, 0.50f, 0.05f, 0.15f, -0.05f, 0.0f);
        addFactoryPreset ("Fat Drumkit",       55.f, 28.f, 28.f,  450.f, 0.1f, 0.40f, 2500.f, 0.40f, 0.30f, 0.40f,  0.0f,  0.5f);
    }

    void rescanUserPresets()
    {
        userPresetNames.clear();
        auto dir = getUserPresetDirectory();
        if (! dir.exists()) return;

        for (auto file : dir.findChildFiles (juce::File::findFiles, false, "*.km1preset"))
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
