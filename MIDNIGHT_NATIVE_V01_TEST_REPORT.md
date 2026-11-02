# Midnight Native V0.1 delivery and acceptance

2 November 2026. Branch: `feature/midnight-native-v0.1`, continued from
`0580e55137355a7a880ad52b6c0825d75901f821`. The initial working tree was clean;
the target branch did not exist locally or remotely. No experiment/upstream merge,
dependency upgrade, reference-workspace edit or physical motor test was performed.

The deliverable is an integrated Windows application with native SLCAN, saved motor
configuration/direct tests, adaptive event editing, condition boxes, nested graphical
timelines and Art-Net output. Software acceptance uses fake motors and loopback UDP.
**Physical acceptance is pending. Desktop file-dialog acceptance is also incomplete.**
This report does not equate packet transmission or simulated feedback with drive acceptance.

## Build and launch

- Executable: `Binaries/MidnightNative/MidnightNativePOC.exe` (retained artifact name).
- One-click launcher: `Run-Midnight-Native-V01.cmd`; no mandatory OSC/Python motor bridge.
- Demonstration: `Midnight/examples/Midnight_V01_Demo.noisette`.
- Instructions and operator checklist: [README](README-MIDNIGHT-NATIVE-V01.md).
- Build result/hash: [build evidence](Midnight/evidence/v01-build.json).

The existing build script, external JUCE 7.0.12 revision
`d8b1e81eca9bd52a2f0e0783f687f991fa084a7f`, pinned submodules, MSVC 14.44 compiler
and Windows SDK 10.0.26100.0 are retained. The machine-local path comes from ignored
`Midnight/juce-path.txt`. No JUCE directory/link was created inside this workspace;
CI and generated tracked exporter projects are unchanged. Binaries remain ignored.

## Reference inspection and adaptation

Read-only local references were available at the requested checkpoints:

| Reference | Inspected material and resulting implementation |
|---|---|
| V2 `7eb07769f6a1f199526c2324f4c9108d99d2ed7b` | `README-V2.md`, protocol report, `protocol.py`, `motor.py`, `linking.py`, `haptics.py`, `transport.py` and relevant control tests. Model-specific limits, packet layouts, feedback addressing, coupling equation and following deadband are carried into native code. |
| Original `b5418747bb4ce584c22a57baba38320955a9a7f4` | `Reference_controller/main_canable_dual_haptic_working.py`, including `RobStrideMotor`, `LINKS`, link creation, `on_rs_position`, OSC link handler and haptic loop. One master/multiple followers and bidirectional spring/damper remain distinct. |

The original file SHA-256 is
`0dfe47ce2d90337498e207ee72c7ee88f029ddc9f2097cfe2150cc03232a73a9`.
The differential runner evaluates only pure reference functions/the motor class
with a fake CAN object; it never executes reference startup or opens a port.

RS01/RS02 writable parameter limits are 44/33 rad/s, 23/16 A and 17/14 Nm respectively.
The Type-1 torque encoding remains +/-17 Nm for both; the selected model bounds
the configured torque-limit parameter. These are protocol ranges, not motor ratings
or a claim that torque mode enforces MIN/MAX. No hidden high-current override,
application-zero offset, firmware write or hardware CAN-ID rewrite is introduced.

SLCAN uses one receiver/thread per saved adapter, a 2-second serial-open startup
interval and the pinned bitrate mapping (including S7=750 kbit/s, S9=83.3 kbit/s).
The known 1 Mbps / 2 Mbaud profile is editable. Windows instance identity is preferred;
missing/ambiguous matches require manual port selection. Discovery cannot identify
motor models or prove that configured CAN IDs exist on the bus.

## What was tested

Machine-readable counts and exit codes are authoritative:
[native acceptance](Midnight/evidence/v01-acceptance.json),
[original POC regression](Midnight/evidence/v01-poc-acceptance.json),
[Python tests](Midnight/evidence/v01-python.txt),
[reference comparison](Midnight/evidence/v01-reference-parity.txt).

| Final software suite | Result |
|---|---|
| V0.1 production core | 78 checks passed |
| V0.1 application integration | 52 creation + 54 fresh-process reload checks passed |
| Original native POC regression | 34 creation + 31 reload checks passed |
| Existing Python tests | 17 tests passed |
| Pinned reference differential checks | Original CSP sequence and 29 additional cases passed |

- Production core: literal packets, RS01/RS02 limits, address routing/request-echo
  rejection, mechanical zero command, feedback wraparound and drive faults;
  identity resolution; latest-command replacement, completion ownership;
  condition edges/delay/ALL/ANY; nested pause/end/stop; one master/two followers;
  haptic calculation, staleness and drive-mode transition; DMX fades and copy IDs.
