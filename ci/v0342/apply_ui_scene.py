from pathlib import Path
root=Path.cwd()

def rep(rel,old,new):
    p=root/rel
    s=p.read_text(encoding="utf-8-sig")
    if old not in s:
        raise SystemExit("v0.3.42 anchor missing: "+rel+" :: "+old[:140])
    p.write_text(s.replace(old,new,1),encoding="utf-8")

rep("CMakeLists.txt","project(FinalFightXNative VERSION 0.3.41 LANGUAGES CXX)","project(FinalFightXNative VERSION 0.3.42 LANGUAGES CXX)")
rep("CMakeLists.txt",
'''add_executable(v0341_offscreenkill_validator tests/v0341_offscreenkill_validator.cpp)
target_link_libraries(v0341_offscreenkill_validator PRIVATE ffx_data)
''',
'''add_executable(v0341_offscreenkill_validator tests/v0341_offscreenkill_validator.cpp)
target_link_libraries(v0341_offscreenkill_validator PRIVATE ffx_data)

add_executable(v0342_ui_scene_validator tests/v0342_ui_scene_validator.cpp)
target_link_libraries(v0342_ui_scene_validator PRIVATE ffx_data)
''')
rep("src/main_win.cpp","Native C++ Port v0.3.41","Native C++ Port v0.3.42")
rep("src/Game.cpp","NATIVE PORT v0.3.41","NATIVE PORT v0.3.42")

p=root/"src/OpenBorSemantics.h"
s=p.read_text(encoding="utf-8-sig")
anchor="inline int openBorEffectiveOffscreenKill(int configured){"
helper='''inline float openBorLegacyUiOffsetX(float logicalWidth){
    return std::max(0.f,(logicalWidth-320.f)*.5f);
}

inline float openBorSceneFadeOpacity(float elapsed,float duration,float fadeSeconds=.10f){
    fadeSeconds=std::max(.001f,fadeSeconds);
    const float in=std::clamp(elapsed/fadeSeconds,0.f,1.f);
    if(duration<=fadeSeconds)return in;
    const float out=std::clamp((duration-elapsed)/fadeSeconds,0.f,1.f);
    return std::min(in,out);
}

'''
if anchor not in s: raise SystemExit("v0.3.42 semantic anchor missing")
p.write_text(s.replace(anchor,helper+anchor,1),encoding="utf-8")

p=root/"src/Renderer.h"
s=p.read_text(encoding="utf-8-sig")
s=s.replace('    float animationDuration(const std::string& rel);\n','    float animationDuration(const std::string& rel);\n    bool preloadAnimation(const std::string& rel){return animatedImage(rel)!=nullptr;}\n',1)
s=s.replace('    void text(const std::wstring& s,float x,float y,float size,D2D1_COLOR_F c,bool center=false);\n','    void text(const std::wstring& s,float x,float y,float size,D2D1_COLOR_F c,bool center=false);\n    void textCenteredAt(const std::wstring& s,float centerX,float y,float width,float size,D2D1_COLOR_F c);\n',1)
s=s.replace('    std::unordered_map<std::string,AnimatedImageInfo> animations_;\n','    std::unordered_map<std::string,AnimatedImageInfo> animations_;\n    std::unordered_map<int,Microsoft::WRL::ComPtr<IDWriteTextFormat>> textFormats_;\n',1)
p.write_text(s,encoding="utf-8")

p=root/"src/Renderer.cpp"
s=p.read_text(encoding="utf-8-sig")
s=s.replace('''const ImageInfo* Renderer::image(const std::string& rel){
    auto it=images_.find(rel);''','''const ImageInfo* Renderer::image(const std::string& rel){
    if(!target_&&!createTarget())return nullptr;auto it=images_.find(rel);''',1)
s=s.replace('''const AnimatedImageInfo* Renderer::animatedImage(const std::string& rel){
    auto it=animations_.find(rel);''','''const AnimatedImageInfo* Renderer::animatedImage(const std::string& rel){
    if(!target_&&!createTarget())return nullptr;auto it=animations_.find(rel);''',1)
