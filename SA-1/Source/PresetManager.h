/*
  ==============================================================================

    PresetManager.h
    SA-1 Luxury Quick Sampler — kit preset system.

    Persists complete pad banks (file paths + per-pad parameters) as XML
    presets in the user data directory. Factory "Init" preset wipes all
    pads and resets parameters to defaults.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SamplerEngine.h"
#include "StepSequencer.h"

namespace SA1
{
    class PresetManager
    {
    public:
        PresetManager (SamplerEngine& engineRef, StepSequencer& seqRef)
            : engine (engineRef), sequencer (seqRef)
        {
            rescanUserPresets();
            currentPresetName = "Init";
        }

        //--------------------------------------------------------------------
        // Preset listing

        juce::StringArray getFactoryPresetNames() const
        {
            return { "Init" };
        }

        juce::StringArray getUserPresetNames() const { return userPresetNames; }

        juce::StringArray getAllPresetNames() const
        {
            auto n = getFactoryPresetNames();
            n.addArray (userPresetNames);
            return n;
        }

        juce::String getCurrentPresetName() const { return currentPresetName; }

        int getCurrentIndex() const
        {
            return getAllPresetNames().indexOf (currentPresetName);
        }

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
                           .getChildFile ("SA-1").getChildFile ("Presets");
            if (! dir.exists()) dir.createDirectory();
            return dir;
        }

        //--------------------------------------------------------------------
        // State (pad data) helpers — used by both the preset manager and the
        // host's getStateInformation/setStateInformation

        /** Build a ValueTree representing the engine's pad bank + master state. */
        juce::ValueTree captureState() const
        {
            juce::ValueTree state ("SA1Kit");
            state.setProperty ("masterGainDb", engine.getMasterGainDb(), nullptr);

            juce::ValueTree padsNode ("Pads");
            for (int i = 0; i < kNumPads; ++i)
            {
                const auto& s = engine.getPad (i).getState();
                juce::ValueTree p ("Pad");
                p.setProperty ("index",       i,                  nullptr);
                p.setProperty ("filePath",    s.filePath,         nullptr);
                p.setProperty ("displayName", s.displayName,      nullptr);
                p.setProperty ("gainDb",      s.gainDb,           nullptr);
                p.setProperty ("pan",         s.pan,              nullptr);
                p.setProperty ("pitchSemis",  s.pitchSemis,       nullptr);
                p.setProperty ("fineCents",   s.fineCents,        nullptr);
                p.setProperty ("attackMs",    s.attackMs,         nullptr);
                p.setProperty ("releaseMs",   s.releaseMs,        nullptr);
                p.setProperty ("startPos",    s.startPos,         nullptr);
                p.setProperty ("reverse",     s.reverse,          nullptr);
                p.setProperty ("chokeGroup",  s.chokeGroup,       nullptr);
                p.setProperty ("midiNote",    s.midiNote,         nullptr);
                padsNode.appendChild (p, nullptr);
            }
            state.appendChild (padsNode, nullptr);

            state.appendChild (captureSequencer(), nullptr);
            return state;
        }

        /** Serialise the step sequencer (mode, parameters and pattern grid). */
        juce::ValueTree captureSequencer() const
        {
            juce::ValueTree seq ("Sequencer");
            seq.setProperty ("enabled",  sequencer.isEnabled(),          nullptr);
            seq.setProperty ("sync",     (int) sequencer.getSyncMode(),  nullptr);
            seq.setProperty ("hold",     (int) sequencer.getHoldMode(),  nullptr);
            seq.setProperty ("rate",     sequencer.getRateIndex(),       nullptr);
            seq.setProperty ("numSteps", sequencer.getNumSteps(),        nullptr);
            seq.setProperty ("swing",    sequencer.getSwing(),           nullptr);

            for (int p = 0; p < kNumPads; ++p)
            {
                juce::String mask, vels;
                for (int s = 0; s < kSeqMaxSteps; ++s)
                {
                    mask << (sequencer.getStep (p, s) ? '1' : '0');
                    if (s > 0) vels << ',';
                    vels << juce::String (sequencer.getStepVel (p, s), 3);
                }
                juce::ValueTree t ("Track");
                t.setProperty ("pad",  p,    nullptr);
                t.setProperty ("mask", mask, nullptr);
                t.setProperty ("vels", vels, nullptr);
                seq.appendChild (t, nullptr);
            }
            return seq;
        }

        void applySequencer (const juce::ValueTree& seq)
        {
            // Always start from a clean slate so presets without a sequencer
            // node reset cleanly.
            sequencer.clearAll();
            sequencer.setEnabled  (false);
            sequencer.setSyncMode (SeqSync::HostLock);
            sequencer.setHoldMode (SeqHold::Gate);
            sequencer.setRateIndex (3);
            sequencer.setNumSteps  (16);
            sequencer.setSwing     (0.0f);

            if (! seq.isValid() || ! seq.hasType ("Sequencer"))
                return;

            sequencer.setEnabled  ((bool) seq.getProperty ("enabled",  false));
            sequencer.setSyncMode ((SeqSync) (int) seq.getProperty ("sync", 0));
            sequencer.setHoldMode ((SeqHold) (int) seq.getProperty ("hold", 0));
            sequencer.setRateIndex ((int) seq.getProperty ("rate",     3));
            sequencer.setNumSteps  ((int) seq.getProperty ("numSteps", 16));
            sequencer.setSwing    ((float) seq.getProperty ("swing",   0.0f));

            for (int c = 0; c < seq.getNumChildren(); ++c)
            {
                auto t = seq.getChild (c);
                if (! t.hasType ("Track")) continue;
                const int p = (int) t.getProperty ("pad", -1);
                if (! juce::isPositiveAndBelow (p, kNumPads)) continue;

                const juce::String mask = t.getProperty ("mask", "").toString();
                juce::StringArray  vels;
                vels.addTokens (t.getProperty ("vels", "").toString(), ",", "");

                for (int s = 0; s < kSeqMaxSteps; ++s)
                {
                    sequencer.setStep (p, s, s < mask.length() && mask[s] == '1');
                    if (s < vels.size())
                        sequencer.setStepVel (p, s, vels[s].getFloatValue());
                }
            }
        }

        /** Apply a captured ValueTree back into the engine. Re-loads sample files
            from disk if their paths still exist. */
        void applyState (const juce::ValueTree& state)
        {
            if (! state.isValid() || ! state.hasType ("SA1Kit"))
                return;

            engine.setMasterGainDb ((float) state.getProperty ("masterGainDb", 0.0f));

            auto padsNode = state.getChildWithName ("Pads");
            for (int c = 0; c < padsNode.getNumChildren(); ++c)
            {
                auto pNode = padsNode.getChild (c);
                const int idx = (int) pNode.getProperty ("index", -1);
                if (! juce::isPositiveAndBelow (idx, kNumPads)) continue;

                auto& pad = engine.getPad (idx);
                pad.clearSample();

                auto& s = pad.getState();
                s.gainDb       = (float) pNode.getProperty ("gainDb",      0.0f);
                s.pan          = (float) pNode.getProperty ("pan",         0.0f);
                s.pitchSemis   = (float) pNode.getProperty ("pitchSemis",  0.0f);
                s.fineCents    = (float) pNode.getProperty ("fineCents",   0.0f);
                s.attackMs     = (float) pNode.getProperty ("attackMs",    1.0f);
                s.releaseMs    = (float) pNode.getProperty ("releaseMs",   200.0f);
                s.startPos     = (float) pNode.getProperty ("startPos",    0.0f);
                s.reverse      = (bool)  pNode.getProperty ("reverse",     false);
                s.chokeGroup   = (int)   pNode.getProperty ("chokeGroup",  0);
                s.midiNote     = (int)   pNode.getProperty ("midiNote",    -1);

                const juce::String path = pNode.getProperty ("filePath", "").toString();
                if (path.isNotEmpty())
                {
                    juce::File f (path);
                    if (f.existsAsFile())
                        engine.loadSampleIntoPad (idx, f);
                    else
                    {
                        s.filePath    = path;             // remember path even if missing
                        s.displayName = f.getFileNameWithoutExtension() + " (missing)";
                    }
                }
            }

            applySequencer (state.getChildWithName ("Sequencer"));

            if (onPresetLoaded) onPresetLoaded();
        }

        //--------------------------------------------------------------------
        // Preset I/O

        void loadPreset (const juce::String& name)
        {
            if (name == "Init")
            {
                resetToInit();
                return;
            }

            auto file = fileForPresetName (name);
            if (! file.existsAsFile())
                return;

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
            if (trimmed.isEmpty() || isFactoryPreset (trimmed))
                return;

            auto file = fileForPresetName (trimmed);
            auto state = captureState();
            if (auto xml = state.createXml())
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
            if (currentPresetName == name)
                currentPresetName = "Init";

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
            for (int i = 0; i < kNumPads; ++i)
            {
                engine.clearPad (i);
                auto& s = engine.getPad (i).getState();
                s = PadState {};   // reset all parameters
            }
            engine.setMasterGainDb (0.0f);
            applySequencer ({});   // clears pattern + resets sequencer defaults
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
        juce::File fileForPresetName (const juce::String& name) const
        {
            return getUserPresetDirectory().getChildFile (name + ".sa1preset");
        }

        void rescanUserPresets()
        {
            userPresetNames.clear();
            auto dir = getUserPresetDirectory();
            if (! dir.exists()) return;

            for (auto file : dir.findChildFiles (juce::File::findFiles, false, "*.sa1preset"))
                userPresetNames.add (file.getFileNameWithoutExtension());

            userPresetNames.sortNatural();
        }

        void setCurrentPresetName (const juce::String& name, bool notify = true)
        {
            currentPresetName = name;
            if (notify && onCurrentPresetChanged)
                onCurrentPresetChanged();
        }

        SamplerEngine&    engine;
        StepSequencer&    sequencer;
        juce::StringArray userPresetNames;
        juce::String      currentPresetName;
    };
}
