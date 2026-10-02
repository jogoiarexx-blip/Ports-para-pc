from pathlib import Path
import sys

src=Path(sys.argv[1])
portable=Path(sys.argv[2]) if len(sys.argv)>2 else None

def read(rel):
    return (src/rel).read_text(encoding="utf-8-sig")

cm=read("CMakeLists.txt")
mw=read("src/main_win.cpp")
g=read("src/Game.cpp")
gh=read("src/Game.h")
d=read("src/OpenBorData.cpp")
dh=read("src/OpenBorData.h")
sem=read("src/OpenBorSemantics.h")

checks={
    "version-cmake":"VERSION 0.3.34" in cm,
    "version-window":"Native C++ Port v0.3.34" in mw,
    "version-menu":"NATIVE PORT v0.3.34" in g,
    "jump-helper":"openBorJumpFrameMotion" in sem and 'type=="enemy"||type=="npc"' in sem,
    "jump-replaces-velocity":"a.va=jm.lift*42.f;a.vx=jm.x*42.f*dir;a.vz=jm.z*24.f" in g,
    "jump-old-accumulation-removed":"a.vx+=dx*42.f*dir" not in g and "std::max(a.va,h*42.f)" not in g,
    "bouncefactor-storage":"float bounceFactor=4.f" in dh,
    "bouncefactor-parser":'cmd=="bouncefactor"' in d,
    "bounce-impact-helper":"openBorBounceVelocity" in sem,
    "bounce-real-impact":"handleLanding(a,floor,impactVelocity)" in g and "a.vx/=an->bounceFactor" in g,
    "bounce-no-one-shot-gate":"!a.bounced&&starts(a.anim" not in g,
    "blast-attackbox":"bool blast=false" in dh,
    "damageonlanding-blast-parser":'atk.blast=t.size()>2?toInt(t[2])!=0:false' in d,
    "blast-reaction-runtime":"t.blastReaction=attack->blast" in g,
    "fall-attack-gate":'starts(att.anim,"fall")&&!att.blastReaction' in g,
    "throwdist-default-2-5":"float throwDist=2.5f, throwHeight=0.f" in dh,
    "throwdamage-from-thrower":"std::max(0,a.def->throwDamage)" in g,
    "throwheight-fallback":"victim->def->throwHeight!=0.f" in g and "victim->def->jumpHeight" in g,
    "throwdamage-on-landing":"releaseGrab(a,true,0,vx,va,\"fall\")" in g and "victim->landingDamage=dmg" in g,
    "native-semantic-test-target":"v0334_semantics_validator" in cm,
}

if portable and (portable/"assets/data/chars").exists():
    root=portable/"assets/data"
    counts={"jumpframe":0,"damageonlanding":0,"throwframe":0,"throwframewait":0,"bounce":0}
    damage_blast=0
    files_jump=set()
    for p in (root/"chars").rglob("*.txt"):
        try: lines=p.read_text(encoding="latin-1").splitlines()
        except Exception: continue
        for line in lines:
            line=line.split("#",1)[0].strip()
            if not line: continue
            parts=line.split()
            cmd=parts[0].lower()
            if cmd in counts:
                counts[cmd]+=1
                if cmd=="jumpframe": files_jump.add(str(p))
                if cmd=="damageonlanding" and len(parts)>2 and parts[2]=="1": damage_blast+=1
    print("PAK_COUNTS",counts,"jump_files",len(files_jump),"damage_blast",damage_blast)
    checks["pak-jumpframe-143"]=counts["jumpframe"]==143
    checks["pak-jumpframe-36-files"]=len(files_jump)==36
    checks["pak-damageonlanding-25"]=counts["damageonlanding"]==25 and damage_blast==25
    checks["pak-throwframe-8"]=counts["throwframe"]==8
    checks["pak-throwframewait-3"]=counts["throwframewait"]==3
else:
    print("PAK_COUNTS=SKIP complete character assets unavailable in CI snapshot")

bad=[k for k,v in checks.items() if not v]
for k,v in checks.items():
    print(f"{k}={'OK' if v else 'FAIL'}")
if bad:
    raise SystemExit("v0.3.34 validation failed: "+", ".join(bad))
print("v0.3.34 combat motion fidelity validator=OK")
