# Midnight Native - verified Windows results

Date: 2 November 2026, Europe/Brussels. Branch: `experiment/midnight-native-ui`.

**Result: the existing Midnight Native prototype builds and runs on Windows.**
Stock Chataigne compiled first. The custom executable passed **65 native checks** across
creation and fresh-process reload, plus **17 Python tests**. The visible interface was launched
and exercised with the simulation receiver. No physical motors were connected.

## Executables and reproduction

- Prototype: `Binaries/MidnightNative/MidnightNativePOC.exe` (19,561,984 bytes).
- Normal launch: `Run-Midnight-Native-POC.cmd` (owns the loopback receiver).
- Pristine baseline: `Binaries/ChataigneBaseline/Chataigne.exe` (19,510,272 bytes).
- Both are x64 Release development builds, with whole-program optimization disabled.
  They are local artifacts; binaries, intermediate files and downloaded dependencies are ignored.

Prototype SHA-256: `25f6054a537ae31e60227359e9ded75ff8e4dd964ae191e3b9c1d1afe37bc96b`.
Baseline SHA-256: `fb4d48f25a731818bb20aaaa18dd4e6acafd4e8e321495753af03cab016cfeeb`.

```bat
Build-Midnight-Native-POC.cmd --check
Build-Midnight-Native-POC.cmd --baseline
Build-Midnight-Native-POC.cmd
Run-Midnight-Native-POC.cmd
```

Close the GUI before rebuilding or running the native acceptance suite, because it owns the
feedback port. Using the Python executable in `Midnight/python-path.txt`:

```bat
python -m unittest discover -s Midnight/tests -v
python Midnight/test_native.py
```

The native runner links the production application objects into a console acceptance executable.
It invokes the existing panel callbacks and real managers, conditions, commands, automation,
playback clock, and document APIs. Python only orchestrates processes and simulates the receiver.
It does not send the control packets or substitute an execution engine.

## Build and dependency verification

| Item | Verified result |
|---|---|
| MSBuild | Visual Studio Build Tools 2026, MSBuild 18.11.1.51303, discovered through vswhere |
| C++ compiler | MSVC 19.44.35229 x64; pinned `VCToolsVersion=14.44.35207` (v143 compiler) |
| MSBuild platform files | Installer supplied v145 wrappers, omitted v143 wrappers; local generated projects select v145 while retaining the 14.44 compiler |
| Windows SDK | 10.0.26100.0 headers, libraries and successful resource compilation/linking |
| Modified JUCE | Official `benkuper/JUCE`, `develop-local`, revision `d8b1e81eca9bd52a2f0e0783f687f991fa084a7f`, version 7.0.12 |
| JUCE location | User-approved external `C:\Users\Simone\Dev\JUCE`; D: has no mounted media on this PC |
| Recursive submodules | All seven recorded revisions present, unchanged |
| WebView2 SDK | Exporter's pinned Microsoft.Web.WebView2 1.0.902.49 restored from NuGet |
| External libraries | Stock and custom application linked successfully against the checked-in libraries |
| Runtime DLLs | Official Windows release archive staged; all 21 DLLs loaded successfully |
| Stock application | Unmodified source exported from `359701090e332c0a0c5dfde4b17de3c2be499524`, built before the prototype |
| Experimental application | Existing implementation resumed, compiled and launched; no engine/submodule redesign |

The machine-local JUCE path is stored in ignored `Midnight/juce-path.txt`. No JUCE repository,
gitlink, copy, junction or symlink was created in Chataigne. CI and tracked exporter projects
were not changed. MSBuild's file tracker required execution outside the Codex filesystem sandbox.
No Windows security or execution-policy setting was changed.

Evidence: [toolchain](Midnight/evidence/windows-toolchain.json),
[baseline build](Midnight/evidence/baseline-result.json),
[prototype build](Midnight/evidence/native-result.json),
[DLL checks](Midnight/evidence/runtime-dll-check.json).
Full compiler logs remain locally in `Midnight/evidence/*.log`.

