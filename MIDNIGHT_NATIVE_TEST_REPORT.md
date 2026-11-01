# Midnight Native POC - actual test report

Date: 2 November 2026 (Europe/Brussels).

**Overall acceptance: NOT PASSED.** Native compilation is blocked. No custom JUCE executable
was built or launched, and the mandatory native project restart/persistence test was not run.
The Python simulated receiver and build-preparation code have successful independent tests.

## Recorded operations

| Operation | Result | Evidence |
|---|---|---|
| Official source and recursive submodules | Success; application and submodules pinned | Midnight/dependencies.json |
| Unmodified baseline via official PowerShell helper | Blocked by existing script execution policy; policy unchanged | Midnight/evidence/baseline-build.txt |
| Unmodified baseline via official build_dev.bat | Failed before compilation: MSBuild.exe not found | Midnight/evidence/baseline-native-build.txt |
| Experiment build preflight | Exit 2: VS2022 C++, Windows SDK10 and modified JUCE absent | Midnight/evidence/build-preflight.json |
| Native launcher without executable | Exit 2 with build instructions; no helper or GUI started | Midnight/evidence/native-launcher.txt |
| OSC codec, simulated motors, local UDP, build preparation | **15 tests passed**, 0.051 seconds in the recorded run | Midnight/evidence/python-tests.txt |
| Diagnostic sender to Python receiver | **102 commands sent, 102 acknowledged** | JSONL and loopback-result.json |
| Native C++ compile/link/runtime | Unverified; no executable or native runtime evidence exists | Build blocked |
| V2 preservation | Clean at 7eb07769f6a1f199526c2324f4c9108d99d2ed7b | Git status/HEAD verification |
| Physical motor/CAN testing | Not performed or permitted | No CAN/serial code imported or called |

The 15 tests cover canonical OSC bytes, strings/int32/float32, Unicode, immediate bundles,
rejection of future-timetag bundles, malformed packets and non-finite values; simulated
speed/arrival/no overshoot, Stop followed by explicit movement, independent motors and invalid
commands; actual loopback acknowledgement/feedback/error recovery; and generated exporter
path/output isolation. They do not substitute mocks for native engine execution.

The first golden-byte test had one extra padding byte in the manually written expected
fixture. It was corrected to OSC's four-byte alignment and the full suite rerun.
An initial Python build-script string-literal error was also corrected before preflight tests.
These were software-development failures, not Chataigne runtime or engine failures.

## OSC evidence and timing

The sender was **Midnight/test_receiver_loopback.py**, not Chataigne.
It sent one RUNNING notification, 100 target/speed commands, and one stop.
Real UDP sockets were bound to 127.0.0.1 on ephemeral ports. Logs contain monotonic
timestamps, packet bytes, accepted address/arguments and returned messages.

- [Transmitted test packets](Midnight/evidence/transmitter-loopback.jsonl)
- [Received packets and returned OSC](Midnight/evidence/receiver-loopback.jsonl)
- [Measured result](Midnight/evidence/loopback-result.json)

Round-trip acknowledgement timings from this short local run:

| Minimum | Median | 95th percentile | Maximum |
|---:|---:|---:|---:|
| 0.1646 ms | 0.1708 ms | 0.1989 ms | 0.5984 ms |

These are Python-to-Python loopback measurements. They establish neither native sequence
scheduling accuracy nor CAN latency, haptic-loop timing, real-time guarantees or physical motion.
No Chataigne TX log or Chataigne-origin packet has been recorded.

## Mandatory project persistence test

**Not run.** No .noisette fixture is presented as a successfully saved project.
The C++ code constructs native objects and calls the existing document APIs. That is a
persistence strategy, not evidence of a successful restart.

After a native build, follow README-MIDNIGHT-NATIVE-POC.md: save an edited demo, quit the
entire process, restart, reopen, inspect states/transitions/keys, replay through conditions
and compare actual OSC logs. Same-process JSON loading would not satisfy this requirement.

## Final acceptance matrix

| Criterion | Current result |
|---|---|
| 1. Custom Midnight C++/JUCE interface launches | Not achieved: no native build |
| 2. Operates actual State Machine | Native API implementation present; runtime unverified |
| 3. Operates actual Time Machine | Native API implementation present; runtime unverified |
| 4. Creates/modifies engine objects | C++ creation/editing source present; runtime unverified |
| 5. State transition triggers sequence | Standard conditions/actions wired in source; runtime unverified |
| 6. Sequence executes automation and OSC | Native layers/commands authored in source; runtime unverified |
| 7. Receiver receives Chataigne messages | Not achieved: only Python-origin test packets verified |
| 8. Save/reopen preserves configuration | Not tested; mandatory acceptance outstanding |
| 9. No replacement of execution engine | Source diff confirms engine/submodule implementations unchanged |
| 10. Plausible full-interface foundation | Source-level feasibility supported; runtime proof incomplete |

## Remaining risks

The C++ additions may contain compile or runtime integration errors; there has been no
compiler/linker check. First-run checks must include comparator initialization, activation
ordering, command target resolution after reload, trigger-at-zero behavior, end/seek semantics,
editor lifetime during project replacement, feedback binding and runtime DLL loading.

The panel finds demo objects by name and has a simple fixed control layout. It is not a
production Motor Manager. Continuous automation and discrete motor instructions are separate
examples. No physical bridge, haptics, linking, custom cue scheduler or physical trajectory
execution is implemented. V2 remains the verified motor-control implementation.
