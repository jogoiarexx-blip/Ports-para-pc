#pragma once
#ifdef _WIN32
#include "Audio.h"
#include "Input.h"
#include "OpenBorData.h"
#include "OpenBorSemantics.h"
#include "Renderer.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ffx {
enum class Team { Neutral, Player, Enemy, Ally };

struct Actor {
    int id=0;
    const EntityDef* def=nullptr;
    std::string baseModel,displayName;
    float x=0,z=200,a=0,vx=0,vz=0,va=0;
    int hp=1,maxHp=1;
    bool facingLeft=false,player=false,dead=false,boss=false,effect=false,deathAnimationFinished=false;
    bool attackWindowActive=false,attackOneConsumed=false,grabReady=false;
    Team team=Team::Neutral;
    int playerIndex=-1;

    bool projectile=false,projectileExploding=false,projectileArc=false,removeOnHit=false;
    int projectileOwner=0;
    float projectileLife=0;

    int grabbedBy=0,grabTarget=0;
    int weaponNumber=0,weaponUses=0,paletteMap=0;
    bool unequipAfterAnim=false;

    std::string anim="idle";
    size_t frame=0;
    float frameTime=0,deathTime=0,deathFinishedAt=-1.f;
    std::string riseAnim;
    uint64_t attackSerial=0;
    std::unordered_set<int> hitTargets;

    float aiThink=0;
    int attackCursor=0;
    std::string dropItem;
    bool dropSpawned=false,scoreAwarded=false;
    int combo=0;
    float comboTimer=0;

    float invincibleTime=0,hitInvincibleTime=0,combatPauseTime=0;
    bool invincibleNoBlink=false;
    int landingDamage=0,landingMode=0;
    int pendingEnergyCost=0;
    int thrownBy=0;
    bool bounced=false,safeLandingRequested=false,subjectToScreen=false;
};

class Game {
public:
    bool init(HWND hwnd,const std::filesystem::path& exeDir);
    void shutdown();
    void update(float dt);
    void render();
    void resize(UINT w,UINT h);
    void toggleFullscreen();
private:
    enum class Mode{Scene,StageComplete,Title,Options,Graphics,Controls,Pause,Select,Loading,Playing,Continue,GameOver,Finished};
    struct PendingSpawn {
        std::string model,mode;
        float x=0,z=0,a=0,power=0;
        bool left=false,effect=false;
        Team team=Team::Neutral;
        int owner=0;
    };
    struct PlayerSlot {
        int actorId=0,selected=0,paletteMap=0,lives=0,score=0,nextLifeScore=30000,nextCreditScore=999999,hudTargetId=0;
        bool joined=false,ready=false;
        float respawnTimer=0;
    };

    Mode mode_=Mode::Title,sceneAfterMode_=Mode::Title;
    Renderer renderer_;
    Audio audio_;
    Input input_;
    OpenBorDatabase db_;
    std::filesystem::path exeDir_,dataRoot_,saveFile_,settingsFile_;
    HWND hwnd_{};
    WINDOWPLACEMENT windowedPlacement_{sizeof(WINDOWPLACEMENT)};
    LONG_PTR windowedStyle_=0;
    bool haveWindowedPlacement_=false,fullscreen_=false;
    int musicVolume_=75,sfxVolume_=85,windowScale_=3;
    int graphicsPreset_=1,upscaleMode_=2,filterMode_=0,filterStrength_=55,graphicsIndex_=0;
    bool integerScale_=true,widescreen_=false,vsync_=true;
    float uiFade_=1.f;
    int menuIndex_=0,optionsIndex_=0,pauseIndex_=0;
    int controlsPlayer_=0,controlsIndex_=0,controlsActionIndex_=0;
    enum class ControlsPage{Root,Keyboard,Gamepad};
    ControlsPage controlsPage_=ControlsPage::Root;
    bool bindingCapture_=false;
    Mode optionsReturnMode_=Mode::Title,controlsReturnMode_=Mode::Options;

    std::vector<Actor> actors_;
    std::unordered_map<std::string,int> globalActorVars_;
    std::unordered_map<std::string,int> globalIntVars_;
    std::vector<PendingSpawn> pendingSpawns_;
    int nextId_=1,playerId_=0;
    std::array<PlayerSlot,Input::MaxPlayers> slots_{};
    std::vector<std::string> players_{"cody","guy","haggar"};

    int stageIndex_=0;
    LevelDef level_;
    float cameraX_=0,levelProgress_=0,maxLevelProgress_=0,furthestProgress_=0,backscrollFloor_=0,bgScrollX_=0,verticalStageScroll_=0;
    size_t nextSpawn_=0,nextWait_=0,nextGroup_=0,nextBlockade_=0;
    int groupMin_=0,groupMax_=100;
    int bossesDeclared_=0,bossesRemaining_=0;
    bool groupRefillLocked_=false,bossClearTriggered_=false;
    float clearTimer_=0,stageTime_=100.f,loadingTime_=0.f;
    int highScore_=0;
    float completeTick_=0,completeIdle_=0;
    int completeStageNumber_=0,completeBeepCounter_=0;
    std::array<int,Input::MaxPlayers> completeClear_{},completeLife_{};

    int credits_=6;
    float continueTimer_=10.f;

    float totalTime_=0,shakeTime_=0,shakeAmpX_=0,shakeAmpY_=0,flashTime_=0,savePulse_=0;

