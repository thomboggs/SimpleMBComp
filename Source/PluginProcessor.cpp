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
    
    floatHelper(lowBandComp.attack, Names::Attack_Low_Band);
    floatHelper(lowBandComp.release, Names::Release_Low_Band);
    floatHelper(lowBandComp.threshold, Names::Threshold_Low_Band);
    
    floatHelper(midBandComp.attack, Names::Attack_Mid_Band);
    floatHelper(midBandComp.release, Names::Release_Mid_Band);
    floatHelper(midBandComp.threshold, Names::Threshold_Mid_Band);
    
    floatHelper(highBandComp.attack, Names::Attack_High_Band);
    floatHelper(highBandComp.release, Names::Release_High_Band);
    floatHelper(highBandComp.threshold, Names::Threshold_High_Band);
    
    auto choiceHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };
    
    choiceHelper(lowBandComp.ratio, Names::Ratio_Low_Band);
    choiceHelper(midBandComp.ratio, Names::Ratio_Mid_Band);
    choiceHelper(highBandComp.ratio, Names::Ratio_High_Band);
    
    auto boolHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };
    
    boolHelper(lowBandComp.bypassed, Names::Bypassed_Low_Band);
    boolHelper(midBandComp.bypassed, Names::Bypassed_Mid_Band);
    boolHelper(highBandComp.bypassed, Names::Bypassed_High_Band);
    
    boolHelper(lowBandComp.mute, Names::Mute_Low_Band);
    boolHelper(midBandComp.mute, Names::Mute_Mid_Band);
    boolHelper(highBandComp.mute, Names::Mute_High_Band);
    
    boolHelper(lowBandComp.solo, Names:: Solo_Low_Band);
    boolHelper(midBandComp.solo, Names:: Solo_Mid_Band);
    boolHelper(highBandComp.solo, Names::Solo_High_Band);
    
    // Crossover
    floatHelper(lowMidCrossover, Names::Low_Mid_Crossover_Freq);
    floatHelper(midHighCrossover, Names::Mid_High_Crossover_Freq);
    
    // Gain
    floatHelper(inputGainParam, Names::Gain_In);
    floatHelper(outputGainParam, Names::Gain_Out);
    
    LP0.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP0.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    
    AP1.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    
    LP1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    
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
    
    for (auto& compressor : compressors)
    {
        compressor.prepare(spec);
    }
    
    inputGain.prepare(spec);
    outputGain.prepare(spec);
    inputGain.setRampDurationSeconds(0.05);
    outputGain.setRampDurationSeconds(0.05);    
    
    
    LP0.prepare(spec);
    HP0.prepare(spec);
    
    AP1.prepare(spec);
    
    LP1.prepare(spec);
    HP1.prepare(spec);
    
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
    
    for ( auto& compressor : compressors )
    {
        compressor.updateCompressorSettings();
    }

    inputGain.setGainDecibels(inputGainParam->get());
    outputGain.setGainDecibels(outputGainParam->get());
    
    applyGain(buffer, inputGain);
    
//    auto inputGainBlock = juce::dsp::AudioBlock<float>(buffer);
//    auto inputGainContext = juce::dsp::ProcessContextReplacing<float>(inputGainBlock);
//    inputGain.setGainDecibels(inputGainParam->get());
//    inputGain.process(inputGainContext);
    
    for ( auto& filterBuffer : filterBuffers )
    {
        filterBuffer = buffer;
    }
    
    auto lowMidCutoff = lowMidCrossover->get();
    LP0.setCutoffFrequency(lowMidCutoff);
    HP0.setCutoffFrequency(lowMidCutoff);
    
    auto midHighCutoff = midHighCrossover->get();
    AP1.setCutoffFrequency(midHighCutoff);
    LP1.setCutoffFrequency(midHighCutoff);
    HP1.setCutoffFrequency(midHighCutoff);
    
    auto fb0Block = juce::dsp::AudioBlock<float>(filterBuffers[0]);
    auto fb1Block = juce::dsp::AudioBlock<float>(filterBuffers[1]);
    auto fb2Block = juce::dsp::AudioBlock<float>(filterBuffers[2]);
    
    auto fb0Context = juce::dsp::ProcessContextReplacing<float>(fb0Block);
    auto fb1Context = juce::dsp::ProcessContextReplacing<float>(fb1Block);
    auto fb2Context = juce::dsp::ProcessContextReplacing<float>(fb2Block);
    
    LP0.process(fb0Context);
    AP1.process(fb0Context);
    
    HP0.process(fb1Context);
    filterBuffers[2] = filterBuffers[1];
    
    LP1.process(fb1Context);
    HP1.process(fb2Context);
    
    for ( size_t i = 0; i < filterBuffers.size(); ++i)
    {
        compressors[i].process(filterBuffers[i]);
    }
    
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
    
    auto bandsAreSoloed = false;
    for ( auto& comp : compressors )
    {
        if ( comp.solo->get())
        {
            bandsAreSoloed = true;
            break;
        }
    }
    
    if ( bandsAreSoloed )
    {
        for ( size_t i = 0; i < compressors.size(); ++i)
        {
            if ( compressors[i].solo->get() )
            {
                addFilterBand(buffer, filterBuffers[i]);
            }
        }
    }
    else
    {
        for ( size_t i = 0; i < compressors.size(); ++i)
        {
            if ( ! compressors[i].mute->get() )
            {
                addFilterBand(buffer, filterBuffers[i]);
            }
        }
    }
    
    applyGain(buffer, outputGain);
