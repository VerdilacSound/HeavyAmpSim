# HeavyAmpSim 0.1.0 alpha — Windows x64 prototype

This prerelease is an early routing/DSP prototype for doom, stoner and desert
rock guitar. It includes a shared drive/fuzz pedalboard, four parallel amp/cab
lanes with level/pan, and parallel stereo delay and FDN reverb returns.

Download `HeavyAmpSim-Windows-x64-Release-VST3.zip`. Extract it, copy the entire
`HeavyAmpSim.vst3` folder into `C:\Program Files\Common Files\VST3`, and rescan
your DAW. Preserve the complete bundle directory structure. Windows x64 is
required. Only the T lane has audible level initially; raise other lane levels
to blend. There is no installer or code signature.

The Windows workflow builds the VST3/standalone targets and runs the DSP and
processor tests before packaging. This is not a real DAW or listening test.
Public binaries are experimental and may have host compatibility issues.

Amp/pedal voicings are provisional and cabinet responses are synthetic.
Schematic-derived circuits, complete original amplifier controls, measured IRs,
psychoacoustic reverb validation, aliasing measurements and Windows DAW
qualification remain open. This release does not claim hardware equivalence
or finished mix-ready sound quality.

License: AGPL-3.0-only. The download includes license notices. Matching
`HeavyAmpSim-Source.zip`, `JUCE-8.0.6-Source.zip` and `SHA256SUMS.txt` are attached
to the release. See README and docs/DESIGN.md in the source for build steps,
architecture and the remaining work.
