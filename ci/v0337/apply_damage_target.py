from pathlib import Path
root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit("v0.3.37 anchor missing: "+rel+" :: "+old[:100])
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("CMakeLists.txt","project(FinalFightXNative VERSION 0.3.36 LANGUAGES CXX)","project(FinalFightXNative VERSION 0.3.37 LANGUAGES CXX)")
rep("CMakeLists.txt",
'''add_executable(v0336_transition_validator tests/v0336_transition_validator.cpp)
target_link_libraries(v0336_transition_validator PRIVATE ffx_data)
''',
'''add_executable(v0336_transition_validator tests/v0336_transition_validator.cpp)
target_link_libraries(v0336_transition_validator PRIVATE ffx_data)

add_executable(v0337_damage_target_validator tests/v0337_damage_target_validator.cpp)
target_link_libraries(v0337_damage_target_validator PRIVATE ffx_data)
''')
rep("src/main_win.cpp","Native C++ Port v0.3.36","Native C++ Port v0.3.37")
rep("src/Game.cpp","NATIVE PORT v0.3.36","NATIVE PORT v0.3.37")
rep("src/Game.h",
'''    bool isHostile(const Actor&att,const Actor&target) const;
    bool canDamage(const Actor&att,const Actor&target) const;''',
'''    std::string combatType(const Actor&actor) const;
    bool isHostile(const Actor&att,const Actor&target) const;
    bool canDamage(const Actor&att,const Actor&target) const;''')

p=root/"src/OpenBorSemantics.h"
s=p.read_text(encoding="utf-8-sig")
anchor='inline bool isReverseDirection(const std::string& rawDirection){'
insert=r'''inline std::string openBorEffectiveCombatType(const std::string& rawModelType,const std::string& rawRole,bool projectile=false){
    if(projectile)return "shot";
    auto type=semanticLower(rawModelType),role=semanticLower(rawRole);
    // Weapon models in legacy OpenBOR copy the entity's original modeldata type.
    // The native port swaps the definition pointer instead, so recover that persistent role here.
    if(type.empty()||type=="none"||type=="icon"){
        if(role=="player"||role=="enemy"||role=="npc")return role;
    }
    return type;
}

inline bool openBorLegacyDefaultCanDamage(const std::string& rawAttackerType,const std::string& rawSubtype,const std::string& rawTargetType){
    auto a=semanticLower(rawAttackerType),sub=semanticLower(rawSubtype),t=semanticLower(rawTargetType);
    if(a=="enemy")return t=="player"||t=="shot"||(sub=="arrow"&&t=="obstacle");
    if(a=="player")return t=="player"||t=="enemy"||t=="obstacle";
    if(a=="trap"||a=="obstacle"||a=="pshot"||a=="shot")return t=="enemy"||t=="player"||t=="obstacle";
    if(a=="npc")return t=="enemy"||t=="obstacle";
    if(a=="item")return t=="player";
    return false;
}

inline bool openBorLegacyDefaultHostile(const std::string& rawAttackerType,const std::string& rawTargetType){
    auto a=semanticLower(rawAttackerType),t=semanticLower(rawTargetType);
    if(a=="enemy")return t=="player";
    if(a=="player")return t=="player"||t=="enemy"||t=="obstacle";
    if(a=="pshot"||a=="shot"||a=="npc")return t=="enemy";
    return false;
}

'''
if anchor not in s: raise SystemExit("v0.3.37 semantic anchor missing")
p.write_text(s.replace(anchor,insert+anchor,1),encoding="utf-8")

