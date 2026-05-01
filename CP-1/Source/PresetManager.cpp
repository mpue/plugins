/*
  ==============================================================================
    CP-1 Compressor — Preset Manager implementation
  ==============================================================================
*/

#include "PresetManager.h"

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& vts)
    : apvts (vts)
{
    buildPresetList();
}

//==============================================================================
const std::vector<PresetManager::FactoryData>& PresetManager::getFactoryData()
{
    static const std::vector<FactoryData> data =
    {
        //                        thr     ratio  atk     rel     knee   mkup   mix     hpf     det  link   autoR
        { "Init",               -18.0f,  4.0f,  10.0f,  120.0f,  6.0f,  0.0f, 100.0f,  20.0f, 0, true,  false },
        { "Vocal",              -22.0f,  3.0f,  12.0f,  100.0f,  8.0f,  5.0f, 100.0f,  80.0f, 1, true,  true  },
        { "Drum Bus",           -16.0f,  4.0f,  25.0f,   80.0f,  4.0f,  3.0f, 100.0f,  60.0f, 0, true,  false },
        { "Mix Bus Glue",       -14.0f,  2.0f,  30.0f,  250.0f, 12.0f,  2.0f, 100.0f,  20.0f, 1, true,  true  },
        { "Parallel Crush",     -35.0f, 12.0f,   0.5f,   40.0f,  0.0f,  0.0f,  35.0f,  20.0f, 0, true,  false },
        { "Brick Wall",          -3.0f, 20.0f,   0.1f,   60.0f,  1.0f,  0.0f, 100.0f,  20.0f, 0, true,  false },
        { "Slow Leveler",       -24.0f,  2.5f,  80.0f,  600.0f, 14.0f,  6.0f, 100.0f,  20.0f, 1, true,  true  },
        { "Punch",              -14.0f,  4.0f,  50.0f,   50.0f,  3.0f,  2.0f, 100.0f,  20.0f, 0, true,  false },
        { "Bass Control",       -18.0f,  3.0f,  15.0f,  150.0f,  8.0f,  3.0f, 100.0f,  20.0f, 1, false, false },
        { "De-Esser",           -25.0f,  6.0f,   0.5f,   30.0f,  2.0f,  0.0f, 100.0f, 500.0f, 0, true,  false },
    };
    return data;
}

//==============================================================================
void PresetManager::buildPresetList()
{
    presets.clear();

    for (auto& fd : getFactoryData())
        presets.push_back ({ fd.name, true, {} });

    numFactoryPresets = (int) presets.size();

    auto dir = getUserPresetDirectory();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
        files.sort();
        for (auto& f : files)
            presets.push_back ({ f.getFileNameWithoutExtension(), false, f });
    }
}

juce::File PresetManager::getUserPresetDirectory() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    dir = dir.getChildFile ("Application Support");
#endif
    return dir.getChildFile ("CP-1").getChildFile ("Presets");
}

//==============================================================================
int PresetManager::getNumPresets() const
{
    return (int) presets.size();
}

juce::String PresetManager::getPresetName (int index) const
{
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return presets[(size_t) index].name;
    return {};
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    juce::StringArray names;
    for (auto& p : presets)
        names.add (p.name);
    return names;
}

bool PresetManager::isFactoryPreset (int index) const
{
    return juce::isPositiveAndBelow (index, numFactoryPresets);
}

void PresetManager::setCurrentPresetIndex (int index)
{
    currentIndex = juce::jlimit (0, juce::jmax (0, (int) presets.size() - 1), index);
}

//==============================================================================
juce::ValueTree PresetManager::buildState (float threshold, float ratio, float attack,
                                           float release, float knee, float makeup,
                                           float mix, float scHpf, int detector,
                                           bool stereoLink, bool autoRelease) const
{
    juce::ValueTree state (apvts.state.getType());

    auto addParam = [&] (const juce::String& id, float value)
    {
        juce::ValueTree child (id);
        child.setProperty ("value", value, nullptr);
        state.addChild (child, -1, nullptr);
    };

    addParam ("threshold",   threshold);
    addParam ("ratio",       ratio);
    addParam ("attack",      attack);
    addParam ("release",     release);
    addParam ("knee",        knee);
    addParam ("makeup",      makeup);
    addParam ("mix",         mix);
    addParam ("scHpf",       scHpf);
    addParam ("detector",    (float) detector);
    addParam ("stereoLink",  stereoLink ? 1.0f : 0.0f);
    addParam ("autoRelease", autoRelease ? 1.0f : 0.0f);
    addParam ("extSc",       0.0f);
    addParam ("scListen",    0.0f);
    addParam ("bypass",      0.0f);

    return state;
}

//==============================================================================
void PresetManager::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentIndex = index;

    if (presets[(size_t) index].isFactory)
    {
        auto& data = getFactoryData();
        auto& fd = data[(size_t) index];
        auto state = buildState (fd.threshold, fd.ratio, fd.attack, fd.release,
                                 fd.knee, fd.makeup, fd.mix, fd.scHpf,
                                 fd.detector, fd.stereoLink, fd.autoRelease);
        apvts.replaceState (state);
    }
    else
    {
        auto& file = presets[(size_t) index].file;
        if (auto xml = juce::parseXML (file))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            if (state.isValid())
                apvts.replaceState (state);
        }
    }
}

void PresetManager::loadNextPreset()
{
    if (presets.empty()) return;
    loadPreset ((currentIndex + 1) % (int) presets.size());
}

void PresetManager::loadPreviousPreset()
{
    if (presets.empty()) return;
    loadPreset ((currentIndex - 1 + (int) presets.size()) % (int) presets.size());
}

//==============================================================================
int PresetManager::saveUserPreset (const juce::String& name)
{
    auto dir = getUserPresetDirectory();
    dir.createDirectory();

    auto file = dir.getChildFile (name + ".xml");

    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (file);

    buildPresetList();

    for (int i = 0; i < (int) presets.size(); ++i)
    {
        if (presets[(size_t) i].name == name && ! presets[(size_t) i].isFactory)
        {
            currentIndex = i;
            return i;
        }
    }
    return currentIndex;
}

bool PresetManager::deleteUserPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return false;
    if (presets[(size_t) index].isFactory)
        return false;

    auto file = presets[(size_t) index].file;
    if (file.deleteFile())
    {
        buildPresetList();
        if (currentIndex >= (int) presets.size())
            currentIndex = juce::jmax (0, (int) presets.size() - 1);
        return true;
    }
    return false;
}

void PresetManager::refreshUserPresets()
{
    auto currentName = getPresetName (currentIndex);
    buildPresetList();

    for (int i = 0; i < (int) presets.size(); ++i)
    {
        if (presets[(size_t) i].name == currentName)
        {
            currentIndex = i;
            return;
        }
    }
    currentIndex = 0;
}
