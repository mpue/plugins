/*
  ==============================================================================

	MainUI.h
	Created: 30 May 2024 2:22:23pm
	Author:  mpue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../DSP/ReverbController.h"
#include "../Parameters.h"
#include "SpectrumAnalyzer.h"
#include "ParticleSystemComponent.h"
#include "../AuthComponent.h"

#define PADDING 180
#define MARGIN  20
#define SMALL_GROUP_PADDING 100


class MainUI : public juce::Component,
	public juce::Slider::Listener,
	public juce::ToggleButton::Listener
{
public:

	MainUI(UltraVerbAudioProcessor& p) : processor(p)
	{
		this->reverb = p.reverb.get();
		
		setSize(1280, 1150);
		setBounds(0, 0, 1280, 1150);

		diffusionGroup.reset(new juce::GroupComponent("diffusion_group", "Diffusion"));
		addAndMakeVisible(diffusionGroup.get());
		diffusionGroup->setBounds(90, 100, 1200, 400);

		configureSlider(diffusion_delay_slider, "diffusion_delay_slider", diffusion_delay_label, "Delay", diffusionGroup, 0, 0, 128, 0, 1, 0.01, PADDING);
		configureSlider(diffusion_modamt_slider, "diffusion_modamt_slider", diffusion_modamt_label, "Mod Amt", diffusionGroup, 1, 0, 128, 0, 1, 0.01, PADDING);
		configureSlider(diffusion_feedback_slider, "diffusion_feedback_slider", diffusion_feedback_label, "Feedback", diffusionGroup, 2, 0, 128, 0, 1, 0.01, PADDING);
		configureSlider(diffusion_modrate_slider, "diffusion_modrate_slider", diffusion_modrate_label, "Mod Rate", diffusionGroup, 3, 0, 128, 0, 1, 0.01, PADDING);

		// lr TOP =========================================================================================== // 

		lateReflectionsGroupL.reset(new juce::GroupComponent("late_reflections_group", "Late Reflections"));
		addAndMakeVisible(lateReflectionsGroupL.get());
		lateReflectionsGroupL->setBounds(40, 420, 1200, 400);

		configureSlider(lateref_size_slider, "lateref_size_slider", lateref_size_label, "Size", lateReflectionsGroupL, 0, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(lateref_size_modamt_slider, "lateref_size_modamt_slider", lateref_size_modamt_label, "Mod Amt", lateReflectionsGroupL, 1, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(lateref_size_modrate_slider, "lateref_size_modrate_slider", lateref_size_modrate_label, "Mod Rate", lateReflectionsGroupL, 0, 1, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(lateref_decay_slider, "lateref_decay_slider", lateref_decay_label, "Decay", lateReflectionsGroupL, 1, 1, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		// BOTTOM 

		lateReflectionsGroupR.reset(new juce::GroupComponent("late_reflections_group", "Late Reflections"));
		addAndMakeVisible(lateReflectionsGroupR.get());
		lateReflectionsGroupR->setBounds(980, 420, 1200, 400);

		configureSlider(lateref_delay_slider, "lateref_delay_slider", lateref_delay_label, "Delay", lateReflectionsGroupR, 0, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(lateref_delay_modamt_slider, "lateref_delay_modamt_slider", lateref_delay_modamt_label, "Mod Amt", lateReflectionsGroupR, 1, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(lateref_feedback_slider, "lateref_size_modrate_slider", lateref_feedback_label, "Feedback", lateReflectionsGroupR, 0, 1, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(lateref_delay_modrate_slider, "lateref_delay_modrate_slider", lateref_delay_modrate_label, "Mod Rate", lateReflectionsGroupR, 1, 1, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);

		// eq 

		eqGroup.reset(new juce::GroupComponent("eqGroup", "EQ"));
		addAndMakeVisible(eqGroup.get());
		eqGroup->setBounds(400, 5, 1000, 200);

		configureSlider(eq_low_slider, "eq_low_slider", eq_low_label, "Low Freq", eqGroup, 0, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(eq_high_slider, "eq_high_slider", eq_high_label, "High Freq", eqGroup, 1, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(eq_cutoff_slider, "eq_cutoff_slider", eq_cutoff_label, "Cutoff", eqGroup, 2, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(eq_low_gain_slider, "eq_low_gain_slider", eq_low_gain_label, "Low Gain", eqGroup, 3, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(eq_high_gain_slider, "eq_high_gain_slider", eq_high_gain_label, "High Gain", eqGroup, 4, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);

		tapGroup.reset(new juce::GroupComponent("tap_group", "Tap Delay"));
		addAndMakeVisible(tapGroup.get());
		tapGroup->setBounds(90, 900, 1200, 200);

		configureSlider(tap_count_slider, "tap_count_slider", tap_count__label, "Count", tapGroup, 0, 0, 128, 0, 256, 1, PADDING);
		configureSlider(tap_decay_slider, "tap_decay_slider", tap_decay_label, "Decay", tapGroup, 1, 0, 128, 0, 1, 0.01, PADDING);
		configureSlider(tap_predelay_slider, "tap_predelay_slider", tap_predelay_label, "Feedback", tapGroup, 2, 0, 128, 0, 1, 0.01, PADDING);
		configureSlider(tap_length_slider, "tap_length_slider", tap_length_label, "Mod Rate", tapGroup, 3, 0, 128, 0, 1, 0.01, PADDING);

		// mixer TOP =========================================================================================== // 

		mixerGroup.reset(new juce::GroupComponent("mixer_group", "Mixer"));
		addAndMakeVisible(mixerGroup.get());
		mixerGroup->setBounds(200, 1050, 1000, 200);

		configureSlider(mixer_dry_slider, "mixer_dry_slider", mixer_dry_label, "Dry", mixerGroup, 0, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(mixer_early_slider, "mixer_early_slider", mixer_early_label, "Early", mixerGroup, 1, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(mixer_late_slider, "mixer_late_slider", mixer_late_label, "Late", mixerGroup, 2, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);

		// mixer BOTTOM 

		configureSlider(mixer_input_slider, "mixer_input_slider", mixer_input_label, "Input", mixerGroup, 3, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(mixer_highcut_slider, "mixer_highcut_slider", mixer_highcut_label, "High Cut", mixerGroup, 4, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(mixer_lowcut_slider, "mixer_lowcut_slider", mixer_lowcut_label, "Low Cut", mixerGroup, 5, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);
		configureSlider(mixer_cross_slider, "mixer_cross_slider", mixer_cross_label, "Cross seed", mixerGroup, 6, 0, 64, 0, 1, 0.01, SMALL_GROUP_PADDING);

		particleSystemComponent.reset(new ParticleSystemComponent());
		addAndMakeVisible(particleSystemComponent.get());
		particleSystemComponent->setBounds(370, 360, 520, 420);

		spectrum.reset(new SpectrumAnalyzer());
		addAndMakeVisible(spectrum.get());
		spectrum->setBounds(370, 360, 520, 420);

		ui_layer_toggle_particles.reset(new juce::ToggleButton("Particles"));
		addAndMakeVisible(ui_layer_toggle_particles.get());
		ui_layer_toggle_particles->setBounds(380,370, 130, 32);
		ui_layer_toggle_particles->setToggleState(true,false);
		ui_layer_toggle_particles->addListener(this);

		ui_layer_toggle_spectrum.reset(new juce::ToggleButton("Spectrum"));
		addAndMakeVisible(ui_layer_toggle_spectrum.get());
		ui_layer_toggle_spectrum->setBounds(550, 370, 130, 32);
		ui_layer_toggle_spectrum->setToggleState(true, false);
		ui_layer_toggle_spectrum->addListener(this);

		postpre_toggle.reset(new juce::ToggleButton("Post"));
		addAndMakeVisible(postpre_toggle.get());
		postpre_toggle->setBounds(720,370, 130, 32);
		postpre_toggle->addListener(this);

		auth_component.reset(new AuthComponent());
		addAndMakeVisible(auth_component.get());
		auth_component->setBounds(370, 360, 520, 420);



	}

	~MainUI() override
	{
		diffusionGroup = nullptr;
		diffusion_delay_slider = nullptr;
		diffusion_modamt_slider = nullptr;
		diffusion_feedback_slider = nullptr;
		diffusion_modrate_slider = nullptr;
		lateReflectionsGroupL = nullptr;
		lateReflectionsGroupR = nullptr;
		lateref_size_slider = nullptr;
		lateref_size_modamt_slider = nullptr;
		lateref_delay_slider = nullptr;
		lateref_delay_modamt_slider = nullptr;
		lateref_decay_slider = nullptr;
		lateref_size_modrate_slider = nullptr;
		lateref_feedback_slider = nullptr;
		lateref_delay_modrate_slider = nullptr;
		eqGroup = nullptr;
		eq_low_slider = nullptr;
		eq_low_gain_slider = nullptr;
		eq_high_slider = nullptr;
		eq_high_gain_slider = nullptr;
		eq_cutoff_slider = nullptr;
	}

	void paint(juce::Graphics& g) override
	{
		// g.drawImage(juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize), 0, 0, 512, 122, 0, 0, 512, 122);
		// g.drawImage(juce::ImageCache::getFromMemory(BinaryData::q_png, BinaryData::q_pngSize), 630, 320, 270, 184, 0, 0, 270, 184);
		// g.drawImage(juce::ImageCache::getFromMemory(BinaryData::ui_png, BinaryData::ui_pngSize), 0, 0, 1280, 1150, 0, 0, getWidth(), getHeight());

		juce::Rectangle<float> bounds(0, 0, getWidth(), getHeight());
		juce::Image img = juce::ImageCache::getFromMemory(BinaryData::ui_png, BinaryData::ui_pngSize);

		// Apply the transform and draw the image
		g.drawImage(img, bounds);
		// Create an AffineTransform for scaling
	}

	void resized() override
	{
	}

	void configureSlider(std::unique_ptr<juce::Slider>& slider, juce::String name,
		std::unique_ptr<juce::Label>& label, juce::String labelText,
		std::unique_ptr<juce::GroupComponent>& group,
		int x, int y, int size, float start, float end, float stepsize, float padding) {
		slider.reset(new juce::Slider(name));
		group->addAndMakeVisible(slider.get());
		slider->setRange(0, 1, 0.01f);
		slider->setBounds(x * (size + padding) + MARGIN, y * (size + padding) + MARGIN, size, size);
		slider->setSliderStyle(juce::Slider::RotaryVerticalDrag);
		slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 80, 20);
		slider->addListener(this);
		/*
		label.reset(new juce::Label(labelText + "_label", labelText));
		group->addAndMakeVisible(label.get());
		label->setFont(juce::Font(12.00f, juce::Font::plain).withTypefaceStyle("Regular"));
		label->setJustificationType(juce::Justification::centred);
		label->setEditable(false, false, false);
		label->setColour(juce::TextEditor::textColourId, juce::Colours::black);
		label->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x00000000));

		label->setBounds(x * (size + PADDING) + MARGIN, y * (size + PADDING) + MARGIN + size * 0.85f, 64, 24);
		*/
	}


	void buttonClicked(juce::Button* button) override {

		bool enabled = button->getToggleState();

		if (button == postpre_toggle.get()) {
			processor.post = enabled;
		}
		else if (button == ui_layer_toggle_spectrum.get()) {
			spectrum->setVisible(enabled);
			repaint();
		}
		else if (button == ui_layer_toggle_particles.get()) {
			particleSystemComponent->setVisible(enabled);
			repaint();
		}
	}

	void sliderValueChanged(juce::Slider* slider) override {

		// Diffusion

		if (slider == diffusion_delay_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EarlyDiffuseDelay, slider->getValue());
		}
		if (slider == diffusion_feedback_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EarlyDiffuseFeedback, slider->getValue());
		}
		if (slider == diffusion_modamt_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EarlyDiffuseModAmount, slider->getValue());
		}
		if (slider == diffusion_modrate_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EarlyDiffuseModRate, slider->getValue());
		}

		// Late reflections

		if (slider == lateref_size_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::LateLineSize, slider->getValue());
		}
		if (slider == lateref_size_modamt_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::LateLineModAmount, slider->getValue());
		}
		if (slider == lateref_delay_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::LateDiffuseDelay, slider->getValue());
		}
		if (slider == lateref_delay_modamt_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::LateDiffuseModAmount, slider->getValue());
		}
		if (slider == lateref_decay_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::LateLineDecay, slider->getValue());
		}
		if (slider == lateref_size_modrate_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::LateLineModRate, slider->getValue());
		}
		if (slider == lateref_feedback_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::LateDiffuseFeedback, slider->getValue());
		}
		if (slider == lateref_delay_modrate_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::LateDiffuseModRate, slider->getValue());
		}

		// EQ

		if (slider == eq_low_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EqLowFreq, slider->getValue());
		}
		if (slider == eq_low_gain_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EqLowGain, slider->getValue());
		}
		if (slider == eq_high_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EqHighFreq, slider->getValue());
		}
		if (slider == eq_high_gain_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EqHighGain, slider->getValue());
		}
		if (slider == eq_cutoff_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EqCutoff, slider->getValue());
		}

		// TAP

		if (slider == tap_count_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::TapCount, slider->getValue());
		}
		if (slider == tap_decay_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::TapDecay, slider->getValue());
		}
		if (slider == tap_predelay_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::TapPredelay, slider->getValue());
		}
		if (slider == tap_length_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::TapLength, slider->getValue());
		}

		// Mixer

		if (slider == mixer_dry_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::DryOut, slider->getValue());
		}
		if (slider == mixer_early_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EarlyOut, slider->getValue());
		}
		if (slider == mixer_late_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::LateOut, slider->getValue());
		}
		if (slider == mixer_input_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::InputMix, slider->getValue());
		}
		if (slider == mixer_highcut_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::HighCut, slider->getValue());
		}
		if (slider == mixer_lowcut_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::LowCut, slider->getValue());
		}
		if (slider == mixer_cross_slider.get()) {
			reverb->SetParameter(Cloudseed::Parameter::EqCrossSeed, slider->getValue());
		}

	}

	std::unique_ptr<SpectrumAnalyzer> spectrum;;
	std::unique_ptr<ParticleSystemComponent> particleSystemComponent;

