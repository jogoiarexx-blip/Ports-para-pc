from pathlib import Path

root = Path.cwd()
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8-sig")
main = (root / "src" / "main_win.cpp").read_text(encoding="utf-8-sig")

checks = {
    "version": "VERSION 0.3.31" in cmake,
    "title": "Native C++ Port v0.3.31" in main,
    "algorithm": "#include <algorithm>" in main,
    "minimized_pause": "if(IsIconic(h))" in main,
    "frame_gap_bound": "std::clamp(std::chrono::duration<float>(now-last).count(),0.f,.10f)" in main,
    "substep": "constexpr float maxStep=1.f/120.f" in main,
    "catchup_cap": "substeps<12" in main,
    "update_step": "g->update(step)" in main,
}
bad = [k for k,v in checks.items() if not v]
if bad:
    raise SystemExit("v0.3.31 validation failed: " + ", ".join(bad))
print("v0.3.31 runtime stability validator=OK")
