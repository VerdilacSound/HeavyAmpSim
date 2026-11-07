#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace heavy {
namespace {
float db(float x) noexcept { return std::pow(10.f, x / 20.f); }
float bounded(float x, float lo, float hi) noexcept {
    return std::isfinite(x) ? std::clamp(x, lo, hi) : lo;
}
float saturate(float x, float bias) noexcept { return std::tanh(x + bias) - std::tanh(bias); }
}
Parameters::Parameters() {
    lanes[0].level = .7f;
    lanes[1].level = lanes[2].level = lanes[3].level = 0.f;
    for (int i = 0; i < laneCount; ++i) lanes[i].cabinet = i;
}
void OnePole::prepare(double rate, float hz) {
    state = 0.f;
    coefficient = 1.f - std::exp(-2.f * pi * hz / static_cast<float>(rate));
}
void Fir::prepare() {
    history.fill(0.f); position = 0;
    float sum = 0.f;
    for (int i = 0; i < taps; ++i) {
        const float x = static_cast<float>(i - 32);
        // 4x interpolation/reconstruction, 0.1125 cycles per high-rate sample.
        const float sinc = x == 0.f ? .225f : std::sin(2.f * pi * .1125f * x) / (pi * x);
        const float window = .42f - .5f * std::cos(2.f * pi * i / 64.f)
                                   + .08f * std::cos(4.f * pi * i / 64.f);
        kernel[i] = sinc * window; sum += kernel[i];
    }
    for (auto& k : kernel) k /= sum;
}
float Fir::tick(float x) noexcept {
    history[position] = x;
    float y = 0.f; int p = position;
    for (int i = 0; i < taps; ++i) {
        y += kernel[i] * history[p];
        if (--p < 0) p = taps - 1;
    }
    if (++position == taps) position = 0;
    return y;
}
void Cabinet::prepare(double rate) {
    // Synthetic, original speaker/cabinet responses. These are NOT measured IRs.
    constexpr float lowCuts[] {75.f, 85.f, 65.f, 90.f, 48.f, 40.f};
    constexpr float highCuts[] {4200.f, 5100.f, 3600.f, 5800.f, 2900.f, 2400.f};
    constexpr float resonance[] {120.f, 145.f, 105.f, 170.f, 80.f, 65.f};
    for (int c = 0; c < cabinetCount; ++c) {
        OnePole hp, lp1, lp2;
        hp.prepare(rate, lowCuts[c]); lp1.prepare(rate, highCuts[c]); lp2.prepare(rate, highCuts[c]);
        float sum = 0.f;
        for (int i = 0; i < taps; ++i) {
            float x = i == 0 ? 1.f : 0.f;
            x = lp2.low(lp1.low(hp.high(x)));
            const float t = static_cast<float>(i / rate);
            x += .006f * std::exp(-t * 180.f) * std::sin(2.f * pi * resonance[c] * t);
            kernels[c][i] = x; sum += std::abs(x);
        }
        for (auto& x : kernels[c]) x *= 1.8f / std::max(1.f, sum);
    }
    history.fill(0.f); current = kernels[0]; position = selected = remaining = 0;
}
void Cabinet::select(int index) noexcept {
    index = std::clamp(index, 0, cabinetCount - 1);
    if (selected != index) { selected = index; remaining = 1024; }
}
float Cabinet::tick(float x) noexcept {
    history[position] = x;
    float y = 0.f; int p = position;
    for (int i = 0; i < taps; ++i) {
        if (remaining > 0) current[i] += (kernels[selected][i] - current[i]) / remaining;
        y += current[i] * history[p];
        if (--p < 0) p = taps - 1;
    }
    if (remaining > 0) --remaining;
    if (++position == taps) position = 0;
    return y;
}
void Amp::prepare(double rate) {
    low.prepare(rate, 220.f); high.prepare(rate, 1800.f);
    coupling.prepare(rate, 35.f); presence.prepare(rate, 3300.f); dc.prepare(rate, 8.f);
    envelope = 0.f; sagCoefficient = 1.f - std::exp(-1.f / static_cast<float>(rate * .08));
}
float Amp::tick(float x, const Lane& p, int model) noexcept {
    constexpr float inputGain[] {4.f, 6.f, 18.f, 3.5f};
    constexpr float bias[] {.06f, .12f, .035f, .09f};
    const float clean = coupling.high(x);
    envelope += sagCoefficient * (std::abs(clean) - envelope);
    const float sag = 1.f / (1.f + envelope * (model == 2 ? .3f : 1.8f));
    float y = saturate(clean * (.3f + p.gain * inputGain[model]), bias[model]);
    if (model == 2) { y = saturate(y * (1.f + p.gain * 4.f), -.04f); }
    if (model == 1) { y = saturate(y * 1.7f, .1f); }
    const float bottom = low.low(y), top = high.high(y), middle = y - bottom - top;
    y = bottom * db((p.bass - .5f) * 18.f) + middle * db((p.mid - .5f) * 16.f)
            + top * db((p.treble - .5f) * 18.f);
    y += presence.high(y) * p.presence * .7f;
    return dc.high(saturate(y * sag * 1.3f, bias[model] * .5f)) * .65f;
}
void Delay::prepare(double sampleRate) {
    rate = sampleRate; phase = 0.; position = 0; timeMs = 380.f;
    for (int i = 0; i < 2; ++i) {
        lines[i].assign(static_cast<std::size_t>(rate * 2.1) + 8, 0.f);
        damping[i].prepare(rate, 4300.f);
    }
}
float Delay::read(const std::vector<float>& line, float samples) const noexcept {
    double p = static_cast<double>(position) - std::clamp(samples, 1.f, static_cast<float>(line.size() - 2));
    if (p < 0.) p += static_cast<double>(line.size());
    const auto a = static_cast<std::size_t>(p);
    const float f = static_cast<float>(p - static_cast<double>(a));
    return line[a] + f * (line[(a + 1) % line.size()] - line[a]);
}
Stereo Delay::tick(Stereo input, const Parameters& p) noexcept {
    timeMs += .0002f * (p.delayMs - timeMs);
    phase += 2. * pi * .63 / rate;
    if (phase > 2. * pi) phase -= 2. * pi;
    Stereo wet;
    float* channels[] {&wet.left, &wet.right};
    float feed[] {input.left, input.right};
    for (int c = 0; c < 2; ++c) {
        const float wow = p.delayMode == DelayMode::studio ? .00015f : .0012f;
        const float offset = static_cast<float>(std::sin(phase + c * 1.73)) * wow;
        const float samples = static_cast<float>(rate) * timeMs * .001f * (1.f + offset);
        const float primary = read(lines[c], samples);
        float tap = primary;
        if (p.delayMode == DelayMode::drum)
            tap = .35f * read(lines[c], samples * .25f) + .25f * read(lines[c], samples * .5f)
                  + .15f * read(lines[c], samples * .75f) + .25f * primary;
        // Separate L/R state: stereo input is never collapsed to mono.
        const float filtered = damping[c].low(tap);
        const float colour = p.delayMode == DelayMode::studio ? filtered : std::tanh(filtered * 1.2f) / 1.2f;
        lines[c][position] = feed[c] + p.feedback * colour;
        *channels[c] = colour;
    }
    if (++position == lines[0].size()) position = 0;
    return wet;
}
void Reverb::prepare(double sampleRate) {
    rate = sampleRate;
    constexpr double lengths[] {.0297, .0371, .0411, .0437, .0531, .0593, .0617, .0719};
    for (int i = 0; i < 8; ++i)
        lines[i].assign(static_cast<std::size_t>(rate * lengths[i]), 0.f);
    positions.fill(0); damped.fill(0.f);
}
Stereo Reverb::tick(Stereo input, const Parameters& p) noexcept {
    std::array<float, 8> v {};
    float sum = 0.f;
    // Orthogonal Householder feedback network with separate stereo injection/output vectors.
    for (int i = 0; i < 8; ++i) {
        v[i] = lines[i][positions[i]];
        damped[i] += (.03f + (1.f - p.reverbDamping) * .65f) * (v[i] - damped[i]);
        sum += damped[i];
    }
    constexpr float lIn[] {1, 1, -1, -1, 1, -1, 1, -1};
    constexpr float rIn[] {1, -1, 1, -1, -1, 1, 1, -1};
    constexpr float lOut[] {1, -1, -1, 1, 1, 1, -1, -1};
    constexpr float rOut[] {-1, 1, -1, 1, 1, -1, -1, 1};
    Stereo wet;
    for (int i = 0; i < 8; ++i) {
        const float seconds = static_cast<float>(lines[i].size() / rate);
        const float rt60 = p.reverbDecay * (.5f + p.reverbSize);
        const float feedback = std::pow(10.f, -3.f * seconds / rt60);
        lines[i][positions[i]] = .18f * (lIn[i] * input.left + rIn[i] * input.right)
                                  + feedback * (.25f * sum - damped[i]);
        if (++positions[i] == lines[i].size()) positions[i] = 0;
        wet.left += v[i] * lOut[i] * .25f;
        wet.right += v[i] * rOut[i] * .25f;
    }
    return wet;
}
void Engine::prepare(double sampleRate) {
    if (!std::isfinite(sampleRate) || sampleRate < 22050. || sampleRate > 384000.)
        throw std::invalid_argument("Sample rate must be 22050..384000 Hz");
    smoothing = 1.f - std::exp(-1.f / static_cast<float>(sampleRate * .02));
    interpolator.prepare();
    for (int i = 0; i < laneCount; ++i) {
        decimators[i].prepare(); amps[i].prepare(sampleRate * 4.); cabs[i].prepare(sampleRate);
    }
    driveTone.prepare(sampleRate * 4., 2200.f); fuzzTone.prepare(sampleRate * 4., 1000.f);
    pedalHighPass.prepare(sampleRate * 4., 35.f); pedalDc.prepare(sampleRate * 4., 10.f);
    delay.prepare(sampleRate); reverb.prepare(sampleRate);
    current = target;
    driveMix = target.pedals.driveOn ? 1.f : 0.f;
    fuzzMix = target.pedals.fuzzOn ? 1.f : 0.f;
    for (auto& l : current.lanes) if (!l.enabled) l.level = 0.f;
    const bool anySolo = std::any_of(target.lanes.begin(), target.lanes.end(), [](const Lane& l) { return l.solo && l.enabled; });
    if (anySolo) for (int i = 0; i < laneCount; ++i) if (!target.lanes[i].solo) current.lanes[i].level = 0.f;
    clearMeters();
}
void Engine::setParameters(const Parameters& p) noexcept {
    target = p;
    target.inputDb = bounded(p.inputDb, -24.f, 24.f); target.outputDb = bounded(p.outputDb, -60.f, 6.f);
    auto unit = [](float& x) { x = bounded(x, 0.f, 1.f); };
    auto& pd = target.pedals;
    unit(pd.drive); unit(pd.driveTone); unit(pd.driveLevel);
    unit(pd.sustain); unit(pd.fuzzTone); unit(pd.fuzzLevel);
    pd.fuzzType = std::clamp(pd.fuzzType, 0, 3);
    for (auto& l : target.lanes) {
        unit(l.gain); unit(l.bass); unit(l.mid); unit(l.treble); unit(l.presence); unit(l.level);
        l.pan = bounded(l.pan, -1.f, 1.f); l.cabinet = std::clamp(l.cabinet, 0, cabinetCount - 1);
    }
    target.delayMs = bounded(p.delayMs, 40.f, 1500.f);
    target.feedback = bounded(p.feedback, 0.f, .88f);
    unit(target.delayBlend); unit(target.reverbBlend); unit(target.reverbSize); unit(target.reverbDamping);
    target.reverbDecay = bounded(p.reverbDecay, .2f, 12.f);
    if (static_cast<int>(target.delayMode) < 0 || static_cast<int>(target.delayMode) > 2)
        target.delayMode = DelayMode::echo;
}
void Engine::smooth() noexcept {
    auto s = [this](float& a, float b) { a += smoothing * (b - a); };
    s(driveMix, target.pedals.driveOn ? 1.f : 0.f);
    s(fuzzMix, target.pedals.fuzzOn ? 1.f : 0.f);
    s(current.inputDb, target.inputDb); s(current.outputDb, target.outputDb);
    auto& a = current.pedals; const auto& b = target.pedals;
    // Switches crossfade in tick; tone, level and drive are smoothed continuously.
    s(a.drive, b.drive); s(a.driveTone, b.driveTone); s(a.driveLevel, b.driveLevel);
    s(a.sustain, b.sustain); s(a.fuzzTone, b.fuzzTone); s(a.fuzzLevel, b.fuzzLevel);
    a.fuzzType = b.fuzzType;
    const bool anySolo = std::any_of(target.lanes.begin(), target.lanes.end(), [](const Lane& l) { return l.solo && l.enabled; });
    for (int i = 0; i < laneCount; ++i) {
        auto& x = current.lanes[i]; const auto& y = target.lanes[i];
        s(x.gain, y.gain); s(x.bass, y.bass); s(x.mid, y.mid); s(x.treble, y.treble); s(x.presence, y.presence);
        s(x.level, y.enabled && (!anySolo || y.solo) ? y.level : 0.f); s(x.pan, y.pan);
        cabs[i].select(y.cabinet);
    }
    s(current.delayBlend, target.delayBlend); s(current.feedback, target.feedback);
    s(current.reverbBlend, target.reverbBlend); s(current.reverbSize, target.reverbSize);
    s(current.reverbDecay, target.reverbDecay); s(current.reverbDamping, target.reverbDamping);
    current.delayMs = target.delayMs; current.delayMode = target.delayMode;
}
Stereo Engine::tick(float input) noexcept {
    smooth();
    input = (std::isfinite(input) ? std::clamp(input, -16.f, 16.f) : 0.f) * db(current.inputDb);
    std::array<float, laneCount> down {};
    for (int phase = 0; phase < 4; ++phase) {
        float x = interpolator.tick(phase == 0 ? input * 4.f : 0.f);
        const auto& p = current.pedals;
        const float lowDrive = driveTone.low(x);
        const float driven = ((1.f - p.drive) * x + p.drive * saturate(x * (2.f + p.drive * 18.f), .08f));
        const float driveOut = (driven * (.4f + p.driveTone) + lowDrive * (1.f - p.driveTone)) * p.driveLevel * 2.f;
        // Both paths are always warm; switching bypass is smoothed at base rate.
        x += driveMix * (driveOut - x);
        const float fuzzInput = pedalHighPass.high(x);
        constexpr float fuzzGains[] {28.f, 21.f, 16.f, 34.f};
        constexpr float scoops[] {.65f, .8f, .95f, .5f};
        float fuzz = std::tanh(fuzzInput * (2.f + p.sustain * fuzzGains[p.fuzzType]));
        fuzz = std::tanh(fuzz * 2.2f);
        const float bottom = fuzzTone.low(fuzz);
        fuzz = (bottom * (1.f - p.fuzzTone) + (fuzz - bottom) * p.fuzzTone * scoops[p.fuzzType]) * p.fuzzLevel;
        x += fuzzMix * (fuzz - x);
        x = pedalDc.high(x);
        for (int i = 0; i < laneCount; ++i) {
            const float v = decimators[i].tick(amps[i].tick(x, current.lanes[i], i));
            if (phase == 0) down[i] = v;
        }
    }
    Stereo dry;
    for (int i = 0; i < laneCount; ++i) {
        const auto& p = current.lanes[i];
        const float y = cabs[i].tick(down[i]) * p.level;
        lanePeaks[i] = std::max(lanePeaks[i], std::abs(y));
        const float angle = (p.pan + 1.f) * pi * .25f;
        dry.left += y * std::cos(angle); dry.right += y * std::sin(angle);
    }
    const Stereo echo = delay.tick(dry, current), space = reverb.tick(dry, current);
    const float gain = db(current.outputDb);
    Stereo result {(dry.left + echo.left * current.delayBlend + space.left * current.reverbBlend) * gain,
                   (dry.right + echo.right * current.delayBlend + space.right * current.reverbBlend) * gain};
    // Soft safety ceiling, not a mastered loudness/true-peak limiter.
    auto ceiling = [](float x) { return std::abs(x) <= .9f ? x : std::copysign(.9f + .1f * std::tanh((std::abs(x) - .9f) * 10.f), x); };
    result.left = ceiling(result.left); result.right = ceiling(result.right);
    return result;
}
void Engine::process(const float* mono, float* left, float* right, std::size_t frames) noexcept {
    clearMeters();
    for (std::size_t i = 0; i < frames; ++i) {
        const auto y = tick(mono[i]); left[i] = y.left; right[i] = y.right;
    }
}
}
