from pathlib import Path
root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit("v0.3.40 anchor missing: "+rel+" :: "+old[:120])
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("CMakeLists.txt","project(FinalFightXNative VERSION 0.3.39 LANGUAGES CXX)","project(FinalFightXNative VERSION 0.3.40 LANGUAGES CXX)")
rep("CMakeLists.txt",
'''add_executable(v0339_transition_scene_validator tests/v0339_transition_scene_validator.cpp)
target_link_libraries(v0339_transition_scene_validator PRIVATE ffx_data)
''',
'''add_executable(v0339_transition_scene_validator tests/v0339_transition_scene_validator.cpp)
target_link_libraries(v0339_transition_scene_validator PRIVATE ffx_data)

add_executable(v0340_blocked_entity_validator tests/v0340_blocked_entity_validator.cpp)
target_link_libraries(v0340_blocked_entity_validator PRIVATE ffx_data)
''')
rep("src/main_win.cpp","Native C++ Port v0.3.39","Native C++ Port v0.3.40")
rep("src/Game.cpp","NATIVE PORT v0.3.39","NATIVE PORT v0.3.40")

p=root/"src/OpenBorSemantics.h"
s=p.read_text(encoding="utf-8-sig")
anchor="inline bool openBorShouldPersistProgress(int canSave,bool playing,bool paused,bool loading){"
helper='''inline bool openBorExitBlockApplies(int blocked,bool projectile,bool effect,bool dead){
    return blocked!=0&&!projectile&&!effect&&!dead;
}

'''
if anchor not in s: raise SystemExit("v0.3.40 semantic anchor missing")
p.write_text(s.replace(anchor,helper+anchor,1),encoding="utf-8")

rep("src/Game.cpp",
'''    if(a.projectile||a.effect||a.dead)return;
    if(level_.blocked!=0&&a.player)a.x=std::min(a.x,openBorExitBlockedMaxX(level_.worldWidth,a.z,level_.zMax));''',
'''    if(a.projectile||a.effect||a.dead)return;
    if(openBorExitBlockApplies(level_.blocked,a.projectile,a.effect,a.dead))a.x=std::min(a.x,openBorExitBlockedMaxX(level_.worldWidth,a.z,level_.zMax));''')

(root/"tests/v0340_blocked_entity_validator.cpp").write_text(r'''#include "OpenBorData.h"
#include "OpenBorSemantics.h"
#include <cmath>
#include <iostream>
using namespace ffx;
static bool near(float a,float b){return std::fabs(a-b)<.001f;}
int main(int argc,char**argv){
    int bad=0; auto check=[&](bool ok,const char*msg){if(!ok){++bad;std::cerr<<"FAIL: "<<msg<<"\n";}};
    check(openBorExitBlockApplies(1,false,false,false),"blocked must constrain normal actors");
    check(openBorExitBlockApplies(2,false,false,false),"any nonzero blocked value must constrain normal actors");
    check(!openBorExitBlockApplies(0,false,false,false),"blocked 0 must not constrain actors");
    check(!openBorExitBlockApplies(1,true,false,false),"projectiles use their own collision path");
    check(!openBorExitBlockApplies(1,false,true,false),"effects are not movement actors");
    check(!openBorExitBlockApplies(1,false,false,true),"dead actors do not need movement constraint");
    check(near(openBorExitBlockedMaxX(320.f,205.f,240.f),255.f),"Build 3797 blocked-exit geometry");
    if(argc>1){
        OpenBorDatabase db;std::string err;check(db.load(argv[1],&err),"database load");
        if(err.empty()&&db.campaign().stages.size()>=18){
            auto l3=db.loadLevel(db.campaign().stages[2]);
            auto l18=db.loadLevel(db.campaign().stages[17]);
            check(l3.blocked==2,"64th.3 blocked=2");
            check(l18.blocked==1,"64th.18 blocked=1");
            bool boss=false;for(const auto&s:l3.spawns)boss|=s.boss;
            check(boss,"64th.3 has a boss affected by normal actor movement boundaries");
        }
    }
    if(bad){std::cerr<<"v0340_blocked_entity_failures="<<bad<<"\n";return 3;}
    std::cout<<"v0340_blocked_entity=OK normal_actors=constrained blocked_values=1/2\n";return 0;
}
''',encoding="utf-8")
print("v0.3.40 blocked-entity patch applied")
