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
import io
import tarfile
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
                "-products",
                "*",
                "-requires",
                "Microsoft.Component.MSBuild",
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
                "No MSBuild installation was found."
            )
    if msbuild:
        installation = msbuild.parents[3]
        compilers = list((installation / "VC/Tools/MSVC").glob("14.4*/bin/Hostx64/x64/cl.exe"))
        if not compilers:
            issues.append("MSVC v143 x64 compiler is missing from the MSBuild installation.")
    sdk = (
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        / "Windows Kits/10/Include"
    )
    if not any(sdk.glob("10.*/um/Windows.h")) or not any((sdk.parent / "Lib").glob("10.*/um/x64/kernel32.lib")):
        issues.append(
            "Windows 10/11 SDK headers are missing (the installed SDK 8.1 is insufficient)."
        )
    if not (juce / "modules/juce_core/juce_core.h").is_file():
        issues.append(
            f"Modified JUCE 7.0.12 checkout missing: {juce}; see Midnight/dependencies.json."
        )
    else:
        rev = subprocess.run(
            ["git", "-c", "safe.directory=" + juce.as_posix(), "-C", str(juce), "rev-parse", "HEAD"],
            text=True,
            capture_output=True,
            check=False,
        )
        if rev.returncode:
            issues.append("Could not verify JUCE revision: " + rev.stderr.strip())
        elif rev.stdout.strip() != DEPENDENCIES["juce_revision"]:
            issues.append(
                "JUCE revision differs from the recorded upstream develop-local revision; establish compatibility before building."
            )
    return msbuild, issues


def prepare_project(juce, folder, source_root=ROOT, baseline=False):
    source = source_root / "Builds/VisualStudio2022_CI"
    folder.mkdir(parents=True, exist_ok=True)
    for name in ("resources.rc", "icon.ico", "packages.config"):
        shutil.copy2(source / name, folder / name)
    project = (source / "Chataigne_App.vcxproj").read_text(encoding="utf-8")
    # Same directory depth as the official exporter: all other relative paths remain valid.
    project = project.replace(r"..\..\JUCE" + "\\", str(juce) + "\\")
    project = project.replace("../../JUCE/", juce.as_posix() + "/")
    # Resolve source paths explicitly so a pristine baseline can share the exact
    # pinned, read-only module and External libraries without copying repositories.
    for name in ("Source", "JuceLibraryCode", "External", "Modules", "Resources"):
        base = ROOT if name in ("External", "Modules", "Resources") else source_root
        project = project.replace("../../" + name, (base / name).as_posix())
        project = project.replace("..\\..\\" + name, str(base / name))
    tree = ET.fromstring(project)
    target_name = "Chataigne" if baseline else "MidnightNativePOC"
    output_name = "ChataigneBaseline" if baseline else "MidnightNative"
    for element in tree.findall(".//m:OutDir", NS):
        element.text = str(ROOT / "Binaries" / output_name) + "\\"
    for element in tree.findall(".//m:TargetName", NS):
        element.text = target_name
    for element in tree.findall(".//m:Link/m:OutputFile", NS):
        element.text = "$(OutDir)$(TargetName).exe"
    # Match upstream build_dev.bat: Release optimizations, without lengthy LTCG.
    for element in tree.findall(".//m:WholeProgramOptimization", NS):
        element.text = "false"
    for element in tree.findall(".//m:LinkTimeCodeGeneration", NS):
        element.text = "Default"
    for element in tree.findall(".//m:ClCompile/m:AdditionalOptions", NS):
        element.text = "/MP8 " + element.text
    ET.register_namespace("", NS["m"])
    target = folder / (target_name + ".vcxproj")
    ET.ElementTree(tree).write(target, encoding="utf-8", xml_declaration=True)
    return target


