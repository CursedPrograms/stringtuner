// Offline check: pitch accuracy from C0 (octobass) to C8 (top of the piano) at several sample
// rates, weak fundamentals, inharmonic strings, silence, speed, and snapshots of the window.
//   stringtunerCheck <out dir>
#include "../Source/PluginProcessor.h"
#include "../Source/Instruments.h"
#include <chrono>
#include <iostream>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << '\n';
    failures += ok ? 0 : 1;
}

// a plucked / bowed string: harmonics falling off as 1/k, the higher ones dying faster
std::vector<float> string (double sr, double hz, double seconds, double fundamental, double inharmonicity, int seed)
{
    const auto n = (int) (seconds * sr);
    std::vector<double> sum ((size_t) n, 0.0);
    juce::Random rnd (seed);
    for (int k = 1; k <= 40; ++k)
    {
        const auto f = hz * k * std::sqrt (1.0 + inharmonicity * k * k);
        if (f > 0.45 * sr)
            break;
        const auto amp = k == 1 ? fundamental : 1.0 / k;
        const auto phase = rnd.nextDouble() * juce::MathConstants<double>::twoPi;
        const auto w = juce::MathConstants<double>::twoPi * f / sr;
        const auto decay = 0.5 + 0.3 * k;
        for (int i = 0; i < n; ++i)
            sum[(size_t) i] += amp * std::exp (-decay * i / sr) * std::sin (w * i + phase);
    }
    double peak = 1e-12;
    for (auto v : sum)
        peak = std::max (peak, std::abs (v));
    std::vector<float> out ((size_t) n);
    for (int i = 0; i < n; ++i)
        out[(size_t) i] = (float) (0.4 * sum[(size_t) i] / peak) + 1e-4f * (rnd.nextFloat() - 0.5f);
    return out;
}

// play it in blocks like a host would, asking for a reading 30 times a second like the window does
Tuner::Reading listen (const std::vector<float>& x, double sr, float minHz, float maxHz)
{
    Tuner t;
    t.prepare (sr);
    Tuner::Reading last;
    const auto every = (int) (sr / 30.0);
    for (int pos = 0, sinceUpdate = 0; pos < (int) x.size(); pos += 256, sinceUpdate += 256)
    {
        t.push (x.data() + pos, std::min (256, (int) x.size() - pos));
        if (sinceUpdate >= every)
        {
            last = t.update (minHz, maxHz);
            sinceUpdate = 0;
        }
    }
    return t.update (minHz, maxHz);
}

float centsOff (float got, double want)
{
    return got > 0.0f ? (float) (1200.0 * std::log2 (got / want)) : 9999.0f;
}

void snapshot (const juce::File& file, int instrument, double hz, bool lockString = false)
{
    StringTunerProcessor p;
    auto* param = p.params.getParameter ("instrument");
    param->setValueNotifyingHost (param->convertTo0to1 ((float) instrument));
    if (lockString)
    {
        auto* s = p.params.getParameter ("string");
        s->setValueNotifyingHost (s->convertTo0to1 (1.0f));
    }
    p.tuner.prepare (48000.0);
    const auto x = string (48000.0, hz, 1.0, 1.0, 0.0, 3);
    p.tuner.push (x.data(), (int) x.size());
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat().writeImageToStream (image, stream);
}

