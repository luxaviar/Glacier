#include "input.h"

namespace glacier {

bool Input::IsRelativeMode() {
    return Instance()->mouse().IsRelativeMode();
}

bool Input::IsMouseRightDown() {
    return Instance()->mouse().IsRightDown();
}

bool Input::IsMouseRightUp() {
    return Instance()->mouse().IsRightUp();
}

bool Input::IsMouseLeftDown() {
    return Instance()->mouse().IsLeftDown();
}

bool Input::IsMouseLeftUp() {
    return Instance()->mouse().IsLeftUp();
}

bool Input::IsKeyDown(Keyboard::Keys key) {
    return Instance()->keyboard().IsKeyDown(key);
}

bool Input::IsKeyUp(Keyboard::Keys key) {
    return Instance()->keyboard().IsKeyUp(key);
}

bool Input::IsJustKeyDown(Keyboard::Keys key) {
    return Instance()->keyboard().IsJustKeyDown(key);
}

bool Input::IsJustKeyUp(Keyboard::Keys key) {
    return Instance()->keyboard().IsJustKeyUp(key);
}

const Keyboard::State& Input::GetKeyState() {
    return Instance()->keyboard().GetState();
}

const Keyboard::State& Input::GetJustKeyDownState() {
    return Instance()->keyboard().GetTracker().pressed;
}

const Keyboard::State& Input::GetJustKeyUpState() {
    return Instance()->keyboard().GetTracker().released;
}

void Input::ReadRawDelta(int& x, int& y) {
    Instance()->mouse().ReadRawDelta(x, y);
}

int Input::GetPosX() {
    return Instance()->mouse().GetPosX();
}

int Input::GetPosY() {
    return Instance()->mouse().GetPosY();
}

}
