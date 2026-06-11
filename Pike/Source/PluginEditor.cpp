/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterIDs.h"
#include "params/ModMatrixDefs.h"

using namespace pike::gui;

namespace
{
    constexpr int kHeaderHeight = 52;
    constexpr int kPresetHeight = 34;

    CtrlSpec K (juce::String id, juce::String name) { return { CtrlType::Knob,   std::move (id), std::move (name) }; }
    CtrlSpec C (juce::String id, juce::String name) { return { CtrlType::Combo,  std::move (id), std::move (name) }; }
    CtrlSpec T (juce::String id, juce::String name) { return { CtrlType::Toggle, std::move (id), std::move (name) }; }

    //--------------------------------------------------------------------------
    std::vector<GroupSpec> oscPage()
    {
        std::vector<GroupSpec> g;

        // Osc 1, Osc 2, then Master (top-right), then Osc 3 + Routing.
        for (int n = 0; n < 2; ++n)
            g.push_back ({ "Osc " + juce::String (n + 1),
                           { C (pid::oscWave[n],   "Wave"),
                             K (pid::oscOctave[n], "Oct"),
                             K (pid::oscSemi[n],   "Semi"),
                             K (pid::oscFine[n],   "Fine"),
                             K (pid::oscLevel[n],  "Level"),
                             K (pid::oscPW[n],     "PW"),
                             K (pid::oscWtPos[n],  "WT Pos") } });

        g.push_back ({ "Master", { K (pid::masterGain, "Gain") } });

        g.push_back ({ "Osc 3",
                       { C (pid::oscWave[2],   "Wave"),
                         K (pid::oscOctave[2], "Oct"),
                         K (pid::oscSemi[2],   "Semi"),
                         K (pid::oscFine[2],   "Fine"),
                         K (pid::oscLevel[2],  "Level"),
                         K (pid::oscPW[2],     "PW"),
                         K (pid::oscWtPos[2],  "WT Pos") } });

        g.push_back ({ "Routing / Mixer",
                       { T (pid::osc2Sync,     "Sync 2"),
                         T (pid::osc3Sync,     "Sync 3"),
                         K (pid::fmAmount,     "FM 3>1"),
                         K (pid::ringModLevel, "Ring 1x2"),
                         K (pid::noiseLevel,   "Noise") } });
        return g;
    }

    std::vector<GroupSpec> fxPage()
    {
        return {
            { "Distortion",
              { T (pid::distOn, "On"), C (pid::distType, "Type"), K (pid::distDrive, "Drive"), K (pid::distMix, "Mix") } },
            { "Chorus",
              { T (pid::chorusOn, "On"), K (pid::chorusRate, "Rate"), K (pid::chorusDepth, "Depth"),
                K (pid::chorusFeedback, "Fbk"), K (pid::chorusMix, "Mix") } },
            { "Delay",
              { T (pid::delayOn, "On"), T (pid::delaySync, "Sync"), K (pid::delayTime, "Time"),
                C (pid::delayDiv, "Div"), K (pid::delayFeedback, "Fbk"), K (pid::delayMix, "Mix"),
                T (pid::delayPingpong, "Ping-Pong") } },
            { "Reverb",
              { T (pid::reverbOn, "On"), K (pid::reverbSize, "Size"), K (pid::reverbDamping, "Damp"),
                K (pid::reverbWidth, "Width"), K (pid::reverbMix, "Mix") } },
        };
    }

    std::vector<GroupSpec> arpVoicePage()
    {
        return {
            { "Arpeggiator",
              { T (pid::arpOn, "On"), C (pid::arpMode, "Mode"), C (pid::arpRate, "Rate"),
                K (pid::arpGate, "Gate"), K (pid::arpOctaves, "Octaves"), T (pid::arpLatch, "Latch") } },
            { "Voice",
              { C (pid::voiceMode, "Mode"), K (pid::unisonCount, "Unison"),
                K (pid::unisonDetune, "Detune"), K (pid::stereoWidth, "Width"),
                K (pid::glideTime, "Glide") } },
        };
    }
}

//==============================================================================
PikeContent::PikeContent (PikeAudioProcessor& p) : audioProcessor (p)
{
    auto& apvts = audioProcessor.getValueTreeState();

    tabs.setOutline (0);
    tabs.setTabBarDepth (28);
    addAndMakeVisible (tabs);

    const auto tabColour = juce::Colour (0xff141820);
    tabs.addTab ("Oscillators",  tabColour, new Page (apvts, oscPage()),                                       true);
    tabs.addTab ("Filter / Env", tabColour, new FilterEnvPage (apvts, audioProcessor.getVisualState()),        true);
    tabs.addTab ("Mod",          tabColour, new ModPage (apvts),                                               true);
    tabs.addTab ("FX",           tabColour, new Page (apvts, fxPage()),                                        true);
    tabs.addTab ("Arp / Voice",  tabColour, new Page (apvts, arpVoicePage()),                                  true);

    // Preset bar.
    addAndMakeVisible (presetBox);
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this] { loadPresetAtComboId (presetBox.getSelectedId()); };

    for (auto* b : { &prevButton, &nextButton, &saveButton })
        addAndMakeVisible (*b);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    saveButton.onClick = [this] { showSaveDialog(); };
    refreshPresets();

    // Header eyecatchers.
    scope = std::make_unique<Oscilloscope> (audioProcessor.getVisualState());
    meter = std::make_unique<LevelMeter>   (audioProcessor.getVisualState());
    addAndMakeVisible (*scope);
    addAndMakeVisible (*meter);
}

