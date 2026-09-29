from pathlib import Path

def rep(s, old, new, label):
    if old not in s:
        raise SystemExit(f"missing {label}")
    return s.replace(old, new, 1)

p=Path("src/OpenBorData.h")
s=p.read_text()
s=rep(s,
'''    bool noFlash=false;
    int landingDamage=0, landingMode=0;
    bool customDrop=false;''',
'''    bool noFlash=false;
    int landingDamage=0;
    bool blast=false;
    bool customDrop=false;''',
"AttackBox blast state")
p.write_text(s)

p=Path("src/OpenBorData.cpp")
s=p.read_text()
s=rep(s,
'if(cmd=="damageonlanding"){atk.landingDamage=t.size()>1?toInt(t[1]):0;atk.landingMode=t.size()>2?toInt(t[2]):0;continue;}',
'''if(cmd=="damageonlanding"){
            // OpenBOR legacy: arg1 = damage applied on landing, arg2 = blast state.
            atk.landingDamage=t.size()>1?toInt(t[1]):0;
            atk.blast=t.size()>2?toInt(t[2])!=0:false;
            continue;
        }''',
"damageonlanding parser")
s=rep(s,
'''            const int landingDamage=atk.landingDamage;
            const int landingMode=atk.landingMode;
            atk={};
            atk.landingDamage=landingDamage;
            atk.landingMode=landingMode;''',
'''            const int landingDamage=atk.landingDamage;
            const bool blast=atk.blast;
            atk={};
            atk.landingDamage=landingDamage;
            atk.blast=blast;''',
"damageonlanding modifier persistence")
p.write_text(s)

p=Path("src/Game.h")
s=p.read_text()
s=rep(s,
'''    int landingDamage=0,landingMode=0;
    int pendingEnergyCost=0;''',
'''    int landingDamage=0;
    bool fallingState=false,blastActive=false;
    int blastOwner=0,blastCreditPlayer=-1;
    int pendingEnergyCost=0;''',
"actor fall/blast state")
p.write_text(s)

p=Path("src/Game.cpp")
s=p.read_text()

s=rep(s,
'''static bool isOffensiveAnimation(const Animation& an){
    for(const auto&f:an.frames)if(f.attack.rect.valid&&f.attack.damage>0)return true;
    for(const auto&e:an.events)if(e.kind=="throw"||e.kind=="toss"||e.kind=="shoot")return true;
    return false;
}''',
'''static bool isOffensiveAnimation(const Animation& an){
    bool hasContact=false;
    for(const auto&f:an.frames)if(f.attack.rect.valid){
        hasContact=true;
        if(f.attack.damage>0||f.attack.knockdown>0)return true;
    }
    if(hasContact&&an.followAnim>0)return true;
    for(const auto&e:an.events)if(e.kind=="throw"||e.kind=="toss"||e.kind=="shoot")return true;
    return false;
}''',
"AI offensive zero-force semantics")

s=rep(s,
'''void Game::handleLanding(Actor&a,float floorHeight){if(a.safeLandingRequested){a.safeLandingRequested=false;a.landingDamage=0;if(animation(a,"land"))setAnim(a,"land");else setAnim(a,"idle");return;}''',
'''void Game::handleLanding(Actor&a,float floorHeight){if(a.safeLandingRequested){a.safeLandingRequested=false;a.landingDamage=0;a.fallingState=false;a.blastActive=false;a.blastOwner=0;a.blastCreditPlayer=-1;if(animation(a,"land"))setAnim(a,"land");else setAnim(a,"idle");return;}''',
"safe landing blast clear")

s=rep(s,
'''if(a.def&&a.def->bounce&&!a.bounced&&starts(a.anim,"fall")){a.bounced=true;a.va=85.f;a.a=floorHeight+1.f;return;}if(a.def&&!a.def->noQuake&&starts(a.anim,"fall"))''',
'''if(a.def&&a.def->bounce&&!a.bounced&&starts(a.anim,"fall")){a.bounced=true;a.va=85.f;a.a=floorHeight+1.f;return;}a.fallingState=false;a.blastActive=false;a.blastOwner=0;a.blastCreditPlayer=-1;if(a.def&&!a.def->noQuake&&starts(a.anim,"fall"))''',
"final landing blast clear")

