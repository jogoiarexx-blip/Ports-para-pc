#if defined(__EMSCRIPTEN__)
#include "Renderer_web.h"
#include <emscripten.h>
#include <algorithm>
#include <cmath>
#include <codecvt>
#include <locale>

namespace ffx {
EM_JS(void, js_ffx_begin_frame, (int logicalW,int logicalH,int post,int upscale,int filter,int strength,int integerScale), {
    if (globalThis.FFXWeb) FFXWeb.beginFrame(logicalW,logicalH,!!post,upscale,filter,strength,!!integerScale);
});
EM_JS(void, js_ffx_end_frame, (), { if (globalThis.FFXWeb) FFXWeb.endFrame(); });
EM_JS(void, js_ffx_clear, (float r,float g,float b,float a), { if (globalThis.FFXWeb) FFXWeb.clear(r,g,b,a); });
EM_JS(void, js_ffx_fill_rect, (float x,float y,float w,float h,float r,float g,float b,float a,float opacity), {
    if (globalThis.FFXWeb) FFXWeb.fillRect(x,y,w,h,r,g,b,a,opacity);
});
EM_JS(void, js_ffx_draw_image, (const char* p,float x,float y,float w,float h,int flip,float opacity), {
    if (globalThis.FFXWeb) FFXWeb.drawImage(UTF8ToString(p),x,y,w,h,!!flip,opacity);
});
EM_JS(void, js_ffx_draw_cover, (const char* p,float opacity), {
    if (globalThis.FFXWeb) FFXWeb.drawCover(UTF8ToString(p),opacity);
});
EM_JS(void, js_ffx_draw_remap, (const char* src,const char* base,const char* alt,float x,float y,float w,float h,int flip,float opacity), {
    if (globalThis.FFXWeb) FFXWeb.drawRemapped(UTF8ToString(src),UTF8ToString(base),UTF8ToString(alt),x,y,w,h,!!flip,opacity);
});
EM_JS(void, js_ffx_text, (const char* s,float x,float y,float size,float r,float g,float b,float a,int center,float centerX,float width), {
    if (globalThis.FFXWeb) FFXWeb.text(UTF8ToString(s),x,y,size,r,g,b,a,!!center,centerX,width);
});
EM_JS(void, js_ffx_resize, (int w,int h), { if (globalThis.FFXWeb) FFXWeb.onNativeResize(w,h); });
EM_JS(void, js_ffx_config, (int wide,int hd), { if (globalThis.FFXWeb) FFXWeb.configure(!!wide,!!hd); });
EM_JS(void, js_ffx_preload, (const char* p), { if (globalThis.FFXWeb) FFXWeb.ensureImage(UTF8ToString(p)); });
EM_JS(int, js_ffx_image_size, (const char* p), {
    if(!globalThis.FFXWeb||!FFXWeb.assetMeta)return 0;
    const m=FFXWeb.assetMeta[UTF8ToString(p)];if(!m)return 0;
    return (((m.w|0)&65535)<<16)|((m.h|0)&65535);
});
EM_JS(float, js_ffx_anim_duration, (const char* p), {
    if(!globalThis.FFXWeb||!FFXWeb.assetMeta)return 1.5;
    const m=FFXWeb.assetMeta[UTF8ToString(p)];return m&&m.d?+m.d:1.5;
});

bool Renderer::init(HWND,const std::filesystem::path& root){root_=root;hdAvailable_=true;js_ffx_config(wideAspect_,hdTextures_);return true;}
void Renderer::shutdown(){images_.clear();animations_.clear();}
void Renderer::resize(UINT w,UINT h){pixelW_=std::max(1u,w);pixelH_=std::max(1u,h);js_ffx_resize((int)pixelW_,(int)pixelH_);}
bool Renderer::begin(bool post){postActive_=post;js_ffx_begin_frame((int)logicalWidth(),(int)logicalHeight(),post,upscaleMode_,filterMode_,filterStrength_,integerScale_);return true;}
void Renderer::end(){js_ffx_end_frame();postActive_=false;}
void Renderer::clear(D2D1_COLOR_F c){js_ffx_clear(c.r,c.g,c.b,c.a);}
void Renderer::fillScreen(D2D1_COLOR_F c,float opacity){fillRect(0,0,logicalWidth(),logicalHeight(),c,opacity);}
std::string Renderer::resolve(const std::string& rel,bool animated) const{
    if(!animated&&hdTextures_&&rel.size()>4&&rel.substr(rel.size()-4)==".gif")return "assets/data_hd/"+rel.substr(0,rel.size()-4)+".png";
    return std::string("assets/data/")+rel;
}
const ImageInfo* Renderer::image(const std::string& rel){
    auto it=images_.find(rel);if(it!=images_.end())return &it->second;
    int packed=js_ffx_image_size(rel.c_str());if(!packed)return nullptr;
    ImageInfo info;info.w=(float)((packed>>16)&65535);info.h=(float)(packed&65535);info.physical=resolve(rel,false);js_ffx_preload(info.physical.c_str());
    auto [ins,_]=images_.emplace(rel,std::move(info));return &ins->second;
}
const AnimatedImageInfo* Renderer::animatedImage(const std::string& rel){
    auto it=animations_.find(rel);if(it!=animations_.end())return &it->second;
    int packed=js_ffx_image_size(rel.c_str());if(!packed)return nullptr;
    AnimatedImageInfo info;info.w=(float)((packed>>16)&65535);info.h=(float)(packed&65535);info.duration=js_ffx_anim_duration(rel.c_str());info.physical=resolve(rel,true);js_ffx_preload(info.physical.c_str());
    auto [ins,_]=animations_.emplace(rel,std::move(info));return &ins->second;
}
float Renderer::animationDuration(const std::string& rel){auto a=animatedImage(rel);return a?a->duration:0.f;}
void Renderer::drawImageCoverScreen(const std::string& rel,float opacity){auto im=image(rel);if(!im)return;js_ffx_draw_cover(im->physical.c_str(),opacity);}
void Renderer::drawImage(const std::string& rel,float x,float y,bool flip,float opacity){auto im=image(rel);if(!im)return;js_ffx_draw_image(im->physical.c_str(),x,y,im->w,im->h,flip,opacity);}
void Renderer::drawImageScaled(const std::string& rel,float x,float y,float w,float h,bool flip,float opacity){auto im=image(rel);if(!im||w<=0||h<=0)return;js_ffx_draw_image(im->physical.c_str(),x,y,w,h,flip,opacity);}
void Renderer::drawImageRemapped(const std::string& rel,const std::string& basePalette,const std::string& altPalette,float x,float y,bool flip,float opacity){
    auto im=image(rel);if(!im)return;if(basePalette.empty()||altPalette.empty()){drawImage(rel,x,y,flip,opacity);return;}
    std::string bp=std::string("assets/data/")+basePalette,ap=std::string("assets/data/")+altPalette;
    js_ffx_draw_remap(im->physical.c_str(),bp.c_str(),ap.c_str(),x,y,im->w,im->h,flip,opacity);
}
void Renderer::drawAnimatedImage(const std::string& rel,float,float x,float y,bool,float opacity){auto a=animatedImage(rel);if(!a)return;js_ffx_draw_image(a->physical.c_str(),x,y,a->w,a->h,false,opacity);}
void Renderer::drawImageTiledX(const std::string& rel,float cameraX,float parallax,float y){auto im=image(rel);if(!im||im->w<=0)return;float start=-std::fmod(cameraX*parallax,im->w);if(start>0)start-=im->w;for(float x=start;x<logicalWidth();x+=im->w)drawImage(rel,x,y);}
void Renderer::fillRect(float x,float y,float w,float h,D2D1_COLOR_F c,float opacity){js_ffx_fill_rect(x,y,w,h,c.r,c.g,c.b,c.a,opacity);}
std::string Renderer::utf8(const std::wstring& s){
    std::string o;for(wchar_t wc:s){uint32_t c=(uint32_t)wc;if(c<0x80)o.push_back((char)c);else if(c<0x800){o.push_back((char)(0xC0|(c>>6)));o.push_back((char)(0x80|(c&63)));}else if(c<0x10000){o.push_back((char)(0xE0|(c>>12)));o.push_back((char)(0x80|((c>>6)&63)));o.push_back((char)(0x80|(c&63)));}else{o.push_back((char)(0xF0|(c>>18)));o.push_back((char)(0x80|((c>>12)&63)));o.push_back((char)(0x80|((c>>6)&63)));o.push_back((char)(0x80|(c&63)));}}return o;
}
void Renderer::text(const std::wstring& s,float x,float y,float size,D2D1_COLOR_F c,bool center){auto u=utf8(s);js_ffx_text(u.c_str(),x,y,size,c.r,c.g,c.b,c.a,center,0,0);}
void Renderer::textCenteredAt(const std::wstring& s,float centerX,float y,float width,float size,D2D1_COLOR_F c){auto u=utf8(s);js_ffx_text(u.c_str(),0,y,size,c.r,c.g,c.b,c.a,1,centerX,width);}
void Renderer::setPostSettings(int up,int filter,int strength,bool integerScale,bool widescreen,bool vsync){upscaleMode_=std::clamp(up,0,2);filterMode_=std::clamp(filter,0,3);filterStrength_=std::clamp(strength,0,100);integerScale_=integerScale;wideAspect_=widescreen;vsync_=vsync;images_.clear();animations_.clear();js_ffx_config(wideAspect_,hdTextures_);}
void Renderer::setHdTextures(bool enabled){if(hdTextures_==enabled)return;hdTextures_=enabled;images_.clear();animations_.clear();js_ffx_config(wideAspect_,hdTextures_);}
}
#endif
