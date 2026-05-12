/*
  ==============================================================================

    PresetManager.h
    HH-1 factory + user preset system. Factory presets are defined in code;
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
        return { "tune", "metal", "harmonics", "hpCut", "bpCut", "shimmerQ",
                 "decay", "hold", "noise", "color",
                 "drive", "tone", "width", "output" };
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
                       .getChildFile ("HH-1").getChildFile ("Presets");
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

        auto file = getUserPresetDirectory().getChildFile (name + ".hh1preset");
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

        auto file = getUserPresetDirectory().getChildFile (trimmed + ".hh1preset");
        auto state = apvts.copyState();
        if (auto xml = state.createXml())
            xml->writeTo (file, {});

        rescanUserPresets();
        setCurrentPresetName (trimmed, false);
        if (onPresetListChanged) onPresetListChanged();
    }

    void deleteUserPreset (const juce::String& name)
    {
        auto file = getUserPresetDirectory().getChildFile (name + ".hh1preset");
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
                           float tune, float metal, float harmonics,
                           float hpCut, float bpCut, float shimmerQ,
                           float decay, float hold, float noise, float color,
                           float drive, float tone, float width, float output)
    {
        FactoryPreset p;
        p.name = name;
        p.values["tune"]      = tune;
        p.values["metal"]     = metal;
        p.values["harmonics"] = harmonics;
        p.values["hpCut"]     = hpCut;
        p.values["bpCut"]     = bpCut;
        p.values["shimmerQ"]  = shimmerQ;
        p.values["decay"]     = decay;
        p.values["hold"]      = hold;
        p.values["noise"]     = noise;
        p.values["color"]     = color;
        p.values["drive"]     = drive;
        p.values["tone"]      = tone;
        p.values["width"]     = width;
        p.values["output"]    = output;
        factoryPresets.add (std::move (p));
    }

    void initFactoryPresets()
    {
        factoryPresets.clear();

        //                  name              tune  metal harm   hpCut   bpCut shimQ  decay hold noise color  drive  tone  width   out
        addFactoryPreset ("Init",             800.f, 0.85f, 1.00f, 6500.f, 9000.f, 4.0f,  90.f, 0.00f, 0.55f, 7000.f, 0.20f,  0.00f, 0.55f, 0.0f);
        addFactoryPreset ("Closed Tight",     820.f, 0.92f, 1.00f, 7500.f, 9500.f, 5.5f,  55.f, 0.00f, 0.40f, 8200.f, 0.18f,  0.10f, 0.30f, 0.0f);
        addFactoryPreset ("Closed Crisp",     880.f, 0.88f, 1.00f, 8000.f,10500.f, 6.0f,  70.f, 0.00f, 0.35f, 9500.f, 0.20f,  0.25f, 0.40f, 0.5f);
        addFactoryPreset ("Open Smooth",      780.f, 0.85f, 0.95f, 6000.f, 8500.f, 4.0f, 180.f, 0.65f, 0.55f, 7000.f, 0.18f,  0.05f, 0.65f, 0.0f);
        addFactoryPreset ("Open Wash",        740.f, 0.78f, 1.00f, 5500.f, 8000.f, 3.5f, 240.f, 0.85f, 0.65f, 6500.f, 0.15f, -0.05f, 0.80f, 0.5f);
        addFactoryPreset ("Vintage 808",      720.f, 0.55f, 0.85f, 5000.f, 7800.f, 2.5f, 110.f, 0.10f, 0.75f, 6200.f, 0.30f, -0.20f, 0.45f, 1.0f);
        addFactoryPreset ("Acoustic Soft",    680.f, 0.50f, 0.90f, 4200.f, 7000.f, 2.0f, 140.f, 0.30f, 0.85f, 5400.f, 0.10f, -0.30f, 0.55f,-1.0f);
        addFactoryPreset ("Trap Sizzle",      900.f, 0.92f, 1.00f, 7000.f,11000.f, 7.0f, 130.f, 0.45f, 0.55f, 9200.f, 0.22f,  0.30f, 0.75f, 1.0f);
        addFactoryPreset ("Lo-Fi Worn",       640.f, 0.70f, 0.80f, 3800.f, 6500.f, 2.2f, 100.f, 0.10f, 0.70f, 4800.f, 0.45f, -0.50f, 0.35f,-1.0f);
        addFactoryPreset ("Industrial",       960.f, 0.95f, 1.00f, 7800.f, 9800.f, 6.5f,  85.f, 0.00f, 0.50f, 8800.f, 0.55f,  0.40f, 0.55f, 1.5f);
        addFactoryPreset ("Funk Shake",       820.f, 0.82f, 1.00f, 7200.f, 9800.f, 5.5f,  45.f, 0.00f, 0.42f, 8000.f, 0.20f,  0.20f, 0.35f, 0.0f);
        addFactoryPreset ("Studio Brilliant", 880.f, 0.88f, 1.00f, 7000.f,12000.f, 7.5f, 120.f, 0.20f, 0.50f,10500.f, 0.15f,  0.50f, 0.65f, 1.0f);
        addFactoryPreset ("Cymbal Spread",    760.f, 0.80f, 1.00f, 5800.f, 9500.f, 4.5f, 220.f, 0.75f, 0.62f, 7800.f, 0.18f,  0.10f, 0.95f, 0.5f);
        addFactoryPreset ("Pedal Chick",      900.f, 0.90f, 0.95f, 8500.f,10500.f, 5.0f,  35.f, 0.00f, 0.30f, 9500.f, 0.20f,  0.20f, 0.20f, 0.0f);
        addFactoryPreset ("Flanged Metal",    820.f, 0.95f, 1.00f, 6500.f, 9200.f, 8.0f, 160.f, 0.40f, 0.45f, 8000.f, 0.30f,  0.25f, 0.85f, 1.0f);
        addFactoryPreset ("Disco Pump",       820.f, 0.85f, 0.95f, 6800.f, 9000.f, 4.5f,  90.f, 0.00f, 0.55f, 7400.f, 0.40f,  0.05f, 0.55f, 1.5f);
    }

    void rescanUserPresets()
    {
        userPresetNames.clear();
        auto dir = getUserPresetDirectory();
        if (! dir.exists()) return;

        for (auto file : dir.findChildFiles (juce::File::findFiles, false, "*.hh1preset"))
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
