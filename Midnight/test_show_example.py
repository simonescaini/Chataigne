import os, subprocess, json
import test_native
from generate_show_audio import generate
root=test_native.ROOT
generate(root/'Midnight/runtime/show-control-assets/Demonstration.wav')
exe=test_native.compile_test('show_example_test.cpp','show_example_test')
results={}
for mode in ('create','reload'):
    settings=root/('Midnight/runtime/show-example-'+mode+'-settings');settings.mkdir(parents=True,exist_ok=True)
    p=subprocess.run([str(exe),mode,str(root)],env=dict(os.environ,APPDATA=str(settings),LOCALAPPDATA=str(settings)),capture_output=True,text=True,timeout=120)
    (root/('Midnight/evidence/show-control/example-'+mode+'.txt')).write_text(p.stdout+p.stderr)
    print(p.stdout+p.stderr,flush=True);results[mode]={'exit_code':p.returncode,'checks':p.stdout.count('PASS ')};p.check_returncode()
(root/'Midnight/evidence/show-control/example-results.json').write_text(json.dumps(results,indent=2))
