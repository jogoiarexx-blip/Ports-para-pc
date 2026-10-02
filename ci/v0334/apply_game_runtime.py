from pathlib import Path

root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit(f"v0.3.34 game anchor missing: {rel}: {old[:120]!r}")
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("src/main_win.cpp","Native C++ Port v0.3.33","Native C++ Port v0.3.34")
rep("src/Game.cpp","NATIVE PORT v0.3.33","NATIVE PORT v0.3.34")

rep("src/Game.h",
'''    int landingDamage=0,landingMode=0;
    int pendingEnergyCost=0;''',
'''    int landingDamage=0;
    bool blastReaction=false;
    int pendingEnergyCost=0;''')

rep("src/Game.h",
'''    void handleLanding(Actor&a,float floorHeight=0);''',
'''    void handleLanding(Actor&a,float floorHeight=0,float impactVelocity=0);''')

rep("src/Game.cpp",
'''    a.anim=nn;a.frame=0;a.frameTime=0;a.deathAnimationFinished=false;a.deathFinishedAt=-1.f;
    a.hitTargets.clear();a.attackWindowActive=false;a.attackOneConsumed=false;''',
'''    a.anim=nn;a.frame=0;a.frameTime=0;a.deathAnimationFinished=false;a.deathFinishedAt=-1.f;
    if(a.anim=="idle"||starts(a.anim,"rise"))a.blastReaction=false;
    a.hitTargets.clear();a.attackWindowActive=false;a.attackOneConsumed=false;''')

rep("src/Game.cpp",
'''else if(e.kind=="jump"){float h=e.args.empty()?3.f:toFloat(e.args[0],3.f),dx=e.args.size()>1?toFloat(e.args[1]):0.f,dz=e.args.size()>2?toFloat(e.args[2]):0.f,dir=a.facingLeft?-1.f:1.f;a.va=std::max(a.va,h*42.f);a.vx+=dx*42.f*dir;a.vz+=dz*24.f;}''',
'''else if(e.kind=="jump"){float h=e.args.empty()?0.f:toFloat(e.args[0],0.f);bool hasX=e.args.size()>1;float dx=hasX?toFloat(e.args[1]):0.f,dz=e.args.size()>2?toFloat(e.args[2]):0.f;auto jm=openBorJumpFrameMotion(a.def?a.def->type:std::string{},a.def?a.def->jumpHeight:4.f,h,hasX,dx,dz);float dir=a.facingLeft?-1.f:1.f;a.va=jm.lift*42.f;a.vx=jm.x*42.f*dir;a.vz=jm.z*24.f;if(jm.lift!=0.f)a.a+=.5f;}''')

rep("src/Game.cpp",
'''            const float vx=(a.facingLeft?-1.f:1.f)*std::max(18.f,std::abs(victim->def->throwDist)*42.f);
            const float va=std::max(45.f,std::abs(victim->def->throwHeight)*42.f);
            const int dmg=std::max(0,victim->def->throwDamage);
            a.hitTargets.insert(victim->id);
            releaseGrab(a,true,dmg,vx,va,"fall");''',
'''            const float vx=(a.facingLeft?-1.f:1.f)*(victim->def->throwDist*42.f);
            const float lift=(victim->def->throwHeight!=0.f)?victim->def->throwHeight:victim->def->jumpHeight;
            const float va=lift*42.f;
            const int dmg=std::max(0,a.def->throwDamage);
            victim->facingLeft=a.facingLeft;
            a.hitTargets.insert(victim->id);
            releaseGrab(a,true,0,vx,va,"fall");
            victim->landingDamage=dmg;''')

rep("src/Game.cpp",
'''void Game::handleLanding(Actor&a,float floorHeight){if(a.safeLandingRequested){a.safeLandingRequested=false;a.landingDamage=0;if(animation(a,"land"))setAnim(a,"land");else setAnim(a,"idle");return;}if(a.landingDamage>0){a.hp=std::max(0,a.hp-a.landingDamage);a.landingDamage=0;if(a.hp<=0){a.dead=true;if(animation(a,"death"))setAnim(a,"death");else if(animation(a,"fall"))setAnim(a,"fall");return;}}if(a.def&&a.def->bounce&&!a.bounced&&starts(a.anim,"fall")){a.bounced=true;a.va=85.f;a.a=floorHeight+1.f;return;}if(a.def&&!a.def->noQuake&&starts(a.anim,"fall")){shakeTime_=std::max(shakeTime_,.12f);shakeAmpX_=2;shakeAmpY_=2;}auto an=animation(a,a.anim);if(!an)return;for(const auto&e:an->events)if(e.kind=="land"){if(e.frame>=0&&e.frame<(int)an->frames.size()&&(int)a.frame<e.frame){a.frame=(size_t)e.frame;a.frameTime=0;if(!e.args.empty())queueEffect(lower(e.args[0]),a.x,a.z,floorHeight,a.facingLeft);enterFrame(a);}break;}}''',
'''void Game::handleLanding(Actor&a,float floorHeight,float impactVelocity){if(a.safeLandingRequested){a.safeLandingRequested=false;a.landingDamage=0;if(animation(a,"land"))setAnim(a,"land");else setAnim(a,"idle");return;}auto an=animation(a,a.anim);if(a.def&&a.def->bounce&&an&&starts(a.anim,"fall")){float rebound=openBorBounceVelocity(std::max(0.f,-impactVelocity),an->bounceFactor);if(rebound>0.f){a.bounced=true;a.vx/=an->bounceFactor;a.vz/=an->bounceFactor;a.va=rebound;a.a=floorHeight+.5f;if(!a.def->noQuake){shakeTime_=std::max(shakeTime_,.12f);shakeAmpX_=2;shakeAmpY_=2;}}}if(a.landingDamage>0){a.hp=std::max(0,a.hp-a.landingDamage);a.landingDamage=0;if(a.hp<=0){a.dead=true;if(animation(a,"death"))setAnim(a,"death");else if(animation(a,"fall"))setAnim(a,"fall");return;}}if(a.def&&!a.def->noQuake&&starts(a.anim,"fall")&&a.va==0){shakeTime_=std::max(shakeTime_,.12f);shakeAmpX_=2;shakeAmpY_=2;}an=animation(a,a.anim);if(!an)return;for(const auto&e:an->events)if(e.kind=="land"){if(e.frame>=0&&e.frame<(int)an->frames.size()&&(int)a.frame<e.frame){a.frame=(size_t)e.frame;a.frameTime=0;if(!e.args.empty())queueEffect(lower(e.args[0]),a.x,a.z,floorHeight,a.facingLeft);enterFrame(a);}break;}}''')

