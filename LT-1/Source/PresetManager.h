/*
  ==============================================================================

    LT-1 — Luxury Limiter
    PresetManager.h

    Robust XML-based preset system. Presets live as individual .ltpreset files
    inside the user's application data directory so they survive across DAW
    sessions and plugin formats.

    Header-only so the existing build target picks it up via the
    PluginProcessor.cpp / PluginEditor.cpp translation units without needing a
    Projucer re-save.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PresetManager : public juce::ChangeBroadcaster
{
public:
    static constexpr const char* extension         = "ltpreset";
    static constexpr const char* defaultPresetName = "Init";

    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : state (s)
    {
        auto dir = getPresetDirectory();
        if (! dir.exists())
            dir.createDirectory();

        // Ship factory presets on first launch so the dropdown is never empty.
        if (getPresetNames().isEmpty())
            installFactoryPresets();
    }

    juce::File getPresetDirectory() const
    {
        auto root = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        return root.getChildFile ("Application Support/LT-1/Presets");
       #else
        return root.getChildFile ("LT-1/Presets");
       #endif
    }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;
        auto dir = getPresetDirectory();
        if (! dir.exists()) return names;

        for (auto& f : dir.findChildFiles (juce::File::findFiles, false,
                                            juce::String ("*.") + extension))
            names.add (f.getFileNameWithoutExtension());

        names.sortNatural();
        return names;
    }

    bool savePreset (const juce::String& name)
    {
        const auto trimmed = name.trim();
        if (trimmed.isEmpty()) return false;

        auto vt  = state.copyState();
        auto xml = vt.createXml();
        if (xml == nullptr) return false;

        auto file = fileForName (trimmed);
        if (! file.getParentDirectory().exists())
            file.getParentDirectory().createDirectory();
        if (file.existsAsFile())
            file.deleteFile();
        if (! xml->writeTo (file)) return false;

        currentPresetName = trimmed;
        sendChangeMessage();
        return true;
    }

    bool loadPreset (const juce::String& name)
    {
        auto file = fileForName (name);
        if (! file.existsAsFile()) return false;

        if (auto xml = juce::XmlDocument::parse (file))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            if (! vt.isValid()) return false;
            state.replaceState (vt);
            currentPresetName = name;
            sendChangeMessage();
            return true;
        }
        return false;
    }

    bool deletePreset (const juce::String& name)
    {
        auto file = fileForName (name);
        if (! file.existsAsFile()) return false;

        if (file.deleteFile())
        {
            if (currentPresetName == name)
                currentPresetName = {};
            sendChangeMessage();
            return true;
        }
        return false;
    }

    void loadInitPreset()
    {
        for (auto* p : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                rp->setValueNotifyingHost (rp->getDefaultValue());

        currentPresetName = defaultPresetName;
        sendChangeMessage();
    }

    juce::String getCurrentPresetName() const { return currentPresetName; }
    void markAsModified() { currentPresetName = {}; sendChangeMessage(); }

    bool loadNext()
    {
        auto names = getPresetNames();
        if (names.isEmpty()) return false;
        int idx = names.indexOf (currentPresetName);
        idx = (idx + 1) % names.size();
        return loadPreset (names[idx]);
    }

    bool loadPrevious()
    {
        auto names = getPresetNames();
        if (names.isEmpty()) return false;
        int idx = names.indexOf (currentPresetName);
        idx = (idx <= 0 ? names.size() - 1 : idx - 1);
        return loadPreset (names[idx]);
    }

private:
    juce::File fileForName (const juce::String& name) const
    {
        return getPresetDirectory().getChildFile (name + "." + extension);
    }

    void installFactoryPresets()
    {
        struct Factory { const char* name; float thr, ceil, rel, knee, in, out, look, autoR, link; };
        const Factory factory[] =
        {
            { "Transparent Master", -1.0f, -0.3f, 120.0f, 2.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
            { "Loud & Proud",       -6.0f, -0.3f,  60.0f, 4.0f, 3.0f, 0.0f, 1.0f, 1.0f, 1.0f },
            { "Vocal Polish",       -3.0f, -0.5f, 200.0f, 6.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
            { "Drum Bus",           -4.0f, -0.2f,  40.0f, 2.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
            { "Brickwall",          -2.0f, -0.1f,  20.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
            { "Smooth Glue",        -2.0f, -0.4f, 300.0f, 8.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
        };

        auto add = [] (juce::ValueTree& params, const juce::String& id, float v)
        {
            juce::ValueTree pn ("PARAM");
            pn.setProperty ("id", id, nullptr);
            pn.setProperty ("value", v, nullptr);
            params.appendChild (pn, nullptr);
        };

        for (auto& p : factory)
        {
            juce::ValueTree vt ("LT1State");
            juce::ValueTree params ("PARAMS");
            add (params, "threshold",   p.thr);
            add (params, "ceiling",     p.ceil);
            add (params, "release",     p.rel);
            add (params, "knee",        p.knee);
            add (params, "inGain",      p.in);
            add (params, "outGain",     p.out);
            add (params, "lookahead",   p.look);
            add (params, "autoRelease", p.autoR);
            add (params, "stereoLink",  p.link);
            add (params, "bypass",      0.0f);
            vt.appendChild (params, nullptr);

            if (auto xml = vt.createXml())
                xml->writeTo (fileForName (p.name));
        }
    }

    juce::AudioProcessorValueTreeState& state;
    juce::String currentPresetName;
};
