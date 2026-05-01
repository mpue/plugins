/*
  ==============================================================================

    PresetManager.cpp

  ==============================================================================
*/

#include "PresetManager.h"

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s)
    : apvts (s)
{
}

juce::File PresetManager::getUserPresetsFolder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RV-1")
                   .getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

void PresetManager::initialise()
{
    buildFactoryPresets();
    rescanUserPresets();

    if (! factoryPresets.empty())
    {
        applyPreset (factoryPresets.front());
        currentPresetName = factoryPresets.front().name;
        currentIsFactory  = true;
        sendChangeMessage();
    }
}

void PresetManager::buildFactoryPresets()
{
    factoryPresets.clear();

    auto add = [this] (juce::String name,
                       float size, float decay, float damping, float predelay,
                       float modulation, float diffusion, float width,
                       float mix, float lowCut, float highCut, int character)
    {
        Preset p;
        p.name = std::move (name);
        p.isFactory = true;
        p.values["size"]       = size;
        p.values["decay"]      = decay;
        p.values["damping"]    = damping;
        p.values["predelay"]   = predelay;
        p.values["modulation"] = modulation;
        p.values["diffusion"]  = diffusion;
        p.values["width"]      = width;
        p.values["mix"]        = mix;
        p.values["lowcut"]     = lowCut;
        p.values["highcut"]    = highCut;
        p.values["character"]  = (float) character;
        factoryPresets.push_back (std::move (p));
    };

    //   name              size  decay damp  pre   mod   diff  width mix   loCut  hiCut   char
    add ("Default",         0.55f, 0.55f, 0.45f, 20.f, 0.30f, 0.72f, 1.00f, 0.30f,  90.f, 9000.f, 0);
    add ("Grand Hall",      0.85f, 0.78f, 0.40f, 35.f, 0.35f, 0.78f, 1.00f, 0.32f, 100.f, 8500.f, 0);
    add ("Concert Hall",    0.75f, 0.70f, 0.45f, 25.f, 0.25f, 0.75f, 0.95f, 0.30f,  90.f, 9000.f, 0);
    add ("Cathedral",       0.95f, 0.92f, 0.55f, 50.f, 0.30f, 0.82f, 1.00f, 0.35f,  80.f, 7000.f, 4);
    add ("Vintage Plate",   0.45f, 0.65f, 0.30f, 10.f, 0.45f, 0.85f, 0.90f, 0.32f, 120.f,10000.f, 1);
    add ("Bright Plate",    0.40f, 0.60f, 0.20f,  5.f, 0.40f, 0.85f, 0.95f, 0.30f, 150.f,12000.f, 1);
    add ("Studio Room",     0.35f, 0.40f, 0.50f,  8.f, 0.20f, 0.70f, 0.90f, 0.22f, 100.f, 8000.f, 2);
    add ("Drum Room",       0.30f, 0.35f, 0.55f,  5.f, 0.15f, 0.68f, 1.00f, 0.25f, 110.f, 7500.f, 2);
    add ("Tight Chamber",   0.45f, 0.50f, 0.45f, 12.f, 0.25f, 0.74f, 0.85f, 0.28f, 100.f, 8500.f, 3);
    add ("Ambient Wash",    0.85f, 0.85f, 0.55f, 80.f, 0.55f, 0.80f, 1.00f, 0.42f,  60.f, 7000.f, 0);
    add ("Vocal Air",       0.55f, 0.55f, 0.35f, 18.f, 0.30f, 0.70f, 1.00f, 0.22f, 200.f,11000.f, 0);
    add ("Lush Vocal",      0.65f, 0.65f, 0.40f, 30.f, 0.45f, 0.78f, 1.00f, 0.30f, 120.f,10000.f, 0);
    add ("Spring Tank",     0.30f, 0.55f, 0.30f, 10.f, 0.85f, 0.75f, 0.85f, 0.32f, 150.f, 6000.f, 5);
    add ("Surf Spring",     0.25f, 0.45f, 0.25f,  5.f, 0.95f, 0.78f, 0.90f, 0.30f, 200.f, 6500.f, 5);
    add ("Synth Pad Bath",  0.90f, 0.88f, 0.60f, 60.f, 0.70f, 0.82f, 1.00f, 0.45f,  60.f, 8000.f, 0);
    add ("Dark Cellar",     0.55f, 0.60f, 0.80f, 25.f, 0.20f, 0.72f, 0.90f, 0.28f,  80.f, 4500.f, 3);
    add ("Endless",         1.00f, 0.98f, 0.30f, 30.f, 0.50f, 0.84f, 1.00f, 0.45f,  70.f, 9500.f, 4);
    add ("Snare Slap",      0.25f, 0.30f, 0.40f,  3.f, 0.10f, 0.66f, 1.00f, 0.20f, 150.f, 9000.f, 2);
    add ("80's Gated",      0.45f, 0.40f, 0.35f, 12.f, 0.20f, 0.78f, 1.00f, 0.30f, 120.f, 9500.f, 1);
    add ("Soft Bloom",      0.65f, 0.70f, 0.55f, 40.f, 0.40f, 0.78f, 1.00f, 0.25f, 100.f, 8000.f, 0);
}

