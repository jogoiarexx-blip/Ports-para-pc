#ifdef _WIN32
#include "Game.h"
#include "OpenBorSemantics.h"
#include "TextUtil.h"
#include "SpriteMath.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <tuple>

namespace ffx {
static std::wstring w(const std::string&s){return std::wstring(s.begin(),s.end());}
static float clampf(float v,float a,float b){return std::max(a,std::min(b,v));}
static bool starts(const std::string&s,const std::string&p){return s.rfind(p,0)==0;}
static bool containsType(const std::vector<std::string>&v,const std::string&t){return std::find(v.begin(),v.end(),lower(t))!=v.end();}
static std::string quotedKey(const std::string&s){auto a=s.find('"');if(a==std::string::npos)return {};auto b=s.find('"',a+1);return b==std::string::npos?std::string{}:lower(s.substr(a+1,b-a-1));}
static bool isOffensiveAnimation(const Animation& an){
    for(const auto&f:an.frames)if(f.attack.rect.valid&&f.attack.damage>0)return true;
    for(const auto&e:an.events)if(e.kind=="throw"||e.kind=="toss"||e.kind=="shoot")return true;
    return false;
}
static std::string fallFromAttackKind(const std::string& kind){
    if(starts(kind,"attack")&&kind.size()>6){auto s=kind.substr(6);if(!s.empty())return "fall"+s;}
    return "fall";
}
static bool attackWouldBeBlocked(const Actor& target,const Actor& attacker,int rawDamage,const AttackBox* attack){
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

bool Game::init(HWND hwnd,const std::filesystem::path& exeDir){
    hwnd_=hwnd;exeDir_=exeDir;dataRoot_=exeDir_/"assets"/"data";saveFile_=exeDir_/"save"/"ffx_save.ini";settingsFile_=exeDir_/"save"/"settings.ini";actors_.reserve(512);pendingSpawns_.reserve(64);
    std::string err;if(!db_.load(dataRoot_,&err))return false;if(!renderer_.init(hwnd,dataRoot_))return false;if(!audio_.init(dataRoot_))return false;
    loadSettings();applyAudioSettings();applyGraphicsSettings();bool wantFullscreen=fullscreen_;fullscreen_=false;if(wantFullscreen)setFullscreen(true);else applyWindowScale();
    credits_=db_.campaign().credits;resetSelection();
    if(std::filesystem::exists(dataRoot_/"scenes"/"logo.txt"))startSceneQueue({"scenes/logo.txt"},Mode::Title);else enterTitle();return true;
}
void Game::shutdown(){if(mode_==Mode::Playing||mode_==Mode::Pause)saveProgress();saveSettings();audio_.shutdown();renderer_.shutdown();}
void Game::resize(UINT wi,UINT he){renderer_.resize(wi,he);}
Actor* Game::player(){return player(0);} const Actor* Game::player()const{return player(0);}
Actor* Game::player(size_t i){if(i>=slots_.size()||!slots_[i].actorId)return nullptr;return actorById(slots_[i].actorId);} const Actor* Game::player(size_t i)const{if(i>=slots_.size()||!slots_[i].actorId)return nullptr;return actorById(slots_[i].actorId);}
Actor* Game::actorById(int id){if(!id)return nullptr;for(auto&a:actors_)if(a.id==id)return &a;return nullptr;} const Actor* Game::actorById(int id)const{if(!id)return nullptr;for(auto&a:actors_)if(a.id==id)return &a;return nullptr;}
int Game::activePlayerCount()const{int n=0;for(const auto&s:slots_)if(s.joined)++n;return n;}
bool Game::anyPlayerAlive()const{for(size_t i=0;i<slots_.size();++i){auto p=player(i);if(slots_[i].joined&&p&&!p->dead)return true;}return false;}
bool Game::allPlayersOut()const{bool any=false;for(const auto&s:slots_)if(s.joined){any=true;if(s.lives>0||s.actorId)return false;}return any;}
Actor* Game::leadPlayer(){Actor*b=nullptr;bool rev=level_.direction=="left"||level_.direction=="leftright";for(size_t i=0;i<slots_.size();++i){auto p=player(i);if(p&&!p->dead&&(!b||(rev?p->x<b->x:p->x>b->x)))b=p;}return b;} const Actor* Game::leadPlayer()const{const Actor*b=nullptr;bool rev=level_.direction=="left"||level_.direction=="leftright";for(size_t i=0;i<slots_.size();++i){auto p=player(i);if(p&&!p->dead&&(!b||(rev?p->x<b->x:p->x>b->x)))b=p;}return b;}

void Game::resetSelection(){for(auto&s:slots_)s={};slots_[0].joined=true;slots_[0].selected=0;for(auto&s:slots_){s.lives=db_.campaign().lives;s.nextLifeScore=std::max(1,db_.rules().lifeScore);s.nextCreditScore=std::max(1,db_.rules().creditScore);}}
void Game::loadSettings(){
    musicVolume_=75;sfxVolume_=85;windowScale_=3;fullscreen_=false;highScore_=0;graphicsPreset_=1;upscaleMode_=2;filterMode_=0;filterStrength_=55;integerScale_=true;widescreen_=false;vsync_=true;input_.resetDefaults();
    std::ifstream in(settingsFile_);std::string line;
    while(std::getline(in,line)){
        auto pos=line.find('=');if(pos==std::string::npos)continue;
        auto k=lower(trim(line.substr(0,pos))),v=trim(line.substr(pos+1));
        if(k=="music_volume")musicVolume_=std::clamp(toInt(v,75),0,100);
        else if(k=="sfx_volume")sfxVolume_=std::clamp(toInt(v,85),0,100);
        else if(k=="fullscreen")fullscreen_=toInt(v,0)!=0;
        else if(k=="window_scale")windowScale_=std::clamp(toInt(v,3),2,4);
        else if(k=="high_score")highScore_=std::max(0,toInt(v,0));
        else if(k=="graphics_preset")graphicsPreset_=std::clamp(toInt(v,1),0,4);
        else if(k=="upscale_mode")upscaleMode_=std::clamp(toInt(v,2),0,2);
        else if(k=="filter_mode")filterMode_=std::clamp(toInt(v,0),0,3);
        else if(k=="filter_strength")filterStrength_=std::clamp(toInt(v,55),0,100);
        else if(k=="integer_scale")integerScale_=toInt(v,1)!=0;
        else if(k=="widescreen")widescreen_=toInt(v,0)!=0;
        else if(k=="vsync")vsync_=toInt(v,1)!=0;
        else{
            for(size_t p=0;p<Input::MaxPlayers;++p){
                std::string prefix="p"+std::to_string(p+1)+"_";
                if(k==prefix+"keyboard_enabled"){input_.setKeyboardEnabled(p,toInt(v,1)!=0);continue;}
                if(k==prefix+"gamepad_index"){input_.setGamepadIndex(p,toInt(v,(int)p));continue;}
                for(size_t a=0;a<Input::ActionCount;++a){auto action=(InputAction)a;auto n=Input::actionSettingName(action);if(k==prefix+"key_"+n)input_.setKeyboardBinding(p,action,toInt(v,input_.bindings(p).keyboard[a]));else if(k==prefix+"pad_"+n)input_.setGamepadBinding(p,action,toInt(v,input_.bindings(p).gamepad[a]));}
            }
        }
    }
}
void Game::saveSettings()const{
    std::error_code ec;std::filesystem::create_directories(settingsFile_.parent_path(),ec);
    std::ofstream o(settingsFile_,std::ios::trunc);if(!o)return;
    o<<"version=6\n"<<"music_volume="<<musicVolume_<<"\n"<<"sfx_volume="<<sfxVolume_<<"\n"<<"fullscreen="<<(fullscreen_?1:0)<<"\n"<<"window_scale="<<windowScale_<<"\n"<<"high_score="<<highScore_<<"\n"<<"graphics_preset="<<graphicsPreset_<<"\n"<<"upscale_mode="<<upscaleMode_<<"\n"<<"filter_mode="<<filterMode_<<"\n"<<"filter_strength="<<filterStrength_<<"\n"<<"integer_scale="<<(integerScale_?1:0)<<"\n"<<"widescreen="<<(widescreen_?1:0)<<"\n"<<"vsync="<<(vsync_?1:0)<<"\n";
    for(size_t p=0;p<Input::MaxPlayers;++p){const auto& b=input_.bindings(p);std::string prefix="p"+std::to_string(p+1)+"_";o<<prefix<<"keyboard_enabled="<<(b.keyboardEnabled?1:0)<<"\n"<<prefix<<"gamepad_index="<<b.gamepadIndex<<"\n";for(size_t a=0;a<Input::ActionCount;++a){auto action=(InputAction)a;auto n=Input::actionSettingName(action);o<<prefix<<"key_"<<n<<"="<<b.keyboard[a]<<"\n"<<prefix<<"pad_"<<n<<"="<<b.gamepad[a]<<"\n";}}
}
void Game::applyAudioSettings(){audio_.setMusicVolume(musicVolume_/100.f);audio_.setSfxVolume(sfxVolume_/100.f);}
void Game::applyGraphicsSettings(){renderer_.setPostSettings(upscaleMode_,filterMode_,filterStrength_,integerScale_,widescreen_,vsync_);}
void Game::applyGraphicsPreset(int preset){graphicsPreset_=std::clamp(preset,0,3);if(graphicsPreset_==0){upscaleMode_=0;filterMode_=0;filterStrength_=0;integerScale_=true;}else if(graphicsPreset_==1){upscaleMode_=2;filterMode_=0;filterStrength_=45;integerScale_=true;}else if(graphicsPreset_==2){upscaleMode_=0;filterMode_=2;filterStrength_=60;integerScale_=true;}else{upscaleMode_=1;filterMode_=0;filterStrength_=0;integerScale_=false;}applyGraphicsSettings();saveSettings();}
void Game::applyWindowScale(){
    if(!hwnd_||fullscreen_)return;windowScale_=std::clamp(windowScale_,2,4);
    DWORD style=(DWORD)GetWindowLongPtr(hwnd_,GWL_STYLE);int logicalW=widescreen_?426:320;RECT r{0,0,logicalW*windowScale_,240*windowScale_};UINT dpi=GetDpiForWindow(hwnd_);AdjustWindowRectExForDpi(&r,style,FALSE,0,dpi);
    int ww=r.right-r.left,wh=r.bottom-r.top;MONITORINFO mi{sizeof(mi)};GetMonitorInfo(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&mi);
    int x=mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-ww)/2,y=mi.rcWork.top+(mi.rcWork.bottom-mi.rcWork.top-wh)/2;
    SetWindowPos(hwnd_,nullptr,x,y,ww,wh,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED|SWP_SHOWWINDOW);
    windowedPlacement_.length=sizeof(WINDOWPLACEMENT);GetWindowPlacement(hwnd_,&windowedPlacement_);windowedStyle_=GetWindowLongPtr(hwnd_,GWL_STYLE);haveWindowedPlacement_=true;
}
void Game::setFullscreen(bool enabled){
    if(!hwnd_)return;if(enabled==fullscreen_&&(!enabled||haveWindowedPlacement_))return;
    if(enabled){windowedStyle_=GetWindowLongPtr(hwnd_,GWL_STYLE);windowedPlacement_.length=sizeof(WINDOWPLACEMENT);GetWindowPlacement(hwnd_,&windowedPlacement_);haveWindowedPlacement_=true;MONITORINFO mi{sizeof(mi)};if(GetMonitorInfo(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&mi)){LONG_PTR style=(windowedStyle_&~WS_OVERLAPPEDWINDOW)|WS_POPUP|WS_VISIBLE;SetWindowLongPtr(hwnd_,GWL_STYLE,style);SetWindowPos(hwnd_,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_NOOWNERZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED|SWP_SHOWWINDOW);}fullscreen_=true;}
    else{fullscreen_=false;if(haveWindowedPlacement_){SetWindowLongPtr(hwnd_,GWL_STYLE,windowedStyle_);SetWindowPlacement(hwnd_,&windowedPlacement_);SetWindowPos(hwnd_,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_FRAMECHANGED|SWP_SHOWWINDOW);}applyWindowScale();}
    RECT rc{};if(GetClientRect(hwnd_,&rc))renderer_.resize((UINT)std::max(1L,rc.right-rc.left),(UINT)std::max(1L,rc.bottom-rc.top));
}
void Game::toggleFullscreen(){setFullscreen(!fullscreen_);saveSettings();beginUiTransition();}
void Game::beginUiTransition(){uiFade_=0.f;}
void Game::enterTitle(){saveSettings();mode_=Mode::Title;menuIndex_=0;beginUiTransition();audio_.pauseMusic(false);audio_.playMusic("music/menu.bor",true);}
void Game::updateTitle(const InputState& in){
    constexpr int count=7;
    if(in.upPressed){menuIndex_=(menuIndex_+count-1)%count;audio_.playSfx("sounds/beep.wav",.6f);}
    if(in.downPressed){menuIndex_=(menuIndex_+1)%count;audio_.playSfx("sounds/beep.wav",.6f);}
    bool accept=in.startPressed||in.attackPressed;if(!accept)return;
    audio_.playSfx("sounds/beep.wav",.8f);
    switch(menuIndex_){
        case 0:resetSelection();mode_=Mode::Select;beginUiTransition();break;
        case 1:if(hasSave()){if(loadSave())return;}else audio_.playSfx("sounds/beep.wav",.35f);break;
        case 2:if(std::filesystem::exists(dataRoot_/"scenes"/"howto.txt"))startSceneQueue({"scenes/howto.txt"},Mode::Title);break;
        case 3:optionsReturnMode_=Mode::Title;optionsIndex_=0;mode_=Mode::Options;beginUiTransition();break;
        case 4:optionsReturnMode_=Mode::Title;controlsReturnMode_=Mode::Title;controlsPage_=ControlsPage::Root;controlsIndex_=0;controlsActionIndex_=0;bindingCapture_=false;mode_=Mode::Controls;beginUiTransition();break;
        case 5:if(std::filesystem::exists(dataRoot_/"scenes"/"creditos.txt"))startSceneQueue({"scenes/creditos.txt"},Mode::Title);break;
        case 6:PostMessage(hwnd_,WM_CLOSE,0,0);break;
    }
}
void Game::updateOptions(const InputState& in){
    constexpr int count=7;if(in.upPressed){optionsIndex_=(optionsIndex_+count-1)%count;audio_.playSfx("sounds/beep.wav",.55f);}if(in.downPressed){optionsIndex_=(optionsIndex_+1)%count;audio_.playSfx("sounds/beep.wav",.55f);}
    int dir=(in.rightPressed?1:0)-(in.leftPressed?1:0);bool accept=in.startPressed||in.attackPressed;
    if(optionsIndex_==0&&dir){musicVolume_=std::clamp(musicVolume_+dir*5,0,100);applyAudioSettings();saveSettings();audio_.playSfx("sounds/beep.wav",.45f);}
    else if(optionsIndex_==1&&dir){sfxVolume_=std::clamp(sfxVolume_+dir*5,0,100);applyAudioSettings();saveSettings();audio_.playSfx("sounds/beep.wav",.7f);}
    else if(optionsIndex_==2&&(dir||accept)){setFullscreen(!fullscreen_);saveSettings();audio_.playSfx("sounds/beep.wav",.7f);}
    else if(optionsIndex_==3&&dir){windowScale_+=dir;if(windowScale_>4)windowScale_=2;if(windowScale_<2)windowScale_=4;applyWindowScale();saveSettings();audio_.playSfx("sounds/beep.wav",.55f);}
    else if(optionsIndex_==4&&accept){graphicsIndex_=0;mode_=Mode::Graphics;beginUiTransition();audio_.playSfx("sounds/beep.wav",.7f);return;}
    else if(optionsIndex_==5&&accept){controlsReturnMode_=Mode::Options;controlsPage_=ControlsPage::Root;controlsIndex_=0;controlsActionIndex_=0;bindingCapture_=false;mode_=Mode::Controls;beginUiTransition();audio_.playSfx("sounds/beep.wav",.7f);return;}
    if(optionsIndex_==6&&accept){saveSettings();mode_=optionsReturnMode_;beginUiTransition();if(mode_==Mode::Title)audio_.playMusic("music/menu.bor",true);return;}
    if(in.backPressed){saveSettings();mode_=optionsReturnMode_;beginUiTransition();if(mode_==Mode::Title)audio_.playMusic("music/menu.bor",true);}
}
void Game::updateGraphics(const InputState& in){
    constexpr int count=8;if(in.upPressed){graphicsIndex_=(graphicsIndex_+count-1)%count;audio_.playSfx("sounds/beep.wav",.5f);}if(in.downPressed){graphicsIndex_=(graphicsIndex_+1)%count;audio_.playSfx("sounds/beep.wav",.5f);}
    int dir=(in.rightPressed?1:0)-(in.leftPressed?1:0);bool accept=in.startPressed||in.attackPressed;
    if(graphicsIndex_==0&&dir){int p=graphicsPreset_==4?1:graphicsPreset_+dir;if(p>3)p=0;if(p<0)p=3;applyGraphicsPreset(p);audio_.playSfx("sounds/beep.wav",.55f);}
    else if(graphicsIndex_==1&&dir){upscaleMode_=(upscaleMode_+dir+3)%3;graphicsPreset_=4;applyGraphicsSettings();saveSettings();audio_.playSfx("sounds/beep.wav",.55f);}
    else if(graphicsIndex_==2&&dir){filterMode_=(filterMode_+dir+4)%4;graphicsPreset_=4;applyGraphicsSettings();saveSettings();audio_.playSfx("sounds/beep.wav",.55f);}
    else if(graphicsIndex_==3&&dir){filterStrength_=std::clamp(filterStrength_+dir*10,0,100);graphicsPreset_=4;applyGraphicsSettings();saveSettings();audio_.playSfx("sounds/beep.wav",.55f);}
    else if(graphicsIndex_==4&&(dir||accept)){integerScale_=!integerScale_;graphicsPreset_=4;applyGraphicsSettings();saveSettings();audio_.playSfx("sounds/beep.wav",.55f);}
    else if(graphicsIndex_==5&&(dir||accept)){widescreen_=!widescreen_;graphicsPreset_=4;applyGraphicsSettings();applyWindowScale();saveSettings();audio_.playSfx("sounds/beep.wav",.55f);}
    else if(graphicsIndex_==6&&(dir||accept)){vsync_=!vsync_;graphicsPreset_=4;applyGraphicsSettings();saveSettings();audio_.playSfx("sounds/beep.wav",.55f);}
    else if(graphicsIndex_==7&&accept){mode_=Mode::Options;optionsIndex_=4;beginUiTransition();return;}
    if(in.backPressed){mode_=Mode::Options;optionsIndex_=4;beginUiTransition();}
}

void Game::updateControls(const InputState& in){
    auto player=(size_t)controlsPlayer_;
    if(controlsPage_==ControlsPage::Root){
        constexpr int count=7;
        if(in.upPressed){controlsIndex_=(controlsIndex_+count-1)%count;audio_.playSfx("sounds/beep.wav",.5f);}
        if(in.downPressed){controlsIndex_=(controlsIndex_+1)%count;audio_.playSfx("sounds/beep.wav",.5f);}
        int dir=(in.rightPressed?1:0)-(in.leftPressed?1:0);bool accept=in.startPressed||in.attackPressed;
        if(controlsIndex_==0&&dir){controlsPlayer_=(controlsPlayer_+dir+(int)Input::MaxPlayers)%(int)Input::MaxPlayers;audio_.playSfx("sounds/beep.wav",.55f);}
        else if(controlsIndex_==1&&(dir||accept)){auto b=input_.bindings(player).keyboardEnabled;input_.setKeyboardEnabled(player,!b);saveSettings();audio_.playSfx("sounds/beep.wav",.55f);}
        else if(controlsIndex_==2&&dir){int gi=input_.bindings(player).gamepadIndex;gi+=dir;if(gi>3)gi=-1;if(gi<-1)gi=3;input_.setGamepadIndex(player,gi);saveSettings();audio_.playSfx("sounds/beep.wav",.55f);}
        else if(controlsIndex_==3&&accept){controlsPage_=ControlsPage::Keyboard;controlsActionIndex_=0;bindingCapture_=false;audio_.playSfx("sounds/beep.wav",.6f);return;}
        else if(controlsIndex_==4&&accept){if(input_.bindings(player).gamepadIndex<0){input_.setGamepadIndex(player,0);saveSettings();}controlsPage_=ControlsPage::Gamepad;controlsActionIndex_=0;bindingCapture_=false;audio_.playSfx("sounds/beep.wav",.6f);return;}
        else if(controlsIndex_==5&&accept){input_.resetPlayerDefaults(player);saveSettings();audio_.playSfx("sounds/beep.wav",.75f);}
        else if(controlsIndex_==6&&accept){saveSettings();if(controlsReturnMode_==Mode::Title){enterTitle();}else{mode_=controlsReturnMode_;if(mode_==Mode::Options)optionsIndex_=5;}return;}
        if(in.backPressed){saveSettings();if(controlsReturnMode_==Mode::Title){enterTitle();}else{mode_=controlsReturnMode_;if(mode_==Mode::Options)optionsIndex_=5;}return;}
        return;
    }

    if(bindingCapture_){
        if(controlsPage_==ControlsPage::Keyboard){
            int vk=input_.takeLastKeyboardPress();
            if(vk==VK_ESCAPE&&controlsActionIndex_!=(int)InputAction::Back){bindingCapture_=false;input_.clearCaptureEvents();return;}
            if(vk>0){input_.setKeyboardBinding(player,(InputAction)controlsActionIndex_,vk);bindingCapture_=false;saveSettings();audio_.playSfx("sounds/beep.wav",.8f);}
        }else{
            if(GetAsyncKeyState(VK_ESCAPE)&0x8000){bindingCapture_=false;input_.clearCaptureEvents();return;}
            int pad=input_.bindings(player).gamepadIndex;if(pad<0)pad=0;int code=input_.takeLastGamepadPress(pad);if(code>0){input_.setGamepadBinding(player,(InputAction)controlsActionIndex_,code);bindingCapture_=false;saveSettings();audio_.playSfx("sounds/beep.wav",.8f);}
        }
        return;
    }

    constexpr int actions=(int)Input::ActionCount;
    if(in.upPressed){controlsActionIndex_=(controlsActionIndex_+actions-1)%actions;audio_.playSfx("sounds/beep.wav",.45f);}
    if(in.downPressed){controlsActionIndex_=(controlsActionIndex_+1)%actions;audio_.playSfx("sounds/beep.wav",.45f);}
    if(in.startPressed||in.attackPressed){bindingCapture_=true;input_.clearCaptureEvents();audio_.playSfx("sounds/beep.wav",.6f);return;}
    if(in.backPressed){auto page=controlsPage_;controlsPage_=ControlsPage::Root;controlsIndex_=page==ControlsPage::Keyboard?3:4;bindingCapture_=false;return;}
}
void Game::updatePause(const InputState& in){
    constexpr int count=4;
    if(in.upPressed){pauseIndex_=(pauseIndex_+count-1)%count;audio_.playSfx("sounds/beep.wav",.55f);}
    if(in.downPressed){pauseIndex_=(pauseIndex_+1)%count;audio_.playSfx("sounds/beep.wav",.55f);}
    bool accept=in.startPressed||in.attackPressed;
    if(in.backPressed){audio_.pauseMusic(false);mode_=Mode::Playing;return;}
    if(!accept)return;audio_.playSfx("sounds/beep.wav",.7f);
    if(pauseIndex_==0){audio_.pauseMusic(false);mode_=Mode::Playing;}
    else if(pauseIndex_==1){optionsReturnMode_=Mode::Pause;optionsIndex_=0;mode_=Mode::Options;beginUiTransition();}
    else if(pauseIndex_==2){saveProgress();}
    else {saveProgress();enterTitle();}
}
void Game::drawUiFade(){if(uiFade_<.999f)renderer_.fillScreen(D2D1::ColorF(0,0,0),clampf(1.f-uiFade_,0.f,1.f));}
void Game::drawMainMenu(){
    float pulse=.5f+.5f*std::sin(totalTime_*4.2f),bob=std::sin(totalTime_*2.1f)*.7f;
    renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.18f);renderer_.drawImage("bgs/title.gif",0,-2,false,.26f);
    renderer_.fillRect(44,61,232,173,D2D1::ColorF(0,0,0),.83f);renderer_.fillRect(44,61,232,2,D2D1::ColorF(1,.58f,.06f),.72f+.20f*pulse);
    renderer_.text(L"FINAL FIGHT X",0,73+bob,15,D2D1::ColorF(0,0,0),true);renderer_.text(L"FINAL FIGHT X",0,72+bob,15,D2D1::ColorF(1,.82f+.08f*pulse,.18f),true);
    if(db_.campaign().hud.highScoreBackground){renderer_.text(L"HI "+std::to_wstring(highScore_),0,90,6.5f,D2D1::ColorF(.9f,.9f,.9f),true);}
    static const wchar_t* labels[]={L"NOVO JOGO",L"CONTINUAR",L"COMO JOGAR",L"OPCOES",L"CONTROLES",L"CREDITOS",L"SAIR"};
    for(int i=0;i<7;++i){bool disabled=(i==1&&!hasSave()),sel=i==menuIndex_;float y=100.f+i*17.f;if(sel){float grow=2.f*pulse;renderer_.fillRect(69-grow,y-1,182+grow*2,17,D2D1::ColorF(.31f,.045f,.018f),.94f);renderer_.fillRect(69-grow,y-1,3,17,D2D1::ColorF(1,.68f,.08f),1.f);renderer_.fillRect(248+grow,y-1,3,17,D2D1::ColorF(1,.68f,.08f),1.f);}auto c=disabled?D2D1::ColorF(.34f,.34f,.34f):(sel?D2D1::ColorF(1,.88f,.28f):D2D1::ColorF(.95f,.95f,.95f));std::wstring label=sel&&!disabled?std::wstring(L"> ")+labels[i]+L" <":labels[i];renderer_.text(label,0,y+2,8.5f,D2D1::ColorF(0,0,0),true);renderer_.text(label,0,y+1,8.5f,c,true);}
    renderer_.fillRect(44,220,232,1,D2D1::ColorF(.45f,.22f,.06f),.8f);renderer_.text(L"SETAS/WASD: MENU   ENTER/J: OK   F11: TELA CHEIA",0,224,5.4f,D2D1::ColorF(.78f,.78f,.78f),true);renderer_.text(L"NATIVE PORT v0.3.30",246,232,4.8f,D2D1::ColorF(.55f,.55f,.55f));drawUiFade();
}
void Game::drawOptions(){
    if(optionsReturnMode_==Mode::Pause){drawGameplay();renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(0,0,0),.72f);}else{renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.52f);renderer_.drawImage("bgs/title.gif",0,-2,false,.18f);}
    renderer_.fillRect(38,24,244,204,D2D1::ColorF(0,0,0),.90f);renderer_.fillRect(38,24,244,2,D2D1::ColorF(1,.62f,.08f),.9f);renderer_.text(L"OPCOES",0,32,14,D2D1::ColorF(1,.82f,.18f),true);
    int logicalW=widescreen_?426:320;std::wstring size=L"JANELA      < "+std::to_wstring(logicalW*windowScale_)+L"x"+std::to_wstring(240*windowScale_)+L" >";
    std::array<std::wstring,7> labels={L"MUSICA      < "+std::to_wstring(musicVolume_)+L"% >",L"EFEITOS     < "+std::to_wstring(sfxVolume_)+L"% >",std::wstring(L"TELA CHEIA  < ")+(fullscreen_?L"SIM":L"NAO")+L" >",size,L"GRAFICOS GPU",L"CONTROLES",L"VOLTAR"};
    float pulse=.5f+.5f*std::sin(totalTime_*5.f);for(int i=0;i<7;++i){bool sel=i==optionsIndex_;float y=58.f+i*22.f;if(sel){renderer_.fillRect(56,y-2,208,18,D2D1::ColorF(.35f,.08f,.04f),.88f);renderer_.fillRect(56,y-2,3,18,D2D1::ColorF(1,.70f,.10f),.75f+.25f*pulse);}renderer_.text(labels[(size_t)i],0,y+1,8.2f,sel?D2D1::ColorF(1,.85f,.2f):D2D1::ColorF(.94f,.94f,.94f),true);}
    renderer_.text(fullscreen_?L"GRAFICOS GPU FUNCIONAM TAMBEM EM TELA CHEIA":L"ESQ/DIR AJUSTA   ENTER OK   ESC VOLTA",0,216,5.4f,D2D1::ColorF(.72f,.72f,.72f),true);drawUiFade();
}
void Game::drawGraphics(){
    renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.66f);renderer_.fillRect(30,18,260,212,D2D1::ColorF(.01f,.01f,.015f),.94f);renderer_.fillRect(30,18,260,2,D2D1::ColorF(.2f,.78f,1.f),.95f);renderer_.text(L"GRAFICOS GPU",0,27,14,D2D1::ColorF(.35f,.86f,1.f),true);
    static const wchar_t* presets[]={L"ORIGINAL",L"PIXEL SHARP",L"ARCADE CRT",L"SUAVE",L"CUSTOM"};static const wchar_t* up[]={L"NEAREST",L"BILINEAR",L"SHARP MIX"};static const wchar_t* fx[]={L"DESLIGADO",L"SCANLINES",L"CRT",L"ARCADE"};
    std::array<std::wstring,8> labels={std::wstring(L"PRESET       < ")+presets[std::clamp(graphicsPreset_,0,4)]+L" >",std::wstring(L"UPSCALE      < ")+up[upscaleMode_]+L" >",std::wstring(L"FILTRO       < ")+fx[filterMode_]+L" >",L"INTENSIDADE  < "+std::to_wstring(filterStrength_)+L"% >",std::wstring(L"PIXEL INTEIRO< ")+(integerScale_?L"SIM":L"NAO")+L" >",std::wstring(L"WIDESCREEN   < ")+(widescreen_?L"SIM":L"NAO")+L" >",std::wstring(L"V-SYNC       < ")+(vsync_?L"SIM":L"NAO")+L" >",L"VOLTAR"};
    float pulse=.5f+.5f*std::sin(totalTime_*5.f);for(int i=0;i<8;++i){float y=49.f+i*20.f;bool sel=i==graphicsIndex_;if(sel){renderer_.fillRect(45,y-2,230,18,D2D1::ColorF(.02f,.18f,.25f),.9f);renderer_.fillRect(45,y-2,3,18,D2D1::ColorF(.25f,.85f,1.f),.7f+.3f*pulse);}renderer_.text(labels[(size_t)i],0,y+1,8.f,sel?D2D1::ColorF(.45f,.9f,1.f):D2D1::ColorF(.93f,.93f,.93f),true);}
    renderer_.text(widescreen_?L"MODO 16:9 HABILITADO | HUD/TEXTO COM PIXEL SNAP":L"RENDER INTERNO 4:3 | WIDESCREEN OPCIONAL 16:9",0,209,5.2f,D2D1::ColorF(.66f,.78f,.82f),true);renderer_.text(L"PIXEL SHARP E O PRESET RECOMENDADO",0,219,5.2f,D2D1::ColorF(.72f,.72f,.72f),true);drawUiFade();
}