old='''void Renderer::text(const std::wstring&s,float x,float y,float size,D2D1_COLOR_F c,bool center){ComPtr<IDWriteTextFormat>fmt;write_->CreateTextFormat(L"Bahnschrift SemiCondensed",nullptr,DWRITE_FONT_WEIGHT_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"pt-br",&fmt);if(center)fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);ComPtr<ID2D1SolidColorBrush>b;active_->CreateSolidColorBrush(c,b.GetAddressOf());active_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_ALIASED);x=snapPx(x);y=snapPx(y);auto rect=center?D2D1::RectF(0,y,logicalWidth(),y+size*2.f):D2D1::RectF(x,y,logicalWidth(),y+size*2.f);active_->DrawText(s.c_str(),(UINT32)s.size(),fmt.Get(),rect,b.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);}
'''
new='''static int textFormatKey(float size,bool center){return ((int)std::lround(size*20.f)<<1)|(center?1:0);}
static IDWriteTextFormat* cachedTextFormat(IDWriteFactory* write,std::unordered_map<int,ComPtr<IDWriteTextFormat>>& cache,float size,bool center){
    const int key=textFormatKey(size,center);auto it=cache.find(key);if(it!=cache.end())return it->second.Get();
    ComPtr<IDWriteTextFormat> fmt;if(FAILED(write->CreateTextFormat(L"Bahnschrift SemiCondensed",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"pt-br",&fmt)))return nullptr;
    fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);if(center)fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    auto [ins,_]=cache.emplace(key,std::move(fmt));return ins->second.Get();
}
void Renderer::text(const std::wstring&s,float x,float y,float size,D2D1_COLOR_F c,bool center){
    if(!active_||s.empty())return;auto* fmt=cachedTextFormat(write_.Get(),textFormats_,size,center);if(!fmt)return;ComPtr<ID2D1SolidColorBrush>b;active_->CreateSolidColorBrush(c,b.GetAddressOf());
    active_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);x=snapPx(x);y=snapPx(y);auto rect=center?D2D1::RectF(0,y,logicalWidth(),y+size*2.25f):D2D1::RectF(x,y,logicalWidth(),y+size*2.25f);active_->DrawText(s.c_str(),(UINT32)s.size(),fmt,rect,b.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void Renderer::textCenteredAt(const std::wstring&s,float centerX,float y,float width,float size,D2D1_COLOR_F c){
    if(!active_||s.empty()||width<=0)return;auto* fmt=cachedTextFormat(write_.Get(),textFormats_,size,true);if(!fmt)return;ComPtr<ID2D1SolidColorBrush>b;active_->CreateSolidColorBrush(c,b.GetAddressOf());active_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    y=snapPx(y);float left=snapPx(centerX-width*.5f);active_->DrawText(s.c_str(),(UINT32)s.size(),fmt,D2D1::RectF(left,y,left+snapPx(width),y+size*2.25f),b.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
'''
if old not in s: raise SystemExit("v0.3.42 renderer text anchor missing")
p.write_text(s.replace(old,new,1),encoding="utf-8")

