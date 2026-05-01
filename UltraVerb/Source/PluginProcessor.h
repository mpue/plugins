/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DSP/ReverbController.h"
#include "Programs.h"
#include "AudioAnalyzer.h"

//==============================================================================
/**
*/
class UltraVerbAudioProcessor  : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener,
    public juce::AudioProcessorParameter::Listener
{
public:
    //==============================================================================
    UltraVerbAudioProcessor();
    ~UltraVerbAudioProcessor() override;

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
    
    std::unique_ptr<Cloudseed::ReverbController> reverb = nullptr;

    juce::AudioProcessorValueTreeState* getValueTreeState();    
    juce::AudioProcessorValueTreeState* parameters = nullptr;

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

    const float* getFFTData() const { return fftData; }

    bool post = false;

    AudioAnalyzer& getAnalyzer() { return analyzer; }


private:
    
    void performFFT()
    {
        if (tempBuffer.getNumSamples() >= fftSize)
        {
            juce::FloatVectorOperations::copy(fftData, tempBuffer.getReadPointer(0), fftSize);
            window.multiplyWithWindowingTable(fftData, fftSize);
            fft.performFrequencyOnlyForwardTransform(fftData);

            //juce::FloatVectorOperations::copy(fftDataR, tempBuffer.getReadPointer(1), fftSize);
            //window.multiplyWithWindowingTable(fftDataR, fftSize);
            //fft.performFrequencyOnlyForwardTransform(fftDataR);

        }
    }

    enum
    {
        fftOrder = 10,
        fftSize = 1 << fftOrder,
    };

    AudioAnalyzer analyzer;

    juce::AudioBuffer<float> buffer;
    juce::AudioBuffer<float> tempBuffer;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float fftData[2 * fftSize] = { 0 };

    float fifo[fftSize];
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;
        
    float ProgramDarkPlate[Cloudseed::Parameter::COUNT];

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UltraVerbAudioProcessor)
};