rep("src/Game.cpp",
r'''bool Game::isHostile(const Actor&att,const Actor&t)const{
    if(att.id==t.id||t.dead||t.effect)return false;
    if(att.def&&!att.def->hostile.empty()){
        if(containsType(att.def->hostile,"none"))return false;
        std::string type=t.projectile?"shot":(t.def?lower(t.def->type):std::string{});
        return !type.empty()&&containsType(att.def->hostile,type);
    }
    if(att.team==Team::Enemy)return t.team==Team::Player||t.team==Team::Ally;
    if(att.team==Team::Player||att.team==Team::Ally)return t.team==Team::Enemy;
    return false;
}''',
r'''std::string Game::combatType(const Actor&a)const{
    std::string role;
    if(a.team==Team::Player)role="player";else if(a.team==Team::Enemy)role="enemy";else if(a.team==Team::Ally)role="npc";
    return openBorEffectiveCombatType(a.def?a.def->type:std::string{},role,a.projectile);
}
bool Game::isHostile(const Actor&att,const Actor&t)const{
    if(att.id==t.id||t.dead||t.effect)return false;
    const std::vector<std::string>* authored=att.def?&att.def->hostile:nullptr;
    if(authored&&authored->empty()&&!att.baseModel.empty())if(auto base=db_.entity(att.baseModel))if(!base->hostile.empty())authored=&base->hostile;
    const auto targetType=combatType(t);
    if(authored&&!authored->empty()){
        if(containsType(*authored,"none"))return false;
        return !targetType.empty()&&containsType(*authored,targetType);
    }
    return openBorLegacyDefaultHostile(combatType(att),targetType);
}''')

rep("src/Game.cpp",
'''bool Game::canDamage(const Actor&att,const Actor&t)const{if(t.effect||t.projectile||att.id==t.id||att.projectileOwner==t.id)return false;if(t.def&&t.def->type=="obstacle")return true;if(att.def&&!att.def->canDamage.empty()){if(!t.def||!containsType(att.def->canDamage,t.def->type))return false;if(att.team==Team::Player&&t.team==Team::Player&&!db_.rules().versusDamage)return false;return true;}if(att.team==Team::Enemy)return t.team==Team::Player||t.team==Team::Ally;if(att.team==Team::Player){if(t.team==Team::Player)return db_.rules().versusDamage;return t.team==Team::Enemy;}if(att.team==Team::Ally)return t.team==Team::Enemy;return false;}''',
'''bool Game::canDamage(const Actor&att,const Actor&t)const{if(t.effect||t.projectile||att.id==t.id||att.projectileOwner==t.id)return false;const std::vector<std::string>* authored=att.def?&att.def->canDamage:nullptr;if(authored&&authored->empty()&&!att.baseModel.empty())if(auto base=db_.entity(att.baseModel))if(!base->canDamage.empty())authored=&base->canDamage;const auto targetType=combatType(t);bool allowed=false;if(authored&&!authored->empty())allowed=containsType(*authored,targetType);else allowed=openBorLegacyDefaultCanDamage(combatType(att),att.def?att.def->subtype:std::string{},targetType);if(!allowed)return false;if(att.team==Team::Player&&t.team==Team::Player&&!db_.rules().versusDamage)return false;return true;}''')

rep("src/Game.cpp",
r'''            if(an->followAnim>0&&an->followCond>0){
                bool hostile=isHostile(att,t);
                bool alive=!t.dead;
                bool grabbable=!t.def||(t.def->antigrab-(att.def?att.def->grabForce:0))<=0;
                switch(an->followCond){
                    case 1:follow=true;break;
                    case 2:follow=hostile;break;
                    case 3:case 5:follow=hostile&&alive&&!blocked;break;
                    case 4:follow=hostile&&alive&&!blocked&&grabbable;break;
                    default:break;
                }
            }''',
r'''            if(an->followAnim>0&&an->followCond>0){
                // Build 3797 checks the attack damage mask (them), not AI hostile, for followcond >= 2.
                // updateCombat already rejected contacts outside canDamage(), so the type condition is satisfied here.
                const bool damageTypeAllowed=canDamage(att,t);
                const bool alive=!t.dead;
                const bool grabbable=!t.def||(t.def->antigrab-(att.def?att.def->grabForce:0))<=0;
                const int cond=an->followCond;
                follow=(cond<2||damageTypeAllowed)&&(cond<3||(alive&&!blocked))&&(cond<4||grabbable);
            }''')