Official sources: [Chataigne](https://github.com/benkuper/Chataigne),
[modified JUCE revision](https://github.com/benkuper/JUCE/tree/d8b1e81eca9bd52a2f0e0783f687f991fa084a7f),
[WebView2 SDK package](https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.902.49),
[Microsoft toolchain documentation](https://learn.microsoft.com/en-us/cpp/overview/acquire-msvc?view=msvc-170).
Download hashes are recorded in the toolchain evidence.

## Native acceptance results

**34 checks passed in the creation process; 31 passed in a second process.**
Both processes exited with code 0.

| Behavior | Result |
|---|---|
| Simulation module | Exactly one OSC module, input disabled, output restricted to 127.0.0.1:19001 |
| State Machine creation | IDLE, ARMED, RUNNING; three native transitions with StandardCondition inputs |
| Native transitions | Arm -> ARMED, Run -> RUNNING, Return Idle -> IDLE; exactly one active state |
| Sequence activation | RUNNING executes the real native sequence command; ARMED does not play |
| Sequence authoring | One native sequence, mapping layer, four automation keys and four timed OSC actions |
| Playback | Clock advances; Pause freezes time; Play resumes; non-looping sequence stops at its end |
| Automation | Real mapping changes the native Automation position parameter during playback |
| Editing | Key at 1 s changed from 45 to 30; duration changed from 6 s to 7 s |
| Invalid shortening | Existing key/event beyond a proposed 4 s duration prevents the edit |
| Direct controls | Native move to 22 at speed 60 and explicit Stop acknowledged by receiver |
| Feedback | Native panel receives OSC acknowledgements and simulated position |
| Persistence | Native asynchronous Save As writes `.noisette`; new process loads it through native FileBasedDocument |
| Restored contents | States, transitions, duration 7, key value 30, four timed actions and command targets restored |
| Restored custom values | Default cupboard-1 identifier, edited target 22 and speed 60 survive; Refresh saved motor fields restores UI entries |
| Replay after restart | Restored state transitions start playback, automation and the four OSC actions again |
| Small dock | A viewport retains access to controls when the panel is only 400 x 220 |

Logs: [create process](Midnight/evidence/native-create.txt),
[reload process](Midnight/evidence/native-reload.txt),
[acceptance summary](Midnight/evidence/native-acceptance.json).
The actual saved project is local at `Midnight/runtime/Midnight_Acceptance.noisette`.

The receiver accepted and acknowledged **9 commands during creation and 8 after reload**.
Movement targets were exactly `[0, 45, 90, 0, 22]` in creation (last one is the direct command),
and `[0, 45, 90, 0]` after reload. Every packet originated in compiled native Chataigne code.
Raw packet bytes, accepted arguments, acknowledgements and position feedback are recorded in
[creation OSC evidence](Midnight/evidence/native-osc-create.jsonl) and
[reload OSC evidence](Midnight/evidence/native-osc-reload.jsonl).
These measurements verify software behavior; they do not establish hard real-time timing.

## Visible application check

The normal launcher started the final custom executable and simulator. The interface displayed
live receiver feedback. The visible panel created a demo, changed IDLE to ARMED through its input
button, selected Run, and started native execution. The receiver observed all four timed moves.
Scrolling exposed the input selector and six editable fields. The native sequence editor showed
the mapping and timed command layers. The final build displayed no upstream update prompt.

Evidence: [native interface screenshot](Midnight/evidence/native-gui.png),
[GUI-origin OSC commands](Midnight/evidence/native-gui-osc.json),
[launcher record](Midnight/evidence/native-launcher.txt).
The GUI remains running with the manually created demonstration unsaved.

Native Save/Open dialog automation was unreliable because the desktop automation helper
reported fragmented/incorrect window geometry. The **document API and process-restart
persistence checks passed**; a full manual Save/Open dialog and graphical-layout restoration
workflow is not claimed as passed. The automated fixture tests the project model and references.
Stopping the first owned GUI process also stopped its launcher and receiver.

## Fixes made

- Discover the installed VS2026 MSBuild and explicitly select the installed v143 compiler.
- Restore WebView2, keep external JUCE paths, isolate baseline/native outputs, and correct the
  linker output filename to match the launcher.
- Correct the typed point initialization and qualify `juce::Result` in the existing C++ panel.
- Serialize custom default values explicitly so the motor identifier is not lost on reload.
- Retain all existing panel controls inside a scrollable viewport.
- Keep the simulator alive when Windows reports an unavailable UDP feedback listener.
- Show the GUI even from a hidden launcher; disable the fork's upstream startup-update check.
- Add reproducible native integration coverage, the UDP-startup regression, and current evidence.

The Python suite passed **17 tests**: [test output](Midnight/evidence/python-tests.txt).
Earlier pre-installation `*-build.txt` / `command-results.json` files are historical records;
the JSON build results and native logs linked above describe the resumed development run.

## Remaining limitations and preservation

This remains a simulation POC with fixed object names, a simple layout and no grouped undo for
full demo creation. Renaming/deleting its objects can break the panel's lookup. Continuous curve
mapping and discrete OSC movement actions are separate: editing a curve does not rewrite the
four authored command arguments. Production motor profiles, physical feedback, haptics, CAN,
trajectory safety and a V2 bridge remain outside this implementation.

Optional dashboard asset downloads emitted warnings in the GUI. Dashboard, media/video,
hardware modules, installer packaging, code signing, optimized release performance and other
platforms were not acceptance-tested. Loading every runtime DLL is not a functional test of
every optional module. No upstream release or tag was published.

Midnight Motion Lab V2 remains clean at `7eb07769f6a1f199526c2324f4c9108d99d2ed7b`.
The original `Midnight_orchestrator` remains clean at `b5418747bb4ce584c22a57baba38320955a9a7f4`.
No original reference project was opened or modified. No physical motor, serial/CAN module,
or physical destination was added to either acceptance project.