void Game::drawControls(){
    if(optionsReturnMode_==Mode::Pause){drawGameplay();renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(0,0,0),.76f);}else{renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.60f);renderer_.drawImage("bgs/title.gif",0,-2,false,.14f);}
    renderer_.fillRect(26,18,268,210,D2D1::ColorF(0,0,0),.92f);
    renderer_.text(L"CONTROLES - P"+std::to_wstring(controlsPlayer_+1),0,25,13,D2D1::ColorF(1,.82f,.18f),true);
    const auto& b=input_.bindings((size_t)controlsPlayer_);
    if(controlsPage_==ControlsPage::Root){
        std::wstring gp=b.gamepadIndex<0?L"DESLIGADO":L"GAMEPAD "+std::to_wstring(b.gamepadIndex+1);
        std::array<std::wstring,7> labels={L"JOGADOR       < P"+std::to_wstring(controlsPlayer_+1)+L" >",std::wstring(L"TECLADO       < ")+(b.keyboardEnabled?L"ATIVO":L"DESLIGADO")+L" >",L"GAMEPAD       < "+gp+L" >",L"CONFIGURAR TECLADO",L"CONFIGURAR GAMEPAD",L"RESTAURAR PADRAO",L"VOLTAR"};
        for(int i=0;i<7;++i){bool sel=i==controlsIndex_;if(sel)renderer_.fillRect(45,56+i*22,230,17,D2D1::ColorF(.35f,.08f,.04f),.85f);renderer_.text(labels[(size_t)i],0,59+i*22,8,sel?D2D1::ColorF(1,.85f,.2f):D2D1::ColorF(D2D1::ColorF::White),true);}
        renderer_.text(L"P1-P3 | XINPUT 1-4 | CONFIGURACAO SALVA",0,214,5.7f,D2D1::ColorF(.68f,.68f,.68f),true);drawUiFade();return;
    }
    bool keyboard=controlsPage_==ControlsPage::Keyboard;
    renderer_.text(keyboard?L"MAPEAR TECLADO":L"MAPEAR GAMEPAD",0,43,8,D2D1::ColorF(.78f,.9f,1.f),true);
    for(int i=0;i<(int)Input::ActionCount;++i){bool sel=i==controlsActionIndex_;float y=57.f+i*14.5f;if(sel)renderer_.fillRect(39,y-1,242,13,D2D1::ColorF(.35f,.08f,.04f),.82f);std::wstring val=keyboard?Input::keyName(b.keyboard[(size_t)i]):Input::padName(b.gamepad[(size_t)i]);renderer_.text(Input::actionName((InputAction)i),48,y,6.5f,sel?D2D1::ColorF(1,.85f,.2f):D2D1::ColorF(D2D1::ColorF::White));renderer_.text(val,184,y,6.5f,sel?D2D1::ColorF(1,.85f,.2f):D2D1::ColorF(.82f,.82f,.82f));}
    if(bindingCapture_){renderer_.fillRect(47,188,226,26,D2D1::ColorF(.2f,.03f,.02f),.96f);renderer_.text(keyboard?L"PRESSIONE UMA TECLA...":L"PRESSIONE UM BOTAO/EIXO...",0,193,8,D2D1::ColorF(1,.85f,.2f),true);renderer_.text(L"ESC CANCELA",0,205,5.5f,D2D1::ColorF(.8f,.8f,.8f),true);}else renderer_.text(L"ENTER REMAPEIA   ESC VOLTA",0,211,6,D2D1::ColorF(.72f,.72f,.72f),true);drawUiFade();
}

