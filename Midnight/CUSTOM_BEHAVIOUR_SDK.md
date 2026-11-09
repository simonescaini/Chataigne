# Custom Behaviour SDK v1

This is the `juce-small-js-v1` runtime in the isolated Midnight Show Control build. Packages are trusted small programs, not an adversarial sandbox. The host provides no filesystem, process, network, raw module, `root`, or `util` object. Package callbacks run serially on their own worker, never on the UI, audio callback, or motor worker. Host requests are nonblocking messages; the coordinator performs output dispatch.

## Package and authoring

A package folder contains `manifest.json`, its relative `entry`, and every declared relative asset. The supplied `packages/finite-light`, `packages/persistent-light`, and `packages/finite-movement` are working starting points. Add a Custom Behaviour cue, choose **Load package folder**, then open **Package parameters and aliases** in the inspector. Bind each declared alias using the native selectors. A motor command alias points to a cue with one shared Move/Link/Haptic behaviour; each package run owns its own command slot. A light alias points to an existing writable parameter. Inputs can read an encoder or an existing parameter.

The loader embeds the exact entry and assets in the saved show, with package ID, authored version, and SHA256 of the canonical manifest/files data. It rejects unsupported profiles, dependencies, escaping relative paths, missing assets, invalid capabilities, and altered hashes. Load and parameter editing execute no JavaScript. Syntax and execution errors are diagnosed at explicit Start. Later disk edits do not change an embedded or running snapshot; explicitly load the new package to use changed bytes.

The first profile supports number, Boolean, and string parameters; input aliases; `start` motor commands; and `setValue` parameter or streaming Move commands. There are no external libraries. Entry budget: 256 KiB. Assets: at most 4 MiB each / 16 MiB total. Callbacks have a 10 ms cooperative interpreter budget. Each direction has at most 128 queued messages, and there are at most 64 outstanding timers. Overflow and callback errors fail only the owning package and trigger its declared motor cleanup.

## Language

The pinned JUCE interpreter supports `var`, functions, objects, arrays, `if`, loops, ordinary arithmetic/comparisons, and its Object/Array/String/Math/JSON helpers. It is not a browser or Node environment. Do not use `let`, `const`, classes, arrow functions, Promises, async/await, imports, or JavaScript `throw`/`try`/`catch`. Use `host.fail(message)` for an explicit package failure. Built-in `eval`/`exec` operate inside this same interpreter; they are not operating-system execution APIs. A program can still consume resources or contain bugs: trust and review it.

## Callbacks

```js
function onStart(context) {}
function onInput(context, alias, capturedValue, token) {}
function onTick(context) {}
function onTimer(context, timerId) {}
function onRequestResult(context, result) {}
function onStop(context, reason) {}
```

Only `onStart` is required. `context.params` is a run-local copy of the validated parameter snapshot; changing it cannot edit the cue. The context also publishes `liveTime`, `sceneTime`, `scenePlaying`, and `packageHash`. Input values and tokens are captured by the host. `onTick` defaults to the scene scope; manifest `tickScope: "live"` explicitly allows ticking while the song is paused. An input's declared `scope: "live"` allows that input callback while paused; other inputs use scene scope.

## Host calls

| Call | Contract |
| --- | --- |
| `host.request(alias, value, restart)` | Returns a request ID, or -1 after admission closes. Requires `write:alias`. Value is a bounded primitive. `restart` is optional and false by default. A busy finite/native alias rejects an ordinary new request; explicit true requests normal takeover. Streaming Move aliases sample the latest requested absolute target. |
| `host.complete()` | Finite packages only. Closes further script output and timers. Aggregate success waits for requested required finite native children. Completed-held motor work stays owned until exact End. |
| `host.timer(id, delaySeconds, scope)` | One-shot timer, explicitly `"live"` or `"scene"`; .01 to 86400 seconds. Reusing an ID replaces its pending timer. Requires `hostTimer`. Re-arm explicitly from a callback for repetition. |
| `host.status(text)` | Bounded diagnostic text; requires `status`. It does not set execution success or motor state. |
| `host.fail(text)` | Explicit scoped package failure and declarative cleanup. |
| `host.assetText(relativeName)` | Reads an already embedded, declared asset as UTF-8 text; requires `assets`. No filesystem call occurs in the callback. |

`onRequestResult` receives `{requestId, accepted, outcome}`. Acceptance establishes dispatch, not finite motor success. Further outcome messages report native success/failure where applicable. A Move succeeds only after its authored clock ends and fresh measured feedback reaches the target. A package that calls `complete()` after requesting a required Move therefore waits for that native outcome.

## End, failure and persistence

End closes admission before cancelling callback queues/timers and ending the package's exact owned motor handles. The optional stop hook is bounded and cannot reopen admission. Cleanup happens even if it fails. Parameters/lights retain their last value; an output reset must be authored explicitly. Old requests and old run generations cannot affect a replacement run. Other cues and the original referenced motor definition remain independent.

The host never forcibly terminates a C++ thread. The interpreter budget is cooperative; allowed native bridges perform short bounded work. Heavy/blocking libraries, video decoding, hard isolation and arbitrary external dependencies are outside this profile. They need a separate process adapter, not a longer timeout or unsafe thread termination.

Software tests establish host behavior, not physical movement accuracy or QLab parity. Normal package Start uses normal outputs; the delivered examples should first be bound to the included fake fixtures for an operator review.
