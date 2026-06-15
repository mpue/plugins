/*
  ==============================================================================
    ActivationOverlay.h
    Author:  Matthias Pueski

    In-Plugin-Aktivierungs-Gate (portiert aus Synthlabs ActivationDialog). Da ein
    Plugin kein eigenes Fenster besitzt und den Host nicht beenden kann, ist dies
    ein Overlay-Component, das den gesamten Editor verdeckt und Eingaben abfaengt,
    solange das Plugin nicht aktiviert ist. Felder fuer E-Mail + License-Key, die
    Aktivierung laeuft im Hintergrund-Thread, damit die UI nicht blockiert.

    Bei Erfolg wird onActivated() auf dem Message-Thread aufgerufen (der Editor
    blendet das Overlay aus und teilt dem Processor den neuen Status mit).
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "LicenseManager.h"

namespace pike { namespace gui {

class ActivationOverlay : public juce::Component
{
public:
    /** @param reactivation        true bei Machine-Mismatch (anderer Einleitungstext).
        @param trialDaysRemaining  verbleibende Demo-Tage; > 0 blendet den
                                    "Continue in demo"-Button ein, sonst gilt das
                                    harte Gate (kein Ueberspringen mehr).
        @param onActivated         auf dem Message-Thread nach erfolgreicher Aktivierung.
        @param onSkipDemo          auf dem Message-Thread, wenn der User die Demo
                                    weiternutzt (Overlay schliessen). */
    ActivationOverlay (bool reactivation,
                       int  trialDaysRemaining,
                       std::function<void()> onActivated,
                       std::function<void()> onSkipDemo);
    ~ActivationOverlay() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void activateClicked();
    void serverClicked();
    void onActivationFinished (ActivationResult result);
    void setBusy (bool busy);

    static juce::String messageForResult (ActivationResult result);

    std::function<void()> onActivated;
    std::function<void()> onSkipDemo;
    int                   trialDays = 0;

    juce::Label        headline;
    juce::Label        demoLabel;
    juce::Label        emailLabel  { {}, "Email:" };
    juce::Label        keyLabel    { {}, "License key:" };
    juce::TextEditor   emailEditor;
    juce::TextEditor   keyEditor;
    juce::TextButton   activateButton { "Activate" };
    juce::TextButton   serverButton   { "Server..." };
    juce::TextButton   skipButton     { "Continue in demo" };
    juce::Label        statusLabel;

    juce::Rectangle<int> cardBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationOverlay)
};

}} // namespace pike::gui
