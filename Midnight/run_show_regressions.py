"""Final isolated software suites. Never opens physical motor adapters."""
from pathlib import Path
import concurrent.futures,json,subprocess,sys,time
root=Path(__file__).resolve().parents[1]
evidence=root/'Midnight/evidence/show-control/final';evidence.mkdir(parents=True,exist_ok=True)
suites=[('native',['Midnight/test_native.py']),('v01',['Midnight/test_v01.py','--integration']),('multi-motor',['Midnight/test_multi_motor.py']),('integration',['Midnight/test_chataigne_integration.py']),('range',['Midnight/test_integration_range.py']),('curve',['Midnight/test_show_curve.py']),('recording',['Midnight/test_recording.py']),('coordinator',['Midnight/test_show_coordinator.py']),('mapping',['Midnight/test_show_mapping.py']),('package',['Midnight/test_show_package.py']),('music',['Midnight/test_show_music.py']),('audio',['Midnight/test_show_audio.py']),('example',['Midnight/test_show_example.py']),('python',['-m','unittest','discover','-s','Midnight/tests','-v'])]
def run(suite):
 name,args=suite;started=time.monotonic()
 with (evidence/(name+'.txt')).open('w',encoding='utf-8') as log:
  p=subprocess.run([sys.executable,*args],cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=900)
 result=dict(exit_code=p.returncode,seconds=round(time.monotonic()-started,2))
 print(name,result,flush=True);return name,result
results={}
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
 for name,result in pool.map(run,suites):
  results[name]=result;(evidence/'results.json').write_text(json.dumps(results,indent=2))
name,result=run(('reference-parity',['Midnight/compare_reference.py']));results[name]=result
(evidence/'results.json').write_text(json.dumps(results,indent=2))
sys.exit(any(result['exit_code'] for result in results.values()))
