# Midnight / Chataigne integration checkpoint
Baseline: feature/midnight-native-v0.1 at 0fccc6adace3fdc6198e5aaa83e7e6543a43ee43.
Integration: feature/midnight-chataigne-integration in a separate sibling worktree.
The original 1.10.5b6 executable SHA-256 matches the recorded multi-motor build report
(254ecbe98b593ff84f55363cfe3b9f9933925fa197f70c91fa1e1b4129860634).
All six recorded native source hashes were checked; see evidence/integration-baseline.json.
The report does not capture all source/toolchain inputs, so full reproducibility is unverified.
Uncommitted project_file shows and autosaves are preserved in the original and copied into
ignored Midnight/runtime/preserved-milestone. Original settings were copied there too.
No original executable, launcher, settings, dependency or show file was modified.

## Implementation checkpoints
1. Shared service and Midnight module; isolated build/settings identity.
2. Per-use Cupboard conditions and native commands; explicit lifecycle hooks.
3. Reusable editors and Chataigne absolute-angle Automation/Bezier playback.
4. Project persistence, fake CAN integration tests, examples, runnable build.

## Source findings and decisions
- NativeCore.cpp Runtime::claim/move/stop supersede complete interactions.
  Original main_canable_dual_haptic_working.py allows master Moves while Link continues,
  and suppresses position/link writes during haptics. Both reference-aligned choices approved and implemented: master Moves retain Link; active Haptic suppresses Move without queuing.
- Native Link calls range-checked move; original on_rs_position bypasses door clamping.
  Explicit requirement: Move rejects out-of-range targets; Link/Haptic ignore door ranges.
  Curves must not be clipped or rescaled.
- Native beginTorque/step/disable supplies supported RobStride haptics. RMD haptics are
  unsupported. Reuse drivers and preserve RS01/RS02/RS04/RMD move support.
- NativeService owns the hardware registry, one Bus per adapter and worker execution.
  ChataigneEngine persists midnightNative; do not add another serial controller.
- ConditionManager owns the Add condition factory; Action::updateDisables propagates
  lifecycle state. ConditionGroup did not propagate disabled state to children; integration now does.
  Construction/editor activity must remain inert.
- Automation provides editable keys/Bezier and getValueAtPosition; reuse the same model.
  Nominal speed describes authored endpoint distance/duration, not a drive speed cap.
- RobStride Move writes current limit 0x7018 (amps), not calibrated maximum torque.
- Source observations, software tests and operator physical reports remain distinct.

## Approved build dependency
AGENTS.md requires D:/Projects/Dev/JUCE, absent on this machine.
Existing native build used C:/Users/Simone/Dev/JUCE. Explicit user exception approved.
No JUCE clone, copy, submodule, junction or symlink will be created.

## Internal validation checkpoints
- Shared service/module, conditions/commands and reusable editors: source checkpoint 292a11e.
- Persistence: create/reload passed; 11 preserved show/autosave compatibility derivatives passed.
- Final validation and executable hash are recorded in Midnight/evidence.
