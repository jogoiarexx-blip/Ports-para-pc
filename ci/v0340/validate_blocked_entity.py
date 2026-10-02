from pathlib import Path
import sys
src=Path(sys.argv[1])
def read(rel): return (src/rel).read_text(encoding="utf-8-sig")
cm=read("CMakeLists.txt"); mw=read("src/main_win.cpp"); g=read("src/Game.cpp"); sem=read("src/OpenBorSemantics.h")
checks={
 "version-cmake":"VERSION 0.3.40" in cm,
 "version-window":"Native C++ Port v0.3.40" in mw,
 "version-menu":"NATIVE PORT v0.3.40" in g,
 "validator-cmake":"add_executable(v0340_blocked_entity_validator" in cm,
 "blocked-helper":"openBorExitBlockApplies" in sem,
 "blocked-normal-actors":"openBorExitBlockApplies(level_.blocked,a.projectile,a.effect,a.dead)" in g,
 "old-player-only-removed":"level_.blocked!=0&&a.player" not in g,
 "blocked-geometry":"worldWidth-30.f-(zMax-z)" in sem,
 "validator-present":(src/"tests/v0340_blocked_entity_validator.cpp").exists(),
}
bad=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f"{k}={'OK' if v else 'FAIL'}")
if bad: raise SystemExit("v0.3.40 validation failed: "+", ".join(bad))
print("v0.3.40 blocked entity validator=OK")
