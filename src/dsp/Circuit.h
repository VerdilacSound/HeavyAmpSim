#pragma once
#include <array>
#include <cstddef>

namespace heavy::circuit {

// Voltages, amperes, ohms and farads throughout. This module contains circuit
// primitives, independent of plugin gain calibration and cabinet processing.
struct ValveParameters {
    double supply = 330., plateResistance = 100000., cathodeResistance = 680.;
    double bypassCapacitance = 250.e-6, gridStopper = 68000.;
    double mu = 83.5, kg = 1060., exponent = 1.4, kp = 600., kvb = 300.;
};
double korenPlateCurrent(double plateToCathode, double gridToCathode,
                         const ValveParameters&) noexcept;

class CommonCathode {
public:
    void prepare(double sampleRate, ValveParameters);
    // Input is grid-source volts; output is AC anode volts. Positive input
    // produces an inverted output. The coupling network belongs to the caller.
    double tick(double gridSourceVoltage) noexcept;
    double biasCathode() const noexcept { return biasK; }
    double biasPlate() const noexcept { return biasP; }
    double cathodeVoltage() const noexcept { return cathode; }
    const ValveParameters& components() const noexcept { return p; }
private:
    static constexpr std::size_t tableSize = 8193;
    struct Current { double value, derivative; };
    Current plate(double gridToCathode) const noexcept;
    Current grid(double sourceToCathode) const noexcept;
    ValveParameters p;
    std::array<double, tableSize> currents {};
    double cathode = 0., biasK = 0., biasP = 0., capacitorConductance = 0.;
};

class RcHighPass {
public:
    void prepare(double sampleRate, double resistance, double capacitance);
    double tick(double input) noexcept;
private:
    double g = 0., memory = 0.;
};

// Linear nodal analysis with trapezoidal capacitor companions. One ideal
// signal source, ground, up to eight internal nodes; bounded storage and no
// allocation. Resistors can change without resetting capacitor charge.
class PassiveNetwork {
public:
    static constexpr int ground = -1, input = -2, maximumNodes = 8;
    explicit PassiveNetwork(int nodes);
    int resistor(int a, int b, double ohms);
    int capacitor(int a, int b, double farads);
    void setResistance(int id, double ohms) noexcept;
    void prepare(double sampleRate);
    bool update() noexcept;
    void tick(double inputVoltage) noexcept;
    double voltage(int node) const noexcept { return voltages[static_cast<std::size_t>(node)]; }
private:
    struct Resistor { int a = 0, b = 0; double resistance = 1.; };
    struct Capacitor { int a = 0, b = 0; double capacitance = 0., conductance = 0., history = 0.; };
    using Matrix = std::array<std::array<double, maximumNodes>, maximumNodes>;
    void stamp(Matrix&, int a, int b, double conductance) const noexcept;
    int nodes, resistorCount = 0, capacitorCount = 0;
    std::array<Resistor, 32> resistors {};
    std::array<Capacitor, 12> capacitors {};
    std::array<double, maximumNodes> voltages {};
    Matrix inverse {};
    bool dirty = true;
};

// Selected Model T reference: 56k slope, 270p/22n/22n, 1M treble,
// 1M bass rheostat, 25k mid divider, 1M output load. See CIRCUITS.md.
class ModelTToneStack {
public:
    ModelTToneStack();
    void prepare(double sampleRate);
    void setControls(double bass, double middle, double treble) noexcept;
    double tick(double input) noexcept;
private:
    PassiveNetwork network {6};
    int bassR = 0, midUpper = 0, midLower = 0, trebleUpper = 0, trebleLower = 0;
};

// First reference-backed subsystem. The two input triodes, coupling capacitors,
// mixer RC and third triode are represented; follower loading and the power
// amplifier are deliberately NOT claimed by this partial development model.
class ModelTPreampStudy {
public:
    void prepare(double sampleRate);
    void setControls(double normal, double bright, double bass, double mid, double treble) noexcept;
    double tick(double inputVolts) noexcept;
private:
    CommonCathode normalStage, brightStage, gainStage;
    RcHighPass normalCoupling, brightCoupling, brightMixer;
    ModelTToneStack tone;
    double normalVolume = .5, brightVolume = .5;
};
}
