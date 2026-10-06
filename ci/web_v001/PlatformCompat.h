#pragma once
#if defined(__EMSCRIPTEN__)
#include <cstdint>
#include <cmath>
using HWND = void*;
using UINT = unsigned int;
using LONG_PTR = long long;
struct WINDOWPLACEMENT { unsigned int length=sizeof(WINDOWPLACEMENT); };
struct D2D1_COLOR_F { float r=0.f,g=0.f,b=0.f,a=1.f; };
namespace D2D1 {
struct ColorF : D2D1_COLOR_F {
    enum Enum { Black, White };
    ColorF(float rr,float gg,float bb,float aa=1.f){r=rr;g=gg;b=bb;a=aa;}
    ColorF(Enum e,float aa=1.f){float v=e==White?1.f:0.f;r=g=b=v;a=aa;}
};
}
constexpr int VK_BACK=8;
constexpr int VK_TAB=9;
constexpr int VK_RETURN=13;
constexpr int VK_SHIFT=16;
constexpr int VK_CONTROL=17;
constexpr int VK_MENU=18;
constexpr int VK_ESCAPE=27;
constexpr int VK_SPACE=32;
constexpr int VK_LEFT=37;
constexpr int VK_UP=38;
constexpr int VK_RIGHT=39;
constexpr int VK_DOWN=40;
constexpr int VK_NUMPAD0=96;
constexpr int VK_NUMPAD1=97;
constexpr int VK_NUMPAD2=98;
constexpr int VK_NUMPAD3=99;
constexpr int VK_NUMPAD4=100;
constexpr int VK_NUMPAD5=101;
constexpr int VK_NUMPAD6=102;
constexpr int VK_NUMPAD7=103;
constexpr int VK_NUMPAD8=104;
constexpr int VK_NUMPAD9=105;
constexpr int VK_DECIMAL=110;
#endif
