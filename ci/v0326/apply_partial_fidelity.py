from pathlib import Path

def rep(s, old, new, label):
    if old not in s:
        raise SystemExit(f"missing {label}")
    return s.replace(old,new,1)

# 1) Legacy attack Z depth and quakeframe parsing.
p=Path("src/OpenBorData.h")
s=p.read_text()
s=rep(
    s,
    '''struct AttackBox {
    RectF rect;
    int damage=0, knockdown=0, pause=0, noBlock=0;
    bool noFlash=false;''',
    '''struct AttackBox {
    RectF rect;
    int damage=0, knockdown=0, pause=0, noBlock=0;
    float zDepth=13.f; // OpenBOR legacy default: grabdistance(36)/3 + 1.
    bool noFlash=false;''',
    "AttackBox z depth"
)
p.write_text(s)

p=Path("src/OpenBorData.cpp")
s=p.read_text()
s=rep(
    s,
    'if(cmd=="quakeframe"&&t.size()>1){pushEvent(cur,"quake",toInt(t[1]),std::vector<std::string>(t.begin()+2,t.end()));continue;}',
    '''if(cmd=="quakeframe"&&t.size()>1){
            int start=toInt(t[1]),repeat=t.size()>2?std::max(1,toInt(t[2],1)):1;
            std::string vertical=t.size()>3?t[3]:"3";
            for(int q=0;q<repeat;++q)pushEvent(cur,"quake",start+q,{vertical});
            continue;
        }''',
    "quakeframe parser"
)
s=rep(
    s,
    '''            if(t.size()>9)atk.pause=toInt(t[9]);
            continue;''',
    '''            if(t.size()>9)atk.pause=toInt(t[9]);
            if(t.size()>10){float z=std::abs(toFloat(t[10]));atk.zDepth=z>0.f?z:13.f;}
            continue;''',
    "attack z parser"
)
p.write_text(s)

# 2) Runtime: projectile placement, quake, depth collision, spawn aliases.
p=Path("src/Game.h")
s=p.read_text()
s=rep(s,'    std::string baseModel;','    std::string baseModel,displayName;', "Actor displayName")
s=rep(
    s,
    'void queueProjectile(const Actor& owner,const std::string& model,const std::string& mode,float power);',
    'void queueProjectile(const Actor& owner,const std::string& model,const std::string& mode,float height,float power=0.f);',
    "queueProjectile declaration"
)
p.write_text(s)

p=Path("src/Game.cpp")
s=p.read_text()
s=rep(
    s,
    '''void Game::queueProjectile(const Actor&owner,const std::string&model,const std::string&mode,float power){if(model.empty()||!db_.entity(model))return;float dir=owner.facingLeft?-1.f:1.f;pendingSpawns_.push_back({lower(model),mode,owner.x+dir*22.f,owner.z,owner.a+24.f,power,owner.facingLeft,false,owner.team,owner.id});}''',
    '''void Game::queueProjectile(const Actor&owner,const std::string&model,const std::string&mode,float height,float power){if(model.empty()||!db_.entity(model))return;pendingSpawns_.push_back({lower(model),mode,owner.x,owner.z,owner.a+height,power,owner.facingLeft,false,owner.team,owner.id});}''',
    "queueProjectile runtime"
)

