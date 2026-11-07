# Circuit modeling rebuild

The requirement is **musical DSP derived from the selected schematics and
component values**. Matching a particular physical specimen, component
tolerance, transformer or microphone recording perfectly is not required.
The topology and component interactions are the basis of each model; arbitrary
EQ/saturation differences are not a substitute for the requested circuits.

## Current state

The new code in `src/dsp/Circuit.*` is an independently testable circuit
subsystem. It is **not connected to the plugin audio callback yet**. The
released alpha.1 and the plugin's current amp/pedal/cab algorithms still use
the earlier prototype. GUI changes and solo/meters do not fix their sound.
No new circuit-modeling binary has been released.

The work now implemented is:

- A bounded-storage passive nodal solver, with resistor conductances and
  trapezoidal capacitor companions. Capacitor history survives resistor/pot
  changes. No audio-time allocation is performed by the solver.
- A Koren-law resistor-loaded valve stage, DC self-bias solution, cathode
  resistor/bypass-capacitor memory, and approximate grid conduction through
  a grid stopper. The plate load line is tabulated during preparation.
- RC coupling networks with R and C expressed in actual electrical units.
- The six-node, three-capacitor Model T tone network described below.
- A **partial preamp study** using the normal/bright input stages, their 22 nF
  coupling capacitors, the 470 kOhm/470 pF mixer and following gain stage.
  The cathode follower, interstage loading and power amp are not completed;
  this study is not labeled a full Model T recreation.

## Selected Model T reference

Public circuit adaptation by MARKTHERENCE:

https://github.com/MARKTHERENCE/LiveSPICE-MK-Circuits/blob/4f3f8836a797e113055d7179fb8349f85e54f077/amps/Sunn%20Model%20T%20-%20Both%20(Preamp%20only).schx

Reference repository commit:
`4f3f8836a797e113055d7179fb8349f85e54f077`.

Downloaded file SHA-256:
`5cc91b260677e3045b39b14796937d713dfd3aa6379093279c57dd745bfc5eb2`.

This is a secondary circuit transcription, not an authenticated manufacturer
scan. Its component values and connections have been inspected; agreement
with a selected primary first-generation schematic remains to be checked.
No C++ implementation was copied from that project. The repository carries
GPLv3; circuit provenance is retained here and its source remains accessible.

| Reference section | Values used |
|---|---|
| Input triodes | B+ 330 V; plate 100 kOhm; cathode 680 Ohm / 250 uF; grid stopper 68 kOhm |
| Channel coupling | 22 nF into nominal 1 MOhm volume/load, one per input branch |
| Channel mixing | Two 470 kOhm mixer arms; 470 pF bright bypass |
| Following gain stage | Plate 100 kOhm; cathode 820 Ohm / 22 uF; nominal 470 kOhm source arm |
| Tone network | 56 kOhm slope; 270 pF treble cap; two 22 nF caps |
| Tone pots/load | Bass 1 MOhm rheostat; mid 25 kOhm divider; treble 1 MOhm divider; load 1 MOhm |

The tone solver preserves the mid-pot wiper connection and bass rheostat;
it does not replace this network with independent boost/cut EQ bands.
Endpoints use a 1 Ohm numerical floor on pot segments to avoid ideal shorts.

## Explicit primitive-model approximations

The input triodes in the transcription select a Dempwolf model, whereas this
first primitive uses a Koren model for the valve. The Koren parameters in the
reference are mu=83.5, Kg=1060, exponent=1.4, Kp=600, Kvb=300. Using a different
reasonable tube law is an engineering approximation, not a claim of matching
the transcription's complete nonlinear simulation.

The one-dimensional anode-current table omits subtracting cathode voltage from
B+ in its resistor load line. The independently simulated bias and 1 kHz gain
are currently within 1% for the tested input stage. That bound does not apply
to every overdrive condition or tube configuration.

Cathode-capacitor integration uses backward Euler. Grid conduction currently
uses a Shockley approximation with Is=2 nA and nVt=0.2 V; those values are
modeling assumptions, not measured 12AX7 junction data. Miller capacitance,
stage-to-stage loading, grid-charge effects in coupling capacitors and the
follower require further work before calling the whole preamp implemented.

## Independent validation

`tools/circuit_reference.py` generates committed reference fixtures using
ngspice 44.2. It builds a separate SPICE netlist and does not read/evaluate
the C++ solver. ngspice is needed only to regenerate references, not for
ordinary builds or for the real-time plugin.

The passive network is checked at 80, 250, 1000, 3500 and 7000 Hz for four
control settings. At the 192 kHz component-model rate, the worst measured
complex-response relative error is 0.003639 (0.364%). The comparison includes
phase as well as magnitude; digital frequency warping is retained.

The valve's DC plate/cathode voltages and small-signal complex gain are checked
against a separate ngspice nonlinear valve circuit. Tests also check KCL at
bias, inversion, physical rail bounds, cathode bias shift and recovery.
These tests validate individual subsystems, not the finished amp's musicality.

## Remaining amp models and sound work

- Model T: complete follower/loading, channel controls, phase inverter,
  6550 output section, negative feedback/presence and supply/load behavior.
- GT120: public LiveSPICE circuit files were located for its preamp, switching
  and power section. Extract/verify their component map before implementing
  the distinct network and controls.
- SLO100: a primary SLO circuit reference is still needed; generic “SLO-style”
  code and other Soldano models are not accepted as substitutes.
- Early V4: secondary VT40/V4 preamp references were located, but the primary
  schematic and its active/selectable mid network still need verification.
- Pedals: build the revision-specific transistor/diode/op-amp circuits and
  passive tone networks; replacing gain constants is insufficient.
- Cabs: implement an actual IR workflow and licensed/provenanced cabinet
  responses, with phase/timing control for parallel blending.
- Evaluate pickup-level calibration, dynamics, aliasing and level-matched
  listening comparisons after integration. A white-box circuit model alone
  does not establish a good-sounding finished instrument.

The primary archive requests to `el34world.com` and
`www.prowessamplifiers.com` returned HTTP 403 from this environment's proxy.
Their domain additions are saved in the environment draft, awaiting application
through environment settings. Existing network settings were preserved.

## GUI development

The editor now shows all four amp/cab lanes together, with a shared pedalboard,
stereo returns, individual lane meters, solo/clear-solo, level/pan mixer, global
meters and rotary controls. Numeric entry, host attachments and original
parameter IDs are preserved. State schema 2 supplies defaults for new controls
when loading schema-1 sessions.

`heavy_ui_snapshot` renders the actual native JUCE editor for review. It is
not a web mockup or an image-generation design. The interface marks the circuit
rebuild and synthetic cabs as development work until those engines are replaced.
