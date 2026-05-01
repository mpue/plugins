/*
  ==============================================================================
    ST-1  -  Luxury Saturation
    PluginEditor.cpp
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ST1AudioProcessorEditor::ST1AudioProcessorEditor (ST1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetManager (p.apvts),
      presetBar (presetManager, [this] { repaint(); }),
      visualizer (p),
      inMeter  ("INPUT",  p.inLevelDbL,  p.inLevelDbR),
      outMeter ("OUTPUT", p.outLevelDbL, p.outLevelDbR)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (visualizer);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    auto setupLabel = [this] (juce::Label& lab, const juce::String& text)
    {
        lab.setText (text, juce::dontSendNotification);
        lab.setJustificationType (juce::Justification::centred);
        lab.setColour (juce::Label::textColourId, juce::Colour (0xff7a99c0));
        lab.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        addAndMakeVisible (lab);
    };

    setupLabel (driveLabel,  "DRIVE");
    setupLabel (biasLabel,   "BIAS");
    setupLabel (toneLabel,   "TONE");
    setupLabel (mixLabel,    "MIX");
    setupLabel (outputLabel, "OUTPUT");

    for (auto* k : { &driveKnob, &biasKnob, &toneKnob, &mixKnob, &outputKnob })
    {
        styleKnob (*k);
        addAndMakeVisible (k);
    }

    driveAtt  = std::make_unique<SAttachment> (audioProcessor.apvts, ST1AudioProcessor::pidDrive,  driveKnob);
    biasAtt   = std::make_unique<SAttachment> (audioProcessor.apvts, ST1AudioProcessor::pidBias,   biasKnob);
    toneAtt   = std::make_unique<SAttachment> (audioProcessor.apvts, ST1AudioProcessor::pidTone,   toneKnob);
    mixAtt    = std::make_unique<SAttachment> (audioProcessor.apvts, ST1AudioProcessor::pidMix,    mixKnob);
    outputAtt = std::make_unique<SAttachment> (audioProcessor.apvts, ST1AudioProcessor::pidOutput, outputKnob);

    // Mode selector
    addAndMakeVisible (modeCombo);
    modeCombo.addItemList (SaturationEngine::getModeNames(), 1);
    modeAtt = std::make_unique<CAttachment> (audioProcessor.apvts, ST1AudioProcessor::pidMode, modeCombo);

    setupLabel (modeLabel, "MODE");

    // Oversampling
    addAndMakeVisible (oversamplingCombo);
    oversamplingCombo.addItemList ({ "Off", "2x", "4x" }, 1);
    oversamplingAtt = std::make_unique<CAttachment> (audioProcessor.apvts,
                                                      ST1AudioProcessor::pidOversampling,
                                                      oversamplingCombo);

    setupLabel (oversamplingLabel, "OVERSAMPLING");

    // Bypass
    addAndMakeVisible (bypassButton);
    bypassButton.setButtonText ("BYPASS");
    bypassAtt = std::make_unique<BAttachment> (audioProcessor.apvts, ST1AudioProcessor::pidBypass, bypassButton);

    setSize (760, 580);
}

ST1AudioProcessorEditor::~ST1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void ST1AudioProcessorEditor::styleKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
    s.setColour (juce::Slider::textBoxTextColourId,    juce::Colour (0xffe8e8e8));
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

//==============================================================================
void ST1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Brushed metal-ish background gradient
    juce::ColourGradient bg (juce::Colour (0xff20262f), b.getX(), b.getY(),
                             juce::Colour (0xff0c0f14), b.getX(), b.getBottom(), false);
    bg.addColour (0.55, juce::Colour (0xff181d25));
    g.setGradientFill (bg);
    g.fillAll();

    // Top brand band
    auto top = b.removeFromTop (52.0f);

    juce::ColourGradient topGrad (juce::Colour (0xff2a3142), top.getX(), top.getY(),
                                  juce::Colour (0xff141821), top.getX(), top.getBottom(), false);
    g.setGradientFill (topGrad);
    g.fillRect (top);

    g.setColour (juce::Colour (0xff2c343f));
    g.drawHorizontalLine ((int) top.getBottom() - 1, top.getX(), top.getRight());

    // Brand text
    auto brandRect = top.withTrimmedLeft (18.0f).withTrimmedRight (320.0f);
    g.setColour (juce::Colour (0xfff5d27a));
    g.setFont (juce::FontOptions ("Georgia", 26.0f, juce::Font::bold));
    g.drawText ("ST-1", brandRect, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff7a99c0));
    g.setFont (juce::FontOptions (11.5f, juce::Font::italic));
    auto subRect = brandRect.translated (62.0f, 0.0f);
    g.drawText ("LUXURY  SATURATION", subRect, juce::Justification::centredLeft, false);

    // Subtle accent line under header
    g.setColour (juce::Colour (0xffd4af37).withAlpha (0.55f));
    g.drawHorizontalLine ((int) top.getBottom(), top.getX() + 18.0f, top.getX() + 240.0f);

    // Light "noise" specks for luxury feel - very sparse
    juce::Random r (0xC0DECAFE);
    g.setColour (juce::Colours::white.withAlpha (0.012f));
    for (int i = 0; i < 600; ++i)
    {
        float x = r.nextFloat() * (float) getWidth();
        float y = 52.0f + r.nextFloat() * (float) (getHeight() - 52);
        g.fillRect (x, y, 1.0f, 1.0f);
    }

    // Footer hint
    g.setColour (juce::Colour (0xff5d6a7d).withAlpha (0.6f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (juce::String ("v") + JucePlugin_VersionString,
                getLocalBounds().removeFromBottom (16).reduced (10, 0),
                juce::Justification::centredRight, false);
}

void ST1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // ------- Top bar (brand + presets) ----------
    auto top = bounds.removeFromTop (52);
    auto presetArea = top.removeFromRight (340).reduced (8, 8);
    presetBar.setBounds (presetArea);

    // ------- Bottom controls strip --------------
    auto bottom = bounds.removeFromBottom (44).reduced (12, 6);
    {
        auto modeArea = bottom.removeFromLeft (220);
        modeLabel.setBounds (modeArea.removeFromLeft (52));
        modeCombo.setBounds (modeArea.reduced (4, 6));

        auto bypassArea = bottom.removeFromRight (110);
        bypassButton.setBounds (bypassArea.reduced (6, 6));

        auto osArea = bottom.removeFromRight (210);
        oversamplingLabel.setBounds (osArea.removeFromLeft (110));
        oversamplingCombo.setBounds (osArea.reduced (4, 6));
    }

    // ------- Knob row (5 knobs) ----------
    auto knobsRow = bounds.removeFromBottom (140);
    knobsRow.reduce (12, 4);

    const int n = 5;
    juce::Slider* knobs[n] = { &driveKnob, &biasKnob, &toneKnob, &mixKnob, &outputKnob };
    juce::Label*  labels[n] = { &driveLabel, &biasLabel, &toneLabel, &mixLabel, &outputLabel };

    const int colW = knobsRow.getWidth() / n;
    for (int i = 0; i < n; ++i)
    {
        auto col = knobsRow.removeFromLeft (colW);
        labels[i]->setBounds (col.removeFromTop (16));
        knobs[i]->setBounds (col.reduced (10, 4));
    }

    // ------- Main visualizer + meters ----------
    auto vizRow = bounds.reduced (12, 8);
    auto inMeterArea  = vizRow.removeFromLeft (76);
    auto outMeterArea = vizRow.removeFromRight (76);
    vizRow.reduce (8, 0);

    inMeter.setBounds (inMeterArea);
    outMeter.setBounds (outMeterArea);
    visualizer.setBounds (vizRow);
}
