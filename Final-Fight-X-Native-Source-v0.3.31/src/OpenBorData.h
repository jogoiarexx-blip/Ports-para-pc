#pragma once
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ffx {
struct RectF { float x{},y{},w{},h{}; bool valid=false; };
struct AttackBox {
    RectF rect;
    int damage=0, knockdown=0, pause=0, noBlock=0;
    float zDepth=13.f; // OpenBOR legacy default: grabdistance(36)/3 + 1.
    bool noFlash=false;
    int landingDamage=0, landingMode=0;
    bool customDrop=false;
    float dropY=0, dropX=0, dropZ=0;
    std::string kind;
};
struct PlatformBox {
    float x=0,z=0,lowerLeft=0,upperLeft=0,lowerRight=0,upperRight=0,depth=0,alt=0;
    bool valid=false;
};
struct FrameEvent {
    int frame=-1;
    std::string kind;
    std::vector<std::string> args;
};
struct AnimFrame {
    std::string image;
    int delay=10;
    float offsetX=0, offsetY=0;
    RectF bbox;
    AttackBox attack;
    float moveX=0, moveZ=0, moveA=0;
    float setA=-1;
    std::string sound, hitfx, hitflash;
    std::vector<std::string> commands;
    PlatformBox platform;
};
struct Animation {
    std::string name;
    bool loop=false;
    float rangeMin=0.f, rangeMax=72.f;
    int energyCost=0;
    int followAnim=-1, followCond=-1, throwFrameWait=-1;
    bool attackOne=false, fastAttack=false;
    std::string projectileModel;
    std::vector<FrameEvent> events;
    std::vector<AnimFrame> frames;
};
struct ScriptFunctionDef {
    std::string name;
    std::string playerNameEquals;
    std::string clearGlobal;
    bool killSelf=false;
    bool subjectToScreenOnGlobal=false;
    std::string subjectGlobal;
    int subjectGlobalValue=0;
    int subjectToScreenValue=0;
};
struct EntityDef {
    std::string name, type="enemy", subtype, icon, deathSound, bombModel, animationScript, basePalette;
    std::string defaultFlash, shotModel;
    int health=100;
    float speed=6.f;
    int shadow=0, weaponNumber=0, counter=0, shootNum=0, typeShot=0, reload=0, score=0, remove=1;
    int throwFrameWait=-1, throwDamage=21;
    float throwDist=2.f, throwHeight=4.f;
    int noDieBlink=0;
    int makeInv=0, offscreenKill=1000, thold=0;
    int antigrab=0, grabFinish=0, grabBack=0, grabForce=0, aggression=50, blockOdds=0;
    bool noLife=false, noAtFlash=false, noQuake=false, bounce=false, makeInvNoBlink=false, toFlip=false;
    float defenseAll=1.f;
    std::string aiMove, blockFlash;
    std::vector<std::string> weapons, canDamage, hostile, modelLoads;
    std::vector<std::pair<std::string,std::string>> remaps;
    std::vector<int> attackChain;
    std::unordered_map<std::string,Animation> animations;
    std::unordered_map<std::string,std::string> commands;
    std::unordered_map<std::string,ScriptFunctionDef> animationFunctions;
    std::unordered_map<std::string,std::vector<std::string>> raw;
};
struct SpawnDef {
    std::string model, alias, item;
    float x=0,z=200,a=0;
    bool flip=false, boss=false;
    int healthOverride=0, map=0;
    std::string weapon;
    float trigger=0;
};
struct LevelGroup { float trigger=0; int min=0,max=0; };
struct LevelBlockade { float trigger=0, position=0; };
struct LevelWall {
    float x=0,z=0;
    float upperLeft=0,lowerLeft=0,upperRight=0,lowerRight=0;
    float depth=0,height=0;
};
struct LevelDef {
    std::string file, background, frontPanel, music, direction="right";
    std::vector<std::string> panels;
    std::vector<int> panelOrder;
    std::vector<SpawnDef> spawns;
    std::vector<LevelBlockade> blockades;
    std::vector<float> waits;
    std::vector<LevelGroup> groups;
    std::vector<LevelWall> walls;
    float worldWidth=320, maxTrigger=0, zMin=170,zMax=240, bgSpeed=0;
    int bgSpeedDirection=0;
    int blocked=0, setTime=100;
    bool noTime=false,noResetTime=false;
};
struct HudPoint { int x=0,y=0; };
struct HudScoreLayout {
    HudPoint name{}, dash{}, score{};
    int font=0;
};
struct HudJoinLayout {
    HudPoint name{}, select{}, prompt{};
    int font=0;
};
struct HudSelectMenuLayout {
    HudPoint character{}, ready{};
    bool configured=false;
};
struct HudColor { int r=0,g=0,b=0; };
struct HudLifePalette {
    HudColor blackbox{0,0,0}, whitebox{238,238,238};
    std::map<int,HudColor> colors{{25,{255,0,0}},{50,{255,128,0}},{100,{255,255,0}},{200,{187,255,0}},{300,{119,255,119}},{400,{0,255,187}},{500,{255,255,255}}};
};
struct HudPlayerLayout {
    HudPoint life{}, icon{}, lives{}, lifeX{}, enemyLife{}, enemyIcon{}, enemyName{};
    int lifeXFont=0;
    HudScoreLayout score{};
    HudJoinLayout join{};
    HudSelectMenuLayout selectMenu{};
};
struct LoadingBarConfig {
    int mode=0,bx=0,by=0,bsize=0,tx=1000,ty=1000,font=0,refreshMs=100;
};
struct HudConfig {
    std::array<HudPlayerLayout,4> players{};
    int lifeBarWidth=62,lifeBarHeight=3,lifeBarNoBorder=0,lifeBarType=0,lifeBarOrientation=0;
    HudLifePalette lifePalette{};
    std::array<int,6> timeLoc{{192,20,180,320,1,-1}};
    std::string timeIcon; int timeIconX=190,timeIconY=5;
    bool highScoreBackground=false;
    std::array<LoadingBarConfig,2> loading{};
};
struct StageEntry {
    std::string file;
    float zMin=170,zMax=240;
    int chapter=0;
    bool showCompleteAfter=false;
    std::vector<std::string> scenesAfter;
};
struct SceneStep {
    enum class Kind{Animation,Music,Silence};
    Kind kind=Kind::Animation;
    std::string asset;
    bool loop=false;
    int x=0,y=0;
    bool skip=false,noskip=false;
};
struct SceneDef { std::string file; std::vector<SceneStep> steps; };
struct Campaign {
    std::vector<StageEntry> stages;
    std::vector<std::string> introScenes;
    std::vector<std::string> endScenes;
    std::array<int,4> scoreBonuses{{10000,1000,100,0}};
    int maxPlayers=3, lives=5, credits=6, continueScore=0, canSave=0;
    bool noSame=false, noShowComplete=false, showRushBonus=false;
    HudConfig hud;
};
struct ModelEntry { std::string name,path; bool preload=false; };
struct GlobalRules {
    int lifeScore=30000, creditScore=999999;
    int autoland=0, noAirCancel=0;
    bool noCost=false, noLost=false, colourSelect=false, versusDamage=false;
    std::array<int,4> spDirection{{1,0,1,0}};
};

class OpenBorDatabase {
public:
    bool load(const std::filesystem::path& dataRoot, std::string* error=nullptr);
    const EntityDef* entity(const std::string& name) const;
    const Campaign& campaign() const { return campaign_; }
    const GlobalRules& rules() const { return rules_; }
    const std::vector<ModelEntry>& models() const { return models_; }
    const std::filesystem::path& root() const { return root_; }
    LevelDef loadLevel(const StageEntry& entry) const;
    SceneDef loadScene(const std::string& file) const;
private:
    std::filesystem::path root_;
    Campaign campaign_;
    GlobalRules rules_;
    std::vector<ModelEntry> models_;
    std::unordered_map<std::string,EntityDef> entities_;
    static EntityDef parseEntity(const std::filesystem::path& p);
    static Campaign parseCampaign(const std::filesystem::path& p);
    static std::vector<ModelEntry> parseModels(const std::filesystem::path& p,GlobalRules* rules);
};
}
