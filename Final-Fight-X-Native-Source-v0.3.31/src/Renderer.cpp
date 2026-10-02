#ifdef _WIN32
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#include <d2d1helper.h>
#include <propidl.h>
#include <cstdint>
#include <cstring>
#include <unordered_map>
namespace ffx {
using Microsoft::WRL::ComPtr;
static std::wstring wide(const std::filesystem::path&p){return p.wstring();}
static inline float snapPx(float v){return std::round(v);}

struct RawWicImage{UINT w=0,h=0;std::vector<unsigned char> px;};

static bool decodeRawFrame(IWICImagingFactory* wic,IWICBitmapFrameDecode* frame,RawWicImage& out,bool forcePaletteZeroTransparent=true){
    if(!wic||!frame)return false;
    ComPtr<IWICFormatConverter> conv;
    if(FAILED(wic->CreateFormatConverter(&conv)))return false;
    if(FAILED(conv->Initialize(frame,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
    if(FAILED(conv->GetSize(&out.w,&out.h))||!out.w||!out.h)return false;
    out.px.resize((size_t)out.w*out.h*4);
    if(FAILED(conv->CopyPixels(nullptr,out.w*4,(UINT)out.px.size(),out.px.data())))return false;

    // Sprite/model art follows the OpenBOR convention that palette index 0 is
    // transparent even when the GIF omits a Graphic Control Extension. Scene GIFs
    // are different: they must obey the GIF's own transparency metadata, otherwise
    // legitimate palette colors disappear.
    WICPixelFormatGUID pf{};
    bool keyed=false;
    if(forcePaletteZeroTransparent&&SUCCEEDED(frame->GetPixelFormat(&pf)) && IsEqualGUID(pf,GUID_WICPixelFormat8bppIndexed)){
        std::vector<unsigned char> idx((size_t)out.w*out.h);
        if(SUCCEEDED(frame->CopyPixels(nullptr,out.w,(UINT)idx.size(),idx.data()))){
            for(size_t i=0;i<idx.size();++i){
                if(idx[i]==0){
                    auto* d=out.px.data()+i*4;
                    d[0]=d[1]=d[2]=d[3]=0;
                }
            }
            keyed=true;
        }
    }
    // Fallback for unusual indexed decoders: use palette entry 0 as a color key.
    if(forcePaletteZeroTransparent&&!keyed){
        ComPtr<IWICPalette> pal;
        if(SUCCEEDED(wic->CreatePalette(&pal)) && SUCCEEDED(frame->CopyPalette(pal.Get()))){
            UINT count=0;
            if(SUCCEEDED(pal->GetColorCount(&count)) && count){
                WICColor c=0; UINT actual=0;
                if(SUCCEEDED(pal->GetColors(1,&c,&actual)) && actual){
                    const unsigned char kb=(unsigned char)(c&0xff);
                    const unsigned char kg=(unsigned char)((c>>8)&0xff);
                    const unsigned char kr=(unsigned char)((c>>16)&0xff);
                    for(size_t i=0,n=out.px.size()/4;i<n;++i){
                        auto* d=out.px.data()+i*4;
                        if(d[0]==kb&&d[1]==kg&&d[2]==kr){
                            d[0]=d[1]=d[2]=d[3]=0;
                        }
                    }
                }
            }
        }
    }
    return true;
}

static bool decodeRaw(IWICImagingFactory* wic,const std::filesystem::path& path,RawWicImage& out){
    ComPtr<IWICBitmapDecoder> dec;ComPtr<IWICBitmapFrameDecode> frame;
    if(FAILED(wic->CreateDecoderFromFilename(path.wstring().c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&dec)))return false;
    if(FAILED(dec->GetFrame(0,&frame)))return false;
    return decodeRawFrame(wic,frame.Get(),out,true);
}
bool Renderer::init(HWND hwnd,const std::filesystem::path& root){hwnd_=hwnd;root_=root;CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory_.GetAddressOf())))return false;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(wic_.GetAddressOf()))))return false;if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write_.GetAddressOf()))))return false;RECT r{};GetClientRect(hwnd_,&r);pixelW_=r.right-r.left;pixelH_=r.bottom-r.top;return createTarget();}
void Renderer::discardDeviceResources(){sceneTarget_.Reset();target_.Reset();active_=nullptr;animations_.clear();remapped_.clear();images_.clear();}
void Renderer::shutdown(){discardDeviceResources();write_.Reset();wic_.Reset();factory_.Reset();CoUninitialize();}
bool Renderer::createTarget(){if(!factory_)return false;auto size=D2D1::SizeU(std::max(1u,pixelW_),std::max(1u,pixelH_));auto hp=D2D1::HwndRenderTargetProperties(hwnd_,size,vsync_?D2D1_PRESENT_OPTIONS_NONE:D2D1_PRESENT_OPTIONS_IMMEDIATELY);HRESULT hr=factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),hp,target_.GetAddressOf());return SUCCEEDED(hr);}
bool Renderer::ensureSceneTarget(){if(sceneTarget_)return true;if(!target_&&!createTarget())return false;auto lw=(UINT)std::round(logicalWidth());auto lh=(UINT)std::round(logicalHeight());return SUCCEEDED(target_->CreateCompatibleRenderTarget(D2D1::SizeF(logicalWidth(),logicalHeight()),D2D1::SizeU(lw,lh),D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,sceneTarget_.GetAddressOf()));}
void Renderer::setPostSettings(int upscaleMode,int filterMode,int filterStrength,bool integerScale,bool widescreen,bool vsync){upscaleMode_=std::clamp(upscaleMode,0,2);filterMode_=std::clamp(filterMode,0,3);filterStrength_=std::clamp(filterStrength,0,100);integerScale_=integerScale;bool recreate=(vsync_!=vsync)||(wideAspect_!=widescreen);wideAspect_=widescreen;if(recreate){vsync_=vsync;discardDeviceResources();}else vsync_=vsync;}
void Renderer::resize(UINT w,UINT h){pixelW_=std::max(1u,w);pixelH_=std::max(1u,h);if(target_)target_->Resize(D2D1::SizeU(pixelW_,pixelH_));float lw=logicalWidth(),lh=logicalHeight();float sx=pixelW_/lw,sy=pixelH_/lh;scale_=std::min(sx,sy);offX_=(pixelW_-lw*scale_)*.5f;offY_=(pixelH_-lh*scale_)*.5f;}
bool Renderer::begin(bool postProcess){if(!target_&&!createTarget())return false;resize(pixelW_,pixelH_);postActive_=postProcess;if(postActive_){if(!ensureSceneTarget())return false;active_=sceneTarget_.Get();active_->BeginDraw();active_->SetTransform(D2D1::Matrix3x2F::Identity());}else{active_=target_.Get();active_->BeginDraw();active_->SetTransform(D2D1::Matrix3x2F::Scale(scale_,scale_)*D2D1::Matrix3x2F::Translation(offX_/scale_,offY_/scale_));}return true;}
void Renderer::compositeScene(){if(!target_||!sceneTarget_)return;ComPtr<ID2D1Bitmap> frame;if(FAILED(sceneTarget_->GetBitmap(frame.GetAddressOf()))||!frame)return;target_->BeginDraw();target_->SetTransform(D2D1::Matrix3x2F::Identity());target_->Clear(D2D1::ColorF(D2D1::ColorF::Black));float lw=logicalWidth(),lh=logicalHeight();float fit=std::min(pixelW_/lw,pixelH_/lh);float outScale=fit;if(integerScale_&&fit>=1.f)outScale=std::max(1.f,std::floor(fit));float dw=lw*outScale,dh=lh*outScale,left=snapPx((pixelW_-dw)*.5f),top=snapPx((pixelH_-dh)*.5f);D2D1_RECT_F dst=D2D1::RectF(left,top,left+dw,top+dh);float strength=filterStrength_/100.f;if(filterMode_>=2&&strength>0){float glow=.035f+.055f*strength;for(int ox=-1;ox<=1;++ox)for(int oy=-1;oy<=1;++oy)if(ox||oy)target_->DrawBitmap(frame.Get(),D2D1::RectF(left+ox,top+oy,left+dw+ox,top+dh+oy),glow,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);}
    if(upscaleMode_==0)target_->DrawBitmap(frame.Get(),dst,1.f,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);else if(upscaleMode_==1)target_->DrawBitmap(frame.Get(),dst,1.f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);else{target_->DrawBitmap(frame.Get(),dst,1.f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);target_->DrawBitmap(frame.Get(),dst,.22f+.28f*strength,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);}
    if(filterMode_==1||filterMode_==2||filterMode_==3){ComPtr<ID2D1SolidColorBrush> scan;target_->CreateSolidColorBrush(D2D1::ColorF(0,0,0),scan.GetAddressOf());scan->SetOpacity((filterMode_==1?.18f:.12f)+.24f*strength);float step=std::max(2.f,outScale*2.f),thick=std::max(1.f,outScale*.28f);for(float y=top+outScale;y<top+dh;y+=step)target_->FillRectangle(D2D1::RectF(left,y,left+dw,std::min(top+dh,y+thick)),scan.Get());}
    if((filterMode_==2||filterMode_==3)&&strength>0){D2D1_GRADIENT_STOP stops[3]={{0.f,D2D1::ColorF(0,0,0,0)},{.58f,D2D1::ColorF(0,0,0,0)},{1.f,D2D1::ColorF(0,0,0,.42f*strength)}};ComPtr<ID2D1GradientStopCollection> gs;target_->CreateGradientStopCollection(stops,3,gs.GetAddressOf());ComPtr<ID2D1RadialGradientBrush> vignette;target_->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(D2D1::Point2F(left+dw*.5f,top+dh*.5f),D2D1::Point2F(0,0),dw*.62f,dh*.72f),gs.Get(),vignette.GetAddressOf());target_->FillRectangle(dst,vignette.Get());}
    if(filterMode_==3&&strength>0){ComPtr<ID2D1SolidColorBrush>warm;target_->CreateSolidColorBrush(D2D1::ColorF(1.f,.48f,.10f),warm.GetAddressOf());warm->SetOpacity(.025f+.035f*strength);target_->FillRectangle(dst,warm.Get());}
    auto hr=target_->EndDraw();if(hr==D2DERR_RECREATE_TARGET)discardDeviceResources();}
