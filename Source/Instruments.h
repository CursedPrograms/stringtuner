#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// Open strings as MIDI notes (60 = middle C, 69 = A4), in the order they're usually named.
struct Instrument
{
    const char* name;
    const char* group;
    std::vector<int> strings;          // empty = chromatic
};

constexpr int maxStrings = 8;

const std::vector<Instrument>& instruments();
juce::StringArray instrumentNames();

juce::String noteName (int midi);                  // "E1", "C#4"
float hzFor (float midi, float referenceA4);
float midiFor (float hz, float referenceA4);
int nearestString (const Instrument&, float midi);  // -1 for chromatic
