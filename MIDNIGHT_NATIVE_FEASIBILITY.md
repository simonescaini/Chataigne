# Midnight native feasibility assessment

2 November 2026. **All native conclusions below are source-level findings, not runtime proof.**
The native build is blocked by absent VS2022 C++/SDK/JUCE prerequisites. The simulated Python
receiver is the only executed application component. See [actual test results](MIDNIGHT_NATIVE_TEST_REPORT.md).

| Question | Answer and practical consequence |
|---|---|
| 1. Can we replace Chataigne's interface? | Yes in architectural terms: `OrganicMainContentComponent`, `MainContentComponent` and ShapeShifter compose the workspace separately from engine managers. Start with custom docks, preserving the existing editor panels. Full replacement and embedded-editor behavior are unverified. |
| 2. Can we control Time Machine directly? | Its native manager creates ChataigneSequence objects; sequence parameters/triggers expose duration, current time and Play/Pause/Stop. The panel source calls these APIs. Playback through the new panel has not run. |
| 3. Can we control State Machine directly? | StateManager, StateTransitionManager, StandardCondition, ActivationCondition and ConsequenceManager expose the required operations. The new source creates real native objects and does not define another state machine. Execution is unverified. |
| 4. Can we create automation curves programmatically? | `Automation::addKey`, key value/position parameters, easing types and `setLength` exist. The source authors Mapping1DLayer keys and changes them through Midnight controls. No native interpolation result has been measured. |
| 5. Can we reuse the graphical timeline editor? | `TimeMachineView::setSequence` and `SequenceEditorView` provide a direct route; Chataigne already extends the transport UI this way. The POC opens that existing editor. Embedding it inside a redesigned workspace still needs lifecycle/layout tests. |
| 6. Can we create custom motor-management panels? | Yes: JUCE controls in a registered ShapeShifter panel can edit native model parameters and invoke commands. Production motor-profile persistence and validation are future adapters, not part of this demo. |
| 7. Can we connect the Python motor controller? | OSC is a plausible process boundary. A new simulated schema and receiver are tested over UDP. Existing V2 has not been given an OSC API, and the full Chataigne-to-Python path has not run. A real bridge needs explicit translation and acknowledgements through V2's existing service. |
| 8. How much Chataigne modification is needed? | This source prototype adds one module and small hooks in three application/UI files. No execution-engine or submodule change was made. That does not establish the change size needed for a complete Midnight product. |
| 9. Can compatibility with future updates be maintained? | Probably with pinned revisions and regression tests, not guaranteed. This is a source integration against internal APIs. Preserve native `.noisette` objects/commands, isolate adapters and review upstream migrations. |
| 10. Is it preferable to continuing V2 independently? | Promising for mature timelines, conditions and mappings, but not established yet. V2 already has hardware evidence. Keep it intact and require a successful native build, restart/persistence test and simulated end-to-end run before choosing a migration. |

## Future feature mapping

| Midnight requirement | Chataigne foundation | Work still needed |
|---|---|---|
| Add/Edit Motor, RS01/RS02, parameters, zero offsets and limits | Custom ShapeShifter/JUCE panel; native controllable containers | Stable motor identifiers and a persisted profile schema; delegate actual protocol validation/control to the Python service. |
| Multiple CAN motors, direct Move/Stop | OSCModule/CustomOSCCommand to the existing service | V2 OSC adapter, command results, motor ownership, mode changes, feedback freshness and error reporting. No CAN code should move into the POC. |
| Virtual cupboards | JUCE component observing native parameters/receiver feedback | Explicit distinction between intended, simulated and actual positions. |
| Timeline and curves | ChataigneSequence, Mapping1DLayer, Automation, native editors | Motor actions should initially remain discrete verified position/speed commands. Editing a curve is not physical trajectory verification. |
| QLab cue organization | Sequence/State commands, native TimeCue/ChataigneCue conditions | Ordered manual GO cursor, auto-continue vs auto-follow semantics, named execution instances and cancellation semantics require a deliberate adapter. |
| Nested timelines | SequenceBlock / SequenceBlockLayer references | Breadcrumb UI is straightforward; independent overlapping instances of the same reusable definition are not established. |
| IN/OUT events and push input | StateMachine conditions, Actions and Consequences | Domain-specific controls and graphs; tactile force behavior still belongs to the motor service. |
| One-to-many linking and haptics | Native actions activate/deactivate service interactions | Keep continuous motor loops within the existing controller. Do not stream haptic torques through a show timeline or assume Windows/OSC hard real-time timing. |
| External OSC | Existing OSC modules and routing | Define a versioned service contract and response/error protocol. The POC uses a simulation-only schema. |

## Incompatibilities that matter

**Definition versus execution instance.** A Sequence owns a mutable currentTime/isPlaying state.
SequenceBlock holds a target reference to that object; SequenceBlockLayer directly seeks and
plays/pauses it. This is not evidence for separate concurrent instances of the same definition.
Two parent timelines targeting one child may contend for its clock. Do not promise reusable
parallel instances without a focused experiment and likely instance/definition adaptation.

**Cue meaning.** Chataigne's timed cues, condition-enabled cues and native sequence commands
are useful primitives, but do not by themselves reproduce a QLab cue list with independent
GO cursor, continuation timing and instance-scoped stopping. That semantic layer remains to be designed.

**Feedback and completion.** Timeline time is not proof of a motor reaching its target.
Completion-follow logic must use service acknowledgements/feedback where appropriate.
UDP messages are not reliable delivery or a substitute for an independent stop mechanism.
These concerns are future integration work, not reasons to add global movement permissions.

**Project loading and object lifetime.** Built-in state load behavior may activate actions.
The demo is authored to load IDLE and deactivate ARMED/RUNNING. Its IDLE entry stops the
sequence and sends only a simulated stop. Whether all command references resolve correctly
across an actual restart is still unverified. The panel currently finds demo objects by fixed names;
renaming/deleting them requires recovery or a new demo. Production identifiers and listeners are needed.

**Two different demonstrations.** Native curve automation updates a displayable native value;
timed events send discrete simulated target/speed messages. The latter do not track edits to
curve keys automatically. This intentional separation avoids introducing an unverified motor trajectory path.

## Decision and next experiment

Use the smallest fork: one Midnight dock plus native State Machine and Sequence Editor panels.
Keep the Python controller as a separate authority for motors. Avoid replacing engine internals.

The next decision point is a supported VS2022/JUCE build followed by the mandatory native
save -> close process -> restart -> reopen -> replay test. Until that succeeds, this is a promising
architecture with a tested simulated receiver, **not an accepted native application prototype**.