void Game::drawPauseOverlay(){
    renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(0,0,0),.66f);renderer_.fillRect(67,52,186,137,D2D1::ColorF(0,0,0),.86f);renderer_.text(L"PAUSADO",0,60,16,D2D1::ColorF(1,.82f,.18f),true);
    static const wchar_t* labels[]={L"CONTINUAR",L"OPCOES",L"SALVAR",L"VOLTAR AO MENU"};
    for(int i=0;i<4;++i){bool sel=i==pauseIndex_;if(sel)renderer_.fillRect(82,96+i*22,156,17,D2D1::ColorF(.35f,.08f,.04f),.85f);renderer_.text(labels[i],0,99+i*22,9,sel?D2D1::ColorF(1,.85f,.2f):D2D1::ColorF(D2D1::ColorF::White),true);}
}
int Game::nextSelectableCharacter(size_t slot,int direction)const{int cur=slots_[slot].selected;for(int tries=0;tries<(int)players_.size();++tries){cur=(cur+direction+(int)players_.size())%(int)players_.size();if(!db_.campaign().noSame)return cur;bool used=false;for(size_t j=0;j<slots_.size();++j)if(j!=slot&&slots_[j].joined&&slots_[j].selected==cur)used=true;if(!used)return cur;}return slots_[slot].selected;}

const Animation* Game::animation(const Actor&a,const std::string&n)const{if(!a.def)return nullptr;auto it=a.def->animations.find(lower(n));if(it!=a.def->animations.end())return &it->second;return nullptr;}
const AnimFrame* Game::frame(const Actor&a)const{auto an=animation(a,a.anim);if(!an||an->frames.empty())return nullptr;return &an->frames[std::min(a.frame,an->frames.size()-1)];}
void Game::setAnim(Actor&a,const std::string&n,bool restart){
    auto an=animation(a,n);if(!an||an->frames.empty())return;
    auto nn=lower(n);if(!restart&&a.anim==nn)return;
    a.anim=nn;a.frame=0;a.frameTime=0;a.deathAnimationFinished=false;a.deathFinishedAt=-1.f;
    a.hitTargets.clear();a.attackWindowActive=false;a.attackOneConsumed=false;
    if(a.anim=="grab")a.grabReady=false;
    ++a.attackSerial;enterFrame(a);
}
std::string Game::commandAnimation(const Actor&a,const std::string&key,const std::string&fallback)const{if(a.def){auto it=a.def->commands.find(lower(key));if(it!=a.def->commands.end()&&animation(a,it->second))return it->second;}return animation(a,fallback)?fallback:std::string{};}
std::string Game::reactionAnimation(const Actor&a,const std::string&attackKind,ReactionKind reaction)const{
    for(const auto& n:reactionCandidates(attackKind,reaction))if(animation(a,n))return n;
    return {};
}

void Game::queueEffect(const std::string&model,float x,float z,float a,bool sourceFacingLeft){if(model.empty())return;auto d=db_.entity(model);if(!d)return;bool left=d->toFlip?sourceFacingLeft:false;pendingSpawns_.push_back({lower(model),"effect",x,z,a,0,left,true,Team::Neutral,0});}
void Game::queueProjectile(const Actor&owner,const std::string&model,const std::string&mode,float height,float power){if(model.empty()||!db_.entity(model))return;pendingSpawns_.push_back({lower(model),mode,owner.x,owner.z,owner.a+height,power,owner.facingLeft,false,owner.team,owner.id});}
void Game::processPendingSpawns(){auto q=std::move(pendingSpawns_);pendingSpawns_.clear();for(const auto&s:q){auto a=createActor(s.model,s.x,s.z,s.left,false,false,0,{},s.team);if(!a)continue;a->a=s.a;a->projectileOwner=s.owner;if(s.effect){a->effect=true;continue;}a->projectile=true;a->projectileLife=0;a->removeOnHit=a->def->remove!=0;float dir=s.left?-1.f:1.f;float sp=std::max(8.f,a->def->speed);if(s.mode=="toss"){a->projectileArc=true;a->vx=dir*sp*8.f;a->va=std::max(95.f,s.power*2.3f);}else{a->vx=dir*sp*16.f;}}}

void Game::executeFrameCommands(Actor&a,const AnimFrame&f){
    for(const auto& raw:f.commands){
        auto c=trim(raw),lc=lower(c);auto parts=splitWs(lc);if(parts.empty())continue;auto cmd=parts[0];auto par=cmd.find('(');if(par!=std::string::npos)cmd=cmd.substr(0,par);
        if(cmd=="setglobalvar"){
            auto key=quotedKey(c);if(key.empty())continue;
            if(lc.find("null()")!=std::string::npos){globalActorVars_.erase(key);globalIntVars_.erase(key);continue;}
            if(lc.find("getlocalvar")!=std::string::npos&&lc.find("self")!=std::string::npos){globalActorVars_[key]=a.id;globalIntVars_.erase(key);continue;}
            auto q2=c.find('"',c.find('"')+1);if(q2!=std::string::npos){auto tail=trim(c.substr(q2+1));try{size_t used=0;int v=std::stoi(tail,&used);if(used>0){globalIntVars_[key]=v;globalActorVars_.erase(key);}}catch(...){}}
            continue;
        }
        if(!a.def)continue;auto fnIt=a.def->animationFunctions.find(cmd);if(fnIt==a.def->animationFunctions.end())continue;const auto&fn=fnIt->second;
        bool playerMatch=fn.playerNameEquals.empty();
        if(!playerMatch)for(size_t i=0;i<slots_.size();++i){auto p=player(i);if(p&&lower(p->baseModel)==fn.playerNameEquals){playerMatch=true;break;}}
        if(playerMatch&&fn.killSelf){
            a.dead=true;a.deathTime=1.f;a.deathAnimationFinished=true;a.deathFinishedAt=a.deathTime;
            for(auto it=globalActorVars_.begin();it!=globalActorVars_.end();)it->second==a.id?it=globalActorVars_.erase(it):++it;
        }
        if(playerMatch&&!fn.clearGlobal.empty()){globalActorVars_.erase(fn.clearGlobal);globalIntVars_.erase(fn.clearGlobal);}
        if(fn.subjectToScreenOnGlobal){auto it=globalIntVars_.find(fn.subjectGlobal);if(it!=globalIntVars_.end()&&it->second==fn.subjectGlobalValue)a.subjectToScreen=fn.subjectToScreenValue!=0;}
    }
}

void Game::executeFrameEvents(Actor&a,const Animation&an){for(const auto&e:an.events){if(e.frame!=(int)a.frame)continue;if(e.kind=="flip")a.facingLeft=!a.facingLeft;else if(e.kind=="jump"){float h=e.args.empty()?3.f:toFloat(e.args[0],3.f),dx=e.args.size()>1?toFloat(e.args[1]):0.f,dz=e.args.size()>2?toFloat(e.args[2]):0.f,dir=a.facingLeft?-1.f:1.f;a.va=std::max(a.va,h*42.f);a.vx+=dx*42.f*dir;a.vz+=dz*24.f;}else if(e.kind=="quake"){float v=e.args.empty()?3.f:toFloat(e.args[0],3.f);auto cf=frame(a);shakeTime_=std::max(shakeTime_,openBorFrameSeconds(cf?cf->delay:10)*1.25f);shakeAmpX_=0.f;shakeAmpY_=std::abs(v);}else if(e.kind=="throw"){float h=e.args.empty()?70.f:toFloat(e.args[0],70.f);if(std::abs(h)<.001f)h=70.f;queueProjectile(a,an.projectileModel,"throw",h);if(a.player&&a.weaponNumber==3)a.unequipAfterAnim=true;}else if(e.kind=="toss"){float h=e.args.empty()?70.f:toFloat(e.args[0],70.f);if(std::abs(h)<.001f)h=70.f;std::string m=!a.def->bombModel.empty()?a.def->bombModel:an.projectileModel;queueProjectile(a,m,"toss",h,h);}else if(e.kind=="shoot"){float h=e.args.empty()?70.f:toFloat(e.args[0],70.f);if(std::abs(h)<.001f)h=70.f;std::string m=an.projectileModel;if(m.empty()&&a.def&&db_.entity(a.def->shotModel))m=a.def->shotModel;queueProjectile(a,m,"shoot",h);}}}
void Game::enterFrame(Actor&a){
    auto an=animation(a,a.anim);auto f=frame(a);if(!an||!f)return;
    const bool attackActive=f->attack.rect.valid;
    if(attackActive&&!a.attackWindowActive){
        if(!an->attackOne)a.hitTargets.clear();
        a.attackWindowActive=true;++a.attackSerial;
    }else if(!attackActive&&a.attackWindowActive){
        a.attackWindowActive=false;
        if(!an->attackOne)a.hitTargets.clear();
    }
    if(!f->sound.empty())audio_.playSfx(f->sound,.75f);
    float dir=a.facingLeft?-1.f:1.f;a.x+=f->moveX*dir;a.z+=f->moveZ;
    if(f->setA>=0)a.a=f->setA;else a.a+=f->moveA;
    if(a.anim=="throw"&&a.grabTarget&&a.def&&a.def->throwFrameWait>=0&&(int)a.frame==a.def->throwFrameWait){
        auto victim=actorById(a.grabTarget);
        if(victim&&victim->def){
            const float vx=(a.facingLeft?-1.f:1.f)*std::max(18.f,std::abs(victim->def->throwDist)*42.f);
            const float va=std::max(45.f,std::abs(victim->def->throwHeight)*42.f);
            const int dmg=std::max(0,victim->def->throwDamage);
            a.hitTargets.insert(victim->id);
            releaseGrab(a,true,dmg,vx,va,"fall");
        }
    }
    executeFrameCommands(a,*f);if(a.dead)return;executeFrameEvents(a,*an);
}
void Game::advanceAnim(Actor&a,float dt){
    if(a.combatPauseTime>0)return;auto an=animation(a,a.anim);if(!an||an->frames.empty())return;
    auto f=frame(a);float seconds=openBorFrameSeconds(f?f->delay:10);a.frameTime+=dt;
    while(a.frameTime>=seconds){
        a.frameTime-=seconds;
        if(a.frame+1<an->frames.size()){++a.frame;enterFrame(a);}
        else if(an->loop){a.frame=0;a.attackWindowActive=false;a.attackOneConsumed=false;a.hitTargets.clear();enterFrame(a);}
        else{
            std::string prev=a.anim;
            if(a.dead){
                if(starts(prev,"fall")||prev=="burn"||prev=="shock"){float floor=platformFloor(a);if(a.a>floor+1.f||std::abs(a.va)>1.f){a.frame=an->frames.size()-1;a.frameTime=0;return;}}
                a.deathAnimationFinished=true;if(a.deathFinishedAt<0)a.deathFinishedAt=a.deathTime;return;
            }
            if(prev=="grab"&&a.grabTarget&&a.def&&a.def->grabFinish!=0){a.grabReady=true;a.frame=an->frames.size()-1;a.frameTime=0;return;}
            if(a.unequipAfterAnim){a.unequipAfterAnim=false;unequipWeapon(a);setAnim(a,"idle");return;}
            if(a.effect){a.dead=true;return;}
            if(a.projectile){if(a.projectileExploding){a.dead=true;return;}a.frame=0;a.frameTime=0;a.attackWindowActive=false;a.hitTargets.clear();enterFrame(a);return;}
            if(starts(prev,"grabattack")&&a.grabTarget){setAnim(a,"grab");return;}
            if(prev=="grabup"&&a.grabTarget){releaseGrab(a,true,20,(a.facingLeft?-1.f:1.f)*55.f,185.f,"fall15");setAnim(a,"idle");return;}
            if(starts(prev,"fall")||prev=="burn"||prev=="shock"){
                float floor=platformFloor(a);if(a.a>floor+1.f||std::abs(a.va)>1.f){a.frame=an->frames.size()-1;a.frameTime=0;return;}
                auto rn=!a.riseAnim.empty()&&animation(a,a.riseAnim)?a.riseAnim:(animation(a,"rise")?"rise":"idle");
                a.riseAnim.clear();setAnim(a,rn);return;
            }
            if(starts(prev,"pain")||prev=="bpain"||prev=="spain"){setAnim(a,"idle");return;}
            if(starts(prev,"rise")||prev=="brise"||prev=="srise"){setAnim(a,"idle");return;}
            if(starts(prev,"attack")||starts(prev,"freespecial")||prev=="jumpattack"||prev=="jumpforward"||starts(prev,"grabforward")||starts(prev,"grabbackward")){
                if(starts(prev,"freespecial"))a.pendingEnergyCost=0;setAnim(a,a.grabTarget&&animation(a,"grab")?"grab":"idle");return;
            }
            setAnim(a,"idle");return;
        }
        an=animation(a,a.anim);f=frame(a);seconds=openBorFrameSeconds(f?f->delay:10);
    }
}


