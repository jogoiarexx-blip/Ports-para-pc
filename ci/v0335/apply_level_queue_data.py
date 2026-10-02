from pathlib import Path

root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit("v0.3.35 data anchor missing: "+rel)
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("CMakeLists.txt",
    "project(FinalFightXNative VERSION 0.3.34 LANGUAGES CXX)",
    "project(FinalFightXNative VERSION 0.3.35 LANGUAGES CXX)")

rep("CMakeLists.txt",
'''add_executable(v0334_semantics_validator tests/v0334_semantics_validator.cpp)
target_link_libraries(v0334_semantics_validator PRIVATE ffx_data)
''',
'''add_executable(v0334_semantics_validator tests/v0334_semantics_validator.cpp)
target_link_libraries(v0334_semantics_validator PRIVATE ffx_data)

add_executable(v0335_level_actions_validator tests/v0335_level_actions_validator.cpp)
target_link_libraries(v0335_level_actions_validator PRIVATE ffx_data)
''')

rep("src/OpenBorData.h",
'''struct LevelGroup { float trigger=0; int min=0,max=0; };
struct LevelBlockade { float trigger=0, position=0; };
struct LevelWall {''',
'''struct LevelGroup { float trigger=0; int min=0,max=0; };
struct LevelBlockade { float trigger=0, position=0; };
enum class LevelActionKind { Spawn, Wait, Group, Blockade };
struct LevelAction {
    LevelActionKind kind=LevelActionKind::Spawn;
    float trigger=0;
    size_t spawnIndex=0;
    int groupMin=0,groupMax=0;
    float blockade=0;
};
struct LevelWall {''')

rep("src/OpenBorData.h",
'''    std::vector<LevelBlockade> blockades;
    std::vector<float> waits;
    std::vector<LevelGroup> groups;
    std::vector<LevelWall> walls;''',
'''    std::vector<LevelBlockade> blockades;
    std::vector<float> waits;
    std::vector<LevelGroup> groups;
    std::vector<LevelAction> actions;
    std::vector<LevelWall> walls;''')

rep("src/OpenBorData.cpp",
'''    SpawnDef pending{}; bool havePending=false; float lastAt=0; std::vector<std::pair<std::string,std::vector<std::string>>> pendingLevelEvents;
    auto flushSpawn=[&](float trigger){if(havePending){pending.trigger=trigger;l.spawns.push_back(pending);pending={};havePending=false;}};
    auto flushEvents=[&](float trigger){for(auto&e:pendingLevelEvents){
        if(e.first=="wait")l.waits.push_back(trigger);
        else if(e.first=="group"&&e.second.size()>=2)l.groups.push_back({trigger,toInt(e.second[0]),toInt(e.second[1])});
        else if(e.first=="blockade"&&!e.second.empty())l.blockades.push_back({trigger,toFloat(e.second[0])});
    }pendingLevelEvents.clear();};''',
'''    SpawnDef pending{}; bool havePending=false; float lastAt=0; std::vector<std::pair<std::string,std::vector<std::string>>> pendingLevelEvents;
    auto flushSpawn=[&](float trigger){if(havePending){pending.trigger=trigger;size_t idx=l.spawns.size();l.spawns.push_back(pending);l.actions.push_back({LevelActionKind::Spawn,trigger,idx});pending={};havePending=false;}};
    auto flushEvents=[&](float trigger){for(auto&e:pendingLevelEvents){
        if(e.first=="wait"){l.waits.push_back(trigger);l.actions.push_back({LevelActionKind::Wait,trigger});}
        else if(e.first=="group"&&e.second.size()>=2){int mn=toInt(e.second[0]),mx=toInt(e.second[1]);l.groups.push_back({trigger,mn,mx});LevelAction a;a.kind=LevelActionKind::Group;a.trigger=trigger;a.groupMin=mn;a.groupMax=mx;l.actions.push_back(a);}
        else if(e.first=="blockade"&&!e.second.empty()){float pos=toFloat(e.second[0]);l.blockades.push_back({trigger,pos});LevelAction a;a.kind=LevelActionKind::Blockade;a.trigger=trigger;a.blockade=pos;l.actions.push_back(a);}
    }pendingLevelEvents.clear();};''')

