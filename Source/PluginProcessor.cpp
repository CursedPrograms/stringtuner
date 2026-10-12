#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Instruments.h"

namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::StringArray strings { "Auto" };
    for (int s = 1; s <= maxStrings; ++s)
        strings.add (juce::String (s));

    return {
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "instrument", 1 }, "Instrument",
                                                      instrumentNames(), 2),      // violin
        std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "reference", 1 }, "A4", juce::NormalisableRange<float> (415.0f, 466.0f, 1.0f), 440.0f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return "A " + juce::String (juce::roundToInt (v)) + " Hz"; })
                .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("0123456789.").getFloatValue(); })),
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "string", 1 }, "String", strings, 0),
        std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "mute", 1 }, "Mute", false),
    };
}
}

StringTunerProcessor::StringTunerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "stringtuner", createLayout())
{
    mute = params.getRawParameterValue ("mute");
}

bool StringTunerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const auto ok = [] (const juce::AudioChannelSet& s) { return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo(); };
    return ok (in) && ok (out);
}

void StringTunerProcessor::prepareToPlay (double sampleRate, int maxBlockSize)
{
    tuner.prepare (sampleRate);
    mono.assign ((size_t) juce::jmax (1, maxBlockSize), 0.0f);
}

void StringTunerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto ins = getTotalNumInputChannels(), outs = getTotalNumOutputChannels();
    const auto n = buffer.getNumSamples();

    if (ins == 1 || (ins > 1 && (size_t) n > mono.size()))
        tuner.push (buffer.getReadPointer (0), n);
    else if (ins > 1)
    {
        for (int i = 0; i < n; ++i)
            mono[(size_t) i] = 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i));
        tuner.push (mono.data(), n);
    }

    if (mute->load() > 0.5f)
    {
        buffer.clear();
        return;
    }
    for (int ch = ins; ch < outs; ++ch)                  // mono in, stereo out
        if (ins > 0)
            buffer.copyFrom (ch, 0, buffer, 0, 0, n);
        else
            buffer.clear (ch, 0, n);
}

juce::AudioProcessorEditor* StringTunerProcessor::createEditor()
{
    return new StringTunerEditor (*this);
}

void StringTunerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = params.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void StringTunerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (params.state.getType()))
            params.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StringTunerProcessor();
}