Actor* Game::createActor(const std::string&model,float x,float z,bool left,bool isPlayer,bool boss,int hpOverride,const std::string&dropItem,Team forcedTeam,int playerIndex){auto d=db_.entity(model);if(!d)return nullptr;Actor a;a.id=nextId_++;a.def=d;a.baseModel=lower(model);a.displayName=d->name;a.x=x;a.z=z;a.facingLeft=left;a.player=isPlayer;a.playerIndex=playerIndex;a.boss=boss;a.maxHp=hpOverride>0?hpOverride:std::max(1,d->health);a.hp=a.maxHp;a.dropItem=dropItem;if(forcedTeam!=Team::Neutral)a.team=forcedTeam;else if(isPlayer)a.team=Team::Player;else if(d->type=="enemy")a.team=Team::Enemy;else if(d->type=="npc")a.team=Team::Ally;else a.team=Team::Neutral;if(isPlayer&&d->makeInv!=0){a.invincibleTime=(float)std::abs(d->makeInv);a.invincibleNoBlink=d->makeInvNoBlink;}a.anim=animation(a,"spawn")?"spawn":animation(a,"idle")?"idle":(d->animations.empty()?"":d->animations.begin()->first);actors_.push_back(std::move(a));auto*r=&actors_.back();enterFrame(*r);return r;}
void Game::spawnActor(const SpawnDef&s){float base=cameraForProgress(s.trigger);auto a=createActor(s.model,base+s.x,s.z,s.flip,false,s.boss,s.healthOverride,s.item);if(a){a->a=s.a;if(!s.alias.empty())a->displayName=s.alias;a->paletteMap=s.map;if(!s.weapon.empty()){if(auto wi=db_.entity(s.weapon))equipWeapon(*a,*wi);}}}

void Game::startGame(){credits_=db_.campaign().credits;for(auto&s:slots_)if(s.joined){s.lives=db_.campaign().lives;s.score=0;s.nextLifeScore=std::max(1,db_.rules().lifeScore);s.nextCreditScore=std::max(1,db_.rules().creditScore);s.respawnTimer=0;}continueTimer_=10.f;if(!db_.campaign().introScenes.empty())startSceneQueue(db_.campaign().introScenes,Mode::Playing,0);else{mode_=Mode::Playing;loadStage(0,false);}}
bool Game::hasSave()const{return db_.campaign().canSave>0&&std::filesystem::exists(saveFile_);}
void Game::saveProgress(){if(db_.campaign().canSave<=0||(mode_!=Mode::Playing&&mode_!=Mode::Pause))return;std::error_code ec;std::filesystem::create_directories(saveFile_.parent_path(),ec);std::ofstream o(saveFile_,std::ios::trunc);if(!o)return;o<<"version=3\n"<<"stage="<<stageIndex_<<"\n"<<"credits="<<credits_<<"\n";for(size_t i=0;i<slots_.size();++i){auto&s=slots_[i];o<<"p"<<i<<".joined="<<(s.joined?1:0)<<"\n"<<"p"<<i<<".selected="<<s.selected<<"\n"<<"p"<<i<<".palette="<<s.paletteMap<<"\n"<<"p"<<i<<".lives="<<s.lives<<"\n"<<"p"<<i<<".score="<<s.score<<"\n";if(auto p=player(i))o<<"p"<<i<<".hp="<<p->hp<<"\n"<<"p"<<i<<".def="<<(p->def?p->def->name:p->baseModel)<<"\n"<<"p"<<i<<".weapon="<<p->weaponNumber<<"\n"<<"p"<<i<<".uses="<<p->weaponUses<<"\n";}}
bool Game::loadSave(){if(!hasSave())return false;std::ifstream in(saveFile_);if(!in)return false;int savedStage=0,savedCredits=db_.campaign().credits;std::array<int,3> hp{0,0,0},weapon{0,0,0},uses{0,0,0};std::array<std::string,3> def{};resetSelection();std::string line;while(std::getline(in,line)){auto pos=line.find('=');if(pos==std::string::npos)continue;auto k=line.substr(0,pos),v=line.substr(pos+1);if(k=="stage")savedStage=toInt(v);else if(k=="credits")savedCredits=toInt(v);else for(size_t i=0;i<3;++i){auto pfx="p"+std::to_string(i)+".";if(k.rfind(pfx,0)!=0)continue;auto f=k.substr(pfx.size());if(f=="joined")slots_[i].joined=toInt(v)!=0;else if(f=="selected")slots_[i].selected=(int)clampf((float)toInt(v),0.f,2.f);else if(f=="palette")slots_[i].paletteMap=std::max(0,toInt(v));else if(f=="lives")slots_[i].lives=toInt(v);else if(f=="score")slots_[i].score=toInt(v);else if(f=="hp")hp[i]=toInt(v);else if(f=="def")def[i]=lower(v);else if(f=="weapon")weapon[i]=toInt(v);else if(f=="uses")uses[i]=toInt(v);}}credits_=savedCredits;for(auto&s:slots_){s.nextLifeScore=((s.score/std::max(1,db_.rules().lifeScore))+1)*std::max(1,db_.rules().lifeScore);s.nextCreditScore=((s.score/std::max(1,db_.rules().creditScore))+1)*std::max(1,db_.rules().creditScore);}mode_=Mode::Playing;loadStage(std::clamp(savedStage,0,(int)db_.campaign().stages.size()-1),false);for(size_t i=0;i<3;++i)if(auto p=player(i)){if(hp[i]>0)p->hp=std::min(p->maxHp,hp[i]);if(weapon[i]>0&&!def[i].empty()){if(auto wd=db_.entity(def[i])){p->def=wd;p->weaponNumber=weapon[i];p->weaponUses=uses[i];}}}return true;}
void Game::loadStage(int idx,bool preserveState){if(idx<0||idx>=(int)db_.campaign().stages.size()){if(!db_.campaign().endScenes.empty())startSceneQueue(db_.campaign().endScenes,Mode::Finished);else{mode_=Mode::Finished;audio_.playMusic("music/ending.bor",false);}return;}struct PState{int hp=-1,w=0,u=0;std::string def;};std::array<PState,3> old{};int oldChapter=-1;if(preserveState){for(size_t i=0;i<3;++i)if(auto p=player(i)){old[i].hp=p->hp;old[i].w=p->weaponNumber;old[i].u=p->weaponUses;if(p->def)old[i].def=p->def->name;}if(stageIndex_>=0&&stageIndex_<(int)db_.campaign().stages.size())oldChapter=db_.campaign().stages[stageIndex_].chapter;}stageIndex_=idx;level_=db_.loadLevel(db_.campaign().stages[idx]);{float visualWidth=0.f;if(!level_.panels.empty()){std::vector<int> order=level_.panelOrder;if(order.empty())for(size_t pi=0;pi<level_.panels.size();++pi)order.push_back((int)pi);for(int pi:order)if(pi>=0&&pi<(int)level_.panels.size())if(auto im=renderer_.image(level_.panels[(size_t)pi]))visualWidth+=im->w;}if(visualWidth<=0&&!level_.background.empty())if(auto im=renderer_.image(level_.background))visualWidth=im->w;if(visualWidth>0)level_.worldWidth=std::max(renderer_.logicalWidth(),visualWidth);else level_.worldWidth=std::max(level_.worldWidth,renderer_.logicalWidth());}stageTime_=(float)level_.setTime;globalActorVars_.clear();globalIntVars_.clear();actors_.clear();pendingSpawns_.clear();playerId_=0;nextSpawn_=nextWait_=nextGroup_=nextBlockade_=0;groupMin_=0;groupMax_=100;bossesDeclared_=(int)std::count_if(level_.spawns.begin(),level_.spawns.end(),[](const SpawnDef& sp){return sp.boss;});bossesRemaining_=bossesDeclared_;groupRefillLocked_=false;bossClearTriggered_=false;levelProgress_=maxLevelProgress_=0;backscrollFloor_=0;bgScrollX_=0;verticalStageScroll_=0;bool reverse=isReverseScroll();float viewW=renderer_.logicalWidth();cameraX_=isVerticalScroll()?0.f:(reverse?std::max(0.f,level_.worldWidth-viewW):0.f);furthestProgress_=0;clearTimer_=0;bool sameChapter=preserveState&&oldChapter==db_.campaign().stages[idx].chapter;float mid=(level_.zMin+level_.zMax)*.5f;for(size_t i=0;i<slots_.size();++i){slots_[i].actorId=0;slots_[i].respawnTimer=0;if(!slots_[i].joined||slots_[i].lives<=0)continue;float rawX=cameraX_+(reverse?250.f-(float)i*24.f:70.f+(float)i*24.f),rawZ=mid+(float)((int)i-1)*10.f;auto safe=findSafePlayerSpawnPoint(level_,rawX,rawZ,reverse,renderer_.logicalWidth());float px=safe.first,pz=safe.second;auto p=createActor(players_[(size_t)slots_[i].selected],px,pz,reverse,true,false,0,{},Team::Player,(int)i);if(p){p->paletteMap=slots_[i].paletteMap;slots_[i].actorId=p->id;if(!playerId_)playerId_=p->id;if(sameChapter&&old[i].hp>0){p->hp=std::min(p->maxHp,old[i].hp);if(old[i].w>0&&!old[i].def.empty())if(auto wd=db_.entity(old[i].def)){p->def=wd;p->weaponNumber=old[i].w;p->weaponUses=old[i].u;}}}}updateLevelRules();while(nextSpawn_<level_.spawns.size()&&level_.spawns[nextSpawn_].trigger<=0){auto&s0=level_.spawns[nextSpawn_];auto d0=db_.entity(s0.model);bool enemy0=d0&&d0->type=="enemy";if(enemy0&&!canSpawnGroupedEnemy())break;spawnActor(s0);++nextSpawn_;}audio_.playMusic(level_.music,true);loadingTime_=.45f;mode_=Mode::Loading;if(db_.campaign().canSave>0)saveProgress();}
void Game::respawnPlayer(size_t i,bool fullHealth){if(!level_.noResetTime)stageTime_=(float)level_.setTime;if(i>=slots_.size()||!slots_[i].joined||slots_[i].lives<=0)return;if(auto p=player(i))breakGrabLinks(*p);int old=slots_[i].actorId;actors_.erase(std::remove_if(actors_.begin(),actors_.end(),[&](const Actor&a){return a.id==old;}),actors_.end());float mid=(level_.zMin+level_.zMax)*.5f;bool reverse=isReverseScroll();float rawX=cameraX_+(reverse?250.f-(float)i*24.f:70.f+(float)i*24.f),rawZ=mid+(float)((int)i-1)*10.f;auto safe=findSafePlayerSpawnPoint(level_,rawX,rawZ,reverse,renderer_.logicalWidth());float px=safe.first,pz=safe.second;auto p=createActor(players_[(size_t)slots_[i].selected],px,pz,reverse,true,false,0,{},Team::Player,(int)i);if(p){p->paletteMap=slots_[i].paletteMap;slots_[i].actorId=p->id;if(i==0)playerId_=p->id;if(!fullHealth)p->hp=std::max(1,p->maxHp/2);}slots_[i].respawnTimer=0;}

bool Game::isHostile(const Actor&att,const Actor&t)const{
    if(att.id==t.id||t.dead||t.effect)return false;
    if(att.def&&!att.def->hostile.empty()){
        if(containsType(att.def->hostile,"none"))return false;
        std::string type=t.projectile?"shot":(t.def?lower(t.def->type):std::string{});
        return !type.empty()&&containsType(att.def->hostile,type);
    }
    if(att.team==Team::Enemy)return t.team==Team::Player||t.team==Team::Ally;
    if(att.team==Team::Player||att.team==Team::Ally)return t.team==Team::Enemy;
    return false;
}
Actor* Game::nearestOpponent(Actor&a){Actor*best=nullptr;float bd=std::numeric_limits<float>::max();for(auto&t:actors_){if(t.dead||t.id==a.id||t.projectile||t.effect||!isHostile(a,t))continue;float dx=t.x-a.x,dz=t.z-a.z,d=dx*dx+dz*dz;if(d<bd){bd=d;best=&t;}}return best;}
bool Game::beginGrab(Actor&g,Actor&t){if(g.grabTarget||t.grabbedBy||g.dead||t.dead||t.projectile||t.effect)return false;if(!db_.rules().noLost&&g.player&&g.weaponNumber>0)unequipWeapon(g);if(t.def&&g.def&&(t.def->antigrab-g.def->grabForce)>0)return false;if(!animation(g,"grab"))return false;g.grabTarget=t.id;t.grabbedBy=g.id;g.vx=g.vz=0;t.vx=t.vz=t.va=0;t.a=platformFloor(t);setAnim(g,"grab");if(animation(t,"grabbed"))setAnim(t,"grabbed");updateGrabBinding(g);return true;}
void Game::updateGrabBinding(Actor&g){auto t=actorById(g.grabTarget);if(!t||t->dead){g.grabTarget=0;g.grabReady=false;return;}float dir=g.facingLeft?-1.f:1.f;t->x=g.x+dir*17.f;t->z=g.z;t->a=g.a;t->facingLeft=!g.facingLeft;t->vx=t->vz=t->va=0;}
void Game::releaseGrab(Actor&g,bool thrown,int dmg,float vx,float va,const std::string&fallAnim){
    auto t=actorById(g.grabTarget);if(!t){g.grabTarget=0;g.grabReady=false;return;}
    t->grabbedBy=0;g.grabTarget=0;g.grabReady=false;
    if(thrown){
        if(dmg>0)damage(*t,g,dmg,1,"","",{},nullptr);
        t->vx=vx;t->va=va;t->a=std::max(platformFloor(*t)+2.f,t->a);t->thrownBy=g.id;
        std::string fn=animation(*t,fallAnim)?fallAnim:(animation(*t,"fall")?"fall":"idle");
        if(starts(fn,"fall")&&fn.size()>4){auto rn="rise"+fn.substr(4);t->riseAnim=animation(*t,rn)?rn:(animation(*t,"rise")?"rise":"");}
        else t->riseAnim=animation(*t,"rise")?"rise":"";
        setAnim(*t,fn);
    }else if(!t->dead){auto pn=reactionAnimation(*t,"attack",ReactionKind::Pain);setAnim(*t,pn.empty()?"idle":pn);}
}
void Game::breakGrabLinks(Actor&a){if(a.grabTarget){auto t=actorById(a.grabTarget);if(t)t->grabbedBy=0;a.grabTarget=0;a.grabReady=false;}if(a.grabbedBy){auto g=actorById(a.grabbedBy);if(g){g->grabTarget=0;g->grabReady=false;}a.grabbedBy=0;}}

