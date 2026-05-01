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
                   .getChildFile ("SW-1")
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
                       float width,
                       float lowW, float midW, float highW,
                       float xLow, float xHigh,
                       float bassMonoHz, float bassMonoOn,
                       float shimmer, float haas, float rotation,
                       float output, float mix)
    {
        Preset p;
        p.name      = std::move (name);
        p.isFactory = true;
        p.values["width"]      = width;
        p.values["lowWidth"]   = lowW;
        p.values["midWidth"]   = midW;
        p.values["highWidth"]  = highW;
        p.values["xLow"]       = xLow;
        p.values["xHigh"]      = xHigh;
        p.values["bassMonoHz"] = bassMonoHz;
        p.values["bassMonoOn"] = bassMonoOn;
        p.values["shimmer"]    = shimmer;
        p.values["haas"]       = haas;
        p.values["rotation"]   = rotation;
        p.values["output"]     = output;
        p.values["mix"]        = mix;
        p.values["bypass"]     = 0.0f;
        p.values["monoCheck"]  = 0.0f;
        factoryPresets.push_back (std::move (p));
    };

    //   name                          wid   lo    mid   hi    xLo   xHi   bsMon bsOn shimmer haas  rot  out   mix
    add ("Default",                    100,  100,  100,  100,  250,  3500, 120,  1,    0,     0,    0,   0,    100);
    add ("Subtle Polish",              115,  90,   115,  130,  220,  3500, 110,  1,    8,     0,    0,   0,    100);
    add ("Master Bus Glue",            110,  85,   105,  120,  220,  4000, 110,  1,    5,     0,    0,   0,    100);
    add ("Vocal Air",                  130,  80,   115,  165,  300,  5000, 150,  1,   18,     0,    0,   0,    100);
    add ("Acoustic Bloom",             140,  90,   125,  165,  280,  4000, 130,  1,   22,     0,    0,   0,    100);
    add ("Wide Pad",                   170,  85,   135,  185,  260,  3500, 140,  1,   35,     0,    0,   0,    100);
    add ("Chorus Doubler",             160,  90,   125,  170,  260,  3200, 140,  1,   30,     6.0f, 0,   0,    100);
    add ("Drum Bus Open",              125,  85,   120,  140,  220,  4500, 130,  1,   12,     0,    0,   0,    100);
    add ("Tight Low / Wide High",      130,  60,   110,  175,  240,  4500, 160,  1,   18,     0,    0,   0,    100);
    add ("Mono Bass / Stereo Top",     140,  40,   115,  185,  220,  4500, 200,  1,   25,     0,    0,   0,    100);
    add ("Cinematic Wide",             175,  90,   140,  185,  280,  3000, 150,  1,   45,     0,    0,   0,    100);
    add ("Synth Lead Spread",          165,  80,   130,  180,  300,  3500, 140,  1,   30,    12.0f, 0,   0,    100);
    add ("Lush Strings",               180,  85,   135,  185,  260,  3000, 140,  1,   55,     0,    0,   0,    100);
    add ("Mid-Forward Vocal",           90,  100,   80,  110,  300,  4500, 140,  1,    0,     0,    0,   0,    100);
    add ("Side-Forward",               150, 110,  140,  155,  280,  3500, 140,  1,    8,     0,    0,   0,    100);
    add ("Tilt Left",                  120, 100,  120,  130,  260,  3800, 140,  1,    8,     0,   -8,   0,    100);
    add ("Tilt Right",                 120, 100,  120,  130,  260,  3800, 140,  1,    8,     0,    8,   0,    100);
    add ("Reference (Mono)",           100, 100,  100,  100,  250,  3500, 120,  0,    0,     0,    0,   0,    100);
    add ("Tape Stereo",                115,  85,  115,  130,  220,  3500, 130,  1,   12,     2.0f, 0,   0,    100);
    add ("Dimension Lite",             140,  95,  120,  155,  260,  3500, 140,  1,   28,     0,    0,   0,    100);
    add ("Halo Shimmer",               150,  90,  125,  175,  280,  3000, 150,  1,   60,     0,    0,   0,    100);
    add ("Modern Pop Master",          120,  85,  110,  135,  220,  4000, 120,  1,   10,     0,    0,   0,    100);
    add ("Vintage Glue",               110,  90,  110,  120,  240,  3500, 130,  1,    6,     0,    0,   0,    100);
    add ("Phone-Style Mono",            85, 100,  100,   85,  300,  4000, 200,  1,    0,     0,    0,   0,    100);
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
        if (xml == nullptr || ! xml->hasTagName ("SW1Preset"))
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

    juce::XmlElement xml ("SW1Preset");
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
