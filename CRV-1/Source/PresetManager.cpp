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
                   .getChildFile ("CRV-1")
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
        if (onIRChangeRequest)
            onIRChangeRequest (factoryPresets.front().irName);

        applyPreset (factoryPresets.front());
        currentPresetName = factoryPresets.front().name;
        currentIsFactory  = true;
        sendChangeMessage();
    }
}

void PresetManager::buildFactoryPresets()
{
    factoryPresets.clear();

    auto add = [this] (juce::String name, juce::String ir,
                       float size, float decay, float predelay,
                       float lowCut, float highCut,
                       float modulation, float width,
                       float mix, float outputDb)
    {
        Preset p;
        p.name = std::move (name);
        p.isFactory = true;
        p.irName = std::move (ir);
        p.values["size"]       = size;
        p.values["decay"]      = decay;
        p.values["predelay"]   = predelay;
        p.values["lowcut"]     = lowCut;
        p.values["highcut"]    = highCut;
        p.values["modulation"] = modulation;
        p.values["width"]      = width;
        p.values["mix"]        = mix;
        p.values["output"]     = outputDb;
        factoryPresets.push_back (std::move (p));
    };

    //    name                  IR                 size  decay  pre   loCut hiCut  mod   width mix  out
    add ("Default",             "Concert Hall",    0.50f, 0.50f, 20.f,  90.f, 10000.f, 0.25f, 1.00f, 0.30f,  0.0f);
    add ("Grand Cathedral",     "Grand Cathedral", 0.65f, 0.70f, 40.f,  70.f,  8500.f, 0.30f, 1.05f, 0.32f,  0.0f);
    add ("Symphony Hall",       "Symphony Hall",   0.55f, 0.55f, 22.f,  90.f,  9500.f, 0.22f, 1.00f, 0.30f,  0.0f);
    add ("Lush Opera",          "Opera House",     0.60f, 0.65f, 25.f,  85.f,  9000.f, 0.30f, 1.10f, 0.32f,  0.0f);
    add ("Vintage Plate",       "Vintage Plate",   0.45f, 0.55f, 12.f, 140.f, 11000.f, 0.35f, 0.95f, 0.30f,  0.0f);
    add ("Bright Plate",        "Studio Plate",    0.45f, 0.50f,  6.f, 180.f, 13000.f, 0.30f, 1.00f, 0.30f,  0.0f);
    add ("Warm Chamber",        "Wood Chamber",    0.50f, 0.55f, 14.f, 110.f,  8500.f, 0.25f, 0.95f, 0.30f,  0.0f);
    add ("Crystal Chamber",     "Bright Chamber",  0.50f, 0.50f, 10.f, 130.f, 11500.f, 0.28f, 1.00f, 0.28f,  0.0f);
    add ("Tight Drum Room",     "Drum Room",       0.40f, 0.40f,  4.f, 110.f, 10000.f, 0.18f, 1.00f, 0.25f,  0.0f);
    add ("Air Around Vocals",   "Vocal Room",      0.50f, 0.45f, 18.f, 200.f, 12000.f, 0.30f, 1.00f, 0.22f,  0.0f);
    add ("Ambient Cavern",      "Ambient Cavern",  0.70f, 0.75f, 55.f,  60.f,  8000.f, 0.45f, 1.10f, 0.40f,  0.0f);
    add ("Infinite Bloom",      "Infinite Bloom",  0.80f, 0.90f, 65.f,  55.f,  7500.f, 0.55f, 1.10f, 0.45f,  0.0f);
    add ("Dark Cellar",         "Wood Chamber",    0.55f, 0.60f, 20.f,  70.f,  4500.f, 0.20f, 0.90f, 0.28f,  0.0f);
    add ("Synth Pad Bath",      "Ambient Cavern",  0.75f, 0.80f, 50.f,  60.f,  7000.f, 0.60f, 1.15f, 0.42f,  0.0f);
    add ("Snare Slap",          "Drum Room",       0.30f, 0.30f,  3.f, 150.f,  9500.f, 0.15f, 1.00f, 0.20f,  0.0f);
    add ("Vintage 80s Vocal",   "Studio Plate",    0.55f, 0.55f, 25.f, 120.f, 12000.f, 0.40f, 1.05f, 0.30f,  0.0f);
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
        if (xml == nullptr || ! xml->hasTagName ("CRV1Preset"))
            continue;

        Preset p;
        p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
        p.irName = xml->getStringAttribute ("ir", "");
        p.isFactory = false;

        for (int i = 0; i < xml->getNumAttributes(); ++i)
        {
            auto key = xml->getAttributeName (i);
            if (key == "name" || key == "ir") continue;
            auto val = xml->getDoubleAttribute (key);
            p.values[key] = (float) val;
        }
        userPresets.push_back (std::move (p));
    }

    sendChangeMessage();
}

void PresetManager::applyPreset (const Preset& p)
{
    if (! p.irName.isEmpty() && onIRChangeRequest)
        onIRChangeRequest (p.irName);

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
    applyPreset (factoryPresets[(size_t) index]);
    currentPresetName = factoryPresets[(size_t) index].name;
    currentIsFactory  = true;
    sendChangeMessage();
}

void PresetManager::loadUserPreset (int index)
{
    if (index < 0 || index >= (int) userPresets.size()) return;
    applyPreset (userPresets[(size_t) index]);
    currentPresetName = userPresets[(size_t) index].name;
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

bool PresetManager::saveUserPreset (const juce::String& name, const juce::String& irName)
{
    if (name.isEmpty()) return false;

    auto file = getUserPresetsFolder().getChildFile (sanitizeFilename (name) + ".xml");

    juce::XmlElement xml ("CRV1Preset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("ir",   irName);

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
            if (factoryPresets[(size_t) i].name == currentPresetName) { currentIndex = i; break; }
    }
    else
    {
        for (int i = 0; i < userCount; ++i)
            if (userPresets[(size_t) i].name == currentPresetName) { currentIndex = factoryCount + i; break; }
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
            if (factoryPresets[(size_t) i].name == currentPresetName) { currentIndex = i; break; }
    }
    else
    {
        for (int i = 0; i < userCount; ++i)
            if (userPresets[(size_t) i].name == currentPresetName) { currentIndex = factoryCount + i; break; }
    }

    int prev = (currentIndex - 1 + total) % total;
    if (prev < factoryCount) loadFactoryPreset (prev);
    else                     loadUserPreset (prev - factoryCount);
}
