# Midnight Native V0.1

Windows application on the native Chataigne experiment. Start with
**`Run-Midnight-Native-V01.cmd`**. The executable is
`Binaries/MidnightNative/MidnightNativePOC.exe` (the retained build artifact name).
The V0.1 launcher needs no Python motor bridge or OSC receiver.

## Multi-motor profiles

Hardware Setup → Add Motor supports **RS01, RS02, RS04 and RMD-L-7025** on the
existing shared CANable transport. Model/profile selection is explicit; adapter
discovery cannot identify motors or installed firmware. All support direct Move,
measured-position Jog, MIN/MAX, movement events, position conditions, nested
timelines and one-master/multiple-follower position linking. These additions are
software-tested; none has been physically accepted in this assignment.

RS04 uses its own private CAN2.0 profile, with Type-1/2 velocity ±15 rad/s, torque
±120 Nm and Kp/Kd scales 5000/100. Its CSP parameter limits are 20 rad/s, 90 A and
120 Nm, which are separate from packet scales and motor ratings. Selecting RS04
does not raise saved current, speed or haptic torque. RS01/RS02 retain their prior
packet behavior. Haptics require exactly two RobStride motors, including mixed
RS01/RS02/RS04 pairs. RS04 uses Type-1 response feedback and polling; its ambiguous
Type-24 active-report command is not sent.

The MyActuator profile is **V3 controller / Servo Motor Control Protocol V4.2,
250208 file**, supplied on the manufacturer's L-series download page. It is a
selected document profile, **not verification of the installed L-7025 firmware**.
Use device ID 1..32, not an arbitration ID. Requests are standard CAN `0x140+ID`,
replies `0x240+ID`; this profile requires an already compatible 1 Mbps bus. It uses
`0xA4` absolute output-shaft position and `0x92` measured multi-turn angle, both in
0.01° units. No assumed encoder resolution or extra gear multiplier is used.
Speed is whole degrees/s; **0 means no drive speed limit** in this protocol.
Existing drive acceleration configuration is retained.

For MyActuator, Move starts position control; there is no separate Enable/Run
command in the selected profile. **Stop / Disable** sends `0x80` (output off).
The device-specific **Stop (retain closed-loop state)** sends `0x81`; stopping a
continuous link still ends the entire interaction using its established disables.
Current-limit editing, torque coupling, MyActuator haptics and cross-brand haptics
are unavailable. Mixed-brand one-way position following is supported.

Coordinate direction is an explicit +1/-1 setting. Targets, live position,
conditions and MIN/MAX all use that displayed coordinate frame, with no application
zero offset. Captured limits use fresh feedback. All devices on a reused adapter
share its saved bus settings; explicit edits to those fields apply to all motors sharing it.

Mechanical zero is always a separate deliberate operation. RobStride sends Type 6
while stopped; persistence remains unverified. MyActuator sends `0x64`, which the
selected manual specifies as a ROM write requiring a separate controller restart.
The application does not restart the controller or reset its displayed position;
the display continues to use measured replies. Test persistence separately and only
if the operator intends to change the controller's zero.

See [multi-motor test report](MIDNIGHT_MULTI_MOTOR_TEST_REPORT.md) for pinned
sources, discrepancies, capability tables, measurements and the physical checklist.

## First use

1. Open **Hardware Setup → Add Motor**. Select a detected serial adapter, or enter
   its port manually. Set the motor model, CAN ID, host ID where supported, bus bitrate and serial
   baud. The starting profile is RS02, ID 126, host 254, CAN 1 Mbps and serial
   2 Mbaud. No COM port is assumed. CANable VID/PID 16D0/117E describe a device type,
   not a unique motor or adapter.
2. Set mechanical MIN/MAX target bounds, speed and current where supported. **Save + Connect for
   feedback** observes the configured motors without enabling them. For another
   motor on the same CANable, choose **Reuse** that adapter.
3. Test **Move to Position**, jog, Enable and Stop/Disable directly in the motor
   settings. Measured mechanical position and the requested target are separate.
   Mechanical zero is a distinct controller-write button; nothing calls it on
   connect, save, load or reconnect. Zero persistence after power loss is unverified.
4. Add an event. The inner **Event Window** specifies device input/output and the
   relevant movement, following, haptic or DMX controls. The surrounding **IN/OUT**
   box specifies activation, continuation and deactivation relationships.
5. Add a timeline, double-click its box to enter, and add events or nested timelines.
   Drag blocks horizontally to edit their start offset and vertically to arrange
   lanes. Drag the right edge to edit a timeline's duration or an ordinary event's
   explicitly labelled planning width. Zoom and scroll expose long timelines.
