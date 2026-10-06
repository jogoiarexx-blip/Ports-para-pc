#if defined(__EMSCRIPTEN__)
#include "Audio_web.h"
#include <emscripten.h>
#include <algorithm>
namespace ffx {
EM_JS(void, js_audio_sfx, (const char* p,float v), { if(globalThis.FFXWeb) FFXWeb.audio.playSfx(UTF8ToString(p),v); });
EM_JS(void, js_audio_music, (const char* p,int loop,float v), { if(globalThis.FFXWeb) FFXWeb.audio.playMusic(UTF8ToString(p),!!loop,v); });
EM_JS(void, js_audio_stop, (), { if(globalThis.FFXWeb) FFXWeb.audio.stopMusic(); });
EM_JS(void, js_audio_pause, (int p), { if(globalThis.FFXWeb) FFXWeb.audio.pauseMusic(!!p); });
static std::string webAudioPath(std::string rel){if(rel.size()>4&&rel.substr(rel.size()-4)==".bor")rel.replace(rel.size()-4,4,".ogg");return std::string("assets/data/")+rel;}
bool Audio::init(const std::filesystem::path& root){root_=root;return true;}
void Audio::shutdown(){stopMusic();}
void Audio::update(){}
void Audio::playSfx(const std::string& rel,float v){if(rel.empty()||sfxVolume_<=0)return;auto p=webAudioPath(rel);js_audio_sfx(p.c_str(),std::clamp(v,0.f,1.f)*sfxVolume_);}
void Audio::playMusic(const std::string& rel,bool loop){if(rel.empty())return;auto p=webAudioPath(rel);js_audio_music(p.c_str(),loop,musicVolume_);}
void Audio::stopMusic(){js_audio_stop();}
void Audio::pauseMusic(bool p){js_audio_pause(p);}
void Audio::setMusicVolume(float v){musicVolume_=std::clamp(v,0.f,1.f);}
void Audio::setSfxVolume(float v){sfxVolume_=std::clamp(v,0.f,1.f);}
}
#endif
