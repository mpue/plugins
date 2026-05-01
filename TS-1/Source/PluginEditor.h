/*
  ==============================================================================

    TS-1 Transient Shaper – Editor

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ElegantDarkLookAndFeel.h"

//==============================================================================
//  Envelope visualiser – shows fast envelope plus gain change overlay.
//==============================================================================
class TS1Visualiser : public juce::Component, private juce::Timer
{
public:
    explicit TS1Visualiser (TS1AudioProcessor& p) : processor (p)
    {
        envSnap.fill  (0.0f);
        gainSnap.fill (0.0f);
        startTimerHz (30);
    }

    ~TS1Visualiser() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto fb = getLocalBounds().toFloat();
        const auto bg = fb.reduced (2.0f);

        // Background
        g.setColour (juce::Colour (0xff10141c));
        g.fillRoundedRectangle (bg, 8.0f);

        // Frame
        g.setColour (juce::Colour (0xff222a3a));
        g.drawRoundedRectangle (bg.reduced (0.5f), 8.0f, 1.0f);

        // Centre line (zero-gain)
        const float centreY = bg.getY() + bg.getHeight() * 0.55f;

        // Grid
        g.setColour (juce::Colour (0xff1c2638));
        for (int i = 1; i < 5; ++i)
        {
            const float y = bg.getY() + bg.getHeight() * (i / 5.0f);
            g.drawHorizontalLine ((int) y, bg.getX(), bg.getRight());
        }
        for (int i = 1; i < 12; ++i)
        {
            const float x = bg.getX() + bg.getWidth() * (i / 12.0f);
            g.drawVerticalLine ((int) x, bg.getY(), bg.getBottom());
        }
        // Centre line accent
        g.setColour (juce::Colour (0xff2b3a55));
        g.drawHorizontalLine ((int) centreY, bg.getX(), bg.getRight());

        const int n = (int) envSnap.size();

        // Envelope path (filled)
        {
            juce::Path p, fill;
            for (int i = 0; i < n; ++i)
            {
                const float v = juce::jlimit (0.0f, 1.0f, envSnap[(size_t) i] * 1.4f);
                const float x = bg.getX() + (i / (float) (n - 1)) * bg.getWidth();
                const float y = centreY - v * (bg.getHeight() * 0.55f - 4.0f);
                if (i == 0) { p.startNewSubPath (x, y); fill.startNewSubPath (x, centreY); fill.lineTo (x, y); }
                else        { p.lineTo (x, y);          fill.lineTo (x, y); }
            }
            fill.lineTo (bg.getRight(), centreY);
            fill.closeSubPath();

            juce::ColourGradient grad (
                juce::Colour (0x884d9eff), bg.getCentreX(), bg.getY(),
                juce::Colour (0x114d9eff), bg.getCentreX(), centreY, false);
            g.setGradientFill (grad);
            g.fillPath (fill);

            g.setColour (juce::Colour (0xff7fb6ff));
            g.strokePath (p, juce::PathStrokeType (1.4f));
        }

        // Gain change ribbon below centre line — bright = boost, red = cut
        {
            const float scaleDb = 18.0f;
            const float halfH   = bg.getHeight() * 0.40f;
            for (int i = 0; i < n - 1; ++i)
            {
                const float gDb  = juce::jlimit (-scaleDb, scaleDb, gainSnap[(size_t) i]);
                const float t    = gDb / scaleDb;
                const float x    = bg.getX() + (i / (float) (n - 1)) * bg.getWidth();
                const float w    = bg.getWidth() / (float) (n - 1) + 1.0f;
                const float h    = std::abs (t) * halfH;
                if (h < 0.5f) continue;

                juce::Rectangle<float> r;
                if (t >= 0.0f) r = { x, centreY - h, w, h };
                else           r = { x, centreY,     w, h };

                g.setColour (t >= 0.0f
                             ? juce::Colour (0xff4dffae).withAlpha (0.55f)
                             : juce::Colour (0xffff5a4d).withAlpha (0.55f));
                g.fillRect (r);
            }
        }

        // Header strip (label + transient activity LED)
        const float headerH = 18.0f;
        auto header = juce::Rectangle<float> (bg.getX() + 8.0f, bg.getY() + 4.0f,
                                              bg.getWidth() - 16.0f, headerH);

        g.setColour (juce::Colour (0xff8aa1c4));
        g.setFont (juce::Font (10.5f, juce::Font::bold));
        g.drawText ("ENVELOPE", header.withWidth (90.0f), juce::Justification::centredLeft);

        // Activity LED bar – right aligned in header
        const float act = juce::jlimit (0.0f, 1.0f, processor.getTransientActivity() * 1.3f);
        auto led = juce::Rectangle<float> (header.getRight() - 130.0f,
                                           header.getY() + 5.0f, 130.0f, 7.0f);
        g.setColour (juce::Colour (0xff1a2030));
        g.fillRoundedRectangle (led, 2.5f);
        if (act > 0.005f)
        {
            auto fillR = led.reduced (1.0f).withWidth ((led.getWidth() - 2.0f) * act);
            // Glow
            g.setColour (juce::Colour (0x664d9eff));
            g.fillRoundedRectangle (fillR.expanded (2.0f, 1.5f), 3.5f);
            g.setColour (juce::Colour (0xff4d9eff));
            g.fillRoundedRectangle (fillR, 2.0f);
        }
        g.setColour (juce::Colour (0xff8aa1c4));
        g.drawText ("TRANSIENT",
                    juce::Rectangle<float> (led.getX() - 80.0f, led.getY() - 3.0f, 75.0f, 13.0f),
                    juce::Justification::centredRight);

        // Numeric gain readout (top-right, below LED)
        const float gDb = processor.getGainChangeDb();
        const auto gainStr = (gDb >= 0.0f ? "+" : "") + juce::String (gDb, 1) + " dB";
        g.setColour (gDb >= 0.0f ? juce::Colour (0xff4dffae) : juce::Colour (0xffff7a73));
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (gainStr,
                    juce::Rectangle<float> (bg.getRight() - 90.0f, bg.getBottom() - 24.0f, 82.0f, 18.0f),
                    juce::Justification::centredRight);

        g.setColour (juce::Colour (0xff8aa1c4));
        g.setFont (juce::Font (9.5f));
        g.drawText ("Δ GAIN",
                    juce::Rectangle<float> (bg.getRight() - 160.0f, bg.getBottom() - 24.0f, 65.0f, 18.0f),
                    juce::Justification::centredRight);
    }

    void timerCallback() override
    {
        std::array<float, TS1AudioProcessor::waveformSize> envTmp{}, gainTmp{};
        processor.copyWaveformSnapshot (envTmp.data(), gainTmp.data(),
                                        TS1AudioProcessor::waveformSize);

        const float a = 0.55f;
        for (size_t i = 0; i < envSnap.size(); ++i)
        {
            const int srcIdx = (int) ((float) i / (float) (envSnap.size() - 1)
                                      * (float) (TS1AudioProcessor::waveformSize - 1));
            envSnap[i]  = a * envTmp[(size_t) srcIdx]  + (1.0f - a) * envSnap[i];
            gainSnap[i] = a * gainTmp[(size_t) srcIdx] + (1.0f - a) * gainSnap[i];
        }
        repaint();
    }

private:
    TS1AudioProcessor& processor;
    std::array<float, 256> envSnap{};
    std::array<float, 256> gainSnap{};
};

//==============================================================================
//  Vertical level meter with peak hold.
//==============================================================================
class TS1Meter : public juce::Component, private juce::Timer
{
public:
    enum class Mode { Level, GainBipolar };

    explicit TS1Meter (Mode m = Mode::Level) : mode (m) { startTimerHz (30); }
    ~TS1Meter() override { stopTimer(); }

    void setLabel (const juce::String& l) { label = l; }
    void setSource (std::function<float()> s) { source = std::move (s); }
    void setMaxValue (float m) { maxValue = m; }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        auto labelArea = bounds.removeFromTop (14.0f);
        g.setColour (juce::Colour (0xff8aa1c4));
        g.setFont (juce::Font (9.5f, juce::Font::bold));
        g.drawText (label, labelArea, juce::Justification::centred);

        bounds.reduce (0.0f, 2.0f);

        // Background trough
        g.setColour (juce::Colour (0xff0d1419));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (juce::Colour (0xff222a3a));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

        const auto inner = bounds.reduced (2.0f);

        if (mode == Mode::Level)
        {
            const float v = juce::jlimit (0.0f, 1.0f, displayValue / maxValue);

            auto fillR = inner;
            fillR.setHeight (inner.getHeight() * v);
            fillR.setY (inner.getBottom() - fillR.getHeight());

            juce::ColourGradient grad (
                juce::Colour (0xffff5a4d), inner.getCentreX(), inner.getY(),
                juce::Colour (0xff4d9eff), inner.getCentreX(), inner.getBottom(), false);
            grad.addColour (0.30, juce::Colour (0xffffaa33));
            grad.addColour (0.55, juce::Colour (0xff4dffae));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fillR, 2.0f);

            // Peak hold marker
            if (peakHold > 0.005f)
            {
                const float py = inner.getBottom() - inner.getHeight()
                                  * juce::jlimit (0.0f, 1.0f, peakHold / maxValue);
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.fillRect (juce::Rectangle<float> (inner.getX(), py - 1.0f, inner.getWidth(), 2.0f));
            }

            // Tick marks
            g.setColour (juce::Colour (0x551a2030));
            for (int i = 1; i < 8; ++i)
            {
                const float ty = inner.getY() + inner.getHeight() * (i / 8.0f);
                g.drawHorizontalLine ((int) ty, inner.getX(), inner.getRight());
            }
        }
        else // GainBipolar
        {
            const float scale = maxValue;
            const float v = juce::jlimit (-scale, scale, displayValue);
            const float t = v / scale;
            const float midY = inner.getCentreY();

            // Centre line
            g.setColour (juce::Colour (0xff2b3a55));
            g.drawHorizontalLine ((int) midY, inner.getX(), inner.getRight());

            const float h = std::abs (t) * (inner.getHeight() * 0.5f);
            juce::Rectangle<float> r;
            if (t >= 0.0f) r = { inner.getX(), midY - h, inner.getWidth(), h };
            else           r = { inner.getX(), midY,     inner.getWidth(), h };

            g.setColour (t >= 0.0f
                          ? juce::Colour (0xff4dffae)
                          : juce::Colour (0xffff5a4d));
            g.fillRoundedRectangle (r, 1.5f);
        }
    }

    void timerCallback() override
    {
        if (source)
        {
            const float raw = source();
            if (mode == Mode::Level)
            {
                const float a = (raw > displayValue) ? 0.7f : 0.12f;
                displayValue = a * raw + (1.0f - a) * displayValue;
                if (raw > peakHold) peakHold = raw;
                else peakHold *= 0.985f;
            }
            else
            {
                displayValue = 0.5f * raw + 0.5f * displayValue;
            }
        }
        repaint();
    }

private:
    Mode mode;
    juce::String label;
    float displayValue = 0.0f;
    float peakHold     = 0.0f;
    float maxValue     = 1.0f;
    std::function<float()> source;
};

//==============================================================================
class TS1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    TS1AudioProcessorEditor (TS1AudioProcessor&);
    ~TS1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void rebuildPresetMenu();
    void onPresetChosen();
    void onSavePreset();
    void onSaveAsPreset();
    void onDeletePreset();
    void shiftPreset (int delta);

    TS1AudioProcessor& audioProcessor;
    ElegantDarkLookAndFeel laf;

    // Header
    juce::Label titleLabel;
    juce::Label subtitleLabel;

    // Preset bar
    juce::ComboBox    presetCombo;
    juce::TextButton  prevBtn      { "<" };
    juce::TextButton  nextBtn      { ">" };
    juce::TextButton  saveBtn      { "Save" };
    juce::TextButton  saveAsBtn    { "Save As" };
    juce::TextButton  deleteBtn    { "Delete" };

    // Visualiser
    TS1Visualiser visualiser { audioProcessor };

    // Knobs
    juce::Slider attackKnob, sustainKnob, sensitivityKnob;
    juce::Label  attackLbl, sustainLbl, sensLbl;

    // Bottom sliders
    juce::Slider outputSlider, mixSlider;
    juce::Label  outputLbl, mixLbl;
    juce::ToggleButton bypassToggle { "Bypass" };

    // Meters
    TS1Meter inputMeter  { TS1Meter::Mode::Level };
    TS1Meter outputMeter { TS1Meter::Mode::Level };
    TS1Meter gainMeter   { TS1Meter::Mode::GainBipolar };

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SAtt> attackAtt, sustainAtt, sensitivityAtt, outputAtt, mixAtt;
    std::unique_ptr<BAtt> bypassAtt;

    juce::StringArray factoryNames;
    juce::StringArray userNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TS1AudioProcessorEditor)
};
