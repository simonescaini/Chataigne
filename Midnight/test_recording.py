import os,subprocess
import test_native
r=test_native.ROOT
exe=test_native.compile_test('recording_test.cpp','recording_test')
settings=r/'Midnight/runtime/recording-test-settings';settings.mkdir(parents=True,exist_ok=True)
p=subprocess.run([str(exe)],env=dict(os.environ,APPDATA=str(settings),LOCALAPPDATA=str(settings)),capture_output=True,text=True,timeout=60)
(r/'Midnight/evidence/show-control/recording-test.txt').write_text(p.stdout+p.stderr)
print((p.stdout+p.stderr)[-4000:]);p.check_returncode()
