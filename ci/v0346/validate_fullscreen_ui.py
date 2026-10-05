from pathlib import Path
import re,sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path.cwd()
g=(root/"src/Game.cpp").read_text(encoding="utf-8-sig")
cm=(root/"CMakeLists.txt").read_text(encoding="utf-8-sig")
checks={
 "v0346-version":"VERSION 0.3.46" in cm,
 "hd-sharp-kept":"TEXTURAS HD" in g,
 "menu-version":"v0.3.46 HD SHARP UI" in g,
 "graphics-safe-area":"void Game::drawGraphics()" in g and "const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth())" in g,
 "graphics-nine-row-fit":bool(re.search(r'for\(int i=0;i<9;\+\+i\)\{float y=45\.f\+i\*17\.5f;',g)),
 "stage-complete-safe-width":"float colW=320.f/(float)active" in g,
 "stage-complete-offset":"float x=ux+colW*(float)col+9.f" in g,
 "fullscreen-repaint":"InvalidateRect(hwnd_,nullptr,FALSE)" in g,
 "main-menu-centered":"renderer_.fillRect(ux+44,61,232,173" in g,
 "options-centered":"renderer_.fillRect(ux+38,24,244,204" in g,
 "controls-centered":"renderer_.fillRect(ux+26,18,268,210" in g,
 "pause-centered":"renderer_.fillRect(ux+67,52,186,137" in g,
 "select-centered":"float x=ux+8.f+(float)i*104.f" in g,
 "loading-centered":"renderer_.drawImage(bg,ux,0)" in g,
}
bad=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f"{k}={'OK' if v else 'FAIL'}")
if bad: raise SystemExit("v0.3.46 fullscreen UI validation failed: "+", ".join(bad))
print("v0.3.46 fullscreen UI validator=OK")
