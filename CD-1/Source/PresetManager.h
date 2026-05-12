/*
  ==============================================================================

    PresetManager.h
    CD-1 factory + user preset system. Factory presets are defined in code
    and shipped with every build; user presets are stored as XML files in
    the user data directory and survive across sessions.

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
        juce::StringArray ids;

        // per-drum
        const char* drumPrefix[] = { "boom", "hit", "crack", "sub" };
        for (auto* d : drumPrefix)
        {
            ids.add (juce::String (d) + "Tune");
            ids.add (juce::String (d) + "Decay");
            ids.add (juce::String (d) + "Level");
            ids.add (juce::String (d) + "Pan");
        }

        // master macros
        ids.addArray ({ "depth", "impact", "air", "drive",
                        "width", "size", "tone", "output",
                        "rvSize", "rvDamp", "rvLow" });
        return ids;
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
                       .getChildFile ("CD-1").getChildFile ("Presets");
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

        auto file = getUserPresetDirectory().getChildFile (name + ".cd1preset");
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

        auto file = getUserPresetDirectory().getChildFile (trimmed + ".cd1preset");
        auto state = apvts.copyState();
        if (auto xml = state.createXml())
            xml->writeTo (file, {});

        rescanUserPresets();
        setCurrentPresetName (trimmed, false);
        if (onPresetListChanged) onPresetListChanged();
    }

    void deleteUserPreset (const juce::String& name)
    {
        auto file = getUserPresetDirectory().getChildFile (name + ".cd1preset");
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

    // Helper: build a preset entry. The order matches the parameter IDs:
    //   per drum (4): tune (st), decay (×), level (0..1.5), pan (-1..1)
    //   master:        depth, impact, air, drive, width, size, tone, output(dB),
    //                  rvSize, rvDamp, rvLow(Hz)
    void addFactoryPreset (const juce::String& name,
                           // boom
                           float bT, float bD, float bL, float bP,
                           // hit
                           float hT, float hD, float hL, float hP,
                           // crack
                           float cT, float cD, float cL, float cP,
                           // sub
                           float sT, float sD, float sL, float sP,
                           // master macros
                           float depth, float impact, float air, float drive,
                           float width, float size, float tone, float outDb,
                           // reverb internals
                           float rvSize, float rvDamp, float rvLow)
    {
        FactoryPreset p;
        p.name = name;

        p.values["boomTune"]  = bT;  p.values["boomDecay"]  = bD;
        p.values["boomLevel"] = bL;  p.values["boomPan"]    = bP;

        p.values["hitTune"]   = hT;  p.values["hitDecay"]   = hD;
        p.values["hitLevel"]  = hL;  p.values["hitPan"]     = hP;

        p.values["crackTune"] = cT;  p.values["crackDecay"] = cD;
        p.values["crackLevel"]= cL;  p.values["crackPan"]   = cP;

        p.values["subTune"]   = sT;  p.values["subDecay"]   = sD;
        p.values["subLevel"]  = sL;  p.values["subPan"]     = sP;

        p.values["depth"]  = depth;  p.values["impact"]   = impact;
        p.values["air"]    = air;    p.values["drive"]    = drive;
        p.values["width"]  = width;  p.values["size"]     = size;
        p.values["tone"]   = tone;   p.values["output"]   = outDb;

        p.values["rvSize"] = rvSize; p.values["rvDamp"]   = rvDamp;
        p.values["rvLow"]  = rvLow;

        factoryPresets.add (std::move (p));
    }

    void initFactoryPresets()
    {
        factoryPresets.clear();

        //                  name                   |--- BOOM ---|  |--- HIT ---|  |--- CRACK --|  |--- SUB ---|     depth imp   air   drv   wid   sz    tone   out    rvSz  rvDmp rvLow
        addFactoryPreset ("Init",                   0,1.0f,1.0f,-0.10f,  0,1.0f,1.0f,0.10f,  0,1.0f,1.0f,0.30f,  0,1.0f,1.0f,-0.30f,
                                                    0.50f,0.50f, 0.0f, 0.20f, 0.70f, 0.40f, 0.0f,  0.0f,
                                                    0.45f,0.50f, 90.0f);

        addFactoryPreset ("Trailer Boom",          -2,1.5f,1.10f,-0.05f,  0,1.0f,0.85f,0.10f,  0,1.0f,0.55f,0.30f, -2,1.6f,1.10f,-0.20f,
                                                    0.85f,0.65f, 0.05f, 0.30f, 0.85f, 0.70f, 0.10f,  1.0f,
                                                    0.70f,0.55f, 80.0f);

        addFactoryPreset ("Hans Hit",              -1,1.2f,1.05f,-0.15f,  0,1.0f,0.95f, 0.15f, 0,1.1f,0.65f,0.20f, -1,1.4f,1.00f,-0.10f,
                                                    0.75f,0.70f, 0.10f, 0.35f, 0.80f, 0.55f, 0.10f,  0.5f,
                                                    0.55f,0.45f, 90.0f);

        addFactoryPreset ("Inception Drop",         -3,2.0f,1.20f, 0.00f,  0,1.5f,0.80f, 0.10f, 0,1.0f,0.40f,0.30f, -3,2.0f,1.10f, 0.00f,
                                                    0.95f,0.55f,-0.05f, 0.40f, 0.90f, 0.85f,-0.05f,  1.5f,
                                                    0.85f,0.70f, 70.0f);

        addFactoryPreset ("Epic Tom Wall",           0,1.4f,0.85f,-0.30f,  2,1.3f,1.10f,-0.20f, 4,1.2f,0.50f,0.25f,  0,1.0f,0.70f, 0.30f,
                                                    0.55f,0.75f, 0.15f, 0.30f, 0.95f, 0.65f, 0.05f,  0.5f,
                                                    0.65f,0.45f, 90.0f);

        addFactoryPreset ("Subterranean",            0,1.8f,1.10f, 0.00f,  0,1.0f,0.50f, 0.0f,  0,1.0f,0.20f,0.10f, -2,2.5f,1.20f, 0.00f,
                                                    1.0f, 0.30f,-0.10f, 0.20f, 0.70f, 0.60f,-0.20f,  1.5f,
                                                    0.55f,0.55f, 60.0f);

        addFactoryPreset ("Battle Drums",            0,1.0f,1.00f,-0.20f,  0,0.9f,1.10f, 0.20f, 0,0.8f,0.85f,0.30f,  0,0.9f,0.65f,-0.10f,
                                                    0.60f,0.85f, 0.20f, 0.45f, 0.85f, 0.40f, 0.20f,  1.0f,
                                                    0.40f,0.40f,100.0f);

        addFactoryPreset ("War Snare",              -1,0.7f,0.50f,-0.05f,  1,0.9f,0.85f, 0.10f, 2,1.3f,1.10f,0.00f, -1,0.8f,0.45f,-0.10f,
                                                    0.45f,0.95f, 0.30f, 0.40f, 0.90f, 0.55f, 0.25f,  0.5f,
                                                    0.50f,0.40f, 110.0f);

        addFactoryPreset ("Distant Thunder",        -2,1.7f,1.00f,-0.10f,  0,1.5f,0.65f, 0.0f,  0,1.0f,0.30f,0.30f, -3,2.2f,1.10f, 0.10f,
                                                    0.85f,0.40f,-0.10f, 0.55f, 0.95f, 0.95f,-0.20f,  1.5f,
                                                    0.95f,0.75f, 60.0f);

        addFactoryPreset ("Hybrid Score",            0,1.2f,1.00f,-0.10f,  1,1.1f,1.05f, 0.10f, 3,1.0f,0.85f,0.25f,  0,1.3f,0.85f,-0.20f,
                                                    0.70f,0.70f, 0.10f, 0.30f, 0.85f, 0.55f, 0.10f,  1.0f,
                                                    0.60f,0.50f, 90.0f);

        addFactoryPreset ("Stadium Stomp",          -1,1.0f,1.10f, 0.00f, -1,0.8f,1.00f, 0.0f,  0,0.9f,0.55f,0.20f, -2,1.0f,0.80f, 0.0f,
                                                    0.65f,0.85f, 0.15f, 0.55f, 0.50f, 0.30f, 0.05f,  1.0f,
                                                    0.30f,0.30f, 110.0f);

        addFactoryPreset ("Dry Brutal",              0,0.8f,1.00f, 0.0f,  0,0.7f,1.00f, 0.0f,  2,0.7f,0.90f,0.0f,  0,0.7f,0.70f, 0.0f,
                                                    0.45f,0.90f, 0.20f, 0.60f, 0.30f, 0.05f, 0.10f,  0.0f,
                                                    0.10f,0.30f, 130.0f);

        addFactoryPreset ("Soft Underscore",        -2,1.4f,0.85f,-0.20f, -1,1.3f,0.70f, 0.20f, 0,1.5f,0.30f,0.30f, -2,1.5f,0.85f,-0.10f,
                                                    0.55f,0.30f,-0.05f, 0.10f, 0.90f, 0.75f,-0.10f, -1.5f,
                                                    0.75f,0.65f, 80.0f);

        addFactoryPreset ("Tribal Heart",            0,1.1f,1.00f,-0.15f,  2,1.0f,1.00f, 0.20f, 4,0.9f,0.65f,0.30f,  0,1.0f,0.65f,-0.05f,
                                                    0.55f,0.75f, 0.20f, 0.30f, 0.80f, 0.45f, 0.05f,  0.5f,
                                                    0.45f,0.40f,100.0f);

        addFactoryPreset ("Cosmic Pulse",           -3,2.0f,0.95f, 0.0f,  0,1.5f,0.55f, 0.0f, -2,1.5f,0.30f,0.0f, -4,2.5f,1.20f, 0.0f,
                                                    1.0f,0.30f,-0.20f, 0.20f, 0.95f, 0.95f,-0.30f,  1.0f,
                                                    0.95f,0.80f, 50.0f);
    }

    void rescanUserPresets()
    {
        userPresetNames.clear();
        auto dir = getUserPresetDirectory();
        if (! dir.exists()) return;

        for (auto file : dir.findChildFiles (juce::File::findFiles, false, "*.cd1preset"))
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
