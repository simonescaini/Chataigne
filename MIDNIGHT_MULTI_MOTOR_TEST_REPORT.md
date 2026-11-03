# Midnight Native V0.1 multi-motor implementation

3 November 2026. Continued `feature/midnight-native-v0.1` from
`f4a52852dd766fdb107c25c9314f5fd25dae7f88`, after fetching origin and verifying
matching local/remote heads. Initial unrelated material: untracked `project_file/`.
It was not opened for acceptance, modified, staged or stashed. No reference workspace,
submodule, upstream branch, UI experiment branch or dependency version was changed.

The integrated Windows application supports RS04 and a named MyActuator L-7025
profile alongside existing RS01/RS02. All acceptance below is software execution
using fake CAN and isolated settings. **Physical acceptance: none.** Desktop file
dialogs remain manually unverified; the stopped Computer Use session was not resumed.
Native document API and actual editor-component checks are separate evidence.

## Sources and selected profiles

Downloaded manuals are retained under ignored `Midnight/runtime/protocol-sources/`.
Hashes and section/page references below identify the evidence without distributing
manufacturer PDFs in Git. Selected profile identity is saved independently of motor
name and device ID. Installed firmware has not been queried or verified.

| Source | Revision and identity | Use |
|---|---|---|
| [RobStride 04 User Manual, CAN2.0/private protocol](https://github.com/RobStride/Product_Information/blob/b05095a98d5a586092d4011c3dbb6b7fb719a63b/Product%20Literature/CAN2.0/RS04/RS04User%20Manual260713.pdf) | Filename 260713 (13 July 2026); repository `b05095a98d5a586092d4011c3dbb6b7fb719a63b` (30 October 2026); Git blob `bc92a2ef968b7940fd924326106e997c3f00e12a`; SHA-256 `6782bf105cf4cc945142e65356bd7a21a9fc007b008ec3616307956030d72b31` | PDF pp44–46: operation, feedback, stop and zero; pp49/54: active reporting; pp51–52: parameters; p55: firmware-dependent zero behavior. Relevant table pages rendered and visually read. |
| [Manufacturer Python scales](https://github.com/RobStride/Python_Sample/blob/cbf977e56c842d57a65f3f17c1b1ecaef002c424/robstride_dynamics/table.py), [protocol](https://github.com/RobStride/Python_Sample/blob/cbf977e56c842d57a65f3f17c1b1ecaef002c424/robstride_dynamics/protocol.py), [bus implementation](https://github.com/RobStride/Python_Sample/blob/cbf977e56c842d57a65f3f17c1b1ecaef002c424/robstride_dynamics/bus.py) | Commit `cbf977e56c842d57a65f3f17c1b1ecaef002c424`, commit dated 4 June 2026. Blobs respectively `e2091338189d653367d3bf7e797d6a4879c927b8`, `706d8aee7c2c7b573c7d241e7e40befec4455f73`, `13e56da5af5258f082b702337fdbbc3e1726e2f3` | Model scales and parameter/type cross-check, with numeric discrepancy described below. |
| [MyActuator L-series downloads](https://www.myactuator.com/downloads-lseries), [official archive](https://static.wixstatic.com/archives/cab28a_8136b0f46af3455ea383921d5aa02ce5.zip) | `(L)Protocol and manual of V3 -251029` package, retrieved 3 November 2026 | Establishes manufacturer association of the profile with L-series. Does not identify the user's installed controller. |
| Servo Motor Control Protocol / Motor Motion Protocol V4.2-250208.pdf, in that archive | Applicable driver V3; cover May 2024; revision history 28 May 2024; filename 8 February 2025. SHA-256 `50371859b2d026ba3929f82afbf81ab841de676af422f22c8375eb359267d49b` | §1.1 standard IDs/DLC/bitrate; §2.10 ROM zero and restart; §2.12 multi-turn angle; §2.14–15 status; §2.17–18 shutdown/stop; §2.21 A4. PDF pp33,37,57 tables visually inspected, adjacent descriptions extracted. |
| L Series Product Manual 251029.pdf, same archive | 29 October 2025 filename; SHA-256 `f49e31f2086bfd002807321e2aa9adde8f4bba634909164ba221f8aaeb669ee7` | L-7025 direct-drive CAN/RS485 variants. No motor-shaft/output-shaft ratio is introduced into already output-shaft angular commands. |
| MC Series Brushless Servo Driver Manual-240611.pdf, same archive | 11 June 2024 filename; SHA-256 `eb5cd58cfac42877a00867c461afd5e0e2cf592a5f6dbeca141af29a6a40ab16` | Driver-family context; not evidence of installed firmware. |
| [MyActuator Chinese software/protocol downloads](https://www.myactuator.cn/downlaodrj) | Accessed 3 November 2026; lists multiple families/revisions, including V4.3. Linked `/txxy` endpoint timed out. | Corroborates that multiple profiles exist; no automatic selection of newest protocol. |
| [Original controller](https://github.com/simonescaini/Midnight_orchestrator/blob/b5418747bb4ce584c22a57baba38320955a9a7f4/Reference_controller/main_canable_dual_haptic_working.py) | Commit `b5418747bb4ce584c22a57baba38320955a9a7f4`; SHA-256 `0dfe47ce2d90337498e207ee72c7ee88f029ddc9f2097cfe2150cc03232a73a9` | Read-only LegacyMotor, linking and spring/damper reference. Differential test evaluates only isolated pure/class code on a fake bus. |
| [V2 protocol report and tests](https://github.com/simonescaini/Midnight_orchestrator/tree/7eb07769f6a1f199526c2324f4c9108d99d2ed7b) | Local reference HEAD `7eb07769f6a1f199526c2324f4c9108d99d2ed7b`, clean | Read-only protocol report, model vectors, control/transport tests, following/haptic law. No reference controller or physical test launched. |

Saved profiles: `robstride-private-v01` for RS01/RS02 (preserved behavior),
`robstride-private-rs04-260713` for RS04, `myactuator-v3-v4.2` for RMD-L-7025.
Missing profile/direction in old version-1 documents resolves to the original model
profile and +1; existing names, stable IDs, addresses, ranges and event references remain.
Unknown profiles fail validation instead of silently selecting another protocol.

## Evidence decisions and discrepancies

RS04 Type-1 and Type-2 use ±4π rad position, ±15 rad/s velocity, ±120 Nm torque,
0..5000 Kp and 0..100 Kd. CSP parameters instead permit 20 rad/s, 90 A and 120 Nm.
Electrical/mechanical ratings do not determine packet scaling or user settings.
The Python SDK agrees on scales, but its bus uses `0x7FFF` for signed-range mapping,
where the manual tables specify 0..65535 across both endpoints. This implementation
uses that full range, nearest representable value and existing round-to-even behavior;
it does not copy the SDK's one-count asymmetry. Literal zero, positive/negative torque,
gain, feedback and existing reference parity tests cover the choice.

The RS04 Type-24 table prints seven payload bytes in an eight-byte field, and its
response table labels Type 24 while the function description says Type 2. The legacy
controller sends a different payload. **RS04 active-report enable is not implemented**;
Type-1 command responses supply Type-2 samples, with existing mechanical/velocity
polling fallback. Resolving this optional capability requires a complete eight-byte
manufacturer example for the installed firmware. Existing RS01/RS02 reporting
behavior remains unchanged and still needs firmware-specific physical validation.

RS04 zero is Type 6 with byte 0 = 1 after ordinary stop. The manual distinguishes
older/newer firmware target behavior during zero and disallows PP zero. There is no
automatic Type-22 save, zero-state write or persistence claim. Operator verification
of the installed firmware and persistence remains necessary.

For MyActuator, `0xA4` is signed LE int32 centidegrees with LE uint16 whole output-shaft
degrees/s. `0x92` returns signed LE int32 centidegrees in bytes 4..7. It is multi-turn,
not modulo 360. Its signed representable limits are not extended by invented unwrapping.
Position is not reconstructed from `0x60` counts: LegacyMotor explicitly assumes CPR
65536. Neither that assumption nor its immediate local-zero reset is copied. The
coarse int16 whole-degree angle in A4/9C replies is not used for event completion;
signed speed is output-shaft degrees/s, converted internally to radians/s for existing
feedback math. No gearing multiplier is applied to either angular command or reply.

The manufacturer's L-series package supports choosing V3/V4.2, but does not prove the
user's L-7025 is that controller. Older same-ID reply or seven-byte angle profiles are
not accepted as this profile. Minimum applicability evidence is controller/firmware
identity or an operator capture of addressed `0x92`/status replies against this manual.
No firmware, CAN protocol, device ID, driver parameters or bitrate is rewritten to fit.

MyActuator zero `0x64` writes the current encoder position to ROM and requires controller
restart (`0x76` in the manual) to become effective. The UI exposes the ROM write with
that explanation, but does not issue restart or reset the displayed angle. The operator
can separately power-cycle/restart under their normal procedure. The old script's
persistence comment is not treated as hardware acceptance.

`0x80` turns output off and clears the control state; `0x81` stops speed while retaining
closed-loop state. The latter has a separate direct control, ending a move's ownership;
continuous interactions retain their existing whole-group disable semantics. The
selected document does not define a separate `0x88` Run command. Move directly sends
A4, including after Stop/Disable. Separate Enable is unavailable with a visible reason.
A4 speed 0 has the documented meaning of no speed limit, explicitly shown in the UI;
fractional speed below the 1°/s field resolution is rejected rather than becoming zero.
Drive-side acceleration remains whatever the operator configured; no motion curve is
added or rewritten.

## Integrated behavior and capability table

| Model | Movement/Jog, speed | Measured position/status | Mechanical zero | Following | Haptics | Physical verification |
|---|---|---|---|---|---|---|
| RS01 | Implemented; software-tested; existing CSP bytes | Mechanical/Type-2 and faults, software-tested | Type 6, software-tested; persistence unknown | Master/multiple followers, software-tested | Two RobStride endpoints; software-tested | None |
| RS02 | Implemented; software-tested; existing CSP bytes | Mechanical/Type-2 and faults, software-tested | Type 6, software-tested; persistence unknown | Master/multiple followers, software-tested | Two RobStride endpoints; software-tested | None |
| RS04 | Implemented; software-tested; own parameter limits | Own Type-2 scales and faults, software-tested | Type 6, software-tested; installed firmware/persistence unknown | Master/multiple followers, software-tested | Own endpoint scales; all nine ordered RS pair combinations tested | None |
| RMD-L-7025 / V3–V4.2 | A4 Move/Jog with whole dps, software-tested | 92 precise multi-turn angle, 9C speed, 9A faults, software-tested | 64 ROM write, software-tested; separate restart required, physical effect/persistence unverified | Master/multiple followers, including mixed brands/directions, software-tested | Unsupported and locally explained | None |

The native Frame carries format and DLC through comparisons, queues, wire encoding,
receive routing, monitoring and the fake adapter. Standard `t` and extended `T` streams
can interleave; malformed frames are not padded into replies. A protocol driver boundary
selects addressing, commands and capabilities while preserving scheduling, event
ownership, arrival, conditions and timeline design. TX updates sent bookkeeping only;
fresh measured position is required for completion. Polling cannot wait for an absent
motor or require a RobStride register on MyActuator.

One adapter owns one connection and receiver; same numeric device IDs on the two
different frame formats are legal, duplicate routing within one format is rejected.
RobStride host/device equality is rejected because it makes read echoes ambiguous.
MyActuator does not validate or transmit a host ID. Bus settings belong to the shared
adapter; model selection never changes them, and explicit bus edits apply to all its motors. MyActuator's selected profile requires 1 Mbps and
reports an incompatible saved bus instead of changing it. No auto discovery of models
or drive addresses is claimed.

Explicit direction applies to targets, measured coordinates and haptic endpoint torque.
MIN/MAX are user target ranges in those coordinates, not torque-mode travel control.
Followers retain saved speed/current where supported; no maximum-current override from
the original link setup is copied. Haptic spring/damper, alignment, signs, damping within
deadband, staleness threshold and explicit stop semantics remain intact. MyActuator has
no exposed current-limit field or haptic torque control.

## Build, tests and timing

Uses the existing `Midnight/juce-path.txt` external JUCE checkout
(`C:\Users\Simone\Dev\JUCE`, the previously documented machine exception for unavailable
D:), revision `d8b1e81eca9bd52a2f0e0783f687f991fa084a7f`; no nested checkout/link created.
Pinned MSVC 14.44, SDK 10.0.26100.0, submodules and generated build strategy retained.
MSBuild initially failed with sandbox FileTracker E_ACCESSDENIED before compilation;
the authorized build outside that restriction succeeded. No CI dependency changes.

Reproduce with the Python executable recorded in ignored `Midnight/python-path.txt`:

```bat
Build-Midnight-Native-POC.cmd --check
Build-Midnight-Native-POC.cmd
python -m unittest discover -s Midnight/tests -v
python Midnight/test_v01.py --integration
python Midnight/test_multi_motor.py
python Midnight/test_native.py
python Midnight/compare_reference.py
```

Final results ([build/hash](Midnight/evidence/multi-motor-build.json),
[core log](Midnight/evidence/multi-motor-core.txt),
[V0.1 regression](Midnight/evidence/multi-motor-v01-acceptance.json),
[mixed worker/native reload](Midnight/evidence/multi-motor-integration.json),
[POC regression](Midnight/evidence/multi-motor-poc-acceptance.json),
[Python](Midnight/evidence/multi-motor-python.txt),
[reference comparison](Midnight/evidence/multi-motor-reference-parity.txt)):

| Suite | Passed |
|---|---|
| Production core (existing + new literal vectors/dispatch/streams) | 161 checks |
| Existing V0.1 application creation / fresh-process reload | 52 / 54 checks |
| New four-model application creation / fresh-process reload | 42 / 42 checks |
| Original native POC creation / fresh-process reload | 34 / 31 checks |
| Existing Python build/OSC/simulator suite | 17 tests |
| Pinned differential reference | Original six-packet CSP sequence after documented stop, plus 29 parameter/torque/spring-damper cases |

Coverage includes standard/extended/fragmented mixed streams, short and malformed
frames, request echoes, format-specific routing, literal signed MyActuator vectors,
model-specific RS04 nonzero feedback/torques/gains, faults, stop/disable/zero, all
nine ordered RobStride haptic model pairs and unsupported MyActuator haptics. Real
worker tests cover four interleaved models on one fake adapter, one absent motor,
actual A4 transmission bookkeeping, fresh arrival and a subsequent nested event,
measured-position conditions, both mixed-brand following directions with two followers,
direct Move/Jog editor callbacks, profile/coordinate persistence and observation-only
reconnect. Settings identities are dedicated test applications, with isolated process
environment/project directories. No saved physical project is loaded.

A test-harness compile initially needed its actual NativePanel constructor; corrected
before running acceptance. The POC linker also encountered a missing MainIncludes object
while a rebuild was replacing it; rerunning after the build completed passed. The final
software results above include no failing checks. Historical tracked examples/screenshots
regenerated by existing tests were restored; this assignment's evidence uses separate names.

Comparable workload: existing three-motor V0.1 scenario, measured 10-degree replacement,
linking/haptics, 30 render/serialization iterations and a 250 ms blocked GUI. Before is the
unmodified checkpoint run in this assignment; after uses the new production implementation.

| Measurement | Before | After |
|---|---:|---:|
| Worker intervals | 958 | 2318 |
| Mean / max interval, ms | 9.201 / 16.622 | 3.688 / 5.518 |
| Max excess above mean, ms | 7.421 | 1.830 |
| Measured crossing RX to replacement target TX, ms | 94.265 | 59.639 |
| Max enqueue to write, ms | 78.832 | 56.113 |
| Queue high-water, packets | 15 | 15 |
| Maximum motor feedback age at final snapshot, ms | 15.383 | 14.986 |

[Before JSON](Midnight/evidence/multi-motor-baseline-metrics.json),
[after JSON](Midnight/evidence/multi-motor-comparable-after-metrics.json).
These are single Windows simulation runs, not an attributable optimization benchmark:
OS timer resolution/load varied, and the fake RobStride adapter now also emits the
Type-2 replies to Type-1 commands that the manual specifies. The command gaps and
150 ms freshness threshold were not changed. Trigger latency includes retained CSP
setup waits; serial USB, actual CAN arbitration and drive/mechanism response are absent.

The separate [four-model measurement](Midnight/evidence/multi-motor-worker-metrics.json)
uses one RS04/RS01 haptic pair, all four model polls, 30 native component renders/serializations
and the same GUI block. It records 147 intervals, mean/max **3.768/5.503 ms**, queue high-water
**6**, max enqueue-to-write **0.107 ms**, maximum feedback age at snapshot **15.991 ms**,
and injected master-position to MyActuator target TX **20.930 ms** (includes the polling
interval). That latter stimulus-to-wire measure is distinct from the earlier measured-RX
crossing metric. The worker progressed during the blocked GUI in both suites. No timing
number is a guaranteed real-time bound.
The runners use isolated settings/project paths and fake adapters; they never launch
saved physical projects. The application launcher remains `Run-Midnight-Native-V01.cmd`,
targeting `Binaries/MidnightNative/MidnightNativePOC.exe`. No desktop launcher/dialog
automation was performed.

## Operator-only physical checks

1. Confirm actual model, controller/firmware profile, device ID, direction and bus bitrate
   using existing hardware records. Connect for observation; compare measured shaft
   angles, negative direction, multiple revolutions and speed with the mechanism.
2. Within the mechanism's existing limits, use a small direct Move/Jog at the user's chosen
   speed/current where supported. Check output-off, MyActuator retained-loop Stop, and a
   subsequent deliberate Move. TX alone does not establish acceptance or torque release.
3. Run a short movement event, measured-position condition and nested timeline; inspect
   normal completion, supersession and Stop independently. Reload should restore only
   observation, with stable event references and unchanged ranges.
4. Test one-way following in each brand direction, then one master with two followers.
   Verify follower settings and coordinate signs. Test exactly two supported RobStride
   motors for haptics with deliberately chosen K/D/torque; include RS04 endpoint scaling.
5. **Optional zero/persistence test, separate from motion acceptance:** if changing zero is
   intended, stop the mechanism and invoke mechanical zero explicitly. For MyActuator,
   check the 64 reply, then separately restart and read 92. For RobStride check the Type-6
   effect on installed firmware. Compare after power loss without any host offset. Do not
   infer a persistent hardware zero from a local display or successful serial write.

Physical bus timing, firmware command acceptance, torque/coupling feel, zero persistence,
adapter hotplug behavior and real DMX remain operator work. DMX code and UI evaluation
were kept separate; existing loopback Art-Net regression is software evidence only.

## Reviewable checkpoints

- `cc56e80`: explicit CAN format/DLC and wire validation.
- `fef8f94`: RS04 endpoint scales, parameters and preserved old-model parity.
- `72f26d9`: MyActuator profile, runtime/editor integration and mixed-model acceptance.
- A separate documentation/evidence commit contains this report and final measurements.

The only unrelated working-tree material remains the untracked `project_file/` folder.
Binaries, protocol downloads and test runtime data remain ignored. No force push or merge
is part of delivery. The final remote SHA is verified after pushing and reported in chat.