bool Game::equipWeapon(Actor&p,const EntityDef&item){if(item.subtype!="weapon"||item.weaponNumber<=0)return false;auto base=db_.entity(p.baseModel);if(!base)return false;size_t idx=(size_t)(item.weaponNumber-1);if(idx>=base->weapons.size())return false;auto name=base->weapons[idx];if(name=="none")return false;auto wd=db_.entity(name);if(!wd)return false;p.def=wd;p.weaponNumber=item.weaponNumber;p.weaponUses=item.counter>0?item.counter:((wd->typeShot&&wd->shootNum>0)?wd->shootNum:999);if(animation(p,"get"))setAnim(p,"get");else setAnim(p,"idle");return true;}
void Game::unequipWeapon(Actor&p){auto base=db_.entity(p.baseModel);if(!base)return;p.def=base;p.weaponNumber=0;p.weaponUses=0;if(!animation(p,p.anim))setAnim(p,"idle");}
void Game::registerScore(int pi,int amount){if(amount<=0||pi<0||pi>=(int)slots_.size())return;auto&s=slots_[(size_t)pi];s.score+=amount;highScore_=std::max(highScore_,s.score);while(db_.rules().lifeScore>0&&s.score>=s.nextLifeScore){++s.lives;audio_.playSfx("sounds/get.wav",.9f);s.nextLifeScore+=db_.rules().lifeScore;}while(db_.rules().creditScore>0&&s.score>=s.nextCreditScore){++credits_;audio_.playSfx("sounds/get.wav",.9f);s.nextCreditScore+=db_.rules().creditScore;}}

void Game::resolveWalls(Actor&a,float oldX,float oldZ){
    if(a.projectile||a.effect||a.dead)return;
    for(const auto&w:level_.walls){
        if(a.a>w.height+2.f)continue;
        auto inside=[&](float x,float z){
            return openBorTerrainContainsPoint(w.x,w.z,w.upperLeft,w.lowerLeft,w.upperRight,w.lowerRight,w.depth,x,z);
        };
        if(!inside(a.x,a.z))continue;
        // Resolve one axis at a time. Because the wall span is interpolated at the actor's Z,
        // slanted/trapezoidal OpenBOR terrain remains slanted instead of becoming a giant box.
        if(!inside(oldX,a.z)){a.x=oldX;continue;}
        if(!inside(a.x,oldZ)){a.z=oldZ;continue;}
        a.x=oldX;a.z=oldZ;
    }
    if(level_.blocked!=0&&a.player)a.x=std::min(a.x,std::max(8.f,level_.worldWidth-15.f));
}

float Game::platformFloor(const Actor&a)const{float best=0;for(const auto&p:actors_){if(p.id==a.id||p.dead||p.projectile||p.effect)continue;auto f=frame(p);if(!f||!f->platform.valid)continue;const auto&pf=f->platform;float px=p.x-f->offsetX+pf.x,pz=p.z-f->offsetY+pf.z;float left=px+std::min({pf.lowerLeft,pf.upperLeft,pf.lowerRight,pf.upperRight}),right=px+std::max({pf.lowerLeft,pf.upperLeft,pf.lowerRight,pf.upperRight});float zn=pz-pf.depth,zf=pz+3.f;if(a.x>=left-5&&a.x<=right+5&&a.z>=zn-3&&a.z<=zf+3){float top=std::max(0.f,p.a+pf.alt);if(top<=a.a+18.f)best=std::max(best,top);}}return best;}

void Game::updatePlayer(Actor&a,const InputState&in,float dt){if(a.dead||a.grabbedBy||a.combatPauseTime>0)return;if(!starts(a.anim,"freespecial"))a.pendingEnergyCost=0;float floor=platformFloor(a);bool grounded=std::abs(a.a-floor)<1.2f&&std::abs(a.va)<1.f;
    if(!grounded&&db_.rules().autoland!=2&&in.up&&in.jumpPressed)a.safeLandingRequested=true;
    if(a.grabTarget){auto t=actorById(a.grabTarget);if(!t||t->dead){a.grabTarget=0;a.grabReady=false;setAnim(a,"idle");return;}updateGrabBinding(a);if(a.def&&a.def->grabFinish!=0&&!a.grabReady&&a.anim=="grab")return;bool throwing=starts(a.anim,"grabforward")||starts(a.anim,"grabbackward")||a.anim=="grabup"||starts(a.anim,"follow"),striking=starts(a.anim,"grabattack");if(!throwing&&!striking){if(in.up&&in.attackPressed&&animation(a,"grabup")){setAnim(a,"grabup");return;}if(in.specialPressed){std::string n=animation(a,"grabbackward")?"grabbackward":"grabforward";setAnim(a,n);return;}if(in.jumpPressed&&animation(a,"grabforward")){setAnim(a,"grabforward");return;}if(in.attackPressed){std::string n=(a.combo++%2==0&&animation(a,"grabattack"))?"grabattack":"grabattack2";if(!animation(a,n))n="grabattack";setAnim(a,n);return;}float dx=(in.right?1.f:0.f)-(in.left?1.f:0.f),dz=(in.down?1.f:0.f)-(in.up?1.f:0.f);if((dx||dz)&&animation(a,"grabwalk")){float sp=32.f,ox=a.x,oz=a.z;a.x+=dx*sp*dt;a.z+=dz*sp*.5f*dt;resolveWalls(a,ox,oz);if(dx<0)a.facingLeft=true;else if(dx>0)a.facingLeft=false;setAnim(a,"grabwalk",false);}else setAnim(a,"grab",false);}updateGrabBinding(a);return;}
    bool offensiveAir=starts(a.anim,"jumpattack")||starts(a.anim,"freespecial");
    bool busy=starts(a.anim,"attack")||starts(a.anim,"freespecial")||starts(a.anim,"pain")||starts(a.anim,"fall")||a.anim=="get"||offensiveAir;
    bool airTransition=grounded||db_.rules().noAirCancel==0||a.anim=="jump"||a.anim=="jumpforward";
    if(in.attack2Pressed&&!busy&&grounded){auto n=commandAnimation(a,"a2","freespecial2");if(!n.empty()){auto an=animation(a,n);if(!an||a.hp>an->energyCost){setAnim(a,n);if(an&&an->energyCost>0){if(db_.rules().noCost)a.pendingEnergyCost=an->energyCost;else a.hp=std::max(1,a.hp-an->energyCost);}}return;}}
    if(in.specialPressed&&!busy&&grounded){auto n=commandAnimation(a,"s","freespecial");if(!n.empty()){auto an=animation(a,n);if(!an||a.hp>an->energyCost){setAnim(a,n);if(an&&an->energyCost>0){if(db_.rules().noCost)a.pendingEnergyCost=an->energyCost;else a.hp=std::max(1,a.hp-an->energyCost);}}return;}if(animation(a,"block")){setAnim(a,"block");return;}}
    if(in.attackPressed&&!busy&&grounded){Actor*grabCandidate=nullptr;float bd=9999;for(auto&t:actors_){if(t.dead||t.team!=Team::Enemy||t.grabbedBy||t.projectile)continue;float dx=std::abs(t.x-a.x),dz=std::abs(t.z-a.z);if(dx<24&&dz<13&&dx+dz<bd){bd=dx+dz;grabCandidate=&t;}}if(grabCandidate&&beginGrab(a,*grabCandidate))return;std::string n;auto base=db_.entity(a.baseModel);const auto&chain=(base&&!base->attackChain.empty())?base->attackChain:a.def->attackChain;if(!chain.empty()){int step=chain[(size_t)(a.combo%(int)chain.size())];a.combo=(a.combo+1)%(int)chain.size();n=(step==1&&animation(a,"attack"))?"attack":"attack"+std::to_string(step);}else{a.combo=(a.combo%3)+1;n=a.combo==1?(animation(a,"attack")?"attack":"attack1"):a.combo==2?"attack2":"attack3";}if(!animation(a,n))n=animation(a,"attack")?"attack":"attack1";setAnim(a,n);a.comboTimer=.55f;return;}
    if(in.jumpPressed&&!busy&&grounded){a.va=145.f;if(animation(a,"jump"))setAnim(a,"jump");}
    if(a.comboTimer>0){a.comboTimer-=dt;if(a.comboTimer<=0)a.combo=0;}
    if(!busy){float dx=(in.right?1.f:0.f)-(in.left?1.f:0.f),dz=(in.down?1.f:0.f)-(in.up?1.f:0.f);if(dx||dz){float len=std::sqrt(dx*dx+dz*dz);dx/=len;dz/=len;float speed=60.f+std::max(0.f,a.def->speed-6.f)*4.f,ox=a.x,oz=a.z;a.x+=dx*speed*dt;a.z+=dz*speed*.65f*dt;resolveWalls(a,ox,oz);if(dx<0)a.facingLeft=true;else if(dx>0)a.facingLeft=false;if(grounded)setAnim(a,"walk",false);}else if(grounded)setAnim(a,"idle",false);}
    if(!grounded&&in.attackPressed&&airTransition){std::string n=(in.down&&animation(a,"jumpattack2"))?"jumpattack2":"jumpattack";if(animation(a,n))setAnim(a,n);}
    a.z=clampf(a.z,level_.zMin,level_.zMax);a.x=clampf(a.x,cameraX_+8,std::min(level_.worldWidth-15,movementBoundary()));
}

void Game::updateEnemy(Actor&a,float dt){
    if(a.dead||a.projectile||a.effect||a.grabbedBy||a.combatPauseTime>0)return;
    auto t=nearestOpponent(a);if(!t)return;
    if(a.grabTarget){updateGrabBinding(a);if(a.def&&a.def->grabFinish!=0&&!a.grabReady&&a.anim=="grab")return;if(a.aiThink<=0&&animation(a,"grabattack")){setAnim(a,"grabattack");a.aiThink=.75f;}return;}
    a.aiThink-=dt;
    float dx=t->x-a.x,dz=t->z-a.z,dist=std::abs(dx);
    bool busy=starts(a.anim,"attack")||starts(a.anim,"pain")||starts(a.anim,"fall")||starts(a.anim,"rise")||starts(a.anim,"grab");
    if(busy)return;

    // Level spawn coordinates in OpenBOR are allowed to begin outside the viewport/playable Z band.
    // Those actors must enter the arena before proximity/attack AI is allowed to stop their movement.
    const auto entry=openBorArenaEntryIntent(a.x,a.z,cameraX_,cameraX_+renderer_.logicalWidth(),level_.zMin,level_.zMax);
    if(entry.active){
        float speed=(a.boss?34.f:28.f)+std::min(35.f,a.def->speed*2.1f);
        float desiredX=entry.xDir,desiredZ=entry.zDir;
        float ox=a.x,oz=a.z;
        a.x+=desiredX*speed*dt;
        a.z+=desiredZ*speed*.65f*dt;
        resolveWalls(a,ox,oz);
        if(desiredX<0)a.facingLeft=true;else if(desiredX>0)a.facingLeft=false;
        if(desiredX||desiredZ)setAnim(a,"walk",false);else setAnim(a,"idle",false);
        constexpr float kEnemyDepthEntryMargin=64.f;
        a.z=clampf(a.z,level_.zMin-kEnemyDepthEntryMargin,level_.zMax+kEnemyDepthEntryMargin);
        return;
    }

    if(dist<23&&std::abs(dz)<12&&animation(a,"grab")&&a.def&&t->def&&(t->def->antigrab-a.def->grabForce)<=0&&((a.id+a.attackCursor)%5==0)){if(beginGrab(a,*t)){a.aiThink=.7f;return;}}
    std::vector<std::string> choices;
    for(const auto&[name,an]:a.def->animations){if(starts(name,"attack")&&isOffensiveAnimation(an)&&dist+8>=an.rangeMin&&dist<=std::max(an.rangeMin+8.f,an.rangeMax)&&std::abs(dz)<32)choices.push_back(name);}
    std::sort(choices.begin(),choices.end());
    if(starts(t->anim,"attack")&&dist<55&&std::abs(dz)<18&&animation(a,"block")){unsigned mask=(unsigned)std::max(0,a.def->blockOdds);unsigned seed=(unsigned)(a.id*1103515245u+(unsigned)t->attackSerial*12345u+(unsigned)a.attackCursor);if(openBorBlockOddsPass(seed,(int)mask)){setAnim(a,"block");a.aiThink=.28f;return;}}
    if(a.aiThink<=0&&!choices.empty()){auto n=choices[(size_t)(a.attackCursor++%choices.size())];setAnim(a,n);float ag=clampf(a.def->aggression/100.f,.2f,1.5f);a.aiThink=(a.boss?.28f:.5f)/ag+.18f;return;}
    float speed=(a.boss?34.f:28.f)+std::min(35.f,a.def->speed*2.1f);
    bool chase=a.def->aiMove=="chase";
    float desiredX=(chase||dist>42)?(dx>0?1.f:-1.f):0.f,desiredZ=(std::abs(dz)>8)?(dz>0?1.f:-1.f):0.f;
    float ox=a.x,oz=a.z;
    a.x+=desiredX*speed*dt;
    a.z+=desiredZ*speed*.65f*dt;
    resolveWalls(a,ox,oz);
    if(desiredX<0)a.facingLeft=true;else if(desiredX>0)a.facingLeft=false;
    if(desiredX||desiredZ)setAnim(a,"walk",false);else setAnim(a,"idle",false);
    constexpr float kEnemyDepthEntryMargin=64.f;
    a.z=clampf(a.z,level_.zMin-kEnemyDepthEntryMargin,level_.zMax+kEnemyDepthEntryMargin);
}
void Game::updateProjectile(Actor&a,float dt){if(a.dead||!a.projectile||a.combatPauseTime>0)return;a.projectileLife+=dt;int off=a.def?std::max(80,a.def->offscreenKill):200;if(a.projectileLife>8.f||a.x<cameraX_-off||a.x>cameraX_+renderer_.logicalWidth()+off){a.dead=true;return;}if(a.projectileExploding)return;a.x+=a.vx*dt;a.z+=a.vz*dt;if(a.projectileArc){a.a+=a.va*dt;a.va-=300.f*dt;if(a.a<=0){a.a=0;a.va=0;a.vx=a.vz=0;a.projectileExploding=true;a.projectileArc=false;if(animation(a,"attack"))setAnim(a,"attack");else if(animation(a,"attack1"))setAnim(a,"attack1");else a.dead=true;}}}
void Game::handleLanding(Actor&a,float floorHeight){if(a.safeLandingRequested){a.safeLandingRequested=false;a.landingDamage=0;if(animation(a,"land"))setAnim(a,"land");else setAnim(a,"idle");return;}if(a.landingDamage>0){a.hp=std::max(0,a.hp-a.landingDamage);a.landingDamage=0;if(a.hp<=0){a.dead=true;if(animation(a,"death"))setAnim(a,"death");else if(animation(a,"fall"))setAnim(a,"fall");return;}}if(a.def&&a.def->bounce&&!a.bounced&&starts(a.anim,"fall")){a.bounced=true;a.va=85.f;a.a=floorHeight+1.f;return;}if(a.def&&!a.def->noQuake&&starts(a.anim,"fall")){shakeTime_=std::max(shakeTime_,.12f);shakeAmpX_=2;shakeAmpY_=2;}auto an=animation(a,a.anim);if(!an)return;for(const auto&e:an->events)if(e.kind=="land"){if(!e.args.empty())queueEffect(lower(e.args[0]),a.x,a.z,floorHeight,a.facingLeft);if(e.frame>=0&&e.frame<(int)an->frames.size()){a.frame=(size_t)e.frame;a.frameTime=0;enterFrame(a);break;}}}
void Game::updatePhysics(Actor&a,float dt){if(a.projectile||a.grabbedBy||a.combatPauseTime>0)return;float floor=platformFloor(a),oldA=a.a;bool airborne=a.a>floor+.01f||std::abs(a.va)>.01f;float ox=a.x,oz=a.z;a.x+=a.vx*dt;a.z+=a.vz*dt;resolveWalls(a,ox,oz);a.vx*=std::pow(.06f,dt);a.vz*=std::pow(.06f,dt);floor=platformFloor(a);if(airborne){a.a+=a.va*dt;a.va-=360.f*dt;if(a.va<=0&&a.a<=floor&&oldA>=floor-2.f){a.a=floor;a.va=0;handleLanding(a,floor);if(a.va==0&&!a.dead&&!starts(a.anim,"fall")&&!starts(a.anim,"pain")&&!starts(a.anim,"grab"))setAnim(a,"idle");}}else a.a=floor;}

