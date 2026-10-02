#ifdef _WIN32
#include "Audio.h"
#include "BorAudio.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
namespace ffx {
static float clamp01(float v){return std::max(0.f,std::min(1.f,v));}
static uint32_t u32(const std::vector<unsigned char>&b,size_t o){uint32_t v=0;if(o+4<=b.size())std::memcpy(&v,b.data()+o,4);return v;}
static uint16_t u16(const std::vector<unsigned char>&b,size_t o){uint16_t v=0;if(o+2<=b.size())std::memcpy(&v,b.data()+o,2);return v;}
bool Audio::init(const std::filesystem::path&root){root_=root;if(FAILED(XAudio2Create(&xa_,0,XAUDIO2_DEFAULT_PROCESSOR)))return false;if(FAILED(xa_->CreateMasteringVoice(&master_)))return false;return true;}
void Audio::shutdown(){stopMusic();for(auto&a:active_)if(a.v){a.v->Stop();a.v->DestroyVoice();}active_.clear();cache_.clear();if(master_){master_->DestroyVoice();master_=nullptr;}if(xa_){xa_->Release();xa_=nullptr;}}
std::shared_ptr<AudioClip> Audio::load(const std::string&rel,bool cacheClip){
    if(cacheClip){auto it=cache_.find(rel);if(it!=cache_.end())return it->second;}
    auto p=root_/std::filesystem::path(rel);std::ifstream f(p,std::ios::binary);if(!f)return {};
    std::vector<unsigned char>b((std::istreambuf_iterator<char>(f)),{});auto c=std::make_shared<AudioClip>();
    auto ext=p.extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char ch){return (char)std::tolower(ch);});
    if(ext==".bor"){
        BorPcm decoded;if(!decodeBorMusic(b,decoded))return {};
        c->fmt.wFormatTag=WAVE_FORMAT_PCM;c->fmt.nChannels=decoded.channels;c->fmt.nSamplesPerSec=decoded.sampleRate;c->fmt.wBitsPerSample=16;c->fmt.nBlockAlign=(WORD)(decoded.channels*2u);c->fmt.nAvgBytesPerSec=decoded.sampleRate*c->fmt.nBlockAlign;c->fmt.cbSize=0;
        c->data.resize(decoded.samples.size()*sizeof(int16_t));if(!decoded.samples.empty())std::memcpy(c->data.data(),decoded.samples.data(),c->data.size());
    }
    else if(ext==".wav"&&b.size()>44&&std::memcmp(b.data(),"RIFF",4)==0){
        size_t pos=12;bool gotFmt=false,gotData=false;
        while(pos+8<=b.size()){
            uint32_t id=u32(b,pos),sz=u32(b,pos+4);size_t d=pos+8;if(d+sz>b.size())break;
            if(id==0x20746d66){if(sz>=16){c->fmt.wFormatTag=u16(b,d);c->fmt.nChannels=u16(b,d+2);c->fmt.nSamplesPerSec=u32(b,d+4);c->fmt.nAvgBytesPerSec=u32(b,d+8);c->fmt.nBlockAlign=u16(b,d+12);c->fmt.wBitsPerSample=u16(b,d+14);c->fmt.cbSize=0;gotFmt=true;}}
            else if(id==0x61746164){c->data.assign(b.begin()+d,b.begin()+d+sz);gotData=true;}
            pos=d+sz+(sz&1);
        }
        if(!gotFmt||!gotData||c->fmt.wFormatTag!=WAVE_FORMAT_PCM||!c->fmt.nChannels||!c->fmt.nBlockAlign)return {};
    }else return {};
    if(cacheClip)cache_[rel]=c;return c;
}
void Audio::playSfx(const std::string&rel,float vol){if(!xa_||rel.empty()||sfxVolume_<=0)return;auto c=load(rel,true);if(!c)return;IXAudio2SourceVoice*v{};if(FAILED(xa_->CreateSourceVoice(&v,&c->fmt)))return;XAUDIO2_BUFFER xb{};xb.AudioBytes=(UINT32)c->data.size();xb.pAudioData=c->data.data();xb.Flags=XAUDIO2_END_OF_STREAM;if(FAILED(v->SubmitSourceBuffer(&xb))){v->DestroyVoice();return;}v->SetVolume(clamp01(vol)*sfxVolume_);v->Start();active_.push_back({v,c});}
void Audio::update(){active_.erase(std::remove_if(active_.begin(),active_.end(),[](Active&a){XAUDIO2_VOICE_STATE s{};a.v->GetState(&s);if(!s.BuffersQueued){a.v->DestroyVoice();return true;}return false;}),active_.end());}
void Audio::stopMusic(){if(music_){music_->Stop();music_->DestroyVoice();music_=nullptr;}musicClip_.reset();musicPaused_=false;}
void Audio::pauseMusic(bool paused){if(!music_||musicPaused_==paused)return;if(paused)music_->Stop();else music_->Start();musicPaused_=paused;}
void Audio::setMusicVolume(float v){musicVolume_=clamp01(v);if(music_)music_->SetVolume(musicVolume_);}
void Audio::setSfxVolume(float v){sfxVolume_=clamp01(v);}
void Audio::playMusic(const std::string&rel,bool loop){stopMusic();if(!xa_||rel.empty())return;musicClip_=load(rel,false);if(!musicClip_)return;if(FAILED(xa_->CreateSourceVoice(&music_,&musicClip_->fmt))){musicClip_.reset();return;}XAUDIO2_BUFFER xb{};xb.AudioBytes=(UINT32)musicClip_->data.size();xb.pAudioData=musicClip_->data.data();xb.Flags=XAUDIO2_END_OF_STREAM;xb.LoopCount=loop?XAUDIO2_LOOP_INFINITE:0;if(FAILED(music_->SubmitSourceBuffer(&xb))){music_->DestroyVoice();music_=nullptr;musicClip_.reset();return;}music_->SetVolume(musicVolume_);music_->Start();musicPaused_=false;}
}
#endif
