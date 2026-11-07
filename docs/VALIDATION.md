# Development validation — 2026-11-07

Validated in the attached Linux x86_64 cloud machine using GCC 14.2, CMake
3.31.6, Ninja 1.11.1 and JUCE 8.0.6 at
`51a8a6d7aeae7326956d747737ccf1575e61e209`.

## Passed

- Release builds of `HeavyAmpSim_VST3` and `HeavyAmpSim_Standalone`.
- JUCE VST3 helper generated the bundle's `moduleinfo.json` successfully.
- CTest: `dsp` and `plugin`, 2/2 suites passed.
- Separate Debug core build with AddressSanitizer and UBSan: 1/1 suite passed.
- Core test groups: oversampling FIR gain/delay, four-lane isolation/muting/pan,
  all three stereo delay modes, stereo reverb response/decay, zero heap
  allocations in processing, block invariance, invalid input handling, rapid
  automation/cab changes and finite bounded output at six sample rates.
- Processor checks: all controls exposed and recalled, mono and stereo input
  layouts, real signal processing, 0/1/17/511-sample blocks, sample-rate
  reinitialization, reported 16-sample latency and delayed bypass impulse.

Build artifacts are in `build/HeavyAmpSim_artefacts/Release/`. They are ignored
development outputs, not published release packages. Linux artifacts cannot be
installed as Windows plugins.

## Reproduce in this cloud machine

The machine lacks system GUI development headers and an elevated installer.
Signed Debian package indexes were verified with the Debian archive keyring,
and development packages were downloaded and extracted under
`/workspace/system-deps/root`; no TLS or package-signature checks were disabled.
JUCE is checked out under `/workspace/dependencies/JUCE`. Python tooling lives
in `/workspace/tools-venv`, outside the source repository.

```sh
export CPLUS_INCLUDE_PATH=/workspace/system-deps/root/usr/include:/workspace/system-deps/root/usr/include/freetype2
export LIBRARY_PATH=/workspace/system-deps/root/usr/lib/x86_64-linux-gnu
export PKG_CONFIG_SYSROOT_DIR=/workspace/system-deps/root
export PKG_CONFIG_LIBDIR=/workspace/system-deps/root/usr/lib/x86_64-linux-gnu/pkgconfig:/workspace/system-deps/root/usr/share/pkgconfig
/workspace/tools-venv/bin/cmake -S /workspace/HeavyAmpSim -B /workspace/HeavyAmpSim/build \
  -G Ninja -DCMAKE_MAKE_PROGRAM=/workspace/tools-venv/bin/ninja \
  -DJUCE_SOURCE_DIR=/workspace/dependencies/JUCE -DCMAKE_BUILD_TYPE=Release
/workspace/tools-venv/bin/cmake --build /workspace/HeavyAmpSim/build --parallel 4
/workspace/tools-venv/bin/ctest --test-dir /workspace/HeavyAmpSim/build --output-on-failure
```

For other machines use the Windows instructions in README or install JUCE's
normal platform development prerequisites. The workspace-local extracted SDK
is a workaround for this machine, not a distributable toolchain or a saved cloud
environment configuration change.

## Not verified

Windows compilation or Windows DAW loading; interactive GUI layout/accessibility;
standalone audio-device playback; a full external VST3 validator; real guitar DI
auditions; amp-to-hardware comparisons; cabinet IR provenance; psychoacoustic
reverb criteria; alias/true-peak/IMD measurements; minimum-machine CPU budgets.

The initial state-recall test supplied fractional values to boolean host controls;
JUCE stores their effective discrete state. The test now uses valid snapped
control values and checks every control. No assertion was removed or disabled.

The schematic archive probe returned proxy CONNECT 403. No reference schematic
has been obtained or matched. No GitHub workflow was dispatched, no changes
were pushed, and no environment configuration was modified or published.

## Earlier cloud Windows build attempt

The user subsequently requested a downloadable Windows binary. A Windows
binary has **not** been produced yet.

- Confirmed JUCE 8.0.6 explicitly rejects MinGW in `juce_TargetPlatform.h`.
  The downloaded MinGW toolchain was not used to create a plugin, and the
  framework's compiler check was not patched out.
- Prepared Clang 19.1.7 with the clang-cl driver and LLD 19.1.7 locally, plus
  native JUCE tools installed under `/workspace/juce-sdk` for a future supported
  MSVC-ABI cross-build. No cross-build has been validated yet.
- Downloaded xwin 0.10.0 from its official GitHub release, checked its published
  SHA-256, and attempted to obtain Microsoft CRT / SDK files with it.
- Microsoft SDK retrieval failed because the redirect from
  `https://aka.ms/vs/17/release/channel` to
  `download.visualstudio.microsoft.com` is denied by the proxy (HTTP 403).
- Saved only that domain addition to the environment configuration draft,
  preserving the existing package-manager preset and repository settings.
  Saving the draft did not apply it to the running instance or publish it;
  current observed custom allowed hosts were still empty at the last check.
- Added `scripts/build-windows.ps1` and connected CI to it. The script builds,
  runs tests, checks the Windows x64 PE header and packages the complete VST3
  bundle. It remains unexecuted on Windows.
- Existing Linux DSP/processor tests remain passing (2/2 suites).

To continue the cloud Windows build, review and save the domain change in
environment settings, then recheck runtime status and the SDK request. Further
compiler/SDK compatibility issues, if any, must be resolved before offering a
Windows binary. The source build kit is not a compiled VST3.

## GitHub Windows prerelease — completed

The user subsequently authorized uploading the code and publishing a Windows
download, and selected AGPLv3 licensing. The source and tag are now on GitHub:

- Source commit: `ecf8697c69dec74305df8898900aed0561cc5242`.
- Tag: `v0.1.0-alpha.1`.
- Main Windows build: https://github.com/VerdilacSound/HeavyAmpSim/actions/runs/42354465719
  — success, 3m 44s.
- Tagged Windows build and release: https://github.com/VerdilacSound/HeavyAmpSim/actions/runs/42354503644
  — success, 4m 8s.
- Release: https://github.com/VerdilacSound/HeavyAmpSim/releases/tag/v0.1.0-alpha.1

GitHub's Windows runner compiled the VST3/standalone targets, ran both test
suites, packaged the VST3 and published the prerelease with corresponding
application/JUCE sources and checksums. The earlier cloud SDK network blocker
did not affect this native GitHub runner.

The published binary ZIP was downloaded again and independently checked:

- ZIP CRC integrity passes; archive size 2,864,524 bytes.
- SHA-256 matches published SHA256SUMS.txt:
  `b70656a6264227dc7cb28ae1e71ce57c2231cc5cab08af36532d158f0ebed966`.
- `HeavyAmpSim.vst3/Contents/x86_64-win/HeavyAmpSim.vst3` has the PE signature
  and x86_64 machine code (`0x8664`), and the bundle contains VST3 metadata.
- Imports include MSVCP140, VCRUNTIME140 and VCRUNTIME140_1, so the README links
  the Microsoft Visual C++ x64 runtime needed for installation.

Real Windows DAW playback, external validator runs, visual UI review and tonal
fidelity/listening checks remain unverified. The release is an unsigned alpha
prototype, with the limitations stated in its release notes.
