from pathlib import Path
import sys

root=Path(sys.argv[1]) if len(sys.argv)>1 else Path.cwd()
game=(root/"src/Game.cpp").read_text(encoding="utf-8-sig")

checks={
    "armed-player-refuses-second-weapon":"if(p.weaponNumber>0)return false;p.def=wd;" in game,
    "zero-force-contact-can-drop-weapon":"if(t.weaponNumber>0)dropEquippedWeapon(t);" in game,
    "old-damage-only-drop-removed":"if((amount>0||knockdown>0)&&t.weaponNumber>0)dropEquippedWeapon(t);" not in game,
    "failed-pickup-keeps-ground-item":'bool picked=true;if(a.def->subtype=="weapon")picked=equipWeapon' in game and 'if(!picked)continue;a.dead=true;' in game,
    "old-auto-swap-removed":"if(p.weaponNumber>0)dropEquippedWeapon(p);p.def=wd;" not in game,
}
failed=[k for k,v in checks.items() if not v]
for k,v in checks.items():
    print(f"{k}={'OK' if v else 'FAIL'}")
if failed:
    raise SystemExit("final v0.3.32 weapon validation failed: "+", ".join(failed))
print("v0.3.32 final weapon contact/pickup validator=OK")
