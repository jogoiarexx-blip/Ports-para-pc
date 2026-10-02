from pathlib import Path

root=Path.cwd()

def patch(rel, old, new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit(f"final v0.3.32 counter anchor missing: {rel}")
    p.write_text(s.replace(old,new,1),encoding="utf-8")

# OpenBOR legacy default: weapons/projectiles start with three allowed losses
# when counter is omitted.
patch(
    "src/OpenBorData.h",
    "int shadow=0, weaponNumber=0, counter=0, shootNum=0, typeShot=0, reload=0, score=0, remove=1;",
    "int shadow=0, weaponNumber=0, counter=3, shootNum=0, typeShot=0, reload=0, score=0, remove=1;",
)

# counter N disappears on the Nth loss. A counter 4 weapon therefore remains
# collectible after losses 1..3 and is removed on loss 4.
patch(
    "src/Game.cpp",
    "spawn=nextDrop<=std::max(0,item->counter);",
    "spawn=nextDrop<std::max(0,item->counter);",
)

for rel in ("CHANGELOG-v0.3.32.txt","PAK-FIDELITY-AUDIT-v0.3.32.txt"):
    p=root/rel
    if not p.exists():
        continue
    s=p.read_text(encoding="utf-8-sig")
    s=s.replace("disappears only after exceeding counter","disappears on the counter-th loss")
    s=s.replace("disappear only after exceeding counter","disappear on the counter-th loss")
    p.write_text(s,encoding="utf-8")

print("v0.3.32 final counter/drop semantics applied")
