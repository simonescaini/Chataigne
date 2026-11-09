"""Real application-object acceptance in separate processes, all adapters fake."""
import json,os,subprocess
from pathlib import Path
import test_native
ROOT=test_native.ROOT
def main():
 exe=test_native.compile_test("chataigne_integration_test.cpp","chataigne_integration_test")
 settings=ROOT/"Midnight/runtime/integration-test-settings"
 settings.mkdir(parents=True,exist_ok=True)
 env=dict(os.environ,APPDATA=str(settings),LOCALAPPDATA=str(settings))
 outcomes={}
 for mode in ("create","reload"):
  with (ROOT/f"Midnight/evidence/integration-{mode}.txt").open("w") as log:
   result=subprocess.run([str(exe),mode,str(ROOT/"Midnight/runtime/Integration_Acceptance.noisette")],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
  content=(ROOT/f"Midnight/evidence/integration-{mode}.txt").read_text()
  print(content[-2500:],flush=True)
  outcomes[mode]={"exit_code":result.returncode,"checks":content.count("PASS ")}
  (ROOT/"Midnight/evidence/integration-acceptance.json").write_text(json.dumps(outcomes,indent=2))
  result.check_returncode()

 # Preserve source copies too: only generated compatibility derivatives change adapters.
 copies=sorted((ROOT/"Midnight/runtime/preserved-milestone/project_file").rglob("*.noisette"))
 outcomes["compatibility"]=[]
 for i,source in enumerate(copies):
  data=json.loads(source.read_text())
  for adapter in data.get("midnightNative",{}).get("adapters",[]):
   adapter.update(simulated=True,port="",identity="")
  # Copied-show compatibility never opens the original physical MIDI endpoints.
  for module in data.get("modules",{}).get("items",[]):
   if module.get("type")=="MIDI":
    for parameter in module.get("params",{}).get("parameters",[]):
     if parameter.get("controlAddress")=="/devices":
      parameter.update(value=["",""],inputName="",outputName="")
  copy=ROOT/f"Midnight/runtime/compatibility_{i}.noisette"
  copy.write_text(json.dumps(data))
  result=subprocess.run([str(exe),"compatibility",str(copy)],env=env,capture_output=True,text=True,timeout=90)
  (ROOT/f"Midnight/evidence/compatibility-{i}.txt").write_text(result.stdout+result.stderr)
  result.check_returncode()
  outcomes["compatibility"].append({"source":str(source.relative_to(ROOT)),"checks":result.stdout.count("PASS "),"exit_code":0})
 print("Compatibility copies passed:",len(copies),flush=True)
 (ROOT/"Midnight/evidence/integration-acceptance.json").write_text(json.dumps(outcomes,indent=2))
if __name__=="__main__":main()