void PresetManager::rescanUserPresets()
{
    userPresets.clear();
    auto dir = getUserPresetsFolder();
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, "*.xml");
    files.sort();

    for (auto& f : files)
    {
        std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (f));
        if (xml == nullptr || ! xml->hasTagName ("RV1Preset"))
            continue;

        Preset p;
        p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
        p.isFactory = false;

        for (int i = 0; i < xml->getNumAttributes(); ++i)
        {
            auto key = xml->getAttributeName (i);
            if (key == "name") continue;
            auto val = xml->getDoubleAttribute (key);
            p.values[key] = (float) val;
        }
        userPresets.push_back (std::move (p));
    }

    sendChangeMessage();
}

void PresetManager::applyPreset (const Preset& p)
{
    for (auto& kv : p.values)
    {
        if (auto* param = apvts.getParameter (kv.first))
        {
            auto range = apvts.getParameterRange (kv.first);
            float normalised = range.convertTo0to1 (kv.second);
            param->beginChangeGesture();
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
            param->endChangeGesture();
        }
    }
}

void PresetManager::loadFactoryPreset (int index)
{
    if (index < 0 || index >= (int) factoryPresets.size()) return;
    applyPreset (factoryPresets[index]);
    currentPresetName = factoryPresets[index].name;
    currentIsFactory  = true;
    sendChangeMessage();
}

void PresetManager::loadUserPreset (int index)
{
    if (index < 0 || index >= (int) userPresets.size()) return;
    applyPreset (userPresets[index]);
    currentPresetName = userPresets[index].name;
    currentIsFactory  = false;
    sendChangeMessage();
}

void PresetManager::loadPresetByName (const juce::String& name)
{
    for (size_t i = 0; i < factoryPresets.size(); ++i)
        if (factoryPresets[i].name == name) { loadFactoryPreset ((int) i); return; }
    for (size_t i = 0; i < userPresets.size(); ++i)
        if (userPresets[i].name == name) { loadUserPreset ((int) i); return; }
}

juce::String PresetManager::sanitizeFilename (const juce::String& name)
{
    juce::String out;
    for (auto c : name)
    {
        if (juce::CharacterFunctions::isLetterOrDigit (c) || c == ' ' || c == '_' || c == '-')
            out += juce::String::charToString (c);
        else
            out += "_";
    }
    return out.trim();
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    if (name.isEmpty()) return false;

    auto file = getUserPresetsFolder().getChildFile (sanitizeFilename (name) + ".xml");

    juce::XmlElement xml ("RV1Preset");
    xml.setAttribute ("name", name);

    for (auto* p : apvts.processor.getParameters())
    {
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
        {
            auto pid = withID->getParameterID();
            if (auto* param = apvts.getParameter (pid))
            {
                auto range = apvts.getParameterRange (pid);
                float val = range.convertFrom0to1 (param->getValue());
                xml.setAttribute (pid, (double) val);
            }
        }
    }

    if (! xml.writeTo (file))
        return false;

    rescanUserPresets();
    currentPresetName = name;
    currentIsFactory  = false;
    sendChangeMessage();
    return true;
}

bool PresetManager::deleteUserPreset (const juce::String& name)
{
    auto file = getUserPresetsFolder().getChildFile (sanitizeFilename (name) + ".xml");
    if (! file.existsAsFile()) return false;
    bool ok = file.deleteFile();
    if (ok) rescanUserPresets();
    return ok;
}

void PresetManager::nextPreset()
{
    const int factoryCount = (int) factoryPresets.size();
    const int userCount    = (int) userPresets.size();
    const int total = factoryCount + userCount;
    if (total == 0) return;

    int currentIndex = -1;
    if (currentIsFactory)
    {
        for (int i = 0; i < factoryCount; ++i)
            if (factoryPresets[i].name == currentPresetName) { currentIndex = i; break; }
    }
    else
    {
        for (int i = 0; i < userCount; ++i)
            if (userPresets[i].name == currentPresetName) { currentIndex = factoryCount + i; break; }
    }

    int next = (currentIndex + 1) % total;
    if (next < factoryCount) loadFactoryPreset (next);
    else                     loadUserPreset (next - factoryCount);
}

void PresetManager::previousPreset()
{
    const int factoryCount = (int) factoryPresets.size();
    const int userCount    = (int) userPresets.size();
    const int total = factoryCount + userCount;
    if (total == 0) return;

    int currentIndex = 0;
    if (currentIsFactory)
    {
        for (int i = 0; i < factoryCount; ++i)
            if (factoryPresets[i].name == currentPresetName) { currentIndex = i; break; }
    }
    else
    {
        for (int i = 0; i < userCount; ++i)
            if (userPresets[i].name == currentPresetName) { currentIndex = factoryCount + i; break; }
    }

    int prev = (currentIndex - 1 + total) % total;
    if (prev < factoryCount) loadFactoryPreset (prev);
    else                     loadUserPreset (prev - factoryCount);
}