//    auto outputGainBlock = juce::dsp::AudioBlock<float>(buffer);
//    auto outputGainContext = juce::dsp::ProcessContextReplacing<float>(outputGainBlock);
//    outputGain.setGainDecibels(outputGainParam->get());
//    outputGain.process(outputGainContext);
    
//    addFilterBand(buffer, filterBuffers[0]);
//    addFilterBand(buffer, filterBuffers[1]);
//    addFilterBand(buffer, filterBuffers[2]);
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
    
    auto gainRange = juce::NormalisableRange<float>(-24.f,
                                                    24.f,
                                                    0.5f,
                                                    1.f);
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Threshold_Low_Band),
                                                           params.at(Names::Threshold_Low_Band),
                                                           juce::NormalisableRange<float>(-60.f, 12.f, 1.f, 1.f),
                                                           0.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Threshold_Mid_Band),
                                                           params.at(Names::Threshold_Mid_Band),
                                                           juce::NormalisableRange<float>(-60.f, 12.f, 1.f, 1.f),
                                                           0.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Threshold_High_Band),
                                                           params.at(Names::Threshold_High_Band),
                                                           juce::NormalisableRange<float>(-60.f, 12.f, 1.f, 1.f),
                                                           0.f));
    
    auto attackReleaseRange =  juce::NormalisableRange<float>( 5.f, 500.f, 1.f, 1.f );
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Attack_Low_Band),
                                                           params.at(Names::Attack_Low_Band),
                                                           attackReleaseRange,
                                                           50.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Attack_Mid_Band),
                                                           params.at(Names::Attack_Mid_Band),
                                                           attackReleaseRange,
                                                           50.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Attack_High_Band),
                                                           params.at(Names::Attack_High_Band),
                                                           attackReleaseRange,
                                                           50.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Release_Low_Band),
                                                           params.at(Names::Release_Low_Band),
                                                           attackReleaseRange,
                                                           250.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Release_Mid_Band),
                                                           params.at(Names::Release_Mid_Band),
                                                           attackReleaseRange,
                                                           250.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Release_High_Band),
                                                           params.at(Names::Release_High_Band),
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
    layout.add(std::make_unique<juce::AudioParameterChoice>(params.at(Names::Ratio_Mid_Band),
                                                            params.at(Names::Ratio_Mid_Band),
                                                            strArray,
                                                            2));
    layout.add(std::make_unique<juce::AudioParameterChoice>(params.at(Names::Ratio_High_Band),
                                                            params.at(Names::Ratio_High_Band),
                                                            strArray,
                                                            2));
    
    layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Names::Bypassed_Low_Band),
                                                          params.at(Names::Bypassed_Low_Band),
                                                          false));
    layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Names::Bypassed_Mid_Band),
                                                          params.at(Names::Bypassed_Mid_Band),
                                                          false));
    layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Names::Bypassed_High_Band),
                                                          params.at(Names::Bypassed_High_Band),
                                                          false));
    
    layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Names::Mute_Low_Band),
                                                          params.at(Names::Mute_Low_Band),
                                                          false));
    layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Names::Mute_Mid_Band),
                                                          params.at(Names::Mute_Mid_Band),
                                                          false));
    layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Names::Mute_High_Band),
                                                          params.at(Names::Mute_High_Band),
                                                          false));
    
    layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Names::Solo_Low_Band),
                                                          params.at(Names::Solo_Low_Band),
                                                          false));
    layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Names::Solo_Mid_Band),
                                                          params.at(Names::Solo_Mid_Band),
                                                          false));
    layout.add(std::make_unique<juce::AudioParameterBool>(params.at(Names::Solo_High_Band),
                                                          params.at(Names::Solo_High_Band),
                                                          false));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Low_Mid_Crossover_Freq),
                                                           params.at(Names::Low_Mid_Crossover_Freq),
                                                           juce::NormalisableRange<float>(20.f, 999.f, 1.f, 1.f),
                                                           500.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Mid_High_Crossover_Freq),
                                                           params.at(Names::Mid_High_Crossover_Freq),
                                                           juce::NormalisableRange<float>(1000.f, 20000.f, 1.f, 1.f),
                                                           3000.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Gain_In),
                                                           params.at(Names::Gain_In),
                                                           gainRange,
                                                           0.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(params.at(Names::Gain_Out),
                                                           params.at(Names::Gain_Out),
                                                           gainRange,
                                                           0.f));
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleMBCompAudioProcessor();
}
