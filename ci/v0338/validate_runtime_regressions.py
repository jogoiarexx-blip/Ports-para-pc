from pathlib import Path
import sys

src=Path(sys.argv[1])
def read(rel): return (src/rel).read_text(encoding="utf-8-sig")

cm=read("CMakeLists.txt")
mw=read("src/main_win.cpp")
g=read("src/Game.cpp")
sem=read("src/OpenBorSemantics.h")
checks={
 "version-cmake":"VERSION 0.3.38" in cm,
 "version-window":"Native C++ Port v0.3.38" in mw,
 "version-menu":"NATIVE PORT v0.3.38" in g,
 "stage1-cmake":"add_executable(stage1_entry_validator tests/stage1_entry_validator.cpp)" in cm,
 "v0338-cmake":"add_executable(v0338_runtime_regression_validator tests/v0338_runtime_regression_validator.cpp)" in cm,
 "save-helper":"openBorShouldPersistProgress" in sem,
 "save-loading-enabled":"mode_==Mode::Loading" in g and "openBorShouldPersistProgress(db_.campaign().canSave" in g,
 "stage-load-save-order":"mode_=Mode::Loading;if(db_.campaign().canSave>0)saveProgress();" in g,
 "old-save-gate-removed":"mode_!=Mode::Playing&&mode_!=Mode::Pause))return;" not in g,
 "stage1-test-present":(src/"tests/stage1_entry_validator.cpp").exists(),
 "v0338-test-present":(src/"tests/v0338_runtime_regression_validator.cpp").exists(),
}
bad=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f"{k}={'OK' if v else 'FAIL'}")
if bad: raise SystemExit("v0.3.38 validation failed: "+", ".join(bad))
print("v0.3.38 runtime regression validator=OK")
# trigger v0.3.38 Windows build after workflow registration
