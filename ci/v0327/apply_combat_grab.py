from pathlib import Path

def rep(s, old, new, label):
    if old not in s:
        raise SystemExit(f"missing {label}")
    return s.replace(old,new,1)

p=Path("src/Game.cpp")
s=p.read_text()

# OpenBOR treats a valid zero-force collision box as an active attack window.
s=rep(
    s,
    'const bool attackActive=f->attack.rect.valid&&f->attack.damage>0;',
    'const bool attackActive=f->attack.rect.valid;',
    "zero-force attack window"
)

# The old port released grabforward/backward in enterFrame with hardcoded damage and velocity,
# before the actual attack box was tested. Let updateCombat resolve the authored attack instead.
old='''    if(a.grabTarget&&(starts(a.anim,"grabforward")||starts(a.anim,"grabbackward"))&&f->attack.damage>0){
        auto t=actorById(a.grabTarget);if(t){a.hitTargets.insert(t->id);releaseGrab(a,true,f->attack.damage,(a.facingLeft?-1.f:1.f)*105.f,150.f,fallFromAttackKind(f->attack.kind));}
    }
'''
if old not in s:
    raise SystemExit("hardcoded throw release block missing")
s=s.replace(old,"",1)

# A player follow animation is part of the grab/throw sequence and must not be overwritten back to grab.
s=rep(
    s,
    'bool throwing=starts(a.anim,"grabforward")||starts(a.anim,"grabbackward")||a.anim=="grabup",striking=starts(a.anim,"grabattack");',
    'bool throwing=starts(a.anim,"grabforward")||starts(a.anim,"grabbackward")||a.anim=="grabup"||starts(a.anim,"follow"),striking=starts(a.anim,"grabattack");',
    "player follow grab state"
)

start=s.index('void Game::damage(Actor&t,Actor&att,')
end=s.index('void Game::updateCombat(){',start)
new_damage=r'''void Game::damage(Actor&t,Actor&att,int amount,int knockdown,const std::string&hitfx,const std::string&hitflash,const std::string&kind,const AttackBox* attack){
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
'''
s=s[:start]+new_damage+s[end:]

start=s.index('void Game::updateCombat(){')
end=s.index('\n\n\nbool Game::enemiesAlive()',start)
new_combat=r'''void Game::updateCombat(){
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
                bool hostile=canDamage(att,t);
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
'''
s=s[:start]+new_combat+s[end:]

s=s.replace("v0.3.26","v0.3.27")
p.write_text(s)

p=Path("src/main_win.cpp")
p.write_text(p.read_text().replace("v0.3.26","v0.3.27"))

print("apply_v0327_combat_grab=OK")