6. Select a box and use **GO selected**. **Pause timeline** freezes scheduling;
   GO resumes. Stop and Restart have separate semantics, described below.
7. Save/Open ordinary native `.noisette` projects. **New** in the Midnight toolbar
   retains hardware definitions and clears event content. Projects retain their own
   hardware definitions. Opening a project never resumes execution.

**Demo (simulated)** adds three independent fake motor profiles without overwriting
existing hardware settings. The supplied `Midnight/examples/Midnight_V01_Demo.noisette`
contains the same example. It starts idle, with a slow 130° movement, a measured
10° crossing that replaces it with a fast 45° movement, an independent motor,
position-triggered Art-Net fade, nested timeline, one-master/two-follower link and
a separate one-to-one haptic pair. Link and haptic activation are deliberate GO
actions. In motor settings, **Simulation only: set measured position to Target**
provides a hand-position stimulus; it cannot target a physical adapter.

## Execution choices

- **One definition drives execution.** Movement target and speed in the editor are
  the actual command values. Position/speed mode is CSP, not an interpolated curve.
  Physical curve execution is unavailable and labelled so. Block width estimates
  do not decide arrival: completion requires fresh mechanical feedback within the
  saved arrival tolerance.
- **Latest deliberate command wins.** A newer command ends the previous event's
  ownership and cancels its queued packets. Superseded, stopped, failed and normally
  completed events remain distinct. There is no resume stack. A superseded event
  cannot later disable a motor now owned by another event.
- **Continuous transitions stop the complete interaction.** Moving/stopping any
  member of a link or haptic pair ends its producer. Haptics attempt zero torque
  and disable both members before a subsequent deliberate CSP setup. Nothing
  silently re-enables an old producer. Follow uses each destination's saved
  speed/current and the reference 1:1 mechanical-position/deadband algorithm.
- **Haptics** use the pinned spring/damper law, signs, relative alignment and torque
  saturation. Alignment is relative coupling geometry, not mechanical zero.
  Position deadband removes spring error but keeps damping. Fresh unwrapped Type-2
  position is preferred, with mechanical polling fallback; velocity freshness is
  independent. Missing fresh position ends the interaction with zero/disable
  attempts. MIN/MAX constrain requested position targets; they are not physical
  travel stops in torque mode.
- **Conditions:** an increasing crossing means previous position below threshold
  and a fresh value at/above it; decreasing is the reverse. Either direction accepts
  both. Activation captures the current baseline, so merely being beyond a threshold
  does not synthesize a crossing. Conditions latch once per activation. Repeat
  rearms after completion and requires a new crossing. ALL latches each condition;
  ANY accepts the first condition whose delay expires. Add both start and stop
  conditions with ANY for “starts OR stops.”
  An explicit Manual GO rule latches when GO is pressed and participates in the same
  delays and ALL/ANY combination. Without that explicit rule, GO starts the selected
  event directly; GO on a paused timeline resumes it.
- **Timeline offsets are earliest eligibility times.** Additional IN conditions
  must also fire. Conditional boxes explicitly label their uncertain execution
  time; their drawn offset is not a promised performance time. Conditions can
  latch during an active parent's lead-in. Starting/restarting a parent resets its
  children's condition baselines.
- **Pause:** freezes this timeline and nested timeline clocks and prevents waiting
  children from starting. Already issued moves, follows and haptics continue. A
  latched condition delay uses elapsed wall time; new conditions are not sampled
  while their ancestor is paused. **Normal end:** cancels still-waiting children,
  leaving ongoing children to finish or be explicitly stopped. **Stop:** stops
  descendants and disables only their owned motors. **Restart:** stops descendants,
  resets conditions/time and starts from zero. These are provisional V0.1 semantics.
- Outgoing continuation relationships are projections of destination IN rules;
  they are not a second executable copy. Copy/Paste clones a box and all descendants,
  remaps internal references and exposes retained external references in the editor.
  Display names and CAN addresses are not object identities.

## Native integration and limitations

The adapter is in `Source/Midnight/Native{Core,Service,Panel}.*`. Native Chataigne
`FileBasedDocument` persists the `midnightNative` section alongside existing native
data. Timeline scheduling reuses Chataigne's `SequencePlaybackClock` at 250 Hz in
the worker, avoiding the GUI-dispatched callback path of the stock sequence UI.
The focused adapter supplies device ownership, nested eligibility and edge rules
needed for this interface; it does not duplicate Chataigne's media/state/automation
subsystems. The original POC panel and acceptance suite remain available.