p=root/"src/Game.cpp"
s=p.read_text(encoding="utf-8-sig")
subs=[
('''void Game::drawMainMenu(){
    float pulse=.5f+.5f*std::sin(totalTime_*4.2f),bob=std::sin(totalTime_*2.1f)*.7f;
    renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.18f);renderer_.drawImage("bgs/title.gif",0,-2,false,.26f);
    renderer_.fillRect(44,61,232,173,D2D1::ColorF(0,0,0),.83f);renderer_.fillRect(44,61,232,2,D2D1::ColorF(1,.58f,.06f),.72f+.20f*pulse);
''','''void Game::drawMainMenu(){
    float pulse=.5f+.5f*std::sin(totalTime_*4.2f),bob=std::sin(totalTime_*2.1f)*.7f;const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());
    renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.18f);renderer_.drawImage("bgs/title.gif",ux,-2,false,.26f);
    renderer_.fillRect(ux+44,61,232,173,D2D1::ColorF(0,0,0),.83f);renderer_.fillRect(ux+44,61,232,2,D2D1::ColorF(1,.58f,.06f),.72f+.20f*pulse);
'''),
('renderer_.fillRect(69-grow,y-1,182+grow*2,17','renderer_.fillRect(ux+69-grow,y-1,182+grow*2,17'),
('renderer_.fillRect(69-grow,y-1,3,17','renderer_.fillRect(ux+69-grow,y-1,3,17'),
('renderer_.fillRect(248+grow,y-1,3,17','renderer_.fillRect(ux+248+grow,y-1,3,17'),
('renderer_.fillRect(44,220,232,1','renderer_.fillRect(ux+44,220,232,1'),
('renderer_.text(L"NATIVE PORT v0.3.42",246,232','renderer_.text(L"NATIVE PORT v0.3.42",ux+246,232'),
('''void Game::drawOptions(){
    if(optionsReturnMode_==Mode::Pause){drawGameplay();renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(0,0,0),.72f);}else{renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.52f);renderer_.drawImage("bgs/title.gif",0,-2,false,.18f);}
    renderer_.fillRect(38,24,244,204,D2D1::ColorF(0,0,0),.90f);renderer_.fillRect(38,24,244,2,D2D1::ColorF(1,.62f,.08f),.9f);renderer_.text(L"OPCOES",0,32,14,D2D1::ColorF(1,.82f,.18f),true);
''','''void Game::drawOptions(){
    const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());if(optionsReturnMode_==Mode::Pause){drawGameplay();renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(0,0,0),.72f);}else{renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.52f);renderer_.drawImage("bgs/title.gif",ux,-2,false,.18f);}
    renderer_.fillRect(ux+38,24,244,204,D2D1::ColorF(0,0,0),.90f);renderer_.fillRect(ux+38,24,244,2,D2D1::ColorF(1,.62f,.08f),.9f);renderer_.text(L"OPCOES",0,32,14,D2D1::ColorF(1,.82f,.18f),true);
'''),
('renderer_.fillRect(56,y-2,208,18','renderer_.fillRect(ux+56,y-2,208,18'),
('renderer_.fillRect(56,y-2,3,18','renderer_.fillRect(ux+56,y-2,3,18'),
('''void Game::drawGraphics(){
    renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.66f);renderer_.fillRect(30,18,260,212,D2D1::ColorF(.01f,.01f,.015f),.94f);renderer_.fillRect(30,18,260,2,D2D1::ColorF(.2f,.78f,1.f),.95f);renderer_.text(L"GRAFICOS GPU",0,27,14,D2D1::ColorF(.35f,.86f,1.f),true);
''','''void Game::drawGraphics(){
    const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.66f);renderer_.drawImage("bgs/title.gif",ux,-2,false,.10f);renderer_.fillRect(ux+30,18,260,212,D2D1::ColorF(.01f,.01f,.015f),.94f);renderer_.fillRect(ux+30,18,260,2,D2D1::ColorF(.2f,.78f,1.f),.95f);renderer_.text(L"GRAFICOS GPU",0,27,14,D2D1::ColorF(.35f,.86f,1.f),true);
'''),
('renderer_.fillRect(45,y-2,230,18','renderer_.fillRect(ux+45,y-2,230,18'),
('renderer_.fillRect(45,y-2,3,18','renderer_.fillRect(ux+45,y-2,3,18'),
('''void Game::drawControls(){
    if(optionsReturnMode_==Mode::Pause){drawGameplay();renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(0,0,0),.76f);}else{renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.60f);renderer_.drawImage("bgs/title.gif",0,-2,false,.14f);}
    renderer_.fillRect(26,18,268,210,D2D1::ColorF(0,0,0),.92f);
''','''void Game::drawControls(){
    const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());if(optionsReturnMode_==Mode::Pause){drawGameplay();renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(0,0,0),.76f);}else{renderer_.drawImageCoverScreen("bgs/title.gif");renderer_.fillScreen(D2D1::ColorF(0,0,0),.60f);renderer_.drawImage("bgs/title.gif",ux,-2,false,.14f);}
    renderer_.fillRect(ux+26,18,268,210,D2D1::ColorF(0,0,0),.92f);
'''),
('renderer_.fillRect(45,56+i*22,230,17','renderer_.fillRect(ux+45,56+i*22,230,17'),
('renderer_.fillRect(39,y-1,242,13','renderer_.fillRect(ux+39,y-1,242,13'),
('renderer_.text(Input::actionName((InputAction)i),48,y','renderer_.text(Input::actionName((InputAction)i),ux+48,y'),
('renderer_.text(val,184,y','renderer_.text(val,ux+184,y'),
('renderer_.fillRect(47,188,226,26','renderer_.fillRect(ux+47,188,226,26'),
('''void Game::drawPauseOverlay(){
    renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(0,0,0),.66f);renderer_.fillRect(67,52,186,137,D2D1::ColorF(0,0,0),.86f);renderer_.text(L"PAUSADO",0,60,16,D2D1::ColorF(1,.82f,.18f),true);
''','''void Game::drawPauseOverlay(){
    const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());renderer_.fillRect(0,0,renderer_.logicalWidth(),240,D2D1::ColorF(0,0,0),.66f);renderer_.fillRect(ux+67,52,186,137,D2D1::ColorF(0,0,0),.86f);renderer_.text(L"PAUSADO",0,60,16,D2D1::ColorF(1,.82f,.18f),true);
'''),
('renderer_.fillRect(82,96+i*22,156,17','renderer_.fillRect(ux+82,96+i*22,156,17'),
('''void Game::startSceneQueue(const std::vector<std::string>&files,Mode after,int nextStage){sceneFiles_=files;sceneFileIndex_=sceneStep_=0;sceneStepTime_=0;sceneX_=sceneY_=0;sceneSkip_=sceneNoSkip_=false;sceneNextStage_=nextStage;sceneVisual_.clear();sceneAfterMode_=after;mode_=Mode::Scene;loadNextSceneFile();}
''','''void Game::startSceneQueue(const std::vector<std::string>&files,Mode after,int nextStage){sceneFiles_=files;sceneFileIndex_=sceneStep_=0;sceneStepTime_=0;sceneX_=sceneY_=0;sceneSkip_=sceneNoSkip_=false;sceneNextStage_=nextStage;sceneVisual_.clear();sceneAfterMode_=after;for(const auto& file:sceneFiles_){auto warm=db_.loadScene(file);for(const auto& st:warm.steps)if(st.kind==SceneStep::Kind::Animation&&!st.asset.empty())renderer_.preloadAnimation(st.asset);}mode_=Mode::Scene;loadNextSceneFile();}
'''),
('''void Game::drawHud(){
    const auto& hud=db_.campaign().hud;
''','''void Game::drawHud(){
    const auto& hud=db_.campaign().hud;const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());
'''),
('    auto visible=[&](const HudPoint& p){return p.x>=0&&p.y>=0&&p.x<(int)renderer_.logicalWidth()&&p.y<240;};','    auto visible=[&](const HudPoint& p){return p.x>=0&&p.y>=0&&p.x<320&&p.y<240;};'),
('const float x=(float)pos.x,y=(float)pos.y,w=(float)bw,h=(float)bh;','const float x=ux+(float)pos.x,y=(float)pos.y,w=(float)bw,h=(float)bh;'),
('auto hudText=[&](const std::wstring& text,const HudPoint& p,float size=6.f){if(visible(p)&&!text.empty())renderer_.text(text,(float)p.x,(float)p.y,size,D2D1::ColorF(1,1,1));};','auto hudText=[&](const std::wstring& text,const HudPoint& p,float size=6.f){if(visible(p)&&!text.empty())renderer_.text(text,ux+(float)p.x,(float)p.y,size,D2D1::ColorF(1,1,1));};'),
('renderer_.drawImage(p->def->icon,(float)h.icon.x,(float)h.icon.y)','renderer_.drawImage(p->def->icon,ux+(float)h.icon.x,(float)h.icon.y)'),
('renderer_.drawImage(target->def->icon,(float)h.enemyIcon.x,(float)h.enemyIcon.y)','renderer_.drawImage(target->def->icon,ux+(float)h.enemyIcon.x,(float)h.enemyIcon.y)'),
('renderer_.drawImage(hud.timeIcon,(float)hud.timeIconX,(float)hud.timeIconY)','renderer_.drawImage(hud.timeIcon,ux+(float)hud.timeIconX,(float)hud.timeIconY)'),
('''void Game::drawSelect(){
    // Dynamic character roulette.''','''void Game::drawSelect(){
    const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());
    // Dynamic character roulette.'''),
('float x=8.f+(float)i*104.f;','float x=ux+8.f+(float)i*104.f;'),
('float sx=spriteLeftFromOrigin((float)sm.character.x,fr.offsetX,im->w,previewLeft),sy=(float)sm.character.y-fr.offsetY;','float sx=spriteLeftFromOrigin(ux+(float)sm.character.x,fr.offsetX,im->w,previewLeft),sy=(float)sm.character.y-fr.offsetY;'),
('auto visiblePoint=[&](const HudPoint& p){return p.x>=0&&p.y>=0&&p.x<(int)renderer_.logicalWidth()&&p.y<240;};','auto visiblePoint=[&](const HudPoint& p){return p.x>=0&&p.y>=0&&p.x<320&&p.y<240;};'),
('renderer_.text(L"SELECT HERO",(float)hc.join.select.x,(float)hc.join.select.y','renderer_.text(L"SELECT HERO",ux+(float)hc.join.select.x,(float)hc.join.select.y'),
('renderer_.text(nm,(float)hc.join.name.x,(float)hc.join.name.y','renderer_.text(nm,ux+(float)hc.join.name.x,(float)hc.join.name.y'),
('renderer_.text(L"READY!",(float)hc.selectMenu.ready.x,(float)hc.selectMenu.ready.y','renderer_.text(L"READY!",ux+(float)hc.selectMenu.ready.x,(float)hc.selectMenu.ready.y'),
('renderer_.text(L"START",x+4,112,9,D2D1::ColorF(.65f,.65f,.65f),true);','renderer_.textCenteredAt(L"START",x+48,112,86,9,D2D1::ColorF(.65f,.65f,.65f));'),
('join.prompt.x<(int)renderer_.logicalWidth()','join.prompt.x<320'),
('renderer_.text(L"PRESS START",(float)join.prompt.x,(float)join.prompt.y','renderer_.text(L"PRESS START",ux+(float)join.prompt.x,(float)join.prompt.y'),
('''void Game::drawLoading(){
    const auto& hud=db_.campaign().hud;size_t which=stageIndex_==0?0:1;const auto& b=hud.loading[which];
    std::string bg=which==0?"bgs/loading.gif":"bgs/loading2.gif";renderer_.drawImageCoverScreen(bg,.78f);renderer_.drawImage(bg,0,0);
    float progress=clampf(1.f-loadingTime_/.45f,0.f,1.f);if(b.mode==1||b.mode==3){renderer_.fillRect((float)b.bx,(float)b.by,(float)b.bsize,5,D2D1::ColorF(.12f,.12f,.12f),.9f);renderer_.fillRect((float)b.bx,(float)b.by,b.bsize*progress,5,D2D1::ColorF(.9f,.2f,.08f),.95f);}if(b.tx<renderer_.logicalWidth()&&b.ty<240)renderer_.text(L"LOADING...",(float)b.tx,(float)b.ty,7,D2D1::ColorF(D2D1::ColorF::White));
}
''','''void Game::drawLoading(){
    const auto& hud=db_.campaign().hud;size_t which=stageIndex_==0?0:1;const auto& b=hud.loading[which];const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());
    std::string bg=which==0?"bgs/loading.gif":"bgs/loading2.gif";renderer_.drawImageCoverScreen(bg,.62f);renderer_.drawImage(bg,ux,0);
    float progress=clampf(1.f-loadingTime_/.45f,0.f,1.f);if(b.mode==1||b.mode==3){renderer_.fillRect(ux+(float)b.bx,(float)b.by,(float)b.bsize,5,D2D1::ColorF(.12f,.12f,.12f),.9f);renderer_.fillRect(ux+(float)b.bx,(float)b.by,b.bsize*progress,5,D2D1::ColorF(.9f,.2f,.08f),.95f);}if(b.tx<320&&b.ty<240)renderer_.text(L"LOADING...",ux+(float)b.tx,(float)b.ty,7,D2D1::ColorF(D2D1::ColorF::White));
}
'''),
('    if(mode_==Mode::Scene){if(!sceneVisual_.empty())renderer_.drawAnimatedImage(sceneVisual_,sceneStepTime_,(float)sceneX_,(float)sceneY_,false);renderer_.end();return;}','    if(mode_==Mode::Scene){if(!sceneVisual_.empty()){float d=renderer_.animationDuration(sceneVisual_);if(d<=.11f)d=1.5f;float alpha=openBorSceneFadeOpacity(sceneStepTime_,d);renderer_.drawAnimatedImage(sceneVisual_,sceneStepTime_,openBorLegacyUiOffsetX(renderer_.logicalWidth())+(float)sceneX_,(float)sceneY_,false,alpha);}renderer_.end();return;}')
]
for old,new in subs:
    if old not in s: raise SystemExit("v0.3.42 game anchor missing: "+old[:120])
    s=s.replace(old,new,1)
