/*
  ==============================================================================

    PresetManager.h
    Created: Preset management for EQ8
    Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "EQBand.h"

class PresetManager
{
public:
    PresetManager(std::array<EQBand, 8>& bands) : eqBands(bands)
    {
        presetDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("EQ8")
            .getChildFile("Presets");
        presetDirectory.createDirectory();
        initFactoryPresets();
    }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;

        for (auto& name : factoryPresetNames)
            names.add(name);

        auto files = presetDirectory.findChildFiles(juce::File::findFiles, false, "*.xml");
        files.sort();

        for (auto& file : files)
        {
            auto name = file.getFileNameWithoutExtension();
            if (!names.contains(name))
                names.add(name);
        }

        return names;
    }

    void savePreset(const juce::String& name)
    {
        if (name.isEmpty() || isFactoryPreset(name))
            return;

        auto xml = createStateXml(name);
        auto file = presetDirectory.getChildFile(name + ".xml");
        xml->writeTo(file);
        currentPresetName = name;
    }

    bool loadPreset(const juce::String& name)
    {
        for (int i = 0; i < factoryPresetNames.size(); ++i)
        {
            if (factoryPresetNames[i] == name)
            {
                applyFactoryPreset(i);
                currentPresetName = name;
                return true;
            }
        }

        auto file = presetDirectory.getChildFile(name + ".xml");
        if (file.existsAsFile())
        {
            auto xml = juce::parseXML(file);
            if (xml != nullptr)
            {
                restoreFromStateXml(*xml);
                currentPresetName = name;
                return true;
            }
        }

        return false;
    }

    void deletePreset(const juce::String& name)
    {
        if (isFactoryPreset(name))
            return;

        auto file = presetDirectory.getChildFile(name + ".xml");
        if (file.existsAsFile())
            file.deleteFile();

        if (currentPresetName == name)
            currentPresetName = factoryPresetNames[0];
    }

    bool isFactoryPreset(const juce::String& name) const
    {
        return factoryPresetNames.contains(name);
    }

    juce::String getCurrentPresetName() const { return currentPresetName; }

    void loadNextPreset()
    {
        auto names = getPresetNames();
        int idx = names.indexOf(currentPresetName);
        if (idx >= 0 && idx < names.size() - 1)
            loadPreset(names[idx + 1]);
        else if (names.size() > 0)
            loadPreset(names[0]);
    }

    void loadPreviousPreset()
    {
        auto names = getPresetNames();
        int idx = names.indexOf(currentPresetName);
        if (idx > 0)
            loadPreset(names[idx - 1]);
        else if (names.size() > 0)
            loadPreset(names[names.size() - 1]);
    }

    std::unique_ptr<juce::XmlElement> createStateXml(const juce::String& name = "") const
    {
        auto xml = std::make_unique<juce::XmlElement>("EQ8State");
        xml->setAttribute("presetName", name.isEmpty() ? currentPresetName : name);

        for (int i = 0; i < 8; ++i)
        {
            auto* bandXml = xml->createNewChildElement("Band");
            bandXml->setAttribute("index", i);
            bandXml->setAttribute("type", static_cast<int>(eqBands[i].getType()));
            bandXml->setAttribute("frequency", static_cast<double>(eqBands[i].getFrequency()));
            bandXml->setAttribute("gain", static_cast<double>(eqBands[i].getGain()));
            bandXml->setAttribute("q", static_cast<double>(eqBands[i].getQ()));
            bandXml->setAttribute("enabled", eqBands[i].isEnabled());
        }

        return xml;
    }

    void restoreFromStateXml(const juce::XmlElement& xml)
    {
        if (xml.getTagName() != "EQ8State")
            return;

        currentPresetName = xml.getStringAttribute("presetName", "Default");

        for (auto* bandXml : xml.getChildIterator())
        {
            if (bandXml->getTagName() != "Band")
                continue;

            int index = bandXml->getIntAttribute("index", -1);
            if (index < 0 || index >= 8)
                continue;

            eqBands[index].setType(static_cast<EQBand::FilterType>(bandXml->getIntAttribute("type", 1)));
            eqBands[index].setFrequency(static_cast<float>(bandXml->getDoubleAttribute("frequency", 1000.0)));
            eqBands[index].setGain(static_cast<float>(bandXml->getDoubleAttribute("gain", 0.0)));
            eqBands[index].setQ(static_cast<float>(bandXml->getDoubleAttribute("q", 0.707)));
            eqBands[index].setEnabled(bandXml->getBoolAttribute("enabled", true));
        }
    }

private:
    std::array<EQBand, 8>& eqBands;
    juce::String currentPresetName = "Default";
    juce::File presetDirectory;
    juce::StringArray factoryPresetNames;

    void initFactoryPresets()
    {
        factoryPresetNames.add("Default");
        factoryPresetNames.add("Vocal Presence");
        factoryPresetNames.add("Bass Boost");
        factoryPresetNames.add("Bright");
        factoryPresetNames.add("Warm");
        factoryPresetNames.add("Smiley Curve");
        factoryPresetNames.add("Mid Scoop");
    }

    void applyFactoryPreset(int index)
    {
        resetToDefault();

        switch (index)
        {
            case 0: break; // Default - already reset
            case 1: // Vocal Presence
                eqBands[4].setGain(3.0f);
                eqBands[5].setGain(4.0f);
                eqBands[6].setGain(2.0f);
                break;
            case 2: // Bass Boost
                eqBands[0].setGain(6.0f);
                eqBands[1].setGain(3.0f);
                break;
            case 3: // Bright
                eqBands[6].setGain(3.0f);
                eqBands[7].setGain(5.0f);
                break;
            case 4: // Warm
                eqBands[0].setGain(3.0f);
                eqBands[1].setGain(2.0f);
                eqBands[7].setGain(-3.0f);
                break;
            case 5: // Smiley Curve
                eqBands[0].setGain(4.0f);
                eqBands[1].setGain(2.0f);
                eqBands[3].setGain(-3.0f);
                eqBands[4].setGain(-2.0f);
                eqBands[6].setGain(2.0f);
                eqBands[7].setGain(4.0f);
                break;
            case 6: // Mid Scoop
                eqBands[3].setGain(-5.0f);
                eqBands[4].setGain(-3.0f);
                break;
        }
    }

    void resetToDefault()
    {
        float defaultFreqs[] = { 80.0f, 150.0f, 400.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f, 12000.0f };
        float defaultQs[] = { 0.707f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.707f };
        EQBand::FilterType defaultTypes[] = {
            EQBand::LowShelf, EQBand::Peak, EQBand::Peak, EQBand::Peak,
            EQBand::Peak, EQBand::Peak, EQBand::Peak, EQBand::HighShelf
        };

        for (int i = 0; i < 8; ++i)
        {
            eqBands[i].setType(defaultTypes[i]);
            eqBands[i].setFrequency(defaultFreqs[i]);
            eqBands[i].setGain(0.0f);
            eqBands[i].setQ(defaultQs[i]);
            eqBands[i].setEnabled(true);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
