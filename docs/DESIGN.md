# Product target and engineering plan

Updated requirement: the selected schematics and their components define the
models. Musical, responsive behavior is the goal; matching a particular physical
amp perfectly is not required. Validate electrical subsystem behavior against
the reference circuits and SPICE, then evaluate listening and playability.
Hardware A/B comparisons are optional refinement, not a prerequisite to this
approach. Current implementation status is tracked in [CIRCUITS.md](CIRCUITS.md).

## Intended sound

Four independently voiced power amps and cabinets should function like a single
playable guitar instrument: large low mids, clear low-tuned attacks, controlled
fizz, and stereo ambience that does not erase the center. A mix-ready preset
must be judged against bass and drums at matched loudness, not just sound large
in isolation. No guitar processor can guarantee that every blend fits every mix.

Windows x64 VST3 is the first release target. The Linux builds used during
development establish portable compilation and DSP behavior, not Windows host
compatibility. No environment publication or GitHub push is part of building.

## Hardware references and provenance

The user accepts readily accessible public schematics as the modeling authority.
Before implementing a circuit, record the actual document URL, revision/date,
page, file SHA-256, component values, switch wiring and any uncertain annotation.
Keep a local unmodified reference for engineering use where permitted. A product
name alone does not resolve revision differences. Do not label the current
voicing code a schematic-derived model.

The initial attempt to reach `https://www.prowessamplifiers.com/schematics/`
was denied by the cloud proxy (HTTP CONNECT 403 on 2026-11-07). No schematic
was retrieved or verified. Archive access or supplied reference files are still
needed; network-denied pages are not evidence about a circuit's contents.

Reference targets, to be confirmed against the selected documents:

| Family | Circuit/control questions to resolve before matching |
|---|---|
| First-generation Sunn Model T | Two input channel volumes (normal/bright), input linking/loading, shared bass/middle/treble and presence, exact first-generation preamp/phase inverter/power supply; no later reissue substitution |
| MatAmp GT120 | Exact vintage revision, multi-position bass/voice switching, bass/treble/presence, drive/volume and any boost wiring; do not use a generic Marshall mid control as an authenticity claim |
| Soldano SLO100 | Original normal/overdrive preamp and master levels, shared EQ/presence, normal bright and clean/crunch switches, channel switching; do not silently add later depth modifications |
| Early-1970s Ampeg V4 | Dual-input channel volumes/loading, common EQ, selectable mid frequencies, sensitivity/voicing switches, original output section and speaker load; confirm year and exclude later master-volume revisions |
| Centaur drive | Input buffer, charge-pump/headroom, gain-dependent clean/drive mixing, clipping network, tone and output controls |
| Muff variants | Explicit Triangle, Ram's Head, Russian and op-amp schematic selections; transistor/op-amp topologies are not interchangeable gain presets |

The generic amp gain/bass/mid/treble/presence controls in 0.1 intentionally
serve only as a development surface. They are **not** the requested full original
control sets. Add the confirmed controls with stable new host IDs and schema
migration; never reuse an existing ID for a different physical function.

## Circuit modeling approach

1. Build linear component networks from MNA/state-space models. Verify tone stacks
   and switch positions with offline SPICE sweeps before inserting nonlinearity.
2. Use wave-digital or implicit nonlinear solvers for interacting diode/tube
   stages, with bounded iterations and a documented fallback. Include interstage
   loading rather than a chain of independent memoryless waveshapers.
3. Account for grid conduction, coupling-cap bias shift, cathode bypass dynamics,
   phase-inverter imbalance, negative feedback/presence, output transformer/load
   interaction and power-supply sag as indicated by the selected circuits.
4. Treat the amplifier speaker load and the audible cabinet IR as different
   functions. An IR alone does not reproduce power-amp/speaker interaction.
5. Calibrate instrument volts to dBFS. Compare clean-to-clipped transfer curves,
   spectral envelopes, dynamic recovery and knob sweeps at several input levels
   against the circuit simulation; refine musical response through listening.

Use independent states and the same oversampling mode/latency on all four lanes.
For production, evaluate longer polyphase filters or a suitable IIR oversampler,
then measure aliasing rather than choosing quality by the oversampling number.
Keep all allocations, IR loading, transforms, file I/O and solver construction
off the audio thread. Publish prepared immutable IR/model data via a safe swap.
Crossfade switched topologies and IR changes. Host state includes all hidden
model revision and quality settings as well as the visible knobs.

## Cabinet engine

Use partitioned FFT convolution with rate-correct IR resampling, denormal
suppression, fixed reported latency and background preparation. Curate several
4x12 types plus 8x10 and 2x15 bass cabs with appropriate usage rights. Store mic
position and phase/timing metadata. Preserve natural phase by default; offer
explicit time/polarity alignment across lanes instead of silently changing it.
Start with one mic per lane, then evaluate dual-mic blends against CPU budgets.
The current short synthetic FIRs exist to exercise the routing until that system
is implemented; they must not be marketed as those captured cabs.

## Ambience

Delay must retain two input channels and two sets of state in every mode. Model
single-head tape behavior, a verified Echorec head-selection matrix, and a cleaner
studio transport with controllable wow/flutter, saturation and bandwidth. Add
tempo sync after free-time mode is host-tested. Bound loop gain across every
head pattern so maximum feedback decays without unintended self-oscillation.

Reverb needs distinct left/right early-reflection and late-field injection paths,
a diffuse stable late field, frequency-dependent decay, controllable predelay,
width, low cut and modulation. Assess impulse responses, energy-decay curves,
interaural correlation, modal coloration, mono fold-down and perceptual listening.
An orthogonal FDN is a starting point, not proof of psychoacoustic accuracy.

Both effects receive the same **post-pan stereo dry mix**, remain wet-only
internally, and return in parallel. A future conventional 0..100% crossfade option
must be labeled separately from the current additive return controls.

## Release gates

- Circuit provenance and original controls confirmed for every model/revision.
- Independent reference renders/SPICE comparisons at clean, edge and saturated
  levels; gain, tone and transient-response tolerances agreed before measurement.
- Licensed/measured IRs, correct resampling at 44.1/48/88.2/96/176.4/192 kHz,
  no lane latency mismatch, predictable mono compatibility.
- Aliasing, DC, noise floor, intermodulation and output true-peak measurements;
  long stress tests under automation including model switches and host resets.
- Listening sessions with real clean DI guitar files, several tunings/pickups,
  level-matched hardware/reference comparisons, and full bass/drum mixes.
- Worst-case CPU at a 64-sample block with all amps/effects on, evaluated on the
  chosen minimum Windows CPU; quality/performance mode only after measurement.
- Windows VST3 validator/pluginval and real DAW tests (for example REAPER and
  Cubase): scan, mono/stereo layouts, bypass, automation, presets, recall, offline
  export, variable blocks, sample-rate changes and multiple instances.
- Versioned state migration, factory presets, accessibility, installer/signing
  and third-party license review before distributing release binaries.

These gates remain open. Passing the current automated tests validates the
prototype's engineering foundation, not the final tonal fidelity or readiness
of the requested product.
