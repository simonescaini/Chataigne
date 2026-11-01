"""Prepare/build the official VS2022 exporter with an external pinned JUCE checkout.
No compiler installation, execution-policy change, engine substitution or upstream push.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parent.parent
DEPENDENCIES = json.loads((ROOT / "Midnight/dependencies.json").read_text())
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}


def preflight(juce):
    issues = []
    vswhere = (
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        / "Microsoft Visual Studio/Installer/vswhere.exe"
    )
    msbuild = None
    if not vswhere.is_file():
        issues.append(
            "Visual Studio Installer/vswhere.exe is missing; VS2022 C++ Build Tools (v143) are required."
        )
    else:
        found = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-version",
                "[17.0,18.0)",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-find",
                r"MSBuild\**\Bin\MSBuild.exe",
            ],
            text=True,
            capture_output=True,
            check=False,
        )
        if found.returncode == 0 and found.stdout.strip():
            msbuild = Path(found.stdout.splitlines()[0])
        else:
            issues.append(
                "No VS2022 MSBuild installation with the C++ workload was found."
            )
    sdk = (
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        / "Windows Kits/10/Include"
    )
    if not sdk.is_dir():
        issues.append(
            "Windows 10/11 SDK headers are missing (the installed SDK 8.1 is insufficient)."
        )
    if not (juce / "modules/juce_core/juce_core.h").is_file():
        issues.append(
            f"Modified JUCE 7.0.12 checkout missing: {juce}; see Midnight/dependencies.json."
        )
    else:
        rev = subprocess.run(
            ["git", "-C", str(juce), "rev-parse", "HEAD"],
            text=True,
            capture_output=True,
            check=False,
        )
        if rev.returncode or rev.stdout.strip() != DEPENDENCIES["juce_revision"]:
            issues.append(
                "JUCE revision differs from the recorded upstream develop-local revision; establish compatibility before building."
            )
    return msbuild, issues


def prepare_project(juce, folder):
    source = ROOT / "Builds/VisualStudio2022_CI"
    folder.mkdir(parents=True, exist_ok=True)
    for name in ("resources.rc", "icon.ico", "packages.config"):
        shutil.copy2(source / name, folder / name)
    project = (source / "Chataigne_App.vcxproj").read_text(encoding="utf-8")
    # Same directory depth as the official exporter: all other relative paths remain valid.
    project = project.replace(r"..\..\JUCE" + "\\", str(juce) + "\\")
    project = project.replace("../../JUCE/", juce.as_posix() + "/")
    tree = ET.fromstring(project)
    for element in tree.findall(".//m:OutDir", NS):
        element.text = str(ROOT / "Binaries/MidnightNative") + "\\"
    for element in tree.findall(".//m:TargetName", NS):
        element.text = "MidnightNativePOC"
    ET.register_namespace("", NS["m"])
    target = folder / "MidnightNativePOC.vcxproj"
    ET.ElementTree(tree).write(target, encoding="utf-8", xml_declaration=True)
    return target


def stage_runtime():
    cache = ROOT / "Midnight/runtime"
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / "Chataigne-win-x64-release-dependencies.zip"
    url = "https://benjamin.kuperberg.fr/chataigne/user/data/Chataigne-win-x64-release-dependencies.zip"
    if not archive.is_file():
        with (
            urlopen(url, timeout=60) as response,
            archive.with_suffix(".download").open("wb") as output,
        ):
            shutil.copyfileobj(response, output)
        archive.with_suffix(".download").replace(archive)
    output = ROOT / "Binaries/MidnightNative"
    output.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as bundle:
        for member in bundle.infolist():
            destination = (output / member.filename).resolve()
            if not destination.is_relative_to(output.resolve()):
                raise ValueError("Unsafe path in runtime archive")
        bundle.extractall(output)
    if not (output / "libmpv-2.dll").is_file():
        raise RuntimeError("Official runtime pack did not contain libmpv-2.dll")
    (cache / "runtime-download.json").write_text(
        json.dumps(
            {"url": url, "sha256": hashlib.sha256(archive.read_bytes()).hexdigest()},
            indent=2,
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true", help="Report dependencies without building"
    )
    parser.add_argument(
        "--juce",
        type=Path,
        default=Path(os.environ.get("MIDNIGHT_JUCE", r"D:\Projects\Dev\JUCE")),
    )
    args = parser.parse_args()
    juce = args.juce.resolve()
    if juce.is_relative_to(ROOT.resolve()):
        parser.error("JUCE must remain outside the Chataigne checkout, per AGENTS.md")
    msbuild, issues = preflight(juce)
    result = {
        "msbuild": str(msbuild) if msbuild else None,
        "juce": str(juce),
        "blockers": issues,
        "compiled": False,
    }
    evidence = ROOT / "Midnight/evidence"
    evidence.mkdir(exist_ok=True)
    (evidence / "build-preflight.json").write_text(json.dumps(result, indent=2) + "\n")
    if issues:
        print("Native build BLOCKED before compilation:")
        for issue in issues:
            print("- " + issue)
        print("No software was installed and no security settings were changed.")
        return 2
    if args.check:
        print("Prerequisites found; compilation/runtime have not been verified.")
        return 0
    project = prepare_project(juce, ROOT / "Builds/MidnightNative")
    command = [
        str(msbuild),
        str(project),
        "/t:Build",
        "/p:Configuration=Release",
        "/p:Platform=x64",
        "/p:PreferredToolArchitecture=x64",
        "/m:2",
        "/v:minimal",
        "/nologo",
    ]
    with (evidence / "native-build.log").open("w") as log:
        code = subprocess.call(
            command, cwd=project.parent, stdout=log, stderr=subprocess.STDOUT
        )
    if code:
        print("Build failed; see Midnight/evidence/native-build.log")
        return code
    stage_runtime()
    print(
        "Executable built and official runtime pack staged. Runtime/persistence acceptance still required."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
