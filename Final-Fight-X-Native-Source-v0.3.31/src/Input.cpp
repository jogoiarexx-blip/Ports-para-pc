#ifdef _WIN32
#include "Input.h"
#include <Xinput.h>
#include <algorithm>
#include <cwchar>
#include <sstream>
namespace ffx {

static bool rawDown(const std::array<bool,256>& keys,int vk){return vk>=0&&vk<256&&keys[(size_t)vk];}

Input::Input(){resetDefaults();}

void Input::edges(InputState& c,const InputState& p){
    c.leftPressed=c.left&&!p.left;c.rightPressed=c.right&&!p.right;c.upPressed=c.up&&!p.up;c.downPressed=c.down&&!p.down;
    c.attackPressed=c.attack&&!p.attack;c.attack2Pressed=c.attack2&&!p.attack2;c.jumpPressed=c.jump&&!p.jump;c.specialPressed=c.special&&!p.special;c.startPressed=c.start&&!p.start;c.backPressed=c.back&&!p.back;
}

static int defKey(size_t p,InputAction a){
    static constexpr int defs[3][10]={
        {VK_LEFT,VK_RIGHT,VK_UP,VK_DOWN,'J','I','K','L',VK_RETURN,VK_ESCAPE},
        {VK_NUMPAD4,VK_NUMPAD6,VK_NUMPAD8,VK_NUMPAD2,VK_NUMPAD1,VK_NUMPAD7,VK_NUMPAD3,VK_NUMPAD5,VK_NUMPAD0,VK_DECIMAL},
        {'F','H','T','G','Y','O','U','P','3','4'}
    };
    return defs[std::min<size_t>(p,2)][(size_t)a];
}
static int defPad(InputAction a){
    switch(a){
        case InputAction::Left:return (int)PadCode::MoveLeft;
        case InputAction::Right:return (int)PadCode::MoveRight;
        case InputAction::Up:return (int)PadCode::MoveUp;
        case InputAction::Down:return (int)PadCode::MoveDown;
        case InputAction::Attack:return (int)PadCode::X;
        case InputAction::Attack2:return (int)PadCode::Y;
        case InputAction::Jump:return (int)PadCode::A;
        case InputAction::Special:return (int)PadCode::B;
        case InputAction::Start:return (int)PadCode::Start;
        case InputAction::Back:return (int)PadCode::Back;
        default:return 0;
    }
}
void Input::resetDefaults(){for(size_t p=0;p<MaxPlayers;++p)resetPlayerDefaults(p);}
void Input::resetPlayerDefaults(size_t p){
    if(p>=MaxPlayers)return;auto& b=bindings_[p];b.keyboardEnabled=true;b.gamepadIndex=(int)p;
    for(size_t a=0;a<ActionCount;++a){b.keyboard[a]=defKey(p,(InputAction)a);b.gamepad[a]=defPad((InputAction)a);}
}
void Input::setKeyboardBinding(size_t p,InputAction a,int vk){if(p<MaxPlayers&&vk>=0&&vk<256)bindings_[p].keyboard[(size_t)a]=vk;}
void Input::setGamepadBinding(size_t p,InputAction a,int code){if(p<MaxPlayers&&code>=0&&code<=(int)PadCode::MoveRight)bindings_[p].gamepad[(size_t)a]=code;}
void Input::setKeyboardEnabled(size_t p,bool e){if(p<MaxPlayers)bindings_[p].keyboardEnabled=e;}
void Input::setGamepadIndex(size_t p,int i){if(p<MaxPlayers)bindings_[p].gamepadIndex=std::clamp(i,-1,3);}

bool Input::padCodeDown(const XINPUT_GAMEPAD& g,int code){
    switch((PadCode)code){
        case PadCode::DPadUp:return (g.wButtons&XINPUT_GAMEPAD_DPAD_UP)!=0;
        case PadCode::DPadDown:return (g.wButtons&XINPUT_GAMEPAD_DPAD_DOWN)!=0;
        case PadCode::DPadLeft:return (g.wButtons&XINPUT_GAMEPAD_DPAD_LEFT)!=0;
        case PadCode::DPadRight:return (g.wButtons&XINPUT_GAMEPAD_DPAD_RIGHT)!=0;
        case PadCode::A:return (g.wButtons&XINPUT_GAMEPAD_A)!=0;
        case PadCode::B:return (g.wButtons&XINPUT_GAMEPAD_B)!=0;
        case PadCode::X:return (g.wButtons&XINPUT_GAMEPAD_X)!=0;
        case PadCode::Y:return (g.wButtons&XINPUT_GAMEPAD_Y)!=0;
        case PadCode::LB:return (g.wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER)!=0;
        case PadCode::RB:return (g.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER)!=0;
        case PadCode::Back:return (g.wButtons&XINPUT_GAMEPAD_BACK)!=0;
        case PadCode::Start:return (g.wButtons&XINPUT_GAMEPAD_START)!=0;
        case PadCode::LThumb:return (g.wButtons&XINPUT_GAMEPAD_LEFT_THUMB)!=0;
        case PadCode::RThumb:return (g.wButtons&XINPUT_GAMEPAD_RIGHT_THUMB)!=0;
        case PadCode::LT:return g.bLeftTrigger>80;
        case PadCode::RT:return g.bRightTrigger>80;
        case PadCode::LSUp:return g.sThumbLY>9000;
        case PadCode::LSDown:return g.sThumbLY<-9000;
        case PadCode::LSLeft:return g.sThumbLX<-9000;
        case PadCode::LSRight:return g.sThumbLX>9000;
        case PadCode::RSUp:return g.sThumbRY>9000;
        case PadCode::RSDown:return g.sThumbRY<-9000;
        case PadCode::RSLeft:return g.sThumbRX<-9000;
        case PadCode::RSRight:return g.sThumbRX>9000;
        case PadCode::MoveUp:return (g.wButtons&XINPUT_GAMEPAD_DPAD_UP)!=0||g.sThumbLY>9000;
        case PadCode::MoveDown:return (g.wButtons&XINPUT_GAMEPAD_DPAD_DOWN)!=0||g.sThumbLY<-9000;
        case PadCode::MoveLeft:return (g.wButtons&XINPUT_GAMEPAD_DPAD_LEFT)!=0||g.sThumbLX<-9000;
        case PadCode::MoveRight:return (g.wButtons&XINPUT_GAMEPAD_DPAD_RIGHT)!=0||g.sThumbLX>9000;
        default:return false;
    }
}
int Input::detectPadPress(const XINPUT_GAMEPAD& n,const XINPUT_GAMEPAD& o){
    for(int code=(int)PadCode::DPadUp;code<=(int)PadCode::RSRight;++code)if(padCodeDown(n,code)&&!padCodeDown(o,code))return code;
    return -1;
}

void Input::update(){
    prev_=cur_;uiPrev_=ui_;for(auto& s:cur_)s={};ui_={};
    rawKeyPrev_=rawKeyCur_;lastKeyboardPress_=-1;
    for(int vk=1;vk<256;++vk){rawKeyCur_[(size_t)vk]=(GetAsyncKeyState(vk)&0x8000)!=0;if(rawKeyCur_[(size_t)vk]&&!rawKeyPrev_[(size_t)vk]&&lastKeyboardPress_<0)lastKeyboardPress_=vk;}

    padPrev_=padCur_;lastGamepadPress_={{-1,-1,-1,-1}};
    for(DWORD i=0;i<4;++i){XINPUT_STATE xs{};padConnected_[i]=XInputGetState(i,&xs)==ERROR_SUCCESS;if(padConnected_[i])padCur_[i]=xs;else padCur_[i]={};lastGamepadPress_[i]=detectPadPress(padCur_[i].Gamepad,padPrev_[i].Gamepad);}

    for(size_t p=0;p<MaxPlayers;++p){
        auto& s=cur_[p];const auto& b=bindings_[p];
        auto down=[&](InputAction a){bool v=false;size_t ai=(size_t)a;if(b.keyboardEnabled)v|=rawDown(rawKeyCur_,b.keyboard[ai]);if(b.gamepadIndex>=0&&b.gamepadIndex<4&&padConnected_[(size_t)b.gamepadIndex])v|=padCodeDown(padCur_[(size_t)b.gamepadIndex].Gamepad,b.gamepad[ai]);return v;};
        s.left=down(InputAction::Left);s.right=down(InputAction::Right);s.up=down(InputAction::Up);s.down=down(InputAction::Down);
        s.attack=down(InputAction::Attack);s.attack2=down(InputAction::Attack2);s.jump=down(InputAction::Jump);s.special=down(InputAction::Special);s.start=down(InputAction::Start);s.back=down(InputAction::Back);edges(s,prev_[p]);
    }

    // Dedicated UI navigation fallback. Remapping gameplay can never lock the user out of menus.
    ui_=cur_[0];
    ui_.left|=rawDown(rawKeyCur_,VK_LEFT)||rawDown(rawKeyCur_,'A');ui_.right|=rawDown(rawKeyCur_,VK_RIGHT)||rawDown(rawKeyCur_,'D');
    ui_.up|=rawDown(rawKeyCur_,VK_UP)||rawDown(rawKeyCur_,'W');ui_.down|=rawDown(rawKeyCur_,VK_DOWN)||rawDown(rawKeyCur_,'S');
    ui_.start|=rawDown(rawKeyCur_,VK_RETURN);ui_.attack|=rawDown(rawKeyCur_,'J');ui_.back|=rawDown(rawKeyCur_,VK_ESCAPE);
    if(padConnected_[0]){const auto& g=padCur_[0].Gamepad;ui_.left|=padCodeDown(g,(int)PadCode::DPadLeft)||padCodeDown(g,(int)PadCode::LSLeft);ui_.right|=padCodeDown(g,(int)PadCode::DPadRight)||padCodeDown(g,(int)PadCode::LSRight);ui_.up|=padCodeDown(g,(int)PadCode::DPadUp)||padCodeDown(g,(int)PadCode::LSUp);ui_.down|=padCodeDown(g,(int)PadCode::DPadDown)||padCodeDown(g,(int)PadCode::LSDown);ui_.attack|=padCodeDown(g,(int)PadCode::A);ui_.start|=padCodeDown(g,(int)PadCode::Start);ui_.back|=padCodeDown(g,(int)PadCode::B)||padCodeDown(g,(int)PadCode::Back);}
    edges(ui_,uiPrev_);
}

int Input::takeLastKeyboardPress(){int v=lastKeyboardPress_;lastKeyboardPress_=-1;return v;}
int Input::takeLastGamepadPress(int i){if(i<0||i>=4)return -1;int v=lastGamepadPress_[(size_t)i];lastGamepadPress_[(size_t)i]=-1;return v;}
void Input::clearCaptureEvents(){lastKeyboardPress_=-1;lastGamepadPress_={{-1,-1,-1,-1}};}

const wchar_t* Input::actionName(InputAction a){
    switch(a){case InputAction::Left:return L"ESQUERDA";case InputAction::Right:return L"DIREITA";case InputAction::Up:return L"CIMA";case InputAction::Down:return L"BAIXO";case InputAction::Attack:return L"ATAQUE";case InputAction::Attack2:return L"ATAQUE 2";case InputAction::Jump:return L"PULO";case InputAction::Special:return L"ESPECIAL";case InputAction::Start:return L"START/PAUSE";case InputAction::Back:return L"VOLTAR";default:return L"?";}
}
const char* Input::actionSettingName(InputAction a){
    switch(a){case InputAction::Left:return "left";case InputAction::Right:return "right";case InputAction::Up:return "up";case InputAction::Down:return "down";case InputAction::Attack:return "attack";case InputAction::Attack2:return "attack2";case InputAction::Jump:return "jump";case InputAction::Special:return "special";case InputAction::Start:return "start";case InputAction::Back:return "back";default:return "unknown";}
}
std::wstring Input::keyName(int vk){
    if(vk<=0)return L"--";
    switch(vk){case VK_LEFT:return L"LEFT";case VK_RIGHT:return L"RIGHT";case VK_UP:return L"UP";case VK_DOWN:return L"DOWN";case VK_RETURN:return L"ENTER";case VK_ESCAPE:return L"ESC";case VK_SPACE:return L"SPACE";case VK_TAB:return L"TAB";case VK_BACK:return L"BACKSPACE";case VK_SHIFT:return L"SHIFT";case VK_CONTROL:return L"CTRL";case VK_MENU:return L"ALT";case VK_NUMPAD0:return L"NUM0";case VK_NUMPAD1:return L"NUM1";case VK_NUMPAD2:return L"NUM2";case VK_NUMPAD3:return L"NUM3";case VK_NUMPAD4:return L"NUM4";case VK_NUMPAD5:return L"NUM5";case VK_NUMPAD6:return L"NUM6";case VK_NUMPAD7:return L"NUM7";case VK_NUMPAD8:return L"NUM8";case VK_NUMPAD9:return L"NUM9";case VK_DECIMAL:return L"NUM.";}
    if((vk>='0'&&vk<='9')||(vk>='A'&&vk<='Z'))return std::wstring(1,(wchar_t)vk);
    if(vk>=VK_F1&&vk<=VK_F24)return L"F"+std::to_wstring(vk-VK_F1+1);
    UINT scan=MapVirtualKeyW((UINT)vk,MAPVK_VK_TO_VSC)<<16;wchar_t name[64]{};if(GetKeyNameTextW((LONG)scan,name,64)>0)return name;
    return L"VK"+std::to_wstring(vk);
}
const wchar_t* Input::padName(int code){
    switch((PadCode)code){case PadCode::DPadUp:return L"DPAD UP";case PadCode::DPadDown:return L"DPAD DOWN";case PadCode::DPadLeft:return L"DPAD LEFT";case PadCode::DPadRight:return L"DPAD RIGHT";case PadCode::A:return L"A";case PadCode::B:return L"B";case PadCode::X:return L"X";case PadCode::Y:return L"Y";case PadCode::LB:return L"LB";case PadCode::RB:return L"RB";case PadCode::Back:return L"BACK";case PadCode::Start:return L"START";case PadCode::LThumb:return L"L3";case PadCode::RThumb:return L"R3";case PadCode::LT:return L"LT";case PadCode::RT:return L"RT";case PadCode::LSUp:return L"LS UP";case PadCode::LSDown:return L"LS DOWN";case PadCode::LSLeft:return L"LS LEFT";case PadCode::LSRight:return L"LS RIGHT";case PadCode::RSUp:return L"RS UP";case PadCode::RSDown:return L"RS DOWN";case PadCode::RSLeft:return L"RS LEFT";case PadCode::RSRight:return L"RS RIGHT";case PadCode::MoveUp:return L"DPAD/LS UP";case PadCode::MoveDown:return L"DPAD/LS DOWN";case PadCode::MoveLeft:return L"DPAD/LS LEFT";case PadCode::MoveRight:return L"DPAD/LS RIGHT";default:return L"--";}
}
}
#endif
