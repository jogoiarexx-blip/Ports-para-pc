from pathlib import Path
import sys
src=Path(sys.argv[1])

def read(rel): return (src/rel).read_text(encoding="utf-8-sig")
cm=read("CMakeLists.txt"); mw=read("src/main_win.cpp"); g=read("src/Game.cpp"); sem=read("src/OpenBorSemantics.h")
checks={
"version-cmake":"VERSION 0.3.36" in cm,
"version-window":"Native C++ Port v0.3.36" in mw,
"version-menu":"NATIVE PORT v0.3.36" in g,
"spawn-helper":"openBorDefaultPlayerSpawn" in sem,
"spawn-right-defaults":"20.f+30.f*(float)playerIndex" in sem,
"spawn-left-defaults":"legacyViewWidth-20.f-30.f*(float)playerIndex" in sem,
"default-z":"zMin+5.f" in sem,
"camera-average":"openBorMultiplayerCameraTarget" in sem and "sum/(float)playerX.size()-viewWidth*.5f" in sem,
"blocked-diagonal":"worldWidth-30.f-(zMax-z)" in sem,
"loadstage-default-spawn":"openBorDefaultPlayerSpawn(i,reverse,level_.zMin,level_.zMax)" in g,
"respawn-default-spawn":"openBorDefaultPlayerSpawn(i,reverse,level_.zMin,level_.zMax)" in g,
"screen-left-10":"cameraX_+10.f" in g,
"screen-right-10":"cameraX_+viewW-10.f" in g,
"camera-active-average":"openBorMultiplayerCameraTarget(activePlayerX,viewW)" in g,
"old-lead-camera-removed":"lead->x-200.f" not in g and "lead->x-120.f" not in g,
"wall-aware-clamp":"if(p->x!=beforeX)resolveWalls(*p,beforeX,beforeZ)" in g,
"transition-validator":"v0336_transition_validator" in cm,
}
bad=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f"{k}={'OK' if v else 'FAIL'}")
if bad: raise SystemExit("v0.3.36 validation failed: "+", ".join(bad))
print("v0.3.36 transition fidelity validator=OK")
