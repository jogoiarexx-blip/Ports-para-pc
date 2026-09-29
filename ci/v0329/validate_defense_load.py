from pathlib import Path
import sys

src=Path(sys.argv[1])
asset=Path(sys.argv[2]) if len(sys.argv)>2 else None
h=(src/"src/OpenBorData.h").read_text(errors="ignore")
d=(src/"src/OpenBorData.cpp").read_text(errors="ignore")
g=(src/"src/Game.cpp").read_text(errors="ignore")

checks={
 "defense_all_missing_zero": 'e.defenseAll=t.size()>2?toFloat(t[2],0.f):0.f;' in d,
 "model_load_storage": "modelLoads" in h,
 "model_load_parser": 'cmd=="load"&&t.size()>1' in d,
 "defense_runtime": "raw*t.def->defenseAll" in g,
 "zero_force_preserved": "const bool contactOnly=raw==0" in g,
 "v0328_remove": "score=0, remove=1;" in h,
 "v0328_join_hud": "HudJoinLayout" in h,
}
bad=[k for k,v in checks.items() if not v]
if bad: raise SystemExit("source checks failed: "+",".join(bad))

if asset and (asset/"assets/data/chars").exists():
    data=asset/"assets/data"
    defense=[]
    loads=[]
    for p in data.rglob("*.txt"):
        try: lines=p.read_text(errors="ignore").splitlines()
        except: continue
        in_anim=False
        for n,raw in enumerate(lines,1):
            t=raw.split("#",1)[0].split()
            if not t: continue
            cmd=t[0].lower()
            if cmd=="anim": in_anim=True
            if not in_anim and cmd=="defense" and len(t)==2 and t[1].lower()=="all":
                defense.append((str(p.relative_to(data)),n))
            if not in_anim and cmd=="load" and len(t)>1:
                loads.append((str(p.relative_to(data)),n,t[1].lower()))
    assert len(defense)==3, defense
    assert len(loads)==9, loads
    print(f"defense_load_assets=OK defense_all_missing={len(defense)} model_loads={len(loads)}")
else:
    print("defense_load_assets=SKIP complete character assets unavailable in CI snapshot")

print("v0329_source_semantics=OK")
