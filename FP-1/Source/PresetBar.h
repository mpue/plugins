/*
  ==============================================================================

    PresetBar.h
    Strip with previous / next, preset menu, save and delete actions.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PresetManager.h"

class PresetBar : public juce::Component
{
public:
    explicit PresetBar (PresetManager& m) : manager (m)
    {
        prevBtn.setButtonText ("<");
        nextBtn.setButtonText (">");
        saveBtn.setButtonText ("Save");
        delBtn .setButtonText ("Delete");

        for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &delBtn })
            addAndMakeVisible (b);

        addAndMakeVisible (presetMenu);
        presetMenu.setTextWhenNothingSelected ("Init");

        rebuildMenu();

        prevBtn.onClick = [this] { manager.prev(); };
        nextBtn.onClick = [this] { manager.next(); };
        saveBtn.onClick = [this] { showSaveDialog(); };
        delBtn .onClick = [this] { confirmDelete(); };

        presetMenu.onChange = [this]
        {
            const int idx = presetMenu.getSelectedItemIndex();
            if (idx >= 0) manager.loadPreset (idx);
        };

        manager.onListChanged    = [this] { rebuildMenu(); };
        manager.onCurrentChanged = [this] { syncSelection(); };

        syncSelection();
    }

    ~PresetBar() override
    {
        manager.onListChanged    = nullptr;
        manager.onCurrentChanged = nullptr;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4, 6);

        prevBtn.setBounds (r.removeFromLeft (28));
        r.removeFromLeft (4);
        nextBtn.setBounds (r.removeFromLeft (28));
        r.removeFromLeft (8);

        delBtn .setBounds (r.removeFromRight (60));
        r.removeFromRight (4);
        saveBtn.setBounds (r.removeFromRight (60));
        r.removeFromRight (8);

        presetMenu.setBounds (r);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff141a26));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colour (0xff2a3344));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    }

private:
    void rebuildMenu()
    {
        presetMenu.clear (juce::dontSendNotification);
        for (int i = 0; i < manager.getNumPresets(); ++i)
        {
            if (auto* p = manager.getPreset (i))
                presetMenu.addItem (p->displayName(), i + 1);
        }
        syncSelection();
    }

    void syncSelection()
    {
        presetMenu.setSelectedItemIndex (manager.getCurrentIndex(), juce::dontSendNotification);
    }

    void showSaveDialog()
    {
        auto* aw = new juce::AlertWindow ("Save preset",
                                          "Enter a name for the new preset:",
                                          juce::AlertWindow::NoIcon);
        aw->addTextEditor ("name", manager.getCurrentName(), {});
        aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        aw->enterModalState (true,
            juce::ModalCallbackFunction::create (
                [this, aw] (int result)
                {
                    std::unique_ptr<juce::AlertWindow> owner (aw);
                    if (result != 1) return;
                    const auto name = aw->getTextEditorContents ("name");
                    juce::String err;
                    if (! manager.saveAsUserPreset (name, err))
                    {
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::AlertWindow::WarningIcon, "Could not save preset", err);
                    }
                }), false);
    }

    void confirmDelete()
    {
        if (auto* p = manager.getPreset (manager.getCurrentIndex()))
        {
            if (p->kind != PresetManager::Kind::User)
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::InfoIcon,
                    "Factory preset",
                    "Factory presets cannot be deleted.");
                return;
            }

            juce::AlertWindow::showOkCancelBox (
                juce::AlertWindow::QuestionIcon,
                "Delete preset",
                "Are you sure you want to delete \"" + p->name + "\"?",
                "Delete", "Cancel", nullptr,
                juce::ModalCallbackFunction::create (
                    [this] (int result)
                    {
                        if (result != 1) return;
                        juce::String err;
                        if (! manager.deleteCurrentUserPreset (err))
                        {
                            juce::AlertWindow::showMessageBoxAsync (
                                juce::AlertWindow::WarningIcon, "Could not delete preset", err);
                        }
                    }));
        }
    }

    PresetManager& manager;
    juce::ComboBox  presetMenu;
    juce::TextButton prevBtn, nextBtn, saveBtn, delBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