    std::vector<std::string> sceneFiles_;
    size_t sceneFileIndex_=0,sceneStep_=0;
    SceneDef scene_;
    std::string sceneVisual_;
    float sceneStepTime_=0;
    int sceneX_=0,sceneY_=0;
    bool sceneSkip_=false,sceneNoSkip_=false;
    int sceneNextStage_=-1;

    Actor* player();
    const Actor* player() const;
    Actor* player(size_t index);
    const Actor* player(size_t index) const;
    Actor* actorById(int id);
    const Actor* actorById(int id) const;
    Actor* nearestOpponent(Actor& a);
    Actor* leadPlayer();
    const Actor* leadPlayer() const;
    int activePlayerCount() const;
    bool anyPlayerAlive() const;
    bool allPlayersOut() const;

    void resetSelection();
    void loadSettings();
    void saveSettings() const;
    void applyAudioSettings();
    void applyGraphicsSettings();
    void applyGraphicsPreset(int preset);
    void setFullscreen(bool enabled);
    void applyWindowScale();
    void beginUiTransition();
    void enterTitle();
    void updateTitle(const InputState& in);
    void updateOptions(const InputState& in);
    void updateGraphics(const InputState& in);
    void updatePause(const InputState& in);
    void updateControls(const InputState& in);
    void drawMainMenu();
    void drawOptions();
    void drawGraphics();
    void drawPauseOverlay();
    void drawControls();
    void drawSelect();
    void drawUiFade();
    void drawGameplay();
    void drawLoading();
    int nextSelectableCharacter(size_t slot,int direction) const;
    void startGame();
    bool loadSave();
    void saveProgress();
    bool hasSave() const;
    void loadStage(int idx,bool preserveState=true);
    void respawnPlayer(size_t index,bool fullHealth=true);
    void spawnActor(const SpawnDef& s);
    Actor* createActor(const std::string& model,float x,float z,bool left,bool isPlayer=false,bool boss=false,int hpOverride=0,const std::string& dropItem={},Team forcedTeam=Team::Neutral,int playerIndex=-1);
    void queueEffect(const std::string& model,float x,float z,float a=0,bool sourceFacingLeft=false);
    void queueProjectile(const Actor& owner,const std::string& model,const std::string& mode,float height,float power=0.f);
    void processPendingSpawns();

    const Animation* animation(const Actor&a,const std::string& n) const;
    const AnimFrame* frame(const Actor&a) const;
    void setAnim(Actor&a,const std::string& n,bool restart=true);
    void enterFrame(Actor&a);
    void executeFrameEvents(Actor&a,const Animation& an);
    void executeFrameCommands(Actor&a,const AnimFrame& f);
    void advanceAnim(Actor&a,float dt);
    std::string commandAnimation(const Actor&a,const std::string& key,const std::string& fallback) const;
    std::string reactionAnimation(const Actor&a,const std::string& attackKind,ReactionKind reaction) const;

    void updatePlayer(Actor&a,const InputState& in,float dt);
    void updateEnemy(Actor&a,float dt);
    void updateProjectile(Actor&a,float dt);
    void updatePhysics(Actor&a,float dt);
    void resolveWalls(Actor&a,float oldX,float oldZ);
    float platformFloor(const Actor&a) const;
    void handleLanding(Actor&a,float floorHeight=0);
    void updateGrabBinding(Actor&a);
    bool beginGrab(Actor& grabber,Actor& target);
    void releaseGrab(Actor& grabber,bool thrown=false,int damage=0,float vx=0,float va=0,const std::string& fallAnim={});
    void breakGrabLinks(Actor&a);

    bool equipWeapon(Actor& p,const EntityDef& item);
    void unequipWeapon(Actor& p);
    void registerScore(int playerIndex,int amount);

    void updateCombat();
    bool isHostile(const Actor&att,const Actor&target) const;
    bool canDamage(const Actor&att,const Actor&target) const;
    void damage(Actor&target,Actor&attacker,int amount,int knockdown,const std::string& hitfx,const std::string& hitflash,const std::string& kind={},const AttackBox* attack=nullptr);
    RectF bodyRect(const Actor&a) const;
    RectF attackRect(const Actor&a) const;
    static bool intersects(const RectF&a,const RectF&b);

    bool enemiesAlive() const;
    int activeEnemies() const;
    bool canSpawnGroupedEnemy();
    bool isReverseScroll() const;
    bool isBidirectionalScroll() const;
    bool isVerticalScroll() const;
    float movementBoundary() const;
    float cameraProgress() const;
    float scrollProgress() const;
    float cameraForProgress(float progress) const;
    void advanceLevelProgress(float dt);
    void updateLevelRules();
    bool stageSequenceComplete() const;
    void processBossDefeats();
    void cleanupDead();

    void startSceneQueue(const std::vector<std::string>& files,Mode after,int nextStage=-1);
    void startStageComplete();
    void updateStageComplete(float dt);
    void finishStageComplete(bool awardRemainder=true);
    void advanceAfterStage();
    void drawStageComplete();
    void loadNextSceneFile();
    void advanceSceneStep();
    void updateScene(float dt);

    void drawBackground(float shakeX,float shakeY);
    void drawActor(const Actor&a,float shakeX,float shakeY);
    void drawHud();
    void drawPanels(float shakeX,float shakeY);
};
}
#endif
