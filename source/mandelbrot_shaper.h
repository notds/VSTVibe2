#pragma once
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace VSTVibe2 {

// Persistent traversal state — one instance per audio processor, one per view renderer.
// Holds the phase that sweeps ci through different Mandelbrot slices over time.
struct MandelbrotState {
    double phase = 0.0;  // traversal phase [0, 2π), advances per sample when spice > 0.25
};

// Mandelbrot waveshaper: maps sample amplitude into the Mandelbrot escape landscape.
//
// spiceAmount [0, 1]:
//   0.00–0.25  fixed slice at ci=0.1 (deterministic, same result per amplitude value)
//   0.25–1.00  ci sweeps through different Mandelbrot slices each sample; sweep rate
//              and ci range both scale linearly with (spice - 0.25) / 0.75.
//              At spice=1: full 2π cycle over ~44 100 samples (≈1 sec at 44 100 Hz).
inline float applyMandelbrot(float sample, float spiceAmount, MandelbrotState& state) {
    if (spiceAmount < 1e-4f) return sample;

    // How far above the 25% threshold we are [0, 1]
    const float traversal = std::max(0.0f, (spiceAmount - 0.25f) / 0.75f);

    // Advance traversal phase and derive ci from it.
    // ci sweeps through ±0.65 around the base value 0.1, covering the main cardioid
    // boundary and bulbs — the most harmonically rich region of the set.
    const double ci = 0.1 + std::sin(state.phase) * (0.65 * traversal);
    state.phase += traversal * (2.0 * M_PI / 44100.0);
    if (state.phase >= 2.0 * M_PI) state.phase -= 2.0 * M_PI;

    // cr is still driven by sample amplitude: quiet → inside set, loud → boundary/outside
    const double cr      = static_cast<double>(sample) * 1.25 - 0.125;
    const int    maxIter = 32;

    double zr = 0.0, zi = 0.0;
    int escapedAt = maxIter;
    for (int i = 0; i < maxIter; ++i) {
        const double zr2 = zr * zr - zi * zi + cr;
        const double zi2 = 2.0 * zr * zi + ci;
        zr = zr2;
        zi = zi2;
        if (zr * zr + zi * zi > 4.0) {
            escapedAt = i;
            break;
        }
    }

    // t = 1 inside set (no distortion), t = 0 instant escape (max distortion)
    const float t     = static_cast<float>(escapedAt) / static_cast<float>(maxIter);
    const float chaos = 1.0f - t;

    // Frequency-modulated harmonic fold, then tanh soft-clip
    const float fold   = sample + chaos * std::sin(sample * (1.0f + chaos * 5.0f) * static_cast<float>(M_PI));
    const float shaped = std::tanh(fold * (1.0f + chaos));

    return sample * (1.0f - spiceAmount) + shaped * spiceAmount;
}

} // namespace VSTVibe2
