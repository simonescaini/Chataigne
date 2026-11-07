"""Compile actual native Runtime range regression; never opens an adapter."""
import subprocess
from pathlib import Path
import build
ROOT=build.ROOT
def main():
 msbuild,issues=build.preflight(Path((ROOT/"Midnight/juce-path.txt").read_text().strip()))
 if issues: raise RuntimeError(issues)
 dest=ROOT/"Builds/MidnightIntegration/range"
 dest.mkdir(parents=True,exist_ok=True)
 exe=ROOT/"Binaries/MidnightIntegration/integration_range_test.exe"
 source=ROOT/"Midnight/integration_range_test.cpp"
 command=dest/"compile.cmd"
 vcvars=msbuild.parents[3]/"VC/Auxiliary/Build/vcvars64.bat"
 command.write_text('@echo off\ncall "'+str(vcvars)+'" -vcvars_ver=14.44 >nul\nif errorlevel 1 exit /b %errorlevel%\ncl /nologo /std:c++17 /EHsc /O2 /Fe"'+str(exe)+'" /Fo"'+str(dest/"range.obj")+'" "'+str(source)+'"\nexit /b %errorlevel%\n')
 subprocess.run(["cmd.exe","/d","/c",str(command)],cwd=dest,check=True)
 result=subprocess.run([str(exe)],capture_output=True,text=True)
 (ROOT/"Midnight/evidence/integration-range.txt").write_text(result.stdout+result.stderr)
 result.check_returncode()
 print(result.stdout)
if __name__=="__main__":main()
