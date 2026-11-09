"""Read-only preservation/source-build audit; writes its report in this worktree."""
from pathlib import Path
import hashlib,json,subprocess
root=Path(__file__).resolve().parents[1]
evidence=root/'Midnight/evidence/show-control'
def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for data in iter(lambda:f.read(1024*1024),b''):h.update(data)
 return h.hexdigest()
before=json.loads((evidence/'preservation-before.json').read_text())
changed=[];missing=[]
for name,expected in before['files'].items():
 p=Path(name)
 if not p.exists():missing.append(name)
 elif digest(p)!=expected:changed.append(name)
build=json.loads((root/'Midnight/evidence/show-control-build.json').read_text())
source_mismatches=[name for name,value in build['source_sha256'].items() if digest(root/name)!=value]
exe=root/'Binaries/MidnightShowControl/MidnightShowControl.exe'
result={'preserved_files':len(before['files']),'changed':changed,'missing':missing,'build_exit_code':build['exit_code'],'built_source_mismatches':source_mismatches,'executable':str(exe),'executable_sha256':digest(exe),'prompt_sha256':digest(evidence/'DISPATCHED_ASSIGNMENT.txt')}
for name,path in [('Integration',root.parent/'Chataigne_Midnight_Integration'),('Native_POC',root.parent/'Chataigne_Native_Midnight_POC'),('JUCE',Path('C:/Users/Simone/Dev/JUCE')),('Timeline',root.parent/'Chataigne_Native_Midnight_POC/Modules/juce_timeline'),('OrganicUI',root.parent/'Chataigne_Native_Midnight_POC/Modules/juce_organicui')]:
 def git(*args):return subprocess.check_output(['git','-c','safe.directory='+path.as_posix(),'-C',str(path),*args],text=True).strip()
 result[name]={'head':git('rev-parse','HEAD'),'status':git('status','--short')}
(evidence/'preservation-after.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('Integration','Native_POC')},indent=2))
assert not changed and not missing and not source_mismatches and build['exit_code']==0
assert result['prompt_sha256']==before['prompt_sha256']
