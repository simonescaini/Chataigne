"""Isolated local integration build. Dependencies are read-only; no JUCE provisioning."""
import argparse, hashlib, json, shutil, subprocess
from pathlib import Path
import build
ROOT = Path(__file__).resolve().parent.parent
ORIGINAL = ROOT.parent / "Chataigne_Native_Midnight_POC"

def prepare(juce):
    folder = ROOT / "Builds/MidnightShowControl"
    project = build.prepare_project(juce, folder)
    content = project.read_text()
    for name in ("Modules", "External"):
        content = content.replace(str(ROOT/name), str(ORIGINAL/name)).replace((ROOT/name).as_posix(), (ORIGINAL/name).as_posix())
    content = content.replace("MidnightNativePOC", "MidnightShowControl").replace(str(ROOT/"Binaries/MidnightNative"), str(ROOT/"Binaries/MidnightShowControl"))
    project = folder/"MidnightShowControl.vcxproj"
    if not project.exists() or project.read_text()!=content: project.write_text(content)
    packages = ORIGINAL/"Builds/MidnightNative/packages"
    if packages.exists(): shutil.copytree(packages, folder/"packages", dirs_exist_ok=True)
    else: build.restore_packages(folder)
    return project

def source_manifest():
    files={}
    for folder in ("Source","JuceLibraryCode"):
        for path in sorted((ROOT/folder).rglob("*")):
            if path.is_file() and path.suffix.lower() in (".cpp",".h",".hpp",".c"):
                files[path.relative_to(ROOT).as_posix()]=hashlib.file_digest(path.open("rb"),"sha256").hexdigest()
    return files

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--juce",type=Path,default=Path((ROOT/"Midnight/juce-path.txt").read_text().strip()) if (ROOT/"Midnight/juce-path.txt").exists() else Path(r"D:\Projects\Dev\JUCE"))
    parser.add_argument("--check",action="store_true")
    args=parser.parse_args()
    assert ROOT != ORIGINAL and (ROOT/".git").is_file(), "Separate worktree required"
    msbuild,issues=build.preflight(args.juce)
    if issues: raise SystemExit("\n".join(issues))
    if args.check: print("Preflight passed; no build performed."); return
    project=prepare(args.juce)
    output=ROOT/"Binaries/MidnightShowControl"
    output.mkdir(parents=True,exist_ok=True)
    # Copy runtime dependencies; never overwrite or rebuild the milestone.
    for item in (ORIGINAL/"Binaries/MidnightNative").iterdir():
        if item.is_file() and item.suffix.lower() in (".dll", ".dat", ".pak", ".bin"):
            shutil.copy2(item, output/item.name)
    for name in ("locales","swiftshader"):
        if (ORIGINAL/"Binaries/MidnightNative"/name).is_dir():
            shutil.copytree(ORIGINAL/"Binaries/MidnightNative"/name,output/name,dirs_exist_ok=True)
    command=[str(msbuild),str(project),"/t:Build","/p:Configuration=Release","/p:Platform=x64","/p:PreferredToolArchitecture=x64","/m:2","/v:minimal","/nologo"]+build.toolchain_arguments(msbuild)
    evidence=ROOT/"Midnight/evidence"
    print("Building Show Control; see Midnight/evidence/show-control-build.log",flush=True)
    source_before=source_manifest()
    with (evidence/"show-control-build.log").open("w") as log:
        result=subprocess.run(command,cwd=project.parent,stdout=log,stderr=subprocess.STDOUT)
    report={"command":command,"exit_code":result.returncode,"juce":str(args.juce),"source_sha256":source_before,"sources_unchanged_during_build":source_before==source_manifest()}
    exe=output/"MidnightShowControl.exe"
    if result.returncode==0:
        report["source_commit"]=subprocess.check_output(["git","-c","safe.directory="+ROOT.as_posix(),"-C",str(ROOT),"rev-parse","HEAD"],text=True).strip()
        report["executable"]=str(exe)
        report["sha256"]=hashlib.file_digest(exe.open("rb"),"sha256").hexdigest()
    (evidence/"show-control-build.json").write_text(json.dumps(report,indent=2)+"\n")
    raise SystemExit(result.returncode)
if __name__=="__main__": main()
