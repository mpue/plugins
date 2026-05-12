/*
  ==============================================================================

    PresetBar.h
    HH-1 preset selector strip: prev / combo / next / save / del / init.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PresetManager.h"

class PresetBar : public juce::Component
{
public:
    explicit PresetBar (PresetManager& pm) : presetManager (pm)
    {
        addAndMakeVisible (combo);
        combo.setTextWhenNothingSelected ("Preset");
        combo.setJustificationType (juce::Justification::centred);
        combo.onChange = [this]
        {
            if (suppressCombo) return;
            const int sel = combo.getSelectedId();
            if (sel <= 0) return;
            const auto names = presetManager.getAllPresetNames();
            const int idx = sel - 1;
            if (juce::isPositiveAndBelow (idx, names.size()))
                presetManager.loadPreset (names[idx]);
        };

        prevBtn  .setButtonText ("<");
        nextBtn  .setButtonText (">");
        saveBtn  .setButtonText ("Save");
        deleteBtn.setButtonText ("Del");
        initBtn  .setButtonText ("Init");

        addAndMakeVisible (prevBtn);
        addAndMakeVisible (nextBtn);
        addAndMakeVisible (saveBtn);
        addAndMakeVisible (deleteBtn);
        addAndMakeVisible (initBtn);

        prevBtn  .onClick = [this] { presetManager.selectPrevious(); };
        nextBtn  .onClick = [this] { presetManager.selectNext(); };
        initBtn  .onClick = [this] { presetManager.resetToInit(); };
        saveBtn  .onClick = [this] { showSaveDialog(); };
        deleteBtn.onClick = [this] { showDeleteDialog(); };

        rebuildCombo();

        presetManager.onPresetListChanged    = [this] { rebuildCombo(); };
        presetManager.onCurrentPresetChanged = [this] { rebuildCombo(); };
    }

    ~PresetBar() override
    {
        presetManager.onPresetListChanged    = nullptr;
        presetManager.onCurrentPresetChanged = nullptr;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 6);
        prevBtn .setBounds (r.removeFromLeft (28));
        r.removeFromLeft (4);
        combo   .setBounds (r.removeFromLeft (220));
        r.removeFromLeft (4);
        nextBtn .setBounds (r.removeFromLeft (28));
        r.removeFromLeft (10);
        saveBtn .setBounds (r.removeFromLeft (56));
        r.removeFromLeft (4);
        deleteBtn.setBounds (r.removeFromLeft (52));
        r.removeFromLeft (4);
        initBtn .setBounds (r.removeFromLeft (52));
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        juce::ColourGradient bg (juce::Colour (0xff232a35), bounds.getCentreX(), bounds.getY(),
                                  juce::Colour (0xff14181f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colour (0xff343d4b));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    }

private:
    void rebuildCombo()
    {
        const juce::ScopedValueSetter<bool> guard (suppressCombo, true);
        combo.clear (juce::dontSendNotification);

        const auto factory = presetManager.getFactoryPresetNames();
        const auto user    = presetManager.getUserPresetNames();

        int id = 1;
        if (! factory.isEmpty())
        {
            combo.addSectionHeading ("Factory");
            for (auto& n : factory) combo.addItem (n, id++);
        }
        if (! user.isEmpty())
        {
            combo.addSeparator();
            combo.addSectionHeading ("User");
            for (auto& n : user) combo.addItem (n, id++);
        }

        const auto all = presetManager.getAllPresetNames();
        const int idx = all.indexOf (presetManager.getCurrentPresetName());
        if (idx >= 0)
            combo.setSelectedId (idx + 1, juce::dontSendNotification);
        else
            combo.setText (presetManager.getCurrentPresetName(), juce::dontSendNotification);
    }

    void showSaveDialog()
    {
        auto* aw = new juce::AlertWindow ("Save Preset",
                                          "Enter a name for this preset:",
                                          juce::MessageBoxIconType::NoIcon);

        const auto suggested = presetManager.isFactoryPreset (presetManager.getCurrentPresetName())
                                 ? juce::String ("My Hi-Hat")
                                 : presetManager.getCurrentPresetName();

        aw->addTextEditor ("name", suggested, "Name");
        aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        aw->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, aw] (int result)
            {
                if (result == 1)
                {
                    const auto name = aw->getTextEditorContents ("name").trim();
                    if (name.isEmpty())
                        return;

                    if (presetManager.isFactoryPreset (name))
                    {
                        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                            .withIconType (juce::MessageBoxIconType::WarningIcon)
                            .withTitle ("Reserved Name")
                            .withMessage ("\"" + name + "\" is a factory preset. Choose a different name.")
                            .withButton ("OK"), nullptr);
                        return;
                    }

                    presetManager.saveUserPreset (name);
                }
            }), true);
    }

    void showDeleteDialog()
    {
        const auto current = presetManager.getCurrentPresetName();
        if (! presetManager.isUserPreset (current))
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::InfoIcon)
                .withTitle ("Delete Preset")
                .withMessage ("Factory presets cannot be deleted.")
                .withButton ("OK"), nullptr);
            return;
        }

        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withTitle ("Delete Preset")
            .withMessage ("Delete user preset \"" + current + "\"?")
            .withButton ("Delete")
            .withButton ("Cancel"),
            [this, current] (int result)
            {
                if (result == 1)
                    presetManager.deleteUserPreset (current);
            });
    }

    PresetManager& presetManager;

    juce::ComboBox   combo;
    juce::TextButton prevBtn, nextBtn, saveBtn, deleteBtn, initBtn;
    bool             suppressCombo = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
