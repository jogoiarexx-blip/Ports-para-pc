from pathlib import Path
import sys
root=Path(sys.argv[1])
ph=root/'src/Game.h'; pc=root/'src/Game.cpp'
s=ph.read_text(encoding='utf-8-sig')
s=s.replace('#ifdef _WIN32\n#include "Audio.h"\n#include "Input.h"\n#include "OpenBorData.h"\n#include "OpenBorSemantics.h"\n#include "Renderer.h"', '#if defined(_WIN32) || defined(__EMSCRIPTEN__)\n#if defined(__EMSCRIPTEN__)\n#include "PlatformCompat.h"\n#include "Audio_web.h"\n#include "Input_web.h"\n#include "Renderer_web.h"\n#else\n#include "Audio.h"\n#include "Input.h"\n#include "Renderer.h"\n#endif\n#include "OpenBorData.h"\n#include "OpenBorSemantics.h"')

# Add read-only gameplay telemetry used by the web regression harness.
h_anchor='''    void resize(UINT w,UINT h);
    void toggleFullscreen();
private:'''
h_repl='''    void resize(UINT w,UINT h);
    void toggleFullscreen();
#if defined(__EMSCRIPTEN__)
    int webDebugMode() const;
    int webDebugStageIndex() const;
    float webDebugCameraX() const;
    float webDebugScrollProgress() const;
    int webDebugActorCount() const;
    int webDebugActiveEnemies() const;
    float webDebugPlayerX() const;
    int webDebugPlayerHp() const;
    int webDebugNextSpawn() const;
    int webDebugSpawnCount() const;
#endif
private:'''
if h_anchor not in s: raise SystemExit('Game.h web debug anchor missing')
s=s.replace(h_anchor,h_repl,1)

ph.write_text(s,encoding='utf-8')

s=pc.read_text(encoding='utf-8-sig')
s=s.replace('#ifdef _WIN32\n#include "Game.h"', '#if defined(_WIN32) || defined(__EMSCRIPTEN__)\n#include "Game.h"',1)
anchor='#include <fstream>\n'
insert='#include <fstream>\n#if defined(__EMSCRIPTEN__)\n#include <emscripten.h>\nEM_JS(void, ffx_web_fullscreen, (int enabled), { if(globalThis.FFXWeb) FFXWeb.setFullscreen(!!enabled); });\nEM_JS(void, ffx_web_exit, (), { if(globalThis.FFXWeb) FFXWeb.requestExit(); });\n#endif\n'
if anchor not in s: raise SystemExit('fstream anchor missing')
s=s.replace(anchor,insert,1)
old='loadSettings();applyAudioSettings();applyGraphicsSettings();bool wantFullscreen=fullscreen_;fullscreen_=false;if(wantFullscreen)setFullscreen(true);else applyWindowScale();'
new='''loadSettings();applyAudioSettings();applyGraphicsSettings();bool wantFullscreen=fullscreen_;fullscreen_=false;
#if defined(_WIN32)
    if(wantFullscreen)setFullscreen(true);else applyWindowScale();
#else
    (void)wantFullscreen;applyWindowScale();
#endif'''
if old not in s: raise SystemExit('init fullscreen anchor missing')
s=s.replace(old,new,1)
start=s.index('void Game::applyWindowScale(){'); end=s.index('void Game::toggleFullscreen()',start)
newblock='''void Game::applyWindowScale(){
#if defined(_WIN32)
    if(!hwnd_||fullscreen_)return;windowScale_=std::clamp(windowScale_,2,4);
    DWORD style=(DWORD)GetWindowLongPtr(hwnd_,GWL_STYLE);int logicalW=widescreen_?426:320;RECT r{0,0,logicalW*windowScale_,240*windowScale_};UINT dpi=GetDpiForWindow(hwnd_);AdjustWindowRectExForDpi(&r,style,FALSE,0,dpi);
    int ww=r.right-r.left,wh=r.bottom-r.top;MONITORINFO mi{sizeof(mi)};GetMonitorInfo(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&mi);
    int x=mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-ww)/2,y=mi.rcWork.top+(mi.rcWork.bottom-mi.rcWork.top-wh)/2;
    SetWindowPos(hwnd_,nullptr,x,y,ww,wh,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED|SWP_SHOWWINDOW);
    windowedPlacement_.length=sizeof(WINDOWPLACEMENT);GetWindowPlacement(hwnd_,&windowedPlacement_);windowedStyle_=GetWindowLongPtr(hwnd_,GWL_STYLE);haveWindowedPlacement_=true;
#else
    renderer_.resize((UINT)((widescreen_?426:320)*windowScale_),(UINT)(240*windowScale_));
#endif
}
void Game::setFullscreen(bool enabled){
#if defined(_WIN32)
    if(!hwnd_)return;if(enabled==fullscreen_&&(!enabled||haveWindowedPlacement_))return;
    if(enabled){windowedStyle_=GetWindowLongPtr(hwnd_,GWL_STYLE);windowedPlacement_.length=sizeof(WINDOWPLACEMENT);GetWindowPlacement(hwnd_,&windowedPlacement_);haveWindowedPlacement_=true;MONITORINFO mi{sizeof(mi)};if(GetMonitorInfo(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&mi)){LONG_PTR style=(windowedStyle_&~WS_OVERLAPPEDWINDOW)|WS_POPUP|WS_VISIBLE;SetWindowLongPtr(hwnd_,GWL_STYLE,style);SetWindowPos(hwnd_,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_NOOWNERZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED|SWP_SHOWWINDOW);}fullscreen_=true;}
    else{fullscreen_=false;if(haveWindowedPlacement_){SetWindowLongPtr(hwnd_,GWL_STYLE,windowedStyle_);SetWindowPlacement(hwnd_,&windowedPlacement_);SetWindowPos(hwnd_,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_FRAMECHANGED|SWP_SHOWWINDOW);}applyWindowScale();}
    RECT rc{};if(GetClientRect(hwnd_,&rc))renderer_.resize((UINT)std::max(1L,rc.right-rc.left),(UINT)std::max(1L,rc.bottom-rc.top));InvalidateRect(hwnd_,nullptr,FALSE);
#else
    fullscreen_=enabled;ffx_web_fullscreen(enabled?1:0);
#endif
}
'''
s=s[:start]+newblock+s[end:]

