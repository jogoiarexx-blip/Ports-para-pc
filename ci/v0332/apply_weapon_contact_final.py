from pathlib import Path

root=Path.cwd()

def patch(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit(f"final weapon anchor missing in {rel}: {old[:100]!r}")
    p.write_text(s.replace(old,new,1),encoding="utf-8")

game="src/Game.cpp"

# Legacy pickup: a character already carrying a weapon does not consume a second
# weapon item from the ground.
patch(
    game,
    "if(p.weaponNumber>0)dropEquippedWeapon(p);p.def=wd;",
    "if(p.weaponNumber>0)return false;p.def=wd;",
)

# Default weaploss=0: every successful non-blocked attack contact may drop the
# carried weapon, including authored zero-force contact boxes.
patch(
    game,
    "if((amount>0||knockdown>0)&&t.weaponNumber>0)dropEquippedWeapon(t);",
    "if(t.weaponNumber>0)dropEquippedWeapon(t);",
)

# Do not consume the ground item when equipWeapon refuses pickup because the
# player is already armed.
patch(
    game,
    'if(a.def->subtype=="weapon")equipWeapon(*p,*a.def,a.weaponDropCount,a.baseModel);else{',
    'bool picked=true;if(a.def->subtype=="weapon")picked=equipWeapon(*p,*a.def,a.weaponDropCount,a.baseModel);else{',
)
patch(
    game,
    'registerScore((int)i,a.def->score);}a.dead=true;',
    'registerScore((int)i,a.def->score);}if(!picked)continue;a.dead=true;',
)

for rel in ("CHANGELOG-v0.3.32.txt","PAK-FIDELITY-AUDIT-v0.3.32.txt"):
    p=root/rel
    if not p.exists():
        continue
    s=p.read_text(encoding="utf-8-sig")
    s=s.replace(
        "- weapon swapping: collecting another usable weapon drops the currently equipped one instead of silently deleting it.",
        "- weapon pickup: a character already carrying a weapon leaves another weapon on the ground, matching the legacy engine.",
    )
    s=s.replace(
        "and swapping weapons drops the previous one.",
        "and an already-armed player does not consume/swap another weapon pickup.",
    )
    s=s.replace("a real hit drops an equipped weapon","a non-blocked attack contact drops an equipped weapon")
    p.write_text(s,encoding="utf-8")

print("v0.3.32 final weapon contact/pickup semantics applied")