int indexOf (const char* name)
{
    return instrumentNames().indexOf (name);
}
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    std::cout << "chromatic, C0 to C8, clean strings (within 1 cent):\n";
    for (const auto sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        float worst = 0.0f;
        int worstNote = 0;
        for (int midi = 12; midi <= 108; ++midi)
        {
            const auto want = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            const auto r = listen (string (sr, want, 0.5, 1.0, 0.0, midi), sr, 15.0f, 4600.0f);
            const auto c = std::abs (centsOff (r.hz, want));
            if (c > worst)
            {
                worst = c;
                worstNote = midi;
            }
        }
        check (worst < 1.0f, juce::String (sr / 1000.0, 1) + " kHz: worst " + juce::String (worst, 3) + " cents at " + noteName (worstNote));
    }

    std::cout << "weak fundamental (15 %), C0 to C4, must not jump an octave (within 2 cents):\n";
    for (const auto sr : { 48000.0, 192000.0 })
    {
        float worst = 0.0f;
        int worstNote = 0;
        for (int midi = 12; midi <= 60; midi += 3)
        {
            const auto want = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            const auto r = listen (string (sr, want, 0.5, 0.15, 0.0, midi + 100), sr, 15.0f, 4600.0f);
            const auto c = std::abs (centsOff (r.hz, want));
            if (c > worst)
            {
                worst = c;
                worstNote = midi;
            }
        }
        check (worst < 2.0f, juce::String (sr / 1000.0, 1) + " kHz: worst " + juce::String (worst, 3) + " cents at " + noteName (worstNote));
    }

    std::cout << "stiff (inharmonic) strings have no single pitch; cents above the fundamental (info):\n";
    for (const auto& [midi, b] : { std::pair<int, double> { 23, 0.0004 }, { 28, 0.0003 }, { 40, 0.0002 }, { 64, 0.0001 } })
    {
        const auto want = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
        const auto r = listen (string (48000.0, want, 0.5, 1.0, b, midi), 48000.0, 15.0f, 4600.0f);
        const auto c = centsOff (r.hz, want);
        std::cout << "  info " << noteName (midi) << " B=" << juce::String (b, 4) << ": " << juce::String (c, 2) << " cents\n";
    }

    std::cout << "instrument mode:\n";
    {
        const auto& bass = instruments()[(size_t) indexOf ("Double bass")];
        const auto r = listen (string (48000.0, 41.0, 0.5, 0.3, 0.0, 9), 48000.0, hzFor (28 - 7.0f, 440.0f), hzFor (43 + 7.0f, 440.0f));
        const auto midi = midiFor (r.hz, 440.0f);
        const auto s = nearestString (bass, midi);
        check (s == 0 && std::abs ((midi - 28.0f) * 100.0f + 8.57f) < 0.2f,
               "double bass, 41.0 Hz: string " + juce::String (s + 1) + " (E1), " + juce::String ((midi - 28.0f) * 100.0f, 2) + " cents");
        const auto& piccolo = instruments()[(size_t) indexOf ("Violino piccolo (Bach)")];
        const auto r2 = listen (string (48000.0, hzFor (79.0f, 440.0f) * 1.002, 0.5, 1.0, 0.0, 10), 48000.0, hzFor (58 - 7.0f, 440.0f), hzFor (79 + 7.0f, 440.0f));
        check (nearestString (piccolo, midiFor (r2.hz, 440.0f)) == 3, "violino piccolo, G5 + 3.5 c: string 4 (" + juce::String (centsOff (r2.hz, hzFor (79.0f, 440.0f)), 2) + " c)");
        const auto r3 = listen (string (48000.0, hzFor (69.0f, 415.0f), 0.5, 1.0, 0.0, 11), 48000.0, 15.0f, 4600.0f);
        check (std::abs (midiFor (r3.hz, 415.0f) - 69.0f) < 0.01f, "baroque A = 415 Hz reads as A4 in tune");
    }

    std::cout << "silence:\n";
    {
        std::vector<float> noise (48000);
        juce::Random rnd (1);
        for (auto& v : noise)
            v = 3e-4f * (rnd.nextFloat() - 0.5f);                 // about -76 dBFS
        check (! listen (noise, 48000.0, 15.0f, 4600.0f).valid, "noise floor gives no note");
    }

    std::cout << "speed (one reading, chromatic, the slowest case):\n";
    for (const auto sr : { 48000.0, 192000.0 })
    {
        Tuner t;
        t.prepare (sr);
        const auto x = string (sr, 41.2, 0.5, 1.0, 0.0, 2);
        t.push (x.data(), (int) x.size());
        t.update (15.0f, 4600.0f);
        const auto start = std::chrono::steady_clock::now();
        constexpr int runs = 20;
        for (int i = 0; i < runs; ++i)
            t.update (15.0f, 4600.0f);
        const auto ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count() / runs;
        check (ms < 8.0, juce::String (sr / 1000.0, 1) + " kHz: " + juce::String (ms, 2) + " ms per reading (30 a second)");
    }

    snapshot (outDir.getChildFile ("gui_bass.png"), indexOf ("Double bass"), 40.9);
    snapshot (outDir.getChildFile ("gui_violin.png"), indexOf ("Violin"), 440.4);
    snapshot (outDir.getChildFile ("gui_chromatic.png"), 0, 3136.0 * 0.995);
    snapshot (outDir.getChildFile ("gui_octobass_locked.png"), indexOf ("Octobass"), 16.5, true);

    std::cout << (failures == 0 ? "all passed\n" : juce::String (failures) + " FAILED\n");
    return failures == 0 ? 0 : 1;
}