# Add implementations before toggleFullscreen; these are read-only and do not affect gameplay.
telemetry_anchor='''void Game::toggleFullscreen(){setFullscreen(!fullscreen_);saveSettings();beginUiTransition();}'''
telemetry_impl='''#if defined(__EMSCRIPTEN__)
int Game::webDebugMode() const{return (int)mode_;}
int Game::webDebugStageIndex() const{return stageIndex_;}
float Game::webDebugCameraX() const{return cameraX_;}
float Game::webDebugScrollProgress() const{return scrollProgress();}
int Game::webDebugActorCount() const{return (int)actors_.size();}
int Game::webDebugActiveEnemies() const{return activeEnemies();}
float Game::webDebugPlayerX() const{auto p=player(0);return p?p->x:-1.f;}
int Game::webDebugPlayerHp() const{auto p=player(0);return p?p->hp:-1;}
int Game::webDebugNextSpawn() const{return (int)nextSpawn_;}
int Game::webDebugSpawnCount() const{return (int)level_.spawns.size();}
#endif
void Game::toggleFullscreen(){setFullscreen(!fullscreen_);saveSettings();beginUiTransition();}'''
if telemetry_anchor not in s: raise SystemExit('Game.cpp web telemetry anchor missing')
s=s.replace(telemetry_anchor,telemetry_impl,1)

s=s.replace('case 6:PostMessage(hwnd_,WM_CLOSE,0,0);break;', '#if defined(_WIN32)\n        case 6:PostMessage(hwnd_,WM_CLOSE,0,0);break;\n#else\n        case 6:ffx_web_exit();break;\n#endif',1)
s=s.replace('if(GetAsyncKeyState(VK_ESCAPE)&0x8000){bindingCapture_=false;input_.clearCaptureEvents();return;}', '#if defined(_WIN32)\n            if(GetAsyncKeyState(VK_ESCAPE)&0x8000){bindingCapture_=false;input_.clearCaptureEvents();return;}\n#else\n            if(input_.uiState().back){bindingCapture_=false;input_.clearCaptureEvents();return;}\n#endif',1)
pc.write_text(s,encoding='utf-8')
print('web Game patch applied')