RectF Game::bodyRect(const Actor&a)const{auto f=frame(a);if(!f||!f->bbox.valid)return {a.x-12,a.z-60-a.a,24,60,true};float left=boxLeftFromOrigin(a.x,f->offsetX,f->bbox.x,f->bbox.w,a.facingLeft);return {left,a.z-a.a-f->offsetY+f->bbox.y,f->bbox.w,f->bbox.h,true};}
RectF Game::attackRect(const Actor&a)const{auto f=frame(a);if(!f||!f->attack.rect.valid)return {};auto r=f->attack.rect;float left=boxLeftFromOrigin(a.x,f->offsetX,r.x,r.w,a.facingLeft);return {left,a.z-a.a-f->offsetY+r.y,r.w,r.h,true};}
bool Game::intersects(const RectF&a,const RectF&b){return a.valid&&b.valid&&a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;}
bool Game::canDamage(const Actor&att,const Actor&t)const{if(t.effect||t.projectile||att.id==t.id||att.projectileOwner==t.id)return false;if(t.def&&t.def->type=="obstacle")return true;if(att.def&&!att.def->canDamage.empty()){if(!t.def||!containsType(att.def->canDamage,t.def->type))return false;if(att.team==Team::Player&&t.team==Team::Player&&!db_.rules().versusDamage)return false;return true;}if(att.team==Team::Enemy)return t.team==Team::Player||t.team==Team::Ally;if(att.team==Team::Player){if(t.team==Team::Player)return db_.rules().versusDamage;return t.team==Team::Enemy;}if(att.team==Team::Ally)return t.team==Team::Enemy;return false;}
void Game::damage(Actor&t,Actor&att,int amount,int knockdown,const std::string&hitfx,const std::string&hitflash,const std::string&kind,const AttackBox* attack){
    if(t.dead||amount<0||t.invincibleTime>0)return;
    if(att.player&&att.playerIndex>=0&&att.playerIndex<(int)slots_.size()&&!t.player)slots_[(size_t)att.playerIndex].hudTargetId=t.id;
    if(t.player&&t.playerIndex>=0&&t.playerIndex<(int)slots_.size()&&!att.player)slots_[(size_t)t.playerIndex].hudTargetId=att.id;
    if(t.player&&starts(t.anim,"freespecial"))t.pendingEnergyCost=0;

    const int raw=amount;
    const bool contactOnly=raw==0;
    if(t.def&&raw>0)amount=std::max(0,(int)std::lround(raw*t.def->defenseAll));
    // A positive attack reduced to zero by defense is fully absorbed.
    // A native zero-force attack is different: OpenBOR still treats it as contact.
    if(raw>0&&amount<=0)return;

    bool blocked=attackWouldBeBlocked(t,att,raw,attack);
    bool allowFlash=!attack||!attack->noFlash;
    if(blocked){
        if(!hitfx.empty())audio_.playSfx(hitfx,.45f);
        if(allowFlash&&t.def&&!t.def->blockFlash.empty())queueEffect(t.def->blockFlash,t.x,t.z,t.a,att.x>t.x);
        if(allowFlash)flashTime_=std::max(flashTime_,.018f);
        t.x+=(t.x>=att.x?1.f:-1.f)*2.f;
        return;
    }

    if(amount>0)t.hp-=amount;
    if(!hitfx.empty())audio_.playSfx(hitfx,.8f);
    if(allowFlash&&!hitflash.empty()&&(!t.def||!t.def->noAtFlash)){
        queueEffect(hitflash,t.x,t.z,t.a,att.x>t.x);
        flashTime_=std::max(flashTime_,.028f);
    }
    if(attack&&attack->pause>0){
        float pause=std::min(.25f,openBorPauseSeconds(attack->pause));
        att.combatPauseTime=std::max(att.combatPauseTime,pause);
        t.combatPauseTime=std::max(t.combatPauseTime,pause);
    }

    float dir=(t.x>=att.x)?1.f:-1.f;
    if(!contactOnly||knockdown>0)t.x+=dir*4.f;

    if(amount>0&&t.hp<=0){
        t.hp=0;t.dead=true;t.deathTime=0;t.deathFinishedAt=-1.f;breakGrabLinks(t);
        if(!t.scoreAwarded&&att.team==Team::Player){registerScore(att.playerIndex,t.def?t.def->score:0);t.scoreAwarded=true;}
        if(t.def&&!t.def->deathSound.empty())audio_.playSfx(t.def->deathSound,.75f);
        auto dn=reactionAnimation(t,kind,ReactionKind::Death);
        if(!dn.empty())setAnim(t,dn);else t.deathAnimationFinished=true;
        return;
    }

    if(knockdown>0){
        t.thrownBy=att.id;t.bounced=false;
        if(attack&&attack->customDrop){
            float face=att.facingLeft?-1.f:1.f;
            t.va=std::max(20.f,attack->dropY*42.f);
            t.vx=face*attack->dropX*42.f;
            t.vz=attack->dropZ*24.f;
        }else{
            t.va=95.f;
            t.vx=dir*55.f;
        }
        if(attack){t.landingDamage=std::max(0,attack->landingDamage);t.landingMode=attack->landingMode;}
        auto fn=reactionAnimation(t,kind,ReactionKind::Fall);
        t.riseAnim=reactionAnimation(t,kind,ReactionKind::Rise);
        if(!fn.empty())setAnim(t,fn);
        else{
            auto pn=reactionAnimation(t,kind,ReactionKind::Pain);
            setAnim(t,pn.empty()?"idle":pn);
        }
    }else if(amount>0){
        auto pn=reactionAnimation(t,kind,ReactionKind::Pain);
        setAnim(t,pn.empty()?"idle":pn);
    }
}
void Game::updateCombat(){
    for(auto&att:actors_){
        if(att.dead||att.effect||att.combatPauseTime>0)continue;
        auto an=animation(att,att.anim);
        auto f=frame(att);
        // Zero-force boxes are real OpenBOR contacts (grab/follow/throw triggers).
        // A 0x0 rectangle still disables an attack naturally via rect.valid.
        if(!an||!f||!f->attack.rect.valid)continue;
        if(an->attackOne&&att.attackOneConsumed)continue;

        auto ar=attackRect(att);
        bool follow=false;
        for(auto&t:actors_){
            if(t.dead||t.invincibleTime>0||att.hitTargets.count(t.id)||!canDamage(att,t))continue;
            if(std::abs(att.z-t.z)>std::max(1.f,f->attack.zDepth)||!intersects(ar,bodyRect(t)))continue;
            if(t.hitInvincibleTime>0&&!an->fastAttack)continue;

            bool blocked=attackWouldBeBlocked(t,att,f->attack.damage,&f->attack);
            const bool heldTarget=att.grabTarget==t.id&&t.grabbedBy==att.id;

            att.hitTargets.insert(t.id);
            if(an->attackOne)att.attackOneConsumed=true;
            if(att.player&&att.pendingEnergyCost>0){
                att.hp=std::max(1,att.hp-att.pendingEnergyCost);
                att.pendingEnergyCost=0;
            }

            std::string hitflash=!f->hitflash.empty()?f->hitflash:(att.def?att.def->defaultFlash:std::string{});
            damage(t,att,f->attack.damage,f->attack.knockdown,f->hitfx,hitflash,f->attack.kind,&f->attack);

            // Authored throw attacks release the victim only when the real knockdown
            // attack connects. Preserve the velocities/fall reaction created by damage().
            if(heldTarget&&f->attack.knockdown>0&&att.grabTarget==t.id){
                t.grabbedBy=0;
                att.grabTarget=0;
                att.grabReady=false;
                if(!t.dead)t.a=std::max(platformFloor(t)+2.f,t.a);
            }

            if(f->attack.damage>0||f->attack.knockdown>0)t.hitInvincibleTime=an->fastAttack?.025f:.085f;
            if(att.player&&att.weaponNumber>0&&att.weaponNumber!=3&&att.weaponUses>0){
                if(--att.weaponUses<=0)unequipWeapon(att);
            }

            if(an->followAnim>0&&an->followCond>0){
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
            }

            if(att.projectile&&att.removeOnHit&&!att.projectileExploding){att.dead=true;break;}
            if(follow||an->attackOne)break;
        }
        if(follow){
            auto n="follow"+std::to_string(an->followAnim);
            if(animation(att,n))setAnim(att,n);
        }
    }
}



bool Game::enemiesAlive()const{for(const auto&a:actors_)if(!a.dead&&!a.projectile&&!a.effect&&a.team==Team::Enemy)return true;return false;}
int Game::activeEnemies()const{int n=0;for(const auto&a:actors_)if(!a.dead&&!a.projectile&&!a.effect&&a.team==Team::Enemy)++n;return n;}
bool Game::isReverseScroll()const{return isReverseDirection(level_.direction);}
bool Game::isBidirectionalScroll()const{return isBidirectionalDirection(level_.direction);}
bool Game::isVerticalScroll()const{return isVerticalDirection(level_.direction);}
float Game::cameraProgress()const{float maxCam=std::max(0.f,level_.worldWidth-renderer_.logicalWidth());return isReverseScroll()?maxCam-cameraX_:cameraX_;}
float Game::scrollProgress()const{return levelProgress_;}
float Game::cameraForProgress(float progress)const{float maxCam=std::max(0.f,level_.worldWidth-renderer_.logicalWidth());return isReverseScroll()?clampf(maxCam-progress,0,maxCam):clampf(progress,0,maxCam);}
void Game::advanceLevelProgress(float dt){
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
}
bool Game::stageSequenceComplete()const{
    return nextSpawn_>=level_.spawns.size()&&nextWait_>=level_.waits.size()&&
           nextGroup_>=level_.groups.size()&&nextBlockade_>=level_.blockades.size()&&
           maxLevelProgress_+5.f>=level_.maxTrigger;
}
void Game::processBossDefeats(){
    bool changed=false;
    for(auto&a:actors_)if(a.boss&&a.dead){a.boss=false;if(bossesRemaining_>0)--bossesRemaining_;changed=true;}
    if(!changed||bossClearTriggered_||bossesDeclared_<=0||bossesRemaining_>0)return;
    bossClearTriggered_=true;
    nextSpawn_=level_.spawns.size();nextWait_=level_.waits.size();nextGroup_=level_.groups.size();nextBlockade_=level_.blockades.size();
    for(auto&a:actors_)if(!a.dead&&!a.player&&!a.projectile&&!a.effect&&a.team==Team::Enemy){
        breakGrabLinks(a);a.hp=0;a.dead=true;
        if(a.def&&!a.def->deathSound.empty())audio_.playSfx(a.def->deathSound,.75f);
        auto death=reactionAnimation(a,"attack",ReactionKind::Death);
        if(!death.empty())setAnim(a,death);else{a.deathAnimationFinished=true;a.deathFinishedAt=a.deathTime;}
    }
    clearTimer_=0;
}
void Game::cleanupDead(){
    std::vector<std::tuple<std::string,float,float>>drops;
    for(auto it=globalActorVars_.begin();it!=globalActorVars_.end();)it->second&&!actorById(it->second)?it=globalActorVars_.erase(it):++it;
    for(auto&a:actors_){
        if(!a.player&&!a.boss&&!a.dead&&a.def){int off=std::max(80,a.def->offscreenKill);if(a.x<cameraX_-off||a.x>cameraX_+renderer_.logicalWidth()+off){a.dead=true;a.deathAnimationFinished=true;a.deathFinishedAt=a.deathTime;}}
        if(a.dead&&!a.dropSpawned&&!a.dropItem.empty()){a.dropSpawned=true;drops.emplace_back(a.dropItem,a.x,a.z);}
    }
    std::unordered_set<int> airborneDead;
    for(const auto&a:actors_)if(a.dead&&!a.player&&!a.effect&&!a.projectile&&
        (starts(a.anim,"fall")||a.anim=="burn"||a.anim=="shock")&&
        (a.a>platformFloor(a)+1.f||std::abs(a.va)>1.f))airborneDead.insert(a.id);
    actors_.erase(std::remove_if(actors_.begin(),actors_.end(),[this,&airborneDead](const Actor&a){
        if(!a.dead||a.player)return false;if(a.effect||a.projectile)return true;
        int mode=a.def?a.def->noDieBlink:0;
        if(mode==3){int off=a.def?std::max(80,a.def->offscreenKill):120;return a.x<cameraX_-off||a.x>cameraX_+renderer_.logicalWidth()+off;}
        if(!a.deathAnimationFinished)return false;
        if(airborneDead.contains(a.id))return false;
        if(mode==2)return true;
        float finishedAt=a.deathFinishedAt>=0?a.deathFinishedAt:a.deathTime;
        return a.deathTime-finishedAt>.65f;
    }),actors_.end());
    for(auto&d:drops)createActor(std::get<0>(d),std::get<1>(d),std::get<2>(d),false,false,false,0,{});
}


void Game::startSceneQueue(const std::vector<std::string>&files,Mode after,int nextStage){sceneFiles_=files;sceneFileIndex_=sceneStep_=0;sceneStepTime_=0;sceneX_=sceneY_=0;sceneSkip_=sceneNoSkip_=false;sceneNextStage_=nextStage;sceneVisual_.clear();sceneAfterMode_=after;mode_=Mode::Scene;loadNextSceneFile();}
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
}

