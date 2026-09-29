from pathlib import Path

def rep(s, old, new, label):
    if old not in s:
        raise SystemExit(f"missing {label}")
    return s.replace(old, new, 1)

# Actor attackone state: OpenBOR locks to one entity, not one hit.
p=Path("src/Game.h")
s=p.read_text()
s=rep(
    s,
    "bool attackWindowActive=false,attackOneConsumed=false,grabReady=false;",
    "bool attackWindowActive=false,grabReady=false;\n    int attackOneTarget=0;",
    "attackOne actor state"
)
p.write_text(s)

# Explicitly recognize legacy 'defense all' without a factor.
p=Path("src/OpenBorData.cpp")
s=p.read_text()
s=rep(
    s,
    'else if(cmd=="defense"&&t.size()>1&&lower(t[1])=="all"&&t.size()>2)e.defenseAll=toFloat(t[2],1.f);',
    'else if(cmd=="defense"&&t.size()>1&&lower(t[1])=="all")e.defenseAll=t.size()>2?toFloat(t[2],1.f):1.f;',
    "defense all legacy parser"
)
p.write_text(s)

p=Path("src/Game.cpp")
s=p.read_text()

s=s.replace(
    "a.hitTargets.clear();a.attackWindowActive=false;a.attackOneConsumed=false;",
    "a.hitTargets.clear();a.attackWindowActive=false;a.attackOneTarget=0;"
)
s=s.replace(
    "a.frame=0;a.attackWindowActive=false;a.attackOneConsumed=false;a.hitTargets.clear();",
    "a.frame=0;a.attackWindowActive=false;a.attackOneTarget=0;a.hitTargets.clear();"
)

s=rep(
    s,
    '''    if(attackActive&&!a.attackWindowActive){
        if(!an->attackOne)a.hitTargets.clear();
        a.attackWindowActive=true;++a.attackSerial;
    }else if(!attackActive&&a.attackWindowActive){
        a.attackWindowActive=false;
        if(!an->attackOne)a.hitTargets.clear();
    }''',
    '''    if(attackActive&&!a.attackWindowActive){
        // A new collision window gets a new attack id. attackone only limits
        // which entity may be hit; it does not suppress later boxes on that entity.
        a.hitTargets.clear();
        a.attackWindowActive=true;++a.attackSerial;
    }else if(!attackActive&&a.attackWindowActive){
        a.attackWindowActive=false;
        a.hitTargets.clear();
    }''',
    "attack window reset semantics"
)

s=rep(
    s,
    '''        }else{
            t.va=95.f;
            t.vx=dir*55.f;
        }''',
    '''        }else{
            // OpenBOR legacy default_model_dropv = { x=1.2, y=3, z=0 }.
            // Use the same unit conversion already used for authored dropv.
            t.va=3.f*42.f;
            t.vx=dir*1.2f*42.f;
            t.vz=0.f;
        }''',
    "default OpenBOR dropv"
)

s=rep(s, "        if(an->attackOne&&att.attackOneConsumed)continue;\n", "", "remove one-hit attackone gate")
s=rep(s, "        bool follow=false;", "        bool follow=false;int followTarget=0;", "follow target state")

s=rep(
    s,
    '''        for(auto&t:actors_){
            if(t.dead||t.invincibleTime>0||att.hitTargets.count(t.id)||!canDamage(att,t))continue;''',
    '''        for(auto&t:actors_){
            if(an->attackOne&&att.attackOneTarget&&att.attackOneTarget!=t.id)continue;
            if(t.dead||t.invincibleTime>0||att.hitTargets.count(t.id)||!canDamage(att,t))continue;''',
    "attackone target gate"
)

s=rep(
    s,
    '''            att.hitTargets.insert(t.id);
            if(an->attackOne)att.attackOneConsumed=true;''',
    '''            att.hitTargets.insert(t.id);
            if(an->attackOne&&!att.attackOneTarget)att.attackOneTarget=t.id;''',
    "attackone target capture"
)

s=rep(
    s,
    '''                    default:break;
                }
            }

            if(att.projectile&&att.removeOnHit&&!att.projectileExploding){att.dead=true;break;}''',
    '''                    default:break;
                }
                if(follow)followTarget=t.id;
            }

            if(att.projectile&&att.removeOnHit&&!att.projectileExploding){att.dead=true;break;}''',
    "follow target capture"
)

s=rep(
    s,
    '''        if(follow){
            auto n="follow"+std::to_string(an->followAnim);
            if(animation(att,n))setAnim(att,n);
        }''',
    '''        if(follow){
            auto n="follow"+std::to_string(an->followAnim);
            if(animation(att,n)){
                setAnim(att,n);
                // Follow animations with attackone continue working on the entity
                // that caused the follow-up instead of picking a new nearby target.
                if(auto fan=animation(att,n);fan&&fan->attackOne)att.attackOneTarget=followTarget;
            }
        }''',
    "follow attackone target inheritance"
)

p.write_text(s)
print("apply_v0327_attackone_fix=OK")
