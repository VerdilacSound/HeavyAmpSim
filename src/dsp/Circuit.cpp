#include "Circuit.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace heavy::circuit {
namespace {
constexpr double pi = 3.14159265358979323846;
double softplus(double x) noexcept {
    return x > 40. ? x : (x < -40. ? std::exp(x) : std::log1p(std::exp(x)));
}
double control(double x) noexcept { return std::isfinite(x) ? std::clamp(x, 0., 1.) : .5; }
}
double korenPlateCurrent(double vpk, double vgk, const ValveParameters& p) noexcept {
    if (!(vpk > 0.)) return 0.;
    const double e = vpk / p.kp * softplus(p.kp * (1. / p.mu + vgk / std::sqrt(p.kvb + vpk * vpk)));
    return 2. * std::pow(e, p.exponent) / p.kg;
}
void CommonCathode::prepare(double rate, ValveParameters values) {
    const std::array<double, 11> all {{values.supply, values.plateResistance, values.cathodeResistance,
        values.bypassCapacitance, values.gridStopper, values.mu, values.kg, values.exponent, values.kp, values.kvb, rate}};
    for (const auto value : all) if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite valve parameter");
    if (!(std::isfinite(rate) && rate >= 22050. && rate <= 1536000.) ||
        !(values.supply > 0. && values.plateResistance > 0. && values.cathodeResistance > 0.) ||
        values.bypassCapacitance < 0. || values.gridStopper < 0. ||
        !(values.mu > 0. && values.kg > 0. && values.kp > 0. && values.kvb > 0. && values.exponent > 0.))
        throw std::invalid_argument("Invalid valve components or sample rate");
    p = values;
    // Static resistor-loaded Koren load line. Cathode voltage is small relative
    // to B+: this table omits its subtraction from B+ (documented approximation).
    for (std::size_t i = 0; i < tableSize; ++i) {
        const double vgk = -40. + 56. * static_cast<double>(i) / static_cast<double>(tableSize - 1);
        double lo = 0., hi = p.supply;
        for (int j = 0; j < 48; ++j) {
            const double vp = .5 * (lo + hi);
            if (vp + p.plateResistance * korenPlateCurrent(vp, vgk, p) > p.supply) hi = vp;
            else lo = vp;
        }
        currents[i] = (p.supply - .5 * (lo + hi)) / p.plateResistance;
    }
    double lo = 0., hi = p.supply;
    for (int j = 0; j < 80; ++j) {
        const double vk = .5 * (lo + hi);
        if (vk / p.cathodeResistance > plate(-vk).value) hi = vk; else lo = vk;
    }
    cathode = biasK = .5 * (lo + hi);
    biasP = p.supply - p.plateResistance * plate(-biasK).value;
    capacitorConductance = p.bypassCapacitance * rate; // backward Euler at cathode
}
CommonCathode::Current CommonCathode::plate(double vgk) const noexcept {
    const double x = std::clamp((vgk + 40.) * static_cast<double>(tableSize - 1) / 56.,
                                0., static_cast<double>(tableSize - 1));
    const auto i = std::min(static_cast<std::size_t>(x), tableSize - 2);
    const double slope = (currents[i + 1] - currents[i]) * static_cast<double>(tableSize - 1) / 56.;
    return {currents[i] + (x - static_cast<double>(i)) * (currents[i + 1] - currents[i]),
            vgk > -40. && vgk < 16. ? slope : 0.};
}
CommonCathode::Current CommonCathode::grid(double source) const noexcept {
    if (source <= 0.) return {source, 1.};
    // Approximate grid conduction as a Shockley junction driven through Rg.
    // These junction parameters are modeling assumptions, not measured 12AX7 data.
    constexpr double is = 2.e-9, vt = .2;
    const double r = p.gridStopper;
    if (r == 0.) return {source, 1.};
    double v = std::min(source, vt * std::log1p(source / (r * is)));
    for (int j = 0; j < 6; ++j) {
        const double e = std::exp(std::clamp(v / vt, -40., 40.));
        const double f = v + r * is * (e - 1.) - source;
        v = std::clamp(v - f / (1. + r * is * e / vt), 0., source);
    }
    return {v, 1. / (1. + r * is * std::exp(v / vt) / vt)};
}
double CommonCathode::tick(double input) noexcept {
    if (!std::isfinite(input)) input = 0.;
    input = std::clamp(input, -1000., 1000.);
    const double conductance = capacitorConductance + 1. / p.cathodeResistance;
    const double history = capacitorConductance * cathode;
    double vk = cathode;
    for (int j = 0; j < 8; ++j) {
        const auto g = grid(input - vk);
        const auto a = plate(g.value);
        const double ig = p.gridStopper > 0. ? std::max(0., (input - vk - g.value) / p.gridStopper) : 0.;
        const double dig = p.gridStopper > 0. ? (1. - g.derivative) / p.gridStopper : 0.;
        const double residual = conductance * vk - history - a.value - ig;
        const double derivative = conductance + a.derivative * g.derivative + dig;
        const double next = std::clamp(vk - residual / derivative, 0., p.supply);
        if (std::abs(next - vk) < 1.e-10) { vk = next; break; }
        vk = next;
    }
    cathode = vk;
    const double ip = plate(grid(input - vk).value).value;
    return p.supply - p.plateResistance * ip - biasP;
}
void RcHighPass::prepare(double rate, double resistance, double capacitance) {
    if (!(rate > 0. && resistance > 0. && capacitance > 0.)) throw std::invalid_argument("Invalid RC network");
    g = 1. / (1. + 2. * rate * resistance * capacitance);
    memory = 0.;
}
double RcHighPass::tick(double input) noexcept {
    const double v = (input - memory) * g;
    const double low = memory + v;
    memory = low + v;
    return input - low;
}
PassiveNetwork::PassiveNetwork(int n) : nodes(n) {
    if (nodes < 1 || nodes > maximumNodes) throw std::invalid_argument("Invalid circuit node count");
}
int PassiveNetwork::resistor(int a, int b, double ohms) {
    if (a < input || b < input || a >= nodes || b >= nodes || a == b ||
        !std::isfinite(ohms) || ohms <= 0. || resistorCount == static_cast<int>(resistors.size()))
        throw std::invalid_argument("Invalid resistor");
    resistors[static_cast<std::size_t>(resistorCount)] = {a, b, ohms};
    return resistorCount++;
}
int PassiveNetwork::capacitor(int a, int b, double farads) {
    if (a < input || b < input || a >= nodes || b >= nodes || a == b ||
        !std::isfinite(farads) || farads <= 0. || capacitorCount == static_cast<int>(capacitors.size()))
        throw std::invalid_argument("Invalid capacitor");
    capacitors[static_cast<std::size_t>(capacitorCount)] = {a, b, farads, 0., 0.};
    return capacitorCount++;
}
void PassiveNetwork::setResistance(int id, double ohms) noexcept {
    if (id < 0 || id >= resistorCount || !std::isfinite(ohms) || ohms <= 0.) return;
    auto& r = resistors[static_cast<std::size_t>(id)].resistance;
    if (std::abs(r - ohms) > 1.e-9) { r = ohms; dirty = true; }
}
void PassiveNetwork::stamp(Matrix& m, int a, int b, double g) const noexcept {
    if (a >= 0) m[static_cast<std::size_t>(a)][static_cast<std::size_t>(a)] += g;
    if (b >= 0) m[static_cast<std::size_t>(b)][static_cast<std::size_t>(b)] += g;
    if (a >= 0 && b >= 0) {
        m[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)] -= g;
        m[static_cast<std::size_t>(b)][static_cast<std::size_t>(a)] -= g;
    }
}
void PassiveNetwork::prepare(double rate) {
    if (!(rate > 0. && std::isfinite(rate))) throw std::invalid_argument("Invalid circuit sample rate");
    voltages.fill(0.);
    for (int i = 0; i < capacitorCount; ++i) {
        auto& c = capacitors[static_cast<std::size_t>(i)];
        c.conductance = 2. * c.capacitance * rate; c.history = 0.;
    }
    dirty = true;
    if (!update()) throw std::invalid_argument("Circuit is singular");
}
bool PassiveNetwork::update() noexcept {
    if (!dirty) return true;
    Matrix m {}, candidate {};
    for (int i = 0; i < nodes; ++i) candidate[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] = 1.;
    for (int i = 0; i < resistorCount; ++i) {
        const auto& r = resistors[static_cast<std::size_t>(i)]; stamp(m, r.a, r.b, 1. / r.resistance);
    }
    for (int i = 0; i < capacitorCount; ++i) {
        const auto& c = capacitors[static_cast<std::size_t>(i)]; stamp(m, c.a, c.b, c.conductance);
    }
    for (int col = 0; col < nodes; ++col) {
        const auto k = static_cast<std::size_t>(col);
        int pivot = col;
        for (int row = col + 1; row < nodes; ++row)
            if (std::abs(m[static_cast<std::size_t>(row)][k]) > std::abs(m[static_cast<std::size_t>(pivot)][k])) pivot = row;
        if (std::abs(m[static_cast<std::size_t>(pivot)][k]) < 1.e-18) return false;
        std::swap(m[k], m[static_cast<std::size_t>(pivot)]);
        std::swap(candidate[k], candidate[static_cast<std::size_t>(pivot)]);
        const double reciprocal = 1. / m[k][k];
        for (int j = 0; j < nodes; ++j) { m[k][static_cast<std::size_t>(j)] *= reciprocal; candidate[k][static_cast<std::size_t>(j)] *= reciprocal; }
        for (int row = 0; row < nodes; ++row) {
            if (row == col) continue;
            const auto r = static_cast<std::size_t>(row); const double scale = m[r][k];
            for (int j = 0; j < nodes; ++j) {
                m[r][static_cast<std::size_t>(j)] -= scale * m[k][static_cast<std::size_t>(j)];
                candidate[r][static_cast<std::size_t>(j)] -= scale * candidate[k][static_cast<std::size_t>(j)];
            }
        }
    }
    inverse = candidate; dirty = false; return true;
}
void PassiveNetwork::tick(double vin) noexcept {
    if (!std::isfinite(vin)) vin = 0.;
    std::array<double, maximumNodes> b {};
    auto source = [&b, vin](int a, int c, double g) {
        if (a >= 0 && c == input) b[static_cast<std::size_t>(a)] += g * vin;
        if (c >= 0 && a == input) b[static_cast<std::size_t>(c)] += g * vin;
    };
    for (int i = 0; i < resistorCount; ++i) {
        const auto& r = resistors[static_cast<std::size_t>(i)]; source(r.a, r.b, 1. / r.resistance);
    }
    for (int i = 0; i < capacitorCount; ++i) {
        const auto& c = capacitors[static_cast<std::size_t>(i)]; source(c.a, c.b, c.conductance);
        if (c.a >= 0) b[static_cast<std::size_t>(c.a)] -= c.history;
        if (c.b >= 0) b[static_cast<std::size_t>(c.b)] += c.history;
    }
    for (int i = 0; i < nodes; ++i) {
        double v = 0.;
        for (int j = 0; j < nodes; ++j) v += inverse[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] * b[static_cast<std::size_t>(j)];
        voltages[static_cast<std::size_t>(i)] = v;
    }
    auto voltageAt = [this, vin](int node) { return node == input ? vin : (node == ground ? 0. : voltage(node)); };
    for (int i = 0; i < capacitorCount; ++i) {
        auto& c = capacitors[static_cast<std::size_t>(i)];
        c.history = -2. * c.conductance * (voltageAt(c.a) - voltageAt(c.b)) - c.history;
    }
}
ModelTToneStack::ModelTToneStack() {
    // 0: treble top, 1: slope junction, 2: bass top / treble bottom,
    // 3: mid top, 4: mid wiper, 5: treble wiper / output.
    network.resistor(PassiveNetwork::input, 1, 56000.);
    network.capacitor(PassiveNetwork::input, 0, 270.e-12);
    network.capacitor(1, 2, 22.e-9); network.capacitor(1, 4, 22.e-9);
    bassR = network.resistor(2, 3, 500000.);
    midUpper = network.resistor(3, 4, 12500.); midLower = network.resistor(4, PassiveNetwork::ground, 12500.);
    trebleUpper = network.resistor(0, 5, 500000.); trebleLower = network.resistor(5, 2, 500000.);
    network.resistor(5, PassiveNetwork::ground, 1.e6);
}
void ModelTToneStack::prepare(double rate) { network.prepare(rate); }
void ModelTToneStack::setControls(double bass, double middle, double treble) noexcept {
    bass = control(bass); middle = control(middle); treble = control(treble);
    network.setResistance(bassR, std::max(1., bass * 1.e6));
    network.setResistance(midUpper, std::max(1., (1. - middle) * 25000.));
    network.setResistance(midLower, std::max(1., middle * 25000.));
    network.setResistance(trebleUpper, std::max(1., (1. - treble) * 1.e6));
    network.setResistance(trebleLower, std::max(1., treble * 1.e6));
    network.update();
}
double ModelTToneStack::tick(double input) noexcept { network.tick(input); return network.voltage(5); }
void ModelTPreampStudy::prepare(double rate) {
    ValveParameters input;
    normalStage.prepare(rate, input); brightStage.prepare(rate, input);
    auto gain = input; gain.cathodeResistance = 820.; gain.bypassCapacitance = 22.e-6; gain.gridStopper = 470000.;
    gainStage.prepare(rate, gain);
    normalCoupling.prepare(rate, 1.e6, 22.e-9); brightCoupling.prepare(rate, 1.e6, 22.e-9);
    brightMixer.prepare(rate, 470000. / 2., 470.e-12);
    tone.prepare(rate);
}
void ModelTPreampStudy::setControls(double n, double b, double bass, double mid, double treble) noexcept {
    normalVolume = control(n); brightVolume = control(b); tone.setControls(bass, mid, treble);
}
double ModelTPreampStudy::tick(double input) noexcept {
    const double n = normalCoupling.tick(normalStage.tick(input)) * normalVolume;
    const double b = brightCoupling.tick(brightStage.tick(input)) * brightVolume;
    // Hnormal = 1/(2+sRC), Hbright = (1+sRC)/(2+sRC).
    const double mixed = .5 * (n + b) + .5 * brightMixer.tick(b - n);
    return tone.tick(gainStage.tick(mixed));
}
}
