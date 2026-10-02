#pragma once
#ifdef _WIN32
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
namespace ffx {
struct ImageInfo { Microsoft::WRL::ComPtr<ID2D1Bitmap> bmp; float w=0,h=0; };
struct AnimatedFrameInfo { Microsoft::WRL::ComPtr<ID2D1Bitmap> bmp; float w=0,h=0,delay=.1f; };
struct AnimatedImageInfo { std::vector<AnimatedFrameInfo> frames; float duration=0; };
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
    void drawImage(const std::string& rel,float x,float y,bool flip=false,float opacity=1.f);
    void drawImageScaled(const std::string& rel,float x,float y,float w,float h,bool flip=false,float opacity=1.f);
    void drawImageRemapped(const std::string& rel,const std::string& basePalette,const std::string& altPalette,float x,float y,bool flip=false,float opacity=1.f);
    void drawAnimatedImage(const std::string& rel,float elapsed,float x=0,float y=0,bool loop=false,float opacity=1.f);
    void drawImageTiledX(const std::string& rel,float cameraX,float parallax=1.f,float y=0.f);
    void fillRect(float x,float y,float w,float h,D2D1_COLOR_F c,float opacity=1.f);
    void text(const std::wstring& s,float x,float y,float size,D2D1_COLOR_F c,bool center=false);
    ID2D1HwndRenderTarget* target(){return target_.Get();}
    void setPostSettings(int upscaleMode,int filterMode,int filterStrength,bool integerScale,bool widescreen,bool vsync);
    bool postProcessingActive() const{return postActive_;}
    float logicalWidth() const{return wideAspect_?426.f:320.f;}
    float logicalHeight() const{return 240.f;}
private:
    bool createTarget(); bool ensureSceneTarget(); void compositeScene(); void discardDeviceResources();
    HWND hwnd_{}; std::filesystem::path root_; UINT pixelW_=960,pixelH_=720; float scale_=3,offX_=0,offY_=0;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_; Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1BitmapRenderTarget> sceneTarget_;
    ID2D1RenderTarget* active_{}; bool postActive_=false;
    int upscaleMode_=2,filterMode_=0,filterStrength_=55; bool integerScale_=true,wideAspect_=false,vsync_=true;
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_; Microsoft::WRL::ComPtr<IDWriteFactory> write_;
    std::unordered_map<std::string,ImageInfo> images_;
    std::unordered_map<std::string,ImageInfo> remapped_;
    std::unordered_map<std::string,AnimatedImageInfo> animations_;
};
}
#endif
