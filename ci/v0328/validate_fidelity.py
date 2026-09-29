from pathlib import Path
import sys,re
src=Path(sys.argv[1]) if len(sys.argv)>1 else Path(".")
asset=Path(sys.argv[2]) if len(sys.argv)>2 else Path(".")

h=(src/"src/OpenBorData.h").read_text()
d=(src/"src/OpenBorData.cpp").read_text()
g=(src/"src/Game.cpp").read_text()

checks={
 "remove_default":"score=0, remove=1;" in h,
 "throwframewait_model":"int throwFrameWait=-1;" in h and 'e.throwFrameWait=toInt(t[1])' in d,
 "throwframe_direct":'pushEvent(cur,"throw",toInt(t[1])' in d,
 "remove_parser":'v=="none"?0:v=="hit"?1:toInt(t[1],1)' in d,
 "namej_parser":'+"namej"' in d and "hcfg.join.prompt.x" in d,
 "lifex_parser":'+"lifex"' in d and "hcfg.lifeX.x" in d,
 "join_prompt":'hudText(L"PRESS START",h.join.prompt' in g,
 "lives_x":'hudText(L"x",h.lifeX' in g,
 "zero_force_327":'const bool contactOnly=raw==0;' in g and 'heldTarget&&f->attack.knockdown>0' in g,
}
bad=[k for k,v in checks.items() if not v]
if bad: raise SystemExit("source checks failed: "+",".join(bad))

levels=asset/"assets/data/levels.txt"
if levels.exists():
    rows={}
    for raw in levels.read_text(errors="ignore").splitlines():
        t=raw.split("#",1)[0].split()
        if t: rows[t[0].lower()]=t[1:]
    expected={"p1namej":["23","12","5","1","5","9"],"p2namej":["116","12","98","1","98","9"],"p3namej":["243","12","225","1","225","9"],"p1lifex":["-50","-100"],"p2lifex":["-50","-100"],"p3lifex":["-50","-100"]}
    for k,v in expected.items():
        if rows.get(k)!=v: raise SystemExit(f"{k} mismatch: {rows.get(k)}")
    print("legacy_hud_assets=OK")
else:
    print("legacy_hud_assets=SKIP")

print("v0328_source_semantics=OK")
