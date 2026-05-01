/*
  ==============================================================================

    PresetManager.h
    Factory + user presets backed by XML files on disk.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PresetManager
{
public:
    enum class Kind { Factory, User };

    struct Preset
    {
        juce::String name;
        Kind         kind = Kind::User;
        juce::File   file;                // empty for factory presets
        std::unique_ptr<juce::XmlElement> xml;

        juce::String displayName() const
        {
            return kind == Kind::Factory ? "[F] " + name : name;
        }
    };

    PresetManager (juce::AudioProcessorValueTreeState& s, const juce::String& pluginName)
        : apvts (s)
    {
        userDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile (pluginName)
                    .getChildFile ("Presets");

        if (! userDir.exists())
            userDir.createDirectory();

        buildFactoryPresets();
        rescan();
    }

    void rescan()
    {
        // Keep factory presets, drop user ones, then re-read disk
        std::vector<std::unique_ptr<Preset>> kept;
        for (auto& p : presets)
            if (p->kind == Kind::Factory)
                kept.push_back (std::move (p));
        presets = std::move (kept);

        if (userDir.isDirectory())
        {
            auto files = userDir.findChildFiles (juce::File::findFiles, false, "*.preset");
            files.sort();
            for (auto& f : files)
            {
                if (auto xml = juce::XmlDocument::parse (f))
                {
                    auto p   = std::make_unique<Preset>();
                    p->name  = f.getFileNameWithoutExtension();
                    p->kind  = Kind::User;
                    p->file  = f;
                    p->xml.reset (xml.release());
                    presets.push_back (std::move (p));
                }
            }
        }

        if (onListChanged) onListChanged();
    }

    int getNumPresets() const noexcept { return (int) presets.size(); }

    const Preset* getPreset (int idx) const
    {
        if (juce::isPositiveAndBelow (idx, (int) presets.size()))
            return presets[(size_t) idx].get();
        return nullptr;
    }

    int getCurrentIndex() const noexcept { return currentIndex; }

    juce::String getCurrentName() const
    {
        if (auto* p = getPreset (currentIndex))
            return p->name;
        return "Init";
    }

    void loadPreset (int idx)
    {
        auto* p = getPreset (idx);
        if (p == nullptr || p->xml == nullptr) return;

        auto vt = juce::ValueTree::fromXml (*p->xml);
        if (! vt.isValid()) return;

        // Replace the whole APVTS state (preserves parameter ranges)
        apvts.replaceState (vt);

        currentIndex = idx;
        if (onCurrentChanged) onCurrentChanged();
    }

    void next()
    {
        if (presets.empty()) return;
        loadPreset ((currentIndex + 1) % (int) presets.size());
    }

    void prev()
    {
        if (presets.empty()) return;
        const int n = (int) presets.size();
        loadPreset ((currentIndex - 1 + n) % n);
    }

    // Save the current APVTS state as a user preset on disk.
    bool saveAsUserPreset (const juce::String& rawName, juce::String& errOut)
    {
        const auto safeName = juce::File::createLegalFileName (rawName).trim();
        if (safeName.isEmpty())
        {
            errOut = "Please enter a valid name.";
            return false;
        }

        if (! userDir.exists() && ! userDir.createDirectory().wasOk())
        {
            errOut = "Could not create preset folder.";
            return false;
        }

        auto file = userDir.getChildFile (safeName + ".preset");

        auto state = apvts.copyState();
        if (auto xml = state.createXml())
        {
            if (! xml->writeTo (file))
            {
                errOut = "Could not write preset file.";
                return false;
            }
        }
        else
        {
            errOut = "Could not serialise state.";
            return false;
        }

        rescan();
        // Select the newly-written preset
        for (int i = 0; i < (int) presets.size(); ++i)
            if (presets[(size_t) i]->kind == Kind::User
                && presets[(size_t) i]->name == safeName)
            {
                currentIndex = i;
                if (onCurrentChanged) onCurrentChanged();
                break;
            }
        return true;
    }

    bool deleteCurrentUserPreset (juce::String& errOut)
    {
        auto* p = getPreset (currentIndex);
        if (p == nullptr || p->kind != Kind::User)
        {
            errOut = "Only user presets can be deleted.";
            return false;
        }
        if (! p->file.deleteFile())
        {
            errOut = "Could not delete preset file.";
            return false;
        }
        rescan();
        currentIndex = juce::jlimit (0, juce::jmax (0, (int) presets.size() - 1), currentIndex);
        if (onCurrentChanged) onCurrentChanged();
        return true;
    }

    juce::File getUserDir() const { return userDir; }

    std::function<void()> onListChanged;
    std::function<void()> onCurrentChanged;

private:
    void addFactory (const juce::String& name,
                     std::initializer_list<std::pair<juce::String, float>> values)
    {
        auto p   = std::make_unique<Preset>();
        p->name  = name;
        p->kind  = Kind::Factory;

        // Build a ValueTree mirroring an APVTS state with these parameter values.
        juce::ValueTree state ("CH1");
        for (auto& kv : values)
        {
            juce::ValueTree paramTree ("PARAM");
            paramTree.setProperty ("id",    kv.first,  nullptr);
            paramTree.setProperty ("value", kv.second, nullptr);
            state.appendChild (paramTree, nullptr);
        }
        p->xml.reset (state.createXml().release());
        presets.push_back (std::move (p));
    }

    void buildFactoryPresets()
    {
        // Parameter IDs must match those registered in the processor.
        // values: rate(Hz), depth(%), delay(ms), feedback(%), mix(%), width(%), voices(0..3), tone(Hz), gain(dB), bypass(0/1)
        addFactory ("Init",
            {{"rate", 0.5f}, {"depth", 35.0f}, {"delay", 9.0f}, {"feedback", 0.0f},
             {"mix", 50.0f}, {"width", 70.0f}, {"voices", 3.0f}, {"tone", 8000.0f},
             {"gain", 0.0f}, {"bypass", 0.0f}});

        addFactory ("Subtle Lush",
            {{"rate", 0.22f}, {"depth", 28.0f}, {"delay", 11.0f}, {"feedback", 0.0f},
             {"mix", 35.0f}, {"width", 85.0f}, {"voices", 3.0f}, {"tone", 9000.0f},
             {"gain", 0.0f}, {"bypass", 0.0f}});

        addFactory ("Vintage Tape",
            {{"rate", 0.7f}, {"depth", 55.0f}, {"delay", 12.0f}, {"feedback", 18.0f},
             {"mix", 50.0f}, {"width", 65.0f}, {"voices", 2.0f}, {"tone", 5500.0f},
             {"gain", 0.0f}, {"bypass", 0.0f}});

        addFactory ("80s Wide",
            {{"rate", 1.1f}, {"depth", 60.0f}, {"delay", 8.0f}, {"feedback", 8.0f},
             {"mix", 55.0f}, {"width", 95.0f}, {"voices", 3.0f}, {"tone", 7500.0f},
             {"gain", 0.0f}, {"bypass", 0.0f}});

        addFactory ("Deep & Slow",
            {{"rate", 0.12f}, {"depth", 80.0f}, {"delay", 15.0f}, {"feedback", 0.0f},
             {"mix", 45.0f}, {"width", 100.0f}, {"voices", 3.0f}, {"tone", 6500.0f},
             {"gain", 0.0f}, {"bypass", 0.0f}});

        addFactory ("Shimmer",
            {{"rate", 3.0f}, {"depth", 30.0f}, {"delay", 6.0f}, {"feedback", 12.0f},
             {"mix", 45.0f}, {"width", 80.0f}, {"voices", 3.0f}, {"tone", 11000.0f},
             {"gain", 0.0f}, {"bypass", 0.0f}});

        addFactory ("Doubler",
            {{"rate", 0.18f}, {"depth", 12.0f}, {"delay", 14.0f}, {"feedback", 0.0f},
             {"mix", 50.0f}, {"width", 60.0f}, {"voices", 1.0f}, {"tone", 9500.0f},
             {"gain", 0.0f}, {"bypass", 0.0f}});

        addFactory ("Detuned",
            {{"rate", 1.7f}, {"depth", 90.0f}, {"delay", 10.0f}, {"feedback", -25.0f},
             {"mix", 60.0f}, {"width", 80.0f}, {"voices", 3.0f}, {"tone", 7000.0f},
             {"gain", 0.0f}, {"bypass", 0.0f}});
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::File userDir;
    std::vector<std::unique_ptr<Preset>> presets;
    int currentIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
