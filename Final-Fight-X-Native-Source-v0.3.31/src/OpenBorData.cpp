#include "OpenBorData.h"
#include "TextUtil.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <regex>
#include <sstream>

namespace ffx {
static RectF rectFrom(const std::vector<std::string>& t,size_t i){
    RectF r; if(t.size()>=i+4){r.x=toFloat(t[i]);r.y=toFloat(t[i+1]);r.w=toFloat(t[i+2]);r.h=toFloat(t[i+3]);r.valid=r.w>0&&r.h>0;} return r;
}
static void pushEvent(Animation* a,const std::string& kind,int frame,const std::vector<std::string>& args={}){
    if(!a) return;
    FrameEvent e; e.kind=kind;e.frame=std::max(0,frame);e.args=args;a->events.push_back(std::move(e));
}

static std::unordered_map<std::string,ScriptFunctionDef> parseAnimationScriptFunctions(const std::filesystem::path& p){
    std::unordered_map<std::string,ScriptFunctionDef> out;
    if(!std::filesystem::exists(p))return out;
    std::string src;
    for(auto line:readLines(p)){
        auto c=line.find("//");if(c!=std::string::npos)line=line.substr(0,c);
        src+=line;src+='\n';
    }
    std::regex fnRe(R"(void\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^\)]*\)\s*\{)",std::regex::icase);
    for(std::sregex_iterator it(src.begin(),src.end(),fnRe),end;it!=end;++it){
        auto m=*it;size_t open=src.find('{',(size_t)m.position());if(open==std::string::npos)continue;
        int depth=0;size_t close=open;
        for(size_t i=open;i<src.size();++i){if(src[i]=='{')++depth;else if(src[i]=='}'&&--depth==0){close=i;break;}}
        if(close<=open)continue;
        ScriptFunctionDef f;f.name=lower(m[1].str());std::string body=src.substr(open+1,close-open-1);
        std::smatch sm;
        std::regex playerRe(R"rx(getentityproperty\s*\(\s*p\s*,\s*"name"\s*\)\s*==\s*"([^"]+)")rx",std::regex::icase);
        if(std::regex_search(body,sm,playerRe))f.playerNameEquals=lower(sm[1].str());
        std::regex killRe(R"rx(killentity\s*\(\s*getlocalvar\s*\(\s*"self"\s*\)\s*\))rx",std::regex::icase);
        f.killSelf=std::regex_search(body,killRe);
        std::regex clearRe(R"rx(setglobalvar\s*\(\s*"([^"]+)"\s*,\s*NULL\s*\(\s*\)\s*\))rx",std::regex::icase);
        if(std::regex_search(body,sm,clearRe))f.clearGlobal=lower(sm[1].str());
        std::regex condRe(R"rx(getglobalvar\s*\(\s*"([^"]+)"\s*\)\s*==\s*(-?\d+))rx",std::regex::icase);
        std::smatch cond;
        std::regex screenRe(R"rx(changeentityproperty\s*\(\s*getlocalvar\s*\(\s*"self"\s*\)\s*,\s*"subject_to_screen"\s*,\s*(-?\d+)\s*\))rx",std::regex::icase);
        std::smatch screen;
        if(std::regex_search(body,cond,condRe)&&std::regex_search(body,screen,screenRe)){
            f.subjectToScreenOnGlobal=true;f.subjectGlobal=lower(cond[1].str());f.subjectGlobalValue=toInt(cond[2].str());f.subjectToScreenValue=toInt(screen[1].str());
        }
        out[f.name]=std::move(f);
    }
    return out;
}

EntityDef OpenBorDatabase::parseEntity(const std::filesystem::path& p){
    EntityDef e; Animation* cur=nullptr; int delay=10; float ox=0,oy=0; RectF bbox{}; AttackBox atk{}; PlatformBox platform{}; float mx=0,mz=0,ma=0,seta=-1; std::string snd,hitfx,hitflash; std::vector<std::string> pendingCommands;
    for(auto rawLine: readLines(p)){
        auto hash=rawLine.find('#'); if(hash!=std::string::npos) rawLine=rawLine.substr(0,hash);
        auto s=trim(rawLine); if(s.empty()) continue; auto t=splitWs(s); if(t.empty()) continue; auto cmd=lower(t[0]);
        if(cmd=="anim" && t.size()>1){
            Animation a; a.name=lower(t[1]); auto [it,_]=e.animations.emplace(a.name,std::move(a)); cur=&it->second;
            delay=10;ox=oy=0;bbox={};atk={};platform={};mx=mz=ma=0;seta=-1;snd.clear();hitfx.clear();hitflash.clear();pendingCommands.clear(); continue;
        }
        if(cmd=="grabback"&&t.size()>1){e.grabBack=toInt(t[1]);continue;}
        if(cmd=="grabfinish"&&t.size()>1){e.grabFinish=toInt(t[1]);continue;}
        if(cmd=="throwframewait"&&t.size()>1){e.throwFrameWait=toInt(t[1]);continue;}
        if(cmd=="throwdamage"&&t.size()>1){e.throwDamage=toInt(t[1],21);continue;}
        if(cmd=="throw"&&t.size()>2){e.throwDist=toFloat(t[1],2.f);e.throwHeight=toFloat(t[2],4.f);continue;}
        if(!cur){
            if(cmd=="name"&&t.size()>1)e.name=lower(t[1]);
            else if(cmd=="type"&&t.size()>1)e.type=lower(t[1]);
            else if(cmd=="subtype"&&t.size()>1)e.subtype=lower(t[1]);
            else if(cmd=="health"&&t.size()>1)e.health=toInt(t[1],100);
            else if(cmd=="speed"&&t.size()>1)e.speed=toFloat(t[1],6);
            else if(cmd=="shadow"&&t.size()>1)e.shadow=toInt(t[1]);
            else if(cmd=="icon"&&t.size()>1)e.icon=normalizeAsset(t[1]);
            else if(cmd=="diesound"&&t.size()>1)e.deathSound=normalizeAsset(t[1]);
            else if(cmd=="bomb"&&t.size()>1)e.bombModel=lower(t[1]);
            else if(cmd=="flash"&&t.size()>1)e.defaultFlash=lower(t[1]);
            else if(cmd=="playshotno"&&t.size()>1)e.shotModel=lower(t[1]);
            else if(cmd=="animationscript"&&t.size()>1)e.animationScript=normalizeAsset(t[1]);
            else if(cmd=="palette"&&t.size()>1)e.basePalette=normalizeAsset(t[1]);
            else if(cmd=="remap"&&t.size()>2)e.remaps.push_back({normalizeAsset(t[1]),normalizeAsset(t[2])});
            else if(cmd=="weapnum"&&t.size()>1)e.weaponNumber=toInt(t[1]);
            else if(cmd=="counter"&&t.size()>1)e.counter=toInt(t[1]);
            else if(cmd=="shootnum"&&t.size()>1)e.shootNum=toInt(t[1]);
            else if(cmd=="typeshot"&&t.size()>1)e.typeShot=toInt(t[1]);
            else if(cmd=="reload"&&t.size()>1)e.reload=toInt(t[1]);
            else if(cmd=="score"&&t.size()>1)e.score=toInt(t[1]);
            else if(cmd=="remove"&&t.size()>1){auto v=lower(t[1]);e.remove=v=="none"?0:v=="hit"?1:toInt(t[1],1);}
            else if(cmd=="nodieblink"&&t.size()>1)e.noDieBlink=std::clamp(toInt(t[1]),0,3);
            else if(cmd=="toflip"&&t.size()>1)e.toFlip=toInt(t[1])!=0;
            else if(cmd=="makeinv"&&t.size()>1){e.makeInv=toInt(t[1]);e.makeInvNoBlink=t.size()>2&&toInt(t[2])!=0;}
            else if(cmd=="noquake"&&t.size()>1)e.noQuake=toInt(t[1])!=0;
            else if(cmd=="offscreenkill"&&t.size()>1)e.offscreenKill=toInt(t[1],1000);
            else if(cmd=="bounce"&&t.size()>1)e.bounce=toInt(t[1])!=0;
            else if(cmd=="aimove"&&t.size()>1)e.aiMove=lower(t[1]);
            else if(cmd=="thold"&&t.size()>1)e.thold=toInt(t[1]);
            else if(cmd=="bflash"&&t.size()>1)e.blockFlash=lower(t[1]);
            else if(cmd=="defense"&&t.size()>1&&lower(t[1])=="all")e.defenseAll=t.size()>2?toFloat(t[2],0.f):0.f;
            else if(cmd=="com"&&t.size()>2)e.commands[lower(t[1])]=lower(t[2]);
            else if(cmd=="nolife"&&t.size()>1)e.noLife=toInt(t[1])!=0;
            else if(cmd=="noatflash"&&t.size()>1)e.noAtFlash=toInt(t[1])!=0;
            else if(cmd=="antigrab"&&t.size()>1)e.antigrab=toInt(t[1]);
            else if(cmd=="grabfinish"&&t.size()>1)e.grabFinish=toInt(t[1]);
            else if(cmd=="grabback"&&t.size()>1)e.grabBack=toInt(t[1]);
            else if(cmd=="grabforce"&&t.size()>1)e.grabForce=toInt(t[1]);
            else if(cmd=="aggression"&&t.size()>1)e.aggression=toInt(t[1],50);
            else if(cmd=="blockodds"&&t.size()>1)e.blockOdds=toInt(t[1]);
            else if(cmd=="atchain") for(size_t i=1;i<t.size();++i){int v=toInt(t[i]);if(v>0)e.attackChain.push_back(v);}
            else if(cmd=="weapons") for(size_t i=1;i<t.size();++i)e.weapons.push_back(lower(t[i]));
            else if(cmd=="candamage") for(size_t i=1;i<t.size();++i)e.canDamage.push_back(lower(t[i]));
            else if(cmd=="hostile") for(size_t i=1;i<t.size();++i)e.hostile.push_back(lower(t[i]));
            else if(cmd=="load"&&t.size()>1)e.modelLoads.push_back(lower(t[1]));
            e.raw[cmd]=std::vector<std::string>(t.begin()+1,t.end());
            continue;
        }
        if(cmd=="loop"&&t.size()>1){cur->loop=toInt(t[1])!=0;continue;}
        if(cmd=="fastattack"&&t.size()>1){cur->fastAttack=toInt(t[1])!=0;continue;}
        if(cmd=="attackone"&&t.size()>1){cur->attackOne=toInt(t[1])!=0;continue;}
        if(cmd=="range"&&t.size()>1){cur->rangeMin=toFloat(t[1]);cur->rangeMax=t.size()>2?toFloat(t[2]):cur->rangeMin+40.f;continue;}
        if(cmd=="energycost"&&t.size()>1){cur->energyCost=toInt(t[1]);continue;}
        if(cmd=="followanim"&&t.size()>1){cur->followAnim=toInt(t[1]);continue;}
        if(cmd=="followcond"&&t.size()>1){cur->followCond=toInt(t[1]);continue;}
        if(cmd=="custknife"&&t.size()>1){cur->projectileModel=lower(t[1]);continue;}
        if(cmd=="jumpframe"&&t.size()>2){pushEvent(cur,"jump",toInt(t[1]),std::vector<std::string>(t.begin()+2,t.end()));continue;}
        if(cmd=="landframe"&&t.size()>1){pushEvent(cur,"land",std::max(0,toInt(t[1])-1),std::vector<std::string>(t.begin()+2,t.end()));continue;}
        if(cmd=="throwframe"&&t.size()>1){pushEvent(cur,"throw",toInt(t[1]),std::vector<std::string>(t.begin()+2,t.end()));continue;}
        if(cmd=="tossframe"&&t.size()>1){pushEvent(cur,"toss",toInt(t[1]),std::vector<std::string>(t.begin()+2,t.end()));continue;}
        if(cmd=="shootframe"&&t.size()>1){pushEvent(cur,"shoot",toInt(t[1]),std::vector<std::string>(t.begin()+2,t.end()));continue;}
        if(cmd=="flipframe"){pushEvent(cur,"flip",t.size()>1?toInt(t[1]):0);continue;}
        if(cmd=="quakeframe"&&t.size()>1){
            int start=toInt(t[1]),repeat=t.size()>2?std::max(1,toInt(t[2],1)):1;
            std::string vertical=t.size()>3?t[3]:"3";
            for(int q=0;q<repeat;++q)pushEvent(cur,"quake",start+q,{vertical});
            continue;
        }
        if(cmd=="damageonlanding"){atk.landingDamage=t.size()>1?toInt(t[1]):0;atk.landingMode=t.size()>2?toInt(t[2]):0;continue;}
        if(cmd=="dropv"&&t.size()>1){atk.customDrop=true;atk.dropY=toFloat(t[1]);atk.dropX=t.size()>2?toFloat(t[2]):0;atk.dropZ=t.size()>3?toFloat(t[3]):0;continue;}
        if(cmd=="@cmd"){pendingCommands.push_back(restAfterFirst(s));continue;}
        if(cmd=="delay"&&t.size()>1){delay=std::max(1,toInt(t[1],10));continue;}
        if(cmd=="offset"&&t.size()>2){ox=toFloat(t[1]);oy=toFloat(t[2]);continue;}
        if(cmd=="bbox"){bbox=rectFrom(t,1);continue;}
        if(cmd.rfind("attack",0)==0 || cmd=="shock" || cmd=="burn"){
            // OpenBOR keeps modifiers such as damageonlanding until the next frame,
            // while each attack command refreshes the core attack geometry/flags.
            const int landingDamage=atk.landingDamage;
            const int landingMode=atk.landingMode;
            atk={};
            atk.landingDamage=landingDamage;
            atk.landingMode=landingMode;
            atk.kind=cmd;
            atk.rect=rectFrom(t,1);
            if(t.size()>5)atk.damage=toInt(t[5]);
            if(t.size()>6)atk.knockdown=toInt(t[6]);
            if(t.size()>7)atk.noBlock=toInt(t[7]);
            if(t.size()>8)atk.noFlash=toInt(t[8])!=0;
            if(t.size()>9)atk.pause=toInt(t[9]);
            if(t.size()>10){float z=std::abs(toFloat(t[10]));atk.zDepth=z>0.f?z:13.f;}
            continue;
        }
        if(cmd=="move"&&t.size()>1){mx=toFloat(t[1]);continue;}
        if(cmd=="movez"&&t.size()>1){mz=toFloat(t[1]);continue;}
        if(cmd=="movea"&&t.size()>1){ma=toFloat(t[1]);continue;}
        if(cmd=="seta"&&t.size()>1){seta=toFloat(t[1]);continue;}
        if(cmd=="sound"&&t.size()>1){snd=normalizeAsset(t[1]);continue;}
        if(cmd=="hitfx"&&t.size()>1){hitfx=normalizeAsset(t[1]);continue;}
        if(cmd=="hitflash"&&t.size()>1){hitflash=lower(t[1]);continue;}
        if(cmd=="platform"){
            platform={};
            if(t.size()>=9){platform.x=toFloat(t[1]);platform.z=toFloat(t[2]);platform.lowerLeft=toFloat(t[3]);platform.upperLeft=toFloat(t[4]);platform.lowerRight=toFloat(t[5]);platform.upperRight=toFloat(t[6]);platform.depth=std::abs(toFloat(t[7]));platform.alt=toFloat(t[8]);platform.valid=true;}
            else if(t.size()>=7){platform.x=ox;platform.z=oy;platform.upperLeft=toFloat(t[1]);platform.lowerLeft=toFloat(t[2]);platform.upperRight=toFloat(t[3]);platform.lowerRight=toFloat(t[4]);platform.depth=std::abs(toFloat(t[5]));platform.alt=toFloat(t[6]);platform.valid=true;}
            continue;
        }
        if(cmd=="frame"&&t.size()>1){
            AnimFrame f; f.image=normalizeAsset(t[1]);f.delay=delay;f.offsetX=ox;f.offsetY=oy;f.bbox=bbox;f.attack=atk;f.moveX=mx;f.moveZ=mz;f.moveA=ma;f.setA=seta;f.sound=snd;f.hitfx=hitfx;f.hitflash=hitflash;f.commands=pendingCommands;f.platform=platform;
            cur->frames.push_back(std::move(f)); snd.clear(); pendingCommands.clear(); continue;
        }
    }
    if(e.name.empty()) e.name=lower(p.stem().string());
    return e;
}

std::vector<ModelEntry> OpenBorDatabase::parseModels(const std::filesystem::path& p,GlobalRules* rules){
    std::vector<ModelEntry> out;
    for(auto line: readLines(p)){
        auto h=line.find('#');if(h!=std::string::npos)line=line.substr(0,h);auto t=splitWs(trim(line));if(t.empty())continue;
        auto c=lower(t[0]);
        if((c=="load"||c=="know")&&t.size()>=3) out.push_back({lower(t[1]),normalizeAsset(t[2]),c=="load"});
        else if(rules&&c=="lifescore"&&t.size()>1)rules->lifeScore=toInt(t[1],30000);
        else if(rules&&c=="credscore"&&t.size()>1)rules->creditScore=toInt(t[1],999999);
        else if(rules&&c=="nocost"&&t.size()>1)rules->noCost=toInt(t[1])!=0;
        else if(rules&&c=="nolost"&&t.size()>1)rules->noLost=toInt(t[1])!=0;
        else if(rules&&c=="autoland"&&t.size()>1)rules->autoland=toInt(t[1]);
        else if(rules&&c=="noaircancel"&&t.size()>1)rules->noAirCancel=toInt(t[1]);
        else if(rules&&c=="colourselect"&&t.size()>1)rules->colourSelect=toInt(t[1])!=0;
        else if(rules&&c=="versusdamage"&&t.size()>1)rules->versusDamage=toInt(t[1])!=0;
        else if(rules&&c=="spdirection"&&t.size()>1){
            for(size_t i=0;i<rules->spDirection.size()&&i+1<t.size();++i)rules->spDirection[i]=toInt(t[i+1],rules->spDirection[i])!=0?1:0;
        }
    }return out;
}

Campaign OpenBorDatabase::parseCampaign(const std::filesystem::path& p){
    Campaign c; float zmin=170,zmax=240; int chapter=0,lastStage=-1; std::vector<std::string> pendingScenes;
    auto playerIndex=[](const std::string& cmd)->int{
        if(cmd.size()>1&&cmd[0]=='p'&&cmd[1]>='1'&&cmd[1]<='4')return cmd[1]-'1';
        if(cmd.size()>1&&cmd[0]=='e'&&cmd[1]>='1'&&cmd[1]<='4')return cmd[1]-'1';
        return -1;
    };
    auto setPoint=[](HudPoint& pt,const std::vector<std::string>& t){if(t.size()>1)pt.x=toInt(t[1]);if(t.size()>2)pt.y=toInt(t[2]);};
    for(auto line:readLines(p)){
        auto h=line.find('#');if(h!=std::string::npos)line=line.substr(0,h);auto t=splitWs(trim(line));if(t.empty())continue;auto cmd=lower(t[0]);
        if(cmd=="maxplayers"&&t.size()>1)c.maxPlayers=toInt(t[1],3);
        else if(cmd=="nosame"&&t.size()>1)c.noSame=toInt(t[1])!=0;
        else if(cmd=="cansave"&&t.size()>1)c.canSave=toInt(t[1]);
        else if(cmd=="lives"&&t.size()>1)c.lives=toInt(t[1],5);
        else if(cmd=="credits"&&t.size()>1)c.credits=toInt(t[1],6);
        else if(cmd=="continuescore"&&t.size()>1)c.continueScore=toInt(t[1]);
        else if(cmd=="noshowcomplete"&&t.size()>1)c.noShowComplete=toInt(t[1])!=0;
        else if(cmd=="showrushbonus"&&t.size()>1)c.showRushBonus=toInt(t[1])!=0;
        else if(cmd=="scbonuses"){for(size_t i=0;i<4&&i+1<t.size();++i)c.scoreBonuses[i]=toInt(t[i+1],c.scoreBonuses[i]);}
        else if(cmd=="timeloc")for(size_t i=0;i<6&&i+1<t.size();++i)c.hud.timeLoc[i]=toInt(t[i+1]);
        else if(cmd=="timeicon"&&t.size()>1){c.hud.timeIcon=normalizeAsset(t[1]);if(t.size()>2)c.hud.timeIconX=toInt(t[2]);if(t.size()>3)c.hud.timeIconY=toInt(t[3]);}
        else if(cmd=="hiscorebg")c.hud.highScoreBackground=t.size()<2||toInt(t[1])!=0;
        else if(cmd=="lbarsize"&&t.size()>1){
            c.hud.lifeBarWidth=toInt(t[1],62);
            if(t.size()>2)c.hud.lifeBarHeight=toInt(t[2],3);
            if(t.size()>3)c.hud.lifeBarNoBorder=toInt(t[3]);
            if(t.size()>4)c.hud.lifeBarType=toInt(t[4]);
            if(t.size()>5)c.hud.lifeBarOrientation=toInt(t[5]);
        }
        else if(cmd=="loadingbg"||cmd=="loadingbg2"){
            auto& b=c.hud.loading[cmd=="loadingbg"?0:1];
            if(t.size()>1)b.mode=toInt(t[1]);if(t.size()>2)b.bx=toInt(t[2]);if(t.size()>3)b.by=toInt(t[3]);if(t.size()>4)b.bsize=toInt(t[4]);
            if(t.size()>5)b.tx=toInt(t[5]);if(t.size()>6)b.ty=toInt(t[6]);if(t.size()>7)b.font=toInt(t[7]);if(t.size()>8)b.refreshMs=std::max(1,toInt(t[8],100));
        }
        else if(cmd.size()>2&&(cmd[0]=='p'||cmd[0]=='e')){
            int i=playerIndex(cmd); if(i<0||i>=4)continue; auto& hcfg=c.hud.players[(size_t)i];
            if(cmd=="p"+std::to_string(i+1)+"smenu"){
                if(t.size()>2){hcfg.selectMenu.character.x=toInt(t[1]);hcfg.selectMenu.character.y=toInt(t[2]);hcfg.selectMenu.configured=true;}
                if(t.size()>4){hcfg.selectMenu.ready.x=toInt(t[3]);hcfg.selectMenu.ready.y=toInt(t[4]);}
            }
            else if(cmd=="p"+std::to_string(i+1)+"life")setPoint(hcfg.life,t);
            else if(cmd=="p"+std::to_string(i+1)+"icon")setPoint(hcfg.icon,t);
            else if(cmd=="p"+std::to_string(i+1)+"lifen")setPoint(hcfg.lives,t);
            else if(cmd=="p"+std::to_string(i+1)+"lifex"){
                if(t.size()>2){hcfg.lifeX.x=toInt(t[1]);hcfg.lifeX.y=toInt(t[2]);}
                if(t.size()>3)hcfg.lifeXFont=toInt(t[3]);
            }
            else if(cmd=="p"+std::to_string(i+1)+"namej"){
                if(t.size()>2){hcfg.join.name.x=toInt(t[1]);hcfg.join.name.y=toInt(t[2]);}
                if(t.size()>4){hcfg.join.select.x=toInt(t[3]);hcfg.join.select.y=toInt(t[4]);}
                if(t.size()>6){hcfg.join.prompt.x=toInt(t[5]);hcfg.join.prompt.y=toInt(t[6]);}
                if(t.size()>7)hcfg.join.font=toInt(t[7]);
            }
            else if(cmd=="p"+std::to_string(i+1)+"score"){
                if(t.size()>2){hcfg.score.name.x=toInt(t[1]);hcfg.score.name.y=toInt(t[2]);}
                if(t.size()>4){hcfg.score.dash.x=toInt(t[3]);hcfg.score.dash.y=toInt(t[4]);}
                if(t.size()>6){hcfg.score.score.x=toInt(t[5]);hcfg.score.score.y=toInt(t[6]);}
                if(t.size()>7)hcfg.score.font=toInt(t[7]);
            }
            else if(cmd=="e"+std::to_string(i+1)+"life")setPoint(hcfg.enemyLife,t);
            else if(cmd=="e"+std::to_string(i+1)+"icon")setPoint(hcfg.enemyIcon,t);
            else if(cmd=="e"+std::to_string(i+1)+"name")setPoint(hcfg.enemyName,t);
        }
        else if(cmd=="z"&&t.size()>2){zmin=toFloat(t[1],170);zmax=toFloat(t[2],240);}
        else if(cmd=="file"&&t.size()>1){
            if(!pendingScenes.empty()){
                if(lastStage>=0)c.stages[(size_t)lastStage].scenesAfter.insert(c.stages[(size_t)lastStage].scenesAfter.end(),pendingScenes.begin(),pendingScenes.end());
                else c.introScenes.insert(c.introScenes.end(),pendingScenes.begin(),pendingScenes.end());
                pendingScenes.clear();
            }
            c.stages.push_back({normalizeAsset(t[1]),zmin,zmax,chapter});
            lastStage=(int)c.stages.size()-1;
        }
        else if(cmd=="next"){if(lastStage>=0)c.stages[(size_t)lastStage].showCompleteAfter=true;++chapter;}
        else if(cmd=="scene"&&t.size()>1)pendingScenes.push_back(normalizeAsset(t[1]));
    }
    if(!pendingScenes.empty())c.endScenes.insert(c.endScenes.end(),pendingScenes.begin(),pendingScenes.end());
    const auto lifeFile=p.parent_path()/"lifebar.txt";
    if(std::filesystem::exists(lifeFile))for(auto line:readLines(lifeFile)){
        auto h=line.find('#');if(h!=std::string::npos)line=line.substr(0,h);auto t=splitWs(trim(line));if(t.size()<4)continue;auto key=lower(t[0]);
        HudColor col{std::clamp(toInt(t[1]),0,255),std::clamp(toInt(t[2]),0,255),std::clamp(toInt(t[3]),0,255)};
        if(key=="blackbox")c.hud.lifePalette.blackbox=col;
        else if(key=="whitebox")c.hud.lifePalette.whitebox=col;
        else if(key.rfind("color",0)==0){try{int threshold=std::stoi(key.substr(5));c.hud.lifePalette.colors[threshold]=col;}catch(...){}}
    }
    return c;
}

bool OpenBorDatabase::load(const std::filesystem::path& dataRoot,std::string* error){
    root_=dataRoot; if(!std::filesystem::exists(root_/"models.txt")){if(error)*error="models.txt not found";return false;}
    rules_={};models_=parseModels(root_/"models.txt",&rules_); campaign_=parseCampaign(root_/"levels.txt"); entities_.clear();
    size_t ok=0;
    for(const auto&m:models_){auto p=root_/std::filesystem::path(m.path);if(std::filesystem::exists(p)){auto e=parseEntity(p);if(!e.animationScript.empty())e.animationFunctions=parseAnimationScriptFunctions(root_/std::filesystem::path(e.animationScript));entities_[lower(m.name)]=std::move(e);++ok;}}
    if(ok==0){if(error)*error="no entity definitions loaded";return false;} return true;
}
const EntityDef* OpenBorDatabase::entity(const std::string& name)const{auto it=entities_.find(lower(name));return it==entities_.end()?nullptr:&it->second;}

LevelDef OpenBorDatabase::loadLevel(const StageEntry& entry) const{
    LevelDef l; l.file=entry.file;l.zMin=entry.zMin;l.zMax=entry.zMax; auto p=root_/std::filesystem::path(entry.file);
    SpawnDef pending{}; bool havePending=false; float lastAt=0; std::vector<std::pair<std::string,std::vector<std::string>>> pendingLevelEvents;
    auto flushSpawn=[&](float trigger){if(havePending){pending.trigger=trigger;l.spawns.push_back(pending);pending={};havePending=false;}};
    auto flushEvents=[&](float trigger){for(auto&e:pendingLevelEvents){
        if(e.first=="wait")l.waits.push_back(trigger);
        else if(e.first=="group"&&e.second.size()>=2)l.groups.push_back({trigger,toInt(e.second[0]),toInt(e.second[1])});
        else if(e.first=="blockade"&&!e.second.empty())l.blockades.push_back({trigger,toFloat(e.second[0])});
    }pendingLevelEvents.clear();};
    for(auto line:readLines(p)){
        auto h=line.find('#');if(h!=std::string::npos)line=line.substr(0,h);auto t=splitWs(trim(line));if(t.empty())continue;auto cmd=lower(t[0]);
        if(cmd=="music"&&t.size()>1)l.music=normalizeAsset(t[1]);
        else if(cmd=="settime"&&t.size()>1)l.setTime=std::clamp(toInt(t[1],100),0,100);
        else if(cmd=="notime"&&t.size()>1)l.noTime=toInt(t[1])!=0;
        else if(cmd=="noreset"&&t.size()>1)l.noResetTime=toInt(t[1])!=0;
        else if(cmd=="background"&&t.size()>1)l.background=normalizeAsset(t[1]);
        else if(cmd=="panel"&&t.size()>1)l.panels.push_back(normalizeAsset(t[1]));
        else if(cmd=="frontpanel"&&t.size()>1)l.frontPanel=normalizeAsset(t[1]);
        else if(cmd=="order"&&t.size()>1){for(size_t k=1;k<t.size();++k)for(char ch:t[k])if(ch>='a'&&ch<='z')l.panelOrder.push_back((int)(ch-'a'));}
        else if(cmd=="direction"&&t.size()>1)l.direction=lower(t[1]);
        else if(cmd=="blocked"&&t.size()>1)l.blocked=toInt(t[1]);
        else if(cmd=="bgspeed"&&t.size()>1){l.bgSpeed=toFloat(t[1]);l.bgSpeedDirection=t.size()>2?(toInt(t[2])!=0?1:0):0;}
        else if(cmd=="spawn"&&t.size()>1){flushSpawn(lastAt);pending={};pending.model=lower(t[1]);havePending=true;}
        else if(cmd=="coords"&&havePending&&t.size()>2){pending.x=toFloat(t[1]);pending.z=toFloat(t[2]);if(t.size()>3)pending.a=toFloat(t[3]);}
        else if(cmd=="flip"&&havePending&&t.size()>1)pending.flip=toInt(t[1])!=0;
        else if(cmd=="alias"&&havePending&&t.size()>1)pending.alias=t[1];
        else if(cmd=="item"&&havePending&&t.size()>1)pending.item=lower(t[1]);
        else if(cmd=="weapon"&&havePending&&t.size()>1)pending.weapon=lower(t[1]);
        else if(cmd=="health"&&havePending&&t.size()>1)pending.healthOverride=toInt(t[1]);
        else if(cmd=="boss"&&havePending&&t.size()>1)pending.boss=toInt(t[1])!=0;
        else if(cmd=="map"&&t.size()>1&&havePending)pending.map=toInt(t[1]);
        else if(cmd=="wait")pendingLevelEvents.push_back({"wait",{}});
        else if(cmd=="group")pendingLevelEvents.push_back({"group",std::vector<std::string>(t.begin()+1,t.end())});
        else if(cmd=="blockade"&&t.size()>1)pendingLevelEvents.push_back({"blockade",{t[1]}});
        else if(cmd=="at"&&t.size()>1){float a=toFloat(t[1]);flushSpawn(a);flushEvents(a);lastAt=a;l.maxTrigger=std::max(l.maxTrigger,a);}
        else if(cmd=="wall"&&t.size()>1){float x=toFloat(t[1]);if(t.size()>=9){float z=toFloat(t[2]);float ul=toFloat(t[3]),ll=toFloat(t[4]),ur=toFloat(t[5]),lr=toFloat(t[6]);float dep=std::abs(toFloat(t[7]));float alt=toFloat(t[8]);l.walls.push_back({x,z,ul,ll,ur,lr,dep,alt});}}
    }
    flushSpawn(lastAt);flushEvents(lastAt);
    // OpenBOR level files are allowed to declare events out of numeric order.
    // Runtime spawning is progress-based, so normalize trigger order while preserving
    // author order for actors/events sharing the same trigger.
    std::stable_sort(l.spawns.begin(),l.spawns.end(),[](const auto&a,const auto&b){return a.trigger<b.trigger;});
    std::stable_sort(l.blockades.begin(),l.blockades.end(),[](const auto&a,const auto&b){return a.trigger<b.trigger;});
    std::sort(l.waits.begin(),l.waits.end());
    std::stable_sort(l.groups.begin(),l.groups.end(),[](const auto&a,const auto&b){return a.trigger<b.trigger;});
    return l;
}

SceneDef OpenBorDatabase::loadScene(const std::string& file) const{
    SceneDef s;s.file=normalizeAsset(file);auto p=root_/std::filesystem::path(s.file);
    for(auto line:readLines(p)){
        auto h=line.find('#');if(h!=std::string::npos)line=line.substr(0,h);auto t=splitWs(trim(line));if(t.empty())continue;auto cmd=lower(t[0]);
        if(cmd=="animation"&&t.size()>1){SceneStep st;st.kind=SceneStep::Kind::Animation;st.asset=normalizeAsset(t[1]);if(t.size()>2)st.x=toInt(t[2]);if(t.size()>3)st.y=toInt(t[3]);if(t.size()>4)st.skip=toInt(t[4])!=0;if(t.size()>5)st.noskip=toInt(t[5])!=0;s.steps.push_back(std::move(st));}
        else if(cmd=="music"&&t.size()>1){SceneStep st;st.kind=SceneStep::Kind::Music;st.asset=normalizeAsset(t[1]);st.loop=t.size()>2?toInt(t[2])!=0:false;s.steps.push_back(std::move(st));}
        else if(cmd=="silence"||(cmd=="stop"&&t.size()>1&&lower(t[1])=="music")){SceneStep st;st.kind=SceneStep::Kind::Silence;s.steps.push_back(std::move(st));}
    }
    return s;
}
}
