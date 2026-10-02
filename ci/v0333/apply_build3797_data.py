from pathlib import Path
root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit("v0.3.33 data anchor missing: "+rel)
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("CMakeLists.txt","project(FinalFightXNative VERSION 0.3.32 LANGUAGES CXX)","project(FinalFightXNative VERSION 0.3.33 LANGUAGES CXX)")
rep("src/main_win.cpp","Native C++ Port v0.3.32","Native C++ Port v0.3.33")
rep("src/Game.cpp","NATIVE PORT v0.3.32","NATIVE PORT v0.3.33")
rep("src/OpenBorSemantics.h",
'''inline bool openBorBlockOddsPass(std::uint32_t seed,int blockOdds){
    // Legacy OpenBOR models express this as odds of 1:blockodds.
    // A value of 1 therefore passes every eligible check; <=0 disables
    // random AI blocking for the legacy modules this native port targets.
    if(blockOdds<=0)return false;
    return (seed%(std::uint32_t)blockOdds)==0;
}''',
'''inline bool openBorBlockOddsPass(std::uint32_t seed,int blockOdds){
    // OpenBOR v3.0 / Build 3797 checks: (rand32() & blockodds) == 1.
    if(blockOdds<=0)return false;
    return (seed & (std::uint32_t)blockOdds)==1u;
}''')
rep("src/OpenBorData.h","float speed=6.f;","float speed=6.f, jumpHeight=4.f;")
rep("src/OpenBorData.cpp",
'else if(cmd=="speed"&&t.size()>1)e.speed=toFloat(t[1],6);',
'else if(cmd=="speed"&&t.size()>1)e.speed=toFloat(t[1],6);\n            else if(cmd=="jumpheight"&&t.size()>1)e.jumpHeight=toFloat(t[1],4);')
rep("src/OpenBorData.cpp",
'if(cmd=="landframe"&&t.size()>1){pushEvent(cur,"land",std::max(0,toInt(t[1])-1),std::vector<std::string>(t.begin()+2,t.end()));continue;}',
'if(cmd=="landframe"&&t.size()>1){pushEvent(cur,"land",std::max(0,toInt(t[1])),std::vector<std::string>(t.begin()+2,t.end()));continue;}')
print("v0.3.33 data/Build3797 patch applied")
