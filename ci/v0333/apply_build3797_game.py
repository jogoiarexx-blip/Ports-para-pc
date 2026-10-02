from pathlib import Path
p=Path("src/Game.cpp")
s=p.read_text(encoding="utf-8-sig")
def rep(old,new,label):
    global s
    if old not in s:
        raise SystemExit("v0.3.33 game anchor missing: "+label)
    s=s.replace(old,new,1)

rep(
'''void Game::processPendingSpawns(){auto q=std::move(pendingSpawns_);pendingSpawns_.clear();for(const auto&s:q){auto a=createActor(s.model,s.x,s.z,s.left,false,false,0,{},s.team);if(!a)continue;a->a=s.a;if(s.mode=="item"){a->weaponDropCount=std::max(0,(int)std::lround(s.power));a->weaponItemModel=s.model;continue;}a->projectileOwner=s.owner;if(s.effect){a->effect=true;continue;}a->projectile=true;a->projectileLife=0;a->removeOnHit=a->def->remove!=0;float dir=s.left?-1.f:1.f;float sp=std::max(8.f,a->def->speed);if(s.mode=="toss"){a->projectileArc=true;a->vx=dir*sp*8.f;a->va=std::max(95.f,s.power*2.3f);}else{a->vx=dir*sp*16.f;}}}''',
'''void Game::processPendingSpawns(){auto q=std::move(pendingSpawns_);pendingSpawns_.clear();for(const auto&s:q){auto a=createActor(s.model,s.x,s.z,s.left,false,false,0,{},s.team);if(!a)continue;a->a=s.a;if(s.mode=="item"){a->weaponDropCount=std::max(0,(int)std::lround(s.power));a->weaponItemModel=s.model;continue;}a->projectileOwner=s.owner;if(s.effect){a->effect=true;continue;}a->projectile=true;a->projectileLife=0;a->removeOnHit=a->def->remove!=0;float dir=s.left?-1.f:1.f;float sp=std::max(0.f,a->def->speed);if(s.mode=="toss"){a->projectileArc=true;a->removeOnHit=false;a->vx=dir*sp*8.f;a->va=std::max(0.f,a->def->jumpHeight)*42.f;}else{a->vx=dir*std::max(8.f,sp)*16.f;}}}''',
"toss spawn")
rep(
'''else if(e.kind=="toss"){float h=e.args.empty()?70.f:toFloat(e.args[0],70.f);if(h<0.f)h=0.f;std::string m=!a.def->bombModel.empty()?a.def->bombModel:an.projectileModel;queueProjectile(a,m,"toss",h,h);firedWeapon=true;}''',
'''else if(e.kind=="toss"){float h=e.args.empty()?0.f:toFloat(e.args[0],0.f);if(h<0.f)h=0.f;std::string m=!a.def->bombModel.empty()?a.def->bombModel:an.projectileModel;queueProjectile(a,m,"toss",h,0.f);firedWeapon=true;}''',
"toss event")
rep(
'''void Game::handleLanding(Actor&a,float floorHeight){if(a.safeLandingRequested){a.safeLandingRequested=false;a.landingDamage=0;if(animation(a,"land"))setAnim(a,"land");else setAnim(a,"idle");return;}if(a.landingDamage>0){a.hp=std::max(0,a.hp-a.landingDamage);a.landingDamage=0;if(a.hp<=0){a.dead=true;if(animation(a,"death"))setAnim(a,"death");else if(animation(a,"fall"))setAnim(a,"fall");return;}}if(a.def&&a.def->bounce&&!a.bounced&&starts(a.anim,"fall")){a.bounced=true;a.va=85.f;a.a=floorHeight+1.f;return;}if(a.def&&!a.def->noQuake&&starts(a.anim,"fall")){shakeTime_=std::max(shakeTime_,.12f);shakeAmpX_=2;shakeAmpY_=2;}auto an=animation(a,a.anim);if(!an)return;for(const auto&e:an->events)if(e.kind=="land"){if(!e.args.empty())queueEffect(lower(e.args[0]),a.x,a.z,floorHeight,a.facingLeft);if(e.frame>=0&&e.frame<(int)an->frames.size()){a.frame=(size_t)e.frame;a.frameTime=0;enterFrame(a);break;}}}''',
'''void Game::handleLanding(Actor&a,float floorHeight){if(a.safeLandingRequested){a.safeLandingRequested=false;a.landingDamage=0;if(animation(a,"land"))setAnim(a,"land");else setAnim(a,"idle");return;}if(a.landingDamage>0){a.hp=std::max(0,a.hp-a.landingDamage);a.landingDamage=0;if(a.hp<=0){a.dead=true;if(animation(a,"death"))setAnim(a,"death");else if(animation(a,"fall"))setAnim(a,"fall");return;}}if(a.def&&a.def->bounce&&!a.bounced&&starts(a.anim,"fall")){a.bounced=true;a.va=85.f;a.a=floorHeight+1.f;return;}if(a.def&&!a.def->noQuake&&starts(a.anim,"fall")){shakeTime_=std::max(shakeTime_,.12f);shakeAmpX_=2;shakeAmpY_=2;}auto an=animation(a,a.anim);if(!an)return;for(const auto&e:an->events)if(e.kind=="land"){if(e.frame>=0&&e.frame<(int)an->frames.size()&&(int)a.frame<e.frame){a.frame=(size_t)e.frame;a.frameTime=0;if(!e.args.empty())queueEffect(lower(e.args[0]),a.x,a.z,floorHeight,a.facingLeft);enterFrame(a);}break;}}''',
"landframe runtime")
rep(
'''if(a.va<=0&&a.a<=floor&&oldA>=floor-2.f){a.a=floor;a.va=0;handleLanding(a,floor);if(a.va==0&&!a.dead&&!starts(a.anim,"fall")&&!starts(a.anim,"pain")&&!starts(a.anim,"grab"))setAnim(a,"idle");}''',
'''if(a.va<=0&&a.a<=floor&&oldA>=floor-2.f){a.a=floor;a.va=0;auto landingAnim=animation(a,a.anim);bool authoredLand=false;if(landingAnim)for(const auto&e:landingAnim->events)if(e.kind=="land"){authoredLand=true;break;}handleLanding(a,floor);if(a.va==0&&!a.dead&&!authoredLand&&!starts(a.anim,"fall")&&!starts(a.anim,"pain")&&!starts(a.anim,"grab"))setAnim(a,"idle");}''',
"landing animation hold")
rep(
'''void Game::updateProjectile(Actor&a,float dt){if(a.dead||!a.projectile||a.combatPauseTime>0)return;a.projectileLife+=dt;int off=a.def?std::max(80,a.def->offscreenKill):200;if(a.projectileLife>8.f||a.x<cameraX_-off||a.x>cameraX_+renderer_.logicalWidth()+off){a.dead=true;return;}if(a.projectileExploding)return;a.x+=a.vx*dt;a.z+=a.vz*dt;if(a.projectileArc){a.a+=a.va*dt;a.va-=300.f*dt;if(a.a<=0){a.a=0;a.va=0;a.vx=a.vz=0;a.projectileExploding=true;a.projectileArc=false;if(animation(a,"attack"))setAnim(a,"attack");else if(animation(a,"attack1"))setAnim(a,"attack1");else a.dead=true;}}}''',
'''void Game::updateProjectile(Actor&a,float dt){if(a.dead||!a.projectile||a.combatPauseTime>0)return;a.projectileLife+=dt;int off=a.def?std::max(80,a.def->offscreenKill):200;if(a.projectileLife>8.f||a.x<cameraX_-off||a.x>cameraX_+renderer_.logicalWidth()+off){a.dead=true;return;}if(a.projectileExploding)return;a.x+=a.vx*dt;a.z+=a.vz*dt;if(a.projectileArc){float floor=platformFloor(a);a.a+=a.va*dt;a.va-=300.f*dt;if(a.va<=0&&a.a<=floor){a.a=floor;a.va=0;a.vx=a.vz=0;a.projectileExploding=true;a.projectileArc=false;if(animation(a,"attack"))setAnim(a,"attack");else if(animation(a,"attack1"))setAnim(a,"attack1");else a.dead=true;}}}''',
"bomb landing")
rep(
'''if(allowFlash&&!hitflash.empty()&&(!t.def||!t.def->noAtFlash)){
        queueEffect(hitflash,t.x,t.z,t.a,att.x>t.x);
        flashTime_=std::max(flashTime_,.028f);
    }''',
'''std::string resolvedHitFlash=hitflash;if(t.def&&t.def->noAtFlash)resolvedHitFlash=t.def->defaultFlash;
    if(allowFlash&&!resolvedHitFlash.empty()){
        queueEffect(resolvedHitFlash,t.x,t.z,t.a,att.x>t.x);
        flashTime_=std::max(flashTime_,.028f);
    }''',
"noatflash")
rep(
'''            if(att.projectile&&att.removeOnHit&&!att.projectileExploding){att.dead=true;break;}
            if(follow||an->attackOne)break;''',
'''            if(att.projectile&&att.projectileArc&&!att.projectileExploding){att.projectileExploding=true;att.projectileArc=false;att.vx=att.vz=att.va=0;if(animation(att,"attack2"))setAnim(att,"attack2");else if(animation(att,"attack"))setAnim(att,"attack");else if(animation(att,"attack1"))setAnim(att,"attack1");else att.dead=true;break;}
            if(att.projectile&&att.removeOnHit&&!att.projectileExploding){att.dead=true;break;}
            if(follow||an->attackOne)break;''',
"bomb contact")
p.write_text(s,encoding="utf-8")

