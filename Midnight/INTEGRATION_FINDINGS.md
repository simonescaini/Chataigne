# Source findings and integration boundaries

## Source checkpoint and isolation

The original checkout was `feature/midnight-native-v0.1` at `0fccc6adace3fdc6198e5aaa83e7e6543a43ee43`. Tracked files were clean; the untracked `project_file` show and ten autosaves were preserved and copied to the experimental runtime archive.

The original executable SHA-256 is `254ecbe98b593ff84f55363cfe3b9f9933925fa197f70c91fa1e1b4129860634`, matching its existing multi-motor build report. All six recorded Midnight native source hashes matched. This identifies the reported build more strongly than the displayed 1.10.5b6 version, but the report does not identify every compilation input. Full reproducibility of that milestone is unverified.

The new branch is `feature/midnight-chataigne-integration`, in sibling `Chataigne_Midnight_Integration`. Checkpoint `292a11e` contains shared service/module, per-use conditions/commands and reusable editor integration. Implementation checkpoint dcd71fae6021315b475227f70090096898f5b1fb additionally fixes copied Action targets and editor help. Build products, launcher, settings identity, example projects and test settings are separate. No merge or push was performed.

Shared JUCE `D:/Projects/Dev/JUCE` is absent. The user explicitly approved using the existing external `C:/Users/Simone/Dev/JUCE`, revision `d8b1e81eca9bd52a2f0e0783f687f991fa084a7f4`. No JUCE repository, submodule, link or copy was added. Existing Modules/External build dependencies are read through the original checkout; runtime DLLs are copied to the new output. The working dependencies are not modified.

## Reference sources

- [Original native baseline](https://github.com/simonescaini/Chataigne/tree/0fccc6adace3fdc6198e5aaa83e7e6543a43ee43): NativeCore, NativeService, NativePanel; ChataigneEngine registry persistence; ConditionManager/Action lifecycle; Automation and SequencePlaybackClock.
- [Verified controller](https://github.com/simonescaini/Midnight_orchestrator/blob/b5418747bb4ce584c22a57baba38320955a9a7f4/Reference_controller/main_canable_dual_haptic_working.py): pinned SHA-256 `0dfe47ce2d90337498e207ee72c7ee88f029ddc9f2097cfe2150cc03232a73a9`.
- [FOC_OSC_bridge Cupboard_Cleanup](https://github.com/simonescaini/FOC_OSC_bridge/tree/Cupboard_Cleanup), inspected tree `efaad71bc5f6cc264f71d230b03b6f53bfcc2a08`: contextual structured code, not the verified replacement baseline.
- [Project memory](https://github.com/simonescaini/Midnight_Project_Memory): START_HERE, current status/decisions and Chataigne research consulted.
- [Saved OSC demonstration](https://github.com/simonescaini/FOC_OSC_bridge/blob/Cupboard_Cleanup/chataigne/demo_osc.noisette), blob `82511fe17aa3c4d17e3157f34582361bbff8f091`: its Sequence Automation contains Bezier keys and routes its Mapping output to OSC `/motor/1/position_cmd`. This is an implementation reference, not proof of the exact lost RobStride project.

## Material differences

| Area | Original reference / native baseline | Integration treatment |
| --- | --- | --- |
| Link and movement range | Original linking bypasses door-angle clamp; native Link called range-checked Move. | Corrected explicitly: only Move/curves use configured movement ranges. Link/Haptic retain protocol bounds, not door bounds. |
| Link master ownership | Original permits a master Move while followers remain linked; native claims master and followers and supersedes the whole event. | Baseline native ownership retained. The proposed reference-aligned master behavior awaits a user decision. |
| Haptic command conflict | Original suppresses selected position/link writes while relevant haptics are active; native event ownership supersedes prior interactions. | Baseline native supersession retained. The proposed original-style Move suppression awaits a user decision; no queue or new mutual-exclusion policy was introduced. |
| Haptic activation and feedback | Native validates fresh feedback, uses its worker and existing model drivers; activation/feedback cadence differs from the original. | Existing native implementation retained, not claimed equivalent to the original. |
| Ending Haptic | Native sends zero torque/disable using existing stop behavior; original restores CSP/run mode and saved limits. | Existing native stop retained. No new automatic restore policy. Explicit End exposes that behavior to host logic. |
| Move maximum effort | Native RobStride position path writes current limit 0x7018. | Labelled amperes, not calibrated torque. RMD has no equivalent effort control here. |
| Button lifecycle | Chataigne group disabled state did not reach nested conditions. | Propagated explicitly; user-approved crossing/rearm behavior implemented without evaluation-triggered initialization. |
| Action lifetime | A launched consequence is not implicitly undone by state exit. | Exact-instance End commands and authored On Deactivate examples; deliberately persistent interactions remain possible. |

Native motor generation handling, stop behavior, driver setup, fault/disconnect handling and existing supersession rules are preserved except for the explicit Move-range distinction and pending approved differences above. The spring button reuses native torque capabilities; no replacement position controller or broad controller rewrite was introduced.

## Persistence design

Hardware is stored once in `midnightNative`, with stable motor IDs. Module inputs use ID-derived addresses and display names that update independently. Per-use conditions, commands, curves, follower settings and target references are stored in normal Chataigne project data. Runtime instances have independent ephemeral IDs and are excluded from hardware persistence. Loading a document does not resume these runtime instances; host state load behavior remains under authored Chataigne control.

The native service owns one bus per adapter, the registry, feedback and worker. Editors and module value timers read snapshots. Communication does not depend on an editor being open. Behavior definitions and their editor instances are separate from execution snapshots.

## Validation commands

Run these with the configured Python from the experimental folder:

    Midnight/test_chataigne_integration.py
    Midnight/test_integration_range.py
    Midnight/test_v01.py --integration
    Midnight/test_multi_motor.py
    Midnight/test_native.py
    Midnight/compare_reference.py
    -m unittest discover -s Midnight/tests
    Midnight/verify_preservation.py

All CAN validation uses simulated adapters. Native OSC validation uses loopback ports. The differential script extracts the original RobStride class and selected pure supplemental functions without executing controller startup. The supplemental V2 comparisons are not a replacement behavioral baseline.

The live application launched and loaded the examples with separate identity. A Windows Security network prompt prevented completing live Inspector interaction; no network permission was granted. Rendered actual editor components and programmatic editor callbacks are tested separately. Physical evaluation remains for the operator.

Final executable SHA-256: 860609d25f9972aaf853c20d1e4f3d13f76bf2841436992126f654fb24f5d5d6. The build manifest hashes 775 Source/JuceLibraryCode files and confirms they did not change during the final build. It identifies implementation checkpoint dcd71fae6021315b475227f70090096898f5b1fb.