p.write_text(s,encoding="utf-8")

(root/"tests/v0342_ui_scene_validator.cpp").write_text(r'''#include "OpenBorData.h"
#include "OpenBorSemantics.h"
#include <cmath>
#include <iostream>
using namespace ffx;
static bool near(float a,float b,float e=.01f){return std::fabs(a-b)<=e;}
int main(int argc,char**argv){
    int bad=0;auto check=[&](bool ok,const char*msg){if(!ok){++bad;std::cerr<<"FAIL: "<<msg<<"\n";}};
    check(near(openBorLegacyUiOffsetX(320.f),0.f),"4:3 UI offset");
    check(near(openBorLegacyUiOffsetX(426.f),53.f),"16:9 UI offset must center 320px authored UI");
    check(near(openBorLegacyUiOffsetX(640.f),160.f),"wide UI offset");
    check(near(openBorSceneFadeOpacity(0.f,1.f),0.f),"scene starts faded out");
    check(openBorSceneFadeOpacity(.10f,1.f)>.99f,"scene reaches full opacity after fade-in");
    check(openBorSceneFadeOpacity(.95f,1.f)<.51f,"scene fades out before transition");
    if(argc>1){OpenBorDatabase db;std::string err;check(db.load(argv[1],&err),"database load");if(err.empty()){
        auto logo=db.loadScene("scenes/logo.txt");int anims=0;for(const auto&s:logo.steps)if(s.kind==SceneStep::Kind::Animation)++anims;check(anims==4,"logo scene animation count");
        check(db.campaign().hud.loading[0].bx==55&&db.campaign().hud.loading[0].bsize==200,"loading bar authored layout");
    }}
    if(bad){std::cerr<<"v0342_ui_scene_failures="<<bad<<"\n";return 3;}
    std::cout<<"v0342_ui_scene=OK widescreen_offset=53 scene_fade=100ms\n";return 0;
}
''',encoding="utf-8")