Path("CHANGELOG-v0.3.33.txt").write_text("""Final Fight X Native - v0.3.33 Build 3797 Fidelity

- landframe uses the authored zero-based frame exactly.
- authored landing animations are not rewound and finish naturally.
- blockodds mirrors Build 3797 bit-mask behavior.
- tossframe uses authored spawn altitude plus bomb-model jumpheight.
- tossed bombs land on platform floors and explode on impact/contact.
- noatflash falls back to the victim model's own flash.
- flipframe remains exact-frame after legacy-source verification.
""",encoding="utf-8")
Path("PAK-FIDELITY-AUDIT-v0.3.33.txt").write_text("""Final Fight X v1.0.0 / OpenBOR v3.0 Build 3797

PAK command audit:
landframe=201 corrected
flipframe=188 retained exact-frame
jumpframe=143 retained
noatflash=11 corrected
throwframe=8 retained
blockodds=2 restored to legacy bit-mask
tossframe=2 completed
shootframe=1 retained
""",encoding="utf-8")
Path("README-SOURCE-v0.3.33.txt").write_text("""Final Fight X Native v0.3.33 - Source

cmake -S . -B build -A x64
cmake --build build --config Release --target FinalFightX -- /m

Place FinalFightX.exe beside the assets folder from the portable package.
""",encoding="utf-8")
print("v0.3.33 game/runtime patch applied")
