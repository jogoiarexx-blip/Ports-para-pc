#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ffx {
inline std::string trim(std::string s) {
    auto notSpace=[](unsigned char c){return !std::isspace(c);};
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}
inline std::string lower(std::string s) {
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return (char)std::tolower(c);});
    return s;
}
inline std::vector<std::string> splitWs(const std::string& s) {
    std::istringstream is(s); std::vector<std::string> v; std::string x;
    while(is>>x) v.push_back(x);
    return v;
}
inline std::string restAfterFirst(const std::string& s) {
    auto p=s.find_first_of(" \t"); return p==std::string::npos?std::string{}:trim(s.substr(p+1));
}
inline std::vector<std::string> readLines(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary); std::vector<std::string> out; std::string line;
    while(std::getline(f,line)) { if(!line.empty() && line.back()=='\r') line.pop_back(); out.push_back(line); }
    return out;
}
inline float toFloat(const std::string& s,float d=0.f){try{return std::stof(s);}catch(...){return d;}}
inline int toInt(const std::string& s,int d=0){try{return std::stoi(s);}catch(...){return d;}}
inline std::string normalizeAsset(std::string s) {
    std::replace(s.begin(),s.end(),'\\','/');
    auto l=lower(s); if(l.rfind("data/",0)==0) s=s.substr(5);
    return s;
}
}
