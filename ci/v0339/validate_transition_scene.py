from pathlib import Path
import sys
src=Path(sys.argv[1])
def read(rel): return (src/rel).read_text(encoding="utf-8-sig")
cm=read("CMakeLists.txt"); mw=read("src/main_win.cpp"); g=read("src/Game.cpp"); sem=read("src/OpenBorSemantics.h")
checks={
 "version-cmake":"VERSION 0.3.39" in cm,
 "version-window":"Native C++ Port v0.3.39" in mw,
 "version-menu":"NATIVE PORT v0.3.39" in g,
 "validator-cmake":"add_executable(v0339_transition_scene_validator" in cm,
 "scene-skip-helper":"openBorSceneSkipDisposition" in sem,
 "scene-anybutton":"in.attackPressed||in.attack2Pressed||in.jumpPressed||in.specialPressed" in g,
 "scene-close":"SceneSkipDisposition::CloseScene" in g,
 "vertical-rate-helper":"openBorBuild3797VerticalScrollDelta" in sem,
 "vertical-runtime":"verticalStageScroll_+=openBorBuild3797VerticalScrollDelta(dt)" in g,
 "old-vertical-removed":"sign*30.f*dt" not in g,
 "interlevel-health":"openBorInterLevelHealth" in sem and "p->hp=openBorInterLevelHealth" in g,
 "validator-present":(src/"tests/v0339_transition_scene_validator.cpp").exists(),
}
bad=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f"{k}={'OK' if v else 'FAIL'}")
if bad: raise SystemExit("v0.3.39 validation failed: "+", ".join(bad))
print("v0.3.39 transition/scene validator=OK")
