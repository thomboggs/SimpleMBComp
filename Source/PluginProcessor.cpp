/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleMBCompAudioProcessor::SimpleMBCompAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    using namespace Params;
    const auto& params = GetParams();
    
    auto floatHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };
    
    floatHelper(compressor.attack, Names::Attack_Low_Band);
    floatHelper(compressor.release, Names::Release_Low_Band);
    floatHelper(compressor.threshold, Names::Threshold_Low_Band);
    
    auto choiceHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };
    
    choiceHelper(compressor.ratio, Names::Ratio_Low_Band);
    
    auto boolHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };
    
    boolHelper(compressor.bypassed, Names::Bypassed_Low_Band);
    
    floatHelper(lowCrossover, Names::Low_Mid_Crossover_Freq);
    
}

SimpleMBCompAudioProcessor::~SimpleMBCompAudioProcessor()
{
}

//==============================================================================
const juce::String SimpleMBCompAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleMBCompAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleMBCompAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleMBCompAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleMBCompAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleMBCompAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleMBCompAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleMBCompAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleMBCompAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleMBCompAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleMBCompAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    spec.sampleRate = sampleRate;
    
    compressor.prepare(spec);
    
    LP.prepare(spec);
    HP.prepare(spec);
    LP.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    
    for (auto& buffer : filterBuffers )
    {
        buffer.setSize(spec.numChannels, samplesPerBlock);
    }
}

void SimpleMBCompAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleMBCompAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SimpleMBCompAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
//    compressor.updateCompressorSettings();
//    compressor.process( buffer );
    
    for ( auto& filterBuffer : filterBuffers )
    {
        filterBuffer = buffer;
    }
    
    auto cutoff = lowCrossover->get();
    LP.setCutoffFrequency(cutoff);
    HP.setCutoffFrequency(cutoff);
    
    auto fb0Block = juce::dsp::AudioBlock<float>(filterBuffers[0]);
    auto fb1Block = juce::dsp::AudioBlock<float>(filterBuffers[1]);
    
    auto fb0Context = juce::dsp::ProcessContextReplacing<float>(fb0Block);
    auto fb1Context = juce::dsp::ProcessContextReplacing<float>(fb1Block);
    
    LP.process(fb0Context);
    HP.process(fb1Context);
    
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();
    
    buffer.clear();
    
    auto addFilterBand = [nc = numChannels, ns = numSamples](auto& inputBuffer, const auto& sourceBuffer )
    {
        for (auto i = 0; i < nc; ++i)
        {
            inputBuffer.addFrom(i, 0, sourceBuffer, i, 0, ns);
        }
    };
    
    addFilterBand(buffer, filterBuffers[0]);
    addFilterBand(buffer, filterBuffers[1]);
    
}

//==============================================================================
bool SimpleMBCompAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleMBCompAudioProcessor::createEditor()
{
//    return new SimpleMBCompAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleMBCompAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void SimpleMBCompAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}


juce::AudioProcessorValueTreeState::ParameterLayout SimpleMBCompAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    
    using namespace Params;
    const auto& params = GetParams();
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Threshold_Low_Band),
                                                           params.at(Names::Threshold_Low_Band),
                                                           juce::NormalisableRange<float>(-60.f, 12.f, 1.f, 1.f),
                                                           0.f));
    
    auto attackReleaseRange =  juce::NormalisableRange<float>( 5.f, 500.f, 1.f, 1.f );
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Attack_Low_Band),
                                                           params.at(Names::Attack_Low_Band),
                                                           attackReleaseRange,
                                                           50.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Release_Low_Band),
                                                           params.at(Names::Release_Low_Band),
                                                           attackReleaseRange,
                                                           250.f));
    
    juce::StringArray strArray;
    std::vector<double> threshVec = {1.0, 2.0, 3.0, 5.0, 8.0, 13.0, 21.0, 34.0, 55.0, 89.0};
    for (auto thresh : threshVec)
    {
        strArray.add(juce::String(thresh, 1));
    }
    
    layout.add(std::make_unique<juce::AudioParameterChoice>(params.at(Names::Ratio_Low_Band),
                                                            params.at(Names::Ratio_Low_Band),
                                                            strArray,
                                                            2));
    
    layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Names::Bypassed_Low_Band),
                                                          params.at(Names::Bypassed_Low_Band),
                                                          false));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Low_Mid_Crossover_Freq),
                                                           params.at(Names::Low_Mid_Crossover_Freq),
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f),
                                                           500.f));
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleMBCompAudioProcessor();
}
