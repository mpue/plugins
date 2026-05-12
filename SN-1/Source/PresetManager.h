/*
  ==============================================================================

    PresetManager.h
    SN-1 factory + user preset system. Factory presets are defined in code;
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
        return { "tune", "pitchAmt", "pitchTime", "bodyDecay", "bodyLevel",
                 "noiseLevel", "noiseColor", "noiseDecay", "buzzQ", "snapLevel",
                 "drive", "tone", "output" };
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
                       .getChildFile ("SN-1").getChildFile ("Presets");
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

        auto file = getUserPresetDirectory().getChildFile (name + ".sn1preset");
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

        auto file = getUserPresetDirectory().getChildFile (trimmed + ".sn1preset");
        auto state = apvts.copyState();
        if (auto xml = state.createXml())
            xml->writeTo (file, {});

        rescanUserPresets();
        setCurrentPresetName (trimmed, false);
        if (onPresetListChanged) onPresetListChanged();
    }

    void deleteUserPreset (const juce::String& name)
    {
        auto file = getUserPresetDirectory().getChildFile (name + ".sn1preset");
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
                           float bodyLevel,
                           float noiseLevel, float noiseColor, float noiseDecay,
                           float buzzQ, float snapLevel,
                           float drive, float tone, float output)
    {
        FactoryPreset p;
        p.name = name;
        p.values["tune"]       = tune;
        p.values["pitchAmt"]   = pitchAmt;
        p.values["pitchTime"]  = pitchTime;
        p.values["bodyDecay"]  = bodyDecay;
        p.values["bodyLevel"]  = bodyLevel;
        p.values["noiseLevel"] = noiseLevel;
        p.values["noiseColor"] = noiseColor;
        p.values["noiseDecay"] = noiseDecay;
        p.values["buzzQ"]      = buzzQ;
        p.values["snapLevel"]  = snapLevel;
        p.values["drive"]      = drive;
        p.values["tone"]       = tone;
        p.values["output"]     = output;
        factoryPresets.add (std::move (p));
    }

    void initFactoryPresets()
    {
        factoryPresets.clear();

        //                  name              tune  pAmt  pTime decay  body   nLvl  nClr   nDec   buzz  snap  drive tone   out
        addFactoryPreset ("Init",             200.f,  8.f, 12.f, 110.f, 0.65f, 0.85f, 4500.f, 220.f, 1.6f, 0.55f, 0.18f,  0.0f,  0.0f);
        addFactoryPreset ("Pop Crack",        220.f,  6.f, 10.f,  95.f, 0.55f, 0.85f, 5200.f, 180.f, 2.4f, 0.65f, 0.18f,  0.20f, 0.0f);
        addFactoryPreset ("Rock Smack",       195.f, 10.f, 14.f, 130.f, 0.70f, 0.78f, 3800.f, 240.f, 1.8f, 0.60f, 0.30f,  0.05f, 1.5f);
        addFactoryPreset ("Vintage 909",      230.f,  4.f,  8.f,  85.f, 0.45f, 0.92f, 6000.f, 200.f, 2.2f, 0.70f, 0.15f,  0.30f, 0.0f);
        addFactoryPreset ("808 Snap",         180.f, 12.f, 18.f,  70.f, 0.50f, 0.90f, 7000.f, 130.f, 3.2f, 0.80f, 0.10f,  0.35f,-0.5f);
        addFactoryPreset ("Tight Funk",       240.f,  5.f,  9.f,  85.f, 0.65f, 0.75f, 5500.f, 140.f, 1.4f, 0.55f, 0.20f,  0.15f, 0.5f);
        addFactoryPreset ("Trap Sizzle",      210.f, 10.f, 14.f, 100.f, 0.45f, 0.95f, 8200.f, 380.f, 4.0f, 0.55f, 0.20f,  0.40f, 1.0f);
        addFactoryPreset ("Lo-Fi Tape",       175.f,  8.f, 16.f, 140.f, 0.60f, 0.55f, 2400.f, 260.f, 1.2f, 0.30f, 0.55f, -0.40f, 0.0f);
        addFactoryPreset ("Hip-Hop Wide",     185.f,  6.f, 12.f, 160.f, 0.75f, 0.70f, 3200.f, 320.f, 1.6f, 0.45f, 0.30f, -0.10f, 1.0f);
        addFactoryPreset ("Clappy House",     205.f,  4.f, 10.f,  75.f, 0.40f, 0.95f, 4200.f, 200.f, 1.0f, 0.85f, 0.22f,  0.10f, 0.5f);
        addFactoryPreset ("Brushed Soft",     200.f,  3.f, 14.f, 200.f, 0.45f, 0.50f, 3500.f, 360.f, 0.9f, 0.20f, 0.05f, -0.20f,-1.0f);
        addFactoryPreset ("Industrial",       260.f, 14.f, 12.f, 110.f, 0.55f, 0.85f, 2800.f, 280.f, 2.8f, 0.65f, 0.65f,  0.30f, 1.5f);
        addFactoryPreset ("Cinematic Boom",   140.f, 18.f, 24.f, 280.f, 0.85f, 0.70f, 2200.f, 520.f, 1.4f, 0.40f, 0.40f, -0.30f, 1.5f);
        addFactoryPreset ("Dub Crack",        190.f,  8.f, 12.f, 110.f, 0.55f, 0.80f, 4000.f, 600.f, 5.0f, 0.50f, 0.25f,  0.0f,  0.5f);
        addFactoryPreset ("Ghost",            210.f,  4.f, 10.f,  60.f, 0.35f, 0.45f, 5000.f, 110.f, 1.6f, 0.25f, 0.10f,  0.05f,-3.0f);
        addFactoryPreset ("Big Stadium",      170.f, 12.f, 18.f, 200.f, 0.80f, 0.85f, 3500.f, 420.f, 1.8f, 0.60f, 0.35f,  0.10f, 2.0f);
    }

    void rescanUserPresets()
    {
        userPresetNames.clear();
        auto dir = getUserPresetDirectory();
        if (! dir.exists()) return;

        for (auto file : dir.findChildFiles (juce::File::findFiles, false, "*.sn1preset"))
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
