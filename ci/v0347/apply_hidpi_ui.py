from pathlib import Path
import re

root=Path.cwd()
game=root/"src/Game.cpp"
s=game.read_text(encoding="utf-8-sig")

if "v0.3.46 HD SHARP UI" not in s:
    raise SystemExit("v0.3.47 patch requires v0.3.46 fullscreen UI source")

# HUD readability: keep authored positions but increase small text slightly in widescreen
# and add a one-pixel dark shadow for contrast on bright backgrounds.
old='''void Game::drawHud(){
    const auto& hud=db_.campaign().hud;const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());
'''
new='''void Game::drawHud(){
    const auto& hud=db_.campaign().hud;const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());
    const float hudFontScale=renderer_.logicalWidth()>320.f?1.08f:1.f;
'''
if old not in s: raise SystemExit("drawHud header anchor missing")
s=s.replace(old,new,1)

old='''auto hudText=[&](const std::wstring& text,const HudPoint& p,float size=6.f){if(visible(p)&&!text.empty())renderer_.text(text,ux+(float)p.x,(float)p.y,size,D2D1::ColorF(1,1,1));};'''
new='''auto hudText=[&](const std::wstring& text,const HudPoint& p,float size=6.f){
        if(!visible(p)||text.empty())return;
        const float fs=size*hudFontScale,tx=ux+(float)p.x,ty=(float)p.y;
        renderer_.text(text,tx+1.f,ty+1.f,fs,D2D1::ColorF(0,0,0));
        renderer_.text(text,tx,ty,fs,D2D1::ColorF(1,1,1));
    };'''
if old not in s: raise SystemExit("hudText anchor missing")
s=s.replace(old,new,1)

# Small UI copy becomes more readable without changing panel geometry.
repls=[
('renderer_.text(L"SETAS/WASD: MENU   ENTER/J: OK   F11: TELA CHEIA",0,224,5.4f','renderer_.text(L"SETAS/WASD: MENU   ENTER/J: OK   F11: TELA CHEIA",0,224,5.8f'),
('renderer_.text(L"NATIVE PORT v0.3.46 HD SHARP UI",ux+246,232,4.8f','renderer_.text(L"NATIVE PORT v0.3.47 HD SHARP UI",ux+246,232,5.1f'),
('renderer_.text(fullscreen_?L"GRAFICOS GPU FUNCIONAM TAMBEM EM TELA CHEIA":L"ESQ/DIR AJUSTA   ENTER OK   ESC VOLTA",0,216,5.4f','renderer_.text(fullscreen_?L"GRAFICOS GPU FUNCIONAM TAMBEM EM TELA CHEIA":L"ESQ/DIR AJUSTA   ENTER OK   ESC VOLTA",0,216,5.8f'),
('renderer_.text(widescreen_?L"MODO 16:9 HABILITADO | HUD/TEXTO COM PIXEL SNAP":L"RENDER INTERNO 4:3 | WIDESCREEN OPCIONAL 16:9",0,209,5.2f','renderer_.text(widescreen_?L"MODO 16:9 HABILITADO | HUD/TEXTO COM PIXEL SNAP":L"RENDER INTERNO 4:3 | WIDESCREEN OPCIONAL 16:9",0,209,5.6f'),
('renderer_.text(L"PIXEL SHARP E O PRESET RECOMENDADO",0,219,5.2f','renderer_.text(L"PIXEL SHARP E O PRESET RECOMENDADO",0,219,5.6f'),
('renderer_.text(L"P1-P3 | XINPUT 1-4 | CONFIGURACAO SALVA",0,214,5.7f','renderer_.text(L"P1-P3 | XINPUT 1-4 | CONFIGURACAO SALVA",0,214,6.0f'),
('renderer_.text(L"ENTER REMAPEIA   ESC VOLTA",0,211,6','renderer_.text(L"ENTER REMAPEIA   ESC VOLTA",0,211,6.3f'),
('renderer_.text(L"START/ATAQUE: CONFIRMA   VOLTAR: CANCELA",0,211,6.2f','renderer_.text(L"START/ATAQUE: CONFIRMA   VOLTAR: CANCELA",0,211,6.4f'),
('renderer_.text(db_.rules().colourSelect?L"ESQ/DIR: RODA PERSONAGEM   CIMA/BAIXO: COR   ESC: VOLTA":L"ESQ/DIR: RODA PERSONAGEM   ESC: VOLTA",0,224,5.3f','renderer_.text(db_.rules().colourSelect?L"ESQ/DIR: RODA PERSONAGEM   CIMA/BAIXO: COR   ESC: VOLTA":L"ESQ/DIR: RODA PERSONAGEM   ESC: VOLTA",0,224,5.6f'),
('renderer_.text(L"APERTE UM BOTAO PARA CONTINUAR",0,215,6,','renderer_.text(L"APERTE UM BOTAO PARA CONTINUAR",0,215,6.3f,')
]
for old,new in repls:
    if old in s:
        s=s.replace(old,new,1)
    else:
        print("optional font anchor not present: "+old[:90])

game.write_text(s,encoding="utf-8")

# Improve DPI/display-mode transitions: explicitly refresh renderer size after
# Windows applies DPI or monitor resolution changes.
mw=root/"src/main_win.cpp"
t=mw.read_text(encoding="utf-8-sig")
old='''case WM_DPICHANGED:{auto*r=reinterpret_cast<RECT*>(l);if(r)SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}case WM_DESTROY:'''
new='''case WM_DPICHANGED:{auto*r=reinterpret_cast<RECT*>(l);if(r)SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);if(g){RECT c{};GetClientRect(h,&c);g->resize((UINT)std::max(1L,c.right-c.left),(UINT)std::max(1L,c.bottom-c.top));}return 0;}case WM_DISPLAYCHANGE:{if(g){RECT c{};GetClientRect(h,&c);g->resize((UINT)std::max(1L,c.right-c.left),(UINT)std::max(1L,c.bottom-c.top));}InvalidateRect(h,nullptr,FALSE);return 0;}case WM_DESTROY:'''
if old not in t: raise SystemExit("main_win DPI anchor missing")
t=t.replace(old,new,1)
t=t.replace("Native C++ Port v0.3.46 HD Sharp UI","Native C++ Port v0.3.47 HD Sharp UI",1)
mw.write_text(t,encoding="utf-8")

cm=root/"CMakeLists.txt"
c=cm.read_text(encoding="utf-8-sig")
if "VERSION 0.3.46" not in c: raise SystemExit("CMake v0.3.46 anchor missing")
c=c.replace("VERSION 0.3.46","VERSION 0.3.47",1)
cm.write_text(c,encoding="utf-8")

(root/"CHANGELOG-v0.3.47-HIDPI-UI.txt").write_text("""Final Fight X Native v0.3.47 - HiDPI / Widescreen UI pass

- Preserves all v0.3.46 fullscreen safe-area fixes and v0.3.45 HD Sharp features.
- HUD text gets a subtle dark shadow for contrast over bright stages.
- HUD small-text scale increases slightly in widescreen mode while 4:3 keeps original sizing.
- Small menu/help/footer typography is increased for better 1080p, 1440p and 4K readability.
- Explicit renderer resize/repaint is performed after WM_DPICHANGED and WM_DISPLAYCHANGE.
- Keeps legacy 320px HUD coordinates centered inside the widescreen safe area.
- No gameplay, camera, collision, combat, saves or HD texture assets changed.
""",encoding="utf-8")
print("v0.3.47 HiDPI UI patch applied")
