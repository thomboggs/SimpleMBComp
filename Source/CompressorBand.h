/*
  ==============================================================================

    CompressorBand.h
    Created: 28 Oct 2021 11:00:01pm
    Author:  Thomas Boggs

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>


struct CompressorBand
{
    juce::AudioParameterFloat* attack { nullptr };
    juce::AudioParameterFloat* release { nullptr };
    juce::AudioParameterFloat* threshold { nullptr };
    juce::AudioParameterChoice* ratio { nullptr };
    juce::AudioParameterBool* bypassed { nullptr };
    
    void prepare (const juce::dsp::ProcessSpec spec)
    {
        compressor.prepare(spec);
    }
    
    void updateCompressorSettings ()
    {
        compressor.setAttack(attack->get());
        compressor.setRelease(release->get());
        compressor.setThreshold(threshold->get());
        compressor.setRatio(ratio->getCurrentChoiceName().getFloatValue());
    }
    
    void process (juce::AudioBuffer<float>& buffer)
    {
        auto block = juce::dsp::AudioBlock<float> ( buffer );
        auto context = juce::dsp::ProcessContextReplacing<float> ( block );
        
        context.isBypassed = bypassed->get();
        
        compressor.process( context );
    }
    
private:
    juce::dsp::Compressor<float> compressor;
};
