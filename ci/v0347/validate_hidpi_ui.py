from pathlib import Path
import sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path.cwd()
g=(root/"src/Game.cpp").read_text(encoding="utf-8-sig")
mw=(root/"src/main_win.cpp").read_text(encoding="utf-8-sig")
cm=(root/"CMakeLists.txt").read_text(encoding="utf-8-sig")
checks={
 "v0347-version":"VERSION 0.3.47" in cm,
 "version-menu":"v0.3.47 HD SHARP UI" in g,
 "version-window":"v0.3.47 HD Sharp UI" in mw,
 "hud-safe-area":"const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth())" in g,
 "hud-font-scale":"hudFontScale=renderer_.logicalWidth()>320.f?1.08f:1.f" in g,
 "hud-shadow":"renderer_.text(text,tx+1.f,ty+1.f,fs,D2D1::ColorF(0,0,0))" in g,
 "dpi-refresh":"case WM_DPICHANGED:" in mw and "GetClientRect(h,&c);g->resize" in mw,
 "display-refresh":"case WM_DISPLAYCHANGE:" in mw and "InvalidateRect(h,nullptr,FALSE)" in mw,
 "graphics-fit-kept":"float y=45.f+i*17.5f" in g,
 "stage-safe-width-kept":"float colW=320.f/(float)active" in g,
 "hd-toggle-kept":"TEXTURAS HD" in g,
}
bad=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f"{k}={'OK' if v else 'FAIL'}")
if bad: raise SystemExit("v0.3.47 validation failed: "+", ".join(bad))
print("v0.3.47 HiDPI UI validator=OK")
