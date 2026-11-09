import os,subprocess,json
import test_native
from generate_show_audio import generate
r=test_native.ROOT
exe=test_native.compile_test('show_audio_test.cpp','show_audio_test')
audio=generate(r/'Midnight/runtime/show-control-assets/Demonstration.wav')
results={}
for mode in ('offline','device'):
    settings=r/('Midnight/runtime/show-audio-'+mode+'-settings');settings.mkdir(parents=True,exist_ok=True)
    p=subprocess.run([str(exe),str(audio),mode],env=dict(os.environ,APPDATA=str(settings),LOCALAPPDATA=str(settings)),capture_output=True,text=True,timeout=60)
    (r/('Midnight/evidence/show-control/audio-'+mode+'.txt')).write_text(p.stdout+p.stderr)
    print(p.stdout+p.stderr);results[mode]=p.returncode
    if p.returncode not in (0,2):p.check_returncode()
(r/'Midnight/evidence/show-control/audio-results.json').write_text(json.dumps(results,indent=2))