void Game::update(float dt){dt=std::min(dt,.05f);totalTime_+=dt;uiFade_=std::min(1.f,uiFade_+dt*4.5f);input_.update();audio_.update();if(shakeTime_>0)shakeTime_=std::max(0.f,shakeTime_-dt);if(flashTime_>0)flashTime_=std::max(0.f,flashTime_-dt);for(auto&a:actors_){a.invincibleTime=std::max(0.f,a.invincibleTime-dt);a.hitInvincibleTime=std::max(0.f,a.hitInvincibleTime-dt);a.combatPauseTime=std::max(0.f,a.combatPauseTime-dt);if(mode_==Mode::Playing&&a.dead)a.deathTime+=dt;}auto in0=input_.uiState();
    if(mode_==Mode::Scene){updateScene(dt);return;}
    if(mode_==Mode::StageComplete){updateStageComplete(dt);return;}
    if(mode_==Mode::Title){updateTitle(in0);return;}
    if(mode_==Mode::Options){updateOptions(in0);return;}
    if(mode_==Mode::Graphics){updateGraphics(in0);return;}
    if(mode_==Mode::Controls){updateControls(in0);return;}
    if(mode_==Mode::Pause){updatePause(in0);return;}
    if(mode_==Mode::Loading){loadingTime_=std::max(0.f,loadingTime_-dt);if(loadingTime_<=0)mode_=Mode::Playing;return;}
    if(mode_==Mode::Select){
        std::array<bool,Input::MaxPlayers> joinedNow{};
        const bool p1BackFromReady=slots_[0].ready&&input_.state(0).backPressed;
        for(size_t i=1;i<slots_.size()&&i<(size_t)std::max(1,db_.campaign().maxPlayers);++i){
            auto in=input_.state(i);
            if(!slots_[i].joined&&in.startPressed){slots_[i].joined=true;slots_[i].ready=false;slots_[i].selected=nextSelectableCharacter(i,1);slots_[i].paletteMap=0;joinedNow[i]=true;audio_.playSfx("sounds/beep.wav");}
        }
        for(size_t i=0;i<slots_.size();++i)if(slots_[i].joined){
            if(joinedNow[i])continue;
            auto in=input_.state(i);
            if(slots_[i].ready){
                if(in.backPressed){slots_[i].ready=false;audio_.playSfx("sounds/beep.wav");}
                continue;
            }
            if(in.leftPressed){slots_[i].selected=nextSelectableCharacter(i,-1);slots_[i].paletteMap=0;audio_.playSfx("sounds/beep.wav");}
            if(in.rightPressed){slots_[i].selected=nextSelectableCharacter(i,1);slots_[i].paletteMap=0;audio_.playSfx("sounds/beep.wav");}
            if(db_.rules().colourSelect&&(in.upPressed||in.downPressed)){auto d=db_.entity(players_[(size_t)slots_[i].selected]);int count=d?(int)d->remaps.size():0;if(count>0){int dir=in.downPressed?1:-1;slots_[i].paletteMap=(slots_[i].paletteMap+dir+count+1)%(count+1);audio_.playSfx("sounds/beep.wav",.55f);}}
            if(in.startPressed||in.attackPressed){slots_[i].ready=true;audio_.playSfx("sounds/beep.wav",.7f);}
            else if(i>0&&in.backPressed){slots_[i].joined=false;slots_[i].ready=false;audio_.playSfx("sounds/beep.wav");}
        }
        bool any=false,allReady=true;for(const auto& s:slots_)if(s.joined){any=true;if(!s.ready)allReady=false;}
        if(any&&allReady){startGame();return;}
        if(in0.backPressed&&!p1BackFromReady&&!slots_[0].ready)enterTitle();return;}
    if(mode_==Mode::Continue){continueTimer_-=dt;bool go=false;for(size_t i=0;i<slots_.size();++i)if(slots_[i].joined&&input_.state(i).startPressed)go=true;if(go&&credits_>0){--credits_;for(size_t i=0;i<slots_.size();++i)if(slots_[i].joined){if(db_.campaign().continueScore==1){slots_[i].score=0;slots_[i].nextLifeScore=std::max(1,db_.rules().lifeScore);slots_[i].nextCreditScore=std::max(1,db_.rules().creditScore);}else if(db_.campaign().continueScore==2){++slots_[i].score;}if(slots_[i].lives<=0){slots_[i].lives=db_.campaign().lives;respawnPlayer(i,true);}}continueTimer_=10.f;mode_=Mode::Playing;saveProgress();return;}if(continueTimer_<=0||credits_<=0){mode_=Mode::GameOver;audio_.playMusic("music/gameover.bor",false);}return;}
    if(mode_==Mode::GameOver){if(in0.startPressed||in0.attackPressed)enterTitle();return;}if(mode_==Mode::Finished){if(in0.startPressed||in0.attackPressed)enterTitle();return;}if(input_.state(0).backPressed){pauseIndex_=0;mode_=Mode::Pause;audio_.pauseMusic(true);return;}
    for(size_t i=1;i<slots_.size()&&i<(size_t)db_.campaign().maxPlayers;++i)if(!slots_[i].joined&&input_.state(i).startPressed&&credits_>0){slots_[i].joined=true;slots_[i].selected=nextSelectableCharacter(i,1);slots_[i].lives=db_.campaign().lives;slots_[i].score=0;slots_[i].nextLifeScore=std::max(1,db_.rules().lifeScore);slots_[i].nextCreditScore=std::max(1,db_.rules().creditScore);--credits_;respawnPlayer(i,true);}
    if(!level_.noTime&&stageTime_>0){float before=stageTime_;stageTime_=std::max(0.f,stageTime_-dt);if(before>0&&stageTime_<=0){audio_.playSfx("sounds/timeover.wav",1.f);for(size_t i=0;i<slots_.size();++i)if(auto p=player(i)){if(!p->dead){p->hp=0;p->dead=true;breakGrabLinks(*p);if(animation(*p,"fall"))setAnim(*p,"fall");}}}}
    updateLevelRules();for(size_t i=0;i<slots_.size();++i)if(auto p=player(i))if(!p->dead)updatePlayer(*p,input_.state(i),dt);for(auto&a:actors_)if(!a.player&&!a.projectile&&!a.effect&&(a.team==Team::Enemy||a.team==Team::Ally))updateEnemy(a,dt);for(auto&a:actors_)if(a.projectile)updateProjectile(a,dt);for(auto&a:actors_)if(!a.projectile)updatePhysics(a,dt);for(auto&a:actors_)advanceAnim(a,dt);processPendingSpawns();
    for(size_t i=0;i<slots_.size();++i){auto p=player(i);if(!p||p->dead)continue;for(auto&a:actors_){if(a.dead||a.player||a.projectile||a.effect||!a.def||a.def->type!="item")continue;if(std::abs(a.x-p->x)<24&&std::abs(a.z-p->z)<18){if(a.def->subtype=="weapon")equipWeapon(*p,*a.def);else{if(a.def->reload>0&&p->def&&p->def->typeShot&&p->weaponUses>0&&p->weaponUses<999){int cap=p->def->shootNum>0?p->def->shootNum:p->weaponUses+a.def->reload;p->weaponUses=std::min(cap,p->weaponUses+a.def->reload);}if(a.def->health>0)p->hp=std::min(p->maxHp,p->hp+a.def->health);if(a.def->makeInv!=0){p->invincibleTime=(float)std::abs(a.def->makeInv);p->invincibleNoBlink=a.def->makeInvNoBlink;}registerScore((int)i,a.def->score);}a.dead=true;a.deathTime=1.f;a.deathAnimationFinished=true;a.deathFinishedAt=a.deathTime;audio_.playSfx("sounds/get.wav",.75f);}}}
    updateCombat();processPendingSpawns();processBossDefeats();
    for(size_t i=0;i<slots_.size();++i){auto&s=slots_[i];if(!s.joined)continue;auto p=player(i);if(p&&p->dead){s.respawnTimer+=dt;if(s.respawnTimer>1.8f){if(s.lives>1){--s.lives;respawnPlayer(i,true);saveProgress();}else{s.lives=0;int old=s.actorId;s.actorId=0;actors_.erase(std::remove_if(actors_.begin(),actors_.end(),[&](const Actor&a){return a.id==old;}),actors_.end());s.respawnTimer=0;saveProgress();}}}else if(p)s.respawnTimer=0;}
    if(!anyPlayerAlive()&&allPlayersOut()){if(credits_>0){mode_=Mode::Continue;continueTimer_=10.f;audio_.playMusic("music/gameover.bor",false);}else{mode_=Mode::GameOver;audio_.playMusic("music/gameover.bor",false);}cleanupDead();return;}
    auto lead=leadPlayer();if(!lead){cleanupDead();return;}
    if(level_.bgSpeed!=0){float sign=level_.bgSpeedDirection?1.f:-1.f;bgScrollX_+=sign*std::abs(level_.bgSpeed)*.6f*dt;}
    if(isVerticalScroll()){float sign=lower(level_.direction)=="up"?-1.f:1.f;verticalStageScroll_+=sign*30.f*dt;cameraX_=0;}
    else{
        bool reverse=isReverseScroll();float viewW=renderer_.logicalWidth();float maxCam=std::max(0.f,level_.worldWidth-viewW);float oldCameraX=cameraX_;
        float desired=reverse?clampf(lead->x-200.f,0,maxCam):clampf(lead->x-120.f,0,maxCam);
        float desiredProg=reverse?maxCam-desired:desired;float prog=cameraProgress();
        auto pendingAt=[&](float gate){return nextSpawn_<level_.spawns.size()&&level_.spawns[nextSpawn_].trigger<=gate+5.f;};
        if(nextWait_<level_.waits.size()){
            float gate=level_.waits[nextWait_];
            bool holdForPending=pendingAt(gate)&&prog>=gate-1.f;
            if(desiredProg>=gate-5.f&&(enemiesAlive()||holdForPending))desiredProg=std::min(desiredProg,gate);
        }
        if(isBidirectionalScroll())desiredProg=std::max(desiredProg,backscrollFloor_);
        else{furthestProgress_=std::max(furthestProgress_,desiredProg);desiredProg=furthestProgress_;}
        desired=cameraForProgress(desiredProg);cameraX_+=clampf(desired-cameraX_,-120.f*dt,120.f*dt);cameraX_=std::round(cameraX_);
        float screenDelta=cameraX_-oldCameraX;if(std::abs(screenDelta)>.001f)for(auto&actor:actors_)if(actor.subjectToScreen&&!actor.player&&!actor.dead)actor.x+=screenDelta;
    }
    for(size_t i=0;i<slots_.size();++i)if(auto p=player(i)){
        float lo=isVerticalScroll()?8.f:cameraX_+8.f;float hi=isVerticalScroll()?std::max(8.f,level_.worldWidth-15.f):movementBoundary();
        p->x=clampf(p->x,lo,hi);
    }
    advanceLevelProgress(dt);
    updateLevelRules();float prog=scrollProgress();
    while(nextSpawn_<level_.spawns.size()&&level_.spawns[nextSpawn_].trigger<=prog+5.f){
        auto&s0=level_.spawns[nextSpawn_];auto d0=db_.entity(s0.model);bool enemy=d0&&d0->type=="enemy";
        if(enemy&&!canSpawnGroupedEnemy())break;
        spawnActor(s0);++nextSpawn_;
    }
    cleanupDead();
    if((bossClearTriggered_||stageSequenceComplete())&&!enemiesAlive()){
        clearTimer_+=dt;
        if(clearTimer_>1.2f){
            const auto& st=db_.campaign().stages[(size_t)stageIndex_];
            if(st.showCompleteAfter&&!db_.campaign().noShowComplete)startStageComplete();else advanceAfterStage();
        }
    }else clearTimer_=0;
    savePulse_+=dt;if(savePulse_>5.f){savePulse_=0;saveProgress();}

}

void Game::drawBackground(float shakeX,float shakeY){
    if(level_.background.empty())return;auto im=renderer_.image(level_.background);if(!im||im->w<=0||im->h<=0)return;
    if(isVerticalScroll()){
        float start=std::fmod(verticalStageScroll_,im->h);while(start>0)start-=im->h;while(start<=-im->h)start+=im->h;
        for(float y=start;y<240.f;y+=im->h)renderer_.drawImage(level_.background,shakeX,y+shakeY);
        return;
    }
    float viewW=renderer_.logicalWidth();float maxScroll=std::max(1.f,level_.worldWidth-viewW),imgScroll=std::max(0.f,im->w-viewW);
    float panelWidth=0.f;std::vector<int> order=level_.panelOrder;if(order.empty())for(size_t pi=0;pi<level_.panels.size();++pi)order.push_back((int)pi);for(int pi:order)if(pi>=0&&pi<(int)level_.panels.size())if(auto pim=renderer_.image(level_.panels[(size_t)pi]))panelWidth+=pim->w;
    bool sameStageCanvas=panelWidth>0.f&&std::abs(panelWidth-im->w)<2.f;
    float base=(sameStageCanvas?-std::min(cameraX_,imgScroll):-(cameraX_/maxScroll)*imgScroll)+shakeX;
    if(level_.bgSpeed!=0){
        float start=base+std::fmod(bgScrollX_,im->w);while(start>0)start-=im->w;while(start<=-im->w)start+=im->w;
        for(float x=start;x<viewW;x+=im->w)renderer_.drawImage(level_.background,x,shakeY);
    }else renderer_.drawImage(level_.background,base,shakeY);
}
void Game::drawActor(const Actor&a,float shakeX,float shakeY){
    if(a.invincibleTime>0&&!a.invincibleNoBlink&&((int)(totalTime_*18)&1))return;
    if(a.dead&&a.def){
        int mode=a.def->noDieBlink;bool blink=false;
        if(mode==0)blink=((int)(a.deathTime*18.f)&1)!=0;
        else if(mode==1&&a.deathAnimationFinished){float t=a.deathFinishedAt>=0?a.deathTime-a.deathFinishedAt:a.deathTime;blink=((int)(t*18.f)&1)!=0;}
        if(blink)return;
    }
    auto f=frame(a);if(!f||f->image.empty())return;auto im=renderer_.image(f->image);if(!im)return;
    float originX=a.x-cameraX_+shakeX;float sx=spriteLeftFromOrigin(originX,f->offsetX,im->w,a.facingLeft),sy=a.z-a.a-f->offsetY+shakeY;
    if(a.paletteMap>0&&a.def&&(size_t)a.paletteMap<=a.def->remaps.size()){auto&r=a.def->remaps[(size_t)a.paletteMap-1];renderer_.drawImageRemapped(f->image,r.first,r.second,sx,sy,a.facingLeft);}else renderer_.drawImage(f->image,sx,sy,a.facingLeft);
}
void Game::drawPanels(float sx,float sy){if(level_.panels.empty())return;std::vector<int> ord=level_.panelOrder;if(ord.empty())for(size_t i=0;i<level_.panels.size();++i)ord.push_back((int)i);if(isReverseScroll())std::reverse(ord.begin(),ord.end());float x=0;for(int idx:ord){if(idx<0||idx>=(int)level_.panels.size())continue;auto&path=level_.panels[(size_t)idx];renderer_.drawImage(path,x-cameraX_+sx,sy);auto im=renderer_.image(path);x+=im?im->w:320.f;}}

