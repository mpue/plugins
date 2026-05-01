/*
  ==============================================================================

    AF-1 — Luxurious AutoFilter
    Processor implementation

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AF1AudioProcessor::AF1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#else
     :
#endif
       apvts (*this, nullptr, "AF1", createParameterLayout())
{
    pCutoff      = apvts.getRawParameterValue ("cutoff");
    pResonance   = apvts.getRawParameterValue ("resonance");
    pDrive       = apvts.getRawParameterValue ("drive");
    pFilterType  = apvts.getRawParameterValue ("filterType");
    pSlope       = apvts.getRawParameterValue ("slope");
    pLfoRate     = apvts.getRawParameterValue ("lfoRate");
    pLfoDepth    = apvts.getRawParameterValue ("lfoDepth");
    pLfoShape    = apvts.getRawParameterValue ("lfoShape");
    pEnvAmount   = apvts.getRawParameterValue ("envAmount");
    pEnvAttack   = apvts.getRawParameterValue ("envAttack");
    pEnvRelease  = apvts.getRawParameterValue ("envRelease");
    pMix         = apvts.getRawParameterValue ("mix");
    pOutput      = apvts.getRawParameterValue ("output");
}

AF1AudioProcessor::~AF1AudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AF1AudioProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;

    auto freqRange   = juce::NormalisableRange<float>(20.0f, 20000.0f, 0.01f, 0.25f);
    auto rateRange   = juce::NormalisableRange<float>(0.02f, 20.0f,    0.001f, 0.35f);
    auto attackRange = juce::NormalisableRange<float>(0.5f,  500.0f,   0.001f, 0.4f);
    auto releaseRange= juce::NormalisableRange<float>(5.0f,  2000.0f,  0.001f, 0.4f);

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<P>(juce::ParameterID("cutoff",     1), "Cutoff",
                                          freqRange, 1200.0f,
                                          juce::AudioParameterFloatAttributes()
                                              .withLabel ("Hz")
                                              .withStringFromValueFunction ([](float v, int) {
                                                  if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                                                  return juce::String (v, 1) + " Hz";
                                              })));

    params.push_back (std::make_unique<P>(juce::ParameterID("resonance",  1), "Resonance",
                                          juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.30f));

    params.push_back (std::make_unique<P>(juce::ParameterID("drive",      1), "Drive",
                                          juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f), 0.0f,
                                          juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<C>(juce::ParameterID("filterType", 1), "Filter Type",
                                          juce::StringArray{ "Low Pass", "Band Pass", "High Pass", "Notch" }, 0));

    params.push_back (std::make_unique<C>(juce::ParameterID("slope",      1), "Slope",
                                          juce::StringArray{ "12 dB/oct", "24 dB/oct" }, 1));

    params.push_back (std::make_unique<P>(juce::ParameterID("lfoRate",    1), "LFO Rate",
                                          rateRange, 1.0f,
                                          juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<P>(juce::ParameterID("lfoDepth",   1), "LFO Depth",
                                          juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));

    params.push_back (std::make_unique<C>(juce::ParameterID("lfoShape",   1), "LFO Shape",
                                          juce::StringArray{ "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" }, 0));

    params.push_back (std::make_unique<P>(juce::ParameterID("envAmount",  1), "Env Amount",
                                          juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0001f), 0.0f));

    params.push_back (std::make_unique<P>(juce::ParameterID("envAttack",  1), "Env Attack",
                                          attackRange, 8.0f,
                                          juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<P>(juce::ParameterID("envRelease", 1), "Env Release",
                                          releaseRange, 200.0f,
                                          juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<P>(juce::ParameterID("mix",        1), "Mix",
                                          juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 1.0f));

    params.push_back (std::make_unique<P>(juce::ParameterID("output",     1), "Output",
                                          juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f,
                                          juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String AF1AudioProcessor::getName() const                  { return JucePlugin_Name; }
bool   AF1AudioProcessor::acceptsMidi() const                          { return false; }
bool   AF1AudioProcessor::producesMidi() const                         { return false; }
bool   AF1AudioProcessor::isMidiEffect() const                         { return false; }
double AF1AudioProcessor::getTailLengthSeconds() const                 { return 0.0; }
int    AF1AudioProcessor::getNumPrograms()                             { return 1; }
int    AF1AudioProcessor::getCurrentProgram()                          { return 0; }
void   AF1AudioProcessor::setCurrentProgram (int)                      {}
const  juce::String AF1AudioProcessor::getProgramName (int)            { return {}; }
void   AF1AudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void AF1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    filterStage1.prepare (spec);
    filterStage2.prepare (spec);
    filterStage1.reset();
    filterStage2.reset();

    mixSmoothed   .reset (sampleRate, 0.02);
    outputSmoothed.reset (sampleRate, 0.02);
    driveSmoothed .reset (sampleRate, 0.02);

    mixSmoothed   .setCurrentAndTargetValue (pMix    != nullptr ? pMix   ->load() : 1.0f);
    outputSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput != nullptr ? pOutput->load() : 0.0f));
    driveSmoothed .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDrive  != nullptr ? pDrive ->load() : 0.0f));

    envFollow = 0.0f;
    lfoPhase  = 0.0;
    shPhase   = 1.0;
    shValue   = 0.0f;

    lastEnvAttackMs  = -1.0f;
    lastEnvReleaseMs = -1.0f;
    updateEnvelopeCoeffs (pEnvAttack ? pEnvAttack->load() : 8.0f,
                          pEnvRelease ? pEnvRelease->load() : 200.0f);
}

void AF1AudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AF1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif
    return true;
   #endif
}
#endif

//==============================================================================
float AF1AudioProcessor::softClip (float x) noexcept
{
    // Smooth, asymmetric-friendly tanh shaping → musical drive without harsh fold-back.
    return std::tanh (x);
}

float AF1AudioProcessor::computeLfoValue (float phase, int shape)
{
    // phase: 0..1
    switch (shape)
    {
        case Sine:     return std::sin (phase * juce::MathConstants<float>::twoPi);
        case Triangle: return 4.0f * std::abs (phase - 0.5f) - 1.0f;
        case SawUp:    return phase * 2.0f - 1.0f;
        case SawDown:  return 1.0f - phase * 2.0f;
        case Square:   return phase < 0.5f ? 1.0f : -1.0f;
        case SampleHold: return shValue;
        default:       return 0.0f;
    }
}

void AF1AudioProcessor::updateEnvelopeCoeffs (float attackMs, float releaseMs)
{
    if (! juce::approximatelyEqual (attackMs, lastEnvAttackMs))
    {
        envAttackCoeff = std::exp (-1.0f / ((attackMs / 1000.0f) * (float) currentSampleRate));
        lastEnvAttackMs = attackMs;
    }
    if (! juce::approximatelyEqual (releaseMs, lastEnvReleaseMs))
    {
        envReleaseCoeff = std::exp (-1.0f / ((releaseMs / 1000.0f) * (float) currentSampleRate));
        lastEnvReleaseMs = releaseMs;
    }
}

//==============================================================================
void AF1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    // Read parameters
    const float baseCutoff   = pCutoff   ->load();
    const float resonance    = pResonance->load();
    const float driveDb      = pDrive    ->load();
    const int   filterType   = (int) pFilterType->load();
    const int   slope        = (int) pSlope     ->load();
    const float lfoRateHz    = pLfoRate  ->load();
    const float lfoDepth     = pLfoDepth ->load();
    const int   lfoShape     = (int) pLfoShape  ->load();
    const float envAmount    = pEnvAmount->load();
    const float envAttackMs  = pEnvAttack ->load();
    const float envReleaseMs = pEnvRelease->load();
    const float mixVal       = pMix      ->load();
    const float outputDb     = pOutput   ->load();

    updateEnvelopeCoeffs (envAttackMs, envReleaseMs);

    mixSmoothed   .setTargetValue (mixVal);
    outputSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputDb));
    driveSmoothed .setTargetValue (juce::Decibels::decibelsToGain (driveDb));

    // Configure filter type / Q. Q range: 0.7 (gentle) up to ~14 (lush, near self-osc)
    const float q = juce::jmap (resonance, 0.0f, 1.0f, 0.7071f, 14.0f);

    auto setType = [filterType](juce::dsp::StateVariableTPTFilter<float>& f)
    {
        switch (filterType)
        {
            case LowPass:  f.setType (juce::dsp::StateVariableTPTFilterType::lowpass);  break;
            case BandPass: f.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break;
            case HighPass: f.setType (juce::dsp::StateVariableTPTFilterType::highpass); break;
            case Notch:    f.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break; // notch via subtraction below
            default:       f.setType (juce::dsp::StateVariableTPTFilterType::lowpass);  break;
        }
    };
    setType (filterStage1);
    setType (filterStage2);
    filterStage1.setResonance (q);
    filterStage2.setResonance (q);

    // For sample-rate dependent LFO advancement
    const double phaseInc = (double) lfoRateHz / currentSampleRate;
    const double shPhaseInc = phaseInc; // S&H latches once per cycle

    const int numProcChans = juce::jmin (numIn, numOut);

    // Per-sample DSP, processed channel-interleaved by reading both channels for env
    auto* L = numProcChans > 0 ? buffer.getWritePointer (0) : nullptr;
    auto* R = numProcChans > 1 ? buffer.getWritePointer (1) : L;

    float lastEnv = envFollow;
    float lastLfo = 0.0f;
    float lastModCutoff = baseCutoff;

    for (int i = 0; i < n; ++i)
    {
        // ─ Envelope follower (peak across channels)
        const float dryL = L != nullptr ? L[i] : 0.0f;
        const float dryR = R != nullptr ? R[i] : dryL;
        const float in   = juce::jmax (std::abs (dryL), std::abs (dryR));

        if (in > envFollow) envFollow = envAttackCoeff  * (envFollow - in) + in;
        else                envFollow = envReleaseCoeff * (envFollow - in) + in;
        // soft compress to 0..1 with musical curve
        const float envNorm = juce::jlimit (0.0f, 1.0f, std::pow (envFollow, 0.5f));

        // ─ LFO
        // Sample & hold update
        if (lfoShape == SampleHold)
        {
            shPhase += shPhaseInc;
            if (shPhase >= 1.0)
            {
                shPhase = std::fmod (shPhase, 1.0);
                shValue = shRng.nextFloat() * 2.0f - 1.0f;
            }
        }

        const float lfo = computeLfoValue ((float) lfoPhase, lfoShape) * lfoDepth;
        lfoPhase += phaseInc;
        if (lfoPhase >= 1.0) lfoPhase -= 1.0;

        // ─ Modulated cutoff (in semitones for musical pitch-like modulation)
        // ±36 semitones from LFO at full depth, ±48 from envelope at full amount
        const float lfoSemi = lfo * 36.0f;
        const float envSemi = envAmount * envNorm * 48.0f;
        float modCutoff = baseCutoff * std::pow (2.0f, (lfoSemi + envSemi) / 12.0f);
        modCutoff = juce::jlimit (20.0f, (float) (currentSampleRate * 0.45), modCutoff);

        filterStage1.setCutoffFrequency (modCutoff);
        if (slope == Slope24)
            filterStage2.setCutoffFrequency (modCutoff);

        // ─ Drive
        const float driveGain = driveSmoothed.getNextValue();
        const float mix       = mixSmoothed   .getNextValue();
        const float outGain   = outputSmoothed.getNextValue();

        // Process each channel
        for (int ch = 0; ch < numProcChans; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            const float dry = data[i];
            float x = dry * driveGain;
            x = softClip (x);

            float y = filterStage1.processSample (ch, x);
            if (slope == Slope24)
                y = filterStage2.processSample (ch, y);

            // For Notch: y_input - y_bp gives a notch response (we set BP above)
            if (filterType == Notch)
                y = x - y;

            // Compensate for drive a bit so output level stays sane
            const float comp = 1.0f / std::sqrt (juce::jmax (driveGain, 1.0f));
            y *= comp;

            data[i] = (dry * (1.0f - mix) + y * mix) * outGain;
        }

        lastEnv = envNorm;
        lastLfo = lfo;
        lastModCutoff = modCutoff;
    }

    // Push display values (read by visualiser)
    displayCutoff.store    (lastModCutoff);
    displayResonance.store (resonance);
    displayEnvLevel.store  (lastEnv);
    displayLfoValue.store  (lastLfo);
    displayFilterType.store(filterType);
    displaySlope.store     (slope);
}

//==============================================================================
bool   AF1AudioProcessor::hasEditor() const                            { return true; }
juce::AudioProcessorEditor* AF1AudioProcessor::createEditor()          { return new AF1AudioProcessorEditor (*this); }

//==============================================================================
void AF1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); true)
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        if (xml != nullptr)
            copyXmlToBinary (*xml, destData);
    }
}

void AF1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AF1AudioProcessor();
}
