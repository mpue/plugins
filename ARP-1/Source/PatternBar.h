/*
  ==============================================================================

    PatternBar.h
    ARP-1 — pattern library selector: combo + new / duplicate / delete.

    Editing a pattern auto-saves, so there is deliberately no explicit "Save"
    button here. "New" starts a fresh pattern, "Dup" branches the current one
    under a new name, "Del" removes a pattern from the library.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PatternManager.h"

namespace ARP1
{
    class PatternBar : public juce::Component
    {
    public:
        explicit PatternBar (PatternManager& pm) : patternManager (pm)
        {
            title.setText ("PATTERN", juce::dontSendNotification);
            title.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
            title.setColour (juce::Label::textColourId, juce::Colour (0xff8aa6cc));
            addAndMakeVisible (title);

            addAndMakeVisible (combo);
            combo.setTextWhenNothingSelected ("Pattern");
            combo.setJustificationType (juce::Justification::centredLeft);
            combo.onChange = [this]
            {
                if (suppressCombo) return;
                const int sel = combo.getSelectedId();
                if (sel <= 0) return;
                const auto names = patternManager.getPatternNames();
                if (juce::isPositiveAndBelow (sel - 1, names.size()))
                    patternManager.loadPattern (names[sel - 1]);
            };

            prevBtn.setButtonText ("<");
            nextBtn.setButtonText (">");
            newBtn .setButtonText ("New");
            dupBtn .setButtonText ("Dup");
            delBtn .setButtonText ("Del");

            for (auto* b : { &prevBtn, &nextBtn, &newBtn, &dupBtn, &delBtn })
                addAndMakeVisible (b);

            prevBtn.onClick = [this] { patternManager.selectPrevious(); };
            nextBtn.onClick = [this] { patternManager.selectNext(); };
            newBtn .onClick = [this] { showNameDialog ("New Pattern", "Pattern",
                                        [this] (juce::String n) { patternManager.newPattern (n); }); };
            dupBtn .onClick = [this] { showNameDialog ("Duplicate Pattern",
                                        patternManager.getCurrentPatternName() + " copy",
                                        [this] (juce::String n) { patternManager.duplicateAs (n); }); };
            delBtn .onClick = [this] { showDeleteDialog(); };

            rebuildCombo();

            patternManager.onPatternListChanged    = [this] { rebuildCombo(); };
            patternManager.onCurrentPatternChanged  = [this] { rebuildCombo(); };
        }

        ~PatternBar() override
        {
            patternManager.onPatternListChanged   = nullptr;
            patternManager.onCurrentPatternChanged = nullptr;
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8, 6);
            title.setBounds (r.removeFromLeft (62));
            r.removeFromLeft (4);
            prevBtn.setBounds (r.removeFromLeft (26));
            r.removeFromLeft (3);
            delBtn .setBounds (r.removeFromRight (46));
            r.removeFromRight (4);
            dupBtn .setBounds (r.removeFromRight (46));
            r.removeFromRight (4);
            newBtn .setBounds (r.removeFromRight (46));
            r.removeFromRight (4);
            nextBtn.setBounds (r.removeFromRight (26));
            r.removeFromRight (6);
            combo  .setBounds (r);
        }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
            juce::ColourGradient bg (juce::Colour (0xff202732), bounds.getCentreX(), bounds.getY(),
                                      juce::Colour (0xff141a23), bounds.getCentreX(), bounds.getBottom(), false);
            g.setGradientFill (bg);
            g.fillRoundedRectangle (bounds, 6.0f);
            g.setColour (juce::Colour (0xff2c3543));
            g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
        }

    private:
        void rebuildCombo()
        {
            const juce::ScopedValueSetter<bool> guard (suppressCombo, true);
            combo.clear (juce::dontSendNotification);

            const auto names = patternManager.getPatternNames();
            int id = 1;
            for (auto& n : names) combo.addItem (n, id++);

            const int idx = names.indexOf (patternManager.getCurrentPatternName());
            if (idx >= 0)
                combo.setSelectedId (idx + 1, juce::dontSendNotification);
            else
                combo.setText (patternManager.getCurrentPatternName(), juce::dontSendNotification);
        }

        void showNameDialog (const juce::String& title_, const juce::String& suggested,
                             std::function<void (juce::String)> onAccept)
        {
            auto* aw = new juce::AlertWindow (title_, "Pattern name:",
                                              juce::MessageBoxIconType::NoIcon);
            aw->addTextEditor ("name", suggested, "Name");
            aw->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
            aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            aw->enterModalState (true,
                juce::ModalCallbackFunction::create ([aw, onAccept] (int result)
                {
                    if (result == 1)
                    {
                        const auto name = aw->getTextEditorContents ("name").trim();
                        if (name.isNotEmpty()) onAccept (name);
                    }
                }), true);
        }

        void showDeleteDialog()
        {
            const auto current = patternManager.getCurrentPatternName();
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::QuestionIcon)
                .withTitle ("Delete Pattern")
                .withMessage ("Delete pattern \"" + current + "\" from the library?")
                .withButton ("Delete")
                .withButton ("Cancel"),
                [this, current] (int result)
                {
                    if (result == 1) patternManager.deletePattern (current);
                });
        }

        PatternManager& patternManager;

        juce::Label      title;
        juce::ComboBox   combo;
        juce::TextButton prevBtn, nextBtn, newBtn, dupBtn, delBtn;
        bool             suppressCombo = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternBar)
    };
}