void Game::drawHud(){
    const auto& hud=db_.campaign().hud;
    const int bw=std::max(1,hud.lifeBarWidth),bh=std::max(1,hud.lifeBarHeight);
    auto color=[](const HudColor& c){return D2D1::ColorF(c.r/255.f,c.g/255.f,c.b/255.f);};
    auto visible=[&](const HudPoint& p){return p.x>=0&&p.y>=0&&p.x<(int)renderer_.logicalWidth()&&p.y<240;};
    auto colorForAbsolute=[&](int value){
        for(const auto& [limit,c]:hud.lifePalette.colors)if(value<=limit)return color(c);
        return color(hud.lifePalette.whitebox);
    };
    auto colorForRatio=[&](float ratio){
        int key=ratio<=.20f?25:ratio<=.40f?50:ratio<=.60f?100:ratio<=.80f?200:300;
        auto it=hud.lifePalette.colors.find(key);return it==hud.lifePalette.colors.end()?D2D1::ColorF(1,1,1):color(it->second);
    };
    auto meter=[&](const HudPoint& pos,int hp,int maxHp){
        if(!visible(pos))return;
        const float x=(float)pos.x,y=(float)pos.y,w=(float)bw,h=(float)bh;
        if(!hud.lifeBarNoBorder){
            renderer_.fillRect(x-2,y-2,w+4,h+4,color(hud.lifePalette.blackbox),1.f);
            renderer_.fillRect(x-1,y-1,w+2,h+2,color(hud.lifePalette.whitebox),1.f);
        }
        auto bgIt=hud.lifePalette.colors.find(500);
        renderer_.fillRect(x,y,w,h,bgIt==hud.lifePalette.colors.end()?D2D1::ColorF(.15f,.15f,.15f):color(bgIt->second),.45f);
        hp=std::max(0,hp);maxHp=std::max(1,maxHp);
        if(hud.lifeBarType==1){
            float ratio=clampf((float)hp/(float)maxHp,0.f,1.f);
            float fill=hud.lifeBarOrientation?ratio*h:ratio*w;
            if(fill>0){
                auto c=colorForRatio(ratio);
                if(hud.lifeBarOrientation)renderer_.fillRect(x,y+h-fill,w,fill,c,1.f);
                else renderer_.fillRect(x,y,fill,h,c,1.f);
            }
            return;
        }
        // Legacy absolute mode: 1 HP = 1 pixel and values above width wrap/overlay.
        int remaining=hp,layer=0;
        while(remaining>0&&layer<64){
            int segment=std::min(bw,remaining);
            int represented=std::min(hp,(layer+1)*bw);
            auto c=colorForAbsolute(represented);
            if(hud.lifeBarOrientation){float fill=(float)std::min(bh,segment);renderer_.fillRect(x,y+h-fill,w,fill,c,1.f);}
            else renderer_.fillRect(x,y,(float)segment,h,c,1.f);
            remaining-=segment;++layer;
        }
    };
    auto hudText=[&](const std::wstring& text,const HudPoint& p,float size=6.f){if(visible(p)&&!text.empty())renderer_.text(text,(float)p.x,(float)p.y,size,D2D1::ColorF(1,1,1));};

    for(size_t i=0;i<slots_.size();++i){
        const auto& h=hud.players[i];
        if(!slots_[i].joined){
            if(i>0&&i<(size_t)std::max(1,db_.campaign().maxPlayers)&&credits_>0){
                hudText(L"PRESS START",h.join.prompt,5.5f);
            }
            continue;
        }
        auto p=player(i);
        if(p&&p->def&&!p->def->icon.empty()&&visible(h.icon))renderer_.drawImage(p->def->icon,(float)h.icon.x,(float)h.icon.y);
        if(p)meter(h.life,p->hp,p->maxHp);
        hudText(L"x",h.lifeX,6.f);
        hudText(std::to_wstring(std::max(0,slots_[i].lives)),h.lives,6.f);
        if(p&&p->def)hudText(w(p->def->name),h.score.name,6.f);
        hudText(L"-",h.score.dash,6.f);
        hudText(std::to_wstring(slots_[i].score),h.score.score,6.f);

        const Actor* target=slots_[i].hudTargetId?actorById(slots_[i].hudTargetId):nullptr;
        if(target&&target->def&&!target->def->noLife){
            meter(h.enemyLife,target->hp,target->maxHp);
            if(!target->def->icon.empty()&&visible(h.enemyIcon))renderer_.drawImage(target->def->icon,(float)h.enemyIcon.x,(float)h.enemyIcon.y);
            hudText(w(target->displayName.empty()?target->def->name:target->displayName),h.enemyName,5.5f);
        }
    }
    if(!level_.noTime){
        if(!hud.timeIcon.empty())renderer_.drawImage(hud.timeIcon,(float)hud.timeIconX,(float)hud.timeIconY);
        HudPoint time{hud.timeLoc[0],hud.timeLoc[1]};hudText(std::to_wstring((int)std::ceil(stageTime_)),time,7.f);
    }
}void Game::drawGameplay(){
    float sx=shakeTime_>0?std::sin(totalTime_*86.f)*shakeAmpX_:0,sy=shakeTime_>0?std::cos(totalTime_*71.f)*shakeAmpY_:0;
    drawBackground(sx,sy);drawPanels(sx,sy);
    std::vector<const Actor*>order;for(auto&a:actors_)order.push_back(&a);
    auto drawDepth=[this](const Actor*a){
        float z=a->z;
        if(a->grabTarget){bool behind=a->def&&a->def->grabBack!=0;z+=behind?-.25f:.25f;}
        else if(a->grabbedBy){auto g=actorById(a->grabbedBy);if(g){bool grabberBehind=g->def&&g->def->grabBack!=0;z+=grabberBehind?.25f:-.25f;}}
        return z;
    };
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return drawDepth(a)<drawDepth(b);});
    for(auto*a:order)drawActor(*a,sx,sy);
    if(!level_.frontPanel.empty())renderer_.drawImage(level_.frontPanel,-cameraX_+sx,sy);
    drawHud();if(flashTime_>0)renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(D2D1::ColorF::White),clampf(flashTime_*12.f,0,.65f));
}

void Game::drawSelect(){
    // Dynamic character roulette. Do not draw bgs/select.gif here: that original
    // OpenBOR artwork already contains fixed Guy/Cody/Haggar portraits and made
    // the face appear frozen while only the selected name changed.
    renderer_.fillScreen(D2D1::ColorF(.015f,.012f,.018f),1.f);
    renderer_.fillRect(0,0,renderer_.logicalWidth(),34,D2D1::ColorF(.16f,.025f,.015f),.96f);
    renderer_.fillRect(0,32,renderer_.logicalWidth(),2,D2D1::ColorF(1.f,.58f,.06f),.92f);
    renderer_.text(L"SELECAO DE PERSONAGEM",0,10,12,D2D1::ColorF(1,.82f,.18f),true);
    renderer_.text(L"ESCOLHA SEU LUTADOR",0,26,5.2f,D2D1::ColorF(.72f,.72f,.72f),true);

    auto defAt=[&](int idx)->const EntityDef*{
        if(players_.empty())return nullptr;
        int n=(int)players_.size();idx=(idx%n+n)%n;
        return db_.entity(players_[(size_t)idx]);
    };
    float pulse=.5f+.5f*std::sin(totalTime_*5.f);
    for(size_t i=0;i<3;++i){
        float x=8.f+(float)i*104.f;
        auto accent=i==0?D2D1::ColorF(1.f,.66f,.08f):(i==1?D2D1::ColorF(.12f,.78f,1.f):D2D1::ColorF(.82f,.28f,1.f));
        renderer_.fillRect(x,42,96,159,D2D1::ColorF(.025f,.025f,.035f),slots_[i].joined?.96f:.58f);
        renderer_.fillRect(x,42,96,2,accent,slots_[i].joined?.75f+.25f*pulse:.35f);
        if(slots_[i].joined){
            int sel=slots_[i].selected;
            auto d=defAt(sel);auto prev=defAt(sel-1);auto next=defAt(sel+1);
            renderer_.text(L"P"+std::to_wstring(i+1),x+7,48,7,accent);

            // Roulette strip: previous <- current -> next. The current face is
            // always resolved from slots_[i].selected on every frame.
            if(prev&&!prev->icon.empty())renderer_.drawImageScaled(prev->icon,x+8,65,20,20,false,.42f);
            if(next&&!next->icon.empty())renderer_.drawImageScaled(next->icon,x+68,65,20,20,false,.42f);
            renderer_.fillRect(x+31,59,34,34,D2D1::ColorF(0,0,0),.95f);
            renderer_.fillRect(x+30,58,36,36,accent,.90f);
            if(d&&!d->icon.empty())renderer_.drawImageScaled(d->icon,x+33,61,30,30);
            renderer_.text(L"<",x+3,70,8,accent);renderer_.text(L">",x+86,70,8,accent);

            // OpenBOR p#smenu uses sprite-origin coordinates and WAITING in preference to IDLE.
            if(d){
                auto it=d->animations.find("waiting");if(it==d->animations.end())it=d->animations.find("idle");
                if(it!=d->animations.end()&&!it->second.frames.empty()){
                    const auto& fr=it->second.frames[0];
                    bool previewLeft=i<db_.rules().spDirection.size()&&db_.rules().spDirection[i]==0;
                    const auto& sm=db_.campaign().hud.players[i].selectMenu;
                    if(sm.configured&&sm.character.x>=0&&sm.character.y>=0&&sm.character.x<(int)renderer_.logicalWidth()&&sm.character.y<240){
                        auto im=renderer_.image(fr.image);
                        if(im){float sx=spriteLeftFromOrigin((float)sm.character.x,fr.offsetX,im->w,previewLeft),sy=(float)sm.character.y-fr.offsetY;
                            if(slots_[i].paletteMap>0&&(size_t)slots_[i].paletteMap<=d->remaps.size()){auto&r=d->remaps[(size_t)slots_[i].paletteMap-1];renderer_.drawImageRemapped(fr.image,r.first,r.second,sx,sy,previewLeft);}
                            else renderer_.drawImage(fr.image,sx,sy,previewLeft);
                        }
                    }else renderer_.drawImageScaled(fr.image,x+15,96,66,62,previewLeft);
                }
            }
            renderer_.fillRect(x+5,163,86,29,D2D1::ColorF(.08f,.02f,.015f),.94f);
            std::wstring nm=d?w(d->name):w(players_[(size_t)sel]);
            renderer_.text(nm,x+10,167,8.5f,D2D1::ColorF(1,1,1));
            if(db_.rules().colourSelect)renderer_.text(L"COR "+std::to_wstring(slots_[i].paletteMap+1),x+10,181,5.5f,accent);
            const auto& hc=db_.campaign().hud.players[i];
            auto visiblePoint=[&](const HudPoint& p){return p.x>=0&&p.y>=0&&p.x<(int)renderer_.logicalWidth()&&p.y<240;};
            if(!slots_[i].ready&&visiblePoint(hc.join.select))renderer_.text(L"SELECT HERO",(float)hc.join.select.x,(float)hc.join.select.y,5.2f,D2D1::ColorF(1,1,1));
            if(visiblePoint(hc.join.name))renderer_.text(nm,(float)hc.join.name.x,(float)hc.join.name.y,5.6f,D2D1::ColorF(1,1,1));
            if(slots_[i].ready&&hc.selectMenu.configured&&visiblePoint(hc.selectMenu.ready))renderer_.text(L"READY!",(float)hc.selectMenu.ready.x,(float)hc.selectMenu.ready.y,6.f,accent);
        }else{
            renderer_.text(L"P"+std::to_wstring(i+1),x+7,49,7,D2D1::ColorF(.5f,.5f,.5f));
            renderer_.text(L"START",x+4,112,9,D2D1::ColorF(.65f,.65f,.65f),true);
            const auto& join=db_.campaign().hud.players[i].join;
            if(join.prompt.x>=0&&join.prompt.y>=0&&join.prompt.x<(int)renderer_.logicalWidth()&&join.prompt.y<240)renderer_.text(L"PRESS START",(float)join.prompt.x,(float)join.prompt.y,5.2f,D2D1::ColorF(.72f,.72f,.72f));
        }
    }
    renderer_.fillRect(0,207,renderer_.logicalWidth(),33,D2D1::ColorF(0,0,0),.90f);
    renderer_.text(L"START/ATAQUE: CONFIRMA   VOLTAR: CANCELA",0,211,6.2f,D2D1::ColorF(.95f,.95f,.95f),true);
    renderer_.text(db_.rules().colourSelect?L"ESQ/DIR: RODA PERSONAGEM   CIMA/BAIXO: COR   ESC: VOLTA":L"ESQ/DIR: RODA PERSONAGEM   ESC: VOLTA",0,224,5.3f,D2D1::ColorF(.74f,.74f,.74f),true);
    drawUiFade();
}
void Game::drawStageComplete(){
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

void Game::drawLoading(){
    const auto& hud=db_.campaign().hud;size_t which=stageIndex_==0?0:1;const auto& b=hud.loading[which];
    std::string bg=which==0?"bgs/loading.gif":"bgs/loading2.gif";renderer_.drawImageCoverScreen(bg,.78f);renderer_.drawImage(bg,0,0);
    float progress=clampf(1.f-loadingTime_/.45f,0.f,1.f);if(b.mode==1||b.mode==3){renderer_.fillRect((float)b.bx,(float)b.by,(float)b.bsize,5,D2D1::ColorF(.12f,.12f,.12f),.9f);renderer_.fillRect((float)b.bx,(float)b.by,b.bsize*progress,5,D2D1::ColorF(.9f,.2f,.08f),.95f);}if(b.tx<renderer_.logicalWidth()&&b.ty<240)renderer_.text(L"LOADING...",(float)b.tx,(float)b.ty,7,D2D1::ColorF(D2D1::ColorF::White));
}
void Game::render(){
    bool gpuFrame=(mode_==Mode::Playing||mode_==Mode::Pause||mode_==Mode::Loading||mode_==Mode::Continue||mode_==Mode::GameOver||mode_==Mode::Finished||mode_==Mode::Scene||mode_==Mode::StageComplete);if(!renderer_.begin(gpuFrame))return;renderer_.clear();
    if(mode_==Mode::Scene){if(!sceneVisual_.empty())renderer_.drawAnimatedImage(sceneVisual_,sceneStepTime_,(float)sceneX_,(float)sceneY_,false);renderer_.end();return;}
    if(mode_==Mode::StageComplete){drawStageComplete();renderer_.end();return;}
    if(mode_==Mode::Title){drawMainMenu();renderer_.end();return;}
    if(mode_==Mode::Options){drawOptions();renderer_.end();return;}
    if(mode_==Mode::Graphics){drawGraphics();renderer_.end();return;}
    if(mode_==Mode::Controls){drawControls();renderer_.end();return;}
    if(mode_==Mode::Select){drawSelect();renderer_.end();return;}
    if(mode_==Mode::Continue){renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(D2D1::ColorF::Black));renderer_.text(L"CONTINUE?",0,78,24,D2D1::ColorF(1,.8f,.1f),true);renderer_.text(std::to_wstring((int)std::ceil(continueTimer_)),0,113,26,D2D1::ColorF(D2D1::ColorF::White),true);renderer_.text(L"START - CREDITOS "+std::to_wstring(credits_),0,154,9,D2D1::ColorF(D2D1::ColorF::White),true);renderer_.end();return;}
    if(mode_==Mode::GameOver){renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(D2D1::ColorF::Black));renderer_.text(L"GAME OVER",0,92,24,D2D1::ColorF(1,.12f,.08f),true);renderer_.text(L"HI "+std::to_wstring(highScore_),0,124,8,D2D1::ColorF(1,.82f,.18f),true);renderer_.text(L"ENTER PARA VOLTAR",0,144,10,D2D1::ColorF(D2D1::ColorF::White),true);renderer_.end();return;}
    if(mode_==Mode::Finished){renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(D2D1::ColorF::Black));renderer_.text(L"FIM",0,82,28,D2D1::ColorF(1,.85f,.2f),true);int total=0;for(auto&s:slots_)total+=s.score;renderer_.text(L"PONTOS "+std::to_wstring(total),0,126,10,D2D1::ColorF(D2D1::ColorF::White),true);renderer_.text(L"HI "+std::to_wstring(highScore_),0,148,8,D2D1::ColorF(1,.82f,.18f),true);renderer_.text(L"ENTER PARA VOLTAR",0,166,9,D2D1::ColorF(D2D1::ColorF::White),true);renderer_.end();return;}
    drawGameplay();if(mode_==Mode::Pause)drawPauseOverlay();renderer_.end();
}
}
#endif
