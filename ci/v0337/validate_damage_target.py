from pathlib import Path
import sys

src=Path(sys.argv[1])
portable=Path(sys.argv[2]) if len(sys.argv)>2 else None
def read(rel): return (src/rel).read_text(encoding="utf-8-sig")

cm=read("CMakeLists.txt"); mw=read("src/main_win.cpp"); g=read("src/Game.cpp"); gh=read("src/Game.h"); sem=read("src/OpenBorSemantics.h")
checks={
 "version-cmake":"VERSION 0.3.37" in cm,
 "version-window":"Native C++ Port v0.3.37" in mw,
 "version-menu":"NATIVE PORT v0.3.37" in g,
 "combat-type-method":"std::string Game::combatType" in g and "combatType(const Actor&actor)" in gh,
 "persistent-player-type":'openBorEffectiveCombatType' in sem and 'type=="none"||type=="icon"' in sem,
 "legacy-default-candamage":"openBorLegacyDefaultCanDamage" in sem,
 "legacy-default-hostile":"openBorLegacyDefaultHostile" in sem,
 "obstacle-bypass-removed":'if(t.def&&t.def->type=="obstacle")return true;' not in g,
 "authored-candamage-target-type":"containsType(*authored,targetType)" in g,
 "weapon-base-candamage-inheritance":"base->canDamage.empty()" in g and "authored=&base->canDamage" in g,
 "weapon-base-hostile-inheritance":"base->hostile.empty()" in g and "authored=&base->hostile" in g,
 "followcond-damage-mask":"const bool damageTypeAllowed=canDamage(att,t);" in g,
 "followcond-thresholds":"(cond<2||damageTypeAllowed)&&(cond<3||(alive&&!blocked))&&(cond<4||grabbable)" in g,
 "followcond-hostile-removed":"bool hostile=isHostile(att,t);" not in g,
 "native-validator-target":"v0337_damage_target_validator" in cm,
}
if portable and (portable/"assets/data/chars").exists():
    root=portable/"assets/data/chars"
    counts={"candamage":0,"hostile":0,"projectilehit":0,"followanim":0,"followcond":0}
    follow=[]
    for p in root.rglob("*.txt"):
        try: lines=p.read_text(encoding="latin-1").splitlines()
        except Exception: continue
        for line in lines:
            s=line.split("#",1)[0].strip()
            if not s: continue
            q=s.split(); k=q[0].lower()
            if k in counts: counts[k]+=1
            if k=="followcond": follow.append(q[1] if len(q)>1 else "")
    print("PAK_COUNTS",counts,"followcond_values",follow)
    checks["pak-candamage-37"]=counts["candamage"]==37
    checks["pak-hostile-24"]=counts["hostile"]==24
    checks["pak-projectilehit-0"]=counts["projectilehit"]==0
    checks["pak-followcond-5x3"]=counts["followcond"]==5 and all(v=="3" for v in follow)
else:
    print("PAK_COUNTS=SKIP complete character assets unavailable in CI snapshot")

bad=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f"{k}={'OK' if v else 'FAIL'}")
if bad: raise SystemExit("v0.3.37 validation failed: "+", ".join(bad))
print("v0.3.37 damage target fidelity validator=OK")