rep("src/Game.cpp",
'''void Game::updatePhysics(Actor&a,float dt){if(a.projectile||a.grabbedBy||a.combatPauseTime>0)return;float floor=platformFloor(a),oldA=a.a;bool airborne=a.a>floor+.01f||std::abs(a.va)>.01f;float ox=a.x,oz=a.z;a.x+=a.vx*dt;a.z+=a.vz*dt;resolveWalls(a,ox,oz);a.vx*=std::pow(.06f,dt);a.vz*=std::pow(.06f,dt);floor=platformFloor(a);if(airborne){a.a+=a.va*dt;a.va-=360.f*dt;if(a.va<=0&&a.a<=floor&&oldA>=floor-2.f){a.a=floor;a.va=0;auto landingAnim=animation(a,a.anim);bool authoredLand=false;if(landingAnim)for(const auto&e:landingAnim->events)if(e.kind=="land"){authoredLand=true;break;}handleLanding(a,floor);if(a.va==0&&!a.dead&&!authoredLand&&!starts(a.anim,"fall")&&!starts(a.anim,"pain")&&!starts(a.anim,"grab"))setAnim(a,"idle");}}else a.a=floor;}''',
'''void Game::updatePhysics(Actor&a,float dt){if(a.projectile||a.grabbedBy||a.combatPauseTime>0)return;float floor=platformFloor(a),oldA=a.a;bool airborne=a.a>floor+.01f||std::abs(a.va)>.01f;float ox=a.x,oz=a.z;a.x+=a.vx*dt;a.z+=a.vz*dt;resolveWalls(a,ox,oz);a.vx*=std::pow(.06f,dt);a.vz*=std::pow(.06f,dt);floor=platformFloor(a);if(airborne){a.a+=a.va*dt;a.va-=360.f*dt;if(a.va<=0&&a.a<=floor&&oldA>=floor-2.f){float impactVelocity=a.va;a.a=floor;a.va=0;auto landingAnim=animation(a,a.anim);bool authoredLand=false;if(landingAnim)for(const auto&e:landingAnim->events)if(e.kind=="land"){authoredLand=true;break;}handleLanding(a,floor,impactVelocity);if(a.va==0&&!a.dead&&!authoredLand&&!starts(a.anim,"fall")&&!starts(a.anim,"pain")&&!starts(a.anim,"grab"))setAnim(a,"idle");}}else a.a=floor;}''')

rep("src/Game.cpp",
'''        if(attack){t.landingDamage=std::max(0,attack->landingDamage);t.landingMode=attack->landingMode;}''',
'''        if(attack){t.landingDamage=std::max(0,attack->landingDamage);t.blastReaction=attack->blast;}else t.blastReaction=false;''')

rep("src/Game.cpp",
'''        if(att.dead||att.effect||att.combatPauseTime>0)continue;
        auto an=animation(att,att.anim);''',
'''        if(att.dead||att.effect||att.combatPauseTime>0)continue;
        if(starts(att.anim,"fall")&&!att.blastReaction)continue;
        auto an=animation(att,att.anim);''')

Path("CHANGELOG-v0.3.34.txt").write_text("""Final Fight X Native - v0.3.34 Build 3797 Combat Motion Fidelity

- jumpframe replaces lift/X/Z velocity exactly instead of accumulating it.
- legacy jumpframe defaults are honored for player, enemy and NPC.
- bounce uses actual impact velocity and animation bouncefactor, with repeated diminishing rebounds.
- damageonlanding second argument is the Build 3797 blast flag.
- ordinary fall attack boxes are suppressed unless blast/projectile-style fall is active.
- grab throw defaults corrected: throwdist 2.5, throwheight fallback to victim jumpheight, throwdamage comes from thrower and is applied on landing.
""",encoding="utf-8")

Path("PAK-FIDELITY-AUDIT-v0.3.34.txt").write_text("""Final Fight X v1.0.0 / OpenBOR v3.0 Build 3797

PAK focus for v0.3.34:
jumpframe=143 active uses across 36 character files
damageonlanding=25 active uses; all set blast flag 1
bounce=active on Sodom weapon variants; default animation bouncefactor 4
throwframe=8 verified uses; all use custknife and authored altitude
throwframewait=3 declarations; no anim throw exists in this PAK
""",encoding="utf-8")

Path("README-SOURCE-v0.3.34.txt").write_text("""Final Fight X Native v0.3.34 - Source

cmake -S . -B build -A x64
cmake --build build --config Release --target FinalFightX -- /m

Place FinalFightX.exe beside the assets folder from the portable package.
""",encoding="utf-8")

print("v0.3.34 game/runtime patch applied")