for name in ["README-SOURCE-v0.3.41.txt","BUILD-VALIDATION-v0.3.41.txt","PAK-FIDELITY-AUDIT-v0.3.41.txt","CHANGELOG-v0.3.41.txt"]:
    try:(root/name).unlink()
    except FileNotFoundError:pass
(root/"README-SOURCE-v0.3.42.txt").write_text("""Final Fight X Native v0.3.42 - Source

Focus: fullscreen/widescreen UI centering, scene preload/fades, text rendering cache and readability.

Windows x64:
  cmake -S . -B build -A x64
  cmake --build build --config Release --target FinalFightX v0342_ui_scene_validator -- /m

Place FinalFightX.exe beside the assets folder from the portable package.
""",encoding="utf-8")
(root/"CHANGELOG-v0.3.42.txt").write_text("""Final Fight X Native - v0.3.42

- Centers 320px-authored menus, pause UI, select cards, HUD, loading UI and cutscenes inside 16:9/fullscreen layouts.
- Fixes SELECT START text using whole-screen centering instead of card-local centering.
- Preloads scene GIF animations at scene-queue start to reduce transition hitching.
- Adds short 100ms scene fade-in/fade-out to hide hard visual cuts.
- Caches DirectWrite text formats instead of recreating them for every string every frame.
- Switches UI text to grayscale antialiasing and no-wrap for cleaner, more stable rendering.
- Keeps legacy 4:3 coordinates unchanged at 320x240.
""",encoding="utf-8")
(root/"PAK-FIDELITY-AUDIT-v0.3.42.txt").write_text("""Final Fight X Native v0.3.42 - UI/Scene Audit

PAK facts:
- Cutscene GIFs are authored at 320x244 and scene coordinates are legacy 320px-space values.
- levels.txt loading bar 1 is authored at x=55, y=200, width=200.
- Main HUD/select coordinates are authored against the 320px OpenBOR layout.

Fix:
- In a 426px 16:9 logical surface, the legacy UI receives a +53px horizontal offset.
- At 320px logical width the offset is exactly zero, preserving 4:3 behavior.
""",encoding="utf-8")
(root/"BUILD-VALIDATION-v0.3.42.txt").write_text("""Final Fight X Native v0.3.42 - Build Validation

Adds v0342_ui_scene_validator for 4:3/16:9 centering and scene fade semantics.
Existing native regressions must remain green.
No claim is made of a full interactive Windows campaign playthrough.
""",encoding="utf-8")
print("v0.3.42 UI/scene patch applied")
