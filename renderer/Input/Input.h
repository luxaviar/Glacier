#pragma once

#include <string>
#include <vector>
#include "Common/Singleton.h"
#include "keyboard.h"
#include "mouse.h"

namespace glacier {

class Input : public Singleton<Input> {
public:
    Keyboard& keyboard() { return keyboard_; }
    Mouse& mouse() { return mouse_; }

    static bool IsRelativeMode();

    static bool IsMouseRightDown();
    static bool IsMouseRightUp();

    static bool IsMouseLeftDown();
    static bool IsMouseLeftUp();

    static bool IsKeyDown(Keyboard::Keys key);
    static bool IsKeyUp(Keyboard::Keys key);

    static bool IsJustKeyDown(Keyboard::Keys key);
    static bool IsJustKeyUp(Keyboard::Keys key);

    static const Keyboard::State& GetKeyState();
    static const Keyboard::State& GetJustKeyDownState();
    static const Keyboard::State& GetJustKeyUpState();

    static void ReadRawDelta(int& x, int& y);
    static int GetPosX();
    static int GetPosY();

    static float GetMouseWheelDelta();
    static void EndFrame();

protected:
    Keyboard keyboard_;
    Mouse mouse_;
};

}
