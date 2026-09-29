from pathlib import Path

def rep(s,old,new,label):
    if old not in s:
        raise SystemExit("missing "+label)
    return s.replace(old,new,1)

p=Path("src/OpenBorData.h")
s=p.read_text()
s=rep(s,
    "std::vector<std::string> weapons, canDamage, hostile;",
    "std::vector<std::string> weapons, canDamage, hostile, modelLoads;",
    "model load dependency storage")
p.write_text(s)

p=Path("src/OpenBorData.cpp")
s=p.read_text()
s=rep(s,
    'else if(cmd=="defense"&&t.size()>1&&lower(t[1])=="all"&&t.size()>2)e.defenseAll=toFloat(t[2],1.f);',
    'else if(cmd=="defense"&&t.size()>1&&lower(t[1])=="all")e.defenseAll=t.size()>2?toFloat(t[2],0.f):0.f;',
    "legacy defense all")
s=rep(s,
    'else if(cmd=="hostile") for(size_t i=1;i<t.size();++i)e.hostile.push_back(lower(t[i]));',
    'else if(cmd=="hostile") for(size_t i=1;i<t.size();++i)e.hostile.push_back(lower(t[i]));\n            else if(cmd=="load"&&t.size()>1)e.modelLoads.push_back(lower(t[1]));',
    "model load parser")
p.write_text(s)

for name in ("src/Game.cpp","src/main_win.cpp"):
    p=Path(name); p.write_text(p.read_text().replace("v0.3.28","v0.3.29"))

print("apply_v0329_defense_load=OK")
