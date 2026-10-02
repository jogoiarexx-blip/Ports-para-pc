from pathlib import Path
root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit("v0.3.41 anchor missing: "+rel+" :: "+old[:120])
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("CMakeLists.txt","project(FinalFightXNative VERSION 0.3.40 LANGUAGES CXX)","project(FinalFightXNative VERSION 0.3.41 LANGUAGES CXX)")
rep("CMakeLists.txt",
'''add_executable(v0340_blocked_entity_validator tests/v0340_blocked_entity_validator.cpp)
target_link_libraries(v0340_blocked_entity_validator PRIVATE ffx_data)
''',
'''add_executable(v0340_blocked_entity_validator tests/v0340_blocked_entity_validator.cpp)
target_link_libraries(v0340_blocked_entity_validator PRIVATE ffx_data)

add_executable(v0341_offscreenkill_validator tests/v0341_offscreenkill_validator.cpp)
target_link_libraries(v0341_offscreenkill_validator PRIVATE ffx_data)
''')
rep("src/main_win.cpp","Native C++ Port v0.3.40","Native C++ Port v0.3.41")
rep("src/Game.cpp","NATIVE PORT v0.3.40","NATIVE PORT v0.3.41")
rep("src/OpenBorData.h","int makeInv=0, offscreenKill=1000, thold=0;","int makeInv=0, offscreenKill=0, thold=0;")

p=root/"src/OpenBorSemantics.h"
s=p.read_text(encoding="utf-8-sig")
anchor="inline bool openBorExitBlockApplies(int blocked,bool projectile,bool effect,bool dead){"
if anchor not in s: raise SystemExit("v0.3.41 semantic anchor missing")
helper='''inline int openBorEffectiveOffscreenKill(int configured){
    return configured!=0?configured:3000;
}

inline bool openBorOutsideHorizontalOffscreenKill(float x,float cameraX,float viewWidth,int configured){
    const float osk=(float)openBorEffectiveOffscreenKill(configured);
    return cameraX-x>osk || x-cameraX-viewWidth>osk;
}

'''
p.write_text(s.replace(anchor,helper+anchor,1),encoding="utf-8")

rep("src/Game.cpp",
'''if(!a.player&&!a.boss&&!a.dead&&a.def){int off=std::max(80,a.def->offscreenKill);if(a.x<cameraX_-off||a.x>cameraX_+renderer_.logicalWidth()+off){a.dead=true;a.deathAnimationFinished=true;a.deathFinishedAt=a.deathTime;}}''',
'''if(!a.player&&!a.boss&&!a.dead&&a.def){if(openBorOutsideHorizontalOffscreenKill(a.x,cameraX_,renderer_.logicalWidth(),a.def->offscreenKill)){a.dead=true;a.deathAnimationFinished=true;a.deathFinishedAt=a.deathTime;}}''')
rep("src/Game.cpp",
'''if(mode==3){int off=a.def?std::max(80,a.def->offscreenKill):120;return a.x<cameraX_-off||a.x>cameraX_+renderer_.logicalWidth()+off;}''',
'''if(mode==3){int off=a.def?a.def->offscreenKill:0;return openBorOutsideHorizontalOffscreenKill(a.x,cameraX_,renderer_.logicalWidth(),off);}''')

(root/"tests/v0341_offscreenkill_validator.cpp").write_text(r'''#include "OpenBorData.h"
#include "OpenBorSemantics.h"
#include <iostream>
using namespace ffx;
int main(int argc,char**argv){
    int bad=0; auto check=[&](bool ok,const char*msg){if(!ok){++bad;std::cerr<<"FAIL: "<<msg<<"\n";}};
    check(openBorEffectiveOffscreenKill(0)==3000,"Build 3797 default offscreenkill must be 3000");
    check(openBorEffectiveOffscreenKill(200)==200,"authored offscreenkill must override default");
    check(!openBorOutsideHorizontalOffscreenKill(1575.f,0.f,320.f,0),"J at x=1575 must survive default offscreenkill from a 320px view");
    check(openBorOutsideHorizontalOffscreenKill(1575.f,0.f,320.f,1000),"old 1000px default demonstrates the premature-kill regression");
    if(argc>1){
        OpenBorDatabase db; std::string err; check(db.load(argv[1],&err),"database load");
        if(err.empty()){
            auto j=db.entity("j"); check(j&&j->offscreenKill==0,"J must inherit default offscreenkill");
            bool found=false;
            for(size_t i=0;i<db.campaign().stages.size();++i){
                auto lv=db.loadLevel(db.campaign().stages[i]);
                for(const auto&sp:lv.spawns)if(sp.model=="j"&&sp.x==1575.f){found=true;check(sp.trigger==10.f,"J far-entry trigger must remain at 10");}
            }
            check(found,"64th.4 J x=1575 spawn must be present");
        }
    }
    if(bad){std::cerr<<"v0341_offscreenkill_failures="<<bad<<"\n";return 3;}
    std::cout<<"v0341_offscreenkill=OK default=3000 far_spawn_x=1575\n";return 0;
}
''',encoding="utf-8")

for name in ["README-SOURCE-v0.3.40.txt","BUILD-VALIDATION-v0.3.40.txt","PAK-FIDELITY-AUDIT-v0.3.40.txt","CHANGELOG-v0.3.40.txt"]:
    try:(root/name).unlink()
    except FileNotFoundError:pass

(root/"README-SOURCE-v0.3.41.txt").write_text("""Final Fight X Native v0.3.41 - Source

Focus: Build 3797 offscreenkill fidelity.

Windows x64:
  cmake -S . -B build -A x64
  cmake --build build --config Release --target FinalFightX v0341_offscreenkill_validator -- /m

Place FinalFightX.exe beside the assets folder from the portable package.
""",encoding="utf-8")

(root/"CHANGELOG-v0.3.41.txt").write_text("""Final Fight X Native - v0.3.41

- Restored OpenBOR Build 3797 default offscreenkill: 3000 pixels when the model does not declare a value.
- Preserved authored overrides such as offscreenkill 200 and 9999999.
- Prevents the 64th.4 J spawn at X=1575 from being killed before it can enter the arena.
- Added v0341_offscreenkill_validator.
- Preserved v0.3.40 blocked-exit fidelity and earlier transition/combat fixes.
""",encoding="utf-8")

(root/"PAK-FIDELITY-AUDIT-v0.3.41.txt").write_text("""Final Fight X Native v0.3.41 - PAK Fidelity Audit

Confirmed PAK case:
- levels/ff64th/64th.4.txt spawns model J at coords X=1575, trigger at=10.
- chars/j/j.txt does not declare offscreenkill.

Build 3797 comparison:
- DEFAULT_OFFSCREEN_KILL is 3000.
- check_lost uses the model value only when it is non-zero; otherwise it falls back to 3000.

Regression fixed:
- v0.3.40 stored 1000 as the model default, so J could be removed at 1255 pixels beyond a 320px view.
""",encoding="utf-8")

(root/"BUILD-VALIDATION-v0.3.41.txt").write_text("""Final Fight X Native v0.3.41 - Build Validation

Validation includes the existing native regression suite plus v0341_offscreenkill_validator.
No claim is made here of a full interactive Windows campaign playthrough.
""",encoding="utf-8")
print("v0.3.41 offscreenkill patch applied")
