#pragma once
#ifdef _WIN32
#include <xaudio2.h>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
namespace ffx {
struct AudioClip{WAVEFORMATEX fmt{};std::vector<unsigned char> data;};
class Audio{
public:
    bool init(const std::filesystem::path& root);
    void shutdown();
    void update();
    void playSfx(const std::string& rel,float volume=1.f);
    void playMusic(const std::string& rel,bool loop=true);
    void stopMusic();
    void pauseMusic(bool paused);
    void setMusicVolume(float v);
    void setSfxVolume(float v);
    float musicVolume() const{return musicVolume_;}
    float sfxVolume() const{return sfxVolume_;}
private:
    std::shared_ptr<AudioClip> load(const std::string& rel,bool cacheClip=true);
    std::filesystem::path root_;
    IXAudio2* xa_{};
    IXAudio2MasteringVoice* master_{};
    IXAudio2SourceVoice* music_{};
    std::shared_ptr<AudioClip> musicClip_;
    struct Active{IXAudio2SourceVoice* v{};std::shared_ptr<AudioClip> c;};
    std::vector<Active> active_;
    std::unordered_map<std::string,std::shared_ptr<AudioClip>> cache_;
    float musicVolume_=.75f,sfxVolume_=.85f;
    bool musicPaused_=false;
};
}
#endif
