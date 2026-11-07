# Midnight Chataigne integration experiment

Use **Run-Midnight-Integration.cmd** in the separate `Chataigne_Midnight_Integration` folder. The window title is **MIDNIGHT CHATAIGNE INTEGRATION**. The executable is `Binaries/MidnightIntegration/MidnightIntegration.exe`; preferences and layouts use `%APPDATA%/MIDNIGHT CHATAIGNE INTEGRATION`. The original Native POC remains separate. This experiment is not merged into it.

## Starting the examples

Run **Run-Midnight-Examples.cmd** to open an evaluation copy of [Midnight_Chataigne_Integration.noisette](examples/Midnight_Chataigne_Integration.noisette). The launcher creates the copy once in Midnight/runtime and preserves your edits on later runs. Every adapter in this example is simulated. Every state loads inactive. Start one example state at a time; deactivate buttons and use the authored End consequences before changing interactions on the same motors.

Select the Midnight module to open shared Hardware Setup in the Inspector. The same setup is available in Midnight Show Control. Select a motor and open its settings. For software evaluation, enter an angle in **Target / mechanical degrees**, then press **Simulation only: set measured position to Target**. This changes fake encoder feedback without sending a real motor command. This control rejects physical adapters.

| State | Evaluation |
| --- | --- |
| 01 Button triggers Move | Set cupboard A to 20 degrees, activate the state, then inject 14 degrees. The Action explicitly deactivates its Free button before launching the authored 0-to-45-degree Move. |
| 02 Spring assisted button | Activate at 20 degrees, inject a nearby displacement. Observe commanded response in diagnostics; state exit ends the button. No physical stiffness has been verified. |
| 03 Free rearming button | Inject 20, then 14, hold, return to 20, then 14. Expect one press per crossing. |
| 04 Latched button and explicit reset | Cross once and return. TRUE remains latched. Trigger the Reset latch consequence; the response remains active. |
| 05 Absolute curve from measured 30 degrees | Inject 30 before triggering Play authored curve. The first requested angle is 0. Edit start, endpoint, speed, duration and interior curve points. Requested and measured angles are separate readouts. |
| 06 One master with two followers | State entry starts one Link. Inject master angles. State exit deliberately leaves this interaction running; activate state 10 and trigger End persistent Link to end that exact command instance. |
| 07 Two-motor haptics with explicit state ending | State entry starts the RobStride pair; the authored On Deactivate Action ends that exact Haptic consequence. |
| 08 Encoder Mapping to absolute target | Activate the state, inject cupboard A angles; the normal input/filter/output chain sends those absolute angles to cupboard B. After leaving the state, activate state 10 and trigger End persistent Mapping Move. |
| 09 Group activation and shared configuration | Enable/disable the group to exercise its nested button. Rename a motor in either Hardware Setup view and verify both views and module input names update. |
| 10 End persistent interactions | Enable this separate control state to end commands whose launching states are inactive. Its manual consequences address the exact Link and Mapping output. |

Save your own copy, close it, and reopen it. Check references, custom curve points, follower selections and inactive startup. The software acceptance test does this in a fresh process and also tests copies of the existing show/autosaves. Compatibility derivatives change only adapter transport fields to fake CAN before loading; originals are never opened for execution.

## Authoring

Add **Hardware > Midnight** through Modules. Multiple module views share the same engine-owned registry and service. They do not create another CAN connection. Measured angle, requested angle, feedback freshness, connection and available fault values are normal Chataigne input values, usable by From Input Value conditions and Mappings. A fault value of zero with ?unavailable? status is not evidence of fault-free hardware.

Add **Cupboard button** directly from Add condition. Its settings belong to that condition. Choose a stable motor identity, fixed/captured reference, signed offset, Free/Return response and rearm behavior. Unsupported spring/motor combinations report an error. Editor construction, selection and opening never start a motor interaction.

Choose Midnight **Move to position**, **Link** or **Haptic** in the normal consequence/output command selector. Move Mapping inputs are absolute target angles. Link/Haptic Mapping inputs are Active booleans: TRUE starts once, FALSE explicitly ends that output's instance. Disabling a Mapping does not synthesize FALSE; use End interaction when the enclosing logic requires an end.

**End interaction** selects the exact Start/Move consequence or Mapping output. It ends only runtime instances launched by that command object. Repeated trigger of the same Action consequence replaces/restarts that instance using existing native behavior; there is no automatic queue. Multiple command objects have distinct runtime identities. Deleting an owning command ends its instances. Leaving a state does not automatically undo consequences: author an On Deactivate Action with End interaction, as in scene 07, or deliberately keep it persistent, as in scene 06.

Activate, Deactivate and Reset Cupboard button commands address an exact condition, including nested conditions. A copied Action retains local button/end targets within that copied Action; references to objects outside the Action remain external. Conditions also expose these triggers in their editor.

## Cupboard lifecycle