s=rep(s,
'''    if(att.player&&att.playerIndex>=0&&att.playerIndex<(int)slots_.size()&&!t.player)slots_[(size_t)att.playerIndex].hudTargetId=t.id;
    if(t.player&&t.playerIndex>=0&&t.playerIndex<(int)slots_.size()&&!att.player)slots_[(size_t)t.playerIndex].hudTargetId=att.id;''',
'''    const int creditPlayer=att.player?att.playerIndex:att.blastCreditPlayer;
    if(creditPlayer>=0&&creditPlayer<(int)slots_.size()&&!t.player)slots_[(size_t)creditPlayer].hudTargetId=t.id;
    if(t.player&&t.playerIndex>=0&&t.playerIndex<(int)slots_.size()&&!att.player)slots_[(size_t)t.playerIndex].hudTargetId=att.id;''',
"blast score/HUD credit")

s=rep(s,
'''        if(!t.scoreAwarded&&att.team==Team::Player){registerScore(att.playerIndex,t.def?t.def->score:0);t.scoreAwarded=true;}''',
'''        if(!t.scoreAwarded&&creditPlayer>=0){registerScore(creditPlayer,t.def?t.def->score:0);t.scoreAwarded=true;}''',
"blast kill credit")

s=rep(s,
'''    if(knockdown>0){
        t.thrownBy=att.id;t.bounced=false;''',
'''    if(knockdown>0){
        t.thrownBy=att.id;t.bounced=false;t.fallingState=true;''',
"fall state set")

s=rep(s,
'''        if(attack){t.landingDamage=std::max(0,attack->landingDamage);t.landingMode=attack->landingMode;}''',
'''        if(attack){
            t.landingDamage=std::max(0,attack->landingDamage);
            t.blastActive=attack->blast;
            if(t.blastActive){
                t.blastOwner=att.id;
                t.blastCreditPlayer=creditPlayer;
            }else{
                t.blastOwner=0;
                t.blastCreditPlayer=-1;
            }
        }''',
"blast state transfer")

s=rep(s,
'''        if(!an||!f||!f->attack.rect.valid)continue;

        auto ar=attackRect(att);''',
'''        if(!an||!f||!f->attack.rect.valid)continue;
        // OpenBOR suppresses attack boxes while falling unless the entity was
        // thrown/blasted. A blast victim keeps the authored FALL attack boxes.
        if(att.fallingState&&!att.blastActive)continue;

        auto ar=attackRect(att);''',
"fall attack gate")

s=rep(s,
'''        for(auto&t:actors_){
            if(an->attackOne&&att.attackOneTarget&&att.attackOneTarget!=t.id)continue;
            if(t.dead||t.invincibleTime>0||att.hitTargets.count(t.id)||!canDamage(att,t))continue;''',
'''        for(auto&t:actors_){
            if(an->attackOne&&att.attackOneTarget&&att.attackOneTarget!=t.id)continue;
            const Actor* indirectOwner=att.blastActive?actorById(att.blastOwner):nullptr;
            const bool canHit=indirectOwner?canDamage(*indirectOwner,t):canDamage(att,t);
            if(t.dead||t.invincibleTime>0||att.hitTargets.count(t.id)||!canHit)continue;''',
"blast indirect faction")

s=rep(s,
'''            if(att.player&&att.weaponNumber>0&&att.weaponNumber!=3&&att.weaponUses>0){
                if(--att.weaponUses<=0)unequipWeapon(att);
            }''',
'''            const bool realImpact=f->attack.damage>0||f->attack.knockdown>0;
            if(realImpact&&att.player&&att.weaponNumber>0&&att.weaponNumber!=3&&att.weaponUses>0){
                if(--att.weaponUses<=0)unequipWeapon(att);
            }''',
"sensor weapon durability")

p.write_text(s)
print("apply_v0328_blast=OK")
