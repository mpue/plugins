/*
  ==============================================================================

    PatternManager.h
    ARP-1 Luxury Arpeggiator — standalone pattern library.

    Patterns (step on/off, velocity, ratchet and the pattern length) live as
    their own XML files in the user data directory, independent of patches.
    A patch (preset) only references a pattern by name, so the same pattern can
    be shared across patches and a library of arbitrarily many patterns can be
    built up.

    Auto-save: whenever the pattern is edited the change is written back to its
    file (debounced), and switching pattern or patch flushes any pending save
    first — so the user never has to save a pattern explicitly.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ArpEngine.h"

namespace ARP1
{
    class PatternManager : private juce::Timer
    {
    public:
        explicit PatternManager (ArpEngine& engineRef) : engine (engineRef)
        {
            rescan();

            if (! userPatternNames.contains ("Default"))
            {
                engine.resetPatternToDefault();
                currentName = "Default";
                writeCurrentToDisk();
                rescan();
            }

            currentName = "Default";
            loadPatternFromDisk ("Default");
        }

        ~PatternManager() override
        {
            flush();
            stopTimer();
        }

        //--------------------------------------------------------------------
        // Listing

        juce::StringArray getPatternNames() const { return userPatternNames; }
        juce::String      getCurrentPatternName() const { return currentName; }

        int getCurrentIndex() const { return userPatternNames.indexOf (currentName); }

        bool patternExists (const juce::String& name) const
        {
            return userPatternNames.contains (name);
        }

        juce::File getPatternDirectory() const
        {
            auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("ARP-1").getChildFile ("Patterns");
            if (! dir.exists()) dir.createDirectory();
            return dir;
        }

        //--------------------------------------------------------------------
        // Serialisation (also used to embed a portable copy inside a patch)

        juce::ValueTree capturePattern() const
        {
            juce::ValueTree t ("ARP1Pattern");
            t.setProperty ("name",     currentName,          nullptr);
            t.setProperty ("numSteps", engine.getNumSteps(), nullptr);

            juce::String onMask, vels, ratchets;
            for (int i = 0; i < kMaxSteps; ++i)
            {
                const auto& s = engine.getStep (i);
                onMask << (s.on ? '1' : '0');
                if (i > 0) { vels << ','; ratchets << ','; }
                vels     << juce::String (s.vel, 3);
                ratchets << juce::String (s.ratchet);
            }
            t.setProperty ("on",       onMask,   nullptr);
            t.setProperty ("vels",     vels,     nullptr);
            t.setProperty ("ratchets", ratchets, nullptr);
            return t;
        }

        void applyPattern (const juce::ValueTree& t)
        {
            if (! t.isValid() || ! t.hasType ("ARP1Pattern"))
            {
                engine.resetPatternToDefault();
                return;
            }

            engine.setNumSteps ((int) t.getProperty ("numSteps", 8));
            engine.clearPattern();

            const juce::String onMask = t.getProperty ("on", "").toString();
            juce::StringArray vels, ratchets;
            vels    .addTokens (t.getProperty ("vels", "").toString(),     ",", "");
            ratchets.addTokens (t.getProperty ("ratchets", "").toString(), ",", "");

            for (int i = 0; i < kMaxSteps; ++i)
            {
                engine.setStepOn (i, i < onMask.length() && onMask[i] == '1');
                if (i < vels.size())     engine.setStepVel     (i, vels[i].getFloatValue());
                if (i < ratchets.size()) engine.setStepRatchet (i, ratchets[i].getIntValue());
            }
        }

        //--------------------------------------------------------------------
        // Library operations (each flushes any pending auto-save first)

        void loadPattern (const juce::String& name)
        {
            flush();
            if (! patternExists (name)) return;
            loadPatternFromDisk (name);
            if (onCurrentPatternChanged) onCurrentPatternChanged();
        }

        void loadPatternByIndex (int index)
        {
            if (juce::isPositiveAndBelow (index, userPatternNames.size()))
                loadPattern (userPatternNames[index]);
        }

        /** Create a fresh, empty-ish pattern and switch to it. */
        void newPattern (const juce::String& desiredName = {})
        {
            flush();
            const auto name = uniqueName (desiredName.trim().isNotEmpty() ? desiredName.trim()
                                                                          : "Pattern");
            engine.resetPatternToDefault();
            currentName = name;
            writeCurrentToDisk();
            rescan();
            if (onPatternListChanged)    onPatternListChanged();
            if (onCurrentPatternChanged) onCurrentPatternChanged();
        }

        /** Duplicate the current pattern under a new name and switch to it. */
        void duplicateAs (const juce::String& desiredName)
        {
            flush();
            const auto name = uniqueName (desiredName.trim().isNotEmpty() ? desiredName.trim()
                                                                          : currentName + " copy");
            currentName = name;
            writeCurrentToDisk();      // current engine pattern → new file
            rescan();
            if (onPatternListChanged)    onPatternListChanged();
            if (onCurrentPatternChanged) onCurrentPatternChanged();
        }

        void deletePattern (const juce::String& name)
        {
            flush();
            auto file = fileFor (name);
            if (file.existsAsFile()) file.deleteFile();
            rescan();

            if (currentName == name)
            {
                if (userPatternNames.isEmpty())
                {
                    engine.resetPatternToDefault();
                    currentName = "Default";
                    writeCurrentToDisk();
                    rescan();
                }
                else
                {
                    loadPatternFromDisk (userPatternNames[0]);
                }
            }

            if (onPatternListChanged)    onPatternListChanged();
            if (onCurrentPatternChanged) onCurrentPatternChanged();
        }

        void selectPrevious()
        {
            if (userPatternNames.isEmpty()) return;
            int i = getCurrentIndex();
            i = (i <= 0) ? userPatternNames.size() - 1 : i - 1;
            loadPatternByIndex (i);
        }

        void selectNext()
        {
            if (userPatternNames.isEmpty()) return;
            int i = getCurrentIndex();
            i = (i + 1) % userPatternNames.size();
            loadPatternByIndex (i);
        }

        //--------------------------------------------------------------------
        // Patch integration

        /** Ensure a named pattern is present (creating it from an embedded copy
            or a blank default if missing) and make it current. Used when a patch
            is loaded and assigns its pattern. */
        void ensureAndLoad (const juce::String& name, const juce::ValueTree& embedded)
        {
            flush();
            const auto wanted = name.trim().isEmpty() ? juce::String ("Default") : name.trim();

            if (patternExists (wanted))
            {
                loadPatternFromDisk (wanted);
            }
            else
            {
                if (embedded.isValid() && embedded.hasType ("ARP1Pattern"))
                    applyPattern (embedded);
                else
                    engine.resetPatternToDefault();

                currentName = wanted;
                writeCurrentToDisk();
                rescan();
                if (onPatternListChanged) onPatternListChanged();
            }

            if (onCurrentPatternChanged) onCurrentPatternChanged();
        }

        //--------------------------------------------------------------------
        // Auto-save

        /** Mark the current pattern dirty; debounced write follows shortly. */
        void markDirty()
        {
            dirty = true;
            startTimer (500);
        }

        /** Write any pending change immediately (patch switch, project save, …). */
        void flush()
        {
            stopTimer();
            if (dirty)
            {
                writeCurrentToDisk();
                dirty = false;
            }
        }

        std::function<void()> onPatternListChanged;
        std::function<void()> onCurrentPatternChanged;

    private:
        void timerCallback() override { flush(); }

        void loadPatternFromDisk (const juce::String& name)
        {
            auto file = fileFor (name);
            if (auto xml = juce::XmlDocument::parse (file))
            {
                auto t = juce::ValueTree::fromXml (*xml);
                if (t.isValid())
                {
                    applyPattern (t);
                    currentName = name;
                    dirty = false;
                    return;
                }
            }
            // Fallback: keep current engine pattern but adopt the name.
            currentName = name;
        }

        void writeCurrentToDisk()
        {
            if (currentName.isEmpty()) return;
            if (auto xml = capturePattern().createXml())
                xml->writeTo (fileFor (currentName), {});
        }

        juce::File fileFor (const juce::String& name) const
        {
            return getPatternDirectory().getChildFile (name + ".arp1pattern");
        }

        void rescan()
        {
            userPatternNames.clear();
            auto dir = getPatternDirectory();
            if (! dir.exists()) return;
            for (auto f : dir.findChildFiles (juce::File::findFiles, false, "*.arp1pattern"))
                userPatternNames.add (f.getFileNameWithoutExtension());
            userPatternNames.sortNatural();
        }

        juce::String uniqueName (const juce::String& base) const
        {
            if (! userPatternNames.contains (base)) return base;
            for (int i = 1; i < 9999; ++i)
            {
                auto cand = base + " " + juce::String (i);
                if (! userPatternNames.contains (cand)) return cand;
            }
            return base;
        }

        ArpEngine&        engine;
        juce::StringArray userPatternNames;
        juce::String      currentName;
        bool              dirty = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternManager)
    };
}
