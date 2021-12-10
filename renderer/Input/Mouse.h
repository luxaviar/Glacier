#pragma once

#include <queue>
#include <optional>

namespace glacier {

class Mouse {
public:
    enum class Mode
    {
        kAbsolute = 0,
        kRelative,
    };

    struct State
    {
        bool    left_button = false;
        bool    middle_button = false;
        bool    right_button = false;
        bool    xbutton1 = false;
        bool    xbutton2 = false;
        int     x = 0;
        int     y = 0;
        int     scroll_wheel_value = 0;
        Mode    mode = Mode::kAbsolute;
    };

    class ButtonStateTracker
    {
    public:
        enum class ButtonState
        {
            kUp = 0,         // Button is up
            kHeld = 1,       // Button is held down
            kReleased = 2,   // Button was just released
            kPressed = 3,    // Buton was just pressed
        };

        ButtonState left_button;
        ButtonState middle_button;
        ButtonState right_button;
        ButtonState xbutton1;
        ButtonState xbutton2;

        void Update(const State& state) noexcept;
        void Reset() noexcept;
        State GetLastState() const noexcept { return last_state_; }

    private:
        State last_state_;
    };

public:
    // Retrieve the current state of the mouse
    const State& GetState() const { return state_; }

    bool IsRightDown() const;
    bool IsRightUp() const;

    bool IsLeftDown() const;
    bool IsLeftUp() const;

    bool IsMiddleDown() const;
    bool IsMiddleUp() const;

    void ReadRawDelta(int& x, int& y);

    int GetPosX() const { return state_.x; }
    int GetPosY() const { return state_.y; }

    bool IsRelativeMode() const;

    // Resets the accumulated scroll wheel value
    void ResetScrollWheelValue() noexcept;
    void ResetRawDelta() noexcept;

    // Sets mouse mode (defaults to absolute)
    void SetMode(Mode mode);

    // Feature detection
    //bool IsConnected() const;

    // Cursor visibility
    bool IsVisible() const noexcept;
    void SetVisible(bool visible);
    
    void SetWindow(HWND window);
    void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

private:
    void ClipToWindow() noexcept;

    State state_;
    HWND window_ = nullptr;

    int last_x_ = 0;
    int last_y_ = 0;
    bool in_focus_ = true;
};

}