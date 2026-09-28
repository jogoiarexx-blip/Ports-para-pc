from pathlib import Path
import re,sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(".")
levels=root/"assets"/"data"/"levels.txt"
if not levels.exists():
    print("SKIP: levels.txt not present"); sys.exit(0)
lines=[]
for raw in levels.read_text(errors="ignore").splitlines():
    raw=raw.split("#",1)[0].strip()
    if raw: lines.append(raw)
stage=0
next_after=[]
scenes=[]
for line in lines:
    tok=line.split()
    cmd=tok[0].lower()
    if cmd=="file": stage+=1
    elif cmd=="next": next_after.append(stage)
    elif cmd=="scene" and len(tok)>1: scenes.append(tok[1].replace("\\","/").lower())
assert stage==21, stage
assert next_after==[3,7,9,12], next_after
assert scenes==["data/scenes/fin.txt","data/scenes/corte.txt","data/scenes/creditos.txt"], scenes
print(f"campaign_flow=OK stages={stage} next={next_after} scenes={len(scenes)}")
