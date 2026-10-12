#pragma once
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

// Pitch of the newest stretch of a signal, for anything from an octobass (C0, 16 Hz) to the top of
// a piano (C8, 4186 Hz). YIN (de Cheveigne & Kawahara, 2002), with the difference function done by
// FFT so the long windows the lowest strings need stay cheap. The dip it finds is then re-measured
// exactly, in double, so the cents are right even at 192 kHz.
class PitchDetector
{
public:
    struct Result
    {
        float hz = 0.0f;                // 0 = no pitch
        float clarity = 0.0f;           // 0..1, how periodic it is
    };

    // how many of the newest samples detect() looks at for this lowest note
    static int samplesNeeded (double sampleRate, float minHz);

    Result detect (const float* samples, int length, double sampleRate, float minHz, float maxHz);

private:
    double exactDifference (int lag) const;
    double refine (int lag, int maxLag) const;

    std::unique_ptr<juce::dsp::FFT> fft;
    int fftOrder = -1;
    int window = 0;
    std::vector<float> segment, a, b;
    std::vector<double> energy, diff;
};
