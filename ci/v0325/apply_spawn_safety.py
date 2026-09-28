from pathlib import Path

def rep(s, old, new, label):
    if old not in s:
        raise SystemExit(f"missing {label}")
    return s.replace(old,new,1)

p=Path("src/Game.cpp")
s=p.read_text()

marker='''static bool attackWouldBeBlocked(const Actor& target,const Actor& attacker,int rawDamage,const AttackBox* attack){
    if(attack&&attack->noBlock!=0)return false;
    if(target.anim!="block"||(target.def&&target.def->thold>0&&rawDamage>=target.def->thold))return false;
    // OpenBOR cannot block an attack from behind unless blockback is enabled.
    // Final Fight X does not declare blockback, so facing the attacker is required.
    return target.facingLeft ? attacker.x<=target.x : attacker.x>=target.x;
}
'''
helper='''static bool attackWouldBeBlocked(const Actor& target,const Actor& attacker,int rawDamage,const AttackBox* attack){
    if(attack&&attack->noBlock!=0)return false;
    if(target.anim!="block"||(target.def&&target.def->thold>0&&rawDamage>=target.def->thold))return false;
    // OpenBOR cannot block an attack from behind unless blockback is enabled.
    // Final Fight X does not declare blockback, so facing the attacker is required.
    return target.facingLeft ? attacker.x<=target.x : attacker.x>=target.x;
}

static bool playerSpawnBlockedByTerrain(const LevelDef& level,float x,float z){
    for(const auto&w:level.walls){
        if(openBorTerrainContainsPoint(w.x,w.z,w.upperLeft,w.lowerLeft,w.upperRight,w.lowerRight,w.depth,x,z))return true;
    }
    return false;
}
static std::pair<float,float> findSafePlayerSpawnPoint(const LevelDef& level,float x,float z,bool reverse,float viewW){
    z=clampf(z,level.zMin,level.zMax);
    auto free=[&](float sx,float sz){return !playerSpawnBlockedByTerrain(level,sx,sz);};
    if(free(x,z))return {x,z};

    // OpenBOR's spawnplayer() searches the playable depth band first and then
    // nudges X in the stage's forward direction until the player is outside
    // walls/holes. This port has no holes in Final Fight X, so mirror the wall
    // part while keeping the authored spawn point as close as possible.
    std::vector<float> depthCandidates;
    depthCandidates.push_back(z);
    for(int step=1;step<128;++step){
        bool added=false;
        float hi=z+3.f*step,lo=z-3.f*step;
        if(hi<=level.zMax){depthCandidates.push_back(hi);added=true;}
        if(lo>=level.zMin){depthCandidates.push_back(lo);added=true;}
        if(!added)break;
    }
    if(std::find(depthCandidates.begin(),depthCandidates.end(),level.zMin)==depthCandidates.end())depthCandidates.push_back(level.zMin);
    if(std::find(depthCandidates.begin(),depthCandidates.end(),level.zMax)==depthCandidates.end())depthCandidates.push_back(level.zMax);

    int xSteps=std::max(1,(int)std::ceil(std::max(32.f,viewW)/4.f));
    for(int step=0;step<=xSteps;++step){
        float sx=x+(reverse?-4.f:4.f)*(float)step;
        if(sx<8.f||sx>level.worldWidth-8.f)continue;
        for(float sz:depthCandidates)if(free(sx,sz))return {sx,sz};
    }
    return {x,z};
}
'''
s=rep(s,marker,helper,"spawn helper insertion")

old='''float mid=(level_.zMin+level_.zMax)*.5f;for(size_t i=0;i<slots_.size();++i){slots_[i].actorId=0;slots_[i].respawnTimer=0;if(!slots_[i].joined||slots_[i].lives<=0)continue;float px=cameraX_+(reverse?250.f-(float)i*24.f:70.f+(float)i*24.f);auto p=createActor(players_[(size_t)slots_[i].selected],px,mid+(float)((int)i-1)*10.f,reverse,true,false,0,{},Team::Player,(int)i);'''
new='''float mid=(level_.zMin+level_.zMax)*.5f;for(size_t i=0;i<slots_.size();++i){slots_[i].actorId=0;slots_[i].respawnTimer=0;if(!slots_[i].joined||slots_[i].lives<=0)continue;float rawX=cameraX_+(reverse?250.f-(float)i*24.f:70.f+(float)i*24.f),rawZ=mid+(float)((int)i-1)*10.f;auto safe=findSafePlayerSpawnPoint(level_,rawX,rawZ,reverse,renderer_.logicalWidth());float px=safe.first,pz=safe.second;auto p=createActor(players_[(size_t)slots_[i].selected],px,pz,reverse,true,false,0,{},Team::Player,(int)i);'''
s=rep(s,old,new,"loadStage player spawn")

old='''float mid=(level_.zMin+level_.zMax)*.5f;bool reverse=isReverseScroll();float px=cameraX_+(reverse?250.f-(float)i*24.f:70.f+(float)i*24.f);auto p=createActor(players_[(size_t)slots_[i].selected],px,mid+(float)((int)i-1)*10.f,reverse,true,false,0,{},Team::Player,(int)i);'''
new='''float mid=(level_.zMin+level_.zMax)*.5f;bool reverse=isReverseScroll();float rawX=cameraX_+(reverse?250.f-(float)i*24.f:70.f+(float)i*24.f),rawZ=mid+(float)((int)i-1)*10.f;auto safe=findSafePlayerSpawnPoint(level_,rawX,rawZ,reverse,renderer_.logicalWidth());float px=safe.first,pz=safe.second;auto p=createActor(players_[(size_t)slots_[i].selected],px,pz,reverse,true,false,0,{},Team::Player,(int)i);'''
s=rep(s,old,new,"respawn player")

s=s.replace("v0.3.24","v0.3.25")
p.write_text(s)

p=Path("src/main_win.cpp")
p.write_text(p.read_text().replace("v0.3.24","v0.3.25"))
print("apply_v0325_spawn_safety=OK")