//==============================================================================
void PikeContent::refreshPresets()
{
    auto& pm = audioProcessor.getPresetManager();
    presetBox.clear (juce::dontSendNotification);
    presetItems.clear();

    int id = 1;
    auto factoryNames = pm.getFactoryNames();
    if (! factoryNames.isEmpty())
    {
        presetBox.addSectionHeading ("Factory");
        for (const auto& n : factoryNames) { presetItems.push_back ({ true, n });  presetBox.addItem (n, id++); }
    }
    auto userNames = pm.getUserNames();
    if (! userNames.isEmpty())
    {
        presetBox.addSectionHeading ("User");
        for (const auto& n : userNames) { presetItems.push_back ({ false, n }); presetBox.addItem (n, id++); }
    }

    const auto current = pm.getCurrentName();
    for (size_t i = 0; i < presetItems.size(); ++i)
        if (presetItems[i].name == current)
        {
            presetBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
            break;
        }
}

void PikeContent::loadPresetAtComboId (int comboId)
{
    const int idx = comboId - 1;
    if (! juce::isPositiveAndBelow (idx, (int) presetItems.size()))
        return;

    auto& pm = audioProcessor.getPresetManager();
    if (presetItems[(size_t) idx].factory) pm.loadFactoryByName (presetItems[(size_t) idx].name);
    else                                   pm.loadUser (presetItems[(size_t) idx].name);
}

void PikeContent::stepPreset (int direction)
{
    if (presetItems.empty())
        return;

    int idx = presetBox.getSelectedId() - 1;
    if (idx < 0) idx = 0;
    idx = (idx + direction + (int) presetItems.size()) % (int) presetItems.size();
    presetBox.setSelectedId (idx + 1);
}

void PikeContent::showSaveDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> (
        "Save Preset", "Enter a name for the preset:", juce::MessageBoxIconType::NoIcon);

    saveDialog->addTextEditor ("name", audioProcessor.getPresetManager().getCurrentName());
    saveDialog->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this] (int result)
        {
            if (result == 1)
            {
                const auto saved = audioProcessor.getPresetManager().savePreset (saveDialog->getTextEditorContents ("name"));
                refreshPresets();
                for (size_t i = 0; i < presetItems.size(); ++i)
                    if (! presetItems[i].factory && presetItems[i].name == saved)
                    {
                        presetBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
                        break;
                    }
            }
            saveDialog.reset();
        }), false);
}

//==============================================================================
void PikeContent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0e13));

    auto header = getLocalBounds().removeFromTop (kHeaderHeight).toFloat();
    juce::ColourGradient grad (juce::Colour (0xff232a35), header.getX(), header.getY(),
                               juce::Colour (0xff0d1117), header.getX(), header.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (header);

    auto glint = header.withHeight (5.0f);
    juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.10f), glint.getX(), glint.getY(),
                                juce::Colours::white.withAlpha (0.0f),  glint.getX(), glint.getBottom(), false);
    g.setGradientFill (gloss);
    g.fillRect (glint);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (header.getX(), header.getY() + 0.5f, header.getRight(), header.getY() + 0.5f, 1.0f);
    g.setColour (juce::Colour (0xff4d9eff).withAlpha (0.7f));
    g.drawLine (header.getX(), header.getBottom() - 1.0f, header.getRight(), header.getBottom() - 1.0f, 2.0f);

    auto text = header.reduced (16.0f, 0.0f).withWidth (220.0f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("PIKE", text.removeFromLeft (96.0f), juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff8a93a6));
    g.setFont (juce::Font (11.0f));
    g.drawText ("Polyphonic Hybrid", text.removeFromTop (header.getHeight() * 0.5f).reduced (0, 6),
                juce::Justification::bottomLeft, false);
    g.setColour (juce::Colour (0xff4d9eff));
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText ("Synthesizer  v" JucePlugin_VersionString, text, juce::Justification::topLeft, false);
}

void PikeContent::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (kHeaderHeight).reduced (12, 10);
    header.removeFromLeft (236);
    if (meter != nullptr) meter->setBounds (header.removeFromRight (42));
    header.removeFromRight (10);
    if (scope != nullptr) scope->setBounds (header);

    auto bar = area.removeFromTop (kPresetHeight).reduced (8, 4);
    saveButton.setBounds (bar.removeFromRight (70));
    bar.removeFromRight (6);
    nextButton.setBounds (bar.removeFromRight (32));
    prevButton.setBounds (bar.removeFromRight (32));
    bar.removeFromRight (6);
    presetBox.setBounds (bar.removeFromLeft (juce::jmin (340, bar.getWidth())));

    tabs.setBounds (area);
}

//==============================================================================
PikeAudioProcessorEditor::PikeAudioProcessorEditor (PikeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), content (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);

    setResizable (true, true);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) PikeContent::designW / (double) PikeContent::designH);
    setResizeLimits (PikeContent::designW / 2, PikeContent::designH / 2,
                     PikeContent::designW * 2, PikeContent::designH * 2);

    setSize (1000, 655);
}

PikeAudioProcessorEditor::~PikeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void PikeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void PikeAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().toFloat();
    const float s = juce::jmin (r.getWidth()  / (float) PikeContent::designW,
                                r.getHeight() / (float) PikeContent::designH);

    content.setBounds (0, 0, PikeContent::designW, PikeContent::designH);

    const float sw = PikeContent::designW * s;
    const float sh = PikeContent::designH * s;
    content.setTransform (juce::AffineTransform::scale (s)
                              .translated ((r.getWidth() - sw) * 0.5f, (r.getHeight() - sh) * 0.5f));
}
