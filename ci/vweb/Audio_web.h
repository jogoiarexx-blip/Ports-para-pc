#pragma once
#if defined(__EMSCRIPTEN__)
#include <filesystem>
#include <string>
namespace ffx {
class Audio{
public:
    bool init(const std::filesystem::path& root);
    void shutdown(); void update();
    void playSfx(const std::string& rel,float volume=1.f);
    void playMusic(const std::string& rel,bool loop=true);
    void stopMusic(); void pauseMusic(bool paused);
    void setMusicVolume(float v); void setSfxVolume(float v);
    float musicVolume() const{return musicVolume_;}
    float sfxVolume() const{return sfxVolume_;}
private:
    std::filesystem::path root_;
    float musicVolume_=.75f,sfxVolume_=.85f;
};
}
#endif
