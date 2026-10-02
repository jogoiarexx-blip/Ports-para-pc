from pathlib import Path
import sys
src=Path(sys.argv[1])
def read(rel): return (src/rel).read_text(encoding="utf-8-sig")
cm=read("CMakeLists.txt"); mw=read("src/main_win.cpp"); g=read("src/Game.cpp"); sem=read("src/OpenBorSemantics.h"); rh=read("src/Renderer.h"); rc=read("src/Renderer.cpp")
checks={
 "version-cmake":"VERSION 0.3.42" in cm,
 "version-window":"Native C++ Port v0.3.42" in mw,
 "version-menu":"NATIVE PORT v0.3.42" in g,
 "validator-cmake":"add_executable(v0342_ui_scene_validator" in cm,
 "ui-offset-helper":"openBorLegacyUiOffsetX" in sem and "logicalWidth-320.f" in sem,
 "scene-fade-helper":"openBorSceneFadeOpacity" in sem,
 "scene-preload":"renderer_.preloadAnimation(st.asset)" in g,
 "scene-centered":"openBorLegacyUiOffsetX(renderer_.logicalWidth())+(float)sceneX_" in g,
 "loading-centered":"renderer_.drawImage(bg,ux,0)" in g,
 "menu-centered":"renderer_.fillRect(ux+44,61,232,173" in g,
 "hud-centered":"const float x=ux+(float)pos.x" in g,
 "select-start-local":"textCenteredAt(L\\"START\\",x+48" in g,
 "text-format-cache":"textFormats_" in rh and "cachedTextFormat" in rc,
 "text-grayscale":"D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE" in rc,
 "image-target-guard":"if(!target_&&!createTarget())return nullptr;auto it=images_.find(rel)" in rc,
 "validator-present":(src/"tests/v0342_ui_scene_validator.cpp").exists(),
}
bad=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f"{k}={'OK' if v else 'FAIL'}")
if bad: raise SystemExit("v0.3.42 validation failed: "+", ".join(bad))
print("v0.3.42 UI/scene validator=OK")
