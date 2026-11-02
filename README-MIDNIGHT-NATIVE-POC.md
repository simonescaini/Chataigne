# MIDNIGHT NATIVE POC

**Current status: Windows native executable built; native State Machine, Time Machine,
automation, loopback OSC and cross-process save/reload acceptance passed.**
See [verified results and limitations](MIDNIGHT_NATIVE_TEST_REPORT.md).

Independent checkout: **Chataigne_Native_Midnight_POC**.
Branch: **experiment/midnight-native-ui**. V2 and its controller remain unchanged.

## Commands

From X:\01_PROJECTS\01_Active\2025-001_New_Show\06_orchestrator\Chataigne_Native_Midnight_POC:

~~~bat
Build-Midnight-Native-POC.cmd --check
Build-Midnight-Native-POC.cmd --baseline
Build-Midnight-Native-POC.cmd
Run-Midnight-Native-POC.cmd
~~~

The normal launcher expects **Binaries\MidnightNative\MidnightNativePOC.exe**. After a
successful build it starts the simulated receiver hidden, opens that executable, and
owns/stops the helper when the GUI exits. It never falls back to installed Chataigne or V2.

The independently working receiver can be launched now:

~~~bat
Run-Midnight-Native-POC.cmd --receiver-only
~~~

Stop with Ctrl+C. Live logs go to Midnight\runtime\receiver.jsonl.
The launcher uses Midnight\python-path.txt, or py -3. On another machine, put a Python 3.11+
executable path into that machine-local file. Here it points to the existing standalone Python
3.13.5 installation; it does not depend on V2's environment. No pip dependencies are needed.

## Verified Windows build

The pristine baseline at `359701090e332c0a0c5dfde4b17de3c2be499524` builds to
`Binaries/ChataigneBaseline/Chataigne.exe`. Its source is exported from Git into ignored
`Midnight/runtime/baseline`; it shares the unchanged pinned Modules/External dependencies.
The experimental branch builds separately to `Binaries/MidnightNative/MidnightNativePOC.exe`.
Both are x64 Release development builds (whole-program optimization disabled, as in upstream
`build_dev.bat`). CI and tracked Projucer exporters are unchanged.

This machine uses Visual Studio Build Tools 2026, MSBuild 18.11.1, the installed v143 compiler
19.44.35229 (`VCToolsVersion=14.44.35207`), and Windows SDK 10.0.26100.0. The installer omitted
the legacy v143 MSBuild wrappers, so local builds use the v145 wrappers with the compiler
explicitly pinned to 14.44. No compiler or SDK installation is performed by the build script.

Modified JUCE 7.0.12 comes from official `benkuper/JUCE`, `develop-local`, at
`d8b1e81eca9bd52a2f0e0783f687f991fa084a7f`. The user approved
`C:\Users\Simone\Dev\JUCE` because D: is empty removable media on this PC.
That machine-local selection is in ignored `Midnight/juce-path.txt`; `--juce` and
`MIDNIGHT_JUCE` override it. The default remains the AGENTS.md shared D: path.
JUCE must always remain outside the Chataigne workspace.

The script restores the exporter's pinned Microsoft.Web.WebView2 1.0.902.49 SDK from NuGet,
stages the official Windows release DLL archive, and records its hash. All seven recursive
submodule revisions are pinned in `Midnight/dependencies.json`. Python needs no pip packages.
Build logs and intermediate outputs are local/ignored; concise results are in `Midnight/evidence`.
MSBuild's file tracker requires running outside the Codex filesystem sandbox on this machine.
The fork does not check the upstream Chataigne release channel at startup.

## Manual acceptance procedure

The native automated suite exercises the same panel callbacks and actual engine objects.
The dock scrolls when it is too small to show all controls.

Use only newly created experimental projects. Do not open existing user projects or add
serial/CAN modules.

1. Launch the normal launcher. The **MIDNIGHT NATIVE POC** dock should appear and also be
   available through ShapeShifter's panel menu. Resize/dock it as needed.
2. Click **New experimental project**. This should create the simulation OSC module,
   sequence, automation, four timed commands, three states and three transitions.
   Chataigne's normal unsaved-project prompt is retained.
3. Open **Native State Machine**. Inspect IDLE, ARMED, RUNNING and the three transitions,
   including their input-equals-true conditions.
