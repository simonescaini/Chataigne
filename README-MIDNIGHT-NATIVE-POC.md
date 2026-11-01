# MIDNIGHT NATIVE POC

**Current status: native build blocked; C++ integration source prepared; Python OSC simulation tested.**
No native executable exists. The normal launcher reports that fact and exits with code 2.
Receiver tests are not a successful Chataigne native demonstration.

Independent checkout: **Chataigne_Native_Midnight_POC**.
Branch: **experiment/midnight-native-ui**. V2 and its controller remain unchanged.

## Commands

From X:\01_PROJECTS\01_Active\2025-001_New_Show\06_orchestrator\Chataigne_Native_Midnight_POC:

~~~bat
Build-Midnight-Native-POC.cmd --check
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

## Build prerequisites and blocker

The official unmodified baseline build_dev.bat failed with **MSBuild.exe not found**.
VS2022 C++ Build Tools, Windows SDK10 and modified JUCE are absent. Visual Studio installation
requires administrator permission, so no installer was run. No security settings were changed.

Continuation on a supported machine:

1. Provide Visual Studio 2022 Desktop development with C++, v143 x64 tools and Windows SDK10.
2. Provide official benkuper/JUCE, develop-local, at revision
   d8b1e81eca9bd52a2f0e0783f687f991fa084a7f (7.0.12). Do not substitute stock JUCE 8.
   AGENTS.md names shared location D:\Projects\Dev\JUCE, which does not exist here.
   The build script accepts an explicitly supplied external checkout via --juce PATH or
   MIDNIGHT_JUCE. Never clone JUCE inside Chataigne.
3. Retain the exact submodules recorded in Midnight/dependencies.json.
4. Re-establish an **unmodified baseline** at revision
   359701090e332c0a0c5dfde4b17de3c2be499524 in a separate clean build checkout.
   Follow upstream README/VS2022 instructions. Check launch, native State/Sequence editors
   and save/reopen before attributing failures to the Midnight adapter.
5. Run the experiment build command. It prepares an ignored exporter copy under
   Builds\MidnightNative, using external JUCE and isolated output paths. On compilation
   success it stages the official release DLL pack and records its SHA-256.
   Build output goes to Midnight\evidence\native-build.log.

Exporter preparation has unit tests; a successful full build is still unverified. Further
compiler/linker/runtime problems may surface after prerequisites are supplied. Do not use
Chataigne's upstream updater to replace this fork.

## Native acceptance procedure - pending, not claimed passed

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
~~~

The second command is explicitly a **Python-origin receiver diagnostic**, sending 102
messages and recording both directions. It does not exercise Chataigne.

Read [architecture](MIDNIGHT_NATIVE_ARCHITECTURE_REPORT.md),
[feasibility](MIDNIGHT_NATIVE_FEASIBILITY.md), and
[actual test results](MIDNIGHT_NATIVE_TEST_REPORT.md) before treating native features as verified.
