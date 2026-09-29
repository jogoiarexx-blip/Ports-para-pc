from pathlib import Path
import re,sys

src=Path(sys.argv[1])
asset=Path(sys.argv[2]) if len(sys.argv)>2 else None

h=(src/"src/OpenBorData.h").read_text(errors="ignore")
d=(src/"src/OpenBorData.cpp").read_text(errors="ignore")
gh=(src/"src/Game.h").read_text(errors="ignore")
g=(src/"src/Game.cpp").read_text(errors="ignore")

checks={
 "blast_attackbox":"bool blast=false;" in h,
 "dol_blast_parse":'atk.blast=t.size()>2?toInt(t[2])!=0:false;' in d,
 "dol_persist":"const bool blast=atk.blast;" in d and "atk.blast=blast;" in d,
 "actor_blast":"fallingState=false,blastActive=false" in gh,
 "fall_gate":"if(att.fallingState&&!att.blastActive)continue;" in g,
 "indirect_owner":"indirectOwner=att.blastActive?actorById(att.blastOwner):nullptr" in g,
 "blast_credit":"blastCreditPlayer" in g and "registerScore(creditPlayer" in g,
 "ai_zero_force":"f.attack.damage>0||f.attack.knockdown>0" in g,
 "sensor_no_weapon_use":"if(realImpact&&att.player&&att.weaponNumber>0" in g,
 "landing_clear":"a.fallingState=false;a.blastActive=false;a.blastOwner=0" in g,
}
bad=[k for k,v in checks.items() if not v]
if bad:
    raise SystemExit("v0328_blast_source_fail="+",".join(bad))

if asset and (asset/"assets/data/chars").exists():
    root=asset/"assets/data/chars"
    dol=[]
    fall_boxes=0
    for p in root.rglob("*.txt"):
        anim=""
        try: lines=p.read_text(errors="ignore").splitlines()
        except: continue
        for n,raw in enumerate(lines,1):
            t=raw.split("#",1)[0].split()
            if not t: continue
            cmd=t[0].lower()
            if cmd=="anim" and len(t)>1:
                anim=t[1].lower()
            elif cmd=="damageonlanding":
                dol.append((str(p.relative_to(root)),n,anim,t[1:]))
            elif anim.startswith(("fall","backfall","burn","shock")) and re.fullmatch(r"attack\d*|burn|shock",cmd) and len(t)>6:
                try:
                    x,y,w,h=map(float,t[1:5])
                except: continue
                if w and h: fall_boxes+=1
    assert len(dol)==25, len(dol)
    assert all(x[3][:2]==["0","1"] for x in dol), dol
    assert fall_boxes==116, fall_boxes
    print(f"blast_assets=OK damageonlanding={len(dol)} fall_attack_boxes={fall_boxes}")
else:
    print("blast_assets=SKIP complete char assets unavailable in CI snapshot")

print("v0328_blast_semantics=OK")
