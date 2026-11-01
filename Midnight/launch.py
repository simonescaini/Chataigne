"""Launch only this experiment's executable and the loopback simulation service."""

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receiver-only", action="store_true")
    args = parser.parse_args()
    os.chdir(ROOT)
    receiver = [sys.executable, "-u", str(ROOT / "Midnight/sim_receiver.py")]
    if args.receiver_only:
        return subprocess.call(receiver)
    executable = ROOT / "Binaries/MidnightNative/MidnightNativePOC.exe"
    if not executable.is_file():
        print(
            "MIDNIGHT NATIVE POC has not been built. No native runtime test has passed."
        )
        print(
            "Run Build-Midnight-Native-POC.cmd after installing the documented VS 2022 C++/Windows SDK and modified JUCE dependencies."
        )
        print(
            "For the independently testable simulation receiver: Run-Midnight-Native-POC.cmd --receiver-only"
        )
        return 2
    logs = ROOT / "Midnight/runtime"
    logs.mkdir(parents=True, exist_ok=True)
    # Keep only the GUI visible. The helper is owned by this launcher and is stopped on exit.
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    with (logs / "receiver-console.log").open("w") as output:
        process = subprocess.Popen(
            receiver, stdout=output, stderr=subprocess.STDOUT, creationflags=flags
        )
        try:
            time.sleep(0.3)
            if process.poll() is not None:
                print(
                    "Simulation receiver failed. See Midnight/runtime/receiver-console.log; the app was not launched."
                )
                return 2
            try:
                return subprocess.call([str(executable)], cwd=executable.parent)
            except OSError as exc:
                print(
                    f"Native launch failed: {exc}. Check the official runtime DLL pack."
                )
                return 2
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == "__main__":
    raise SystemExit(main())