void Renderer::end(){if(!active_)return;if(postActive_){auto hr=sceneTarget_->EndDraw();active_=nullptr;if(hr==D2DERR_RECREATE_TARGET){discardDeviceResources();postActive_=false;return;}compositeScene();postActive_=false;}else{auto hr=target_->EndDraw();active_=nullptr;if(hr==D2DERR_RECREATE_TARGET)discardDeviceResources();}}
void Renderer::clear(D2D1_COLOR_F c){if(active_)active_->Clear(c);}
void Renderer::fillScreen(D2D1_COLOR_F c,float opacity){
    if(!active_)return;D2D1_MATRIX_3X2_F old;active_->GetTransform(&old);active_->SetTransform(D2D1::Matrix3x2F::Identity());
    ComPtr<ID2D1SolidColorBrush>b;active_->CreateSolidColorBrush(c,b.GetAddressOf());b->SetOpacity(opacity);float aw=postActive_?logicalWidth():(float)pixelW_,ah=postActive_?logicalHeight():(float)pixelH_;active_->FillRectangle(D2D1::RectF(0,0,aw,ah),b.Get());active_->SetTransform(old);
}
void Renderer::drawImageCoverScreen(const std::string& rel,float opacity){
    auto im=image(rel);if(!active_||!im||!im->bmp||im->w<=0||im->h<=0)return;
    D2D1_MATRIX_3X2_F old;active_->GetTransform(&old);active_->SetTransform(D2D1::Matrix3x2F::Identity());
    float aw=postActive_?logicalWidth():(float)pixelW_,ah=postActive_?logicalHeight():(float)pixelH_;float s=std::max(aw/im->w,ah/im->h),dw=im->w*s,dh=im->h*s;float x=snapPx((aw-dw)*.5f),y=snapPx((ah-dh)*.5f);
    active_->DrawBitmap(im->bmp.Get(),D2D1::RectF(x,y,x+dw,y+dh),opacity,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);active_->SetTransform(old);
}
const ImageInfo* Renderer::image(const std::string& rel){
    auto it=images_.find(rel);if(it!=images_.end())return &it->second;
    auto path=root_/std::filesystem::path(rel);
    ComPtr<IWICBitmapDecoder> dec;ComPtr<IWICBitmapFrameDecode> frame;
    if(FAILED(wic_->CreateDecoderFromFilename(wide(path).c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&dec)))return nullptr;
    if(FAILED(dec->GetFrame(0,&frame)))return nullptr;
    RawWicImage raw;if(!decodeRawFrame(wic_.Get(),frame.Get(),raw))return nullptr;
    ImageInfo info;
    D2D1_BITMAP_PROPERTIES props=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
    if(FAILED(target_->CreateBitmap(D2D1::SizeU(raw.w,raw.h),raw.px.data(),raw.w*4,&props,info.bmp.GetAddressOf())))return nullptr;
    info.w=(float)raw.w;info.h=(float)raw.h;
    auto [ins,_]=images_.emplace(rel,std::move(info));return &ins->second;
}
static unsigned long metadataUInt(IWICMetadataQueryReader* qr,const wchar_t* name,unsigned long fallback=0){
    if(!qr)return fallback;PROPVARIANT pv{};unsigned long out=fallback;
    if(SUCCEEDED(qr->GetMetadataByName(name,&pv))){
        switch(pv.vt){
            case VT_UI1:out=pv.bVal;break;case VT_UI2:out=pv.uiVal;break;case VT_UI4:out=pv.ulVal;break;
            case VT_I1:out=(unsigned long)std::max<int>(0,pv.cVal);break;case VT_I2:out=(unsigned long)std::max<int>(0,pv.iVal);break;case VT_I4:out=(unsigned long)std::max<LONG>(0,pv.lVal);break;
            default:break;
        }
    }
    PropVariantClear(&pv);return out;
}
static void compositePbgra(std::vector<unsigned char>& dst,UINT dw,UINT dh,const RawWicImage& src,UINT left,UINT top){
    if(dst.empty()||src.px.empty()||left>=dw||top>=dh)return;
    UINT copyW=std::min(src.w,dw-left),copyH=std::min(src.h,dh-top);
    for(UINT y=0;y<copyH;++y)for(UINT x=0;x<copyW;++x){
        const auto* s=src.px.data()+((size_t)y*src.w+x)*4;auto* d=dst.data()+((size_t)(top+y)*dw+(left+x))*4;unsigned sa=s[3];
        if(sa==0)continue;if(sa==255){std::memcpy(d,s,4);continue;}unsigned inv=255-sa;
        d[0]=(unsigned char)std::min(255u,(unsigned)s[0]+((unsigned)d[0]*inv+127)/255);
        d[1]=(unsigned char)std::min(255u,(unsigned)s[1]+((unsigned)d[1]*inv+127)/255);
        d[2]=(unsigned char)std::min(255u,(unsigned)s[2]+((unsigned)d[2]*inv+127)/255);
        d[3]=(unsigned char)std::min(255u,sa+((unsigned)d[3]*inv+127)/255);
    }
}
const AnimatedImageInfo* Renderer::animatedImage(const std::string& rel){
    auto it=animations_.find(rel);if(it!=animations_.end())return &it->second;
    auto path=root_/std::filesystem::path(rel);ComPtr<IWICBitmapDecoder> dec;
    if(FAILED(wic_->CreateDecoderFromFilename(wide(path).c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&dec)))return nullptr;
    UINT count=0;if(FAILED(dec->GetFrameCount(&count))||count==0)return nullptr;AnimatedImageInfo info;
    UINT canvasW=0,canvasH=0;ComPtr<IWICMetadataQueryReader> dqr;if(SUCCEEDED(dec->GetMetadataQueryReader(&dqr))){canvasW=(UINT)metadataUInt(dqr.Get(),L"/logscrdesc/Width",0);canvasH=(UINT)metadataUInt(dqr.Get(),L"/logscrdesc/Height",0);}
    std::vector<unsigned char> canvas;
    for(UINT i=0;i<count;++i){
        ComPtr<IWICBitmapFrameDecode> frame;if(FAILED(dec->GetFrame(i,&frame)))continue;
        // Scene/cutscene GIFs use normal GIF transparency. Do not force palette index 0
        // transparent here; that OpenBOR rule is only for sprites/model art.
        RawWicImage raw;if(!decodeRawFrame(wic_.Get(),frame.Get(),raw,false))continue;
        ComPtr<IWICMetadataQueryReader> qr;frame->GetMetadataQueryReader(&qr);
        UINT left=(UINT)metadataUInt(qr.Get(),L"/imgdesc/Left",0),top=(UINT)metadataUInt(qr.Get(),L"/imgdesc/Top",0);
        UINT descW=(UINT)metadataUInt(qr.Get(),L"/imgdesc/Width",raw.w),descH=(UINT)metadataUInt(qr.Get(),L"/imgdesc/Height",raw.h);
        if(!canvasW)canvasW=std::max(raw.w,left+descW);if(!canvasH)canvasH=std::max(raw.h,top+descH);if(!canvasW||!canvasH)continue;
        if(canvas.empty())canvas.assign((size_t)canvasW*canvasH*4,0);
        // Some WIC codecs expose an already-expanded logical-screen frame. In that
        // case the frame pixels are already positioned, so ignore the GIF subrect.
        if(raw.w==canvasW&&raw.h==canvasH){left=0;top=0;}
        auto before=canvas;auto composed=canvas;compositePbgra(composed,canvasW,canvasH,raw,left,top);
        AnimatedFrameInfo af;af.w=(float)canvasW;af.h=(float)canvasH;
        unsigned long d=metadataUInt(qr.Get(),L"/grctlext/Delay",0);if(d)af.delay=std::max(.02f,d/100.f);
        D2D1_BITMAP_PROPERTIES props=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
        if(FAILED(target_->CreateBitmap(D2D1::SizeU(canvasW,canvasH),composed.data(),canvasW*4,&props,af.bmp.GetAddressOf())))continue;
        info.duration+=af.delay;info.frames.push_back(std::move(af));
        unsigned long disposal=metadataUInt(qr.Get(),L"/grctlext/Disposal",1);
        if(disposal==3)canvas=std::move(before);else{canvas=std::move(composed);if(disposal==2&&left<canvasW&&top<canvasH){UINT cw=std::min(raw.w,canvasW-left),ch=std::min(raw.h,canvasH-top);for(UINT y=0;y<ch;++y)std::memset(canvas.data()+((size_t)(top+y)*canvasW+left)*4,0,(size_t)cw*4);}}
    }
    if(info.frames.empty())return nullptr;if(info.duration<=0)info.duration=.1f*info.frames.size();
    auto [ins,_]=animations_.emplace(rel,std::move(info));return &ins->second;
}
float Renderer::animationDuration(const std::string& rel){auto a=animatedImage(rel);return a?a->duration:0.f;}
void Renderer::drawImage(const std::string& rel,float x,float y,bool flip,float opacity){auto im=image(rel);if(!im||!im->bmp)return;x=snapPx(x);y=snapPx(y);D2D1_RECT_F dst=D2D1::RectF(x,y,x+im->w,y+im->h);if(!flip){active_->DrawBitmap(im->bmp.Get(),dst,opacity,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);return;}D2D1_MATRIX_3X2_F old;active_->GetTransform(&old);float cx=x+im->w*.5f;auto local=D2D1::Matrix3x2F::Translation(-cx,0)*D2D1::Matrix3x2F::Scale(-1,1)*D2D1::Matrix3x2F::Translation(cx,0);active_->SetTransform(local*old);active_->DrawBitmap(im->bmp.Get(),dst,opacity,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);active_->SetTransform(old);}
void Renderer::drawImageScaled(const std::string& rel,float x,float y,float w,float h,bool flip,float opacity){auto im=image(rel);if(!im||!im->bmp||w<=0||h<=0)return;x=snapPx(x);y=snapPx(y);w=snapPx(w);h=snapPx(h);D2D1_RECT_F dst=D2D1::RectF(x,y,x+w,y+h);if(!flip){active_->DrawBitmap(im->bmp.Get(),dst,opacity,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);return;}D2D1_MATRIX_3X2_F old;active_->GetTransform(&old);float cx=x+w*.5f;auto local=D2D1::Matrix3x2F::Translation(-cx,0)*D2D1::Matrix3x2F::Scale(-1,1)*D2D1::Matrix3x2F::Translation(cx,0);active_->SetTransform(local*old);active_->DrawBitmap(im->bmp.Get(),dst,opacity,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);active_->SetTransform(old);}
void Renderer::drawImageRemapped(const std::string& rel,const std::string& basePalette,const std::string& altPalette,float x,float y,bool flip,float opacity){
    if(basePalette.empty()||altPalette.empty()){drawImage(rel,x,y,flip,opacity);return;}std::string key=rel+"|"+basePalette+"|"+altPalette;const ImageInfo* im=nullptr;auto it=remapped_.find(key);if(it!=remapped_.end())im=&it->second;else{RawWicImage src,base,alt;if(!decodeRaw(wic_.Get(),root_/std::filesystem::path(rel),src)||!decodeRaw(wic_.Get(),root_/std::filesystem::path(basePalette),base)||!decodeRaw(wic_.Get(),root_/std::filesystem::path(altPalette),alt)||base.w!=alt.w||base.h!=alt.h){drawImage(rel,x,y,flip,opacity);return;}std::unordered_map<uint32_t,uint32_t> map;size_t rn=std::min(base.px.size(),alt.px.size())/4;for(size_t i=0;i<rn;++i){uint32_t a=0,b=0;std::memcpy(&a,base.px.data()+i*4,4);std::memcpy(&b,alt.px.data()+i*4,4);map[a]=b;}size_t sn=src.px.size()/4;for(size_t i=0;i<sn;++i){uint32_t c=0;std::memcpy(&c,src.px.data()+i*4,4);auto mi=map.find(c);if(mi!=map.end())std::memcpy(src.px.data()+i*4,&mi->second,4);}ImageInfo made;D2D1_BITMAP_PROPERTIES props=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));if(FAILED(target_->CreateBitmap(D2D1::SizeU(src.w,src.h),src.px.data(),src.w*4,&props,made.bmp.GetAddressOf()))){drawImage(rel,x,y,flip,opacity);return;}made.w=(float)src.w;made.h=(float)src.h;auto [ins,_]=remapped_.emplace(key,std::move(made));im=&ins->second;}if(!im||!im->bmp)return;x=snapPx(x);y=snapPx(y);D2D1_RECT_F dst=D2D1::RectF(x,y,x+im->w,y+im->h);if(!flip){active_->DrawBitmap(im->bmp.Get(),dst,opacity,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);return;}D2D1_MATRIX_3X2_F old;active_->GetTransform(&old);float cx=x+im->w*.5f;auto local=D2D1::Matrix3x2F::Translation(-cx,0)*D2D1::Matrix3x2F::Scale(-1,1)*D2D1::Matrix3x2F::Translation(cx,0);active_->SetTransform(local*old);active_->DrawBitmap(im->bmp.Get(),dst,opacity,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);active_->SetTransform(old);
}
void Renderer::drawAnimatedImage(const std::string& rel,float elapsed,float x,float y,bool loop,float opacity){auto a=animatedImage(rel);if(!a||a->frames.empty())return;float t=elapsed;if(loop&&a->duration>0)t=std::fmod(std::max(0.f,t),a->duration);else t=std::min(std::max(0.f,t),std::max(0.f,a->duration-.001f));size_t idx=0;float acc=0;for(size_t i=0;i<a->frames.size();++i){acc+=a->frames[i].delay;if(t<acc){idx=i;break;}idx=i;}auto&f=a->frames[idx];if(!f.bmp)return;x=snapPx(x);y=snapPx(y);active_->DrawBitmap(f.bmp.Get(),D2D1::RectF(x,y,x+f.w,y+f.h),opacity,D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);}
void Renderer::drawImageTiledX(const std::string& rel,float cameraX,float parallax,float y){auto im=image(rel);if(!im||im->w<=0)return;float start=-std::fmod(cameraX*parallax,im->w);if(start>0)start-=im->w;for(float x=start;x<logicalWidth();x+=im->w)drawImage(rel,x,y);}
void Renderer::fillRect(float x,float y,float w,float h,D2D1_COLOR_F c,float opacity){ComPtr<ID2D1SolidColorBrush>b;active_->CreateSolidColorBrush(c,b.GetAddressOf());b->SetOpacity(opacity);x=snapPx(x);y=snapPx(y);w=snapPx(w);h=snapPx(h);active_->FillRectangle(D2D1::RectF(x,y,x+w,y+h),b.Get());}
void Renderer::text(const std::wstring&s,float x,float y,float size,D2D1_COLOR_F c,bool center){ComPtr<IDWriteTextFormat>fmt;write_->CreateTextFormat(L"Bahnschrift SemiCondensed",nullptr,DWRITE_FONT_WEIGHT_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"pt-br",&fmt);if(center)fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);ComPtr<ID2D1SolidColorBrush>b;active_->CreateSolidColorBrush(c,b.GetAddressOf());active_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_ALIASED);x=snapPx(x);y=snapPx(y);auto rect=center?D2D1::RectF(0,y,logicalWidth(),y+size*2.f):D2D1::RectF(x,y,logicalWidth(),y+size*2.f);active_->DrawText(s.c_str(),(UINT32)s.size(),fmt.Get(),rect,b.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);}
}
#endif
