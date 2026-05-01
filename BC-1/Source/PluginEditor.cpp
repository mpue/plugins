/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BC1AudioProcessorEditor::BC1AudioProcessorEditor (BC1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetBar (p.presetManager),
      scope (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (scope);

    bypassButton.setButtonText ("BYPASS");
    addAndMakeVisible (bypassButton);

    configureKnob (drive,  "DRIVE");
    configureKnob (bits,   "BITS");
    configureKnob (rate,   "RATE");
    configureKnob (dither, "DITHER");
    configureKnob (tone,   "TONE");
    configureKnob (mix,    "MIX");
    configureKnob (output, "OUTPUT");

    auto& apvts = audioProcessor.apvts;
    driveAtt  = std::make_unique<SAtt> (apvts, "drive",  drive .slider);
    bitsAtt   = std::make_unique<SAtt> (apvts, "bits",   bits  .slider);
    rateAtt   = std::make_unique<SAtt> (apvts, "rate",   rate  .slider);
    ditherAtt = std::make_unique<SAtt> (apvts, "dither", dither.slider);
    toneAtt   = std::make_unique<SAtt> (apvts, "tone",   tone  .slider);
    mixAtt    = std::make_unique<SAtt> (apvts, "mix",    mix   .slider);
    outputAtt = std::make_unique<SAtt> (apvts, "output", output.slider);
    bypassAtt = std::make_unique<BAtt> (apvts, "bypass", bypassButton);

    setSize (880, 560);
}

BC1AudioProcessorEditor::~BC1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void BC1AudioProcessorEditor::configureKnob (KnobControl& k, const juce::String& titleText)
{
    addAndMakeVisible (k.title);
    k.title.setText (titleText, juce::dontSendNotification);
    k.title.setJustificationType (juce::Justification::centred);
    k.title.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    k.title.setColour (juce::Label::textColourId, juce::Colour (0xff8aa6c8));
    k.title.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (k.slider);
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f,
                                   true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 20);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x00000000));
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x00000000));
    k.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd6e6ff));
    k.slider.setVelocityBasedMode (true);
    k.slider.setVelocityModeParameters (0.7, 1, 0.09, false);
    k.slider.setMouseDragSensitivity (180);
    k.slider.setDoubleClickReturnValue (false, 0.0);
}

void BC1AudioProcessorEditor::layoutKnob (KnobControl& k, juce::Rectangle<int> slot)
{
    auto title = slot.removeFromTop (22);
    k.title.setBounds (title);
    k.slider.setBounds (slot);
}

//==============================================================================
void BC1AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // ---------- main background ----------
    juce::ColourGradient bg (juce::Colour (0xff141a23), 0.0f, 0.0f,
                              juce::Colour (0xff0a0d12), 0.0f, (float) bounds.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    // subtle radial sheen
    {
        juce::ColourGradient sheen (juce::Colour (0x14a8c8ff), bounds.getWidth() * 0.5f, -120.0f,
                                     juce::Colour (0x00000000), bounds.getWidth() * 0.5f, 240.0f, true);
        g.setGradientFill (sheen);
        g.fillRect (bounds);
    }

    // ---------- header band ----------
    {
        auto header = bounds.withHeight (62).reduced (12, 8);
        juce::ColourGradient hbg (juce::Colour (0xff1d2533), header.getCentreX(), (float) header.getY(),
                                   juce::Colour (0xff10141d), header.getCentreX(), (float) header.getBottom(), false);
        g.setGradientFill (hbg);
        g.fillRoundedRectangle (header.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (header.toFloat(), 8.0f, 1.0f);

        // brand mark
        auto title = header.reduced (16, 6);
        title.removeFromRight (title.getWidth() / 2);

        g.setColour (juce::Colour (0xff8fc7ff));
        g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
        g.drawText ("BC-1",
                    juce::Rectangle<int> (title.getX(), title.getY() + 2, 70, 24),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff4d9eff));
        g.fillRect (juce::Rectangle<int> (title.getX() + 70, title.getY() + 6, 1, 18));

        g.setColour (juce::Colour (0xffb6c8e2));
        g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
        g.drawText ("LUXURY",
                    juce::Rectangle<int> (title.getX() + 80, title.getY() + 4, 60, 12),
                    juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff8aa6c8));
        g.drawText ("BITCRUSHER",
                    juce::Rectangle<int> (title.getX() + 80, title.getY() + 16, 80, 12),
                    juce::Justification::centredLeft);
    }

    // ---------- knob panel back ----------
    {
        auto knobPanel = juce::Rectangle<int> (12, 62 + 14 + 270 + 14, bounds.getWidth() - 24, 188);
        juce::ColourGradient pbg (juce::Colour (0xff1a212c), knobPanel.getCentreX(), (float) knobPanel.getY(),
                                   juce::Colour (0xff0d1118), knobPanel.getCentreX(), (float) knobPanel.getBottom(), false);
        g.setGradientFill (pbg);
        g.fillRoundedRectangle (knobPanel.toFloat(), 10.0f);
        g.setColour (juce::Colour (0xff262e3a));
        g.drawRoundedRectangle (knobPanel.toFloat(), 10.0f, 1.0f);

        // top accent line
        g.setColour (juce::Colour (0x224d9eff));
        g.drawLine ((float) knobPanel.getX() + 14.0f, (float) knobPanel.getY() + 1.0f,
                    (float) knobPanel.getRight() - 14.0f, (float) knobPanel.getY() + 1.0f, 1.0f);
    }
}

void BC1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // header row
    auto header = bounds.removeFromTop (62).reduced (12, 8);
    {
        auto h = header;
        h.removeFromLeft (190); // logo area
        // bypass on the right
        bypassButton.setBounds (h.removeFromRight (110).reduced (4, 6));
        h.removeFromRight (10);
        presetBar.setBounds (h.reduced (4, 0));
    }

    bounds.removeFromTop (14);

    // scope
    auto scopeArea = bounds.removeFromTop (270).reduced (12, 0);
    scope.setBounds (scopeArea);

    bounds.removeFromTop (14);

    // knob panel
    auto knobPanel = bounds.removeFromTop (188).reduced (24, 14);

    constexpr int knobCount = 7;
    const int slotWidth = knobPanel.getWidth() / knobCount;
    KnobControl* knobs[knobCount] { &drive, &bits, &rate, &dither, &tone, &mix, &output };

    for (int i = 0; i < knobCount; ++i)
    {
        auto slot = juce::Rectangle<int> (knobPanel.getX() + i * slotWidth,
                                           knobPanel.getY(),
                                           slotWidth,
                                           knobPanel.getHeight()).reduced (4, 0);
        layoutKnob (*knobs[i], slot);
    }
}
