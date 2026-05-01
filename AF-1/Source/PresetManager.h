/*
  ==============================================================================

    PresetManager.h
    Robust, file-based preset system for AF-1.
      • Bundled factory presets (read-only, loaded from memory)
      • User presets (XML files in OS-specific user data dir)
      • Save / Save As / Delete with collision protection
      • Next / Previous navigation
      • Notifies a listener on changes

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PresetManager
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void presetListChanged() = 0;
        virtual void currentPresetChanged (const juce::String& name) = 0;
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& v) : apvts (v)
    {
        ensureUserDirectory();
        for (auto& p : makeFactoryPresets())
            factoryPresets.push_back (std::move (p));
        rebuildList();
        // Note: we never auto-apply a preset on construction to preserve the
        // host-restored state. Use loadByIndex() / Init button to load one.
    }

    void addListener    (Listener* l) { listeners.add    (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    juce::StringArray getAllPresetNames() const
    {
        juce::StringArray out;
        for (auto& p : factoryPresets) out.add ("[F]  " + p.name);
        for (auto& f : userFiles)      out.add (f.getFileNameWithoutExtension());
        return out;
    }

    int getNumFactory() const { return (int) factoryPresets.size(); }
    int getNumUser()    const { return userFiles.size(); }

    juce::String getCurrentName() const { return currentName; }
    int          getCurrentIndex() const { return currentIndex; }

    bool loadByIndex (int idx)
    {
        if (idx < 0) return false;

        if (idx < (int) factoryPresets.size())
        {
            applyXml (factoryPresets[(size_t) idx].xml.get());
            currentName  = factoryPresets[(size_t) idx].name;
            currentIndex = idx;
            currentIsFactory = true;
            notifyCurrent();
            return true;
        }

        const int userIdx = idx - (int) factoryPresets.size();
        if (userIdx < userFiles.size())
        {
            const juce::File f = userFiles[userIdx];
            if (auto xml = juce::XmlDocument::parse (f); xml != nullptr)
            {
                applyXml (xml.get());
                currentName  = f.getFileNameWithoutExtension();
                currentIndex = idx;
                currentIsFactory = false;
                notifyCurrent();
                return true;
            }
        }
        return false;
    }

    bool loadNext()     { return loadByIndex (juce::jlimit (0, (int) getAllPresetNames().size() - 1, currentIndex + 1)); }
    bool loadPrevious() { return loadByIndex (juce::jlimit (0, (int) getAllPresetNames().size() - 1, currentIndex - 1)); }

    // Save the current state to a user file.  Returns the saved file (or invalid file on failure).
    juce::File saveAs (const juce::String& nameRaw, bool overwrite)
    {
        const juce::String name = juce::File::createLegalFileName (nameRaw).trim();
        if (name.isEmpty()) return {};

        juce::File target = getUserDir().getChildFile (name + ".afpreset");
        if (target.existsAsFile() && ! overwrite) return {};

        if (auto state = apvts.copyState(); state.isValid())
        {
            std::unique_ptr<juce::XmlElement> xml (state.createXml());
            if (xml != nullptr && xml->writeTo (target))
            {
                rebuildList();
                // select the just-written preset
                const int idx = indexForUserFile (target);
                if (idx >= 0) { currentIndex = idx; currentName = name; currentIsFactory = false; }
                notifyList();
                notifyCurrent();
                return target;
            }
        }
        return {};
    }

    bool saveCurrent()
    {
        if (currentIsFactory || currentName.isEmpty())
            return false;
        return saveAs (currentName, true) != juce::File();
    }

    bool deleteUserPreset (int idx)
    {
        if (idx < (int) factoryPresets.size()) return false;
        const int userIdx = idx - (int) factoryPresets.size();
        if (userIdx >= userFiles.size()) return false;

        if (userFiles[userIdx].deleteFile())
        {
            rebuildList();
            if (currentIndex == idx)
            {
                currentIndex = -1;
                currentName  = {};
            }
            notifyList();
            return true;
        }
        return false;
    }

    juce::File getUserDir() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("AF-1").getChildFile ("Presets");
    }

    void rebuildList()
    {
        userFiles.clear();
        const auto dir = getUserDir();
        if (dir.isDirectory())
        {
            for (auto& f : juce::RangedDirectoryIterator (dir, false, "*.afpreset"))
                userFiles.add (f.getFile());

            std::sort (userFiles.begin(), userFiles.end(),
                       [](const juce::File& a, const juce::File& b)
                       {
                           return a.getFileNameWithoutExtension()
                                   .compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
                       });
        }
    }

    void noteCurrentPresetByName (const juce::String& name)
    {
        // Try factory
        for (size_t i = 0; i < factoryPresets.size(); ++i)
            if (factoryPresets[i].name == name) { currentIndex = (int) i; currentName = name; currentIsFactory = true; notifyCurrent(); return; }

        for (int i = 0; i < userFiles.size(); ++i)
            if (userFiles[i].getFileNameWithoutExtension() == name)
            { currentIndex = (int) factoryPresets.size() + i; currentName = name; currentIsFactory = false; notifyCurrent(); return; }
    }

private:
    struct FactoryPreset
    {
        juce::String name;
        std::unique_ptr<juce::XmlElement> xml;
    };

    void notifyList()    { listeners.call ([](Listener& l) { l.presetListChanged(); }); }
    void notifyCurrent() { listeners.call ([this](Listener& l) { l.currentPresetChanged (currentName); }); }

    void ensureUserDirectory()
    {
        const auto dir = getUserDir();
        if (! dir.isDirectory())
            dir.createDirectory();
    }

    int indexForUserFile (const juce::File& f) const
    {
        for (int i = 0; i < userFiles.size(); ++i)
            if (userFiles[i] == f) return (int) factoryPresets.size() + i;
        return -1;
    }

    void applyXml (juce::XmlElement* xml)
    {
        if (xml == nullptr) return;
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    // ──────────────────────────────────────────────────────────────────────────
    // Factory preset definitions (built in-code so users always have starting points)
    static std::unique_ptr<juce::XmlElement> makePreset (
        float cutoff, float resonance, float drive,
        int filterType, int slope,
        float lfoRate, float lfoDepth, int lfoShape,
        float envAmount, float envAttack, float envRelease,
        float mix, float output)
    {
        auto state = juce::ValueTree ("AF1");

        auto setP = [&](const juce::String& id, float v)
        {
            juce::ValueTree p ("PARAM");
            p.setProperty ("id",    id, nullptr);
            p.setProperty ("value", v,  nullptr);
            state.appendChild (p, nullptr);
        };

        setP ("cutoff",     cutoff);
        setP ("resonance",  resonance);
        setP ("drive",      drive);
        setP ("filterType", (float) filterType);
        setP ("slope",      (float) slope);
        setP ("lfoRate",    lfoRate);
        setP ("lfoDepth",   lfoDepth);
        setP ("lfoShape",   (float) lfoShape);
        setP ("envAmount",  envAmount);
        setP ("envAttack",  envAttack);
        setP ("envRelease", envRelease);
        setP ("mix",        mix);
        setP ("output",     output);

        return std::unique_ptr<juce::XmlElement> (state.createXml().release());
    }

    static std::vector<FactoryPreset> makeFactoryPresets()
    {
        std::vector<FactoryPreset> out;
        auto add = [&] (const juce::String& name, std::unique_ptr<juce::XmlElement> x)
        { out.push_back ({ name, std::move (x) }); };

        // 0 – clean default
        add ("Init",
             makePreset (1200.0f, 0.30f, 0.0f, 0, 1,  1.0f, 0.0f, 0,  0.0f, 8.0f, 200.0f, 1.0f, 0.0f));

        // 1 – classic envelope wah
        add ("Envelope Wah",
             makePreset (450.0f, 0.65f, 6.0f, 0, 1,  1.0f, 0.0f, 0,  0.85f, 4.0f, 180.0f, 1.0f, 0.0f));

        // 2 – slow LFO sweep
        add ("Liquid Sweep",
             makePreset (700.0f, 0.55f, 3.0f, 0, 1,  0.18f, 0.85f, 0,  0.0f, 8.0f, 250.0f, 1.0f, 0.0f));

        // 3 – tempo-ish bandpass pulse
        add ("BP Pulse",
             makePreset (900.0f, 0.55f, 4.0f, 1, 0,  4.0f, 0.95f, 4,  0.0f, 8.0f, 200.0f, 1.0f, 0.0f));

        // 4 – high-pass shimmer
        add ("Shimmer HP",
             makePreset (3500.0f, 0.30f, 1.5f, 2, 1,  0.5f, 0.30f, 0,  0.0f, 5.0f, 150.0f, 1.0f, 0.0f));

        // 5 – S&H robot
        add ("Robot S&H",
             makePreset (1200.0f, 0.70f, 8.0f, 0, 1,  6.0f, 0.95f, 5,  0.0f, 8.0f, 200.0f, 1.0f, 0.0f));

        // 6 – warm vintage drum filter
        add ("Vintage Drums",
             makePreset (1400.0f, 0.45f, 9.0f, 0, 0,  0.0f, 0.0f,  0,  0.65f, 3.0f, 120.0f, 1.0f, 0.0f));

        // 7 – notch space
        add ("Notch Space",
             makePreset (1500.0f, 0.85f, 0.0f, 3, 1,  0.10f, 0.50f, 0,  0.0f, 8.0f, 250.0f, 1.0f, 0.0f));

        // 8 – velvet pad
        add ("Velvet Pad",
             makePreset (2400.0f, 0.20f, 2.0f, 0, 0,  0.07f, 0.20f, 1,  0.0f, 50.0f, 600.0f, 1.0f, 0.0f));

        // 9 – disco bass
        add ("Disco Bass",
             makePreset (350.0f, 0.65f, 7.0f, 0, 1,  2.0f, 0.55f, 1,  0.40f, 4.0f, 130.0f, 1.0f, 0.0f));

        return out;
    }

    juce::AudioProcessorValueTreeState& apvts;

    std::vector<FactoryPreset> factoryPresets;
    juce::Array<juce::File>    userFiles;

    juce::ListenerList<Listener> listeners;

    int          currentIndex      = -1;
    juce::String currentName;
    bool         currentIsFactory  = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
