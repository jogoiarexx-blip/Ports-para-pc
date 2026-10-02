from pathlib import Path
import sys,re

src=Path(sys.argv[1])
portable=Path(sys.argv[2]) if len(sys.argv)>2 else None

def read(rel):
    return (src/rel).read_text(encoding="utf-8-sig")

cm=read("CMakeLists.txt")
mw=read("src/main_win.cpp")
g=read("src/Game.cpp")
d=read("src/OpenBorData.cpp")
dh=read("src/OpenBorData.h")
sem=read("src/OpenBorSemantics.h")

checks={
 "version-cmake":"VERSION 0.3.33" in cm,
 "version-window":"Native C++ Port v0.3.33" in mw,
 "version-menu":"NATIVE PORT v0.3.33" in g,
 "landframe-exact":'pushEvent(cur,"land",std::max(0,toInt(t[1]))' in d and 'toInt(t[1])-1' not in d,
 "landframe-no-rewind":'(int)a.frame<e.frame' in g,
 "landframe-animation-hold":'bool authoredLand=false' in g and '!authoredLand' in g,
 "blockodds-build3797":'return (seed & (std::uint32_t)blockOdds)==1u;' in sem,
 "blockodds-modern-modulo-removed":'seed%(std::uint32_t)blockOdds' not in sem,
 "jumpheight-default":'jumpHeight=4.f' in dh,
 "jumpheight-parser":'cmd=="jumpheight"' in d,
 "toss-altitude-only":'queueProjectile(a,m,"toss",h,0.f)' in g,
 "toss-model-jumpheight":'a->def->jumpHeight)*42.f' in g,
 "toss-no-arrow-removal":'a->removeOnHit=false' in g,
 "toss-platform-floor":'if(a.projectileArc){float floor=platformFloor(a);' in g,
 "toss-impact-explosion":'att.projectile&&att.projectileArc&&!att.projectileExploding' in g,
 "noatflash-victim-fallback":'resolvedHitFlash=t.def->defaultFlash' in g,
 "flipframe-exact-retained":'pushEvent(cur,"flip",t.size()>1?toInt(t[1]):0)' in d,
}
if portable and (portable/"assets/data/chars").exists():
    data=portable/"assets/data"
    counts={"landframe":0,"flipframe":0,"jumpframe":0,"noatflash":0,"throwframe":0,"blockodds":0,"tossframe":0,"shootframe":0}
    for p in (data/"chars").rglob("*.txt"):
        try: text=p.read_text(encoding="latin-1")
        except Exception: continue
        for line in text.splitlines():
            line=line.split("#",1)[0].strip()
            if not line: continue
            cmd=line.split()[0].lower()
            if cmd in counts: counts[cmd]+=1
    checks["pak-landframe-coverage"]=counts["landframe"]>=180
    checks["pak-flipframe-coverage"]=counts["flipframe"]>=180
    checks["pak-jumpframe-coverage"]=counts["jumpframe"]>=130
    checks["pak-noatflash-coverage"]=counts["noatflash"]>=10
    checks["pak-tossframe-two"]=counts["tossframe"]==2
    checks["pak-blockodds-two"]=counts["blockodds"]==2
    print("PAK_COUNTS",counts)
else:
    print("PAK_COUNTS=SKIP complete PAK character assets unavailable in CI snapshot")

bad=[k for k,v in checks.items() if not v]
for k,v in checks.items():
    print(f"{k}={'OK' if v else 'FAIL'}")
if bad:
    raise SystemExit("v0.3.33 validation failed: "+", ".join(bad))
print("v0.3.33 Build 3797 PAK fidelity validator=OK")
