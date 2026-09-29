from pathlib import Path

def rep(s, old, new, label):
    if old not in s:
        raise SystemExit(f"missing {label}")
    return s.replace(old,new,1)

p=Path("src/OpenBorData.h")
s=p.read_text()
s=rep(s,
'int shadow=0, weaponNumber=0, counter=0, shootNum=0, typeShot=0, reload=0, score=0, remove=0;',
'''int shadow=0, weaponNumber=0, counter=0, shootNum=0, typeShot=0, reload=0, score=0, remove=1;
    int throwFrameWait=-1;''',"entity remove default")
s=rep(s,
'''struct HudScoreLayout {
    HudPoint name{}, dash{}, score{};
    int font=0;
};''',
'''struct HudScoreLayout {
    HudPoint name{}, dash{}, score{};
    int font=0;
};
struct HudJoinLayout {
    HudPoint name{}, select{}, prompt{};
    int font=0;
};''',"hud join layout")
s=rep(s,
'''struct HudPlayerLayout {
    HudPoint life{}, icon{}, lives{}, enemyLife{}, enemyIcon{}, enemyName{};
    HudScoreLayout score{};
};''',
'''struct HudPlayerLayout {
    HudPoint life{}, icon{}, lives{}, lifeX{}, enemyLife{}, enemyIcon{}, enemyName{};
    int lifeXFont=0;
    HudScoreLayout score{};
    HudJoinLayout join{};
};''',"hud player layout")
p.write_text(s)

p=Path("src/OpenBorData.cpp")
s=p.read_text()
s=rep(s,
'''        if(cmd=="grabback"&&t.size()>1){e.grabBack=toInt(t[1]);continue;}
        if(cmd=="grabfinish"&&t.size()>1){e.grabFinish=toInt(t[1]);continue;}''',
'''        if(cmd=="grabback"&&t.size()>1){e.grabBack=toInt(t[1]);continue;}
        if(cmd=="grabfinish"&&t.size()>1){e.grabFinish=toInt(t[1]);continue;}
        if(cmd=="throwframewait"&&t.size()>1){e.throwFrameWait=toInt(t[1]);continue;}''',"model throwframewait")
s=rep(s,'else if(cmd=="remove"&&t.size()>1)e.remove=toInt(t[1]);',
'''else if(cmd=="remove"&&t.size()>1){auto v=lower(t[1]);e.remove=v=="none"?0:v=="hit"?1:toInt(t[1],1);}''',"remove parser")
s=rep(s,
'if(cmd=="throwframe"&&t.size()>1){int tf=cur->throwFrameWait>=0?cur->throwFrameWait:toInt(t[1]);pushEvent(cur,"throw",tf,std::vector<std::string>(t.begin()+2,t.end()));continue;}',
'if(cmd=="throwframe"&&t.size()>1){pushEvent(cur,"throw",toInt(t[1]),std::vector<std::string>(t.begin()+2,t.end()));continue;}',
"throwframe independence")
s=rep(s,'        if(cmd=="throwframewait"&&t.size()>1){cur->throwFrameWait=toInt(t[1]);continue;}\n','',"remove animation throwframewait")
s=rep(s,
'''            else if(cmd=="p"+std::to_string(i+1)+"lifen")setPoint(hcfg.lives,t);
            else if(cmd=="p"+std::to_string(i+1)+"score"){''',
'''            else if(cmd=="p"+std::to_string(i+1)+"lifen")setPoint(hcfg.lives,t);
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
            else if(cmd=="p"+std::to_string(i+1)+"score"){''',"legacy join HUD parser")
p.write_text(s)

p=Path("src/Game.cpp")
s=p.read_text()
s=rep(s,
'''    for(size_t i=0;i<slots_.size();++i){
        if(!slots_[i].joined)continue;
        auto p=player(i);const auto& h=hud.players[i];''',
'''    for(size_t i=0;i<slots_.size();++i){
        const auto& h=hud.players[i];
        if(!slots_[i].joined){
            if(i>0&&i<(size_t)std::max(1,db_.campaign().maxPlayers)&&credits_>0){
                hudText(L"PRESS START",h.join.prompt,5.5f);
            }
            continue;
        }
        auto p=player(i);''',"ingame join prompt")
s=rep(s,
'        hudText(std::to_wstring(std::max(0,slots_[i].lives)),h.lives,6.f);',
'''        hudText(L"x",h.lifeX,6.f);
        hudText(std::to_wstring(std::max(0,slots_[i].lives)),h.lives,6.f);''',"legacy lives x")
s=s.replace("v0.3.27","v0.3.28")
p.write_text(s)

p=Path("src/main_win.cpp")
p.write_text(p.read_text().replace("v0.3.27","v0.3.28"))
print("apply_v0328_fidelity=OK")
