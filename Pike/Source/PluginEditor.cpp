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

    std::vector<GroupSpec> filterEnvPage()
    {
        return {
            { "Filter",
              { C (pid::filterType,      "Type"),
                C (pid::filterSlope,     "Slope"),
                K (pid::filterCutoff,    "Cutoff"),
                K (pid::filterResonance, "Reso"),
                K (pid::filterKeyTrack,  "Key Trk"),
                K (pid::filterDrive,     "Drive"),
                K (pid::filterEnvAmount, "Env Amt") } },
            { "Amp Env",
              { K (pid::ampAttack, "A"), K (pid::ampDecay, "D"), K (pid::ampSustain, "S"), K (pid::ampRelease, "R") } },
            { "Filter Env",
              { K (pid::filtAttack, "A"), K (pid::filtDecay, "D"), K (pid::filtSustain, "S"), K (pid::filtRelease, "R") } },
            { "Aux Env",
              { K (pid::auxAttack, "A"), K (pid::auxDecay, "D"), K (pid::auxSustain, "S"), K (pid::auxRelease, "R") } },
        };
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
                K (pid::unisonDetune, "Detune"), K (pid::glideTime, "Glide") } },
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
    addPage ("Filter / Env", filterEnvPage());
    addPage ("Mod",          modPage());
    addPage ("FX",           fxPage());
    addPage ("Arp / Voice",  arpVoicePage());

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
void PikeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));

    auto header = getLocalBounds().removeFromTop (kHeaderHeight).toFloat();
    juce::ColourGradient grad (juce::Colour (0xff262b36), header.getX(), header.getY(),
                               juce::Colour (0xff15171c), header.getX(), header.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (header);

    g.setColour (juce::Colour (0xff4d9eff).withAlpha (0.7f));
    g.drawLine (header.getX(), header.getBottom() - 1.0f, header.getRight(), header.getBottom() - 1.0f, 2.0f);

    auto text = header.reduced (16.0f, 0.0f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("PIKE", text.removeFromLeft (100.0f), juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff888888));
    g.setFont (juce::Font (12.0f));
    g.drawText ("Polyphonic Hybrid Synthesizer", text, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff4d9eff));
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("v" JucePlugin_VersionString, header.reduced (16.0f, 0.0f),
                juce::Justification::centredRight, false);
}

void PikeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (kHeaderHeight);
    tabs.setBounds (area);
}
