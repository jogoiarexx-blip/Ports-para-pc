from pathlib import Path

def replace_once(s, old, new, label):
    if old not in s:
        raise SystemExit(f"missing {label}")
    return s.replace(old, new, 1)

# Campaign structure + ordered scene / next semantics.
p=Path("src/OpenBorData.h"); s=p.read_text()
s=replace_once(
    s,
    'struct StageEntry { std::string file; float zMin=170,zMax=240; int chapter=0; };',
    '''struct StageEntry {
    std::string file;
    float zMin=170,zMax=240;
    int chapter=0;
    bool showCompleteAfter=false;
    std::vector<std::string> scenesAfter;
};''',
    "StageEntry"
)
s=replace_once(
    s,
    '''struct Campaign {
    std::vector<StageEntry> stages;
    std::vector<std::string> endScenes;
    int maxPlayers=3, lives=5, credits=6, continueScore=0, canSave=0;
    bool noSame=false;
    HudConfig hud;
};''',
    '''struct Campaign {
    std::vector<StageEntry> stages;
    std::vector<std::string> introScenes;
    std::vector<std::string> endScenes;
    std::array<int,4> scoreBonuses{{10000,1000,100,0}};
    int maxPlayers=3, lives=5, credits=6, continueScore=0, canSave=0;
    bool noSame=false, noShowComplete=false, showRushBonus=false;
    HudConfig hud;
};''',
    "Campaign"
)
p.write_text(s)

p=Path("src/OpenBorData.cpp"); s=p.read_text()
s=replace_once(
    s,
    'Campaign c; float zmin=170,zmax=240; int chapter=0;',
    'Campaign c; float zmin=170,zmax=240; int chapter=0,lastStage=-1; std::vector<std::string> pendingScenes;',
    "campaign state"
)
s=replace_once(
    s,
    'else if(cmd=="continuescore"&&t.size()>1)c.continueScore=toInt(t[1]);',
    '''else if(cmd=="continuescore"&&t.size()>1)c.continueScore=toInt(t[1]);
        else if(cmd=="noshowcomplete"&&t.size()>1)c.noShowComplete=toInt(t[1])!=0;
        else if(cmd=="showrushbonus"&&t.size()>1)c.showRushBonus=toInt(t[1])!=0;
        else if(cmd=="scbonuses"){for(size_t i=0;i<4&&i+1<t.size();++i)c.scoreBonuses[i]=toInt(t[i+1],c.scoreBonuses[i]);}''',
    "campaign bonus commands"
)
s=replace_once(
    s,
    '''        else if(cmd=="file"&&t.size()>1)c.stages.push_back({normalizeAsset(t[1]),zmin,zmax,chapter});
        else if(cmd=="next")++chapter;
        else if(cmd=="scene"&&t.size()>1)c.endScenes.push_back(normalizeAsset(t[1]));''',
    '''        else if(cmd=="file"&&t.size()>1){
            if(!pendingScenes.empty()){
                if(lastStage>=0)c.stages[(size_t)lastStage].scenesAfter.insert(c.stages[(size_t)lastStage].scenesAfter.end(),pendingScenes.begin(),pendingScenes.end());
                else c.introScenes.insert(c.introScenes.end(),pendingScenes.begin(),pendingScenes.end());
                pendingScenes.clear();
            }
            c.stages.push_back({normalizeAsset(t[1]),zmin,zmax,chapter});
            lastStage=(int)c.stages.size()-1;
        }
        else if(cmd=="next"){if(lastStage>=0)c.stages[(size_t)lastStage].showCompleteAfter=true;++chapter;}
        else if(cmd=="scene"&&t.size()>1)pendingScenes.push_back(normalizeAsset(t[1]));''',
    "campaign flow directives"
)
s=replace_once(
    s,
    '''    const auto lifeFile=p.parent_path()/"lifebar.txt";''',
    '''    if(!pendingScenes.empty())c.endScenes.insert(c.endScenes.end(),pendingScenes.begin(),pendingScenes.end());
    const auto lifeFile=p.parent_path()/"lifebar.txt";''',
    "campaign pending scenes flush"
)
p.write_text(s)

