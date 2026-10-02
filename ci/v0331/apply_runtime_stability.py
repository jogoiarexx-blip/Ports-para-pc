from pathlib import Path

root = Path.cwd()

cmake = root / "CMakeLists.txt"
text = cmake.read_text(encoding="utf-8-sig")
old = "project(FinalFightXNative VERSION 0.3.30 LANGUAGES CXX)"
new = "project(FinalFightXNative VERSION 0.3.31 LANGUAGES CXX)"
if old not in text:
    raise SystemExit("v0.3.30 CMake version anchor not found")
cmake.write_text(text.replace(old, new), encoding="utf-8")

main = root / "src" / "main_win.cpp"
text = main.read_text(encoding="utf-8-sig")
inc_old = "#include <chrono>\n#include <filesystem>"
inc_new = "#include <chrono>\n#include <algorithm>\n#include <filesystem>"
if inc_old not in text:
    raise SystemExit("main_win include anchor not found")
text = text.replace(inc_old, inc_new, 1)
text = text.replace(
    'L"Final Fight X - Native C++ Port v0.3.30"',
    'L"Final Fight X - Native C++ Port v0.3.31"',
    1,
)
loop_old = 'auto last=std::chrono::steady_clock::now();MSG msg{};bool run=true;while(run){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}auto now=std::chrono::steady_clock::now();float dt=std::chrono::duration<float>(now-last).count();last=now;if(g){g->update(dt);g->render();}Sleep(1);}'
loop_new = 'auto last=std::chrono::steady_clock::now();MSG msg{};bool run=true;while(run){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){run=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}auto now=std::chrono::steady_clock::now();if(IsIconic(h)){last=now;Sleep(25);continue;}float dt=std::clamp(std::chrono::duration<float>(now-last).count(),0.f,.10f);last=now;if(g){constexpr float maxStep=1.f/120.f;float remaining=dt;int substeps=0;while(remaining>0.f&&substeps<12){float step=std::min(remaining,maxStep);g->update(step);remaining-=step;++substeps;}g->render();}Sleep(1);}'
if loop_old not in text:
    raise SystemExit("v0.3.30 main loop anchor not found")
text = text.replace(loop_old, loop_new, 1)
main.write_text(text, encoding="utf-8")

(root / "CHANGELOG-v0.3.31.txt").write_text("""Final Fight X Native - v0.3.31

Base: v0.3.30 reconstructed from branch build-ffx-v0330.

Runtime improvements:
- Bounds long frame gaps after Alt-Tab, debugger pauses or driver stalls.
- Splits long frames into simulation substeps of at most 1/120 second.
- Suspends gameplay simulation while the window is minimized and resets the timing origin.
- Keeps authored OpenBOR animation-delay conversion unchanged.
- Updates native window/version metadata to v0.3.31.

Validation:
- cumulative v0.3.22-v0.3.30 validators
- Windows x64 Release build
- v0.3.31 source-token validation
""", encoding="utf-8")

(root / "README-SOURCE-v0.3.31.txt").write_text("""Final Fight X Native v0.3.31 - Source

Windows x64 build:
  cmake -S . -B build -A x64
  cmake --build build --config Release --target FinalFightX -- /m

Place the generated FinalFightX.exe beside the assets folder from the portable package.
Requires Visual Studio 2022 Build Tools (Desktop C++) and CMake 3.20+.
""", encoding="utf-8")

print("v0.3.31 runtime stability patch applied")
