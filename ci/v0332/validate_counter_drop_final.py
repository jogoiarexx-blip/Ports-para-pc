from pathlib import Path
import sys

root=Path(sys.argv[1]) if len(sys.argv)>1 else Path.cwd()
datah=(root/"src/OpenBorData.h").read_text(encoding="utf-8-sig")
game=(root/"src/Game.cpp").read_text(encoding="utf-8-sig")

checks={
    "legacy-counter-default-3":"counter=3, shootNum=0" in datah,
    "counter-four-disappears-on-fourth":"nextDrop<std::max(0,item->counter)" in game,
    "old-off-by-one-removed":"nextDrop<=std::max(0,item->counter)" not in game,
}
failed=[k for k,v in checks.items() if not v]
for k,v in checks.items():
    print(f"{k}={'OK' if v else 'FAIL'}")
if failed:
    raise SystemExit("final v0.3.32 counter validation failed: "+", ".join(failed))
print("v0.3.32 final counter/drop validator=OK")
