#include "Tuner.h"
#include <cmath>

void Tuner::prepare (double sr)
{
    sampleRate = sr;
}

void Tuner::push (const float* samples, int numSamples)
{
    // when nobody is looking at the tuner the FIFO fills up and the rest is simply dropped
    const auto scope = fifo.write (juce::jmin (numSamples, fifo.getFreeSpace()));
    if (scope.blockSize1 > 0)
        std::copy (samples, samples + scope.blockSize1, ring.begin() + scope.startIndex1);
    if (scope.blockSize2 > 0)
        std::copy (samples + scope.blockSize1, samples + scope.blockSize1 + scope.blockSize2, ring.begin() + scope.startIndex2);
}

Tuner::Reading Tuner::update (float minHz, float maxHz)
{
    const auto sr = sampleRate.load();
    if (sr <= 0.0)
        return {};

    const auto needed = (size_t) PitchDetector::samplesNeeded (sr, lowestHz);
    if (history.size() != needed)
        history.assign (needed, 0.0f);

    // slide the newest samples in at the end
    const auto scope = fifo.read (fifo.getNumReady());
    const auto add = [this] (const float* src, int n)
    {
        if (n <= 0)
            return;
        if ((size_t) n >= history.size())
        {
            std::copy (src + n - (int) history.size(), src + n, history.begin());
            return;
        }
        std::move (history.begin() + n, history.end(), history.begin());
        std::copy (src, src + n, history.end() - n);
    };
    add (ring.data() + scope.startIndex1, scope.blockSize1);
    add (ring.data() + scope.startIndex2, scope.blockSize2);

    minHz = juce::jmax (lowestHz, minHz);
    const auto span = juce::jmin ((int) history.size(), PitchDetector::samplesNeeded (sr, minHz));
    const auto* newest = history.data() + history.size() - (size_t) span;

    auto energy = 0.0;
    for (int i = 0; i < span; ++i)
        energy += (double) newest[i] * newest[i];
    if (std::sqrt (energy / span) < 0.001)                // about -60 dBFS: not playing
        return {};

    const auto r = detector.detect (newest, span, sr, minHz, maxHz);
    return { r.hz > 0.0f, r.hz };
}
