# Midnight Show Control — operator guide

This is the separate Windows build in `Chataigne_Midnight_Show_Control`, branch `feature/midnight-qlab-show-control`. Integration and Native POC remain separate. The executable and settings namespace are **MIDNIGHT SHOW CONTROL**.

## First session

1. Start `Run-Midnight-Show-Control-Example.cmd`. It opens `Midnight/examples/Show_Control_Integrated.noisette`, containing four fake cupboards, software light values and a quiet generated 12-second song. Nothing starts on load. Its recorded take is explicitly a software encoder fixture, not a physical capture.
2. The saved layout places Cue List at left, Group Timeline and Active Cues in the middle, and the shared Inspector at right. Recorder, hardware/input diagnostics and Custom Variables are additional tabs. Restore `Midnight/examples/Show_Control.chalayout` through Chataigne's layout menu if needed. Raw Chataigne tools remain available from the panel menu.
3. Select a row to edit it. Orange **STANDBY** identifies the next GO; blue identifies selection; green **ACTIVE** identifies pending/retained work. Selection and GoTo emit no output. **Direct Start** leaves standby alone. **GO** starts standby and advances it. Space is GO only while the cue list itself has focus; text editing does not GO.
4. Inspect the reaction, Mapping and package examples. **Start scene** in the timeline plays the demonstration and schedules fake actions. Change the software CC value under Custom Variables to exercise the light Mapping/package.
5. **Pause song** freezes future scene scheduling while running Moves finish their authored curves and persistent work continues. **Resume song** continues at the same position. **End scene** or **End selected run** closes that run's pending work and owned descendants. Successful Moves can remain **OWNED / HELD** until End or takeover.

The example uses fake adapters. The application also follows ordinary real hardware commands when configured with hardware. There is no new arming step, test unlock or compulsory simulation mode. This development session did not operate physical motors.

## Reaction routine

Select a Reaction routine and press **Add step**. Step settings choose the input cupboard, signed trigger offset, reference/capture, Free/Return response, rearm, after-press deactivation and optional **Wait for reactions**. Choose a movement from the Inspector selector to edit its cupboard and shared curve. **Add movement** creates a batch; Duplicate and Up/Down reuse and order steps.

The supplied routine is cupboard 1 → cupboard 2 throw-and-return; cupboard 4 → cupboard 3 throw-and-return; a later cupboard 1 press → cupboards 2 and 3 together. Inputs activate once per routine. Consuming a trigger route leaves the physical input response active. Reusing an input does not recapture its reference or reset its latch. **Deactivate inputs** or owning scene End explicitly ends responses. A same-motor reaction performs the scoped input handoff before Move.

For a software-only press, open **MIDNIGHT SHOW CONTROL** diagnostics → **Hardware Setup** → select cupboard → **Edit selected motor + test**. Set Target to 20 and press **Simulation only: set measured position to Target**, then repeat at 14. Apply this to M1, then M4, then M1 after returning it to 20. This injects fake measured feedback and is distinct from Move to Position.

By default the next step arms after all required native launches are accepted. **Wait for reactions** also requires finite success, including fresh measured arrival. Failed batches expose each result; **Retry failed members** preserves accepted work. Repeated Move/Restart follows normal native takeover. Ignore is an explicit authored choice.

## Curves and recording

Commands, cues and recordings share one native movement model/editor. Seconds follow the actual duration. Degrees show the selected motor's saved MIN/MAX reference band. The view fits all points and handles with padding, including out-of-range authoring. It never rewrites stored curves. Selected-point values provide exact time/angle information. Existing Move validation still applies.

Recorder workflow: select cupboard → **Record** → guide it → **Stop recording** → name → **Save name** → **Create / restore curve**. Capture uses accepted measured encoder replies and host-monotonic receipt timestamps, not UI polling or requested targets. Record sends no movement, zero or release command. **Release selected motor** is a separate existing native operation; no torque tuning is invented.

The immutable raw take retains motor/reference metadata, samples, gaps and integrity hash. Explicit conversion keeps every sample with linear interpolation and creates a separate editable absolute-angle curve. It does not smooth or simplify. Curve time zero is the first genuine sample; the original retains initial capture latency. The recorded start angle is shown. Interpolation across gaps is not invented measured evidence.

**Play movement** uses the normal absolute Move engine without homing or re-anchoring. **End playback** differs from Stop recording. Closing the editor does not stop playback. **Use recording** in a movement/step inspector adds an independent editable copy to that cue. Save the show to retain raw takes and edits.

## Persistent Mapping

Add Persistent Mapping and choose **Cue-owned native Mapping editor**. Configure native inputs, filters and outputs. **Link existing Mapping** snapshots an original into an independent instance at Start; it never borrows or toggles the original. Two cues can instantiate the same original independently.

Start evaluates the current input once. Navigation leaves processing active. End closes final output admission before cleanup. **End owned interactions** defaults on for motor use. Turning it off ceases updates while retaining owned motor work visibly; End that retained entry again to release it. Light cease leaves its last value. Multiple writers follow actual native dispatch order; no HTP merge is added.

The example includes cupboard measured angle → cupboard Move and software CC → light. This is a fake MIDI-value stimulus, not a physical MIDI-device test. Missing/deleted targets fail explicitly; an accepted run cannot adopt a replacement with the same name, including at final dispatch.