(root/"tests/v0337_damage_target_validator.cpp").write_text(r'''#include "OpenBorData.h"
#include "OpenBorSemantics.h"
#include <algorithm>
#include <iostream>
using namespace ffx;
static bool has(const std::vector<std::string>&v,const char*s){return std::find(v.begin(),v.end(),s)!=v.end();}
int main(int argc,char**argv){
    int bad=0;auto check=[&](bool ok,const char*msg){if(!ok){++bad;std::cerr<<"FAIL: "<<msg<<"\n";}};
    check(openBorEffectiveCombatType("none","player")=="player","armed player must remain player type");
    check(openBorEffectiveCombatType("none","enemy")=="enemy","armed enemy must remain enemy type");
    check(openBorEffectiveCombatType("obstacle","neutral")=="obstacle","obstacle type must remain obstacle");
    check(openBorEffectiveCombatType("none","player",true)=="shot","projectile effective type must be shot");
    check(openBorLegacyDefaultCanDamage("player","","enemy"),"default player should damage enemy");
    check(openBorLegacyDefaultCanDamage("player","","obstacle"),"default player should damage obstacle");
    check(!openBorLegacyDefaultCanDamage("player","","shot"),"default player should not damage shot");
    check(openBorLegacyDefaultCanDamage("enemy","","player"),"default enemy should damage player");
    check(openBorLegacyDefaultCanDamage("enemy","","shot"),"default enemy should damage shot");
    check(!openBorLegacyDefaultCanDamage("enemy","","obstacle"),"default enemy should not damage obstacle");
    check(openBorLegacyDefaultCanDamage("enemy","arrow","obstacle"),"legacy enemy arrow should damage obstacle");
    check(openBorLegacyDefaultCanDamage("npc","","enemy"),"default npc should damage enemy");
    check(openBorLegacyDefaultCanDamage("npc","","obstacle"),"default npc should damage obstacle");
    check(openBorLegacyDefaultHostile("enemy","player"),"default enemy hostile player");
    check(!openBorLegacyDefaultHostile("enemy","npc"),"default enemy not hostile npc");
    if(argc>1){
        OpenBorDatabase db;std::string err;check(db.load(argv[1],&err),"database load");
        auto shell=db.entity("shell"),arrow=db.entity("arrow"),knife=db.entity("knife_"),guy=db.entity("guy"),pipe=db.entity("guy_pipe");
        check(shell&&has(shell->canDamage,"player")&&!has(shell->canDamage,"obstacle"),"shell authored candamage excludes obstacle");
        check(arrow&&has(arrow->canDamage,"enemy")&&!has(arrow->canDamage,"obstacle"),"arrow authored candamage excludes obstacle");
        check(knife&&has(knife->canDamage,"npc")&&!has(knife->canDamage,"obstacle"),"knife authored candamage excludes obstacle");
        check(guy&&guy->type=="player"&&has(guy->canDamage,"enemy"),"guy base combat mask");
        check(pipe&&pipe->type=="none"&&pipe->canDamage.empty(),"guy_pipe relies on persistent player modeldata");
    }
    if(bad){std::cerr<<"v0337_damage_target_failures="<<bad<<"\n";return 3;}
    std::cout<<"v0337_damage_target=OK\n";return 0;
}
''',encoding="utf-8")

(root/"CHANGELOG-v0.3.37.txt").write_text("""Final Fight X Native - v0.3.37 Build 3797 Damage Target Fidelity

- Armed player/enemy models keep their effective combat type after the native definition swap.
- Authored candamage now controls obstacle hits; the unconditional obstacle bypass was removed.
- Legacy candamage defaults are applied when a model does not author the command.
- hostile remains an AI target-selection rule and sees the persistent combat type of armed actors.
- followcond uses the damage-type eligibility mask, matching Build 3797, instead of AI hostile.
- followcond threshold logic is exact for conditions 1 through 5.
- PAK projectile audit: shell, arrow and knife_ intentionally exclude obstacle from candamage.
""",encoding="utf-8")
(root/"PAK-FIDELITY-AUDIT-v0.3.37.txt").write_text("""Final Fight X v1.0.0 / OpenBOR v3.0 Build 3797

Combat targeting audit:
candamage declarations=37
hostile declarations=24
projectilehit declarations=0
followanim/followcond uses=5, all authored followcond 3

Important PAK cases:
shell / arrow / knife_: candamage player npc enemy (no obstacle)
guy/cody/haggar: candamage enemy obstacle npc player
weapon player models use type none and therefore must retain the player's effective TYPE_PLAYER at runtime.
""",encoding="utf-8")
(root/"README-SOURCE-v0.3.37.txt").write_text("""Final Fight X Native v0.3.37 - Source

cmake -S . -B build -A x64
cmake --build build --config Release --target FinalFightX v0337_damage_target_validator -- /m

Place FinalFightX.exe beside the assets folder from the portable package.
""",encoding="utf-8")
print("v0.3.37 damage target patch applied")
