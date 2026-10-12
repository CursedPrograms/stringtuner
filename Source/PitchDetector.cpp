#include "PitchDetector.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr double threshold = 0.15;         // YIN: first dip below this is the period
constexpr double giveUp = 0.35;            // no dip at all below this: not a pitch

void lagsFor (double sr, float minHz, float maxHz, int& minLag, int& maxLag, int& window)
{
    maxLag = (int) std::ceil (sr / minHz) + 1;
    minLag = std::max (2, (int) std::floor (sr / maxHz));
    window = std::max (maxLag * 3 / 2, (int) (0.02 * sr));    // at least 1.5 periods of the lowest note
}
}

int PitchDetector::samplesNeeded (double sr, float minHz)
{
    int minLag, maxLag, w;
    lagsFor (sr, minHz, minHz * 2.0f, minLag, maxLag, w);
    return w + maxLag + 2 + (int) (0.01 * sr);          // + the low-pass warm-up
}

double PitchDetector::exactDifference (int lag) const
{
    double sum = 0.0;
    for (int j = 0; j < window; ++j)
    {
        const double d = (double) segment[(size_t) j] - (double) segment[(size_t) (j + lag)];
        sum += d * d;
    }
    return sum;
}

PitchDetector::Result PitchDetector::detect (const float* x, int length, double sr, float minHz, float maxHz)
{
    int minLag, maxLag;
    lagsFor (sr, minHz, maxHz, minLag, maxLag, window);
    const auto span = window + maxLag + 2;
    if (length < span || minLag + 2 >= maxLag)
        return {};
    const auto warm = std::min (length - span, (int) (0.01 * sr));     // lets the low-pass settle
    x += length - span - warm;

    double mean = 0.0;
    for (int i = 0; i < span + warm; ++i)
        mean += x[i];
    mean /= span + warm;
    segment.resize ((size_t) (span + warm));
    for (int i = 0; i < span + warm; ++i)
        segment[(size_t) i] = (float) (x[i] - mean);

    // Low-pass the copy we analyse (4-pole Butterworth, well above the highest note). Harmonics
    // near Nyquist blur the dip of a period only a dozen samples long and the top notes read an
    // octave low; the pitch itself is in the lower harmonics anyway.
    {
        const auto fc = std::min (0.2 * sr, std::max (2000.0, 1.3 * maxHz));
        const auto w = std::tan (juce::MathConstants<double>::pi * fc / sr);
        for (const auto q : { 0.5411961, 1.3065630 })
        {
            const auto norm = 1.0 / (1.0 + w / q + w * w);
            const auto b0 = w * w * norm, b1 = 2.0 * b0, a1 = 2.0 * (w * w - 1.0) * norm, a2 = (1.0 - w / q + w * w) * norm;
            double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
            for (auto& v : segment)
            {
                const double in = v;
                const auto out = b0 * in + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
                x2 = x1; x1 = in; y2 = y1; y1 = out;
                v = (float) out;
            }
        }
    }
    segment.erase (segment.begin(), segment.begin() + warm);

    // r(lag) = sum over the window of x[j] * x[j + lag], as one FFT cross-correlation
    int order = 1;
    while ((1 << order) < span)
        ++order;
    if (order != fftOrder)
    {
        fft = std::make_unique<juce::dsp::FFT> (order);
        fftOrder = order;
    }
    const auto n = 1 << order;
    a.assign ((size_t) n * 2, 0.0f);
    b.assign ((size_t) n * 2, 0.0f);
    std::copy (segment.begin(), segment.begin() + window, a.begin());
    std::copy (segment.begin(), segment.end(), b.begin());
    fft->performRealOnlyForwardTransform (a.data());
    fft->performRealOnlyForwardTransform (b.data());
    for (size_t k = 0; k < (size_t) n; ++k)
    {
        const auto ar = a[2 * k], ai = a[2 * k + 1], br = b[2 * k], bi = b[2 * k + 1];
        a[2 * k] = ar * br + ai * bi;
        a[2 * k + 1] = ar * bi - ai * br;
    }
    fft->performRealOnlyInverseTransform (a.data());

    energy.assign ((size_t) span + 1, 0.0);
    for (int i = 0; i < span; ++i)
        energy[(size_t) i + 1] = energy[(size_t) i] + (double) segment[(size_t) i] * segment[(size_t) i];
    const auto e0 = energy[(size_t) window];
    if (e0 <= 0.0 || a[0] <= 0.0f)
        return {};
    const auto scale = e0 / a[0];          // r(0) is the window's energy, whatever the FFT scaling

    // difference d(lag) = e(0) + e(lag) - 2 r(lag), then YIN's cumulative mean normalisation
    diff.assign ((size_t) maxLag + 2, 1.0);
    double running = 0.0;
    for (int lag = 1; lag <= maxLag + 1; ++lag)
    {
        const auto eLag = energy[(size_t) (lag + window)] - energy[(size_t) lag];
        const auto d = std::max (0.0, e0 + eLag - 2.0 * a[(size_t) lag] * scale);
        running += d;
        diff[(size_t) lag] = running > 0.0 ? d * lag / running : 1.0;
    }

    const auto depth = [this] (int lag)
    {
        const auto dm = diff[(size_t) lag - 1], d0 = diff[(size_t) lag], dp = diff[(size_t) lag + 1];
        const auto denom = dm - 2.0 * d0 + dp;
        return denom > 0.0 ? std::max (0.0, d0 - 0.125 * (dm - dp) * (dm - dp) / denom) : d0;
    };

    // the first dip below the threshold is the period; dips are judged after interpolation, or a
    // short period that falls between two samples looks shallower than it is
    int best = -1;
    for (int lag = std::max (2, minLag); lag <= maxLag && best < 0; ++lag)
        if (diff[(size_t) lag] <= diff[(size_t) lag - 1] && diff[(size_t) lag] <= diff[(size_t) lag + 1]
            && depth (lag) < threshold)
            best = lag;
    if (best < 0)
    {
        best = std::max (2, minLag);
        for (int lag = best; lag <= maxLag; ++lag)
            if (depth (lag) < depth (best))
                best = lag;
        if (depth (best) > giveUp)
            return {};
    }

    // Big strings often have a weak fundamental, which leaves a dip at half the period. If a
    // multiple of the lag is a much deeper dip, that's the real period.
    for (bool moved = true; moved;)
    {
        moved = false;
        const auto here = depth (best);
        if (here < 0.02)
            break;
        for (int k = 2; k <= 3 && ! moved; ++k)
        {
            const auto lo = std::max (minLag, k * best - k), hi = std::min (maxLag, k * best + k);
            int deepest = -1;
            for (int lag = lo; lag <= hi; ++lag)
                if (deepest < 0 || diff[(size_t) lag] < diff[(size_t) deepest])
                    deepest = lag;
            if (deepest > 0 && depth (deepest) < 0.5 * here)
            {
                best = deepest;
                moved = true;
            }
        }
    }

    // Measure across as many periods as fit: a dip m periods out, divided by m, is m times finer
    // than one period alone (it matters for the top notes, only a dozen samples long).
    auto period = refine (best, maxLag);
    for (int m = 2; m * period <= maxLag - 2; m *= 2)
        period = refine ((int) std::lround (m * period), maxLag) / m;
    return { (float) (sr / period), (float) std::clamp (1.0 - diff[(size_t) best], 0.0, 1.0) };
}

// walk downhill to the exact dip near this lag and fit a parabola through it
double PitchDetector::refine (int lag, int maxLag) const
{
    lag = std::clamp (lag, 2, maxLag);
    auto d0 = exactDifference (lag);
    for (;;)
    {
        if (lag > 2)
            if (const auto left = exactDifference (lag - 1); left < d0) { --lag; d0 = left; continue; }
        if (lag < maxLag)
            if (const auto right = exactDifference (lag + 1); right < d0) { ++lag; d0 = right; continue; }
        break;
    }
    const auto dm = exactDifference (lag - 1), dp = exactDifference (lag + 1);
    const auto denom = dm - 2.0 * d0 + dp;
    return lag + (denom > 0.0 ? std::clamp (0.5 * (dm - dp) / denom, -1.0, 1.0) : 0.0);
}
