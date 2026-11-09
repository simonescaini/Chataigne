import os,subprocess
import test_native
r=test_native.ROOT
exe=test_native.compile_test('show_coordinator_test.cpp','show_coordinator_test')
settings=r/'Midnight/runtime/show-coordinator-settings';settings.mkdir(parents=True,exist_ok=True)
p=subprocess.run([str(exe)],env=dict(os.environ,APPDATA=str(settings),LOCALAPPDATA=str(settings)),capture_output=True,text=True,timeout=90)
(r/'Midnight/evidence/show-control/coordinator-test.txt').write_text(p.stdout+p.stderr)
print((p.stdout+p.stderr)[-7000:]);p.check_returncode()
