from pathlib import Path

root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit("v0.3.35 runtime anchor missing: "+rel+" :: "+old[:100])
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("src/main_win.cpp","Native C++ Port v0.3.34","Native C++ Port v0.3.35")
rep("src/Game.cpp","NATIVE PORT v0.3.34","NATIVE PORT v0.3.35")

rep("src/Game.h",
'''    size_t nextSpawn_=0,nextWait_=0,nextGroup_=0,nextBlockade_=0;
    int groupMin_=0,groupMax_=100;
    int bossesDeclared_=0,bossesRemaining_=0;
    bool groupRefillLocked_=false,bossClearTriggered_=false;''',
'''    size_t nextLevelAction_=0;
    int groupMin_=100,groupMax_=100;
    int bossesDeclared_=0,bossesRemaining_=0;
    bool levelWaiting_=false,bossClearTriggered_=false;''')

rep("src/Game.h",
'''    bool canSpawnGroupedEnemy();
    bool isReverseScroll() const;''',
'''    bool isReverseScroll() const;''')

rep("src/Game.cpp",
'''playerId_=0;nextSpawn_=nextWait_=nextGroup_=nextBlockade_=0;groupMin_=0;groupMax_=100;bossesDeclared_=(int)std::count_if(level_.spawns.begin(),level_.spawns.end(),[](const SpawnDef& sp){return sp.boss;});bossesRemaining_=bossesDeclared_;groupRefillLocked_=false;bossClearTriggered_=false;''',
'''playerId_=0;nextLevelAction_=0;groupMin_=groupMax_=100;levelWaiting_=false;bossesDeclared_=(int)std::count_if(level_.spawns.begin(),level_.spawns.end(),[](const SpawnDef& sp){return sp.boss;});bossesRemaining_=bossesDeclared_;bossClearTriggered_=false;''')

rep("src/Game.cpp",
'''updateLevelRules();while(nextSpawn_<level_.spawns.size()&&level_.spawns[nextSpawn_].trigger<=0){auto&s0=level_.spawns[nextSpawn_];auto d0=db_.entity(s0.model);bool enemy0=d0&&d0->type=="enemy";if(enemy0&&!canSpawnGroupedEnemy())break;spawnActor(s0);++nextSpawn_;}audio_.playMusic(level_.music,true);''',
'''updateLevelRules();audio_.playMusic(level_.music,true);''')

