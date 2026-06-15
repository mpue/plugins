/*
  ==============================================================================
    ActivationOverlay.cpp
    Author:  Matthias Pueski   (portiert aus Synthlabs ActivationDialog)
  ==============================================================================
*/

#include "ActivationOverlay.h"
#include <thread>

namespace pike { namespace gui {

//==============================================================================
ActivationOverlay::ActivationOverlay (bool reactivation,
                                      int  trialDaysRemaining,
                                      std::function<void()> onActivatedCb,
                                      std::function<void()> onSkipDemoCb)
    : onActivated (std::move (onActivatedCb)),
      onSkipDemo  (std::move (onSkipDemoCb)),
      trialDays   (trialDaysRemaining)
{
    // Klicks/Tasten nicht zum verdeckten Editor durchreichen.
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);

    headline.setText (reactivation
                          ? "This device has not been activated for Pike yet."
                          : "Please activate your Pike license.",
                      juce::dontSendNotification);
    headline.setJustificationType (juce::Justification::centredLeft);
    headline.setFont (juce::Font (16.0f, juce::Font::bold));
    addAndMakeVisible (headline);

    // Demo-Status: solange Tage uebrig sind, darf der User ueberspringen.
    demoLabel.setJustificationType (juce::Justification::centredLeft);
    demoLabel.setMinimumHorizontalScale (1.0f);
    if (trialDays > 0)
    {
        demoLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
        demoLabel.setText ("Demo mode: " + juce::String (trialDays)
                               + (trialDays == 1 ? " day left." : " days left."),
                           juce::dontSendNotification);
        skipButton.onClick = [this] { if (onSkipDemo) onSkipDemo(); };
        addAndMakeVisible (skipButton);
    }
    else
    {
        demoLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
        demoLabel.setText ("Your 30-day demo has expired. Please activate to keep using Pike.",
                           juce::dontSendNotification);
    }
    addAndMakeVisible (demoLabel);

    addAndMakeVisible (emailLabel);
    addAndMakeVisible (keyLabel);

    emailEditor.setTextToShowWhenEmpty ("name@example.com", juce::Colours::grey);
    keyEditor.setTextToShowWhenEmpty ("XXXX-XXXX-XXXX-XXXX", juce::Colours::grey);
    addAndMakeVisible (emailEditor);
    addAndMakeVisible (keyEditor);

    activateButton.onClick = [this] { activateClicked(); };
    serverButton.onClick   = [this] { serverClicked(); };
    addAndMakeVisible (activateButton);
    addAndMakeVisible (serverButton);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (statusLabel);
}

ActivationOverlay::~ActivationOverlay() {}

//==============================================================================
void ActivationOverlay::paint (juce::Graphics& g)
{
    // Verdeckt den gesamten Editor (fast deckend) + zentrierte Karte.
    g.fillAll (juce::Colour (0xf2000000));

    g.setColour (juce::Colour (0xff1b2028));
    g.fillRoundedRectangle (cardBounds.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff3a4250));
    g.drawRoundedRectangle (cardBounds.toFloat(), 10.0f, 1.0f);
}

void ActivationOverlay::resized()
{
    // Zentrierte Karte mit fester Groesse, unabhaengig von der Editor-Skalierung.
    const int cardW = 460, cardH = 296;
    cardBounds = juce::Rectangle<int> (0, 0, cardW, cardH)
                     .withCentre (getLocalBounds().getCentre());

    auto area = cardBounds.reduced (16);

    headline.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);
    demoLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (12);

    auto row = [&area] (int h) { auto r = area.removeFromTop (h); area.removeFromTop (8); return r; };

    {
        auto r = row (26);
        emailLabel.setBounds (r.removeFromLeft (96));
        emailEditor.setBounds (r);
    }
    {
        auto r = row (26);
        keyLabel.setBounds (r.removeFromLeft (96));
        keyEditor.setBounds (r);
    }

    statusLabel.setBounds (row (40));

    auto buttons = area.removeFromBottom (30);
    activateButton.setBounds (buttons.removeFromRight (130));
    buttons.removeFromRight (8);
    serverButton.setBounds (buttons.removeFromLeft (110));
    if (skipButton.isVisible())
    {
        buttons.removeFromLeft (8);
        skipButton.setBounds (buttons.removeFromLeft (140));
    }
}

//==============================================================================
void ActivationOverlay::setBusy (bool busy)
{
    activateButton.setEnabled (! busy);
    serverButton.setEnabled (! busy);
    emailEditor.setEnabled (! busy);
    keyEditor.setEnabled (! busy);
}

void ActivationOverlay::activateClicked()
{
    const juce::String email = emailEditor.getText().trim();
    const juce::String key   = keyEditor.getText().trim();

    if (email.isEmpty() || key.isEmpty())
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
        statusLabel.setText ("Please enter your email and license key.", juce::dontSendNotification);
        return;
    }

    setBusy (true);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setText ("Activating...", juce::dontSendNotification);

    // Aktivierung (Netzwerk) im Hintergrund — UI nicht blockieren. Ergebnis
    // wird ueber eine SafePointer-gesicherte callAsync zurueck auf den
    // Message-Thread gebracht.
    juce::Component::SafePointer<ActivationOverlay> safe (this);
    std::thread ([safe, key, email]()
    {
        const ActivationResult result = LicenseManager::getInstance()->activate (key, email);
        juce::MessageManager::callAsync ([safe, result]()
        {
            if (safe != nullptr)
                safe->onActivationFinished (result);
        });
    }).detach();
}

void ActivationOverlay::serverClicked()
{
    // Erlaubt das Umstellen des Lizenzservers vor der Aktivierung.
    auto* w = new juce::AlertWindow (
        "License Server",
        "Base URL of the license server (without the trailing /activate). "
        "Leave empty to use the built-in default.",
        juce::AlertWindow::NoIcon);

    w->setLookAndFeel (&getLookAndFeel());
    w->addTextEditor ("url", LicenseManager::getServerUrlOverride(), "URL:");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true,
        juce::ModalCallbackFunction::create ([w](int result)
        {
            if (result == 1)
                LicenseManager::setServerUrlOverride (w->getTextEditorContents ("url"));
        }),
        true /* deleteWhenDismissed */);
}

void ActivationOverlay::onActivationFinished (ActivationResult result)
{
    if (result == ActivationResult::Success)
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
        statusLabel.setText ("Activation successful.", juce::dontSendNotification);
        if (onActivated)
            onActivated();
        return;
    }

    setBusy (false);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::tomato);
    statusLabel.setText (messageForResult (result), juce::dontSendNotification);
}

juce::String ActivationOverlay::messageForResult (ActivationResult result)
{
    switch (result)
    {
        case ActivationResult::InvalidKey:
            return "Invalid license key or email.";
        case ActivationResult::Refunded:
            return "This license was refunded and is no longer valid.";
        case ActivationResult::LimitReached:
            return "Maximum number of devices reached. Please deactivate a device "
                   "or contact support.";
        case ActivationResult::NetworkError:
            return "Could not reach the license server. Please check your internet connection.";
        case ActivationResult::ServerError:
            return "Unexpected server response. Please try again later.";
        case ActivationResult::SodiumError:
            return "Could not initialize cryptography.";
        case ActivationResult::Success:
        case ActivationResult::Unknown:
        default:
            return "Activation failed. Please contact support.";
    }
}

}} // namespace pike::gui
