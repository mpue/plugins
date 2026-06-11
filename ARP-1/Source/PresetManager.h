/*
  ==============================================================================

    PresetManager.h
    ARP-1 Luxury Arpeggiator — patch (preset) system.

    A patch stores the global arpeggiator parameters plus a reference to a
    pattern (by name) from the standalone pattern library (see PatternManager).
    A portable copy of the pattern is embedded too, so a patch loaded on another
    machine recreates its pattern in the library automatically.

    Switching patch flushes any pending pattern auto-save first, then assigns
    the patch's pattern.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ArpEngine.h"
#include "PatternManager.h"

namespace ARP1
{
    class PresetManager
    {
    public:
        PresetManager (ArpEngine& engineRef, PatternManager& patternRef)
            : engine (engineRef), patterns (patternRef)
        {
            rescanUserPresets();
            currentPresetName = "Init";
        }

        //--------------------------------------------------------------------
        // Preset listing

        juce::StringArray getFactoryPresetNames() const
        {
            juce::StringArray names { "Init" };
            for (const auto& f : factoryPresets())
                names.add (f.name);
            return names;
        }

        juce::StringArray getUserPresetNames() const { return userPresetNames; }

        juce::StringArray getAllPresetNames() const
        {
            auto n = getFactoryPresetNames();
            n.addArray (userPresetNames);
            return n;
        }

        juce::String getCurrentPresetName() const { return currentPresetName; }

        int getCurrentIndex() const { return getAllPresetNames().indexOf (currentPresetName); }

        bool isFactoryPreset (const juce::String& name) const
        {
            return getFactoryPresetNames().contains (name);
        }

        bool isUserPreset (const juce::String& name) const
        {
            return userPresetNames.contains (name);
        }

        juce::File getUserPresetDirectory() const
        {
            auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("ARP-1").getChildFile ("Presets");
            if (! dir.exists()) dir.createDirectory();
            return dir;
        }

        //--------------------------------------------------------------------
        // State capture / apply

        juce::ValueTree captureState() const
        {
            juce::ValueTree state ("ARP1Preset");
            state.setProperty ("enabled",     engine.isEnabled(),     nullptr);
            state.setProperty ("rate",        engine.getRateIndex(),  nullptr);
            state.setProperty ("octaves",     engine.getOctaves(),    nullptr);
            state.setProperty ("direction",   engine.getDirection(),  nullptr);
            state.setProperty ("gate",        engine.getGate(),       nullptr);
            state.setProperty ("swing",       engine.getSwing(),      nullptr);
            state.setProperty ("hold",        engine.getHold(),       nullptr);

            // Pattern is referenced by name and embedded for portability.
            state.setProperty ("patternName", patterns.getCurrentPatternName(), nullptr);
            state.appendChild (patterns.capturePattern(), nullptr);
            return state;
        }

        void applyState (const juce::ValueTree& state)
        {
            if (! state.isValid() || ! state.hasType ("ARP1Preset"))
            {
                resetToInit();
                return;
            }

            engine.setEnabled   ((bool)  state.getProperty ("enabled",   true));
            engine.setRateIndex ((int)   state.getProperty ("rate",      6));
            engine.setOctaves   ((int)   state.getProperty ("octaves",   1));
            engine.setDirection ((int)   state.getProperty ("direction", 0));
            engine.setGate      ((float) state.getProperty ("gate",      0.70f));
            engine.setSwing     ((float) state.getProperty ("swing",     0.0f));
            engine.setHold      ((bool)  state.getProperty ("hold",      false));

            const auto patternName = state.getProperty ("patternName", "Default").toString();
            patterns.ensureAndLoad (patternName, state.getChildWithName ("ARP1Pattern"));

            if (onPresetLoaded) onPresetLoaded();
        }

        //--------------------------------------------------------------------
        // Preset I/O

        void loadPreset (const juce::String& name)
        {
            patterns.flush();   // persist the outgoing pattern before switching

            if (name == "Init") { resetToInit(); return; }

            for (const auto& f : factoryPresets())
                if (f.name == name)
                {
                    applyFactory (f);
                    setCurrentPresetName (name);
                    return;
                }

            auto file = fileForPresetName (name);
            if (! file.existsAsFile()) return;

            if (auto xml = juce::XmlDocument::parse (file))
            {
                auto state = juce::ValueTree::fromXml (*xml);
                if (state.isValid())
                {
                    applyState (state);
                    setCurrentPresetName (name);
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
            const auto trimmed = name.trim();
            if (trimmed.isEmpty() || isFactoryPreset (trimmed)) return;

            patterns.flush();   // make sure the referenced pattern is on disk

            auto file = fileForPresetName (trimmed);
            if (auto xml = captureState().createXml())
                xml->writeTo (file, {});

            rescanUserPresets();
            setCurrentPresetName (trimmed, false);
            if (onPresetListChanged) onPresetListChanged();
        }

        void deleteUserPreset (const juce::String& name)
        {
            auto file = fileForPresetName (name);
            if (file.existsAsFile()) file.deleteFile();

            rescanUserPresets();
            if (currentPresetName == name) currentPresetName = "Init";

            if (onPresetListChanged)    onPresetListChanged();
            if (onCurrentPresetChanged) onCurrentPresetChanged();
        }

        void selectPrevious()
        {
            auto names = getAllPresetNames();
            if (names.isEmpty()) return;
            int i = getCurrentIndex();
            i = (i <= 0) ? names.size() - 1 : i - 1;
            loadPresetByIndex (i);
        }

        void selectNext()
        {
            auto names = getAllPresetNames();
            if (names.isEmpty()) return;
            int i = getCurrentIndex();
            i = (i + 1) % names.size();
            loadPresetByIndex (i);
        }

        void resetToInit()
        {
            patterns.flush();
            engine.setEnabled (true);
            engine.setRateIndex (6);      // 1/16
            engine.setOctaves (1);
            engine.setDirection ((int) Direction::Up);
            engine.setGate (0.70f);
            engine.setSwing (0.0f);
            engine.setHold (false);
            patterns.ensureAndLoad ("Default", {});
            setCurrentPresetName ("Init");
            if (onPresetLoaded) onPresetLoaded();
        }

        void setCurrentPresetNameForRestore (const juce::String& name)
        {
            setCurrentPresetName (name.isEmpty() ? juce::String ("Init") : name);
        }

        std::function<void()> onPresetListChanged;
        std::function<void()> onCurrentPresetChanged;
        std::function<void()> onPresetLoaded;

    private:
        //--------------------------------------------------------------------
        // Built-in factory patches. Each ships an embedded pattern that is
        // materialised into the pattern library the first time it is used.
        struct FactoryPreset
        {
            juce::String name;
            int   rate, octaves, direction, numSteps;
            float gate, swing;
            bool  hold;
            const char* onMask;
            const char* ratchets;
        };

        static const std::vector<FactoryPreset>& factoryPresets()
        {
            static const std::vector<FactoryPreset> presets =
            {
                { "Classic Up",     6, 2, (int) Direction::Up,       8, 0.70f, 0.00f, false,
                  "11111111", "11111111" },
                { "Trance Gate",    6, 1, (int) Direction::Up,      16, 0.45f, 0.00f, true,
                  "1011101110111011", "1111111111111111" },
                { "Swung Octaves",  4, 2, (int) Direction::UpDown,   8, 0.80f, 0.55f, false,
                  "11111111", "11111111" },
                { "Cascade",        6, 3, (int) Direction::Down,    12, 0.65f, 0.00f, false,
                  "111111111111", "111111111111" },
                { "Ratchet Pulse",  6, 2, (int) Direction::Up,       8, 0.55f, 0.20f, false,
                  "10111011", "13121312" },
                { "Wide Converge",  6, 4, (int) Direction::Converge,16, 0.60f, 0.00f, false,
                  "1111111111111111", "1111111111111111" },
                { "Bouncing Triplets", 5, 2, (int) Direction::DownUp, 12, 0.50f, 0.00f, false,
                  "110110110110", "111111111111" },
                { "Held Chord Stab", 1, 1, (int) Direction::Chord,   4, 0.85f, 0.00f, true,
                  "1010", "1111" },
            };
            return presets;
        }

        void applyFactory (const FactoryPreset& f)
        {
            engine.setEnabled (true);
            engine.setRateIndex (f.rate);
            engine.setOctaves (f.octaves);
            engine.setDirection (f.direction);
            engine.setGate (f.gate);
            engine.setSwing (f.swing);
            engine.setHold (f.hold);

            // Build an embedded pattern tree from the factory definition.
            juce::ValueTree pat ("ARP1Pattern");
            pat.setProperty ("name",     f.name, nullptr);
            pat.setProperty ("numSteps", f.numSteps, nullptr);

            const juce::String on (f.onMask), rt (f.ratchets);
            juce::String onMask, vels, ratchets;
            for (int i = 0; i < kMaxSteps; ++i)
            {
                onMask << ((i < on.length() && on[i] == '1') ? '1' : '0');
                if (i > 0) { vels << ','; ratchets << ','; }
                vels     << "0.800";
                ratchets << juce::String (i < rt.length() ? juce::String::charToString (rt[i]).getIntValue() : 1);
            }
            pat.setProperty ("on",       onMask,   nullptr);
            pat.setProperty ("vels",     vels,     nullptr);
            pat.setProperty ("ratchets", ratchets, nullptr);

            patterns.ensureAndLoad (f.name, pat);
            if (onPresetLoaded) onPresetLoaded();
        }

        juce::File fileForPresetName (const juce::String& name) const
        {
            return getUserPresetDirectory().getChildFile (name + ".arp1preset");
        }

        void rescanUserPresets()
        {
            userPresetNames.clear();
            auto dir = getUserPresetDirectory();
            if (! dir.exists()) return;

            for (auto file : dir.findChildFiles (juce::File::findFiles, false, "*.arp1preset"))
                userPresetNames.add (file.getFileNameWithoutExtension());

            userPresetNames.sortNatural();
        }

        void setCurrentPresetName (const juce::String& name, bool notify = true)
        {
            currentPresetName = name;
            if (notify && onCurrentPresetChanged) onCurrentPresetChanged();
        }

        ArpEngine&        engine;
        PatternManager&   patterns;
        juce::StringArray userPresetNames;
        juce::String      currentPresetName;
    };
}
