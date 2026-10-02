from pathlib import Path

root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit(f"v0.3.34 data anchor missing: {rel}: {old[:100]!r}")
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("CMakeLists.txt",
    "project(FinalFightXNative VERSION 0.3.33 LANGUAGES CXX)",
    "project(FinalFightXNative VERSION 0.3.34 LANGUAGES CXX)")

rep("src/OpenBorData.h",
'''    int landingDamage=0, landingMode=0;
    bool customDrop=false;''',
'''    int landingDamage=0;
    bool blast=false, customDrop=false;''')

rep("src/OpenBorData.h",
'''    bool attackOne=false, fastAttack=false;
    std::string projectileModel;''',
'''    bool attackOne=false, fastAttack=false;
    float bounceFactor=4.f;
    std::string projectileModel;''')

rep("src/OpenBorData.h",
'''    int throwFrameWait=-1, throwDamage=21;
    float throwDist=2.f, throwHeight=4.f;''',
'''    int throwFrameWait=-1, throwDamage=21;
    float throwDist=2.5f, throwHeight=0.f;''')

rep("src/OpenBorData.cpp",
'''        if(cmd=="attackone"&&t.size()>1){cur->attackOne=toInt(t[1])!=0;continue;}
        if(cmd=="range"&&t.size()>1){cur->rangeMin=toFloat(t[1]);cur->rangeMax=t.size()>2?toFloat(t[2]):cur->rangeMin+40.f;continue;}''',
'''        if(cmd=="attackone"&&t.size()>1){cur->attackOne=toInt(t[1])!=0;continue;}
        if(cmd=="bouncefactor"&&t.size()>1){cur->bounceFactor=std::max(1.f,std::abs(toFloat(t[1],4.f)));continue;}
        if(cmd=="range"&&t.size()>1){cur->rangeMin=toFloat(t[1]);cur->rangeMax=t.size()>2?toFloat(t[2]):cur->rangeMin+40.f;continue;}''')

rep("src/OpenBorData.cpp",
'''        if(cmd=="damageonlanding"){atk.landingDamage=t.size()>1?toInt(t[1]):0;atk.landingMode=t.size()>2?toInt(t[2]):0;continue;}''',
'''        if(cmd=="damageonlanding"){atk.landingDamage=t.size()>1?toInt(t[1]):0;atk.blast=t.size()>2?toInt(t[2])!=0:false;continue;}''')

rep("src/OpenBorData.cpp",
'''            const int landingDamage=atk.landingDamage;
            const int landingMode=atk.landingMode;
            atk={};
            atk.landingDamage=landingDamage;
            atk.landingMode=landingMode;''',
'''            const int landingDamage=atk.landingDamage;
            const bool blast=atk.blast;
            atk={};
            atk.landingDamage=landingDamage;
            atk.blast=blast;''')

sem=Path("src/OpenBorSemantics.h")
s=sem.read_text(encoding="utf-8-sig")
anchor='''inline bool isReverseDirection(const std::string& rawDirection){'''
insert='''struct JumpFrameMotion { float lift=0.f,x=0.f,z=0.f; };

inline JumpFrameMotion openBorJumpFrameMotion(const std::string& rawType,float jumpHeight,float authoredLift,bool hasAuthoredX,float authoredX=0.f,float authoredZ=0.f){
    JumpFrameMotion out;out.lift=authoredLift;
    const auto type=semanticLower(rawType);
    if(hasAuthoredX){out.x=authoredX;out.z=authoredZ;return out;}
    if(out.lift<=0.f){
        if(type=="player"){out.lift=jumpHeight*.5f;out.x=2.f;}
        else {out.lift=jumpHeight;out.x=0.f;}
        return out;
    }
    if(type=="enemy"||type=="npc")out.x=1.3f;
    return out;
}

inline float openBorBounceVelocity(float impactSpeed,float bounceFactor,float unitScale=42.f){
    bounceFactor=std::max(1.f,std::abs(bounceFactor));
    return impactSpeed>(2.f*unitScale)?impactSpeed/bounceFactor:0.f;
}

'''
if anchor not in s:
    raise SystemExit("v0.3.34 semantics anchor missing")
sem.write_text(s.replace(anchor,insert+anchor,1),encoding="utf-8")

# Small native semantic validator, independent from game assets.
test=Path("tests/v0334_semantics_validator.cpp")
test.write_text(r'''#include "OpenBorSemantics.h"
#include <cmath>
#include <iostream>
using namespace ffx;
static bool near(float a,float b){return std::fabs(a-b)<0.001f;}
int main(){
    auto enemy=openBorJumpFrameMotion("enemy",4.f,4.f,false);
    auto player=openBorJumpFrameMotion("player",4.f,3.f,false);
    auto fallback=openBorJumpFrameMotion("player",4.f,0.f,false);
    auto authored=openBorJumpFrameMotion("enemy",4.f,2.f,true,-2.f,0.f);
    bool ok=near(enemy.lift,4.f)&&near(enemy.x,1.3f)&&near(enemy.z,0.f)
        &&near(player.lift,3.f)&&near(player.x,0.f)
        &&near(fallback.lift,2.f)&&near(fallback.x,2.f)
        &&near(authored.lift,2.f)&&near(authored.x,-2.f)
        &&near(openBorBounceVelocity(168.f,4.f),42.f)
        &&near(openBorBounceVelocity(80.f,4.f),0.f);
    std::cout<<"v0334_semantics="<<(ok?"OK":"FAIL")
             <<" enemyDefaultX="<<enemy.x
             <<" playerFallbackLift="<<fallback.lift
             <<" bounce168="<<openBorBounceVelocity(168.f,4.f)<<"\\n";
    return ok?0:1;
}
''',encoding="utf-8")

cm=Path("CMakeLists.txt")
s=cm.read_text(encoding="utf-8-sig")
anchor='''target_include_directories(ffx_data PUBLIC src)

if(WIN32)'''
insert='''target_include_directories(ffx_data PUBLIC src)

add_executable(v0334_semantics_validator tests/v0334_semantics_validator.cpp)
target_link_libraries(v0334_semantics_validator PRIVATE ffx_data)

if(WIN32)'''
if anchor not in s:
    raise SystemExit("v0.3.34 cmake test anchor missing")
cm.write_text(s.replace(anchor,insert,1),encoding="utf-8")

print("v0.3.34 data/semantics patch applied")