old_events='''void Game::executeFrameEvents(Actor&a,const Animation&an){for(const auto&e:an.events){if(e.frame!=(int)a.frame)continue;if(e.kind=="flip")a.facingLeft=!a.facingLeft;else if(e.kind=="jump"){float h=e.args.empty()?3.f:toFloat(e.args[0],3.f),dx=e.args.size()>1?toFloat(e.args[1]):0.f,dz=e.args.size()>2?toFloat(e.args[2]):0.f,dir=a.facingLeft?-1.f:1.f;a.va=std::max(a.va,h*42.f);a.vx+=dx*42.f*dir;a.vz+=dz*24.f;}else if(e.kind=="quake"){shakeTime_=std::max(shakeTime_,.28f);shakeAmpX_=e.args.empty()?3.f:std::abs(toFloat(e.args[0],3));shakeAmpY_=e.args.size()>1?std::abs(toFloat(e.args[1],3)):3.f;}else if(e.kind=="throw"){float p=e.args.empty()?100.f:toFloat(e.args[0],100);queueProjectile(a,an.projectileModel,"throw",p);if(a.player&&a.weaponNumber==3)a.unequipAfterAnim=true;}else if(e.kind=="toss"){float p=e.args.empty()?50.f:toFloat(e.args[0],50);std::string m=!a.def->bombModel.empty()?a.def->bombModel:an.projectileModel;queueProjectile(a,m,"toss",p);}else if(e.kind=="shoot"){std::string m=an.projectileModel;if(m.empty()&&a.def&&db_.entity(a.def->shotModel))m=a.def->shotModel;queueProjectile(a,m,"shoot",0);}}}'''
new_events='''void Game::executeFrameEvents(Actor&a,const Animation&an){for(const auto&e:an.events){if(e.frame!=(int)a.frame)continue;if(e.kind=="flip")a.facingLeft=!a.facingLeft;else if(e.kind=="jump"){float h=e.args.empty()?3.f:toFloat(e.args[0],3.f),dx=e.args.size()>1?toFloat(e.args[1]):0.f,dz=e.args.size()>2?toFloat(e.args[2]):0.f,dir=a.facingLeft?-1.f:1.f;a.va=std::max(a.va,h*42.f);a.vx+=dx*42.f*dir;a.vz+=dz*24.f;}else if(e.kind=="quake"){float v=e.args.empty()?3.f:toFloat(e.args[0],3.f);auto cf=frame(a);shakeTime_=std::max(shakeTime_,openBorFrameSeconds(cf?cf->delay:10)*1.25f);shakeAmpX_=0.f;shakeAmpY_=std::abs(v);}else if(e.kind=="throw"){float h=e.args.empty()?70.f:toFloat(e.args[0],70.f);if(std::abs(h)<.001f)h=70.f;queueProjectile(a,an.projectileModel,"throw",h);if(a.player&&a.weaponNumber==3)a.unequipAfterAnim=true;}else if(e.kind=="toss"){float h=e.args.empty()?70.f:toFloat(e.args[0],70.f);if(std::abs(h)<.001f)h=70.f;std::string m=!a.def->bombModel.empty()?a.def->bombModel:an.projectileModel;queueProjectile(a,m,"toss",h,h);}else if(e.kind=="shoot"){float h=e.args.empty()?70.f:toFloat(e.args[0],70.f);if(std::abs(h)<.001f)h=70.f;std::string m=an.projectileModel;if(m.empty()&&a.def&&db_.entity(a.def->shotModel))m=a.def->shotModel;queueProjectile(a,m,"shoot",h);}}}'''
s=rep(s,old_events,new_events,"executeFrameEvents")

s=rep(
    s,
    'if(std::abs(att.z-t.z)>40||!intersects(ar,bodyRect(t)))continue;',
    'if(std::abs(att.z-t.z)>std::max(1.f,f->attack.zDepth)||!intersects(ar,bodyRect(t)))continue;',
    "combat z depth"
)
s=rep(
    s,
    'a.def=d;a.baseModel=lower(model);a.x=x;',
    'a.def=d;a.baseModel=lower(model);a.displayName=d->name;a.x=x;',
    "actor display name init"
)
s=rep(
    s,
    '''void Game::spawnActor(const SpawnDef&s){float base=cameraForProgress(s.trigger);auto a=createActor(s.model,base+s.x,s.z,s.flip,false,s.boss,s.healthOverride,s.item);if(a){a->a=s.a;a->paletteMap=s.map;''',
    '''void Game::spawnActor(const SpawnDef&s){float base=cameraForProgress(s.trigger);auto a=createActor(s.model,base+s.x,s.z,s.flip,false,s.boss,s.healthOverride,s.item);if(a){a->a=s.a;if(!s.alias.empty())a->displayName=s.alias;a->paletteMap=s.map;''',
    "spawn alias wiring"
)
s=rep(
    s,
    'hudText(w(target->def->name),h.enemyName,5.5f);',
    'hudText(w(target->displayName.empty()?target->def->name:target->displayName),h.enemyName,5.5f);',
    "HUD alias display"
)
s=s.replace("v0.3.25","v0.3.26")
p.write_text(s)

p=Path("src/main_win.cpp")
p.write_text(p.read_text().replace("v0.3.25","v0.3.26"))

print("apply_v0326_partial_fidelity=OK")
