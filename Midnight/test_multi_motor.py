"""Real worker/native document tests. No physical adapters; isolated application settings."""
import json
import os
import subprocess
from pathlib import Path
import test_native

ROOT = test_native.ROOT

def main():
    exe = test_native.compile_test('multi_motor_integration_test.cpp', 'multi_motor_integration_test')
    settings = ROOT / 'Midnight/runtime/multi-motor-settings'
    settings.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, APPDATA=str(settings), LOCALAPPDATA=str(settings))
    outcomes = {}
    for mode in ('create', 'reload'):
        result = subprocess.run([str(exe), mode, str(ROOT / 'Midnight/runtime/Multi_Motor_Acceptance.noisette')],
                                capture_output=True, text=True, timeout=120, env=env)
        (ROOT / f'Midnight/evidence/multi-motor-{mode}.txt').write_text(result.stdout + result.stderr)
        print((result.stdout + result.stderr) if result.returncode else result.stdout.strip().splitlines()[-1])
        result.check_returncode()
        outcomes[mode] = {'exit_code': result.returncode, 'checks': result.stdout.count('PASS ')}
    (ROOT / 'Midnight/evidence/multi-motor-integration.json').write_text(json.dumps(outcomes, indent=2) + '\n')

if __name__ == '__main__':
    main()
