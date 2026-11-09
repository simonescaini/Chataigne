"""Start only the delivered executable against a copied fake-only show; no UI input."""
import hashlib,json,os,subprocess,time,re,xml.etree.ElementTree as ET
from pathlib import Path
root=Path(__file__).resolve().parents[1]
settings=root/'Midnight/runtime/executable-smoke-settings';settings.mkdir(parents=True,exist_ok=True)
source=root/'Midnight/examples/Show_Control_Integrated.noisette';data=json.loads(source.read_text(encoding='utf-8-sig'))
assert all(a.get('simulated') and not a.get('port') for a in data['midnightNative']['adapters'])
copy=root/'Midnight/runtime/Executable_Smoke.noisette';copy.write_bytes(source.read_bytes())
exe=root/'Binaries/MidnightShowControl/MidnightShowControl.exe'
# Seed only the new product namespace, as the native UI fixtures do, so an
# upstream web Welcome dialog cannot obscure the startup window observation.
prefs=Path(os.environ['APPDATA'])/'MIDNIGHT SHOW CONTROL/MIDNIGHT SHOW CONTROL.settings'
assert prefs.parent.name=='MIDNIGHT SHOW CONTROL'
tree=ET.parse(prefs) if prefs.exists() else ET.ElementTree(ET.Element('PROPERTIES'))
version=re.search(r'versionString\s*=\s*"([^"]+)"',(root/'JuceLibraryCode/JuceHeader.h').read_text()).group(1)
def setting(name,value):
 element=tree.getroot().find("VALUE[@name='"+name+"']")
 if element is None:element=ET.SubElement(tree.getroot(),'VALUE',name=name)
 element.set('val',value)
setting('lastVersion',version);prefs.parent.mkdir(parents=True,exist_ok=True);tree.write(prefs,encoding='utf-8',xml_declaration=True)
startup=subprocess.STARTUPINFO();startup.dwFlags=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
p=subprocess.Popen([str(exe),str(copy)],cwd=exe.parent,env=dict(os.environ,APPDATA=str(settings),LOCALAPPDATA=str(settings)),startupinfo=startup)
try:
 time.sleep(6)
 assert p.poll() is None,'Delivered executable exited during startup'
 cmd=f'Get-Process -Id {p.pid} | Select-Object Id,ProcessName,MainWindowTitle,Responding | ConvertTo-Json -Compress'
 state=json.loads(subprocess.check_output(['powershell','-NoProfile','-Command',cmd],text=True))
 assert state['Responding'] and ('MIDNIGHT SHOW CONTROL' in state['MainWindowTitle'] and 'Executable_Smoke' in state['MainWindowTitle']),'Delivered project window was not the responding primary window: '+str(state)
 result={'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'copied_fake_only_show':str(copy),'requested_APPDATA_override':str(settings),'application_settings_namespace':str(Path(os.environ['APPDATA'])/'MIDNIGHT SHOW CONTROL'),'process':state,'startup':'PASS','ui_input':'NONE; desktop automation helper unavailable','cleanup':'Terminated only this disposable test process; native graceful teardown is covered by application fixtures'}
 (root/'Midnight/evidence/show-control/executable-startup.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:
 if p.poll() is None:p.terminate()
 p.wait(timeout=10)
 tree=ET.parse(prefs);setting('recentFiles',str(source));tree.write(prefs,encoding='utf-8',xml_declaration=True)
