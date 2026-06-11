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
    constexpr int kHeaderHeight = 64;

    CtrlSpec K (juce::String id, juce::String name) { return { CtrlType::Knob,   std::move (id), std::move (name) }; }
    CtrlSpec C (juce::String id, juce::String name) { return { CtrlType::Combo,  std::move (id), std::move (name) }; }
    CtrlSpec T (juce::String id, juce::String name) { return { CtrlType::Toggle, std::move (id), std::move (name) }; }

    //--------------------------------------------------------------------------
    std::vector<GroupSpec> oscPage()
    {
        std::vector<GroupSpec> g;
        g.push_back ({ "Master", { K (pid::masterGain, "Gain") } });

        for (int n = 0; n < 3; ++n)
            g.push_back ({ "Osc " + juce::String (n + 1),
                           { C (pid::oscWave[n],   "Wave"),
                             K (pid::oscOctave[n], "Oct"),
                             K (pid::oscSemi[n],   "Semi"),
                             K (pid::oscFine[n],   "Fine"),
                             K (pid::oscLevel[n],  "Level"),
                             K (pid::oscPW[n],     "PW"),
                             K (pid::oscWtPos[n],  "WT Pos") } });

        g.push_back ({ "Routing / Mixer",
                       { T (pid::osc2Sync,     "Sync 2"),
                         T (pid::osc3Sync,     "Sync 3"),
                         K (pid::fmAmount,     "FM 3>1"),
                         K (pid::ringModLevel, "Ring 1x2"),
                         K (pid::noiseLevel,   "Noise") } });
        return g;
    }

    std::vector<GroupSpec> modPage()
    {
        std::vector<GroupSpec> g;
        for (int n = 0; n < 2; ++n)
            g.push_back ({ "LFO " + juce::String (n + 1),
                           { C (pid::lfoShape[n],   "Shape"),
                             T (pid::lfoSync[n],    "Sync"),
                             K (pid::lfoRate[n],    "Rate"),
                             C (pid::lfoDiv[n],     "Div"),
                             T (pid::lfoKeySync[n], "Key Sync"),
                             T (pid::lfoMono[n],    "Mono"),
                             K (pid::lfoFade[n],    "Fade") } });

        for (int s = 0; s < pike::mod::numSlots; ++s)
            g.push_back ({ "Mod " + juce::String (s + 1),
                           { C (pid::modSourceId (s), "Src"),
                             C (pid::modDestId (s),   "Dst"),
                             K (pid::modDepthId (s),  "Depth") } });
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
PikeAudioProcessorEditor::PikeAudioProcessorEditor (PikeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    tabs.setOutline (0);
    tabs.setTabBarDepth (28);
    addAndMakeVisible (tabs);

    addPage ("Oscillators",  oscPage());

    // Bespoke Filter/Env page with animated envelope displays.
    {
        auto* vp = new PageViewport (new FilterEnvPage (audioProcessor.getValueTreeState(),
                                                        audioProcessor.getVisualState()));
        tabs.addTab ("Filter / Env", juce::Colour (0xff1a1a1a), vp, true);
    }

    addPage ("Mod",          modPage());
    addPage ("FX",           fxPage());
    addPage ("Arp / Voice",  arpVoicePage());

    // Header eyecatchers.
    scope = std::make_unique<pike::gui::Oscilloscope> (audioProcessor.getVisualState());
    meter = std::make_unique<pike::gui::LevelMeter>   (audioProcessor.getVisualState());
    addAndMakeVisible (*scope);
    addAndMakeVisible (*meter);

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

    setResizable (true, true);
    setResizeLimits (820, 480, 2400, 1500);
    setSize (1180, 720);
}

PikeAudioProcessorEditor::~PikeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void PikeAudioProcessorEditor::addPage (const juce::String& name, const std::vector<GroupSpec>& specs)
{
    auto* viewport = new PageViewport (new Page (audioProcessor.getValueTreeState(), specs));
    tabs.addTab (name, juce::Colour (0xff1a1a1a), viewport, true);
}

//==============================================================================
void PikeAudioProcessorEditor::refreshPresets()
{
    auto& pm = audioProcessor.getPresetManager();

    presetBox.clear (juce::dontSendNotification);
    presetItems.clear();

    int id = 1;
    auto factoryNames = pm.getFactoryNames();
    if (! factoryNames.isEmpty())
    {
        presetBox.addSectionHeading ("Factory");
        for (const auto& n : factoryNames)
        {
            presetItems.push_back ({ true, n });
            presetBox.addItem (n, id++);
        }
    }

    auto userNames = pm.getUserNames();
    if (! userNames.isEmpty())
    {
        presetBox.addSectionHeading ("User");
        for (const auto& n : userNames)
        {
            presetItems.push_back ({ false, n });
            presetBox.addItem (n, id++);
        }
    }

    // Reflect the currently-loaded preset name if we can find it.
    const auto current = pm.getCurrentName();
    for (size_t i = 0; i < presetItems.size(); ++i)
        if (presetItems[i].name == current)
        {
            presetBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
            break;
        }
}

void PikeAudioProcessorEditor::loadPresetAtComboId (int comboId)
{
    const int idx = comboId - 1;
    if (! juce::isPositiveAndBelow (idx, (int) presetItems.size()))
        return;

    auto& pm = audioProcessor.getPresetManager();
    if (presetItems[(size_t) idx].factory)
        pm.loadFactoryByName (presetItems[(size_t) idx].name);
    else
        pm.loadUser (presetItems[(size_t) idx].name);
}

void PikeAudioProcessorEditor::stepPreset (int direction)
{
    if (presetItems.empty())
        return;

    int idx = presetBox.getSelectedId() - 1;
    if (idx < 0) idx = 0;
    idx = (idx + direction + (int) presetItems.size()) % (int) presetItems.size();

    presetBox.setSelectedId (idx + 1);   // triggers onChange -> load
}

void PikeAudioProcessorEditor::showSaveDialog()
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
                const auto name = saveDialog->getTextEditorContents ("name");
                const auto saved = audioProcessor.getPresetManager().savePreset (name);
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
void PikeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));

    auto header = getLocalBounds().removeFromTop (kHeaderHeight).toFloat();
    juce::ColourGradient grad (juce::Colour (0xff2c3340), header.getX(), header.getY(),
                               juce::Colour (0xff141821), header.getX(), header.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (header);

    // Glossy sheen over the top half of the header.
    auto sheen = header.withHeight (header.getHeight() * 0.5f);
    juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.10f), sheen.getX(), sheen.getY(),
                                juce::Colours::white.withAlpha (0.0f),  sheen.getX(), sheen.getBottom(), false);
    g.setGradientFill (gloss);
    g.fillRect (sheen);

    // Bright top line + accent bottom line.
    g.setColour (juce::Colours::white.withAlpha (0.12f));
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
    g.drawText ("Synthesizer  v" JucePlugin_VersionString, text,
                juce::Justification::topLeft, false);
}

void PikeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header eyecatchers: scope fills the middle, meter sits at the right.
    auto header = area.removeFromTop (kHeaderHeight).reduced (12, 10);
    header.removeFromLeft (236);                 // logo text area
    if (meter != nullptr) meter->setBounds (header.removeFromRight (42));
    header.removeFromRight (10);
    if (scope != nullptr) scope->setBounds (header);

    // Preset bar.
    auto bar = area.removeFromTop (34).reduced (8, 4);
    saveButton.setBounds (bar.removeFromRight (70));
    bar.removeFromRight (6);
    nextButton.setBounds (bar.removeFromRight (32));
    prevButton.setBounds (bar.removeFromRight (32));
    bar.removeFromRight (6);
    presetBox.setBounds (bar.removeFromLeft (juce::jmin (340, bar.getWidth())));

    tabs.setBounds (area);
}