rep("src/Game.cpp",
'''void Game::advanceLevelProgress(float dt){
    if(isVerticalScroll()){levelProgress_=std::max(levelProgress_,std::abs(verticalStageScroll_));maxLevelProgress_=std::max(maxLevelProgress_,levelProgress_);return;}
    float maxCam=std::max(0.f,level_.worldWidth-renderer_.logicalWidth()),cam=cameraProgress();
    bool atEnd=maxCam<=.5f||(isReverseScroll()?cameraX_<=.5f:cameraX_>=maxCam-.5f);
    if(!atEnd){levelProgress_=cam;maxLevelProgress_=std::max(maxLevelProgress_,levelProgress_);return;}
    float next=std::max(levelProgress_,cam)+std::max(0.f,dt)*100.f;
    if(nextWait_<level_.waits.size()){
        float gate=level_.waits[nextWait_];
        bool pending=nextSpawn_<level_.spawns.size()&&level_.spawns[nextSpawn_].trigger<=gate+5.f;
        if(levelProgress_+5.f>=gate&&(enemiesAlive()||pending))next=std::min(next,gate);
    }
    levelProgress_=next;maxLevelProgress_=std::max(maxLevelProgress_,levelProgress_);
}
float Game::movementBoundary()const{
    float viewW=renderer_.logicalWidth();
    if(isReverseScroll())return std::min(level_.worldWidth-8.f,cameraX_+viewW-8.f);
    float limit=level_.worldWidth-15.f,prog=scrollProgress();
    auto pendingAt=[&](float gate){return nextSpawn_<level_.spawns.size()&&level_.spawns[nextSpawn_].trigger<=gate+5.f;};
    if(nextWait_<level_.waits.size()){
        float gate=level_.waits[nextWait_];bool holdForPending=pendingAt(gate)&&prog>=gate-1.f;if(prog+5.f>=gate&&(enemiesAlive()||holdForPending))limit=std::min(limit,cameraX_+viewW-8.f);
    }
    return limit;
}
bool Game::canSpawnGroupedEnemy(){
    if(groupMax_>=100)return true;
    int active=activeEnemies();
    if(groupRefillLocked_){if(active<groupMin_)groupRefillLocked_=false;else return false;}
    if(active>=groupMax_){groupRefillLocked_=true;return false;}
    return true;
}
void Game::updateLevelRules(){
    float prog=scrollProgress();
    while(nextGroup_<level_.groups.size()&&level_.groups[nextGroup_].trigger<=prog+5.f){
        const auto&g=level_.groups[nextGroup_++];groupMin_=std::max(0,g.min);groupMax_=std::max(groupMin_,g.max);groupRefillLocked_=false;
    }
    if(isBidirectionalScroll()){
        while(nextBlockade_<level_.blockades.size()&&level_.blockades[nextBlockade_].trigger<=prog+5.f){
            backscrollFloor_=std::max(backscrollFloor_,std::max(0.f,level_.blockades[nextBlockade_].position));++nextBlockade_;
        }
    }else nextBlockade_=level_.blockades.size();
    auto resolved=[&](float gate){bool pending=nextSpawn_<level_.spawns.size()&&level_.spawns[nextSpawn_].trigger<=gate+5.f;return !enemiesAlive()&&!pending;};
    while(nextWait_<level_.waits.size()&&prog+5.f>=level_.waits[nextWait_]&&resolved(level_.waits[nextWait_])){
        ++nextWait_;stageTime_=(float)level_.setTime;
    }
}''',
'''void Game::advanceLevelProgress(float dt){
    if(isVerticalScroll()){
        if(!levelWaiting_)levelProgress_=std::max(levelProgress_,std::abs(verticalStageScroll_));
        maxLevelProgress_=std::max(maxLevelProgress_,levelProgress_);return;
    }
    float maxCam=std::max(0.f,level_.worldWidth-renderer_.logicalWidth()),cam=cameraProgress();
    if(levelWaiting_){levelProgress_=std::max(levelProgress_,cam);maxLevelProgress_=std::max(maxLevelProgress_,levelProgress_);return;}
    bool atEnd=maxCam<=.5f||(isReverseScroll()?cameraX_<=.5f:cameraX_>=maxCam-.5f);
    if(!atEnd){levelProgress_=cam;maxLevelProgress_=std::max(maxLevelProgress_,levelProgress_);return;}
    levelProgress_=std::max(levelProgress_,cam)+std::max(0.f,dt)*100.f;
    maxLevelProgress_=std::max(maxLevelProgress_,levelProgress_);
}
float Game::movementBoundary()const{
    float viewW=renderer_.logicalWidth();
    if(isReverseScroll())return std::min(level_.worldWidth-8.f,cameraX_+viewW-8.f);
    float limit=level_.worldWidth-15.f;
    if(levelWaiting_)limit=std::min(limit,cameraX_+viewW-8.f);
    return limit;
}
void Game::updateLevelRules(){
    const float prog=scrollProgress();
    int active=activeEnemies();
    if(openBorLevelQueueCanStart(active,groupMin_)){
        while(nextLevelAction_<level_.actions.size()){
            const auto action=level_.actions[nextLevelAction_];
            if(action.trigger>prog+.5f||!openBorLevelQueueHasCapacity(active,groupMax_))break;
            ++nextLevelAction_;
            if(action.kind==LevelActionKind::Wait){levelWaiting_=true;continue;}
            if(action.kind==LevelActionKind::Group){
                auto limits=openBorLevelGroupLimits(action.groupMin,action.groupMax);
                groupMin_=limits.min;groupMax_=limits.max;
                active=activeEnemies();continue;
            }
            if(action.kind==LevelActionKind::Blockade){
                if(isBidirectionalScroll())backscrollFloor_=std::max(0.f,action.blockade);
                continue;
            }
            if(action.kind==LevelActionKind::Spawn&&action.spawnIndex<level_.spawns.size()){
                spawnActor(level_.spawns[action.spawnIndex]);active=activeEnemies();
            }
        }
    }
    // Build 3797 handles spawn/group entries before clearing a wait gate.
    if(levelWaiting_&&!enemiesAlive()){
        levelWaiting_=false;
        if(!level_.noResetTime)stageTime_=(float)level_.setTime;
    }
}''')

