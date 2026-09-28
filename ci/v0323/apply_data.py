from pathlib import Path

def req_replace(path, old, new, count=1):
    p=Path(path); s=p.read_text()
    if old not in s: raise SystemExit(f'pattern missing in {path}: {old[:80]!r}')
    p.write_text(s.replace(old,new,count))

p=Path('src/OpenBorData.h'); s=p.read_text()
old='''struct HudPoint { int x=0,y=0; };
struct HudPlayerLayout {
    HudPoint life{}, icon{}, lives{}, score{}, enemyLife{}, enemyIcon{}, enemyName{};
};'''
new='''struct HudPoint { int x=0,y=0; };
struct HudScoreLayout {
    HudPoint name{}, dash{}, score{};
    int font=0;
};
struct HudColor { int r=0,g=0,b=0; };
struct HudLifePalette {
    HudColor blackbox{0,0,0}, whitebox{238,238,238};
    std::map<int,HudColor> colors{{25,{255,0,0}},{50,{255,128,0}},{100,{255,255,0}},{200,{187,255,0}},{300,{119,255,119}},{400,{0,255,187}},{500,{255,255,255}}};
};
struct HudPlayerLayout {
    HudPoint life{}, icon{}, lives{}, enemyLife{}, enemyIcon{}, enemyName{};
    HudScoreLayout score{};
};'''
if old not in s: raise SystemExit('HUD structs missing')
s=s.replace(old,new,1)
s=s.replace('int lifeBarWidth=62,lifeBarHeight=3,lifeBarType=0;','int lifeBarWidth=62,lifeBarHeight=3,lifeBarNoBorder=0,lifeBarType=0,lifeBarOrientation=0;\n    HudLifePalette lifePalette{};',1)
s=s.replace('struct SceneStep { enum class Kind{Animation,Music,Silence}; Kind kind=Kind::Animation; std::string asset; bool loop=false; };','''struct SceneStep {
    enum class Kind{Animation,Music,Silence};
    Kind kind=Kind::Animation;
    std::string asset;
    bool loop=false;
    int x=0,y=0;
    bool skip=false,noskip=false;
};''',1)
p.write_text(s)

p=Path('src/OpenBorData.cpp'); s=p.read_text()
s=s.replace('if(cmd=="landframe"&&t.size()>1){pushEvent(cur,"land",toInt(t[1]),std::vector<std::string>(t.begin()+2,t.end()));continue;}','if(cmd=="landframe"&&t.size()>1){pushEvent(cur,"land",std::max(0,toInt(t[1])-1),std::vector<std::string>(t.begin()+2,t.end()));continue;}',1)
s=s.replace('else if(cmd=="lbarsize"&&t.size()>1){c.hud.lifeBarWidth=toInt(t[1],62);if(t.size()>2)c.hud.lifeBarHeight=toInt(t[2],3);if(t.size()>3)c.hud.lifeBarType=toInt(t[3]);}','''else if(cmd=="lbarsize"&&t.size()>1){
            c.hud.lifeBarWidth=toInt(t[1],62);
            if(t.size()>2)c.hud.lifeBarHeight=toInt(t[2],3);
            if(t.size()>3)c.hud.lifeBarNoBorder=toInt(t[3]);
            if(t.size()>4)c.hud.lifeBarType=toInt(t[4]);
            if(t.size()>5)c.hud.lifeBarOrientation=toInt(t[5]);
        }''',1)
s=s.replace('else if(cmd=="p"+std::to_string(i+1)+"score")setPoint(hcfg.score,t);','''else if(cmd=="p"+std::to_string(i+1)+"score"){
                if(t.size()>2){hcfg.score.name.x=toInt(t[1]);hcfg.score.name.y=toInt(t[2]);}
                if(t.size()>4){hcfg.score.dash.x=toInt(t[3]);hcfg.score.dash.y=toInt(t[4]);}
                if(t.size()>6){hcfg.score.score.x=toInt(t[5]);hcfg.score.score.y=toInt(t[6]);}
                if(t.size()>7)hcfg.score.font=toInt(t[7]);
            }''',1)
old='''        else if(cmd=="scene"&&t.size()>1)c.endScenes.push_back(normalizeAsset(t[1]));
    }return c;
}'''
new='''        else if(cmd=="scene"&&t.size()>1)c.endScenes.push_back(normalizeAsset(t[1]));
    }
    const auto lifeFile=p.parent_path()/"lifebar.txt";
    if(std::filesystem::exists(lifeFile))for(auto line:readLines(lifeFile)){
        auto h=line.find('#');if(h!=std::string::npos)line=line.substr(0,h);auto t=splitWs(trim(line));if(t.size()<4)continue;auto key=lower(t[0]);
        HudColor col{std::clamp(toInt(t[1]),0,255),std::clamp(toInt(t[2]),0,255),std::clamp(toInt(t[3]),0,255)};
        if(key=="blackbox")c.hud.lifePalette.blackbox=col;
        else if(key=="whitebox")c.hud.lifePalette.whitebox=col;
        else if(key.rfind("color",0)==0){try{int threshold=std::stoi(key.substr(5));c.hud.lifePalette.colors[threshold]=col;}catch(...){}}
    }
    return c;
}'''
if old not in s: raise SystemExit('campaign end missing')
s=s.replace(old,new,1)
old='''        if(cmd=="animation"&&t.size()>1)s.steps.push_back({SceneStep::Kind::Animation,normalizeAsset(t[1]),false});
        else if(cmd=="music"&&t.size()>1)s.steps.push_back({SceneStep::Kind::Music,normalizeAsset(t[1]),t.size()>2?toInt(t[2])!=0:false});
        else if(cmd=="silence"||(cmd=="stop"&&t.size()>1&&lower(t[1])=="music"))s.steps.push_back({SceneStep::Kind::Silence,{},false});'''
new='''        if(cmd=="animation"&&t.size()>1){SceneStep st;st.kind=SceneStep::Kind::Animation;st.asset=normalizeAsset(t[1]);if(t.size()>2)st.x=toInt(t[2]);if(t.size()>3)st.y=toInt(t[3]);if(t.size()>4)st.skip=toInt(t[4])!=0;if(t.size()>5)st.noskip=toInt(t[5])!=0;s.steps.push_back(std::move(st));}
        else if(cmd=="music"&&t.size()>1){SceneStep st;st.kind=SceneStep::Kind::Music;st.asset=normalizeAsset(t[1]);st.loop=t.size()>2?toInt(t[2])!=0:false;s.steps.push_back(std::move(st));}
        else if(cmd=="silence"||(cmd=="stop"&&t.size()>1&&lower(t[1])=="music")){SceneStep st;st.kind=SceneStep::Kind::Silence;s.steps.push_back(std::move(st));}'''
if old not in s: raise SystemExit('scene parser missing')
s=s.replace(old,new,1)
p.write_text(s)

for name in ['src/main_win.cpp','src/Game.cpp']:
    p=Path(name); s=p.read_text().replace('0.3.22','0.3.23'); p.write_text(s)
print('apply_v0323_data=OK')
