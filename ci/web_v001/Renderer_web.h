#pragma once
#if defined(__EMSCRIPTEN__)
#include "PlatformCompat.h"
#include <filesystem>
#include <string>
#include <unordered_map>
namespace ffx {
struct ImageInfo { float w=0,h=0; std::string physical; };
struct AnimatedImageInfo { float w=0,h=0,duration=1.5f; std::string physical; };
class Renderer {
public:
    bool init(HWND hwnd,const std::filesystem::path& assetRoot);
    void shutdown(); void resize(UINT w,UINT h); bool begin(bool postProcess=false); void end();
    void clear(D2D1_COLOR_F c=D2D1::ColorF(D2D1::ColorF::Black));
    void fillScreen(D2D1_COLOR_F c,float opacity=1.f);
    void drawImageCoverScreen(const std::string& rel,float opacity=1.f);
    const ImageInfo* image(const std::string& rel);
    const AnimatedImageInfo* animatedImage(const std::string& rel);
    float animationDuration(const std::string& rel);
    bool preloadAnimation(const std::string& rel){return animatedImage(rel)!=nullptr;}
    void drawImage(const std::string& rel,float x,float y,bool flip=false,float opacity=1.f);
    void drawImageScaled(const std::string& rel,float x,float y,float w,float h,bool flip=false,float opacity=1.f);
    void drawImageRemapped(const std::string& rel,const std::string& basePalette,const std::string& altPalette,float x,float y,bool flip=false,float opacity=1.f);
    void drawAnimatedImage(const std::string& rel,float elapsed,float x=0,float y=0,bool loop=false,float opacity=1.f);
    void drawImageTiledX(const std::string& rel,float cameraX,float parallax=1.f,float y=0.f);
    void fillRect(float x,float y,float w,float h,D2D1_COLOR_F c,float opacity=1.f);
    void text(const std::wstring& s,float x,float y,float size,D2D1_COLOR_F c,bool center=false);
    void textCenteredAt(const std::wstring& s,float centerX,float y,float width,float size,D2D1_COLOR_F c);
    void* target(){return nullptr;}
    void setPostSettings(int upscaleMode,int filterMode,int filterStrength,bool integerScale,bool widescreen,bool vsync);
    void setHdTextures(bool enabled);
    bool hdTexturesEnabled() const{return hdTextures_;}
    bool hdTexturesAvailable() const{return hdAvailable_;}
    bool postProcessingActive() const{return postActive_;}
    float logicalWidth() const{return wideAspect_?426.f:320.f;}
    float logicalHeight() const{return 240.f;}
private:
    std::string resolve(const std::string& rel,bool animated=false) const;
    static std::string utf8(const std::wstring& s);
    std::filesystem::path root_;
    UINT pixelW_=960,pixelH_=720;
    int upscaleMode_=2,filterMode_=0,filterStrength_=55;
    bool integerScale_=true,wideAspect_=false,vsync_=true,hdTextures_=true,hdAvailable_=true,postActive_=false;
    std::unordered_map<std::string,ImageInfo> images_;
    std::unordered_map<std::string,AnimatedImageInfo> animations_;
};
}
#endif
