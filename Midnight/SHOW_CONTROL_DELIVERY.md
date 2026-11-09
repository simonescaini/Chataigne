# Midnight Show Control delivery

The requested implementation is complete in the isolated Windows worktree `X:\01_PROJECTS\01_Active\2025-001_New_Show\06_orchestrator\Chataigne_Midnight_Show_Control` on [`feature/midnight-qlab-show-control`](https://github.com/simonescaini/Chataigne/tree/feature/midnight-qlab-show-control). Integration and Native POC remain separate and unchanged.

## Run the delivered build

- Example launcher: `Run-Midnight-Show-Control-Example.cmd`.
- General launcher: `Run-Midnight-Show-Control.cmd`.
- Executable: `Binaries/MidnightShowControl/MidnightShowControl.exe` (runtime DLLs remain beside it).
- Settings: `C:/Users/Simone/AppData/Roaming/MIDNIGHT SHOW CONTROL`.
- Self-contained example: `Midnight/examples/Show_Control_Integrated.noisette`; saved layout: `Midnight/examples/Show_Control.chalayout`.
- [Operator guide](SHOW_CONTROL_GUIDE.md), [SDK](CUSTOM_BEHAVIOUR_SDK.md), [acceptance map and limits](SHOW_CONTROL_ACCEPTANCE.md).

First session: run the example launcher, select cues and inspect the reaction, Mapping and package examples, then use **Start scene**, **Pause song**, **Resume song** and **End scene** in the Group Timeline. The example contains four fake cupboards, software light targets, a generated quiet song and a labeled software encoder recording. Loading is inert. Orange standby, blue selection and green active state are independent. The guide describes manual fake presses and recording/curve editing. The delivered application retains ordinary hardware operation when configured by the operator; the software-test boundary adds no unlock or interlock.

## Implemented scope

Scoped Haptic-to-Move handoff shares the native path used by Actions, Mapping and cues. The shared movement editor fits points/handles against seconds and degree references without altering motion. Single-cupboard recording preserves raw measured feedback, monotonic receipt timestamps, gaps and integrity, with explicit conversion into a separate editable absolute-angle movement.

The coordinator separates stable definitions from immutable run snapshots, typed native launch receipts, finite outcomes and retained ownership. It provides GO/standby and Direct Start, pre/post waits and continuation, multi-step reaction authoring with counted input watermarks, partial Retry, explicit Ignore, exact End, safe deletion/Undo and two-pass copies.

Persistent Mapping uses independent native clones with final generation and target-identity checks. Custom Behaviour packages have a versioned manifest, embedded/hash-validated assets, generic parameters/aliases, a serialized JUCE JavaScript worker, explicit clocks and bounded host resources. Finite light/movement and persistent light examples are in `packages/`.

All five Group modes, nested ownership, primary-song native audio clock, waveform/markers, timeline placement and inert inspection/seek are implemented. Pause leaves already-started movements and persistent interactions active; Resume releases deferred scene work without replay. Valid overdue discrete occurrences dispatch once in stable order with lateness diagnostics. No automatic scheduling-delay lockout is added. Saved dock panels expose Cue List, Inspector, Group Timeline, Active Cues, Recorder and hardware/input diagnostics. Definitions/assets reopen without reviving runs.

## Source and build identity

Final application source commit: [`dcf98f8a781189d490370c597114cbcdb6b25515`](https://github.com/simonescaini/Chataigne/commit/dcf98f8a781189d490370c597114cbcdb6b25515). The delivery commit adds documentation/evidence only. `final-build.json` records this exact source commit and SHA-256 for all 794 Source/JuceLibraryCode files; `verify_show_delivery.py` confirms the built sources match. The final incremental build exited zero and reproduced the already startup-tested binary byte for byte. `final-build.log` retains its output.

Executable SHA-256:

`06ca56a00243128955cf238b03cf1f542ab75b5cba9cb76508c63f36d3dc79e4`

Build command: `Build-Midnight-Show-Control.cmd` / `Midnight/build_show_control.py`, Windows x64 Release, Visual Studio 18 BuildTools MSBuild, toolset v145 / VCTools 14.44.35207. The generated local project is ignored; no exporter/CI strategy is changed. Dependency identity:

| Dependency | Checkout | Revision |
| --- | --- | --- |
| JUCE | `C:/Users/Simone/Dev/JUCE` | `d8b1e81eca9bd52a2f0e0783f687f991fa084a7f` |
| Timeline | Native POC `Modules/juce_timeline` | `decf0f92d0be4f2d51f2d23705031a1b891be4ed` |
| OrganicUI | Native POC `Modules/juce_organicui` | `5f3cf46f4fe19648084a0ec4e53e600d2c105523` |

All three checkouts are clean. The assignment-authorized existing C: JUCE checkout is used because `D:/Projects/Dev/JUCE` is absent. No dependency was cloned, copied into the workspace, linked, modified or added as a gitlink.

## Validation and preservation

**RUN/PASS:** all 15 final regression suites, recorded in `evidence/show-control/final/results.json`: native, V0.1, multi-motor, Chataigne integration, range, shared curve, recording, coordinator, Mapping, packages, Groups/music, native audio, integrated example, Python and pinned protocol reference parity. Detailed logs are retained beside the results. The full pass uses the final execution/editor implementation; the subsequent startup-layout/application-name changes are covered by the actual executable smoke. No physical motor adapter was used.

Native integrated UI tests execute actual panel controls and editor focus: 44 creation/authoring checks plus 13 checks in fresh-process reload. Native screenshots were rendered and inspected. Audio verification includes 11 offline-render checks and 6 real Windows device/callback checks. Twelve copied milestone/production shows pass inert round trips after their adapters and MIDI selections are replaced only in the copies. `executable-startup.json` records the responding **MIDNIGHT SHOW CONTROL** window loading a copied fake-only show; it records the same executable hash above. Its welcome-dialog suppression changes only the new product namespace.

**Preservation PASS:** all 1,608 original file hashes match, with zero missing/changed files; all 794 built source files match. Original Integration remains at `1e1dbd6e042400b6a02e8e2a2a4911e3e86dbb9b` with its pre-existing `?? .noisette/`; Native POC remains at `0fccc6adace3fdc6198e5aaa83e7e6543a43ee43` with its pre-existing `?? project_file/`. Original binaries, settings/layouts and production shows are unchanged. The before/after preservation reports contain the exact inventory.

**BLOCKED:** OS mouse-driven desktop automation, because both Computer Use entry points lacked the native pipe. This does not represent a mouse automation pass; real native component/control tests and renders passed.

**NOT RUN / remaining operator checks:** physical capture/reproduction fidelity and effort, installed model/firmware behavior, actual CAN/serial and MIDI/light endpoints, mechanical timing under load, long-duration show soak, non-Windows builds and QLab runtime parity. The first physical session should check configured motor identity/reference/MIN/MAX, scoped Haptic handoff, one manually guided take and its raw gaps/absolute start, playback/arrival and exact End. No physical session was initiated. JS execution is cooperative and intended for trusted small programs; arbitrary blocking C++ termination, video/external workers and universal motor pause/seek/fade are not provided.

## Assignment and version control

The dispatched assignment is retained verbatim at `evidence/show-control/DISPATCHED_ASSIGNMENT.txt`, SHA-256 `e3c3df569ad3d24165b6218a33020b1b43a659aba5c28303dc2357dd2844891f`. Reference memory/review/package-study revisions are recorded in the progress record and baseline evidence. Project Memory and historical design review were not edited.

Meaningful checkpoint commits are preserved on the new feature branch. Final publication uses a normal push to the verified `https://github.com/simonescaini/Chataigne.git` origin. Exact remote-tip verification follows the final documentation commit and is reported in the handoff response. No merge, tag, release, force-push or replacement of existing branches/installations is part of delivery. Binaries, runtime state, credentials and the local-only `site/` tree remain outside Git.