rep("src/Game.cpp",
'''bool Game::stageSequenceComplete()const{
    return nextSpawn_>=level_.spawns.size()&&nextWait_>=level_.waits.size()&&
           nextGroup_>=level_.groups.size()&&nextBlockade_>=level_.blockades.size()&&
           maxLevelProgress_+5.f>=level_.maxTrigger;
}''',
'''bool Game::stageSequenceComplete()const{
    return nextLevelAction_>=level_.actions.size()&&maxLevelProgress_+.5f>=level_.maxTrigger;
}''')

rep("src/Game.cpp",
'''nextSpawn_=level_.spawns.size();nextWait_=level_.waits.size();nextGroup_=level_.groups.size();nextBlockade_=level_.blockades.size();''',
'''nextLevelAction_=level_.actions.size();levelWaiting_=false;''')

rep("src/Game.cpp",
'''if(isVerticalScroll()){float sign=lower(level_.direction)=="up"?-1.f:1.f;verticalStageScroll_+=sign*30.f*dt;cameraX_=0;}''',
'''if(isVerticalScroll()){if(!levelWaiting_){float sign=lower(level_.direction)=="up"?-1.f:1.f;verticalStageScroll_+=sign*30.f*dt;}cameraX_=0;}''')

rep("src/Game.cpp",
'''        auto pendingAt=[&](float gate){return nextSpawn_<level_.spawns.size()&&level_.spawns[nextSpawn_].trigger<=gate+5.f;};
        if(nextWait_<level_.waits.size()){
            float gate=level_.waits[nextWait_];
            bool holdForPending=pendingAt(gate)&&prog>=gate-1.f;
            if(desiredProg>=gate-5.f&&(enemiesAlive()||holdForPending))desiredProg=std::min(desiredProg,gate);
        }
        if(isBidirectionalScroll())desiredProg=std::max(desiredProg,backscrollFloor_);
        else{furthestProgress_=std::max(furthestProgress_,desiredProg);desiredProg=furthestProgress_;}
        desired=cameraForProgress(desiredProg);''',
'''        if(isBidirectionalScroll())desiredProg=std::max(desiredProg,backscrollFloor_);
        else{furthestProgress_=std::max(furthestProgress_,desiredProg);desiredProg=furthestProgress_;}
        if(levelWaiting_)desiredProg=prog;
        desired=cameraForProgress(desiredProg);''')

rep("src/Game.cpp",
'''    advanceLevelProgress(dt);
    updateLevelRules();float prog=scrollProgress();
    while(nextSpawn_<level_.spawns.size()&&level_.spawns[nextSpawn_].trigger<=prog+5.f){
        auto&s0=level_.spawns[nextSpawn_];auto d0=db_.entity(s0.model);bool enemy=d0&&d0->type=="enemy";
        if(enemy&&!canSpawnGroupedEnemy())break;
        spawnActor(s0);++nextSpawn_;
    }
    cleanupDead();''',
'''    advanceLevelProgress(dt);
    cleanupDead();''')

print("v0.3.35 level queue runtime patch applied")