# Runtime stage-complete state machine + ordered scenes.
p=Path("src/Game.h"); s=p.read_text()
s=replace_once(
    s,
    'enum class Mode{Scene,Title,Options,Graphics,Controls,Pause,Select,Loading,Playing,Continue,GameOver,Finished};',
    'enum class Mode{Scene,StageComplete,Title,Options,Graphics,Controls,Pause,Select,Loading,Playing,Continue,GameOver,Finished};',
    "Mode"
)
s=replace_once(
    s,
    'int highScore_=0;',
    '''int highScore_=0;
    float completeTick_=0,completeIdle_=0;
    int completeStageNumber_=0,completeBeepCounter_=0;
    std::array<int,Input::MaxPlayers> completeClear_{},completeLife_{};''',
    "stage complete state"
)
s=replace_once(
    s,
    '''    bool sceneSkip_=false,sceneNoSkip_=false;''',
    '''    bool sceneSkip_=false,sceneNoSkip_=false;
    int sceneNextStage_=-1;''',
    "scene next stage"
)
s=replace_once(
    s,
    'void startSceneQueue(const std::vector<std::string>& files,Mode after);',
    '''void startSceneQueue(const std::vector<std::string>& files,Mode after,int nextStage=-1);
    void startStageComplete();
    void updateStageComplete(float dt);
    void finishStageComplete(bool awardRemainder=true);
    void advanceAfterStage();
    void drawStageComplete();''',
    "flow method declarations"
)
p.write_text(s)

p=Path("src/Game.cpp"); s=p.read_text()
s=replace_once(
    s,
    '''void Game::startGame(){credits_=db_.campaign().credits;for(auto&s:slots_)if(s.joined){s.lives=db_.campaign().lives;s.score=0;s.nextLifeScore=std::max(1,db_.rules().lifeScore);s.nextCreditScore=std::max(1,db_.rules().creditScore);s.respawnTimer=0;}continueTimer_=10.f;mode_=Mode::Playing;loadStage(0,false);}''',
    '''void Game::startGame(){credits_=db_.campaign().credits;for(auto&s:slots_)if(s.joined){s.lives=db_.campaign().lives;s.score=0;s.nextLifeScore=std::max(1,db_.rules().lifeScore);s.nextCreditScore=std::max(1,db_.rules().creditScore);s.respawnTimer=0;}continueTimer_=10.f;if(!db_.campaign().introScenes.empty())startSceneQueue(db_.campaign().introScenes,Mode::Playing,0);else{mode_=Mode::Playing;loadStage(0,false);}}''',
    "startGame"
)

