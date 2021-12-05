#include "keyboard.h"
#include "imgui/imgui.h"

namespace glacier {

void Keyboard::KeyboardStateTracker::Update(const State& state) noexcept {
    auto currPtr = reinterpret_cast<const uint32_t*>(&state);
    auto prevPtr = reinterpret_cast<const uint32_t*>(&last_state_);
    auto releasedPtr = reinterpret_cast<uint32_t*>(&released);
    auto pressedPtr = reinterpret_cast<uint32_t*>(&pressed);
    for (size_t j = 0; j < (256 / 32); ++j) {
        *pressedPtr = *currPtr & ~(*prevPtr);
        *releasedPtr = ~(*currPtr) & *prevPtr;

        ++currPtr;
        ++prevPtr;
        ++releasedPtr;
        ++pressedPtr;
    }

    last_state_ = state;
}

void Keyboard::KeyboardStateTracker::Reset() noexcept {
    memset(this, 0, sizeof(KeyboardStateTracker));
}

Keyboard::Keyboard() {
    Reset();
}

void Keyboard::Reset() noexcept {
    memset(&state_, 0, sizeof(Keyboard));
}

void Keyboard::Update() {
    tracker_.Update(state_);
}

bool Keyboard::IsKeyDown(Keys key) const {
    return state_.IsKeyDown(key);
}

bool Keyboard::IsKeyUp(Keys key) const {
    return state_.IsKeyUp(key);
}

bool Keyboard::IsJustKeyDown(Keys key) const {
    return tracker_.IsKeyPressed(key);
}

bool Keyboard::IsJustKeyUp(Keys key) const {
    return tracker_.IsKeyReleased(key);
}

void Keyboard::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    bool down = false;
    const auto& imio = ImGui::GetIO();

    switch (message)
    {
    case WM_ACTIVATEAPP:
        Reset();
        return;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (imio.WantCaptureKeyboard) {
            return;
        }
        down = true;
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (imio.WantCaptureKeyboard) {
            return;
        }
        break;

    default:
        return;
    }

    int vk = static_cast<int>(wParam);
    switch (vk)
    {
    case VK_SHIFT:
        vk = static_cast<int>(
            MapVirtualKey((static_cast<UINT>(lParam) & 0x00ff0000) >> 16u,
                MAPVK_VSC_TO_VK_EX));
        if (!down)
        {
            // Workaround to ensure left vs. right shift get cleared when both were pressed at same time
            KeyUp(VK_LSHIFT, state_);
            KeyUp(VK_RSHIFT, state_);
        }
        break;

    case VK_CONTROL:
        vk = (static_cast<UINT>(lParam) & 0x01000000) ? VK_RCONTROL : VK_LCONTROL;
        break;

    case VK_MENU:
        vk = (static_cast<UINT>(lParam) & 0x01000000) ? VK_RMENU : VK_LMENU;
        break;
    }

    if (down) {
        KeyDown(vk, state_);
    } else {
        KeyUp(vk, state_);
    }
}

void Keyboard::KeyDown(int key, Keyboard::State& state) noexcept {
    if (key < 0 || key > 0xfe)
        return;

    auto ptr = reinterpret_cast<uint32_t*>(&state);

    unsigned int bf = 1u << (key & 0x1f);
    ptr[(key >> 5)] |= bf;
}

void Keyboard::KeyUp(int key, Keyboard::State& state) noexcept {
    if (key < 0 || key > 0xfe)
        return;

    auto ptr = reinterpret_cast<uint32_t*>(&state);

    unsigned int bf = 1u << (key & 0x1f);
    ptr[(key >> 5)] &= ~bf;
}

}