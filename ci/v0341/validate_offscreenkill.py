from pathlib import Path
import sys
src=Path(sys.argv[1])
def read(rel): return (src/rel).read_text(encoding="utf-8-sig")
cm=read("CMakeLists.txt"); mw=read("src/main_win.cpp"); g=read("src/Game.cpp"); sem=read("src/OpenBorSemantics.h"); h=read("src/OpenBorData.h")
checks={
 "version-cmake":"VERSION 0.3.41" in cm,
 "version-window":"Native C++ Port v0.3.41" in mw,
 "version-menu":"NATIVE PORT v0.3.41" in g,
 "validator-cmake":"add_executable(v0341_offscreenkill_validator" in cm,
 "default-field":"offscreenKill=0" in h,
 "default-helper":"openBorEffectiveOffscreenKill" in sem and "3000" in sem,
 "horizontal-helper":"openBorOutsideHorizontalOffscreenKill" in sem,
 "cleanup-runtime":"openBorOutsideHorizontalOffscreenKill(a.x,cameraX_,renderer_.logicalWidth(),a.def->offscreenKill)" in g,
 "old-1000-default-removed":"offscreenKill=1000" not in h,
 "validator-present":(src/"tests/v0341_offscreenkill_validator.cpp").exists(),
}
bad=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f"{k}={'OK' if v else 'FAIL'}")
if bad: raise SystemExit("v0.3.41 validation failed: "+", ".join(bad))
print("v0.3.41 offscreenkill validator=OK")