rep("src/OpenBorData.cpp",
'''    // OpenBOR level files are allowed to declare events out of numeric order.
    // Runtime spawning is progress-based, so normalize trigger order while preserving
    // author order for actors/events sharing the same trigger.
    std::stable_sort(l.spawns.begin(),l.spawns.end(),[](const auto&a,const auto&b){return a.trigger<b.trigger;});
    std::stable_sort(l.blockades.begin(),l.blockades.end(),[](const auto&a,const auto&b){return a.trigger<b.trigger;});
    std::sort(l.waits.begin(),l.waits.end());
    std::stable_sort(l.groups.begin(),l.groups.end(),[](const auto&a,const auto&b){return a.trigger<b.trigger;});
    return l;''',
'''    // Build 3797 keeps a single sequential spawn/event stream.
    // Do not sort by trigger: authored out-of-order entries intentionally gate later entries.
    return l;''')

sem=Path("src/OpenBorSemantics.h")
s=sem.read_text(encoding="utf-8-sig")
anchor='''inline bool isReverseDirection(const std::string& rawDirection){'''
insert='''struct LevelGroupLimits { int min=100,max=100; };

inline LevelGroupLimits openBorLevelGroupLimits(int authoredMin,int authoredMax){
    LevelGroupLimits out;
    out.min=authoredMin<1?100:authoredMin;
    out.max=authoredMax<1?1:authoredMax;
    return out;
}
inline bool openBorLevelQueueCanStart(int activeEnemies,int groupMin){return activeEnemies<groupMin;}
inline bool openBorLevelQueueHasCapacity(int activeEnemies,int groupMax){return activeEnemies<groupMax;}

'''
if anchor not in s:
    raise SystemExit("v0.3.35 semantics anchor missing")
sem.write_text(s.replace(anchor,insert+anchor,1),encoding="utf-8")

Path("tests/v0335_level_actions_validator.cpp").write_text(r'''#include "OpenBorData.h"
#include "OpenBorSemantics.h"
#include <iostream>
#include <string>
using namespace ffx;

static const char* kind(LevelActionKind k){
    switch(k){case LevelActionKind::Spawn:return "spawn";case LevelActionKind::Wait:return "wait";case LevelActionKind::Group:return "group";case LevelActionKind::Blockade:return "blockade";}
    return "?";
}

int main(int argc,char**argv){
    if(argc<2){std::cerr<<"usage: validator <data-root>\n";return 2;}
    OpenBorDatabase db;std::string err;
    if(!db.load(argv[1],&err)){std::cerr<<"FAIL load: "<<err<<"\n";return 2;}
    int bad=0;auto check=[&](bool ok,const std::string& msg){if(!ok){++bad;std::cerr<<"FAIL: "<<msg<<"\n";}};
    size_t spawns=0,waits=0,groups=0,blockades=0,actions=0,inversions=0;
    for(const auto& st:db.campaign().stages){
        auto l=db.loadLevel(st);actions+=l.actions.size();spawns+=l.spawns.size();waits+=l.waits.size();groups+=l.groups.size();blockades+=l.blockades.size();
        for(size_t i=1;i<l.actions.size();++i)if(l.actions[i].trigger<l.actions[i-1].trigger)++inversions;
    }
    check(spawns==319,"expected 319 authored spawns");
    check(waits==29,"expected 29 waits");
    check(groups==17,"expected 17 groups");
    check(blockades==20,"expected 20 blockades");
    check(actions==385,"expected unified action count 385");
    check(inversions==7,"expected 7 authored trigger inversions");

    auto l=db.loadLevel(db.campaign().stages.at(15));
    struct Expect{LevelActionKind k;float at;const char* model;};
    const Expect exp[]={
        {LevelActionKind::Spawn,0,"yen_"},{LevelActionKind::Spawn,0,"whisky"},
        {LevelActionKind::Spawn,140,"el_gado"},{LevelActionKind::Spawn,100,"poison"},
        {LevelActionKind::Spawn,100,"j"},{LevelActionKind::Wait,140,""},
        {LevelActionKind::Spawn,140,"slash"},{LevelActionKind::Spawn,140,"axl"}
    };
    check(l.actions.size()>=std::size(exp),"64th.16 action list too short");
    for(size_t i=0;i<std::size(exp)&&i<l.actions.size();++i){
        const auto&a=l.actions[i];
        check(a.kind==exp[i].k,"64th.16 action kind mismatch");
        check(a.trigger==exp[i].at,"64th.16 action trigger mismatch");
        if(a.kind==LevelActionKind::Spawn){
            check(a.spawnIndex<l.spawns.size(),"spawn index out of bounds");
            if(a.spawnIndex<l.spawns.size())check(l.spawns[a.spawnIndex].model==exp[i].model,"64th.16 model mismatch");
        }
    }
    check(l.actions.size()>3&&l.actions[2].trigger==140&&l.actions[3].trigger==100,"64th.16 140 to 100 gate was sorted away");
    auto defaultGroup=openBorLevelGroupLimits(0,0);
    auto fourGroup=openBorLevelGroupLimits(4,4);
    check(defaultGroup.min==100&&defaultGroup.max==1,"legacy group zero normalization mismatch");
    check(fourGroup.min==4&&fourGroup.max==4,"group 4/4 normalization mismatch");
    check(openBorLevelQueueCanStart(3,4)&&!openBorLevelQueueCanStart(4,4),"groupmin refill gate mismatch");
    check(openBorLevelQueueHasCapacity(3,4)&&!openBorLevelQueueHasCapacity(4,4),"groupmax capacity gate mismatch");

    if(bad){std::cerr<<"v0335_level_action_failures="<<bad<<"\n";return 3;}
    std::cout<<"v0335_level_actions=OK actions="<<actions<<" inversions="<<inversions
             <<" spawns="<<spawns<<" waits="<<waits<<" groups="<<groups<<" blockades="<<blockades<<"\n";
    return 0;
}
''',encoding="utf-8")