Each adapter owns one native SLCAN thread and receiver using the already pinned
`juce_serial` library. Serial reads/writes and continuous execution are independent
of GUI refresh. Required setup gaps are measured after actual serial writes, and
other motors can use the bus while one motor awaits its next setup packet. Queue
invalidation cannot retract a frame already being written by the OS. An unavailable
adapter cannot deliver disable. No drive-side watchdog or firmware setting is changed.

**CAN / timing status** exposes a bounded in-memory packet monitor and worker timing,
feedback age, queue depth and enqueue-to-write delay. TX means bytes written, not
drive acceptance; only addressed replies count as observation. These Windows timing
measurements are not hard-real-time guarantees. The fake bus verifies software
packet flow and kinematics, not physical torque, motor acceptance or travel limits.

DMX uses the existing Chataigne **DMXModule / DMXArtNetDevice**, with editable UDP
destination and Art-Net port-address. It does not claim unspecified USB-DMX hardware
support. DMX value delivery to the native module uses the message thread; motor and
haptic loops do not. No timing promise is made for DMX under a blocked GUI.

Adapter reconnection uses the saved Windows device instance when available; a
missing or ambiguous match needs manual selection. A device instance may depend on
USB topology if the hardware has no unique serial number. VID/PID alone never picks
one of several adapters. Editing an application CAN assignment does not rewrite a
hardware CAN ID. Reconnection restores observation only.

## Reproducible Windows build and software acceptance

Use the existing pinned dependencies and machine-local `Midnight/juce-path.txt`.
This machine uses the already provisioned external JUCE checkout. Never create a
JUCE clone, gitlink, junction or symlink inside this repository. The build script
does not modify CI or tracked exporter projects.

```bat
Build-Midnight-Native-POC.cmd --check
Build-Midnight-Native-POC.cmd
python -m unittest discover -s Midnight/tests -v
python Midnight/test_native.py
python Midnight/test_v01.py --integration
python Midnight/test_multi_motor.py
python Midnight/compare_reference.py
Run-Midnight-Native-V01.cmd
```

Use the Python executable in ignored `Midnight/python-path.txt` when `python` is not
on PATH. Close this application before rebuilding. The original POC suite now
chooses isolated loopback OSC ports; the V0.1 launcher itself does not use OSC.
`test_v01.py` compiles production core code and links the actual application objects
into a second, native integration executable. All its motors are fake; its Art-Net
destination is loopback. Results are in `Midnight/evidence/v01-*`.
`compare_reference.py` expects the read-only sibling checkouts `Midnight_Motion_Lab_V2`
and `Midnight_orchestrator` at the checkpoints recorded in the test report. It fails
if those references are missing or do not match; it does not run their controllers.

## Operator-only physical checklist

No physical acceptance is performed automatically. Use your established hardware
support and independent emergency stop for these deliberate checks.

1. Close other programs holding the adapter. Add/select the correct CANable and
   model/address. Verify fresh mechanical degrees before commanding movement.
2. Within the mechanism's known usable travel, test a small Move at the chosen
   speed/current, Stop/Disable, and a subsequent deliberate Move. Verify physical
   response separately from transmitted packets. Validate RS01 independently.
3. Enter/capture MIN and MAX; confirm they persist after Save/close/Open without
   changing measured mechanical coordinates. Only if intended, explicitly test
   Set Mechanical 0°. Separately check whether that zero survives a power cycle.
4. Test the slow 130° → measured 10° → fast 45° example only if both targets lie
   inside your actual mechanism's saved range. Verify no old target resumes and
   no obsolete completion disables the newer command.
5. Test one-way following with one follower, then two simultaneous physical
   followers when three motors are available. Two motors do not establish
   one-to-many physical validation. Check deactivate and subsequent deliberate Move.
6. Test a supported, configured haptic pair with your selected K/D/torque and axis
   signs. Verify relative alignment, deactivation, stale-feedback behavior and CSP
   transition. Legacy RS01/RS02 Type-24 reporting compatibility needs installed-firmware validation.
7. Connect the configured Art-Net node; test a channel value/fade and the measured
   position reaction. Check parent normal end, pause, stop and restart separately.
8. Save, close, restart and reopen. Expect hardware definitions/feedback restoration,
   with no automatic enable, motion, haptic activation or zero write.

Native physical movement, RS01/RS02 feedback behavior on the installed firmware,
multi-follower physical operation, coupling feel, zero persistence and physical DMX
remain operator validation. See `MIDNIGHT_NATIVE_V01_TEST_REPORT.md` for the actual
software results and remaining gaps.
