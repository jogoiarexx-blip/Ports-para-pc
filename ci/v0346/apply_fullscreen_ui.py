from pathlib import Path
import re

root=Path.cwd()
game_path=root/"src/Game.cpp"
s=game_path.read_text(encoding="utf-8-sig")

if "TEXTURAS HD" not in s or "v0.3.45 HD SHARP" not in s:
    raise SystemExit("v0.3.46 fullscreen UI patch requires v0.3.45 HD Sharp source")

# 1) Graphics menu: v0.3.45 added the HD texture switch (9th row),
# but the old 20px cadence was authored for eight rows. Compact it enough
# to preserve footer/help text and avoid overlap on fullscreen/DPI layouts.
a=s.find("void Game::drawGraphics()")
b=s.find("void Game::drawControls()",a)
if a<0 or b<0:
    raise SystemExit("drawGraphics function not found")
g=s[a:b]
m=re.search(r'for\(int i=0;i<(\d+);\+\+i\)\{float y=([0-9.]+)f\+i\*([0-9.]+)f;',g)
if not m:
    raise SystemExit("graphics row layout anchor not found")
count=int(m.group(1))
if count < 9:
    raise SystemExit(f"expected HD graphics menu with >=9 rows, got {count}")
old=m.group(0)
new=f'for(int i=0;i<{count};++i){{float y=45.f+i*17.5f;'
g=g.replace(old,new,1)

# Keep all authored 320px menu panels centered inside widescreen logical surfaces.
if "const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth())" not in g:
    raise SystemExit("v0.3.42 graphics safe-area centering missing")
s=s[:a]+g+s[b:]

# 2) Stage-complete screen was the remaining multi-column UI still spreading
# its legacy 320px coordinates across the whole 426px widescreen canvas.
old="int col=0;float colW=renderer_.logicalWidth()/(float)active;"
new="const float ux=openBorLegacyUiOffsetX(renderer_.logicalWidth());int col=0;float colW=320.f/(float)active;"
if old not in s:
    raise SystemExit("stage-complete column anchor missing")
s=s.replace(old,new,1)
old="float x=colW*(float)col+9.f;++col;"
new="float x=ux+colW*(float)col+9.f;++col;"
if old not in s:
    raise SystemExit("stage-complete x anchor missing")
s=s.replace(old,new,1)

# 3) Force a clean repaint after switching fullscreen/borderless geometry.
# WM_SIZE still performs the normal renderer resize; this only prevents stale
# menu contents on drivers that defer the resize/repaint.
old='RECT rc{};if(GetClientRect(hwnd_,&rc))renderer_.resize((UINT)std::max(1L,rc.right-rc.left),(UINT)std::max(1L,rc.bottom-rc.top));\n}'
new='RECT rc{};if(GetClientRect(hwnd_,&rc))renderer_.resize((UINT)std::max(1L,rc.right-rc.left),(UINT)std::max(1L,rc.bottom-rc.top));InvalidateRect(hwnd_,nullptr,FALSE);\n}'
if old not in s:
    raise SystemExit("fullscreen resize anchor missing")
s=s.replace(old,new,1)

# Keep the version visible in the menu/window distinct from the HD texture pack.
s=s.replace("NATIVE PORT v0.3.45 HD SHARP","NATIVE PORT v0.3.46 HD SHARP UI",1)

game_path.write_text(s,encoding="utf-8")

# Window title and CMake version.
mw=root/"src/main_win.cpp"
t=mw.read_text(encoding="utf-8-sig")
t=t.replace("Native C++ Port v0.3.45 HD Sharp","Native C++ Port v0.3.46 HD Sharp UI")
mw.write_text(t,encoding="utf-8")

cm=root/"CMakeLists.txt"
c=cm.read_text(encoding="utf-8-sig")
if "VERSION 0.3.45" in c:
    c=c.replace("VERSION 0.3.45","VERSION 0.3.46",1)
cm.write_text(c,encoding="utf-8")

(root/"CHANGELOG-v0.3.46-FULLSCREEN-UI.txt").write_text("""Final Fight X Native v0.3.46 - Fullscreen UI compatibility

- Keeps v0.3.45 HD Sharp renderer and HD texture toggle intact.
- Centers the remaining legacy 320px stage-complete columns in widescreen.
- Reflows the 9-row Graphics menu introduced by TEXTURAS HD so help/footer text no longer collides.
- Preserves the v0.3.42 safe-area centering used by Title, Options, Graphics, Controls, Pause, Select, HUD, Loading and scenes.
- Forces a clean menu repaint after fullscreen/windowed transitions.
- No gameplay, collision, camera, combat, save or HD texture behavior changed.
""",encoding="utf-8")
print("v0.3.46 fullscreen UI compatibility patch applied")