old_scene='''void Game::startSceneQueue(const std::vector<std::string>&files,Mode after){sceneFiles_=files;sceneFileIndex_=sceneStep_=0;sceneStepTime_=0;sceneX_=sceneY_=0;sceneSkip_=sceneNoSkip_=false;sceneVisual_.clear();sceneAfterMode_=after;mode_=Mode::Scene;loadNextSceneFile();}
void Game::loadNextSceneFile(){if(sceneFileIndex_>=sceneFiles_.size()){sceneVisual_.clear();if(sceneAfterMode_==Mode::Title){enterTitle();return;}mode_=sceneAfterMode_;return;}scene_=db_.loadScene(sceneFiles_[sceneFileIndex_++]);sceneStep_=0;advanceSceneStep();}
void Game::advanceSceneStep(){sceneVisual_.clear();sceneStepTime_=0;sceneX_=sceneY_=0;sceneSkip_=sceneNoSkip_=false;while(true){if(sceneStep_>=scene_.steps.size()){loadNextSceneFile();return;}auto st=scene_.steps[sceneStep_++];if(st.kind==SceneStep::Kind::Music){audio_.playMusic(st.asset,st.loop);continue;}if(st.kind==SceneStep::Kind::Silence){audio_.stopMusic();continue;}sceneVisual_=st.asset;sceneX_=st.x;sceneY_=st.y;sceneSkip_=st.skip;sceneNoSkip_=st.noskip;return;}}
void Game::updateScene(float dt){auto in=input_.state(0);if(in.backPressed){sceneVisual_.clear();if(sceneAfterMode_==Mode::Title){enterTitle();return;}mode_=sceneAfterMode_;return;}if(sceneVisual_.empty()){advanceSceneStep();return;}sceneStepTime_+=dt;float d=renderer_.animationDuration(sceneVisual_);if(d<=.11f)d=1.5f;const bool userSkip=in.startPressed&&sceneSkip_&&!sceneNoSkip_;if(userSkip||sceneStepTime_>=d)advanceSceneStep();}'''
new_scene='''void Game::startSceneQueue(const std::vector<std::string>&files,Mode after,int nextStage){sceneFiles_=files;sceneFileIndex_=sceneStep_=0;sceneStepTime_=0;sceneX_=sceneY_=0;sceneSkip_=sceneNoSkip_=false;sceneNextStage_=nextStage;sceneVisual_.clear();sceneAfterMode_=after;mode_=Mode::Scene;loadNextSceneFile();}
void Game::loadNextSceneFile(){if(sceneFileIndex_>=sceneFiles_.size()){sceneVisual_.clear();if(sceneNextStage_>=0){int n=sceneNextStage_;sceneNextStage_=-1;loadStage(n,true);return;}if(sceneAfterMode_==Mode::Title){enterTitle();return;}mode_=sceneAfterMode_;return;}scene_=db_.loadScene(sceneFiles_[sceneFileIndex_++]);sceneStep_=0;advanceSceneStep();}
void Game::advanceSceneStep(){sceneVisual_.clear();sceneStepTime_=0;sceneX_=sceneY_=0;sceneSkip_=sceneNoSkip_=false;while(true){if(sceneStep_>=scene_.steps.size()){loadNextSceneFile();return;}auto st=scene_.steps[sceneStep_++];if(st.kind==SceneStep::Kind::Music){audio_.playMusic(st.asset,st.loop);continue;}if(st.kind==SceneStep::Kind::Silence){audio_.stopMusic();continue;}sceneVisual_=st.asset;sceneX_=st.x;sceneY_=st.y;sceneSkip_=st.skip;sceneNoSkip_=st.noskip;return;}}
void Game::updateScene(float dt){auto in=input_.state(0);if(in.backPressed){sceneVisual_.clear();if(sceneNextStage_>=0){int n=sceneNextStage_;sceneNextStage_=-1;loadStage(n,true);return;}if(sceneAfterMode_==Mode::Title){enterTitle();return;}mode_=sceneAfterMode_;return;}if(sceneVisual_.empty()){advanceSceneStep();return;}sceneStepTime_+=dt;float d=renderer_.animationDuration(sceneVisual_);if(d<=.11f)d=1.5f;const bool userSkip=in.startPressed&&sceneSkip_&&!sceneNoSkip_;if(userSkip||sceneStepTime_>=d)advanceSceneStep();}

void Game::advanceAfterStage(){
    int next=stageIndex_+1;
    if(stageIndex_>=0&&stageIndex_<(int)db_.campaign().stages.size()){
        const auto& st=db_.campaign().stages[(size_t)stageIndex_];
        if(!st.scenesAfter.empty()){startSceneQueue(st.scenesAfter,Mode::Playing,next);return;}
    }
    loadStage(next,true);
}
void Game::startStageComplete(){
    if(stageIndex_<0||stageIndex_>=(int)db_.campaign().stages.size()){advanceAfterStage();return;}
    const auto& st=db_.campaign().stages[(size_t)stageIndex_];
    completeStageNumber_=st.chapter+1;completeTick_=completeIdle_=0;completeBeepCounter_=0;completeClear_.fill(0);completeLife_.fill(0);
    int clear=db_.campaign().scoreBonuses[3]?completeStageNumber_*db_.campaign().scoreBonuses[0]:db_.campaign().scoreBonuses[0];
    int lifePer=std::max(0,db_.campaign().scoreBonuses[1]);
    for(size_t i=0;i<slots_.size();++i)if(slots_[i].joined&&slots_[i].lives>0){completeClear_[i]=std::max(0,clear);completeLife_[i]=std::max(0,slots_[i].lives*lifePer);}
    audio_.playMusic("music/complete.bor",false);clearTimer_=0;mode_=Mode::StageComplete;
}
void Game::finishStageComplete(bool awardRemainder){
    if(awardRemainder)for(size_t i=0;i<slots_.size();++i){int rest=completeClear_[i]+completeLife_[i];if(rest>0)registerScore((int)i,rest);completeClear_[i]=completeLife_[i]=0;}
    completeTick_=completeIdle_=0;advanceAfterStage();
}
void Game::updateStageComplete(float dt){
    auto in=input_.uiState();
    bool skip=in.startPressed||in.attackPressed||in.attack2Pressed||in.jumpPressed||in.specialPressed||in.backPressed;
    if(skip){finishStageComplete(true);return;}
    completeTick_+=dt;bool moved=false;
    while(completeTick_>=.04f){
        completeTick_-=.04f;
        for(size_t i=0;i<slots_.size();++i)if(slots_[i].joined&&slots_[i].lives>0){
            int* src=completeClear_[i]>0?&completeClear_[i]:(completeLife_[i]>0?&completeLife_[i]:nullptr);
            if(!src)continue;int amount=std::min(100,*src);*src-=amount;registerScore((int)i,amount);moved=true;
        }
        if(moved&&(++completeBeepCounter_%4)==0)audio_.playSfx("sounds/beep.wav",.28f);
    }
    bool done=true;for(size_t i=0;i<slots_.size();++i)if(completeClear_[i]>0||completeLife_[i]>0){done=false;break;}
    if(done){completeIdle_+=dt;if(completeIdle_>1.5f)finishStageComplete(false);}else completeIdle_=0;
}'''
if old_scene not in s:
    raise SystemExit("scene runtime block missing")
