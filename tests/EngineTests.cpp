#include "dsp/Engine.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <random>
#include <stdexcept>

static std::atomic<bool> countAllocations {false};
static std::atomic<int> allocations {0};
void* operator new(std::size_t n) {
    if (countAllocations) ++allocations;
    if (void* p = std::malloc(n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {
void require(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
double energy(heavy::Stereo x) { return x.left * x.left + x.right * x.right; }
void routing() {
    heavy::Parameters p;
    p.lanes[0].pan = -1.f;
    heavy::Engine a; a.setParameters(p); a.prepare(48000);
    double l = 0., r = 0.;
    for (int i = 0; i < 12000; ++i) {
        auto y = a.tick(.15f * std::sin(i * .031f));
        l += y.left * y.left; r += y.right * y.right;
    }
    require(l > .01 && r < 1.e-12, "Hard-left pan must silence the right dry channel");
    for (auto& lane : p.lanes) lane.enabled = false;
    a.setParameters(p); a.prepare(48000);
    for (int i = 0; i < 2000; ++i) require(energy(a.tick(.2f)) == 0., "Disabled lanes must be silent after prepare");
    for (int lane = 0; lane < 4; ++lane) {
        heavy::Parameters base;
        for (auto& x : base.lanes) { x.level = 0.f; x.enabled = false; }
        base.lanes[lane].enabled = true; base.lanes[lane].level = .6f;
        heavy::Engine x, y; x.setParameters(base); x.prepare(48000);
        auto changed = base;
        changed.lanes[(lane + 1) % 4].gain = 1.f;
        changed.lanes[(lane + 1) % 4].cabinet = 5;
        y.setParameters(changed); y.prepare(48000);
        double e = 0.;
        for (int i = 0; i < 3000; ++i) {
            const float in = .2f * std::sin(i * .04f);
            auto a1 = x.tick(in), b1 = y.tick(in);
            require(a1.left == b1.left && a1.right == b1.right, "Inactive lane controls must not affect another lane");
            e += energy(a1);
        }
        require(e > .01, "Each lane must produce audio independently");
    }
}
void stereoEffects() {
    heavy::Parameters p; p.delayMs = 40; p.feedback = .7f;
    for (int mode = 0; mode < 3; ++mode) {
        heavy::Delay d; d.prepare(48000); p.delayMode = static_cast<heavy::DelayMode>(mode);
        double l = 0., r = 0.;
        for (int i = 0; i < 120000; ++i) {
            const auto y = d.tick({i == 0 ? 1.f : 0.f, 0.f}, p);
            l += y.left * y.left; r += y.right * y.right;
        }
        require(l > .0001 && r == 0., "Delay must preserve independent L/R input paths in every mode");
    }
    heavy::Reverb reverb; reverb.prepare(48000); p.reverbDecay = .2f;
    double early = 0., late = 0., difference = 0.;
    for (int i = 0; i < 96000; ++i) {
        const auto y = reverb.tick({i == 0 ? 1.f : 0.f, 0.f}, p);
        require(std::isfinite(y.left) && std::isfinite(y.right), "Reverb must be finite");
        if (i < 24000) early += energy(y);
        if (i > 72000) late += energy(y);
        difference += (y.left - y.right) * (y.left - y.right);
    }
    require(early > .001 && difference > .001, "Reverb must produce a non-mono stereo tail");
    require(late < early * .0001, "Short reverb must decay without runaway feedback");
    heavy::Reverb left, right; left.prepare(48000); right.prepare(48000);
    double distinct = 0.;
    for (int i = 0; i < 8000; ++i) {
        auto a = left.tick({i == 0 ? 1.f : 0.f, 0}, p);
        auto b = right.tick({0, i == 0 ? 1.f : 0.f}, p);
        distinct += (a.left - b.left) * (a.left - b.left);
    }
    require(distinct > .001, "Reverb must respond distinctly to the two input channels");
}
void blocksAndRealtime() {
    heavy::Engine a, b; heavy::Parameters p;
    p.pedals.driveOn = p.pedals.fuzzOn = true; p.delayBlend = .4f; p.reverbBlend = .3f;
    for (auto& lane : p.lanes) lane.level = .5f;
    a.setParameters(p); b.setParameters(p); a.prepare(48000); b.prepare(48000);
    std::array<float, 1024> in {}, l {}, r {};
    for (int i = 0; i < 1024; ++i) in[i] = .2f * std::sin(i * .071f);
    allocations = 0; countAllocations = true;
    a.setParameters(p); a.process(in.data(), l.data(), r.data(), in.size());
    countAllocations = false;
    require(allocations == 0, "The audio callback must allocate no heap memory");
    for (int i = 0; i < 1024; ++i) {
        auto y = b.tick(in[i]);
        require(y.left == l[i] && y.right == r[i], "Processing must not depend on block boundaries");
    }
}
void stress() {
    std::mt19937 rng(12); std::uniform_real_distribution<float> noise(-2.f, 2.f);
    for (double rate : {22050., 44100., 48000., 96000., 192000., 384000.}) {
        heavy::Engine engine; engine.prepare(rate);
        heavy::Parameters p;
        for (auto& lane : p.lanes) { lane.level = 1; lane.gain = 1; lane.bass = 1; lane.presence = 1; }
        p.pedals.driveOn = p.pedals.fuzzOn = true;
        p.feedback = .88f; p.reverbDecay = 12; p.reverbBlend = p.delayBlend = 1;
        p.inputDb = 24; p.outputDb = 6;
        for (int i = 0; i < 6000; ++i) {
            if (i % 64 == 0) {
                for (auto& lane : p.lanes) { lane.cabinet = (i / 64) % 6; lane.pan = noise(rng); }
                p.delayMode = static_cast<heavy::DelayMode>((i / 64) % 3);
                p.pedals.fuzzType = (i / 64) % 4; engine.setParameters(p);
            }
            auto y = engine.tick(i % 1000 == 0 ? std::numeric_limits<float>::quiet_NaN() : noise(rng));
            require(std::isfinite(y.left) && std::isfinite(y.right), "All sample rates/settings must remain finite");
            require(std::abs(y.left) <= 1.f && std::abs(y.right) <= 1.f, "Safety ceiling must bound extreme signals");
        }
    }
    heavy::Engine e; bool threw = false;
    try { e.prepare(0); } catch (const std::invalid_argument&) { threw = true; }
    require(threw, "Invalid sample rates must be rejected");
}
void filters() {
    heavy::Fir f; f.prepare(); float sum = 0; int peak = 0; float top = 0;
    for (int i = 0; i < 100; ++i) {
        const float y = f.tick(i == 0 ? 1.f : 0.f); sum += y;
        if (std::abs(y) > top) { top = std::abs(y); peak = i; }
    }
    require(std::abs(sum - 1.f) < 1.e-5f && peak == 32, "Oversampling FIR must have unity DC gain and correct group delay");
}
}
int main() {
    try {
        filters(); std::cout << "PASS oversampling filter\n";
        routing(); std::cout << "PASS four-lane isolation, mute and pan\n";
        stereoEffects(); std::cout << "PASS stereo delay modes and reverb decay\n";
        blocksAndRealtime(); std::cout << "PASS zero allocations and block invariance\n";
        stress(); std::cout << "PASS sample-rate / automation stress\n";
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
