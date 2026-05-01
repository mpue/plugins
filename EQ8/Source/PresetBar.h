/*
  ==============================================================================

    PresetBar.h
    Created: Preset selection bar UI component
    Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PresetManager.h"

class PresetBar : public juce::Component
{
public:
    PresetBar(PresetManager& pm) : presetManager(pm)
    {
        addAndMakeVisible(presetLabel);
        presetLabel.setText("Preset:", juce::dontSendNotification);
        presetLabel.setJustificationType(juce::Justification::centredRight);
        presetLabel.setFont(juce::Font(14.0f));

        addAndMakeVisible(presetComboBox);
        refreshPresetList();
        presetComboBox.onChange = [this]
        {
            auto name = presetComboBox.getText();
            if (name.isNotEmpty() && name != presetManager.getCurrentPresetName())
            {
                presetManager.loadPreset(name);
                notifyChange();
            }
        };

        addAndMakeVisible(prevButton);
        prevButton.setButtonText("<");
        prevButton.onClick = [this]
        {
            presetManager.loadPreviousPreset();
            refreshPresetList();
            notifyChange();
        };

        addAndMakeVisible(nextButton);
        nextButton.setButtonText(">");
        nextButton.onClick = [this]
        {
            presetManager.loadNextPreset();
            refreshPresetList();
            notifyChange();
        };

        addAndMakeVisible(saveButton);
        saveButton.setButtonText("Save");
        saveButton.onClick = [this] { saveCurrentPreset(); };

        addAndMakeVisible(saveAsButton);
        saveAsButton.setButtonText("Save As");
        saveAsButton.onClick = [this] { savePresetAs(); };

        addAndMakeVisible(deleteButton);
        deleteButton.setButtonText("Delete");
        deleteButton.onClick = [this] { deleteCurrentPreset(); };
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff222222));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

        g.setColour(juce::Colour(0xff404040));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(8, 4);

        presetLabel.setBounds(bounds.removeFromLeft(60));
        bounds.removeFromLeft(5);

        prevButton.setBounds(bounds.removeFromLeft(30));
        bounds.removeFromLeft(3);
        nextButton.setBounds(bounds.removeFromLeft(30));
        bounds.removeFromLeft(8);

        deleteButton.setBounds(bounds.removeFromRight(70));
        bounds.removeFromRight(5);
        saveAsButton.setBounds(bounds.removeFromRight(70));
        bounds.removeFromRight(5);
        saveButton.setBounds(bounds.removeFromRight(70));
        bounds.removeFromRight(8);

        presetComboBox.setBounds(bounds);
    }

    void refreshPresetList()
    {
        presetComboBox.clear(juce::dontSendNotification);
        auto names = presetManager.getPresetNames();

        for (int i = 0; i < names.size(); ++i)
            presetComboBox.addItem(names[i], i + 1);

        int idx = names.indexOf(presetManager.getCurrentPresetName());
        if (idx >= 0)
            presetComboBox.setSelectedId(idx + 1, juce::dontSendNotification);
    }

    std::function<void()> onPresetChanged;

private:
    void saveCurrentPreset()
    {
        auto name = presetManager.getCurrentPresetName();

        if (presetManager.isFactoryPreset(name))
        {
            savePresetAs();
            return;
        }

        presetManager.savePreset(name);
    }

    void savePresetAs()
    {
        auto* dialog = new juce::AlertWindow("Save Preset",
                                              "Enter a name for the preset:",
                                              juce::AlertWindow::NoIcon);
        dialog->addTextEditor("name", presetManager.getCurrentPresetName());
        dialog->addButton("Save", 1);
        dialog->addButton("Cancel", 0);

        dialog->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, dialog](int result)
            {
                if (result == 1)
                {
                    auto name = dialog->getTextEditorContents("name").trim();
                    if (name.isNotEmpty() && !presetManager.isFactoryPreset(name))
                    {
                        presetManager.savePreset(name);
                        refreshPresetList();
                        notifyChange();
                    }
                }
                delete dialog;
            }), false);
    }

    void deleteCurrentPreset()
    {
        auto name = presetManager.getCurrentPresetName();

        if (presetManager.isFactoryPreset(name))
            return;

        auto* dialog = new juce::AlertWindow("Delete Preset",
                                              "Delete preset \"" + name + "\"?",
                                              juce::AlertWindow::WarningIcon);
        dialog->addButton("Delete", 1);
        dialog->addButton("Cancel", 0);

        dialog->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, dialog, name](int result)
            {
                if (result == 1)
                {
                    presetManager.deletePreset(name);
                    presetManager.loadPreset("Default");
                    refreshPresetList();
                    notifyChange();
                }
                delete dialog;
            }), false);
    }

    void notifyChange()
    {
        if (onPresetChanged)
            onPresetChanged();
    }

    PresetManager& presetManager;

    juce::Label presetLabel;
    juce::ComboBox presetComboBox;
    juce::TextButton saveButton;
    juce::TextButton saveAsButton;
    juce::TextButton deleteButton;
    juce::TextButton prevButton;
    juce::TextButton nextButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};
