#include "dsp/Circuit.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
constexpr double pi = 3.14159265358979323846;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

void rcAndResistors() {
    using N = heavy::circuit::PassiveNetwork;
    N divider(1); divider.resistor(N::input, 0, 10000.); divider.resistor(0, N::ground, 10000.);
    divider.prepare(48000.); divider.tick(1.);
    require(std::abs(divider.voltage(0) - .5) < 1.e-12, "Resistor divider must follow Ohm's law");
    heavy::circuit::RcHighPass hp; hp.prepare(48000., 1.e6, 22.e-9);
    double y = 0.; for (int i = 0; i < 48000; ++i) y = hp.tick(1.);
    require(std::abs(y) < 1.e-12, "Coupling capacitor must reject DC");
}

void toneAgainstSpice() {
    std::ifstream input(HEAVY_CIRCUIT_FIXTURE);
    require(input.good(), "ngspice fixture must be available");
    std::string line; std::getline(input, line); int points = 0;
    double worst = 0.;
    while (std::getline(input, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        double bass, mid, treble, hz, real, imag;
        require(static_cast<bool>(row >> bass >> mid >> treble >> hz >> real >> imag), "Malformed ngspice fixture");
        heavy::circuit::ModelTToneStack stack; stack.prepare(192000.); stack.setControls(bass, mid, treble);
        // Independent complex AC reference versus settled sine/cosine projections
        // of the discrete-time implementation, including output phase.
        const double rate = 192000.;
        const int settle = static_cast<int>(std::ceil(30. * rate / hz));
        const int frames = static_cast<int>(std::ceil(20. * rate / hz));
        for (int i = 0; i < settle; ++i) stack.tick(std::sin(2. * pi * hz * i / rate));
        double sinOut = 0., cosOut = 0.;
        for (int i = 0; i < frames; ++i) {
            const double phase = 2. * pi * hz * (i + settle) / rate;
            const double y = stack.tick(std::sin(phase));
            sinOut += y * std::sin(phase); cosOut += y * std::cos(phase);
        }
        const std::complex<double> measured(2. * sinOut / frames, 2. * cosOut / frames);
        const std::complex<double> reference(real, imag);
        const double error = std::abs(measured - reference) / std::max(.001, std::abs(reference));
        worst = std::max(worst, error);
        if (error >= .015) std::cerr << "Tone error " << hz << "Hz " << bass << ',' << mid << ',' << treble << ": " << error << '\n';
        require(error < .015, "Component tone network must match independent ngspice AC reference");
        require(std::abs(measured) <= 1.001, "Passive network must not add gain");
        ++points;
    }
    require(points == 20, "All selected circuit reference points must execute");
    std::cout << "PASS 20 ngspice AC points; maximum complex relative error " << worst << '\n';
}

void valve() {
    using namespace heavy::circuit;
    ValveParameters p;
    require(korenPlateCurrent(200., -1., p) > korenPlateCurrent(200., -3., p), "Increasing grid voltage must increase plate current");
    CommonCathode stage; stage.prepare(192000., p);
    std::ifstream fixture(HEAVY_VALVE_FIXTURE);
    require(fixture.good(), "Independent ngspice valve fixture must exist");
    std::string line; std::getline(fixture, line); std::getline(fixture, line);
    std::replace(line.begin(), line.end(), ',', ' '); std::istringstream row(line);
    double plateV, cathodeV, hz, real, imag;
    require(static_cast<bool>(row >> plateV >> cathodeV >> hz >> real >> imag), "Invalid valve fixture");
    require(std::abs(stage.biasPlate() - plateV) / plateV < .01, "Valve plate bias must agree with ngspice");
    require(std::abs(stage.biasCathode() - cathodeV) / cathodeV < .01, "Valve cathode bias must agree with ngspice");
    double sine = 0., cosine = 0.;
    for (int i = 0; i < 76800; ++i) {
        const double phase = 2. * pi * hz * i / 192000.;
        const double y = stage.tick(.001 * std::sin(phase));
        if (i >= 38400) { sine += y * std::sin(phase); cosine += y * std::cos(phase); }
    }
    const std::complex<double> measured(2. * sine / 38.4, 2. * cosine / 38.4);
    const std::complex<double> expected(real, imag);
    require(std::abs(measured - expected) / std::abs(expected) < .01, "Valve small-signal complex gain must agree with ngspice");
    std::cout << "PASS valve bias and small-signal gain against independent ngspice\n";
    stage.prepare(192000., p);
    const double idleCurrent = (p.supply - stage.biasPlate()) / p.plateResistance;
    require(std::abs(idleCurrent - stage.biasCathode() / p.cathodeResistance) < 1.e-10, "DC bias must satisfy resistor load and cathode KCL");
    require(stage.biasPlate() > 50. && stage.biasPlate() < 300., "Valve must bias inside supply rails");
    for (int i = 0; i < 10000; ++i) require(std::abs(stage.tick(0.)) < 1.e-7, "Initialized valve must not generate a startup DC transient");
    const double small = stage.tick(.001);
    require(small < 0. && std::abs(small) > .01, "Common-cathode stage must invert and amplify");
    const double initialBias = stage.cathodeVoltage();
    for (int i = 0; i < 60000; ++i) {
        const double y = stage.tick(3. * std::sin(i * .031));
        require(std::isfinite(y) && y >= -stage.biasPlate() && y <= p.supply - stage.biasPlate(), "Plate must remain within physical supply rails");
    }
    require(stage.cathodeVoltage() > initialBias + .01, "Cathode capacitor must exhibit signal-dependent bias shift");
    for (int i = 0; i < 600000; ++i) stage.tick(0.);
    require(std::abs(stage.cathodeVoltage() - stage.biasCathode()) < .001, "Cathode bias must recover after an overload");
    std::cout << "PASS nonlinear valve load line, DC bias, inversion, overload and recovery\n";
}
}

int main() {
    try { rcAndResistors(); toneAgainstSpice(); valve(); }
    catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
