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
                   .getChildFile ("GS-1")
                   .getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

void PresetManager::initialise()
{
    buildFactoryPresets();
    rescanUserPresets();
    // Note: we deliberately do NOT apply a preset here. The host (or
    // setStateInformation) is responsible for the current parameter values.
    // The editor reads the preset list and shows the current name (which
    // may stay empty until the user explicitly loads one).
    sendChangeMessage();
}

void PresetManager::buildFactoryPresets()
{
    factoryPresets.clear();

    auto add = [this] (juce::String name,
                       int   source,
                       float position, float spray,  float grainMs, float density,
                       float pitch,    float pSpray, float reverse, float pan,
                       float movement, float attack, float release,
                       float tone,     float drive,
                       float lush,     float space,  float width,
                       float volDb,    int   octave, float lfo)
    {
        Preset p;
        p.name = std::move (name);
        p.isFactory = true;
        p.values["source"]      = (float) source;
        p.values["position"]    = position;
        p.values["spray"]       = spray;
        p.values["grainsize"]   = grainMs;
        p.values["density"]     = density;
        p.values["pitch"]       = pitch;
        p.values["pitchspray"]  = pSpray;
        p.values["reverse"]     = reverse;
        p.values["panspread"]   = pan;
        p.values["movement"]    = movement;
        p.values["attack"]      = attack;
        p.values["release"]     = release;
        p.values["tone"]        = tone;
        p.values["drive"]       = drive;
        p.values["lushness"]    = lush;
        p.values["space"]       = space;
        p.values["width"]       = width;
        p.values["volume"]      = volDb;
        p.values["octave"]      = (float) octave;
        p.values["lforate"]     = lfo;
        factoryPresets.push_back (std::move (p));
    };

    //   name              src  pos  spray gMs  dens  pitch pSpr rev  pan  mov  att   rel   tone  drv  lush spc wid   vol   oct lfo
    add ("Velvet Cloud",     0, 0.30f,0.20f, 140, 30.0f, 0.0f,0.04f,0.10f,0.65f,0.30f,0.80f,2.20f,0.55f,0.10f,0.55f,0.55f,0.85f, -7.0f,  0, 0.30f);
    add ("Cathedral Voices", 2, 0.40f,0.35f, 220, 25.0f, 0.0f,0.03f,0.05f,0.85f,0.45f,1.80f,4.50f,0.45f,0.08f,0.65f,0.85f,1.00f, -8.0f,  0, 0.20f);
    add ("Glass Shimmer",    4, 0.50f,0.30f,  80, 80.0f,12.0f,0.05f,0.20f,0.95f,0.55f,0.40f,3.00f,0.78f,0.05f,0.70f,0.78f,1.00f, -9.0f,  1, 0.25f);
    add ("Bell Rain",        3, 0.55f,0.50f,  60, 70.0f, 0.0f,0.10f,0.25f,0.95f,0.60f,0.20f,2.20f,0.65f,0.10f,0.55f,0.62f,0.95f, -8.0f,  0, 0.40f);
    add ("Strings Garden",   1, 0.35f,0.25f, 200, 28.0f, 0.0f,0.02f,0.05f,0.65f,0.30f,1.40f,3.20f,0.50f,0.18f,0.55f,0.55f,0.90f, -6.5f,  0, 0.25f);
    add ("Air Drift",        5, 0.50f,0.40f, 180, 22.0f, 0.0f,0.03f,0.10f,0.85f,0.55f,2.40f,5.50f,0.55f,0.05f,0.70f,0.80f,1.00f, -8.5f,  0, 0.18f);
    add ("Twilight Vox",     0, 0.45f,0.30f, 160, 25.0f,-12.0f,0.06f,0.10f,0.85f,0.40f,1.20f,3.20f,0.42f,0.10f,0.65f,0.65f,0.95f, -7.0f, -1, 0.22f);
    add ("Frozen Aurora",    4, 0.40f,0.50f, 100, 50.0f, 0.0f,0.20f,0.15f,1.00f,0.65f,0.80f,5.00f,0.78f,0.06f,0.75f,0.88f,1.00f, -8.5f,  1, 0.18f);
    add ("Underwater Choir", 2, 0.30f,0.40f, 240, 18.0f,-12.0f,0.04f,0.10f,0.90f,0.55f,2.00f,4.80f,0.30f,0.15f,0.60f,0.78f,0.95f, -7.5f, -1, 0.20f);
    add ("Bowed Whispers",   1, 0.45f,0.40f, 280, 14.0f, 0.0f,0.04f,0.10f,0.75f,0.40f,2.80f,5.50f,0.50f,0.10f,0.50f,0.60f,0.90f, -7.5f,  0, 0.18f);
    add ("Cinematic Sky",    2, 0.50f,0.40f, 250, 18.0f, 7.0f,0.04f,0.05f,0.95f,0.50f,3.20f,7.00f,0.55f,0.10f,0.65f,0.85f,1.00f, -8.0f,  0, 0.16f);
    add ("Crystal Bells",    3, 0.40f,0.35f,  50, 90.0f,12.0f,0.05f,0.10f,0.90f,0.50f,0.20f,3.50f,0.78f,0.04f,0.55f,0.78f,1.00f, -9.0f,  1, 0.25f);
    add ("Sub Hum Pad",      0, 0.55f,0.20f, 200, 22.0f,-12.0f,0.02f,0.05f,0.55f,0.20f,2.20f,4.00f,0.30f,0.20f,0.45f,0.50f,0.85f, -6.0f, -1, 0.20f);
    add ("Hyperspace",       4, 0.55f,0.65f, 120, 60.0f, 0.0f,0.30f,0.30f,1.00f,0.85f,1.00f,4.50f,0.65f,0.15f,0.80f,0.85f,1.00f, -8.5f,  0, 0.18f);
    add ("Vocal Rain",       0, 0.40f,0.55f,  80, 70.0f, 0.0f,0.10f,0.20f,0.95f,0.55f,0.40f,2.50f,0.50f,0.10f,0.55f,0.65f,0.95f, -7.5f,  0, 0.30f);
    add ("Default",          0, 0.30f,0.20f, 120, 30.0f, 0.0f,0.00f,0.15f,0.65f,0.30f,0.50f,1.80f,0.50f,0.15f,0.55f,0.55f,0.85f, -6.0f,  0, 0.30f);
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
        if (xml == nullptr || ! xml->hasTagName ("GS1Preset"))
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

bool PresetManager::saveUserPreset (const juce::String& name)
{
    if (name.isEmpty()) return false;

    auto file = getUserPresetsFolder().getChildFile (sanitizeFilename (name) + ".xml");

    juce::XmlElement xml ("GS1Preset");
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
