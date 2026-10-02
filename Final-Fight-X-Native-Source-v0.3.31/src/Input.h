#pragma once
#ifdef _WIN32
#include <array>
#include <cstddef>
#include <string>
#include <windows.h>
#include <Xinput.h>
namespace ffx {
struct InputState{
    bool left=false,right=false,up=false,down=false;
    bool attack=false,attack2=false,jump=false,special=false,start=false,back=false;
    bool leftPressed=false,rightPressed=false,upPressed=false,downPressed=false;
    bool attackPressed=false,attack2Pressed=false,jumpPressed=false,specialPressed=false,startPressed=false,backPressed=false;
};

enum class InputAction : size_t { Left,Right,Up,Down,Attack,Attack2,Jump,Special,Start,Back,Count };

// Values stored in settings.ini for XInput bindings.
enum class PadCode : int {
    None=0,
    DPadUp=1,DPadDown=2,DPadLeft=3,DPadRight=4,
    A=5,B=6,X=7,Y=8,LB=9,RB=10,Back=11,Start=12,LThumb=13,RThumb=14,
    LT=15,RT=16,
    LSUp=17,LSDown=18,LSLeft=19,LSRight=20,
    RSUp=21,RSDown=22,RSLeft=23,RSRight=24,
    MoveUp=25,MoveDown=26,MoveLeft=27,MoveRight=28
};

struct PlayerBindings{
    std::array<int,(size_t)InputAction::Count> keyboard{};
    std::array<int,(size_t)InputAction::Count> gamepad{};
    bool keyboardEnabled=true;
    int gamepadIndex=0; // -1 = disabled, 0..3 = XInput controller.
};

class Input{
public:
    static constexpr size_t MaxPlayers=3;
    static constexpr size_t ActionCount=(size_t)InputAction::Count;

    Input();
    void update();
    const InputState& state(size_t index=0)const{return cur_[index<MaxPlayers?index:0];}
    const InputState& uiState()const{return ui_;}

    void resetDefaults();
    void resetPlayerDefaults(size_t player);
    const PlayerBindings& bindings(size_t player)const{return bindings_[player<MaxPlayers?player:0];}
    PlayerBindings& bindings(size_t player){return bindings_[player<MaxPlayers?player:0];}
    void setKeyboardBinding(size_t player,InputAction action,int vk);
    void setGamepadBinding(size_t player,InputAction action,int code);
    void setKeyboardEnabled(size_t player,bool enabled);
    void setGamepadIndex(size_t player,int index);

    int takeLastKeyboardPress();
    int takeLastGamepadPress(int controllerIndex);
    void clearCaptureEvents();

    static const wchar_t* actionName(InputAction action);
    static std::wstring keyName(int vk);
    static const wchar_t* padName(int code);
    static const char* actionSettingName(InputAction action);

private:
    std::array<InputState,MaxPlayers> cur_{},prev_{};
    InputState ui_{},uiPrev_{};
    std::array<PlayerBindings,MaxPlayers> bindings_{};
    std::array<bool,256> rawKeyPrev_{},rawKeyCur_{};
    std::array<XINPUT_STATE,4> padPrev_{};
    std::array<XINPUT_STATE,4> padCur_{};
    std::array<bool,4> padConnected_{};
    int lastKeyboardPress_=-1;
    std::array<int,4> lastGamepadPress_{{-1,-1,-1,-1}};

    static bool padCodeDown(const XINPUT_GAMEPAD& g,int code);
    static int detectPadPress(const XINPUT_GAMEPAD& now,const XINPUT_GAMEPAD& old);
    static void edges(InputState& c,const InputState& p);
};
}
#endif
