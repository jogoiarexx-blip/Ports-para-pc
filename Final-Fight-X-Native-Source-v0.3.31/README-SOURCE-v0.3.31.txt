Final Fight X Native v0.3.31 - Source

Windows x64 build:
  cmake -S . -B build -A x64
  cmake --build build --config Release --target FinalFightX -- /m

Place the generated FinalFightX.exe beside the assets folder from the portable package.
Requires Visual Studio 2022 Build Tools (Desktop C++) and CMake 3.20+.