4. The trigger control clears the three previous demo inputs before raising the selected input;
   HIGH/LOW remains separately editable. Select **Arm**, then **Trigger selected input**. Verify ARMED. Select **Run**, trigger again:
   verify RUNNING, native playback and its OSC state notification. The HIGH checkbox also
   changes the selected Boolean input. Pulsing resets LOW then HIGH for repeatable cycles.
5. Open **Native sequence editor**. Confirm Midnight_Cupboard_Demo, duration 6 seconds,
   Mapping keys (0,0), (1,45), (3,90), (5,0), and four Trigger-layer OSC commands.
6. Set key time 1 and key value 30, then click **Add/update key**. Verify the native editor
   changes. Set duration 7 with **Set duration**. Shortening past existing keys/triggers
   reports the conflict rather than silently moving them.
7. Exercise Play/Pause/resume/Stop. Compare native time and automation values with the
   receiver's separately simulated position. Confirm four timed OSC movement records.
8. Select **Return Idle** and trigger. Verify IDLE, sequence stop and simulated motor stop.
   Repeat Arm/Run. **Activate IDLE** provides direct native state activation for inspection.
9. Enter motor identifier, target and speed; click **Send simulated move**. Confirm native
   outgoing logging, receiver acknowledgement and position. **Stop sequence + motor** sends
   explicit Stop. Direct fields do not rewrite existing timeline command arguments.
10. Save a NEW Midnight/runtime/Midnight_Demo.noisette using **Save project**. Quit the entire
    GUI, confirm the helper exits, restart the launcher and use **Open project**.
11. Confirm three states, three conditional transitions, duration 7 seconds, the edited
    30-degree key and four OSC actions. Refresh saved motor fields, trigger Arm/Run, confirm
    playback and OSC again. Record actual logs/screenshots before marking persistence passed.

The panel currently uses fixed object names. Renaming/deleting them can lose its targets;
native editors remain available. A product needs stable identifiers and full lifecycle handling.
Complete demo creation is not grouped into an undo transaction.

## Simulation-only OSC contract

Commands go to **127.0.0.1:19001**. Replies go to **127.0.0.1:19002**, where the native panel
is intended to listen. These are NOT existing V2 API endpoints.

| Address | OSC arguments | Meaning |
|---|---|---|
| /midnight/sim/move | sff: motor ID, target degrees, speed degrees/s | Rate-limited simulated move; finite positive speed. Numeric int32 values also accepted. |
| /midnight/sim/stop | s: motor ID | Hold current simulated position; no physical brake/disable semantics implied. |
| /midnight/sim/state | s: IDLE / ARMED / RUNNING | Notification only; receiver does not start sequences. |
| /midnight/sim/ack | s: JSON with accepted address/args | Command accepted, not evidence of physical motion. |
| /midnight/sim/feedback | sff: motor ID, simulated position, target | Nominal 20 Hz; scheduling-dependent. |
| /midnight/sim/error | s: message | Rejected command; receiver continues processing valid input. |

Strings are UTF-8, numbers int32/float32, with OSC four-byte alignment. Only immediate bundles
are accepted; future timetags are rejected because Chataigne owns sequence timing.
There is no CAN/serial access, motor fault model, torque model, linking or haptics.
UDP acknowledgement is diagnostic, not a reliable physical-control protocol.

The native automation curve and discrete OSC movement events are separate examples.
Editing a curve does not silently stream it to the receiver. Timed commands retain their
authored motor ID/speed; edit those arguments through the native inspector.

## Reproduce software evidence

Using the Python executable recorded above:

~~~bat
python -m unittest discover -s Midnight/tests -v
python Midnight/test_receiver_loopback.py
python Midnight/test_native.py
~~~

The last command links a native console test to the compiled application objects, invokes the
existing panel callbacks, then saves/exits/loads/replays in a second process. Its only Python
network participant is the simulated receiver; Chataigne originates every control packet.
Use `--skip-build` only when the acceptance executable already matches the current build.

The second command is explicitly a **Python-origin receiver diagnostic**, sending 102
messages and recording both directions. It does not exercise Chataigne.

Read [architecture](MIDNIGHT_NATIVE_ARCHITECTURE_REPORT.md),
[feasibility](MIDNIGHT_NATIVE_FEASIBILITY.md), and
[actual test results](MIDNIGHT_NATIVE_TEST_REPORT.md) before treating native features as verified.
