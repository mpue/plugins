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
                   .getChildFile ("PS-1")
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
                       float pitch, float fine, float mix,
                       float feedback, float formant, float width,
                       float drive, float lowCut, float highCut,
                       int character, float quality)
    {
        Preset p;
        p.name = std::move (name);
        p.isFactory = true;
        p.values["pitch"]    = pitch;
        p.values["fine"]     = fine;
        p.values["mix"]      = mix;
        p.values["feedback"] = feedback;
        p.values["formant"]  = formant;
        p.values["width"]    = width;
        p.values["drive"]    = drive;
        p.values["lowcut"]   = lowCut;
        p.values["highcut"]  = highCut;
        p.values["character"] = (float) character;
        p.values["quality"]  = quality;
        factoryPresets.push_back (std::move (p));
    };

    //                       pitch  fine  mix    fb    fmnt  width drive lowCut hiCut  char  qual
    add ("Default",           0.0f,  0.0f, 1.00f, 0.00f, 0.00f, 0.50f, 0.00f,   30.f, 18000.f, 0, 0.60f);
    add ("Octave Up",        12.0f,  0.0f, 0.65f, 0.00f, 0.50f, 0.40f, 0.00f,   60.f, 12000.f, 0, 0.70f);
    add ("Octave Down",     -12.0f,  0.0f, 0.65f, 0.00f, 0.40f, 0.50f, 0.05f,   30.f, 14000.f, 0, 0.75f);
    add ("Two Octaves Up",   24.0f,  0.0f, 0.55f, 0.00f, 0.55f, 0.45f, 0.00f,  120.f, 11000.f, 4, 0.85f);
    add ("Sub Octave",      -12.0f,  0.0f, 0.55f, 0.00f, 0.40f, 0.30f, 0.10f,   25.f,  6000.f, 0, 0.85f);
    add ("Perfect Fifth",     7.0f,  0.0f, 0.45f, 0.00f, 0.35f, 0.55f, 0.00f,   60.f, 13000.f, 0, 0.65f);
    add ("Perfect Fourth",    5.0f,  0.0f, 0.45f, 0.00f, 0.30f, 0.55f, 0.00f,   60.f, 13000.f, 0, 0.65f);
    add ("Major Third",       4.0f,  0.0f, 0.40f, 0.00f, 0.20f, 0.60f, 0.00f,   80.f, 14000.f, 0, 0.65f);
    add ("Minor Third",       3.0f,  0.0f, 0.40f, 0.00f, 0.20f, 0.60f, 0.00f,   80.f, 14000.f, 0, 0.65f);
    add ("Detune Doubler",    0.0f, 12.0f, 0.50f, 0.00f, 0.10f, 0.80f, 0.00f,   40.f, 16000.f, 2, 0.55f);
    add ("Wide Chorus",       0.0f, -8.0f, 0.45f, 0.00f, 0.00f, 0.95f, 0.00f,   40.f, 16000.f, 2, 0.55f);
    add ("Subtle Thicken",    0.0f,  4.0f, 0.30f, 0.00f, 0.00f, 0.65f, 0.00f,   30.f, 18000.f, 0, 0.50f);
    add ("Vocal Shimmer",    12.0f,  0.0f, 0.40f, 0.55f, 0.45f, 0.55f, 0.00f,   80.f, 11000.f, 3, 0.75f);
    add ("Crystal Cascade",  12.0f,  0.0f, 0.45f, 0.65f, 0.55f, 0.55f, 0.05f,  100.f, 12000.f, 4, 0.85f);
    add ("Infinite Octaves", 12.0f,  0.0f, 0.45f, 0.78f, 0.50f, 0.55f, 0.10f,  100.f, 11000.f, 4, 0.90f);
    add ("Heaven Pad",       12.0f,  3.0f, 0.45f, 0.60f, 0.50f, 0.70f, 0.05f,  120.f,  9500.f, 4, 0.85f);
    add ("Cinematic Drop",  -12.0f,  0.0f, 0.50f, 0.40f, 0.45f, 0.45f, 0.20f,   30.f,  7000.f, 0, 0.85f);
    add ("Monster Voice",   -12.0f, -25.0f, 0.65f, 0.10f, 0.65f, 0.30f, 0.30f,   25.f,  5000.f, 0, 0.80f);
    add ("Chipmunk",         12.0f,  0.0f, 0.85f, 0.00f, 0.00f, 0.30f, 0.00f,  120.f, 14000.f, 1, 0.40f);
    add ("Whisper Boy",       7.0f,  0.0f, 0.50f, 0.00f, 0.30f, 0.40f, 0.00f,  120.f, 14000.f, 1, 0.50f);
    add ("Bass Power",      -12.0f,  0.0f, 0.55f, 0.00f, 0.00f, 0.40f, 0.10f,   25.f,  4500.f, 1, 0.85f);
    add ("Synth Octaver",   -12.0f,  0.0f, 0.50f, 0.00f, 0.00f, 0.50f, 0.05f,   30.f,  9000.f, 1, 0.70f);
    add ("Gentle Lift",       2.0f,  0.0f, 0.35f, 0.00f, 0.20f, 0.55f, 0.00f,   40.f, 16000.f, 0, 0.60f);
    add ("Dreamy Fifths",     7.0f, -3.0f, 0.40f, 0.30f, 0.30f, 0.65f, 0.00f,   80.f, 12500.f, 3, 0.75f);
    add ("Glass Bells",      19.0f,  0.0f, 0.40f, 0.55f, 0.50f, 0.60f, 0.00f,  150.f, 11500.f, 4, 0.80f);
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
        if (xml == nullptr || ! xml->hasTagName ("PS1Preset"))
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

    juce::XmlElement xml ("PS1Preset");
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
