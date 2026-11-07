#pragma once
#include <array>
#include <cstddef>
#include <vector>

namespace heavy {
constexpr float pi = 3.14159265358979323846f;
constexpr int laneCount = 4;
constexpr int cabinetCount = 6;
constexpr int latencySamples = 16;
enum class DelayMode { echo, drum, studio };
struct Pedals {
    bool driveOn = false, fuzzOn = false;
    float drive = .35f, driveTone = .55f, driveLevel = .6f;
    int fuzzType = 0;
    float sustain = .6f, fuzzTone = .45f, fuzzLevel = .5f;
};
struct Lane {
    bool enabled = true;
    bool solo = false;
    float gain = .5f, bass = .5f, mid = .5f, treble = .5f, presence = .4f;
    float level = .5f, pan = 0.f;
    int cabinet = 0;
};
struct Parameters {
    float inputDb = 0.f, outputDb = -6.f;
    Pedals pedals;
    std::array<Lane, laneCount> lanes {};
    DelayMode delayMode = DelayMode::echo;
    float delayMs = 380.f, feedback = .35f, delayBlend = 0.f;
    float reverbSize = .65f, reverbDecay = 2.5f, reverbDamping = .55f, reverbBlend = 0.f;
    Parameters();
};
struct Stereo { float left = 0.f, right = 0.f; };
struct OnePole {
    float state = 0.f, coefficient = .1f;
    void prepare(double rate, float hz);
    float low(float x) noexcept { state += coefficient * (x - state); return state; }
    float high(float x) noexcept { return x - low(x); }
};
struct Fir {
    static constexpr int taps = 65;
    std::array<float, taps> kernel {}, history {};
    int position = 0;
    void prepare();
    float tick(float x) noexcept;
};
class Cabinet {
public:
    static constexpr int taps = 256;
    void prepare(double rate);
    void select(int index) noexcept;
    float tick(float x) noexcept;
private:
    std::array<std::array<float, taps>, cabinetCount> kernels {};
    std::array<float, taps> current {}, history {};
    int position = 0, selected = 0, remaining = 0;
};
struct Amp {
    OnePole low, high, coupling, presence, dc;
    float envelope = 0.f, sagCoefficient = .001f;
    void prepare(double rate);
    float tick(float x, const Lane&, int model) noexcept;
};
class Delay {
public:
    void prepare(double rate);
    Stereo tick(Stereo input, const Parameters&) noexcept;
private:
    float read(const std::vector<float>& buffer, float samples) const noexcept;
    std::array<std::vector<float>, 2> lines;
    std::array<OnePole, 2> damping;
    std::size_t position = 0;
    double rate = 48000., phase = 0.;
    float timeMs = 380.f;
};
class Reverb {
public:
    void prepare(double rate);
    Stereo tick(Stereo input, const Parameters&) noexcept;
private:
    std::array<std::vector<float>, 8> lines;
    std::array<std::size_t, 8> positions {};
    std::array<float, 8> damped {};
    double rate = 48000.;
};
class Engine {
public:
    void prepare(double sampleRate);
    void setParameters(const Parameters&) noexcept;
    Stereo tick(float mono) noexcept;
    // All buffers are allocated in prepare, never in tick or setParameters.
    void process(const float* mono, float* left, float* right, std::size_t frames) noexcept;
    void clearMeters() noexcept { lanePeaks.fill(0.f); }
    const std::array<float, laneCount>& meterPeaks() const noexcept { return lanePeaks; }
private:
    void smooth() noexcept;
    Parameters target, current;
    float smoothing = .001f;
    float driveMix = 0.f, fuzzMix = 0.f;
    Fir interpolator;
    std::array<Fir, laneCount> decimators;
    std::array<Amp, laneCount> amps;
    std::array<Cabinet, laneCount> cabs;
    OnePole driveTone, fuzzTone, pedalHighPass, pedalDc;
    Delay delay;
    Reverb reverb;
    std::array<float, laneCount> lanePeaks {};
};
}
