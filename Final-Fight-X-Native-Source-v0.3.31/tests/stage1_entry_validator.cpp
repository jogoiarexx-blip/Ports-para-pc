#include "OpenBorData.h"
#include "OpenBorSemantics.h"
#include <cmath>
#include <filesystem>
#include <iostream>

using namespace ffx;
static bool nearf(float a,float b,float eps=.05f){return std::fabs(a-b)<=eps;}

int main(int argc,char**argv){
    std::filesystem::path root=argc>1?argv[1]:"assets/data";
    OpenBorDatabase db;std::string err;
    if(!db.load(root,&err)){std::cerr<<"FAIL load: "<<err<<"\n";return 2;}
    if(db.campaign().stages.empty()){std::cerr<<"FAIL: no stages\n";return 2;}
    auto level=db.loadLevel(db.campaign().stages.front());
    int bad=0;auto check=[&](bool ok,const char*msg){if(!ok){++bad;std::cerr<<"FAIL: "<<msg<<"\n";}};

    check(level.walls.size()>=2,"stage 1 walls missing");
    const LevelWall* endWall=nullptr;
    for(const auto&w:level.walls)if(nearf(w.x,1613.f))endWall=&w;
    check(endWall!=nullptr,"end wall x=1613 missing");
    if(endWall){
        check(nearf(endWall->upperLeft,-85.f),"upperLeft lost");
        check(nearf(endWall->lowerLeft,0.f),"lowerLeft lost");
        check(nearf(endWall->upperRight,60.f),"upperRight lost");
        check(nearf(endWall->lowerRight,60.f),"lowerRight lost");
        auto top=openBorTerrainSpanAtZ(endWall->x,endWall->z,endWall->upperLeft,endWall->lowerLeft,endWall->upperRight,endWall->lowerRight,endWall->depth,153.f);
        auto bottom=openBorTerrainSpanAtZ(endWall->x,endWall->z,endWall->upperLeft,endWall->lowerLeft,endWall->upperRight,endWall->lowerRight,endWall->depth,238.f);
        auto outside=openBorTerrainSpanAtZ(endWall->x,endWall->z,endWall->upperLeft,endWall->lowerLeft,endWall->upperRight,endWall->lowerRight,endWall->depth,242.f);
        check(top.valid&&nearf(top.left,1528.f)&&nearf(top.right,1673.f),"top wall span wrong");
        check(bottom.valid&&nearf(bottom.left,1613.f)&&nearf(bottom.right,1673.f),"bottom wall span wrong");
        check(!outside.valid,"z=242 must be outside the wall, not falsely blocked");
        check(!openBorTerrainContainsPoint(endWall->x,endWall->z,endWall->upperLeft,endWall->lowerLeft,endWall->upperRight,endWall->lowerRight,endWall->depth,1599.f,242.f),"bred spawn is falsely inside wall");
    }

    const SpawnDef* bred=nullptr;
    for(const auto&s:level.spawns)if(s.model=="bred"&&nearf(s.trigger,1150.f)){bred=&s;break;}
    check(bred!=nullptr,"bred at 1150 missing");
    if(bred){
        check(nearf(bred->x,449.f)&&nearf(bred->z,242.f),"bred source coords changed");
        const float world=1569.f;
        for(float viewW: {320.f,426.f}){
            const float maxCam=std::max(0.f,world-viewW);
            const float camera=std::min(1150.f,maxCam);
            const float spawnX=camera+bred->x;
            auto intent=openBorArenaEntryIntent(spawnX,bred->z,camera,camera+viewW,level.zMin,level.zMax);
            check(intent.active,"bred must be in arena-entry mode after spawn");
            check(intent.xDir<0.f,"bred must walk left into the visible arena");
            check(intent.zDir<0.f,"bred z=242 must walk toward playable depth");
        }
    }

    if(bad){std::cerr<<"stage1_entry_failures="<<bad<<"\n";return 3;}
    std::cout<<"stage1_entry_semantics=OK walls="<<level.walls.size()<<" spawns="<<level.spawns.size()<<"\n";
    return 0;
}
