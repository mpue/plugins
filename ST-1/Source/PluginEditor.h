/*
  ==============================================================================
    ST-1  -  Luxury Saturation
    PluginEditor.h
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"

//==============================================================================
// PresetManager - handles factory + user presets stored on disk.
//==============================================================================
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        juce::String category;       // "Factory" or "User"
        juce::File   file;           // empty for factory presets
        juce::ValueTree state;       // already populated for factory; loaded on demand for user
        bool         isFactory = false;
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& apvtsRef)
        : apvts (apvtsRef)
    {
        rebuildFactoryPresets();
        ensureUserDirectoryExists();
        refresh();
    }

    static juce::File getUserPresetDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("ST-1").getChildFile ("Presets");
    }

    static juce::String getPresetExtension() { return ".stpreset"; }

    void ensureUserDirectoryExists()
    {
        auto dir = getUserPresetDirectory();
        if (! dir.exists())
            dir.createDirectory();
    }

    void refresh()
    {
        presets.clear();

        for (auto& fp : factoryPresets)
            presets.push_back (fp);

        juce::Array<juce::File> files;
        getUserPresetDirectory().findChildFiles (files, juce::File::findFiles, false,
                                                 "*" + getPresetExtension());
        files.sort();

        for (auto& f : files)
        {
            Preset p;
            p.name      = f.getFileNameWithoutExtension();
            p.category  = "User";
            p.file      = f;
            p.isFactory = false;
            presets.push_back (p);
        }
    }

    int getNumPresets() const noexcept { return (int) presets.size(); }
    const Preset& getPreset (int idx) const noexcept { return presets[(size_t) idx]; }

    int findPresetIndex (const juce::String& name) const
    {
        for (size_t i = 0; i < presets.size(); ++i)
            if (presets[i].name == name)
                return (int) i;
        return -1;
    }

    bool loadPreset (int idx)
    {
        if (! juce::isPositiveAndBelow (idx, (int) presets.size()))
            return false;

        const auto& p = presets[(size_t) idx];

        if (p.isFactory)
        {
            applyState (p.state);
            currentPresetName = p.name;
            currentPresetIsUser = false;
            return true;
        }

        if (auto xml = juce::XmlDocument::parse (p.file))
        {
            auto v = juce::ValueTree::fromXml (*xml);
            if (v.isValid())
            {
                applyState (v);
                currentPresetName = p.name;
                currentPresetIsUser = true;
                return true;
            }
        }
        return false;
    }

    juce::Result savePresetAs (const juce::String& presetName)
    {
        auto trimmed = presetName.trim();
        if (trimmed.isEmpty())
            return juce::Result::fail ("Empty name");

        ensureUserDirectoryExists();
        auto file = getUserPresetDirectory().getChildFile (trimmed + getPresetExtension());

        auto state = apvts.copyState();
        if (auto xml = state.createXml())
        {
            if (xml->writeTo (file))
            {
                refresh();
                currentPresetName   = trimmed;
                currentPresetIsUser = true;
                return juce::Result::ok();
            }
            return juce::Result::fail ("Could not write file");
        }
        return juce::Result::fail ("Could not serialise state");
    }

    bool deleteCurrentUserPreset()
    {
        if (! currentPresetIsUser)
            return false;

        auto file = getUserPresetDirectory().getChildFile (currentPresetName + getPresetExtension());
        if (file.existsAsFile() && file.deleteFile())
        {
            refresh();
            currentPresetName.clear();
            currentPresetIsUser = false;
            return true;
        }
        return false;
    }

    juce::String getCurrentPresetName() const { return currentPresetName; }
    bool         currentIsUserPreset()  const { return currentPresetIsUser; }

private:
    void applyState (const juce::ValueTree& state)
    {
        if (! state.isValid()) return;

        // Replace the whole APVTS state to ensure host parameter changes are sent.
        // The incoming `state` may itself be the APVTS state (correct type) or
        // a wrapper - try to find the right child.
        if (state.getType() == apvts.state.getType())
        {
            apvts.replaceState (state);
        }
        else if (auto child = state.getChildWithName (apvts.state.getType()); child.isValid())
        {
            apvts.replaceState (child);
        }
        else
        {
            // Last resort: walk individual properties keyed by parameter ID.
            for (auto* p : apvts.processor.getParameters())
            {
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    if (state.hasProperty (ranged->paramID))
                    {
                        const float v = (float) (double) state.getProperty (ranged->paramID);
                        ranged->setValueNotifyingHost (ranged->convertTo0to1 (v));
                    }
                }
            }
        }
    }

    void rebuildFactoryPresets()
    {
        factoryPresets.clear();

        auto add = [this] (const juce::String& name,
                           int mode, float drive, float bias, float tone,
                           float mix, float output)
        {
            Preset p;
            p.name      = name;
            p.category  = "Factory";
            p.isFactory = true;

            juce::ValueTree v ("STATE");
            auto setParam = [&] (const juce::String& id, float val)
            {
                juce::ValueTree pv ("PARAM");
                pv.setProperty ("id", id, nullptr);
                pv.setProperty ("value", val, nullptr);
                v.appendChild (pv, nullptr);
            };

            setParam (ST1AudioProcessor::pidMode,         (float) mode);
            setParam (ST1AudioProcessor::pidDrive,        drive);
            setParam (ST1AudioProcessor::pidBias,         bias);
            setParam (ST1AudioProcessor::pidTone,         tone);
            setParam (ST1AudioProcessor::pidMix,          mix);
            setParam (ST1AudioProcessor::pidOutput,       output);
            setParam (ST1AudioProcessor::pidOversampling, 1.0f);
            setParam (ST1AudioProcessor::pidBypass,       0.0f);

            p.state = v;
            factoryPresets.push_back (std::move (p));
        };

        add ("Init",                0,  6.0f, 0.0f,  0.0f,  1.0f,  0.0f);
        add ("Warm Tube",           SaturationEngine::Tube,        9.0f,  0.05f, 0.20f, 1.0f,  -1.0f);
        add ("Vintage Tape",        SaturationEngine::Tape,        7.0f,  0.0f, -0.18f, 1.0f,  -1.5f);
        add ("Console Glue",        SaturationEngine::Tube,        4.5f,  0.02f, 0.05f, 0.85f, -0.5f);
        add ("Crunchy Transistor",  SaturationEngine::Transistor, 16.0f,  0.0f,  0.10f, 1.0f,  -3.0f);
        add ("Diode Asymmetry",     SaturationEngine::Diode,      14.0f,  0.25f, 0.0f,  0.75f, -2.0f);
        add ("Aggressive Fold",     SaturationEngine::Foldback,   22.0f,  0.0f,  0.0f,  0.55f, -4.0f);
        add ("Subtle Color",        SaturationEngine::Tube,        3.0f,  0.0f,  0.10f, 0.30f,  0.0f);
        add ("Studio Bus",          SaturationEngine::Tape,        4.5f,  0.0f, -0.10f, 0.80f, -0.7f);
        add ("Brick Wall Limiter",  SaturationEngine::Hard,       12.0f,  0.0f,  0.0f,  1.0f,  -3.0f);
        add ("Smooth Atan",         SaturationEngine::Soft,        8.0f,  0.0f,  0.0f,  1.0f,  -1.5f);
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> factoryPresets;
    std::vector<Preset> presets;

    juce::String currentPresetName;
    bool currentPresetIsUser = false;
};

//==============================================================================
// LevelMeter - vertical RMS-style meter.
//==============================================================================
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    LevelMeter()  { startTimerHz (30); }
    ~LevelMeter() override { stopTimer(); }

    void setSource (std::atomic<float>& dbValue) { source = &dbValue; }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        // Casing
        g.setColour (juce::Colour (0xff121212));
        g.fillRoundedRectangle (bounds, 3.0f);

        g.setColour (juce::Colour (0xff2c2c2c));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        const float minDb = -60.0f, maxDb = 6.0f;
        const float v = juce::jlimit (minDb, maxDb, currentDb);
        const float vN = (v - minDb) / (maxDb - minDb);

        auto inner = bounds.reduced (3.0f);
        const float h = inner.getHeight();
        const float fillH = h * vN;

        juce::Rectangle<float> fillRect (inner.getX(),
                                         inner.getBottom() - fillH,
                                         inner.getWidth(),
                                         fillH);

        // Gradient: green -> amber -> red
        juce::ColourGradient grad (juce::Colour (0xff39c172), inner.getX(), inner.getBottom(),
                                   juce::Colour (0xffff5050), inner.getX(), inner.getY(), false);
        grad.addColour (0.65, juce::Colour (0xffd9b04a));
        grad.addColour (0.85, juce::Colour (0xfff39c12));

        g.setGradientFill (grad);
        g.fillRect (fillRect);

        // Segmented look
        g.setColour (juce::Colour (0xff121212));
        const int segments = 24;
        const float segH = h / segments;
        for (int i = 1; i < segments; ++i)
            g.fillRect (inner.getX(), inner.getY() + i * segH - 0.5f,
                        inner.getWidth(), 1.0f);

        // Peak hold tick
        if (peakHoldDb > minDb)
        {
            const float pN = (juce::jlimit (minDb, maxDb, peakHoldDb) - minDb) / (maxDb - minDb);
            const float py = inner.getBottom() - h * pN;
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillRect (inner.getX(), py - 1.0f, inner.getWidth(), 2.0f);
        }

        // 0 dB tick
        const float zeroN = (0.0f - minDb) / (maxDb - minDb);
        const float zy = inner.getBottom() - h * zeroN;
        g.setColour (juce::Colour (0xffd4af37).withAlpha (0.6f));
        g.fillRect (inner.getX(), zy - 0.5f, inner.getWidth(), 1.0f);
    }

private:
    void timerCallback() override
    {
        if (source == nullptr) return;

        const float target = source->load();

        // Decay current value smoothly.
        if (target > currentDb) currentDb = target;
        else                    currentDb = target + (currentDb - target) * 0.85f;

        if (target > peakHoldDb)
        {
            peakHoldDb = target;
            peakHoldFrames = 30; // ~1 s at 30Hz
        }
        else if (peakHoldFrames > 0)
        {
            --peakHoldFrames;
        }
        else
        {
            peakHoldDb = juce::jmax (-100.0f, peakHoldDb - 0.5f);
        }

        repaint();
    }

    std::atomic<float>* source = nullptr;
    float currentDb  = -100.0f;
    float peakHoldDb = -100.0f;
    int   peakHoldFrames = 0;
};

//==============================================================================
// SaturationVisualizer - draws transfer curve + live in/out scope overlay.
//==============================================================================
class SaturationVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit SaturationVisualizer (ST1AudioProcessor& p) : processor (p)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    ~SaturationVisualizer() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        // Background panel - subtle warm vignette
        {
            juce::ColourGradient bg (juce::Colour (0xff1a1f28), bounds.getCentreX(), bounds.getCentreY(),
                                     juce::Colour (0xff0e1116), bounds.getRight(), bounds.getBottom(), true);
            g.setGradientFill (bg);
            g.fillRoundedRectangle (bounds, 6.0f);
        }

        g.setColour (juce::Colour (0xff2c343f));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        // Reserve bottom 25% for the scope, top 75% for transfer curve.
        auto curveArea = bounds.reduced (12.0f);
        auto scopeArea = curveArea.removeFromBottom (curveArea.getHeight() * 0.28f);
        curveArea.removeFromBottom (8.0f);

        drawCurve (g, curveArea);
        drawScope (g, scopeArea);

        // Caption
        g.setColour (juce::Colour (0xff7a99c0).withAlpha (0.7f));
        g.setFont (juce::FontOptions (10.5f));
        g.drawText ("INPUT  /  OUTPUT", scopeArea.translated (0.0f, -scopeArea.getHeight() - 12.0f).withHeight (12),
                    juce::Justification::topRight, false);
        g.drawText ("TRANSFER CURVE", curveArea.withHeight (14),
                    juce::Justification::topLeft, false);
    }

private:
    void drawCurve (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const int   mode    = processor.getCurrentMode();
        const float bias    = processor.getCurrentBias();
        const float driveDb = processor.getCurrentDriveDb();
        const float drive   = juce::Decibels::decibelsToGain (driveDb);

        // Inner panel
        g.setColour (juce::Colour (0xff0a0d12));
        g.fillRoundedRectangle (area, 4.0f);

        // Grid
        g.setColour (juce::Colour (0xff232a35));
        const int gridSteps = 8;
        for (int i = 1; i < gridSteps; ++i)
        {
            const float fx = area.getX() + area.getWidth()  * (float) i / gridSteps;
            const float fy = area.getY() + area.getHeight() * (float) i / gridSteps;
            g.drawLine (fx, area.getY(), fx, area.getBottom(), 0.5f);
            g.drawLine (area.getX(), fy, area.getRight(), fy, 0.5f);
        }

        // Axes (centre lines)
        g.setColour (juce::Colour (0xff404a59));
        g.drawLine (area.getCentreX(), area.getY(),
                    area.getCentreX(), area.getBottom(), 1.0f);
        g.drawLine (area.getX(), area.getCentreY(),
                    area.getRight(), area.getCentreY(), 1.0f);

        // Identity reference line (y = x)
        g.setColour (juce::Colour (0xff2f3947).withAlpha (0.85f));
        g.drawLine (area.getX(), area.getBottom(), area.getRight(), area.getY(), 1.0f);

        // Transfer curve
        juce::Path curve;
        const int   N = 256;
        bool started = false;
        for (int i = 0; i < N; ++i)
        {
            const float xN = (float) i / (float) (N - 1);          // 0..1
            const float x  = -1.0f + 2.0f * xN;                    // -1..1
            const float y  = juce::jlimit (-1.2f, 1.2f,
                                            SaturationEngine::shape (x * drive, mode, bias));

            const float px = area.getX() + xN * area.getWidth();
            // Map y in [-1.2, 1.2] -> y-axis (top is positive)
            const float py = area.getCentreY() - (y / 1.2f) * (area.getHeight() * 0.5f);

            if (! started) { curve.startNewSubPath (px, py); started = true; }
            else            curve.lineTo (px, py);
        }

        // Glow pass
        g.setColour (juce::Colour (0xffd9a84a).withAlpha (0.18f));
        g.strokePath (curve, juce::PathStrokeType (5.5f));
        g.setColour (juce::Colour (0xffe7c97a).withAlpha (0.30f));
        g.strokePath (curve, juce::PathStrokeType (3.0f));

        // Main golden curve
        g.setColour (juce::Colour (0xfff5d27a));
        g.strokePath (curve, juce::PathStrokeType (1.6f));

        // Live indicator: ball at the position corresponding to current input level.
        const float inDb = juce::jmax (processor.inLevelDbL.load(), processor.inLevelDbR.load());
        const float inLin = juce::jlimit (0.0f, 1.0f, juce::Decibels::decibelsToGain (inDb, -100.0f));
        if (inLin > 0.001f)
        {
            const float xLive = inLin;
            const float yLive = juce::jlimit (-1.2f, 1.2f,
                                              SaturationEngine::shape (xLive * drive, mode, bias));
            const float px = area.getCentreX() + xLive * (area.getWidth() * 0.5f);
            const float py = area.getCentreY() - (yLive / 1.2f) * (area.getHeight() * 0.5f);

            g.setColour (juce::Colour (0xffffe9b8).withAlpha (0.35f));
            g.fillEllipse (px - 8.0f, py - 8.0f, 16.0f, 16.0f);
            g.setColour (juce::Colour (0xfffff4cc));
            g.fillEllipse (px - 3.5f, py - 3.5f, 7.0f, 7.0f);
        }

        // Frame
        g.setColour (juce::Colour (0xff2c343f));
        g.drawRoundedRectangle (area, 4.0f, 1.0f);
    }

    void drawScope (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (juce::Colour (0xff0a0d12));
        g.fillRoundedRectangle (area, 4.0f);

        // Centre line
        g.setColour (juce::Colour (0xff232a35));
        g.drawLine (area.getX(), area.getCentreY(),
                    area.getRight(), area.getCentreY(), 1.0f);

        const int   N = ST1AudioProcessor::scopeSize;
        const int   wp = processor.scopeWritePos.load();

        auto buildPath = [&] (const std::array<float, ST1AudioProcessor::scopeSize>& src) -> juce::Path
        {
            juce::Path p;
            for (int i = 0; i < N; ++i)
            {
                const int idx = (wp + i) % N;
                const float xN = (float) i / (float) (N - 1);
                const float v  = juce::jlimit (-1.5f, 1.5f, src[(size_t) idx]);
                const float px = area.getX() + xN * area.getWidth();
                const float py = area.getCentreY() - v * (area.getHeight() * 0.45f);
                if (i == 0) p.startNewSubPath (px, py);
                else        p.lineTo (px, py);
            }
            return p;
        };

        auto inPath  = buildPath (processor.scopeIn);
        auto outPath = buildPath (processor.scopeOut);

        // Input - cool grey
        g.setColour (juce::Colour (0xff5d6a7d).withAlpha (0.85f));
        g.strokePath (inPath, juce::PathStrokeType (1.0f));

        // Output - warm gold with glow
        g.setColour (juce::Colour (0xffd9a84a).withAlpha (0.25f));
        g.strokePath (outPath, juce::PathStrokeType (3.0f));
        g.setColour (juce::Colour (0xfff5d27a));
        g.strokePath (outPath, juce::PathStrokeType (1.4f));

        g.setColour (juce::Colour (0xff2c343f));
        g.drawRoundedRectangle (area, 4.0f, 1.0f);
    }

    void timerCallback() override { repaint(); }

    ST1AudioProcessor& processor;
};

//==============================================================================
// Combined input/output meter strip with little label.
//==============================================================================
class MeterStrip : public juce::Component
{
public:
    MeterStrip (const juce::String& title,
                std::atomic<float>& srcL,
                std::atomic<float>& srcR)
        : label (title)
    {
        meterL.setSource (srcL);
        meterR.setSource (srcR);
        addAndMakeVisible (meterL);
        addAndMakeVisible (meterR);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff7a99c0).withAlpha (0.85f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (label, getLocalBounds().removeFromTop (16),
                    juce::Justification::centred, false);

        g.setColour (juce::Colour (0xff7a99c0).withAlpha (0.4f));
        g.setFont (juce::FontOptions (9.5f));
        auto scale = getLocalBounds().reduced (0, 18);
        scale.removeFromBottom (6);

        const std::array<int, 4> ticks { 0, -6, -12, -24 };
        const float minDb = -60.0f, maxDb = 6.0f;
        for (auto t : ticks)
        {
            const float n = ((float) t - minDb) / (maxDb - minDb);
            const float y = scale.getBottom() - n * scale.getHeight();
            g.drawText (juce::String (t), 0, (int) y - 6, getWidth(), 12,
                        juce::Justification::centred, false);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromTop (18);
        b.removeFromBottom (6);

        auto half = b.getWidth() / 2;
        meterL.setBounds (b.removeFromLeft (half - 1).reduced (4, 0));
        meterR.setBounds (b.reduced (4, 0));
    }

private:
    juce::String label;
    LevelMeter meterL, meterR;
};

//==============================================================================
// PresetBar - preset selector + save/saveas/delete buttons.
//==============================================================================
class PresetBar : public juce::Component
{
public:
    PresetBar (PresetManager& pm, std::function<void()> onChange)
        : manager (pm), onPresetChange (std::move (onChange))
    {
        prevButton.setButtonText ("<");
        nextButton.setButtonText (">");
        saveButton.setButtonText ("Save");
        saveAsButton.setButtonText ("Save As");
        deleteButton.setButtonText ("Delete");

        for (auto* b : { &prevButton, &nextButton, &saveButton, &saveAsButton, &deleteButton })
            addAndMakeVisible (b);

        addAndMakeVisible (presetCombo);

        prevButton.onClick   = [this] { step (-1); };
        nextButton.onClick   = [this] { step (+1); };
        saveButton.onClick   = [this] { saveCurrent(); };
        saveAsButton.onClick = [this] { saveAs(); };
        deleteButton.onClick = [this] { deleteCurrent(); };

        presetCombo.onChange = [this]
        {
            const int idx = presetCombo.getSelectedItemIndex();
            if (idx >= 0)
            {
                manager.loadPreset (idx);
                if (onPresetChange) onPresetChange();
            }
        };

        rebuildCombo();
    }

    void rebuildCombo()
    {
        presetCombo.clear (juce::dontSendNotification);

        bool inFactory = false, inUser = false;
        int id = 1;
        const int n = manager.getNumPresets();
        for (int i = 0; i < n; ++i)
        {
            const auto& p = manager.getPreset (i);
            if (p.isFactory && ! inFactory) { presetCombo.addSectionHeading ("Factory"); inFactory = true; }
            if (! p.isFactory && ! inUser)  { presetCombo.addSectionHeading ("User");    inUser    = true; }
            presetCombo.addItem (p.name, id++);
        }

        const auto curName = manager.getCurrentPresetName();
        if (curName.isNotEmpty())
        {
            const int idx = manager.findPresetIndex (curName);
            if (idx >= 0) presetCombo.setSelectedItemIndex (idx, juce::dontSendNotification);
        }
        else if (manager.getNumPresets() > 0)
        {
            presetCombo.setSelectedItemIndex (0, juce::dontSendNotification);
        }
    }

    void selectByName (const juce::String& name)
    {
        const int idx = manager.findPresetIndex (name);
        if (idx >= 0)
            presetCombo.setSelectedItemIndex (idx, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        juce::ColourGradient bg (juce::Colour (0xff1d242f), bounds.getX(), bounds.getY(),
                                 juce::Colour (0xff141821), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (juce::Colour (0xff2c343f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6, 4);

        prevButton  .setBounds (b.removeFromLeft (28));
        b.removeFromLeft (3);
        nextButton  .setBounds (b.removeFromLeft (28));
        b.removeFromLeft (6);

        deleteButton.setBounds (b.removeFromRight (62));
        b.removeFromRight (4);
        saveAsButton.setBounds (b.removeFromRight (76));
        b.removeFromRight (4);
        saveButton  .setBounds (b.removeFromRight (60));
        b.removeFromRight (8);

        presetCombo.setBounds (b);
    }

private:
    void step (int dir)
    {
        const int n = manager.getNumPresets();
        if (n == 0) return;
        int idx = presetCombo.getSelectedItemIndex();
        idx = (idx + dir + n) % n;
        presetCombo.setSelectedItemIndex (idx, juce::sendNotificationSync);
    }

    void saveCurrent()
    {
        if (manager.currentIsUserPreset() && manager.getCurrentPresetName().isNotEmpty())
        {
            manager.savePresetAs (manager.getCurrentPresetName());
            rebuildCombo();
            selectByName (manager.getCurrentPresetName());
        }
        else
        {
            saveAs();
        }
    }

    void saveAs()
    {
        auto* aw = new juce::AlertWindow ("Save Preset",
                                          "Enter a name for the new preset:",
                                          juce::AlertWindow::NoIcon);
        aw->addTextEditor ("name", manager.getCurrentPresetName(), "Name");
        aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        aw->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, aw] (int result)
            {
                std::unique_ptr<juce::AlertWindow> owned (aw);
                if (result == 1)
                {
                    auto name = owned->getTextEditorContents ("name").trim();
                    if (name.isNotEmpty())
                    {
                        auto r = manager.savePresetAs (name);
                        if (r.failed())
                        {
                            juce::AlertWindow::showMessageBoxAsync (
                                juce::AlertWindow::WarningIcon,
                                "Could not save preset", r.getErrorMessage());
                        }
                        else
                        {
                            rebuildCombo();
                            selectByName (name);
                        }
                    }
                }
            }), false);
    }

    void deleteCurrent()
    {
        if (! manager.currentIsUserPreset())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                "Cannot delete",
                "Factory presets cannot be deleted.");
            return;
        }

        auto name = manager.getCurrentPresetName();
        juce::AlertWindow::showOkCancelBox (juce::AlertWindow::QuestionIcon,
            "Delete preset",
            "Delete user preset \"" + name + "\"?",
            "Delete", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([this] (int r)
            {
                if (r == 1)
                {
                    if (manager.deleteCurrentUserPreset())
                    {
                        rebuildCombo();
                        if (manager.getNumPresets() > 0)
                            presetCombo.setSelectedItemIndex (0, juce::sendNotificationSync);
                    }
                }
            }));
    }

    PresetManager& manager;
    std::function<void()> onPresetChange;

    juce::ComboBox presetCombo;
    juce::TextButton prevButton, nextButton, saveButton, saveAsButton, deleteButton;
};

//==============================================================================
// Main editor
//==============================================================================
class ST1AudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit ST1AudioProcessorEditor (ST1AudioProcessor&);
    ~ST1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void styleKnob (juce::Slider& s);

    ST1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel lookAndFeel;

    PresetManager presetManager;
    PresetBar     presetBar;

    SaturationVisualizer visualizer;
    MeterStrip inMeter;
    MeterStrip outMeter;

    juce::Slider driveKnob, biasKnob, toneKnob, mixKnob, outputKnob;
    juce::Label  driveLabel, biasLabel, toneLabel, mixLabel, outputLabel;

    juce::ComboBox  modeCombo;
    juce::Label     modeLabel;
    juce::ComboBox  oversamplingCombo;
    juce::Label     oversamplingLabel;
    juce::ToggleButton bypassButton;

    std::unique_ptr<SAttachment> driveAtt, biasAtt, toneAtt, mixAtt, outputAtt;
    std::unique_ptr<CAttachment> modeAtt, oversamplingAtt;
    std::unique_ptr<BAttachment> bypassAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ST1AudioProcessorEditor)
};
