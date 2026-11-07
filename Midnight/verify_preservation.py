"""Read-only verification of the preserved Native POC checkpoint."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
def main():
 baseline=json.loads((ROOT/"Midnight/evidence/integration-baseline.json").read_text())
 changed=[]
 for name,expected in baseline["files"].items():
  path=Path(name)
  actual=hashlib.file_digest(path.open("rb"),"sha256").hexdigest() if path.is_file() else None
  if actual!=expected: changed.append({"path":name,"expected":expected,"actual":actual})
 report={"checked":len(baseline["files"]),"changes":changed}
 print(json.dumps(report,indent=2))
 if changed:raise SystemExit(1)
if __name__=="__main__":main()
