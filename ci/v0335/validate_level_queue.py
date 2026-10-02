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
    "version-cmake":"VERSION 0.3.35" in cm,
    "version-window":"Native C++ Port v0.3.35" in mw,
    "version-menu":"NATIVE PORT v0.3.35" in g,
    "level-action-kind":"enum class LevelActionKind { Spawn, Wait, Group, Blockade }" in dh,
    "unified-action-storage":"std::vector<LevelAction> actions;" in dh,
    "spawn-action-append":"LevelActionKind::Spawn,trigger,idx" in d,
    "wait-action-append":"LevelActionKind::Wait,trigger" in d,
    "group-action-append":"a.kind=LevelActionKind::Group" in d,
    "blockade-action-append":"a.kind=LevelActionKind::Blockade" in d,
    "old-spawn-sort-removed":"stable_sort(l.spawns" not in d,
    "old-wait-sort-removed":"sort(l.waits" not in d,
    "queue-index":"size_t nextLevelAction_=0;" in gh,
    "legacy-group-defaults":"int groupMin_=100,groupMax_=100;" in gh,
    "wait-state":"bool levelWaiting_=false" in gh,
    "old-separate-indices-removed":"nextSpawn_" not in gh and "nextWait_" not in gh and "nextGroup_" not in gh and "nextBlockade_" not in gh,
    "old-group-lock-removed":"groupRefillLocked_" not in gh,
    "old-group-helper-removed":"canSpawnGroupedEnemy" not in gh and "canSpawnGroupedEnemy" not in g,
    "group-normalization":"openBorLevelGroupLimits" in sem,
    "group-start-gate":"openBorLevelQueueCanStart" in sem and "openBorLevelQueueCanStart(active,groupMin_)" in g,
    "group-capacity-gate":"openBorLevelQueueHasCapacity" in sem and "openBorLevelQueueHasCapacity(active,groupMax_)" in g,
    "authored-order-runtime":"const auto action=level_.actions[nextLevelAction_]" in g,
    "wait-freezes-horizontal":"if(levelWaiting_)desiredProg=prog;" in g,
    "wait-freezes-vertical":"if(!levelWaiting_){float sign=lower(level_.direction)" in g,
    "wait-boundary":"if(levelWaiting_)limit=std::min(limit,cameraX_+viewW-8.f);" in g,
    "wait-clear":"if(levelWaiting_&&!enemiesAlive())" in g,
    "noreset-rule":"if(!level_.noResetTime)stageTime_=(float)level_.setTime;" in g,
    "blockade-stream":"action.kind==LevelActionKind::Blockade" in g,
    "sequence-complete-actions":"nextLevelAction_>=level_.actions.size()" in g,
    "native-validator-target":"v0335_level_actions_validator" in cm,
}

if portable and (portable/"assets/data/levels").exists():
    counts={"spawn":0,"wait":0,"group":0,"blockade":0}
    inversions=0
    for p in (portable/"assets/data/levels").rglob("*.txt"):
        try: lines=p.read_text(encoding="latin-1").splitlines()
        except Exception: continue
        prev=None
        for line in lines:
            line=line.split("#",1)[0].strip()
            if not line: continue
            parts=line.split()
            cmd=parts[0].lower()
            if cmd in counts: counts[cmd]+=1
            if cmd=="at" and len(parts)>1:
                try: cur=float(parts[1])
                except Exception: continue
                if prev is not None and cur<prev: inversions+=1
                prev=cur
    print("LEVEL_PAK_COUNTS",counts,"inversions",inversions)
    checks["pak-spawn-319"]=counts["spawn"]==319
    checks["pak-wait-29"]=counts["wait"]==29
    checks["pak-group-17"]=counts["group"]==17
    checks["pak-blockade-20"]=counts["blockade"]==20
    checks["pak-at-inversions-7"]=inversions==7
else:
    print("LEVEL_PAK_COUNTS=SKIP full levels unavailable in CI snapshot")

bad=[k for k,v in checks.items() if not v]
for k,v in checks.items():
    print(f"{k}={'OK' if v else 'FAIL'}")
if bad:
    raise SystemExit("v0.3.35 validation failed: "+", ".join(bad))
print("v0.3.35 level queue fidelity validator=OK")
