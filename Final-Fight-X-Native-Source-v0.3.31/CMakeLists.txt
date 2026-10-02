cmake_minimum_required(VERSION 3.20)
project(FinalFightXNative VERSION 0.3.31 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(ffx_data STATIC
  src/OpenBorData.cpp
  src/BorAudio.cpp
)
target_include_directories(ffx_data PUBLIC src)

if(WIN32)
  add_executable(FinalFightX WIN32
    src/main_win.cpp
    src/Game.cpp
    src/Renderer.cpp
    src/Audio.cpp
    src/Input.cpp
  )
  target_link_libraries(FinalFightX PRIVATE
    ffx_data
    d2d1 dwrite windowscodecs xaudio2 xinput9_1_0 ole32 user32 gdi32 shell32
  )
  target_compile_definitions(FinalFightX PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00
  )
endif()




