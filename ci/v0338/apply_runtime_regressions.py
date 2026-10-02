from pathlib import Path
root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit("v0.3.38 anchor missing: "+rel+" :: "+old[:120])
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("CMakeLists.txt","project(FinalFightXNative VERSION 0.3.37 LANGUAGES CXX)","project(FinalFightXNative VERSION 0.3.38 LANGUAGES CXX)")
rep("CMakeLists.txt",
'''add_executable(v0337_damage_target_validator tests/v0337_damage_target_validator.cpp)
target_link_libraries(v0337_damage_target_validator PRIVATE ffx_data)
''',
'''add_executable(v0337_damage_target_validator tests/v0337_damage_target_validator.cpp)
target_link_libraries(v0337_damage_target_validator PRIVATE ffx_data)

add_executable(stage1_entry_validator tests/stage1_entry_validator.cpp)
target_link_libraries(stage1_entry_validator PRIVATE ffx_data)

add_executable(v0338_runtime_regression_validator tests/v0338_runtime_regression_validator.cpp)
target_link_libraries(v0338_runtime_regression_validator PRIVATE ffx_data)
''')
rep("src/main_win.cpp","Native C++ Port v0.3.37","Native C++ Port v0.3.38")
rep("src/Game.cpp","NATIVE PORT v0.3.37","NATIVE PORT v0.3.38")

p=root/"src/OpenBorSemantics.h"
s=p.read_text(encoding="utf-8-sig")
anchor="inline std::string openBorEffectiveCombatType("
if anchor not in s:
    raise SystemExit("v0.3.38 semantic anchor missing")
helper='''inline bool openBorShouldPersistProgress(int canSave,bool playing,bool paused,bool loading){
    return canSave>0&&(playing||paused||loading);
}

'''
p.write_text(s.replace(anchor,helper+anchor,1),encoding="utf-8")

rep("src/Game.cpp",
'''void Game::saveProgress(){if(db_.campaign().canSave<=0||(mode_!=Mode::Playing&&mode_!=Mode::Pause))return;''',
'''void Game::saveProgress(){if(!openBorShouldPersistProgress(db_.campaign().canSave,mode_==Mode::Playing,mode_==Mode::Pause,mode_==Mode::Loading))return;''')

(root/"tests/v0338_runtime_regression_validator.cpp").write_text(r'''#include "OpenBorSemantics.h"
#include <iostream>
using namespace ffx;
int main(){
    int bad=0; auto check=[&](bool ok,const char*msg){if(!ok){++bad;std::cerr<<"FAIL: "<<msg<<"\n";}};
    check(!openBorShouldPersistProgress(0,true,false,false),"cansave=0 must disable progress writes");
    check(openBorShouldPersistProgress(1,true,false,false),"playing save must remain enabled");
    check(openBorShouldPersistProgress(1,false,true,false),"pause/manual save must remain enabled");
    check(openBorShouldPersistProgress(1,false,false,true),"stage-entry Loading save must be enabled");
    check(!openBorShouldPersistProgress(1,false,false,false),"title/menu states must not write campaign progress");
    check(openBorShouldPersistProgress(2,false,false,true),"strict cansave=2 must persist newly reached stage during Loading");
    if(bad){std::cerr<<"v0338_runtime_regression_failures="<<bad<<"\n";return 3;}
    std::cout<<"v0338_runtime_regressions=OK\n";return 0;
}
''',encoding="utf-8")

for name in ["README-SOURCE-v0.3.37.txt","BUILD-VALIDATION-v0.3.37.txt","CHANGELOG-v0.3.37.txt"]:
    try:(root/name).unlink()
    except FileNotFoundError:pass

(root/"README-SOURCE-v0.3.38.txt").write_text("""Final Fight X Native v0.3.38 - Source

Build Windows x64 (Visual Studio 2022):
  cmake -S . -B build -A x64
  cmake --build build --config Release --target FinalFightX stage1_entry_validator v0334_semantics_validator v0335_level_actions_validator v0336_transition_validator v0337_damage_target_validator v0338_runtime_regression_validator -- /m

Place FinalFightX.exe beside the assets folder from the portable package.

Regression focus in v0.3.38:
- stage-entry autosave is allowed while the runtime is in Loading mode;
- the Stage 1 entry/wall regression validator is now a normal CMake target;
- previous v0.3.34-v0.3.37 native semantic validators remain enabled.
""",encoding="utf-8")

(root/"CHANGELOG-v0.3.38.txt").write_text("""Final Fight X Native - v0.3.38 Runtime Regression Fixes

- Fixed immediate campaign progress saving when a new level is loaded.
  The runtime intentionally enters Loading immediately before calling saveProgress();
  v0.3.37 rejected every save outside Playing/Pause, so that stage-entry save was a no-op.
- cansave 1/2 can now persist the newly reached stage while Loading; title/menu states remain blocked.
- Added a dedicated v0338 runtime regression validator for the save gate.
- Integrated the existing Stage 1 entry/wall validator into CMake instead of compiling it only by a separate CI command.
- Preserved all v0.3.37 combat-target, followcond, transition, level-queue and movement semantics.
""",encoding="utf-8")

(root/"BUILD-VALIDATION-v0.3.38.txt").write_text("""Final Fight X Native v0.3.38 - Build Validation

Validation targets:
- v0334_semantics_validator
- v0335_level_actions_validator
- v0336_transition_validator
- v0337_damage_target_validator
- stage1_entry_validator
- v0338_runtime_regression_validator

Windows x64 executable is built and validated by the v0.3.38 GitHub Actions workflow.
No claim is made here of a full interactive Windows campaign playthrough.
""",encoding="utf-8")
print("v0.3.38 runtime regression patch applied")
