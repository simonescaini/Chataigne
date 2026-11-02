"""Build and run Midnight V0.1 production-core tests; no physical adapter is opened."""
import argparse
import json
import subprocess
from pathlib import Path
import build

ROOT = build.ROOT

def core():
    msbuild, issues = build.preflight(Path((ROOT / 'Midnight/juce-path.txt').read_text().strip()))
    if issues:
        raise RuntimeError(issues)
    dest = ROOT / 'Builds/MidnightNative/v01-core'
    dest.mkdir(parents=True, exist_ok=True)
    exe = ROOT / 'Binaries/MidnightNative/v01_core_test.exe'
    source = ROOT / 'Midnight/v01_core_test.cpp'
    rsp = dest / 'compile.rsp'
    rsp.write_text('\n'.join(['/nologo', '/std:c++17', '/EHsc', '/O2', '/W4',
        '/Fe"' + str(exe) + '"', '/Fo"' + str(dest / 'core.obj') + '"', '"' + str(source) + '"']))
    vcvars = msbuild.parents[3] / 'VC/Auxiliary/Build/vcvars64.bat'
    command = dest / 'compile.cmd'
    command.write_text('@echo off\ncall "' + str(vcvars) + '" -vcvars_ver=14.44 >nul\n'
        'if errorlevel 1 exit /b %errorlevel%\ncl @"' + str(rsp) + '"\nexit /b %errorlevel%\n')
    subprocess.run(['cmd.exe', '/d', '/c', str(command)], cwd=dest, check=True)
    evidence = ROOT / 'Midnight/evidence'
    trace = evidence / 'v01-reference-packets.txt'
    result = subprocess.run([str(exe), str(trace)], capture_output=True, text=True)
    (evidence / 'v01-core.txt').write_text(result.stdout + result.stderr)
    print((result.stdout + result.stderr) if result.returncode else result.stdout.strip().splitlines()[-1])
    result.check_returncode()
    return {'exit_code': result.returncode, 'checks': result.stdout.count('PASS ')}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--integration', action='store_true')
    args = parser.parse_args()
    outcomes = {'core': core()}
    if args.integration:
        import test_native
        exe = test_native.compile_test('v01_integration_test.cpp', 'v01_integration_test')
        for mode in ('create', 'reload'):
            result = subprocess.run([str(exe), mode, str(ROOT / 'Midnight/runtime/V01_Acceptance.noisette')],
                capture_output=True, text=True, timeout=120)
            (ROOT / f'Midnight/evidence/v01-{mode}.txt').write_text(result.stdout + result.stderr)
            print((result.stdout + result.stderr) if result.returncode else result.stdout.strip().splitlines()[-1])
            result.check_returncode()
            outcomes[mode] = {'exit_code': result.returncode, 'checks': result.stdout.count('PASS ')}
    (ROOT / 'Midnight/evidence/v01-acceptance.json').write_text(json.dumps(outcomes, indent=2) + '\n')

if __name__ == '__main__':
    main()
