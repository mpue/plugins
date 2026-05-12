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
                   .getChildFile ("PM-1")
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
                       float texture, float warmth, float brightness,
                       float movement, float lushness, float space,
                       float delay, float width, float drive,
                       float attack, float release, float volume,
                       int   octave,  int character, float lfoRate)
    {
        Preset p;
        p.name = std::move (name);
        p.isFactory = true;
        p.values["texture"]    = texture;
        p.values["warmth"]     = warmth;
        p.values["brightness"] = brightness;
        p.values["movement"]   = movement;
        p.values["lushness"]   = lushness;
        p.values["space"]      = space;
        p.values["delay"]      = delay;
        p.values["width"]      = width;
        p.values["drive"]      = drive;
        p.values["attack"]     = attack;
        p.values["release"]    = release;
        p.values["volume"]     = volume;
        p.values["octave"]     = (float) octave;
        p.values["character"]  = (float) character;
        p.values["lforate"]    = lfoRate;
        factoryPresets.push_back (std::move (p));
    };

    //   name              tex   warm  bright  mov   lush  space  dly   width drive  att   rel   vol(dB) oct char rate
    add ("Velvet Pad",      0.45f, 0.55f, 0.50f, 0.30f, 0.55f, 0.50f, 0.18f, 0.85f, 0.18f, 1.20f, 2.40f, -6.0f,  0, 0, 0.30f);
    add ("Cinematic Sky",   0.65f, 0.50f, 0.60f, 0.55f, 0.75f, 0.78f, 0.35f, 0.95f, 0.20f, 2.40f, 4.80f, -7.0f,  0, 0, 0.22f);
    add ("Warm Strings",    0.30f, 0.65f, 0.45f, 0.20f, 0.45f, 0.55f, 0.15f, 0.80f, 0.15f, 0.90f, 1.80f, -6.5f,  0, 2, 0.30f);
    add ("Glass Cathedral", 0.55f, 0.40f, 0.78f, 0.45f, 0.65f, 0.80f, 0.25f, 0.95f, 0.10f, 1.80f, 5.50f, -8.0f,  1, 4, 0.18f);
    add ("Choir Of Light",  0.40f, 0.50f, 0.55f, 0.25f, 0.55f, 0.65f, 0.20f, 0.90f, 0.12f, 1.50f, 3.20f, -7.0f,  0, 3, 0.25f);
    add ("Dream Weaver",    0.70f, 0.45f, 0.68f, 0.65f, 0.85f, 0.75f, 0.45f, 1.00f, 0.18f, 2.80f, 6.00f, -7.5f,  0, 0, 0.20f);
    add ("Solar Drift",     0.55f, 0.50f, 0.62f, 0.40f, 0.70f, 0.72f, 0.30f, 0.95f, 0.20f, 2.00f, 4.50f, -7.0f,  0, 5, 0.16f);
    add ("Underwater",      0.50f, 0.70f, 0.30f, 0.50f, 0.80f, 0.70f, 0.40f, 0.95f, 0.15f, 1.80f, 4.20f, -7.5f, -1, 0, 0.18f);
    add ("Crystal Wash",    0.50f, 0.40f, 0.85f, 0.30f, 0.75f, 0.80f, 0.30f, 0.95f, 0.08f, 1.20f, 4.00f, -8.0f,  1, 4, 0.30f);
    add ("Lush Vintage",    0.65f, 0.60f, 0.45f, 0.30f, 0.70f, 0.50f, 0.20f, 0.85f, 0.30f, 1.40f, 2.60f, -6.0f,  0, 0, 0.30f);
    add ("Endless Horizon", 0.80f, 0.50f, 0.55f, 0.55f, 0.85f, 0.85f, 0.55f, 1.00f, 0.18f, 3.80f, 8.50f, -7.5f,  0, 5, 0.12f);
    add ("Ambient Pulse",   0.55f, 0.55f, 0.50f, 0.70f, 0.65f, 0.62f, 0.30f, 0.90f, 0.20f, 1.60f, 3.00f, -6.5f,  0, 0, 0.85f);
    add ("Snow Field",      0.45f, 0.55f, 0.42f, 0.18f, 0.55f, 0.85f, 0.25f, 0.95f, 0.10f, 2.40f, 6.50f, -8.5f,  0, 5, 0.14f);
    add ("Deep Pulse",      0.30f, 0.75f, 0.30f, 0.50f, 0.40f, 0.45f, 0.15f, 0.70f, 0.40f, 0.50f, 1.50f, -5.5f, -1, 0, 0.65f);
    add ("Spectral Pad",    0.70f, 0.40f, 0.70f, 0.65f, 0.80f, 0.70f, 0.50f, 1.00f, 0.15f, 2.20f, 5.20f, -7.5f,  0, 4, 0.22f);
    add ("Old Tape Choir",  0.45f, 0.65f, 0.40f, 0.30f, 0.60f, 0.55f, 0.18f, 0.80f, 0.35f, 1.20f, 2.80f, -6.0f,  0, 3, 0.28f);
    add ("Dust And Stars",  0.62f, 0.45f, 0.68f, 0.50f, 0.80f, 0.78f, 0.40f, 1.00f, 0.18f, 2.80f, 6.50f, -8.0f,  0, 5, 0.18f);
    add ("Soft Bell Pad",   0.55f, 0.40f, 0.78f, 0.30f, 0.65f, 0.65f, 0.25f, 0.92f, 0.10f, 1.00f, 3.50f, -7.5f,  1, 4, 0.30f);
    add ("Neon Twilight",   0.60f, 0.55f, 0.55f, 0.45f, 0.70f, 0.65f, 0.40f, 0.95f, 0.25f, 1.40f, 3.00f, -6.5f,  0, 1, 0.40f);
    add ("Default",         0.50f, 0.55f, 0.55f, 0.35f, 0.55f, 0.45f, 0.18f, 0.85f, 0.20f, 1.20f, 2.20f, -6.0f,  0, 0, 0.35f);
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
        if (xml == nullptr || ! xml->hasTagName ("PM1Preset"))
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

    juce::XmlElement xml ("PM1Preset");
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
