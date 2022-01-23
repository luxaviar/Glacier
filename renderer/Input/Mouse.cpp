#include "mouse.h"
#include "imgui/imgui.h"

namespace glacier {

#define UPDATE_BUTTON_STATE(field) field = static_cast<ButtonState>( ( !!state.field ) | ( ( !!state.field ^ !!last_state_.field ) << 1 ) );

void Mouse::ButtonStateTracker::Update(const State& state) noexcept {
    UPDATE_BUTTON_STATE(left_button)

    assert((!state.left_button && !last_state_.left_button) == (left_button == ButtonState::kUp));
    assert((state.left_button && last_state_.left_button) == (left_button == ButtonState::kHeld));
    assert((!state.left_button && last_state_.left_button) == (left_button == ButtonState::kReleased));
    assert((state.left_button && !last_state_.left_button) == (left_button == ButtonState::kPressed));

    UPDATE_BUTTON_STATE(middle_button)
    UPDATE_BUTTON_STATE(right_button)
    UPDATE_BUTTON_STATE(xbutton1)
    UPDATE_BUTTON_STATE(xbutton2)

    last_state_ = state;
}

void Mouse::ButtonStateTracker::Reset() noexcept {
    last_state_ = {};
}

bool Mouse::IsRightDown() const {
    return state_.right_button;
}

bool Mouse::IsRightUp() const {
    return !state_.right_button;
}

bool Mouse::IsLeftDown() const {
    return state_.left_button;
}

bool Mouse::IsLeftUp() const {
    return !state_.left_button;
}

bool Mouse::IsMiddleDown() const {
    return state_.middle_button;
}

bool Mouse::IsMiddleUp() const {
    return !state_.middle_button;
}

void Mouse::ReadRawDelta(int& x, int& y) {
    x = state_.x;
    y = state_.y;

    //state_.x = state_.y = 0;
}

bool Mouse::IsRelativeMode() const {
    return state_.mode == Mode::kRelative;
}

void Mouse::ResetScrollWheelValue() noexcept {
    state_.scroll_wheel_value = 0;
}

void Mouse::ResetRawDelta() noexcept {
    if (state_.mode == Mode::kRelative) {
        state_.x = state_.y = 0;
    }
}

void Mouse::SetMode(Mode mode) {
    if (state_.mode == mode) return;

    state_.mode = mode;
    if (mode == Mode::kAbsolute) {
        ClipCursor(nullptr);

        POINT point;
        point.x = last_x_;
        point.y = last_y_;

        // We show the cursor before moving it to support Remote Desktop
        ShowCursor(TRUE);

        if (MapWindowPoints(window_, nullptr, &point, 1)) {
            SetCursorPos(point.x, point.y);
        }
        state_.x = last_x_;
        state_.y = last_y_;

        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    } else {
        state_.x = state_.y = 0;
        //mRelativeX = INT32_MAX;
        //mRelativeY = INT32_MAX;

        ShowCursor(FALSE);
        ClipToWindow();

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }
}

//bool Mouse::IsConnected() const {
//    return GetSystemMetrics(SM_MOUSEPRESENT) != 0;
//}

bool Mouse::IsVisible() const noexcept {
    if (state_.mode == Mode::kRelative)
        return false;

    CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
    if (!GetCursorInfo(&info))
        return false;

    return (info.flags & CURSOR_SHOWING) != 0;
}

void Mouse::SetVisible(bool visible) {
    if (state_.mode == Mode::kRelative)
        return;

    CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
    if (!GetCursorInfo(&info))
    {
        throw std::exception("GetCursorInfo");
    }

    bool isvisible = (info.flags & CURSOR_SHOWING) != 0;
    if (isvisible != visible)
    {
        ShowCursor(visible);
    }
}

void Mouse::SetWindow(HWND window) {
    if (window_ == window)
        return;

    assert(window != nullptr);

    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x1 /* HID_USAGE_PAGE_GENERIC */;
    rid.usUsage = 0x2 /* HID_USAGE_GENERIC_MOUSE */;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = window;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
    {
        throw std::exception("RegisterRawInputDevices");
    }

    window_ = window;
}

void Mouse::ClipToWindow() noexcept {
    assert(window_ != nullptr);

    RECT rect;
    GetClientRect(window_, &rect);

    POINT ul;
    ul.x = rect.left;
    ul.y = rect.top;

    POINT lr;
    lr.x = rect.right;
    lr.y = rect.bottom;

    MapWindowPoints(window_, nullptr, &ul, 1);
    MapWindowPoints(window_, nullptr, &lr, 1);

    rect.left = ul.x;
    rect.top = ul.y;

    rect.right = lr.x;
    rect.bottom = lr.y;

    ClipCursor(&rect);
}

void Mouse::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    const auto& imio = ImGui::GetIO();
    switch (message)
    {
    case WM_ACTIVATEAPP:
        if (wParam) {
            in_focus_ = true;
            if (state_.mode == Mode::kRelative) {
                state_.x = state_.y = 0;

                ShowCursor(FALSE);
                ClipToWindow();
            }
        } else {
            int scroll_wheel = state_.scroll_wheel_value;
            state_ = {};
            state_.scroll_wheel_value = scroll_wheel;

            in_focus_ = false;
        }
        return;

    case WM_INPUT:
        if (in_focus_ && state_.mode == Mode::kRelative) {
            RAWINPUT raw;
            UINT rawSize = sizeof(raw);

            UINT result_data = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &rawSize, sizeof(RAWINPUTHEADER));
            if (result_data == UINT(-1)) {
                throw std::exception("GetRawInputData");
            }

            if (raw.header.dwType == RIM_TYPEMOUSE) {
                if (!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                    state_.x += raw.data.mouse.lLastX;
                    state_.y += raw.data.mouse.lLastY;
                }
                else if (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) {
                    // This is used to make Remote Desktop sessons work
                    //const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                    //const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

                    //int x = static_cast<int>((float(raw.data.mouse.lLastX) / 65535.0f) * float(width));
                    //int y = static_cast<int>((float(raw.data.mouse.lLastY) / 65535.0f) * float(height));

                    //if (pImpl->mRelativeX == INT32_MAX)
                    //{
                    //    state_.x = state_.y = 0;
                    //}
                    //else
                    //{
                    //    state_.x = x - pImpl->mRelativeX;
                    //    state_.y = y - pImpl->mRelativeY;
                    //}

                    //pImpl->mRelativeX = x;
                    //pImpl->mRelativeY = y;
                }
            }
        }
        return;

