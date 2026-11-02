"""Link the real application objects into a console acceptance test; run two processes.

No replacement engine, mocked timeline, or Python-origin control messages.
"""
import json
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

import build

ROOT = build.ROOT


def compile_test():
    project_dir = ROOT / 'Builds/MidnightNative'
    project = ET.parse(project_dir / 'MidnightNativePOC.vcxproj')
    compile = next(g for g in project.findall('m:ItemDefinitionGroup', build.NS)
                   if 'Release|x64' in g.attrib.get('Condition', '')).find('m:ClCompile', build.NS)
    destination = project_dir / 'acceptance'
    destination.mkdir(exist_ok=True)
    args = ['/nologo', '/c', '/std:c++17', '/EHsc', '/MD', '/bigobj', '/Zi',
            '/Fo"' + str(destination / 'native_integration_test.obj') + '"',
            '/Fd"' + str(destination / 'compile.pdb') + '"']
    for tag, flag in [('AdditionalIncludeDirectories', '/I'), ('PreprocessorDefinitions', '/D')]:
        for item in compile.find('m:' + tag, build.NS).text.split(';'):
            if not item.startswith('%('):
                args.append(flag + '"' + item + '"')
    args.append('"' + str(ROOT / 'Midnight/native_integration_test.cpp') + '"')
    (destination / 'compile.rsp').write_text('\n'.join(args))
    log = next(project_dir.glob('x64/Release/App/*.tlog/link.command.1.tlog')).read_text(encoding='utf-16')
    link_args = next(line for line in log.splitlines() if line.startswith('/OUT:'))
    objects = next(line for line in log.splitlines() if line.startswith('^'))[1:].split('|')
    executable = ROOT / 'Binaries/MidnightNative/native_integration_test.exe'
    for option, path in [('OUT', executable), ('PDB', destination / 'test.pdb'),
                         ('IMPLIB', destination / 'test.lib'), ('LTCGOUT', destination / 'test.iobj')]:
        link_args = re.sub('/' + option + ':"[^"]+"', lambda _: '/' + option + ':"' + str(path) + '"', link_args)
    link_args = link_args.replace('/SUBSYSTEM:WINDOWS', '/SUBSYSTEM:CONSOLE')
    link_args += '\n' + '\n'.join('"' + obj + '"' for obj in objects if obj.upper().endswith('.OBJ'))
    link_args += '\n"' + str(destination / 'native_integration_test.obj') + '"\n'
    (destination / 'link.rsp').write_text(link_args)
    msbuild, issues = build.preflight(Path((ROOT / 'Midnight/juce-path.txt').read_text().strip()))
    if issues:
        raise RuntimeError(issues)
    vcvars = msbuild.parents[3] / 'VC/Auxiliary/Build/vcvars64.bat'
    command = destination / 'compile.cmd'
    command.write_text('@echo off\ncall "' + str(vcvars) + '" -vcvars_ver=14.44 >nul\n'
                       'if errorlevel 1 exit /b %errorlevel%\n'
                       'cl @"' + str(destination / 'compile.rsp') + '"\n'
                       'if errorlevel 1 exit /b %errorlevel%\n'
                       'link @"' + str(destination / 'link.rsp') + '"\nexit /b %errorlevel%\n')
    subprocess.run(['cmd.exe', '/d', '/c', str(command)], cwd=project_dir, check=True)
    return executable


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--skip-build', action='store_true')
    args = parser.parse_args()
    exe = ROOT / 'Binaries/MidnightNative/native_integration_test.exe' if args.skip_build else compile_test()
    evidence = ROOT / 'Midnight/evidence'
    project = ROOT / 'Midnight/runtime/Midnight_Acceptance.noisette'
    outcomes = {}
    for mode in ('create', 'reload'):
        receiver_log = evidence / ('native-osc-' + mode + '.jsonl')
        receiver_log.write_text('')
        receiver = subprocess.Popen([sys.executable, '-u', str(ROOT / 'Midnight/sim_receiver.py'),
                                     '--log', str(receiver_log)], cwd=ROOT,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        try:
            time.sleep(.3)
            if receiver.poll() is not None:
                raise RuntimeError(receiver.stderr.read().decode())
            with (evidence / ('native-' + mode + '.txt')).open('w') as log:
                result = subprocess.run([str(exe), mode, str(project)], cwd=ROOT,
                                        stdout=log, stderr=subprocess.STDOUT, timeout=90)
            outcomes[mode] = {'exit_code': result.returncode}
            if result.returncode:
                raise RuntimeError('Native ' + mode + ' failed; see evidence log')
        finally:
            receiver.terminate()
            receiver.wait(timeout=5)
        messages = [json.loads(line) for line in receiver_log.read_text().splitlines()]
        received = [m for m in messages if m.get('kind') == 'rx']
        moves = [m['args'][1] for m in received if m['address'] == '/midnight/sim/move']
        outcomes[mode]['move_targets'] = moves
        expected = [0, 45, 90, 0] + ([22] if mode == 'create' else [])
        if moves != expected:
            raise RuntimeError(f'Expected exactly the native authored OSC moves {expected}, got {moves}')
        outcomes[mode]['received_commands'] = len(received)
        outcomes[mode]['acknowledgements'] = sum(m.get('address') == '/midnight/sim/ack' for m in messages)
        if outcomes[mode]['acknowledgements'] != len(received):
            raise RuntimeError('A native OSC command was not acknowledged')
        print(mode, outcomes[mode])
    (evidence / 'native-acceptance.json').write_text(json.dumps(outcomes, indent=2) + '\n')


if __name__ == '__main__':
    main()
