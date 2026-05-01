/*
  ==============================================================================

    AF-1 — Luxurious AutoFilter
    Processor

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>

class AF1AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    AF1AudioProcessor();
    ~AF1AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Public API for the editor / preset manager
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Live values used by the visualiser (range 20..20000 Hz, 0..1 reso)
    float getDisplayCutoffHz()      const noexcept { return displayCutoff.load();    }
    float getDisplayResonance()     const noexcept { return displayResonance.load(); }
    float getEnvelopeLevel()        const noexcept { return displayEnvLevel.load();  }
    float getLfoValue()             const noexcept { return displayLfoValue.load();  }
    int   getFilterTypeIndex()      const noexcept { return displayFilterType.load(); }
    int   getSlopeIndex()           const noexcept { return displaySlope.load();      }
    double getCurrentSampleRate()   const noexcept { return currentSampleRate;        }

private:
    //==============================================================================
    enum FilterType { LowPass = 0, BandPass, HighPass, Notch };
    enum LfoShape   { Sine = 0, Triangle, SawUp, SawDown, Square, SampleHold };
    enum Slope      { Slope12 = 0, Slope24 };

    // Parameter cache
    std::atomic<float>* pCutoff      = nullptr;
    std::atomic<float>* pResonance   = nullptr;
    std::atomic<float>* pDrive       = nullptr;
    std::atomic<float>* pFilterType  = nullptr;
    std::atomic<float>* pSlope       = nullptr;
    std::atomic<float>* pLfoRate     = nullptr;
    std::atomic<float>* pLfoDepth    = nullptr;
    std::atomic<float>* pLfoShape    = nullptr;
    std::atomic<float>* pEnvAmount   = nullptr;
    std::atomic<float>* pEnvAttack   = nullptr;
    std::atomic<float>* pEnvRelease  = nullptr;
    std::atomic<float>* pMix         = nullptr;
    std::atomic<float>* pOutput      = nullptr;

    // DSP
    juce::dsp::StateVariableTPTFilter<float> filterStage1;
    juce::dsp::StateVariableTPTFilter<float> filterStage2; // for 24 dB cascade

    // Smoothed display & control values
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoothed;

    // LFO state
    double lfoPhase = 0.0;
    float  shValue  = 0.0f;     // sample & hold latched value
    double shPhase  = 1.0;      // wraps once per cycle
    juce::Random shRng;

    // Envelope follower
    float envFollow = 0.0f;
    float envAttackCoeff  = 0.0f;
    float envReleaseCoeff = 0.0f;
    float lastEnvAttackMs  = -1.0f;
    float lastEnvReleaseMs = -1.0f;

    // Sample rate
    double currentSampleRate = 44100.0;

    // Display atoms (read by editor)
    std::atomic<float> displayCutoff    { 1000.0f };
    std::atomic<float> displayResonance { 0.3f };
    std::atomic<float> displayEnvLevel  { 0.0f };
    std::atomic<float> displayLfoValue  { 0.0f };
    std::atomic<int>   displayFilterType{ 0 };
    std::atomic<int>   displaySlope     { 0 };

    // Helpers
    float computeLfoValue (float phase, int shape);
    void  updateEnvelopeCoeffs (float attackMs, float releaseMs);
    static float softClip (float x) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AF1AudioProcessor)
};