    case WM_MOUSEMOVE:
        break;

    case WM_LBUTTONDOWN:
        if (imio.WantCaptureMouse) {
            break;
        }
        state_.left_button = true;
        break;

    case WM_LBUTTONUP:
        state_.left_button = false;
        if (imio.WantCaptureMouse) {
            break;
        }
        //state_.left_button = false;
        break;

    case WM_RBUTTONDOWN:
        if (imio.WantCaptureMouse) {
            break;
        }
        state_.right_button = true;
        break;

    case WM_RBUTTONUP:
        if (imio.WantCaptureMouse) {
            break;
        }
        state_.right_button = false;
        break;

    case WM_MBUTTONDOWN:
        if (imio.WantCaptureMouse) {
            break;
        }
        state_.middle_button = true;
        break;

    case WM_MBUTTONUP:
        if (imio.WantCaptureMouse) {
            break;
        }
        state_.middle_button = false;
        break;

    case WM_MOUSEWHEEL:
        if (imio.WantCaptureMouse) {
            break;
        }
        state_.scroll_wheel_value += GET_WHEEL_DELTA_WPARAM(wParam);
        return;

    case WM_XBUTTONDOWN:
        if (imio.WantCaptureMouse) {
            break;
        }

        switch (GET_XBUTTON_WPARAM(wParam))
        {
        case XBUTTON1:
            state_.xbutton1 = true;
            break;

        case XBUTTON2:
            state_.xbutton2 = true;
            break;
        }
        break;

    case WM_XBUTTONUP:
        if (imio.WantCaptureMouse) {
            break;
        }

        switch (GET_XBUTTON_WPARAM(wParam))
        {
        case XBUTTON1:
            state_.xbutton1 = false;
            break;

        case XBUTTON2:
            state_.xbutton2 = false;
            break;
        }
        break;

    case WM_MOUSEHOVER:
        break;

    default:
        // Not a mouse message, so exit
        return;
    }

    if (state_.mode == Mode::kAbsolute) {
        // All mouse messages provide a new pointer position
        int xPos = static_cast<short>(LOWORD(lParam)); // GET_X_LPARAM(lParam);
        int yPos = static_cast<short>(HIWORD(lParam)); // GET_Y_LPARAM(lParam);

        state_.x = last_x_ = xPos;
        state_.y = last_y_ = yPos;
    }
}

}
