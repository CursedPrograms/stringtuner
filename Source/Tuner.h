#pragma once
#include "PitchDetector.h"
#include <atomic>
#include <vector>

// The audio thread only copies samples into a FIFO; the pitch detection runs on the GUI thread
// when the window asks for it.
class Tuner
{
public:
    struct Reading
    {
        bool valid = false;
        float hz = 0.0f;
    };

    static constexpr float lowestHz = 12.0f;             // below the octobass's C0 (16.35 Hz)

    void prepare (double sampleRate);
    void push (const float* samples, int numSamples);    // audio thread
    Reading update (float minHz, float maxHz);            // GUI thread

private:
    juce::AbstractFifo fifo { 1 << 17 };
    std::vector<float> ring = std::vector<float> (1 << 17);
    std::atomic<double> sampleRate { 0.0 };
    std::vector<float> history;
    PitchDetector detector;
};
