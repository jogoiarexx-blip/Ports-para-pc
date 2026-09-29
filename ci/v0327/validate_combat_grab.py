from pathlib import Path
import re,sys

src=Path(sys.argv[1])
assets=Path(sys.argv[2]) if len(sys.argv)>2 else None

game=(src/"src"/"Game.cpp").read_text(errors="ignore")
data=(src/"src"/"OpenBorData.cpp").read_text(errors="ignore")

checks={
    "zero_force_filter_removed": "f->attack.damage<=0" not in game,
    "zero_force_contact_supported": "const bool contactOnly=raw==0" in game,
    "zero_force_attack_window": "const bool attackActive=f->attack.rect.valid;" in game,
    "zero_force_no_generic_invuln": "if(f->attack.damage>0||f->attack.knockdown>0)t.hitInvincibleTime" in game,
    "authored_throw_release": "heldTarget&&f->attack.knockdown>0" in game,
    "hardcoded_throw_removed": "releaseGrab(a,true,f->attack.damage" not in game,
    "player_follow_preserved": 'starts(a.anim,"follow")' in game,
    "attackone_target_lock": "attackOneTarget" in game and "attackOneConsumed" not in game,
    "attackone_follow_inherit": "att.attackOneTarget=followTarget" in game,
    "openbor_default_dropv": "t.va=3.f*42.f" in game and "t.vx=dir*1.2f*42.f" in game,
    "bare_defense_all_recognized": 'lower(t[1])=="all")e.defenseAll=t.size()>2?toFloat(t[2],1.f):1.f' in data,
}
bad=[k for k,v in checks.items() if not v]
if bad:
    raise SystemExit("source_semantics_fail="+",".join(bad))

if assets and (assets/"assets"/"data"/"chars").exists():
    root=assets/"assets"/"data"
    zero=[]
    defense=[]
    follows=[]
    landing=[]
    throw_knock=[]
    attackone=[]
    for p in root.rglob("*.txt"):
        try: lines=p.read_text(errors="ignore").splitlines()
        except: continue
        anim=""
        for n,raw in enumerate(lines,1):
            t=raw.split("#",1)[0].split()
            if not t: continue
            cmd=t[0].lower()
            if cmd=="anim" and len(t)>1: anim=t[1].lower()
            if cmd=="defense" and len(t)>1 and t[1].lower()=="all" and len(t)==2:
                defense.append((str(p.relative_to(root)),n))
            if cmd=="followcond" and len(t)>1:
                follows.append((str(p.relative_to(root)),n,t[1],anim))
            if cmd=="attackone":
                attackone.append((str(p.relative_to(root)),n,anim))
            if cmd=="damageonlanding":
                landing.append(tuple(t[1:]))
            if re.fullmatch(r"attack\d*|burn|shock",cmd) and len(t)>=7:
                try:
                    x,y,w,h=map(float,t[1:5]); dmg=float(t[5]); kd=float(t[6])
                except: continue
                if w!=0 and h!=0 and dmg==0:
                    zero.append((str(p.relative_to(root)),n,anim,kd))
                if anim.startswith(("grabforward","grabbackward","follow")) and kd>0:
                    throw_knock.append((str(p.relative_to(root)),n,anim,dmg,kd))
    assert len(zero)==21, len(zero)
    assert len(defense)==3, defense
    assert len(follows)==5 and all(x[2]=="3" for x in follows), follows
    assert len(landing)==25 and all(x==("0","1") for x in landing), landing
    assert len(throw_knock)==17, throw_knock
    assert len(attackone)==7, attackone
    print(f"asset_semantics=OK zero_force={len(zero)} defense_all={len(defense)} followcond={len(follows)} attackone={len(attackone)} damageonlanding={len(landing)} throw_knock={len(throw_knock)}")
else:
    print("asset_semantics=SKIP complete character assets unavailable in CI snapshot")

print("combat_grab_source=OK")