s=s.replace(old_scene,new_scene,1)

s=replace_once(
    s,
    'if(mode_==Mode::Scene){updateScene(dt);return;}',
    'if(mode_==Mode::Scene){updateScene(dt);return;}\n    if(mode_==Mode::StageComplete){updateStageComplete(dt);return;}',
    "stage complete update dispatch"
)

s=replace_once(
    s,
    'if((bossClearTriggered_||stageSequenceComplete())&&!enemiesAlive()){clearTimer_+=dt;if(clearTimer_>1.2f)loadStage(stageIndex_+1,true);}else clearTimer_=0;',
    '''if((bossClearTriggered_||stageSequenceComplete())&&!enemiesAlive()){
        clearTimer_+=dt;
        if(clearTimer_>1.2f){
            const auto& st=db_.campaign().stages[(size_t)stageIndex_];
            if(st.showCompleteAfter&&!db_.campaign().noShowComplete)startStageComplete();else advanceAfterStage();
        }
    }else clearTimer_=0;''',
    "stage completion transition"
)

insert_before='''void Game::drawLoading(){'''
stage_draw='''void Game::drawStageComplete(){
    renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(D2D1::ColorF::Black));
    renderer_.text(L"STAGE "+std::to_wstring(completeStageNumber_)+L" COMPLETE!",0,31,18,D2D1::ColorF(1,.86f,.18f),true);
    int active=0;for(const auto& sl:slots_)if(sl.joined&&sl.lives>0)++active;active=std::max(1,active);
    int col=0;float colW=renderer_.logicalWidth()/(float)active;
    for(size_t i=0;i<slots_.size();++i){
        const auto& sl=slots_[i];if(!sl.joined||sl.lives<=0)continue;
        float x=colW*(float)col+9.f;++col;
        renderer_.text(L"P"+std::to_wstring(i+1),x,70,8,D2D1::ColorF(.95f,.95f,.95f));
        renderer_.text(L"CLEAR BONUS",x,91,5.5f,D2D1::ColorF(.78f,.78f,.78f));
        renderer_.text(std::to_wstring(completeClear_[i]),x,102,7,D2D1::ColorF(1,1,1));
        renderer_.text(L"LIFE BONUS",x,124,5.5f,D2D1::ColorF(.78f,.78f,.78f));
        renderer_.text(std::to_wstring(completeLife_[i]),x,135,7,D2D1::ColorF(1,1,1));
        renderer_.text(L"TOTAL SCORE",x,159,5.5f,D2D1::ColorF(.78f,.78f,.78f));
        renderer_.text(std::to_wstring(sl.score),x,170,7,D2D1::ColorF(1,.84f,.20f));
    }
    renderer_.text(L"APERTE UM BOTAO PARA CONTINUAR",0,215,6,D2D1::ColorF(.72f,.72f,.72f),true);
}

'''
if insert_before not in s:
    raise SystemExit("drawLoading marker missing")
s=s.replace(insert_before,stage_draw+insert_before,1)

s=replace_once(
    s,
    'bool gpuFrame=(mode_==Mode::Playing||mode_==Mode::Pause||mode_==Mode::Loading||mode_==Mode::Continue||mode_==Mode::GameOver||mode_==Mode::Finished||mode_==Mode::Scene);',
    'bool gpuFrame=(mode_==Mode::Playing||mode_==Mode::Pause||mode_==Mode::Loading||mode_==Mode::Continue||mode_==Mode::GameOver||mode_==Mode::Finished||mode_==Mode::Scene||mode_==Mode::StageComplete);',
    "gpu frame mode"
)
s=replace_once(
    s,
    '''if(mode_==Mode::Scene){if(!sceneVisual_.empty())renderer_.drawAnimatedImage(sceneVisual_,sceneStepTime_,(float)sceneX_,(float)sceneY_,false);renderer_.end();return;}''',
    '''if(mode_==Mode::Scene){if(!sceneVisual_.empty())renderer_.drawAnimatedImage(sceneVisual_,sceneStepTime_,(float)sceneX_,(float)sceneY_,false);renderer_.end();return;}
    if(mode_==Mode::StageComplete){drawStageComplete();renderer_.end();return;}''',
    "stage complete render dispatch"
)

s=s.replace("v0.3.23","v0.3.24")
p.write_text(s)

p=Path("src/main_win.cpp"); s=p.read_text().replace("v0.3.23","v0.3.24"); p.write_text(s)
print("apply_v0324_flow=OK")
