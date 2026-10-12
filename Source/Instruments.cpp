#include "Instruments.h"
#include <cmath>

const std::vector<Instrument>& instruments()
{
    // C0 12, B0 23, E1 28, A1 33, E2 40, A2 45, C3 48, G3 55, C4 60, A4 69, E5 76
    static const std::vector<Instrument> list {
        { "Chromatic",                  "Any note",  {} },

        { "Violino piccolo (Bach)",     "Bowed",     { 58, 65, 72, 79 } },
        { "Violin",                     "Bowed",     { 55, 62, 69, 76 } },
        { "Viola",                      "Bowed",     { 48, 55, 62, 69 } },
        { "Cello",                      "Bowed",     { 36, 43, 50, 57 } },
        { "Double bass",                "Bowed",     { 28, 33, 38, 43 } },
        { "Double bass, low C",         "Bowed",     { 24, 33, 38, 43 } },
        { "Double bass, 5-string",      "Bowed",     { 23, 28, 33, 38, 43 } },
        { "Double bass, solo tuning",   "Bowed",     { 30, 35, 40, 45 } },
        { "Octobass",                   "Bowed",     { 12, 19, 24 } },
        { "Viola da gamba (bass)",      "Bowed",     { 38, 43, 48, 52, 57, 62 } },
        { "Erhu",                       "Bowed",     { 62, 69 } },

        { "Guitar",                     "Guitar",    { 40, 45, 50, 55, 59, 64 } },
        { "Guitar, drop D",             "Guitar",    { 38, 45, 50, 55, 59, 64 } },
        { "Guitar, 7-string",           "Guitar",    { 35, 40, 45, 50, 55, 59, 64 } },
        { "Guitar, 8-string",           "Guitar",    { 30, 35, 40, 45, 50, 55, 59, 64 } },
        { "Baritone guitar",            "Guitar",    { 35, 40, 45, 50, 54, 59 } },

        { "Bass, 4-string",             "Bass",      { 28, 33, 38, 43 } },
        { "Bass, 5-string",             "Bass",      { 23, 28, 33, 38, 43 } },
        { "Bass, 6-string",             "Bass",      { 23, 28, 33, 38, 43, 48 } },
        { "Bass, drop D",               "Bass",      { 26, 33, 38, 43 } },

        { "Ukulele",                    "Small",     { 67, 60, 64, 69 } },
        { "Ukulele, low G",             "Small",     { 55, 60, 64, 69 } },
        { "Baritone ukulele",           "Small",     { 50, 55, 59, 64 } },
        { "Mandolin",                   "Small",     { 55, 62, 69, 76 } },
        { "Mandola",                    "Small",     { 48, 55, 62, 69 } },
        { "Cavaquinho",                 "Small",     { 62, 67, 71, 74 } },
        { "Balalaika (prima)",          "Small",     { 64, 64, 69 } },

        { "Banjo, 5-string (open G)",   "Folk",      { 67, 50, 55, 59, 62 } },
        { "Bouzouki (Irish)",           "Folk",      { 43, 50, 57, 62 } },
    };
    return list;
}

juce::StringArray instrumentNames()
{
    juce::StringArray names;
    for (const auto& i : instruments())
        names.add (i.name);
    return names;
}

juce::String noteName (int midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

float hzFor (float midi, float referenceA4)
{
    return referenceA4 * std::pow (2.0f, (midi - 69.0f) / 12.0f);
}

float midiFor (float hz, float referenceA4)
{
    return 69.0f + 12.0f * std::log2 (hz / referenceA4);
}

int nearestString (const Instrument& instrument, float midi)
{
    int best = -1;
    for (int s = 0; s < (int) instrument.strings.size(); ++s)
        if (best < 0 || std::abs (midi - (float) instrument.strings[(size_t) s])
                             < std::abs (midi - (float) instrument.strings[(size_t) best]))
            best = s;
    return best;
}
