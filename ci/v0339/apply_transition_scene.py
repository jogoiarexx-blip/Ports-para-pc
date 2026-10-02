from pathlib import Path
root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit("v0.3.39 anchor missing: "+rel+" :: "+old[:120])
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("CMakeLists.txt","project(FinalFightXNative VERSION 0.3.38 LANGUAGES CXX)","project(FinalFightXNative VERSION 0.3.39 LANGUAGES CXX)")
rep("CMakeLists.txt",
'''add_executable(v0338_runtime_regression_validator tests/v0338_runtime_regression_validator.cpp)
target_link_libraries(v0338_runtime_regression_validator PRIVATE ffx_data)
''',
'''add_executable(v0338_runtime_regression_validator tests/v0338_runtime_regression_validator.cpp)
target_link_libraries(v0338_runtime_regression_validator PRIVATE ffx_data)

add_executable(v0339_transition_scene_validator tests/v0339_transition_scene_validator.cpp)
target_link_libraries(v0339_transition_scene_validator PRIVATE ffx_data)
''')
rep("src/main_win.cpp","Native C++ Port v0.3.38","Native C++ Port v0.3.39")
rep("src/Game.cpp","NATIVE PORT v0.3.38","NATIVE PORT v0.3.39")

p=root/"src/OpenBorSemantics.h"
s=p.read_text(encoding="utf-8-sig")
anchor='''inline bool openBorShouldPersistProgress(int canSave,bool playing,bool paused,bool loading){
    return canSave>0&&(playing||paused||loading);
}
'''
if anchor not in s: raise SystemExit("v0.3.39 semantics anchor missing")
extra=anchor+r'''
enum class SceneSkipDisposition { Ignore, NextAnimation, CloseScene };

inline SceneSkipDisposition openBorSceneSkipDisposition(bool pressed,bool skipOne,bool noSkip){
    if(!pressed||noSkip)return SceneSkipDisposition::Ignore;
    return skipOne?SceneSkipDisposition::NextAnimation:SceneSkipDisposition::CloseScene;
}

inline float openBorBuild3797VerticalScrollDelta(float dt){
    return std::max(0.f,dt)*100.f;
}

inline int openBorInterLevelHealth(int previousHealth,int maxHealth,bool sameChapter){
    if(maxHealth<=0)return 0;
    if(!sameChapter||previousHealth<=0)return maxHealth;
    return std::min(maxHealth,previousHealth+5);
}
'''
p.write_text(s.replace(anchor,extra,1),encoding="utf-8")

rep("src/Game.cpp",
'''if(sameChapter&&old[i].hp>0)p->hp=std::min(p->maxHp,old[i].hp);''',
'''p->hp=openBorInterLevelHealth(old[i].hp,p->maxHp,sameChapter);''')
rep("src/Game.cpp",
'''if(isVerticalScroll()){if(!levelWaiting_){float sign=lower(level_.direction)=="up"?-1.f:1.f;verticalStageScroll_+=sign*30.f*dt;}cameraX_=0;}''',
'''if(isVerticalScroll()){if(!levelWaiting_)verticalStageScroll_+=openBorBuild3797VerticalScrollDelta(dt);cameraX_=0;}''')
rep("src/Game.cpp",
'''void Game::updateScene(float dt){auto in=input_.state(0);if(in.backPressed){sceneVisual_.clear();if(sceneNextStage_>=0){int n=sceneNextStage_;sceneNextStage_=-1;loadStage(n,true);return;}if(sceneAfterMode_==Mode::Title){enterTitle();return;}mode_=sceneAfterMode_;return;}if(sceneVisual_.empty()){advanceSceneStep();return;}sceneStepTime_+=dt;float d=renderer_.animationDuration(sceneVisual_);if(d<=.11f)d=1.5f;const bool userSkip=in.startPressed&&sceneSkip_&&!sceneNoSkip_;if(userSkip||sceneStepTime_>=d)advanceSceneStep();}''',
'''void Game::updateScene(float dt){
    auto in=input_.state(0);
    if(sceneVisual_.empty()){advanceSceneStep();return;}
    const bool anyButton=in.startPressed||in.backPressed||in.attackPressed||in.attack2Pressed||in.jumpPressed||in.specialPressed;
    const auto skip=openBorSceneSkipDisposition(anyButton,sceneSkip_,sceneNoSkip_);
    if(skip==SceneSkipDisposition::CloseScene){
        for(;sceneStep_<scene_.steps.size();++sceneStep_)if(scene_.steps[sceneStep_].kind==SceneStep::Kind::Silence)audio_.stopMusic();
        sceneVisual_.clear();loadNextSceneFile();return;
    }
    if(skip==SceneSkipDisposition::NextAnimation){advanceSceneStep();return;}
    sceneStepTime_+=dt;float d=renderer_.animationDuration(sceneVisual_);if(d<=.11f)d=1.5f;
    if(sceneStepTime_>=d)advanceSceneStep();
}''')

(root/"tests/v0339_transition_scene_validator.cpp").write_text(r'''#include "OpenBorData.h"
#include "OpenBorSemantics.h"
#include <cmath>
#include <iostream>
using namespace ffx;
static bool near(float a,float b){return std::fabs(a-b)<.001f;}
int main(int argc,char**argv){
    int bad=0;auto check=[&](bool ok,const char*msg){if(!ok){++bad;std::cerr<<"FAIL: "<<msg<<"\n";}};
    check(near(openBorBuild3797VerticalScrollDelta(1.f),100.f),"vertical 100 px/s");
    check(near(openBorBuild3797VerticalScrollDelta(.5f),50.f),"vertical half-second delta");
    check(openBorInterLevelHealth(50,100,true)==55,"inter-level +5 HP");
    check(openBorInterLevelHealth(98,100,true)==100,"HP clamp");
    check(openBorInterLevelHealth(50,100,false)==100,"chapter boundary full HP");
    check(openBorSceneSkipDisposition(true,false,false)==SceneSkipDisposition::CloseScene,"default skip closes scene");
    check(openBorSceneSkipDisposition(true,true,false)==SceneSkipDisposition::NextAnimation,"skipone continues");
    check(openBorSceneSkipDisposition(true,false,true)==SceneSkipDisposition::Ignore,"noskip blocks input");
    if(argc>1){
        OpenBorDatabase db;std::string err;check(db.load(argv[1],&err),"database load");
        if(err.empty()&&db.campaign().stages.size()>=18){
            auto up=db.loadLevel(db.campaign().stages[17]);
            check(up.direction=="up"&&up.blocked!=0,"stage 18 up/blocked");
            auto logo=db.loadScene("scenes/logo.txt");int n=0;bool defaults=true;
            for(const auto& st:logo.steps)if(st.kind==SceneStep::Kind::Animation){++n;defaults=defaults&&!st.skip&&!st.noskip;}
            check(n==4&&defaults,"logo scene default skip flags");
        }
    }
    if(bad){std::cerr<<"v0339_transition_scene_failures="<<bad<<"\n";return 3;}
    std::cout<<"v0339_transition_scene=OK vertical=100px/s interlevel_hp=+5 scene_skip=Build3797\n";return 0;
}
''',encoding="utf-8")
print("v0.3.39 transition/scene patch applied")
