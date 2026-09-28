from pathlib import Path
import os
p=Path('src/Game.h'); s=p.read_text()
s=s.replace('int actorId=0,selected=0,paletteMap=0,lives=0,score=0,nextLifeScore=30000,nextCreditScore=999999;','int actorId=0,selected=0,paletteMap=0,lives=0,score=0,nextLifeScore=30000,nextCreditScore=999999,hudTargetId=0;',1)
s=s.replace('std::string sceneVisual_;\\n    float sceneStepTime_=0;','std::string sceneVisual_;\\n    float sceneStepTime_=0;\\n    int sceneX_=0,sceneY_=0;\\n    bool sceneSkip_=false,sceneNoSkip_=false;',1)
p.write_text(s)

p=Path('src/Game.cpp'); s=p.read_text()
s=s.replace('''    for(auto&a:actors_)if(!a.dead&&!a.player&&!a.projectile&&!a.effect&&a.team==Team::Enemy){
        breakGrabLinks(a);a.hp=0;a.dead=true;
        auto death=reactionAnimation(a,"attack",ReactionKind::Death);''','''    for(auto&a:actors_)if(!a.dead&&!a.player&&!a.projectile&&!a.effect&&a.team==Team::Enemy){
        breakGrabLinks(a);a.hp=0;a.dead=true;
        if(a.def&&!a.def->deathSound.empty())audio_.playSfx(a.def->deathSound,.75f);
        auto death=reactionAnimation(a,"attack",ReactionKind::Death);''',1)
s=s.replace('''void Game::damage(Actor&t,Actor&att,int amount,int knockdown,const std::string&hitfx,const std::string&hitflash,const std::string&kind,const AttackBox* attack){
    if(t.dead||amount<=0||t.invincibleTime>0)return;''','''void Game::damage(Actor&t,Actor&att,int amount,int knockdown,const std::string&hitfx,const std::string&hitflash,const std::string&kind,const AttackBox* attack){
    if(t.dead||amount<=0||t.invincibleTime>0)return;
    if(att.player&&att.playerIndex>=0&&att.playerIndex<(int)slots_.size()&&!t.player)slots_[(size_t)att.playerIndex].hudTargetId=t.id;
    if(t.player&&t.playerIndex>=0&&t.playerIndex<(int)slots_.size()&&!att.player)slots_[(size_t)t.playerIndex].hudTargetId=att.id;''',1)
old='''void Game::startSceneQueue(const std::vector<std::string>&files,Mode after){sceneFiles_=files;sceneFileIndex_=sceneStep_=0;sceneStepTime_=0;sceneVisual_.clear();sceneAfterMode_=after;mode_=Mode::Scene;loadNextSceneFile();}
void Game::loadNextSceneFile(){if(sceneFileIndex_>=sceneFiles_.size()){sceneVisual_.clear();if(sceneAfterMode_==Mode::Title){enterTitle();return;}mode_=sceneAfterMode_;return;}scene_=db_.loadScene(sceneFiles_[sceneFileIndex_++]);sceneStep_=0;advanceSceneStep();}
void Game::advanceSceneStep(){sceneVisual_.clear();sceneStepTime_=0;while(true){if(sceneStep_>=scene_.steps.size()){loadNextSceneFile();return;}auto st=scene_.steps[sceneStep_++];if(st.kind==SceneStep::Kind::Music){audio_.playMusic(st.asset,st.loop);continue;}if(st.kind==SceneStep::Kind::Silence){audio_.stopMusic();continue;}sceneVisual_=st.asset;return;}}
void Game::updateScene(float dt){auto in=input_.state(0);if(in.backPressed){sceneVisual_.clear();if(sceneAfterMode_==Mode::Title){enterTitle();return;}mode_=sceneAfterMode_;return;}if(sceneVisual_.empty()){advanceSceneStep();return;}sceneStepTime_+=dt;float d=renderer_.animationDuration(sceneVisual_);if(d<=.11f)d=1.5f;if(in.startPressed||sceneStepTime_>=d)advanceSceneStep();}'''
new='''void Game::startSceneQueue(const std::vector<std::string>&files,Mode after){sceneFiles_=files;sceneFileIndex_=sceneStep_=0;sceneStepTime_=0;sceneX_=sceneY_=0;sceneSkip_=sceneNoSkip_=false;sceneVisual_.clear();sceneAfterMode_=after;mode_=Mode::Scene;loadNextSceneFile();}
void Game::loadNextSceneFile(){if(sceneFileIndex_>=sceneFiles_.size()){sceneVisual_.clear();if(sceneAfterMode_==Mode::Title){enterTitle();return;}mode_=sceneAfterMode_;return;}scene_=db_.loadScene(sceneFiles_[sceneFileIndex_++]);sceneStep_=0;advanceSceneStep();}
void Game::advanceSceneStep(){sceneVisual_.clear();sceneStepTime_=0;sceneX_=sceneY_=0;sceneSkip_=sceneNoSkip_=false;while(true){if(sceneStep_>=scene_.steps.size()){loadNextSceneFile();return;}auto st=scene_.steps[sceneStep_++];if(st.kind==SceneStep::Kind::Music){audio_.playMusic(st.asset,st.loop);continue;}if(st.kind==SceneStep::Kind::Silence){audio_.stopMusic();continue;}sceneVisual_=st.asset;sceneX_=st.x;sceneY_=st.y;sceneSkip_=st.skip;sceneNoSkip_=st.noskip;return;}}
void Game::updateScene(float dt){auto in=input_.state(0);if(in.backPressed){sceneVisual_.clear();if(sceneAfterMode_==Mode::Title){enterTitle();return;}mode_=sceneAfterMode_;return;}if(sceneVisual_.empty()){advanceSceneStep();return;}sceneStepTime_+=dt;float d=renderer_.animationDuration(sceneVisual_);if(d<=.11f)d=1.5f;const bool userSkip=in.startPressed&&sceneSkip_&&!sceneNoSkip_;if(userSkip||sceneStepTime_>=d)advanceSceneStep();}'''
if old not in s: raise SystemExit('scene runtime block missing')
s=s.replace(old,new,1)
start=s.index('void Game::drawHud(){'); end=s.index('void Game::drawGameplay()',start)
newhud=(Path(os.environ['GITHUB_WORKSPACE'])/'ci'/'v0323'/'hud_function.txt').read_text()
s=s[:start]+newhud+s[end:]
s=s.replace('renderer_.drawAnimatedImage(sceneVisual_,sceneStepTime_,0,-2,false)','renderer_.drawAnimatedImage(sceneVisual_,sceneStepTime_,(float)sceneX_,(float)sceneY_,false)',1)
p.write_text(s)
print('apply_v0323_game=OK')
