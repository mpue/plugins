/*
  ==============================================================================

    PresetManager.h
    Preset handling for Pike.
      - Factory presets are defined in code as parameter overrides (always
        available, no install step). Loading one resets every parameter to its
        default, then applies the overrides.
      - User presets are XML snapshots of the APVTS state, saved as
        ".pikepreset" files under the user's Documents/Pike/Presets folder.
    A combined, ordered name list drives the editor's preset browser.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../params/ParameterIDs.h"
#include "FactoryPresets.h"

namespace pike
{
    class PresetManager
    {
    public:
        PresetManager (juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& state)
            : processor (proc), apvts (state)
        {
            factory = buildFactoryPresets();

            presetDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile ("Pike").getChildFile ("Presets");
            presetDir.createDirectory();
        }

        static constexpr const char* fileExtension = ".pikepreset";

        //======================================================================
        juce::StringArray getFactoryNames() const
        {
            juce::StringArray names;
            for (const auto& p : factory)
                names.add (p.name);
            return names;
        }

        juce::StringArray getUserNames() const
        {
            juce::StringArray names;
            for (const auto& f : findUserFiles())
                names.add (f.getFileNameWithoutExtension());
            return names;
        }

        const juce::String& getCurrentName() const noexcept { return currentName; }

        /** Resets non-parameter state (e.g. MSEG point data) alongside the
            parameters when a factory preset loads. Set by the processor. */
        std::function<void()> onResetNonParamState;

        //======================================================================
        void loadFactory (int index)
        {
            if (! juce::isPositiveAndBelow (index, (int) factory.size()))
                return;

            resetToDefaults();
            if (onResetNonParamState != nullptr)
                onResetNonParamState();
            for (const auto& v : factory[(size_t) index].values)
                if (auto* p = apvts.getParameter (v.first))
                    p->setValueNotifyingHost (p->convertTo0to1 (v.second));

            currentName = factory[(size_t) index].name;
        }

        void loadFactoryByName (const juce::String& name)
        {
            for (size_t i = 0; i < factory.size(); ++i)
                if (factory[i].name == name) { loadFactory ((int) i); return; }
        }

        void loadUser (const juce::String& name)
        {
            auto file = presetDir.getChildFile (name + fileExtension);
            if (! file.existsAsFile())
                return;

            if (auto xml = juce::XmlDocument::parse (file))
                if (xml->hasTagName (apvts.state.getType()))
                {
                    apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    currentName = name;
                }
        }

        /** Saves the current state as a user preset; returns the saved name. */
        juce::String savePreset (const juce::String& rawName)
        {
            auto name = rawName.trim();
            if (name.isEmpty())
                name = "Untitled";

            auto file = presetDir.getChildFile (name + fileExtension);
            if (auto xml = apvts.copyState().createXml())
                xml->writeTo (file);

            currentName = name;
            return name;
        }

        //======================================================================
        void resetToDefaults()
        {
            for (auto* param : processor.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                    rp->setValueNotifyingHost (rp->getDefaultValue());
        }

    private:
        juce::Array<juce::File> findUserFiles() const
        {
            juce::Array<juce::File> files;
            presetDir.findChildFiles (files, juce::File::findFiles, false,
                                      juce::String ("*") + fileExtension);
            files.sort();
            return files;
        }

        juce::AudioProcessor&               processor;
        juce::AudioProcessorValueTreeState& apvts;
        juce::File                          presetDir;
        std::vector<Preset>                 factory;
        juce::String                        currentName { "Init" };
    };
}