## Custom Behaviour packages

Add Custom Behaviour, choose **Load package folder**, then **Package parameters and aliases**. The generic inspector exposes declared fields and named input/output bindings. The show embeds validated code, manifest and relative assets with ID/version/SHA-256. Loading and editing execute nothing. Start freezes code, parameters and bindings.

See [CUSTOM_BEHAVIOUR_SDK.md](CUSTOM_BEHAVIOUR_SDK.md) and `packages/` for finite light, persistent threshold and finite movement examples. Every run has a serialized JUCE JavaScript worker. Callbacks run outside UI/audio/motor threads. Host requests are nonblocking; input/timer scope is explicitly live or scene. End closes admission and performs exact owned cleanup even if onStop fails. Finite completion waits for required native children, then blocks further script output while leaving successful held children endable.

This is a cooperative host for trusted small programs. The 10 ms callback budget and bounded queues are not an adversarial sandbox or a way to interrupt arbitrary blocking C++ code. No OS/filesystem/network bridge or heavy-library host is provided. Video decoding and external worker packages remain future work.

## Groups and music

| Mode | Behavior |
| --- | --- |
| Timeline | Starts eligible children at their placements; child continuation cannot schedule them twice. |
| Start First | Starts the first eligible child and permits its authored continuation chain; standby remains outside. |
| Start First and Enter | Starts the first child and places GO inside. Later entered-list GO remains owned by that exact Group run. |
| Playlist | Advances after finite success, including motor arrival. Persistent children require explicit **Group Next**, which retains them. |
| Random | Chooses one eligible, non-pending child and avoids repeats until the available cycle is consumed. Choice is logged. |

Nested Group End closes only its own descendants and input responses. It does not sweep independently launched runs of nested definitions. End cues resolve the newest matching run in their owning subtree; top-level End cues target top-level runs. Use Active Cues for exact selection of other runs. Finite Group success may retain persistent descendants.

A primary song uses a Timeline Group. **Load song** embeds its bytes. The native audio clip transport is the sole song clock; coordinator scheduling never runs in the audio callback. Waveform, green clock cursor and tap markers use song seconds. The yellow inspection cursor, dragging and numeric placement are inert. Placement/pre-wait is one property here; changing movement duration does not shift later actions. Zoom/pan and optional 0.1-second/marker snapping are available. Waveform display gain is normalized for visibility without changing audio gain.

Pause stops the song and future scene launches, not the native motor clock. Started Moves complete their full authored curves. Persistent Mapping/Link/Haptic continues. Explicitly live reaction routes and live package timers remain active. Scene successors wait for Resume. Resume adds no return wait, replay, re-anchoring or motor gate.

Late coordinator service dispatches valid overdue discrete occurrences once in scheduled-time/authored order. Several may arrive close together; actual lateness is logged. There is no automatic delay lockout. Missing actual audio clock is reported specifically and never becomes fabricated wall-clock time or a show-wide lock. Recovery retains occurrence identity.

**Seek active (pauses)** is explicit and inert; earlier placements are visibly skipped on Resume. **Start from cursor** creates a new run. EOF Resume is a no-op; use explicit Start/Restart. Loops have distinct epoch/iteration occurrence identities. Default repeated Move follows takeover; authored Ignore remains available. Edits affect future starts while active runs retain snapshots.

Intentional QLab differences include measured-arrival Playlist progression, no universal motor pause/fade/seek, no motor crossfade policy, explicit owned cleanup and the assignment's overdue-dispatch behavior. These tests do not establish QLab runtime equivalence.

## Diagnostics, persistence and implementation boundary

Active Cues distinguishes finite outcome from retained ownership and keeps runs after definition deletion. Its log records acceptance, native launch results, lateness, route consumption and exact cleanup. Hardware diagnostics expose requested/measured values, freshness and faults. Clock-end without fresh arrival is not success. Failures are scoped; other work remains usable. Undo restores definitions without reviving runs.

Save/load persists versioned definitions, stable identities, curves, raw recordings and embedded assets, never active execution. Closing/replacing the show closes scheduling before disposing show-owned work. Raw Chataigne tools retain their existing lifetimes.

Coordinator, recorder and editors use the shared Service's high-level events, measured samples and serial-owned receipts. Runtime retains ownership/conflict and completion semantics. Driver retains CAN framing, mode transitions and model-specific behavior. A valid Move automatically ends its affected Haptic pair, applies the existing transition and starts once; the other paired motor is no longer coupled. Unrelated interactions remain active. Later protocol additions can be localized here but may still need higher-level capability changes.

## Build and evidence

Use `Build-Midnight-Show-Control.cmd` / `Midnight/build_show_control.py`. The verified external JUCE checkout is `C:\Users\Simone\Dev\JUCE`, used because the preferred D: checkout is absent. Shared Timeline/OrganicUI dependencies are read from the preserved Native POC checkout. No nested JUCE checkout, copied dependency, symlink or CI change is used.

See [SHOW_CONTROL_ACCEPTANCE.md](SHOW_CONTROL_ACCEPTANCE.md), [SHOW_CONTROL_DELIVERY.md](SHOW_CONTROL_DELIVERY.md), and `evidence/show-control/` for current results, source/binary identity and preservation. Physical recording fidelity, installed motor firmware and mechanical timing remain operator checks, not software-test claims or application restrictions.