private:

	std::unique_ptr<juce::GroupComponent> diffusionGroup;

	std::unique_ptr<juce::Slider> diffusion_delay_slider;
	std::unique_ptr<juce::Label> diffusion_delay_label;
	std::unique_ptr<juce::Label> diffusion_modamt_label;
	std::unique_ptr<juce::Slider> diffusion_modamt_slider;
	std::unique_ptr<juce::Slider> diffusion_feedback_slider;
	std::unique_ptr<juce::Label> diffusion_feedback_label;
	std::unique_ptr<juce::Slider> diffusion_modrate_slider;
	std::unique_ptr<juce::Label> diffusion_modrate_label;

	std::unique_ptr<juce::GroupComponent> lateReflectionsGroupL;
	std::unique_ptr<juce::GroupComponent> lateReflectionsGroupR;

	std::unique_ptr<juce::Slider> lateref_size_slider;
	std::unique_ptr<juce::Label> lateref_size_label;
	std::unique_ptr<juce::Slider> lateref_size_modamt_slider;
	std::unique_ptr<juce::Label> lateref_size_modamt_label;
	std::unique_ptr<juce::Slider> lateref_delay_slider;
	std::unique_ptr<juce::Label> lateref_delay_label;
	std::unique_ptr<juce::Slider> lateref_delay_modamt_slider;
	std::unique_ptr<juce::Label> lateref_delay_modamt_label;
	std::unique_ptr<juce::Slider> lateref_decay_slider;
	std::unique_ptr<juce::Label> lateref_decay_label;
	std::unique_ptr<juce::Slider> lateref_size_modrate_slider;
	std::unique_ptr<juce::Label> lateref_size_modrate_label;
	std::unique_ptr<juce::Slider> lateref_feedback_slider;
	std::unique_ptr<juce::Label> lateref_feedback_label;
	std::unique_ptr<juce::Slider> lateref_delay_modrate_slider;
	std::unique_ptr<juce::Label> lateref_delay_modrate_label;

	std::unique_ptr<juce::GroupComponent> eqGroup;
	std::unique_ptr<juce::Slider> eq_low_slider;
	std::unique_ptr<juce::Label> eq_low_label;
	std::unique_ptr<juce::Slider> eq_low_gain_slider;
	std::unique_ptr<juce::Label> eq_low_gain_label;
	std::unique_ptr<juce::Slider> eq_high_slider;
	std::unique_ptr<juce::Label> eq_high_label;
	std::unique_ptr<juce::Slider> eq_high_gain_slider;
	std::unique_ptr<juce::Label> eq_high_gain_label;
	std::unique_ptr<juce::Slider> eq_cutoff_slider;
	std::unique_ptr<juce::Label> eq_cutoff_label;

	std::unique_ptr<juce::GroupComponent> tapGroup;
	std::unique_ptr<juce::Slider> tap_count_slider;
	std::unique_ptr<juce::Label> tap_count__label;
	std::unique_ptr<juce::Slider> tap_decay_slider;
	std::unique_ptr<juce::Label> tap_decay_label;
	std::unique_ptr<juce::Slider> tap_predelay_slider;
	std::unique_ptr<juce::Label> tap_predelay_label;
	std::unique_ptr<juce::Slider> tap_length_slider;
	std::unique_ptr<juce::Label> tap_length_label;

	std::unique_ptr<juce::GroupComponent> mixerGroup;

	std::unique_ptr<juce::Slider> mixer_dry_slider;
	std::unique_ptr<juce::Label> mixer_dry_label;
	std::unique_ptr<juce::Slider> mixer_early_slider;
	std::unique_ptr<juce::Label> mixer_early_label;
	std::unique_ptr<juce::Slider> mixer_late_slider;
	std::unique_ptr<juce::Label> mixer_late_label;
	std::unique_ptr<juce::Slider> mixer_input_slider;
	std::unique_ptr<juce::Label> mixer_input_label;
	std::unique_ptr<juce::Slider> mixer_highcut_slider;
	std::unique_ptr<juce::Label> mixer_highcut_label;
	std::unique_ptr<juce::Slider> mixer_lowcut_slider;
	std::unique_ptr<juce::Label> mixer_lowcut_label;
	std::unique_ptr<juce::Slider> mixer_cross_slider;
	std::unique_ptr<juce::Label> mixer_cross_label;

	std::unique_ptr<juce::ToggleButton> ui_layer_toggle_spectrum;
	std::unique_ptr<juce::ToggleButton> ui_layer_toggle_particles;
	std::unique_ptr<juce::ToggleButton> postpre_toggle;
	std::unique_ptr<AuthComponent> auth_component;

	Cloudseed::ReverbController* reverb;
	UltraVerbAudioProcessor& processor;


	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainUI)
};
