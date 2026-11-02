"""Compare C++ emitted packets with the pinned original class on a fake bus.

Only the RobStrideMotor AST class is evaluated. No module-level controller code,
serial connection, OSC listener, or physical operation is executed.
"""
import ast
import hashlib
import math
import struct
import subprocess
from types import SimpleNamespace
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent
EXPECTED = '0dfe47ce2d90337498e207ee72c7ee88f029ddc9f2097cfe2150cc03232a73a9'

class Message:
    def __init__(self, arbitration_id, data, is_extended_id=True, **kwargs):
        self.arbitration_id, self.data = arbitration_id, bytes(data)

class FakeBus:
    def __init__(self):
        self.sent = []
    def send(self, message):
        self.sent.append(message)

def main():
    path = ROOT.parent / 'Midnight_orchestrator/Reference_controller/main_canable_dual_haptic_working.py'
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED:
        raise RuntimeError('Reference controller does not match pinned SHA-256')
    tree = ast.parse(data.decode('utf-8'))
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'RobStrideMotor')
    namespace = dict(math=math, struct=struct, dataclass=dataclass, Optional=Optional,
        can=type('FakeCan', (), {'Message': Message, 'Bus': FakeBus}),
        time=type('Clock', (), {'sleep': staticmethod(lambda _: None), 'time': staticmethod(lambda: 1.0)})(),
        RS_DEFAULT_CUR_LIMIT_A=3.0, RS_DEFAULT_SPD_LIMIT_RS=1.0,
        RS_MAX_CUR_A=23.0, RS_MAX_SPD_RS=44.0)
    exec(compile(ast.Module(body=[cls], type_ignores=[]), str(path), 'exec'), namespace)
    motor = namespace['RobStrideMotor'](126, 254, position_zero=0.0)
    bus = FakeBus()
    motor.move_to_pos(bus, math.degrees(1.5), math.degrees(1.0))
    reference = [f'T{f.arbitration_id:08X}8{f.data.hex().upper()}' for f in bus.sent]
    native = [x for x in (ROOT / 'Midnight/evidence/v01-reference-packets.txt').read_text().splitlines() if x]
    assert native[0] == 'T0400FE7E80000000000000000', native[0]
    assert native[1:] == reference, (native, reference)
    v2 = ROOT.parent / 'Midnight_Motion_Lab_V2'
    revision = subprocess.check_output(['git', '-c', f'safe.directory={v2.as_posix()}', '-C', str(v2), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != '7eb07769f6a1f199526c2324f4c9108d99d2ed7b':
        raise RuntimeError('V2 reference is not at the requested checkpoint')
    pure = dict(math=math, struct=struct, can=namespace['can'])
    # Evaluate only constants and pure functions, never reference startup or devices.
    for name in ('protocol.py', 'haptics.py'):
        source = v2 / 'src/midnight_motion_lab_v2' / name
        parsed = ast.parse(source.read_text(encoding='utf-8'))
        nodes = [node for node in parsed.body if isinstance(node, (ast.Assign, ast.FunctionDef))]
        exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), pure)
    compared = 0
    for line in (ROOT / 'Midnight/evidence/v01-reference-packets.txt.parity').read_text().splitlines():
        if not line:
            continue
        parts = line.split('|')
        if parts[0] == 'HAPTIC':
            a, b = int(parts[1]), int(parts[2])
            definition = SimpleNamespace(sign_a=a, sign_b=b, stiffness=2, damping=.5, deadband=.2, torque=1)
            result = pure['spring_damper'](definition, *map(float, parts[3:8]))
            assert all(abs(x - float(y)) < 1e-12 for x, y in zip(result, parts[8:])), parts
        else:
            frame = (pure['torque'](126, float(parts[1])) if parts[0] == 'TORQUE'
                     else pure['write'](126, 254, int(parts[2]), float(parts[3]), parts[1]))
            assert parts[-1] == f'T{frame.arbitration_id:08X}8{frame.data.hex().upper()}', parts
        compared += 1
    result = (f'PASS exact CSP parity: documented stop + {len(reference)} packets from pinned original RobStrideMotor class\n'
              f'PASS {compared} additional RS01/RS02 parameter, Type-1 torque and spring/damper differential cases against pinned V2\n'
              f'Reference SHA-256: {EXPECTED}\nV2 checkpoint: {revision}\nNo reference module startup or hardware code executed.\n')
    (ROOT / 'Midnight/evidence/v01-reference-parity.txt').write_text(result)
    print(result)

if __name__ == '__main__':
    main()
