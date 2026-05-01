/*
  ==============================================================================

    PresetManager.h
    Robust file-based preset system for DL-1.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PresetManager : private juce::ValueTree::Listener
{
public:
    static constexpr const char* presetExtension = ".dl1preset";

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void presetListChanged() = 0;
        virtual void currentPresetChanged(const juce::String& name) = 0;
    };

    PresetManager(juce::AudioProcessorValueTreeState& vts)
        : apvts(vts)
    {
        ensureFactoryPresetsExist();
        rescanPresets();
        apvts.state.addListener(this);
    }

    ~PresetManager() override
    {
        apvts.state.removeListener(this);
    }

    void addListener(Listener* l)    { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    juce::File getPresetsDirectory() const
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("DL-1").getChildFile("Presets");
        if (! dir.exists()) dir.createDirectory();
        return dir;
    }

    const juce::StringArray& getPresetNames() const noexcept { return presetNames; }
    const juce::String&      getCurrentPresetName() const noexcept { return currentPresetName; }
    bool                     isCurrentPresetDirty() const noexcept { return dirty; }

    void rescanPresets()
    {
        presetNames.clear();
        auto dir = getPresetsDirectory();
        juce::Array<juce::File> files;
        dir.findChildFiles(files, juce::File::findFiles, false, juce::String("*") + presetExtension);

        for (auto& f : files)
            presetNames.add(f.getFileNameWithoutExtension());

        presetNames.sortNatural();
        listeners.call([](Listener& l) { l.presetListChanged(); });
    }

    bool loadPreset(const juce::String& name)
    {
        auto file = getPresetsDirectory().getChildFile(name + presetExtension);
        if (! file.existsAsFile()) return false;

        auto xml = juce::XmlDocument::parse(file);
        if (xml == nullptr) return false;

        auto tree = juce::ValueTree::fromXml(*xml);
        if (! tree.isValid()) return false;

        // Replace state
        loadingFromPreset = true;
        apvts.replaceState(tree);
        loadingFromPreset = false;

        currentPresetName = name;
        dirty = false;
        listeners.call([&](Listener& l) { l.currentPresetChanged(currentPresetName); });
        return true;
    }

    bool savePreset(const juce::String& name)
    {
        auto safeName = juce::File::createLegalFileName(name).trim();
        if (safeName.isEmpty()) return false;

        auto file = getPresetsDirectory().getChildFile(safeName + presetExtension);
        auto xml  = apvts.copyState().createXml();
        if (xml == nullptr) return false;

        bool ok = xml->writeTo(file);
        if (ok)
        {
            currentPresetName = safeName;
            dirty = false;
            rescanPresets();
            listeners.call([&](Listener& l) { l.currentPresetChanged(currentPresetName); });
        }
        return ok;
    }

    bool deletePreset(const juce::String& name)
    {
        auto file = getPresetsDirectory().getChildFile(name + presetExtension);
        if (! file.existsAsFile()) return false;

        bool ok = file.deleteFile();
        if (ok)
        {
            if (currentPresetName == name)
            {
                currentPresetName = "Init";
                dirty = true;
                listeners.call([&](Listener& l) { l.currentPresetChanged(currentPresetName); });
            }
            rescanPresets();
        }
        return ok;
    }

    int getCurrentIndex() const
    {
        return presetNames.indexOf(currentPresetName);
    }

    bool loadByIndex(int idx)
    {
        if (idx < 0 || idx >= presetNames.size()) return false;
        return loadPreset(presetNames[idx]);
    }

    bool nextPreset()
    {
        if (presetNames.isEmpty()) return false;
        int idx = juce::jmax(0, getCurrentIndex());
        idx = (idx + 1) % presetNames.size();
        return loadByIndex(idx);
    }

    bool previousPreset()
    {
        if (presetNames.isEmpty()) return false;
        int idx = getCurrentIndex();
        if (idx < 0) idx = 0;
        idx = (idx - 1 + presetNames.size()) % presetNames.size();
        return loadByIndex(idx);
    }

    void setCurrentNameAfterStateRestore(const juce::String& name)
    {
        currentPresetName = name;
        dirty = false;
        listeners.call([&](Listener& l) { l.currentPresetChanged(currentPresetName); });
    }

private:
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override
    {
        if (! loadingFromPreset && ! dirty)
        {
            dirty = true;
            listeners.call([&](Listener& l) { l.currentPresetChanged(currentPresetName); });
        }
    }

    void valueTreeChildAdded   (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    void ensureFactoryPresetsExist()
    {
        auto dir = getPresetsDirectory();

        struct Factory { const char* name; std::map<juce::String, float> values; };

        const std::vector<Factory> factories = {
            { "Init",
              { {"timeL",350.f},{"timeR",525.f},{"linkTimes",0.f},{"sync",0.f},
                {"divisionL",6.f},{"divisionR",6.f},
                {"feedback",0.40f},{"crossfeed",0.0f},
                {"highCut",10000.f},{"lowCut",80.f},
                {"modRate",0.30f},{"modDepth",0.10f},
                {"drive",0.10f},{"width",1.0f},{"ducking",0.0f},
                {"mix",0.35f},{"inGain",0.f},{"outGain",0.f} } },

            { "Velvet Quarter",
              { {"timeL",375.f},{"timeR",375.f},{"linkTimes",1.f},{"sync",0.f},
                {"divisionL",8.f},{"divisionR",8.f},
                {"feedback",0.55f},{"crossfeed",0.0f},
                {"highCut",6500.f},{"lowCut",140.f},
                {"modRate",0.45f},{"modDepth",0.18f},
                {"drive",0.22f},{"width",1.10f},{"ducking",0.10f},
                {"mix",0.40f},{"inGain",0.f},{"outGain",0.f} } },

            { "Pingpong Dream",
              { {"timeL",250.f},{"timeR",375.f},{"linkTimes",0.f},{"sync",0.f},
                {"divisionL",6.f},{"divisionR",8.f},
                {"feedback",0.50f},{"crossfeed",1.0f},
                {"highCut",8000.f},{"lowCut",120.f},
                {"modRate",0.20f},{"modDepth",0.08f},
                {"drive",0.10f},{"width",1.40f},{"ducking",0.05f},
                {"mix",0.45f},{"inGain",0.f},{"outGain",0.f} } },

            { "Tape Saturated",
              { {"timeL",420.f},{"timeR",640.f},{"linkTimes",0.f},{"sync",0.f},
                {"divisionL",8.f},{"divisionR",10.f},
                {"feedback",0.62f},{"crossfeed",0.30f},
                {"highCut",4500.f},{"lowCut",180.f},
                {"modRate",0.55f},{"modDepth",0.30f},
                {"drive",0.55f},{"width",1.20f},{"ducking",0.15f},
                {"mix",0.45f},{"inGain",0.f},{"outGain",0.f} } },

            { "Ambient Hall",
              { {"timeL",780.f},{"timeR",1100.f},{"linkTimes",0.f},{"sync",0.f},
                {"divisionL",10.f},{"divisionR",11.f},
                {"feedback",0.72f},{"crossfeed",0.50f},
                {"highCut",5500.f},{"lowCut",200.f},
                {"modRate",0.18f},{"modDepth",0.45f},
                {"drive",0.12f},{"width",1.50f},{"ducking",0.25f},
                {"mix",0.55f},{"inGain",0.f},{"outGain",0.f} } },

            { "Slap-Back",
              { {"timeL",95.f},{"timeR",115.f},{"linkTimes",0.f},{"sync",0.f},
                {"divisionL",2.f},{"divisionR",2.f},
                {"feedback",0.05f},{"crossfeed",0.0f},
                {"highCut",7000.f},{"lowCut",100.f},
                {"modRate",0.10f},{"modDepth",0.04f},
                {"drive",0.18f},{"width",1.0f},{"ducking",0.0f},
                {"mix",0.30f},{"inGain",0.f},{"outGain",0.f} } },

            { "Dotted Eighths",
              { {"timeL",375.f},{"timeR",375.f},{"linkTimes",0.f},{"sync",1.f},
                {"divisionL",6.f},{"divisionR",8.f},
                {"feedback",0.48f},{"crossfeed",0.30f},
                {"highCut",9000.f},{"lowCut",110.f},
                {"modRate",0.30f},{"modDepth",0.10f},
                {"drive",0.10f},{"width",1.20f},{"ducking",0.10f},
                {"mix",0.40f},{"inGain",0.f},{"outGain",0.f} } },

            { "Cosmic Drift",
              { {"timeL",550.f},{"timeR",730.f},{"linkTimes",0.f},{"sync",0.f},
                {"divisionL",8.f},{"divisionR",9.f},
                {"feedback",0.78f},{"crossfeed",0.65f},
                {"highCut",4000.f},{"lowCut",250.f},
                {"modRate",0.80f},{"modDepth",0.55f},
                {"drive",0.40f},{"width",1.60f},{"ducking",0.20f},
                {"mix",0.60f},{"inGain",0.f},{"outGain",0.f} } },
        };

        for (auto& f : factories)
        {
            auto file = dir.getChildFile(juce::String(f.name) + presetExtension);
            if (file.existsAsFile()) continue;

            // Build a minimal ValueTree compatible with APVTS state format
            juce::ValueTree state(apvts.state.getType().isValid() ? apvts.state.getType()
                                                                  : juce::Identifier("DL1"));
            for (auto& kv : f.values)
            {
                juce::ValueTree p("PARAM");
                p.setProperty("id", kv.first, nullptr);
                p.setProperty("value", kv.second, nullptr);
                state.appendChild(p, nullptr);
            }
            if (auto xml = state.createXml())
                xml->writeTo(file);
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::StringArray presetNames;
    juce::String currentPresetName { "Init" };
    bool dirty = false;
    bool loadingFromPreset = false;

    juce::ListenerList<Listener> listeners;
};