Path("CHANGELOG-v0.3.35.txt").write_text("""Final Fight X Native - v0.3.35 Build 3797 Level Queue Fidelity

- Level spawn/event processing now uses one authored-order queue, like OpenBOR Build 3797.
- Removed trigger sorting that changed intentional out-of-order at entries.
- Preserves all 7 authored trigger inversions found in the PAK.
- wait, group, blockade and spawn now interact in file order instead of separate sorted lists.
- Group defaults and refill/capacity gates mirror the legacy engine.
- Wait gates freeze horizontal and vertical stage scrolling until enemies are cleared.
- Wait timer reset follows the level noreset rule.
- Bidirectional blockade changes are consumed in the same action stream.
- NPC animation-script audit retained: only Guy/Cody/Haggar NPC scripts are referenced, and none are spawned by the current level set.
""",encoding="utf-8")

Path("PAK-FIDELITY-AUDIT-v0.3.35.txt").write_text("""Final Fight X v1.0.0 / OpenBOR v3.0 Build 3797

Level action audit:
spawns=319
wait=29
group=17
blockade=20
unified actions=385
authored at inversions=7 across 64th.1, 64th.4, 64th.13 and 64th.16

Key regression case in 64th.16:
spawn el_gado at 140 is authored before poison/j at 100.
Build 3797 keeps this order; v0.3.34 sorted it. v0.3.35 preserves it.

Script audit:
animationscript references=3 (guy_npc, cody_npc, haggar_npc)
@cmd uses=9
subentity/spawnframe uses=0
No level in the current PAK directly spawns the three NPC models.
""",encoding="utf-8")

Path("README-SOURCE-v0.3.35.txt").write_text("""Final Fight X Native v0.3.35 - Source

cmake -S . -B build -A x64
cmake --build build --config Release --target FinalFightX v0335_level_actions_validator -- /m

Place FinalFightX.exe beside the assets folder from the portable package.
""",encoding="utf-8")

print("v0.3.35 level queue data patch applied")
