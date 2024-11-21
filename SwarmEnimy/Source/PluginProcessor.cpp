/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <immintrin.h>

//==============================================================================
SwarmEnimyAudioProcessor::SwarmEnimyAudioProcessor()
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
}

SwarmEnimyAudioProcessor::~SwarmEnimyAudioProcessor()
{
}

//==============================================================================
const juce::String SwarmEnimyAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SwarmEnimyAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SwarmEnimyAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SwarmEnimyAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SwarmEnimyAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SwarmEnimyAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SwarmEnimyAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SwarmEnimyAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SwarmEnimyAudioProcessor::getProgramName (int index)
{
    return {};
}

void SwarmEnimyAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SwarmEnimyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sample_rate = sampleRate;
}

void SwarmEnimyAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SwarmEnimyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void SwarmEnimyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    __m256 freqs = _mm256_set_ps(800.0f,700.0f,600.0f,500.0f,400.0f,300.0f,200.0f,100.0f);
    float constexpr twoPi = juce::MathConstants<float>::twoPi;
    __m256 phasors_local = _mm256_load_ps(phasors.data()); 
    float sin_out[64];

    for (int samples = 0; samples < buffer.getNumSamples(); samples += 8)
    {
        for (int i = 0; i < 8; ++i) {
            phasors_local = _mm256_fmadd_ps(freqs, _mm256_set1_ps(twoPi / sample_rate), phasors_local);
            __m256 wrap_mask = _mm256_cmp_ps(phasors_local, _mm256_set1_ps(twoPi), _CMP_GE_OQ);
            phasors_local = _mm256_sub_ps(phasors_local, _mm256_and_ps(wrap_mask, _mm256_set1_ps(twoPi)));
            _mm256_store_ps(sin_out + 8 * i, _mm256_sin_ps(phasors_local));
        }

        __m256 acc = _mm256_setzero_ps();
        for (int i = 0; i < 8; ++i) {
            __m256 osc_vals = _mm256_i32gather_ps(sin_out + i, _mm256_set_epi32(56, 48, 40, 32, 24, 16, 8, 0), 4);
            acc = _mm256_add_ps(acc, osc_vals);
        }
        acc = _mm256_mul_ps(acc, _mm256_set1_ps(1.0f / 8.0f));

        for (int i = 0; i < totalNumOutputChannels; ++i) {
            auto write_ptr = buffer.getWritePointer(i);
            _mm256_store_ps(write_ptr + samples, acc);
        }
    }
    _mm256_store_ps(phasors.data(), phasors_local);
}

//==============================================================================
bool SwarmEnimyAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SwarmEnimyAudioProcessor::createEditor()
{
    return new SwarmEnimyAudioProcessorEditor (*this);
}

//==============================================================================
void SwarmEnimyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SwarmEnimyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SwarmEnimyAudioProcessor();
}
