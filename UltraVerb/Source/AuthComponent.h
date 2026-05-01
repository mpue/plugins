/*
  ==============================================================================

	AuthComponent.h
	Created: 4 Jun 2024 4:34:41pm
	Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "KeyGen.h"


//==============================================================================
/*
*/
class SerialNumberInputFilter : public juce::TextEditor::InputFilter
{
public:
	juce::String filterNewText(juce::TextEditor& /*editor*/, const juce::String& newInput) override
	{
		juce::String filteredInput = newInput.retainCharacters("0123456789ABCDEF-");
		if (filteredInput.length() > 19) // 16 digits + 3 hyphens
			filteredInput = filteredInput.substring(0, 19);

		// Ensure hyphens are in the correct positions
		if (filteredInput.length() > 4 && filteredInput[4] != '-')
			filteredInput = filteredInput.substring(0, 4) + "-" + filteredInput.substring(4);
		if (filteredInput.length() > 9 && filteredInput[9] != '-')
			filteredInput = filteredInput.substring(0, 9) + "-" + filteredInput.substring(9);
		if (filteredInput.length() > 14 && filteredInput[14] != '-')
			filteredInput = filteredInput.substring(0, 14) + "-" + filteredInput.substring(14);

		return filteredInput;
	}
};

// ...



class AuthComponent : public juce::Component, public juce::TextEditor::Listener, public juce::Button::Listener
{
public:
	AuthComponent()
	{
		juce::Font defaultFont("Arial", 25.0f, juce::Font::plain); // "Arial" is the font type, 20.0f is the font size, and juce::Font::bold sets the font style to bold

		// emailLabel.setColour(juce::Label::textColourId, juce::Colours::black);
		// serialNumberLabel.setColour(juce::Label::textColourId, juce::Colours::black);

		addAndMakeVisible(emailLabel);
		emailLabel.setText("Email address", juce::dontSendNotification);		
		emailLabel.setFont(defaultFont);
		// emailLabel.attachToComponent(&emailEditor, true);

		addAndMakeVisible(emailEditor);
		emailEditor.setMultiLine(false);
		emailEditor.setReturnKeyStartsNewLine(false);
		emailEditor.setReadOnly(false);
		emailEditor.setCaretVisible(true);
		emailEditor.setPopupMenuEnabled(true);
		emailEditor.setFont(defaultFont);
		emailEditor.setText("");

		addAndMakeVisible(serialNumberLabel);
		serialNumberLabel.setText("Serial Number", juce::dontSendNotification);
		serialNumberLabel.setFont(defaultFont);
		// serialNumberLabel.attachToComponent(&serialNumberEditor, true);


		addAndMakeVisible(serialNumberEditor);
		serialNumberEditor.setMultiLine(false);
		serialNumberEditor.setReturnKeyStartsNewLine(false);
		serialNumberEditor.setReadOnly(false);
		serialNumberEditor.setCaretVisible(true);
		serialNumberEditor.setPopupMenuEnabled(true);
		serialNumberEditor.setFont(defaultFont);
		serialNumberEditor.setText("");

		serialNumberEditor.setInputFilter(new SerialNumberInputFilter(), true);

		emailEditor.addListener(this);
		serialNumberEditor.addListener(this);
	
		validateButtom.setButtonText("Validate");
		addAndMakeVisible(validateButtom);
		validateButtom.addListener(this);

	}

	void paint(juce::Graphics& g) override
	{
		g.fillAll(juce::Colours::lightcyan);
	}

	void resized() override
	{
		emailLabel.setBounds(10,  20,getWidth() - 20, 32); // Set bounds for emailLabel
		emailEditor.setBounds(10, 60, getWidth() - 20, 32);
		serialNumberLabel.setBounds(10, 120, getWidth() - 20, 32);
		serialNumberEditor.setBounds(10, 160, getWidth() - 20, 32);
		// place validate Button on the bottom center of the control
		validateButtom.setBounds(getWidth() / 2 - 50, getHeight() - 50, 100, 30);

	}

private:
	void textEditorTextChanged(juce::TextEditor& editor) override
	{
		if (&editor == &emailEditor)
		{
			// Handle email text change
		}
		else if (&editor == &serialNumberEditor)
		{
			juce::String text = editor.getText();
			text = text.retainCharacters("0123456789ABCDEF");

			// Insert hyphens at the correct positions
			if (text.length() > 4)
				text = text.substring(0, 4) + "-" + text.substring(4);
			if (text.length() > 9)
				text = text.substring(0, 9) + "-" + text.substring(9);
			if (text.length() > 14)
				text = text.substring(0, 14) + "-" + text.substring(14);

			if (text.length() > 19) {
				text = text.substring(0, 19);
			}

			// Update the text without triggering a new textEditorTextChanged event
			editor.removeListener(this);
			editor.setText(text, juce::dontSendNotification);
			editor.addListener(this);
		}
	}

	void buttonClicked(Button* button)  override {
		if (button == &validateButtom) {
			juce::String email = emailEditor.getText();
			juce::String serialNumber = serialNumberEditor.getText();

			if (keyGen.isValidKey(serialNumber.toStdString() , email.toStdString())) {
				juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Success", "Serial number is valid.", "OK", this,
					juce::ModalCallbackFunction::create([this](int) { setVisible(false); }));
			}
			else {
				juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Error", "Serial number is invalid.");
			}
		}
	}

	juce::TextEditor emailEditor;
	juce::TextEditor serialNumberEditor;
	juce::Label emailLabel;
	juce::Label serialNumberLabel;
	juce::TextButton validateButtom;


	KeyGen keyGen;

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuthComponent)
};
