#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace ffx {
enum class ReactionKind { Pain, Fall, Rise, Death };

inline std::string semanticLower(std::string s){
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return (char)std::tolower(c);});
    return s;
}

inline std::string attackTypeSuffix(const std::string& rawKind){
    auto kind=semanticLower(rawKind);
    if(kind.empty()||kind=="attack"||kind=="attack1")return {};
    if(kind.rfind("attack",0)==0&&kind.size()>6){
        auto suffix=kind.substr(6);
        if(!suffix.empty()&&std::all_of(suffix.begin(),suffix.end(),[](unsigned char c){return std::isdigit(c)!=0;}))return suffix;
    }
    return {};
}

inline std::vector<std::string> reactionCandidates(const std::string& rawKind,ReactionKind reaction){
    const auto kind=semanticLower(rawKind);
    const auto suffix=attackTypeSuffix(kind);
    std::vector<std::string> out;
    auto add=[&](const std::string&s){if(!s.empty()&&std::find(out.begin(),out.end(),s)==out.end())out.push_back(s);};
    if(kind=="burn"){
        if(reaction==ReactionKind::Pain){add("bpain");add("pain");add("pain1");}
        else if(reaction==ReactionKind::Fall){add("burn");add("fall");add("fall1");}
        else if(reaction==ReactionKind::Rise){add("riseb");add("brise");add("rise");add("rise1");}
        else {add("bdie");add("bdeath");add("death");add("death1");add("burn");add("fall");add("fall1");}
        return out;
    }
    if(kind=="shock"){
        if(reaction==ReactionKind::Pain){add("spain");add("pain");add("pain1");}
        else if(reaction==ReactionKind::Fall){add("shock");add("fall");add("fall1");}
        else if(reaction==ReactionKind::Rise){add("rises");add("srise");add("rise");add("rise1");}
        else {add("sdie");add("sdeath");add("death");add("death1");add("shock");add("fall");add("fall1");}
        return out;
    }
    if(reaction==ReactionKind::Pain){if(!suffix.empty())add("pain"+suffix);add("pain");add("pain1");}
    else if(reaction==ReactionKind::Fall){if(!suffix.empty())add("fall"+suffix);add("fall");add("fall1");}
    else if(reaction==ReactionKind::Rise){if(!suffix.empty())add("rise"+suffix);add("rise");add("rise1");}
    else {if(!suffix.empty())add("death"+suffix);add("death");add("death1");if(!suffix.empty())add("fall"+suffix);add("fall");add("fall1");}
    return out;
}


struct OpenBorTerrainSpan { bool valid=false; float left=0.f,right=0.f; };

inline OpenBorTerrainSpan openBorTerrainSpanAtZ(float x,float z,float upperLeft,float lowerLeft,float upperRight,float lowerRight,float depth,float sampleZ){
    depth=std::abs(depth);
    const float top=z-depth,bottom=z;
    constexpr float eps=0.001f;
    if(sampleZ<top-eps||sampleZ>bottom+eps)return {};
    const float t=depth>eps?std::clamp((sampleZ-top)/depth,0.f,1.f):1.f;
    float left=x+(upperLeft+(lowerLeft-upperLeft)*t);
    float right=x+(upperRight+(lowerRight-upperRight)*t);
    if(left>right)std::swap(left,right);
    return {true,left,right};
}

inline bool openBorTerrainContainsPoint(float x,float z,float upperLeft,float lowerLeft,float upperRight,float lowerRight,float depth,float sampleX,float sampleZ){
    const auto span=openBorTerrainSpanAtZ(x,z,upperLeft,lowerLeft,upperRight,lowerRight,depth,sampleZ);
    return span.valid&&sampleX>=span.left&&sampleX<=span.right;
}

struct ArenaEntryIntent { bool active=false; float xDir=0.f,zDir=0.f; };

inline ArenaEntryIntent openBorArenaEntryIntent(float actorX,float actorZ,float viewLeft,float viewRight,float zMin,float zMax,float inset=6.f){
    ArenaEntryIntent out;
    const float left=viewLeft+inset,right=viewRight-inset;
    if(actorX<left)out.xDir=1.f;else if(actorX>right)out.xDir=-1.f;
    if(actorZ<zMin)out.zDir=1.f;else if(actorZ>zMax)out.zDir=-1.f;
    out.active=out.xDir!=0.f||out.zDir!=0.f;
    return out;
}
inline bool openBorBlockOddsPass(std::uint32_t seed,int blockOdds){
    if(blockOdds<0)return false;
    return (seed&(std::uint32_t)blockOdds)==0;
}

inline bool isReverseDirection(const std::string& rawDirection){
    auto d=semanticLower(rawDirection);return d=="left"||d=="leftright";
}
inline bool isBidirectionalDirection(const std::string& rawDirection){
    auto d=semanticLower(rawDirection);return d=="both"||d=="rightleft"||d=="leftright";
}
inline bool isVerticalDirection(const std::string& rawDirection){
    auto d=semanticLower(rawDirection);return d=="up"||d=="down";
}
} // namespace ffx
