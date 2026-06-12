/*
  ==============================================================================

    ElegantDarkLookAndFeel.h
    Created: 27 Sep 2025 9:53:04pm
    Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "gui/GlassStyle.h"

class ElegantDarkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ElegantDarkLookAndFeel()
    {
        // Basis Farbschema
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::DocumentWindow::backgroundColourId, juce::Colour(0xff1a1a1a));

        // Text Farben
        setColour(juce::Label::textColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::TextEditor::highlightColourId, juce::Colour(0x66ff9425));
        setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff404040));
        setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xffff9425));

        // Button Farben
        setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff2e2115));   // dark amber
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb86a14)); // active orange
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc8a87a));  // steel-amber text
        setColour(juce::TextButton::textColourOnId,  juce::Colour(0xfffff6e8));  // bright text (on)

        // ComboBox
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff404040));
        setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff404040));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffe8e8e8));

        // PopupMenu
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffff9425));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xffffffff));

        // Slider
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff404040));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xffff9425));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff666666));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff9425));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff404040));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff404040));

        // ToggleButton
        setColour(juce::ToggleButton::textColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffff9425));
        setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff666666));

        // ListBox
        setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::ListBox::outlineColourId, juce::Colour(0xff404040));
        setColour(juce::ListBox::textColourId, juce::Colour(0xffe8e8e8));

        // TreeView
        setColour(juce::TreeView::backgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::TreeView::linesColourId, juce::Colour(0xff404040));
        setColour(juce::TreeView::dragAndDropIndicatorColourId, juce::Colour(0xffff9425));
        setColour(juce::TreeView::selectedItemBackgroundColourId, juce::Colour(0x66ff9425));

        // Scrollbar
        setColour(juce::ScrollBar::backgroundColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xffff9425));
        setColour(juce::ScrollBar::trackColourId, juce::Colour(0xff2a2a2a));

        // TabbedComponent
        setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::TabbedComponent::outlineColourId, juce::Colour(0xff404040));
        setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colour(0xff404040));
        setColour(juce::TabbedButtonBar::frontOutlineColourId, juce::Colour(0xffff9425));

        // TableListBox
        setColour(juce::TableListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::TableListBox::outlineColourId, juce::Colour(0xff404040));
        setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xff333333));
        setColour(juce::TableHeaderComponent::outlineColourId, juce::Colour(0xff404040));
        setColour(juce::TableHeaderComponent::textColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::TableHeaderComponent::highlightColourId, juce::Colour(0xffff9425));

        // ProgressBar
        setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xff404040));
        setColour(juce::ProgressBar::foregroundColourId, juce::Colour(0xffff9425));

        // GroupComponent
        setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff404040));
        setColour(juce::GroupComponent::textColourId, juce::Colour(0xffe8e8e8));

        // DirectoryContentsDisplayComponent
        setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, juce::Colour(0x66ff9425));
        setColour(juce::DirectoryContentsDisplayComponent::textColourId, juce::Colour(0xffe8e8e8));

        // FileBrowserComponent
        setColour(juce::FileBrowserComponent::currentPathBoxBackgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::FileBrowserComponent::currentPathBoxTextColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::FileBrowserComponent::currentPathBoxArrowColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::FileBrowserComponent::filenameBoxBackgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::FileBrowserComponent::filenameBoxTextColourId, juce::Colour(0xffe8e8e8));

        // AlertWindow
        setColour(juce::AlertWindow::backgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::AlertWindow::textColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::AlertWindow::outlineColourId, juce::Colour(0xff404040));

        // TooltipWindow
        setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff333333));
        setColour(juce::TooltipWindow::textColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::TooltipWindow::outlineColourId, juce::Colour(0xff404040));

        // CodeEditorComponent
        setColour(juce::CodeEditorComponent::backgroundColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::CodeEditorComponent::highlightColourId, juce::Colour(0x66ff9425));
        setColour(juce::CodeEditorComponent::defaultTextColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour(0xff2a2a2a));
        setColour(juce::CodeEditorComponent::lineNumberTextId, juce::Colour(0xff999999));

        // CaretComponent
        setColour(juce::CaretComponent::caretColourId, juce::Colour(0xffff9425));

        // HyperlinkButton
        setColour(juce::HyperlinkButton::textColourId, juce::Colour(0xffff9425));

        // KeyboardComponentBase
        setColour(juce::KeyboardComponentBase::upDownButtonBackgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::KeyboardComponentBase::upDownButtonArrowColourId, juce::Colour(0xffe8e8e8));

        // MidiKeyboardComponent
        setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffe8e8e8));
        setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff404040));
        setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x66ff9425));
        setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0x99ff9425));
        setColour(juce::MidiKeyboardComponent::textLabelColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId, juce::Colour(0xffe8e8e8));

		setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xff1a1a1a));


        // Schriftart setzen
        setDefaultSansSerifTypefaceName("Arial");
    }

    // Square LED toggle button
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool isMouseOverButton, bool isButtonDown) override
    {
        const bool  on      = button.getToggleState();
        const float ledSize = juce::jmin(14.0f, (float)button.getHeight() * 0.55f);
        const float ledX    = 2.0f;
        const float ledY    = ((float)button.getHeight() - ledSize) * 0.5f;

        juce::Rectangle<float> led(ledX, ledY, ledSize, ledSize);

        // Outer frame
        g.setColour(juce::Colour(0xff404040));
        g.drawRect(led, 1.0f);

        if (on)
        {
            // Glow halo
            juce::Colour glowCol = juce::Colour(0xffff9425).withAlpha(0.25f);
            g.setColour(glowCol);
            g.fillRect(led.expanded(2.5f));

            // Bright fill
            g.setColour(juce::Colour(0xffff9425));
            g.fillRect(led.reduced(1.0f));

            // Inner highlight
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.fillRect(led.reduced(1.0f).removeFromTop(ledSize * 0.35f).removeFromLeft(ledSize * 0.55f));
        }
        else
        {
            // Dark fill
            g.setColour(juce::Colour(0xff1a1a1a));
            g.fillRect(led.reduced(1.0f));

            // Subtle dim centre
            g.setColour(juce::Colour(0xff2a3040));
            g.fillRect(led.reduced(3.0f));
        }

        // Label text
        g.setColour(button.findColour(juce::ToggleButton::textColourId)
                          .withAlpha(button.isEnabled() ? 1.0f : 0.4f));
        g.setFont(juce::Font(juce::jmin(13.0f, (float)button.getHeight() * 0.55f)));
        g.drawText(button.getButtonText(),
                   (int)(ledX + ledSize + 6.0f), 0,
                   button.getWidth() - (int)(ledX + ledSize + 6.0f), button.getHeight(),
                   juce::Justification::centredLeft, false);
    }

    // Modern button with gradient body, specular top line and glow
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
        bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
        const float corner  = 5.0f;
        const bool  isOn    = button.getToggleState();

        juce::Colour base = backgroundColour;
        if (isButtonDown)      base = base.brighter(0.35f);
        else if (isMouseOverButton) base = base.brighter(0.15f);

        // Subtle top-to-bottom gradient gives depth
        juce::ColourGradient body(
            base.brighter(0.10f), bounds.getX(), bounds.getY(),
            base.darker (0.10f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(bounds, corner);

        // Radial outer glow when hovered or toggled on
        if (isMouseOverButton || isOn)
        {
            juce::Colour glowCol = isOn
                ? base.withAlpha(0.30f)
                : juce::Colour(0x22ff9425);

            juce::Path glowPath;
            glowPath.addRoundedRectangle(bounds.expanded(3.0f), corner + 3.0f);
            juce::ColourGradient glow(
                glowCol,                       bounds.getCentreX(), bounds.getCentreY(),
                glowCol.withAlpha(0.0f),       bounds.getRight(),   bounds.getBottom(), true);
            g.setGradientFill(glow);
            g.fillPath(glowPath);
        }

        // Specular top-edge highlight (makes it look raised)
        g.setColour(juce::Colours::white.withAlpha(isOn ? 0.20f : 0.07f));
        g.drawLine(bounds.getX() + corner, bounds.getY() + 0.5f,
                   bounds.getRight() - corner, bounds.getY() + 0.5f, 1.0f);

        // Border – brighter when active
        juce::Colour borderCol = isOn
            ? base.brighter(0.6f).withAlpha(0.75f)
            : base.brighter(0.25f).withAlpha(0.45f);
        g.setColour(borderCol);
        g.drawRoundedRectangle(bounds, corner, 1.0f);
    }

    // Rotary slider using image strip (128 frames), resolution chosen by slider size.
    // Tick-mark scale drawn in the outer ring around the knob.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& /*slider*/) override
    {
        const int sliderSize = juce::jmin(width, height);

        juce::Image strip = juce::ImageCache::getFromMemory(BinaryData::Knob_64_png, BinaryData::Knob_64_pngSize);

        const float outerRadius = (float)juce::jmin(width / 2, height / 2);
        const float centreX     = (float)x + (float)width  * 0.5f;
        const float centreY     = (float)y + (float)height * 0.5f;

        const float knobRadius = outerRadius * 0.78f;
        const float kx = centreX - knobRadius;
        const float ky = centreY - knobRadius;
        const float kw = knobRadius * 2.0f;

        const int   numTicks = 33;
        const float angleRange = rotaryEndAngle - rotaryStartAngle;

        for (int i = 0; i < numTicks; ++i)
        {
            const float t         = (float)i / (float)(numTicks - 1);
            const float tickAngle = rotaryStartAngle + t * angleRange;
            const bool  isMajor   = (i % 4 == 0);
            const bool  isActive  = (t <= sliderPos + 0.001f);

            const float iR = isMajor ? outerRadius * 0.78f : outerRadius * 0.83f;
            const float oR = isMajor ? outerRadius * 0.99f : outerRadius * 0.93f;

            const float sinA = std::sin(tickAngle);
            const float cosA = std::cos(tickAngle);

            const float px1 = centreX + iR * sinA;
            const float py1 = centreY - iR * cosA;
            const float px2 = centreX + oR * sinA;
            const float py2 = centreY - oR * cosA;

            juce::Colour col;
            if (isActive)
                col = isMajor ? juce::Colour(0xffff9425) : juce::Colour(0xaaff9425);
            else
                col = isMajor ? juce::Colour(0xff555555) : juce::Colour(0xff383838);

            g.setColour(col);
            g.drawLine(px1, py1, px2, py2, isMajor ? 1.5f : 1.0f);
        }

        const int nFrames  = strip.getHeight() / strip.getWidth();
        const int frameIdx = juce::jlimit(0, nFrames - 1,
                                (int)std::ceil(sliderPos * (double)(nFrames - 1)));

        g.drawImage(strip,
                    (int)kx, (int)ky, (int)kw, (int)kw,
                    0, frameIdx * strip.getWidth(), strip.getWidth(), strip.getWidth());
    }

    /*
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (slider.isBar())
        {
            g.setColour(slider.findColour(juce::Slider::trackColourId));
            g.fillRect(slider.isHorizontal() ? juce::Rectangle<float>(static_cast<float>(x), (float)y + (float)height * 0.4f,
                sliderPos - (float)x, (float)height * 0.2f)
                : juce::Rectangle<float>((float)x + (float)width * 0.4f, sliderPos,
                    (float)width * 0.2f, (float)y + (float)height - sliderPos));
        }
        else
        {
            auto trackWidth = juce::jmin(6.0f, slider.isHorizontal() ? (float)height * 0.25f : (float)width * 0.25f);

            juce::Point<float> startPoint(slider.isHorizontal() ? (float)x : (float)x + (float)width * 0.5f,
                slider.isHorizontal() ? (float)y + (float)height * 0.5f : (float)(height + y));

            juce::Point<float> endPoint(slider.isHorizontal() ? (float)(width + x) : startPoint.x,
                slider.isHorizontal() ? startPoint.y : (float)y);

            juce::Path backgroundTrack;
            backgroundTrack.startNewSubPath(startPoint);
            backgroundTrack.lineTo(endPoint);
            g.setColour(slider.findColour(juce::Slider::backgroundColourId));
            g.strokePath(backgroundTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

            juce::Path valueTrack;
            juce::Point<float> minPoint, maxPoint, thumbPoint;

            if (slider.isHorizontal())
            {
                minPoint = startPoint;
                maxPoint = { sliderPos, startPoint.y };
                thumbPoint = { sliderPos, startPoint.y };
            }
            else
            {
                minPoint = { startPoint.x, sliderPos };
                maxPoint = endPoint;
                thumbPoint = { startPoint.x, sliderPos };
            }

            valueTrack.startNewSubPath(minPoint);
            valueTrack.lineTo(maxPoint);
            g.setColour(slider.findColour(juce::Slider::trackColourId));
            g.strokePath(valueTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

            // Thumb mit Glow
            auto thumbRadius = trackWidth * 1.5f;
            g.setColour(juce::Colour(0x66ff9425));
            g.fillEllipse(juce::Rectangle<float>(thumbRadius * 2.0f, thumbRadius * 2.0f).withCentre(thumbPoint));
            g.setColour(slider.findColour(juce::Slider::thumbColourId));
            g.fillEllipse(juce::Rectangle<float>(thumbRadius, thumbRadius).withCentre(thumbPoint));
        }
    }
    */

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearHorizontal)
        {
            // Hintergrund
            g.setColour(juce::Colour(0xFF1a2332));
            g.fillRoundedRectangle(x, y, width, height, 4.0f);

            // VU-Meter Bereich berechnen
            float fillWidth = sliderPos - x;

            // Gradient f�r VU-Meter erstellen (Bernsteint�ne)
            juce::ColourGradient gradient(
                juce::Colour(0xFFf5b32f), x, y,  // Helles Amber (links)
                juce::Colour(0xFF855713), sliderPos, y,  // Dunkles Amber (rechts)
                false
            );

            // Optional: Warnstufe bei hohen Werten (�ber 80%)
            float valueRange = slider.getMaximum() - slider.getMinimum();
            float normalizedValue = (slider.getValue() - slider.getMinimum()) / valueRange;

            if (normalizedValue > 0.8f)
            {
                gradient = juce::ColourGradient(
                    juce::Colour(0xFFf5b32f), x, y,
                    juce::Colour(0xFFb89e6a), sliderPos, y,
                    false
                );
            }

            g.setGradientFill(gradient);
            g.fillRoundedRectangle(x, y, fillWidth, height, 4.0f);

            // Segmentierte Darstellung (optional)
            g.setColour(juce::Colour(0xFF1a2332));
            int segments = 20;
            float segmentWidth = width / (float)segments;
            for (int i = 1; i < segments; ++i)
            {
                float segX = x + i * segmentWidth;
                if (segX < sliderPos)
                {
                    g.drawLine(segX, y + 2, segX, y + height - 2, 2.0f);
                }
            }

            // Glanzeffekt oben
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(0x40ffffff), x, y,
                juce::Colour(0x00ffffff), x, y + height * 0.5f,
                false
            ));
            g.fillRoundedRectangle(x, y, fillWidth, height * 0.5f, 4.0f);

            // Rahmen
            g.setColour(juce::Colour(0xFF0d1419));
            g.drawRoundedRectangle(x, y, width, height, 4.0f, 1.5f);
        }
        else
        {
            // Fallback f�r andere Slider-Stile
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                sliderPos, minSliderPos, maxSliderPos,
                style, slider);
        }
    }

    // Sci-fi tab buttons: chamfered top-right corner, uppercase letterspaced
    // text, neon underline + glow on the active tab.
    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                        bool isMouseOver, bool /*isMouseDown*/) override
    {
        const auto accent = pike::gui::theme::accent;
        auto b = button.getLocalBounds().toFloat().reduced (1.0f, 0.0f);
        const bool front = button.isFrontTab();
        const float chamfer = 8.0f;

        juce::Path shape;
        shape.startNewSubPath (b.getX(), b.getY() + 2.0f);
        shape.lineTo (b.getRight() - chamfer, b.getY() + 2.0f);
        shape.lineTo (b.getRight(), b.getY() + 2.0f + chamfer);
        shape.lineTo (b.getRight(), b.getBottom());
        shape.lineTo (b.getX(), b.getBottom());
        shape.closeSubPath();

        juce::Colour top    = front ? juce::Colour (0xff27323f) : juce::Colour (0xff161b22);
        juce::Colour bottom = front ? juce::Colour (0xff10161d) : juce::Colour (0xff0c1016);
        if (isMouseOver && ! front)
        {
            top    = top.brighter (0.15f);
            bottom = bottom.brighter (0.10f);
        }

        juce::ColourGradient body (top, b.getX(), b.getY(), bottom, b.getX(), b.getBottom(), false);
        g.setGradientFill (body);
        g.fillPath (shape);

        g.setColour (front ? accent.withAlpha (0.6f) : juce::Colours::black.withAlpha (0.5f));
        g.strokePath (shape, juce::PathStrokeType (1.0f));

        // Neon underline on the active tab (with a soft glow strip above it).
        if (front)
        {
            auto base = juce::Rectangle<float> (b.getX() + 2.0f, b.getBottom() - 2.0f, b.getWidth() - 4.0f, 2.0f);
            g.setColour (accent.withAlpha (0.25f));
            g.fillRect (base.translated (0.0f, -2.0f).expanded (0.0f, 1.0f));
            g.setColour (accent);
            g.fillRect (base);
        }

        const juce::Colour textCol = front ? juce::Colour (0xfffff3e6)
                                           : (isMouseOver ? juce::Colour (0xffddc9ac)
                                                          : juce::Colour (0xff94826e));
        g.setColour (textCol);
        g.setFont (pike::gui::hudFont (12.0f, front));
        g.drawText (button.getButtonText().toUpperCase(),
                    button.getTextArea(), juce::Justification::centred, false);
    }

    int getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth) override
    {
        return juce::LookAndFeel_V4::getTabButtonBestWidth (button, tabDepth) + 18;
    }

    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override {}
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}

    // Custom ComboBox Arrow
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
        int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override
    {
        auto cornerSize = 6.0f;
        auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();

        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, cornerSize);

        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds.reduced(0.5f, 0.5f), cornerSize, 1.0f);

        juce::Rectangle<int> arrowZone(width - 30, 0, 20, height);
        juce::Path path;
        path.startNewSubPath((float)arrowZone.getX() + 3.0f, (float)arrowZone.getCentreY() - 2.0f);
        path.lineTo((float)arrowZone.getCentreX(), (float)arrowZone.getCentreY() + 3.0f);
        path.lineTo((float)arrowZone.getRight() - 3.0f, (float)arrowZone.getCentreY() - 2.0f);

        g.setColour(box.findColour(juce::ComboBox::arrowColourId).withAlpha(box.isEnabled() ? 0.9f : 0.2f));
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }

    // Custom ScrollBar
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
        bool isScrollbarVertical, int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override
    {
        juce::Rectangle<int> thumbBounds;

        if (isScrollbarVertical)
            thumbBounds = { x + 2, thumbStartPosition + 2, width - 4, thumbSize - 4 };
        else
            thumbBounds = { thumbStartPosition + 2, y + 2, thumbSize - 4, height - 4 };

        auto cornerSize = (float)juce::jmin(thumbBounds.getWidth(), thumbBounds.getHeight()) * 0.5f;

        g.setColour(scrollbar.findColour(juce::ScrollBar::thumbColourId).withAlpha(isMouseOver ? 0.8f : 0.6f));
        g.fillRoundedRectangle(thumbBounds.toFloat(), cornerSize);
    }

    // Bessere Fonts f�r bessere Lesbarkeit
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return juce::Font(juce::jmin(15.0f, (float)buttonHeight * 0.6f), juce::Font::FontStyleFlags::plain);
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font(14.0f);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(14.0f);
    }

    juce::Font getLabelFont(juce::Label&) override
    {
        return juce::Font(14.0f);
    }

    // Glass-styled popup menu background to match the panels.
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        juce::Rectangle<float> b (0.0f, 0.0f, (float) width, (float) height);

        juce::ColourGradient body (juce::Colour (0xff2b313d), 0.0f, 0.0f,
                                   juce::Colour (0xff171c24), 0.0f, (float) height, false);
        g.setGradientFill (body);
        g.fillRect (b);

        // Thin polished glint along the top edge.
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawHorizontalLine (0, 0.0f, (float) width);

        // Bevel border: dark outer + faint blue inner, like the glass panels.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRect (b, 1.0f);
        g.setColour (juce::Colour (0xffff9425).withAlpha (0.30f));
        g.drawRect (b.reduced (1.0f), 1.0f);
    }


    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
        bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
        const juce::String& text, const juce::String& shortcutKeyText,
        const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        if (isSeparator)
        {
            auto r = area.reduced (8, 0);
            r.removeFromTop (r.getHeight() / 2);
            g.setColour (juce::Colour (0xffff9425).withAlpha (0.25f));
            g.fillRect (r.removeFromTop (1));
            return;
        }

        auto textCol = (textColour != nullptr ? *textColour
                                              : findColour (juce::PopupMenu::textColourId));

        auto r = area.reduced (3, 1);

        // Rounded blue highlight pill on the active item.
        if (isHighlighted && isActive)
        {
            g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId).withAlpha (0.9f));
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
            textCol = findColour (juce::PopupMenu::highlightedTextColourId);
        }

        if (! isActive)
            textCol = textCol.withMultipliedAlpha (0.4f);

        auto font = getPopupMenuFont();
        g.setFont (font);
        g.setColour (textCol);

        auto iconArea = r.removeFromLeft (juce::roundToInt (area.getHeight() * 0.7f)).toFloat();

        if (icon != nullptr)
        {
            icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
        }
        else if (isTicked)
        {
            auto tick = getTickShape (1.0f);
            g.fillPath (tick, tick.getTransformToScaleToFit (iconArea.reduced (iconArea.getWidth() / 4, iconArea.getHeight() / 4), true));
        }

        if (hasSubMenu)
        {
            auto arrowH = 0.6f * font.getAscent();
            auto x = (float) r.getRight() - arrowH;
            auto halfH = (float) area.getCentreY();

            juce::Path path;
            path.startNewSubPath (x, halfH - arrowH * 0.5f);
            path.lineTo (x + arrowH * 0.5f, halfH);
            path.lineTo (x, halfH + arrowH * 0.5f);
            g.strokePath (path, juce::PathStrokeType (2.0f));

            r.removeFromRight (juce::roundToInt (arrowH));
        }

        if (shortcutKeyText.isNotEmpty())
        {
            auto f2 = font.withHeight (font.getHeight() * 0.75f);
            g.setFont (f2);
            g.drawText (shortcutKeyText, r, juce::Justification::centredRight, true);
            g.setFont (font);
        }

        g.drawFittedText (text, r.reduced (4, 0), juce::Justification::centredLeft, 1);
    }

    // Glass-styled alert box so the "Save Preset" dialog matches the theme.
    void drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert,
                       const juce::Rectangle<int>& textArea, juce::TextLayout& textLayout) override
    {
        auto bounds = alert.getLocalBounds().toFloat();

        juce::ColourGradient body (juce::Colour (0xff2b313d), bounds.getX(), bounds.getY(),
                                   juce::Colour (0xff0d1117), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (body);
        g.fillRect (bounds);

        // Polished top glint.
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawHorizontalLine (0, bounds.getX(), bounds.getRight());

        // Message text.
        g.setColour (alert.findColour (juce::AlertWindow::textColourId));
        textLayout.draw (g, textArea.toFloat());

        // Bevel border: dark outer + faint blue inner.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRect (bounds, 1.0f);
        g.setColour (juce::Colour (0xffff9425).withAlpha (0.40f));
        g.drawRect (bounds.reduced (1.0f), 1.0f);
    }
};