| Transition | Result |
| --- | --- |
| Construct, copy, load, select or open editor | Inert; no motor interaction. |
| Owning Action/state enters activity, or explicit Activate | Capture measured reference once if selected; otherwise use authored reference. Clear latch and establish threshold side. Start Free or Return response once. |
| Already at or beyond threshold on activation | No press. Return to the starting side, then cross. |
| Cross in signed offset direction | One press and TRUE. Equality at the threshold counts as crossing from the starting side. |
| Hold beyond threshold | No additional presses. |
| Return with rearm enabled | FALSE and ready for another crossing; response stays active. |
| Return without rearm | TRUE remains latched. |
| Reset | Clear latch using current threshold side; retain reference and physical response. Beyond-threshold reset still requires return and recross. |
| Deactivate, disable owning Action/state/group/condition, or delete | End the owned button response and clear TRUE. |
| Reactivate/re-enable | New activation, including a new capture when selected. |

Grouped conditions propagate their enclosing disabled state. Group truth and individual FALSE results do not themselves deactivate buttons. Explicit Deactivate within an active state keeps the button inactive until a later lifecycle activation or explicit Activate. Editing per-use parameters while running affects the next activation, not the active snapshot. Live measured feedback remains attached to the active motor.

Free sends the existing real disable/release command; merely ceasing to send targets is insufficient. Return uses the existing supported RobStride torque mode and model-specific packet scaling. Its units are commanded Nm, Nm/rad and Nm/(rad/s); this is not a measured torque calibration. A spring button and a position Move cannot retain both control modes on one motor. As approved, scenes must explicitly deactivate that button before moving the same motor. Moving a different motor does not end the button. Spring response retains the native torque interaction's fresh-feedback requirement; a Free button waits for fresh samples when sensing is unavailable.

Cupboard conditions currently require non-multiplexed Actions; unsupported multiplex use reports a warning instead of silently sharing a physical response.

## Absolute curve behavior

The reusable movement editor is shared by Chataigne commands and Midnight event editing. It uses Chataigne Automation and its Linear, Bezier and Hold evaluators. The execution snapshot is serialized from that same model at trigger time. Changes during playback take effect on the next trigger.

Angles are absolute. At trigger, the first authored angle is requested immediately and the clock starts immediately. There is no start-position offset, scaling, preliminary move or arrival wait. Driver setup retains its existing order; the first target is preserved until transmitted. Subsequent curve samples follow elapsed time, including during resisted movement. The existing timeline clock runs in the service; target sampling is approximately 50 Hz. This is a software scheduling description, not a physical timing guarantee.

The authored start and final target are numeric and graphical values. Duration stretches time, preserving custom points and Bezier angular handles. Nominal speed sets duration from absolute endpoint distance/speed. Shape edits retain duration. Endpoint edits synchronize the corresponding number. Curves with coincident endpoints keep explicit duration and show nominal speed as unavailable/0. For reversals, nominal speed is endpoint distance/duration, not path speed or instantaneous velocity.

**Drive speed limit** is independent of nominal speed and retains the existing controller speed-cap role. It can physically limit steeper sections. Curve-derived speeds are not substituted as caps and no new feedback controller is implemented. RobStride **Maximum current / A** writes the existing current-limit parameter; it is not calibrated maximum torque. RMD's current-limit control is unavailable, and its existing whole-degrees/s speed cap is retained.

Move targets and entire authored curves must remain within saved Move MIN/MAX. Invalid keys or Bezier overshoot reject playback with an error; curves are not clipped, reshaped or rescaled. Link and Haptic ignore these Move ranges but retain model/protocol representability checks.

Curve completion requires clock end plus the existing fresh measured arrival test. ?Keep active after arrival? holds the final target; otherwise the drive is disabled on completion. Retrigger restarts the authored curve at time zero. Mapping replacement updates the owned position stream without repeatedly resetting drive setup; pending streamed samples may be replaced with the latest sample, while initial setup and first target remain ordered.

## Evidence boundaries

The original controller is the last operator-verified controller reference. The operator reports physical movement, Link and haptics working in Native POC 1.10.5b6. This integration has software-only validation. No physical motors were operated.

See [technical findings](INTEGRATION_FINDINGS.md), [acceptance results](evidence/integration-acceptance.json) and [baseline evidence](evidence/integration-baseline.json). Selected packet/calculation comparisons are not complete behavioral equivalence. RMD haptics remain unsupported; supported RobStride profiles and RMD movement are retained.

## Validation result

The final run passed 98 creation/execution checks and 22 fresh-process reload checks, plus 99 checks across 11 preserved show/autosave derivatives. Focused range tests passed 13 checks across RS01, RS02, RS04 and RMD. Existing regressions passed 161 core checks, 106 native integration checks, 84 mixed-model checks and 17 Python tests. Loopback OSC received and acknowledged all 17 expected commands across its two processes. Selected original CSP and 29 supplemental packet/calculation comparisons passed.

The actual movement and hardware editor components were rendered and inspected, including numeric values, graphical feedback and curve controls. A live application launch loaded the example workspace. Further live interaction was blocked by a Windows Security prompt; no permission was granted. Dismiss that prompt with Cancel for this local evaluation.

The original 107-file preservation manifest passed unchanged. The source/build manifest and results are in evidence/integration-delivery.json and evidence/integration-build.json.