def toolchain_arguments(msbuild):
    installation = msbuild.parents[3]
    versions = sorted(p.name for p in (installation / "VC/Tools/MSVC").glob("14.4*")
                      if (p / "bin/Hostx64/x64/cl.exe").is_file())
    args = ["/p:VCToolsVersion=" + versions[-1]]
    platforms = installation / "MSBuild/Microsoft/VC/v180/Platforms/x64/PlatformToolsets"
    if (platforms / "v145/Toolset.props").is_file() and not (platforms / "v143/Toolset.props").is_file():
        # VS2026 can install the 14.44 compiler without the legacy platform
        # wrappers. Use its current MSBuild wrappers, retaining the v143 compiler.
        args.append("/p:PlatformToolset=v145")
    return args


def export_baseline():
    destination = ROOT / "Midnight/runtime/baseline"
    destination.mkdir(parents=True, exist_ok=True)
    archive = subprocess.check_output([
        "git", "archive", DEPENDENCIES["chataigne"],
        "Source", "JuceLibraryCode", "Builds/VisualStudio2022_CI",
    ], cwd=ROOT)
    with tarfile.open(fileobj=io.BytesIO(archive)) as bundle:
        bundle.extractall(destination, filter="data")
    return destination


def restore_packages(folder):
    """Restore only the versions recorded by the official Projucer exporter."""
    cache = ROOT / "Midnight/runtime"
    cache.mkdir(parents=True, exist_ok=True)
    for package in ET.parse(folder / "packages.config").getroot():
        name, version = package.attrib["id"], package.attrib["version"]
        destination = folder / "packages" / (name + "." + version)
        if (destination / "build/native" / (name + ".targets")).is_file():
            continue
        archive = cache / (name + "." + version + ".nupkg")
        if not archive.is_file():
            url = f"https://api.nuget.org/v3-flatcontainer/{name.lower()}/{version}/{name.lower()}.{version}.nupkg"
            with urlopen(url, timeout=60) as response, archive.with_suffix(".download").open("wb") as out:
                shutil.copyfileobj(response, out)
            archive.with_suffix(".download").replace(archive)
        with zipfile.ZipFile(archive) as bundle:
            for member in bundle.infolist():
                if not (destination / member.filename).resolve().is_relative_to(destination.resolve()):
                    raise ValueError("Unsafe path in NuGet archive")
            bundle.extractall(destination)


def stage_runtime(baseline=False):
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
    output = ROOT / "Binaries" / ("ChataigneBaseline" if baseline else "MidnightNative")
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
    parser.add_argument("--baseline", action="store_true", help="Build pristine recorded Chataigne source first")
    parser.add_argument(
        "--juce",
        type=Path,
        default=Path(os.environ.get("MIDNIGHT_JUCE", (ROOT / "Midnight/juce-path.txt").read_text().strip()
                     if (ROOT / "Midnight/juce-path.txt").is_file() else r"D:\Projects\Dev\JUCE")),
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
    source_root = export_baseline() if args.baseline else ROOT
    project = prepare_project(juce, ROOT / "Builds" / ("MidnightBaseline" if args.baseline else "MidnightNative"), source_root, args.baseline)
    restore_packages(project.parent)
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
    ] + toolchain_arguments(msbuild)
    result["command"] = command
    log_name = "baseline-build.log" if args.baseline else "native-build.log"
    print(f"Building {project.name}; progress: Midnight/evidence/{log_name}", flush=True)
    with (evidence / log_name).open("w") as log:
        code = subprocess.call(
            command, cwd=project.parent, stdout=log, stderr=subprocess.STDOUT
        )
    if code:
        print(f"Build failed; see Midnight/evidence/{log_name}")
        return code
    stage_runtime(args.baseline)
    result["compiled"] = True
    result["baseline"] = args.baseline
    (evidence / ("baseline-result.json" if args.baseline else "native-result.json")).write_text(json.dumps(result, indent=2) + "\n")
    print(
        "Executable built and official runtime pack staged. Runtime/persistence acceptance still required."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
