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
                   .getChildFile ("TR-1")
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
                       float drive, float crunch, float tone, float body,
                       float texture, float motion, float motionRate,
                       float age, float width, float mix, float outputDb,
                       int character)
    {
        Preset p;
        p.name = std::move (name);
        p.isFactory = true;
        p.values["drive"]      = drive;
        p.values["crunch"]     = crunch;
        p.values["tone"]       = tone;
        p.values["body"]       = body;
        p.values["texture"]    = texture;
        p.values["motion"]     = motion;
        p.values["motionRate"] = motionRate;
        p.values["age"]        = age;
        p.values["width"]      = width;
        p.values["mix"]        = mix;
        p.values["output"]     = outputDb;
        p.values["character"]  = (float) character;
        factoryPresets.push_back (std::move (p));
    };

    // Character codes follow TrashEngine::Character:
    // 0 Tube, 1 Tape, 2 Fuzz, 3 Crush, 4 Telephone, 5 Radio, 6 Mangler, 7 VintageAmp

    //   name                drive crunch tone  body  text   mot  rate  age   width mix  out   char
    add ("Default",          0.45f, 0.00f, 0.50f, 0.50f, 0.00f, 0.00f, 0.50f, 0.20f, 1.0f, 1.0f, -2.0f, 0);
    add ("Velvet Tube",      0.55f, 0.00f, 0.55f, 0.65f, 0.05f, 0.00f, 0.30f, 0.15f, 1.0f, 1.0f, -3.0f, 0);
    add ("Smoking Amp",      0.78f, 0.00f, 0.45f, 0.70f, 0.10f, 0.00f, 0.30f, 0.30f, 1.0f, 1.0f, -4.0f, 7);
    add ("Wall of Fuzz",     0.85f, 0.00f, 0.50f, 0.80f, 0.05f, 0.00f, 0.30f, 0.25f, 1.0f, 1.0f, -6.0f, 2);
    add ("Cassette Glory",   0.50f, 0.20f, 0.45f, 0.55f, 0.30f, 0.10f, 0.20f, 0.45f, 0.85f, 1.0f, -2.0f, 1);
    add ("VHS Bedroom",      0.40f, 0.30f, 0.40f, 0.50f, 0.40f, 0.18f, 0.18f, 0.55f, 0.80f, 1.0f, -2.0f, 1);
    add ("Telephone Call",   0.65f, 0.10f, 0.55f, 0.20f, 0.15f, 0.00f, 0.30f, 0.30f, 0.30f, 1.0f, -3.0f, 4);
    add ("AM Radio",         0.55f, 0.15f, 0.55f, 0.30f, 0.30f, 0.05f, 0.30f, 0.40f, 0.40f, 1.0f, -3.0f, 5);
    add ("Lo-Fi Hip-Hop",    0.40f, 0.45f, 0.42f, 0.65f, 0.20f, 0.20f, 0.18f, 0.55f, 0.85f, 1.0f, -3.0f, 3);
    add ("Bit Mash",         0.30f, 0.85f, 0.50f, 0.55f, 0.10f, 0.00f, 0.30f, 0.20f, 1.0f, 1.0f, -4.0f, 3);
    add ("Vintage Vinyl",    0.30f, 0.10f, 0.40f, 0.55f, 0.55f, 0.04f, 0.05f, 0.50f, 0.85f, 1.0f, -2.0f, 1);
    add ("Drum Smasher",     0.85f, 0.00f, 0.55f, 0.85f, 0.00f, 0.00f, 0.30f, 0.10f, 1.0f, 1.0f, -6.0f, 7);
    add ("Bass Saturator",   0.55f, 0.00f, 0.40f, 0.85f, 0.00f, 0.00f, 0.30f, 0.20f, 0.6f, 1.0f, -3.0f, 0);
    add ("Vocal Crunch",     0.40f, 0.00f, 0.55f, 0.50f, 0.00f, 0.00f, 0.30f, 0.15f, 1.0f, 0.55f, -2.0f, 0);
    add ("Mangled World",    0.65f, 0.20f, 0.55f, 0.55f, 0.05f, 0.35f, 0.55f, 0.35f, 1.0f, 1.0f, -5.0f, 6);
    add ("Synth Wrecker",    0.75f, 0.30f, 0.60f, 0.65f, 0.05f, 0.45f, 0.65f, 0.20f, 1.0f, 1.0f, -5.0f, 6);
    add ("Broken Speaker",   0.60f, 0.30f, 0.30f, 0.40f, 0.30f, 0.15f, 0.20f, 0.75f, 0.80f, 1.0f, -3.0f, 7);
    add ("Garage Tape",      0.55f, 0.10f, 0.45f, 0.65f, 0.25f, 0.08f, 0.18f, 0.50f, 0.90f, 1.0f, -3.0f, 1);
    add ("Hot Drums Bus",    0.45f, 0.00f, 0.55f, 0.75f, 0.00f, 0.00f, 0.30f, 0.10f, 1.0f, 0.65f, -1.5f, 0);
    add ("Cinematic Wash",   0.55f, 0.05f, 0.50f, 0.60f, 0.20f, 0.30f, 0.45f, 0.40f, 1.0f, 0.50f, -3.0f, 1);
    add ("Mauler",           0.95f, 0.10f, 0.55f, 0.90f, 0.05f, 0.00f, 0.30f, 0.25f, 1.0f, 1.0f, -8.0f, 2);
    add ("Subtle Glue",      0.20f, 0.00f, 0.50f, 0.55f, 0.00f, 0.00f, 0.30f, 0.10f, 1.0f, 0.40f, -1.0f, 0);
    add ("Detuned Wobble",   0.45f, 0.20f, 0.55f, 0.55f, 0.10f, 0.65f, 0.35f, 0.30f, 1.0f, 1.0f, -3.0f, 6);
    add ("Old Movie",        0.45f, 0.20f, 0.40f, 0.45f, 0.55f, 0.10f, 0.18f, 0.65f, 0.50f, 1.0f, -3.0f, 1);
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
        if (xml == nullptr || ! xml->hasTagName ("TR1Preset"))
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

    juce::XmlElement xml ("TR1Preset");
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