- Production application objects: a real worker and shared packet-loopback bus
  with three motors; Connect/Move and Stop/Move; measured 10-degree replacement
  of slow 130-degree movement by fast 45-degree movement; normal completion;
  nested native clock execution; actual Chataigne Art-Net UDP packets; linking,
  haptic activation/deactivation, disconnect/reconnect without replay, rename,
  native Save As and a separate-process native document reload.
  Displacement produces nonzero continuous haptic packets for both motors, and the
  configured OUT crossing deactivates the pair before a deliberate CSP move.
- Actual JUCE editor components are instantiated in the integration process.
  Tests exercise saved MIN and adapter reuse, rule removal/Apply, and independent
  timeline Copy/Paste/removal through the production callbacks. These are component
  tests, not desktop mouse/keyboard acceptance.
  Invalid direct targets are also checked for a visible error inside motor settings.
- Exact original CSP byte parity: one documented stop followed by the original
  six-packet initialization/move sequence. An additional 29 differential cases
  compare native model writes, Type-1 torques and spring/damper outputs against V2.
- Original POC coverage is retained. Its test runner now chooses isolated loopback
  ports so an already-open POC receiver is not interrupted. Default POC launch ports
  remain unchanged. Historical evidence is preserved; this run has separate files.

## Timing boundaries

[Worker measurements](Midnight/evidence/v01-worker-metrics.json) record mean/max
loop interval, maximum excess above the mean, observed crossing RX-to-target TX,
enqueue-to-write delay, queue high-water and feedback age at the measurement snapshot.
The workload includes three fake motors, shared production bus queues, continuous
haptic packet generation, repeated rendering/serialization and a 250 ms blocked GUI.
The worker continues while that GUI thread is blocked.

Final run: 2,240 worker intervals; mean **3.820 ms**, maximum **9.876 ms**,
maximum excess above mean **6.056 ms**. Observed position crossing to replacement
target transmission: **62.257 ms**. Maximum enqueue-to-write delay: **57.265 ms**;
queue high-water: **15 packets**. Maximum feedback age across motors at the final
snapshot: **3.045 ms**. These are one measured simulation run, not guaranteed bounds.

The crossing-to-target measurement includes the retained CSP setup spacing after
supersession. It is not merely the time to append a command to a queue. Serial USB
latency, physical CAN arbitration, drive response and mechanism arrival are outside
these simulation measurements. A queued cancellation cannot retract an OS write
already in progress. No hard-real-time guarantee is made. Art-Net module dispatch
uses the GUI thread and has a different timing boundary from motor control.

## Screenshots and visible execution choices

These PNGs are direct renders of production native components in the test process,
not concept artwork: [adaptive haptic editor](Midnight/evidence/v01-adaptive-editor.png),
[graphical timeline](Midnight/evidence/v01-timeline.png),
[inside a nested timeline](Midnight/evidence/v01-nested-timeline.png).

The Event Window describes device interaction; surrounding IN/OUT controls describe
relationships. No Explorer tree or creation/performance mode split is introduced.
Dragging edits executable offsets and timeline duration. Movement block widths are
labelled planning estimates; position/speed completion uses measured arrival.
Curve-driven motor execution is unavailable and explicitly labelled in the editor.

The native project system stores stable IDs, hardware, nested events and relationships.
The focused adapter reuses `SequencePlaybackClock` and the existing DMX module;
it supplies the nested eligibility, edge and device-ownership rules required here.
The old POC automation curve/OSC-command disconnect is not reused. The README records
normal parent end, pause, stop/restart, relative haptic alignment, crossing rearming,
continued ownership after completion and whole-interaction supersession semantics.
An explicit Manual GO rule participates in delays and ALL/ANY. Without that rule,
GO starts the selected box directly; a paused timeline resumes.

## Remaining validation and limitations

- **Physically verified in this assignment: none.** Native RS01/RS02 movement,
  installed-firmware status/Type-24 behavior, adapter hotplug identity, physical
  coupling and zero persistence need the operator checklist. Three fake motors
  establish software multi-following, not simultaneous physical following.
- The fake bus models position/speed kinematics and packet replies. It does not
  model haptic forces, a real drive's command acceptance or mechanical travel stops.
- **Desktop New/Save/Save As/Open and close/restart interaction is not fully verified.**
  The application and Save As dialog were opened, but Computer Use was stopped by
  the user. Desktop automation stayed stopped. Native document API save/reload and
  editor component tests passed; they do not substitute for this remaining workflow.
- Curve-driven motor trajectories are not implemented. No editable dummy curve is
  presented as executable motion. Ordinary CSP remains position/speed mode.
- DMX support exposed here is Art-Net/UDP through Chataigne's existing module.
  No unspecified USB-DMX device compatibility or physical DMX delivery is claimed.
- Disconnect/load/close attempt owned-motor disables while transport is available.
  A missing adapter cannot deliver them; no drive watchdog or protection is bypassed.
