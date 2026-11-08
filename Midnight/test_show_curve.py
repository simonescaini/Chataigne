import os,subprocess
from pathlib import Path
import test_native
r=test_native.ROOT
exe=test_native.compile_test('show_curve_test.cpp','show_curve_test')
settings=r/'Midnight/runtime/curve-test-settings';settings.mkdir(parents=True,exist_ok=True)
p=subprocess.run([str(exe),str(r/'Midnight/evidence/show-control/curve-small.png')],env=dict(os.environ,APPDATA=str(settings),LOCALAPPDATA=str(settings)),capture_output=True,text=True,timeout=60)
(r/'Midnight/evidence/show-control/curve-test.txt').write_text(p.stdout+p.stderr)
print((p.stdout+p.stderr)[-2400:]);p.check_returncode()
