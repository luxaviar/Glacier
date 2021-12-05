#pragma once

#include <optional>
#include <memory>
#include "exception/exception.h"
#include "input/keyboard.h"
#include "input/mouse.h"
#include "render/base/gfxdriver.h"
#include "common/signal.h"

namespace glacier {

class Window {
private:
    // singleton manages registration/cleanup of window class
    class WindowClass
    {
    public:
        static const TCHAR* GetName() noexcept;
        static HINSTANCE GetInstance() noexcept;
    private:
        WindowClass() noexcept;
        ~WindowClass();
        WindowClass( const WindowClass& ) = delete;
        WindowClass& operator=( const WindowClass& ) = delete;
        static constexpr const TCHAR* wndClassName = L"Engine Direct3D Engine Window";
        static WindowClass wndClass;
        HINSTANCE hInst;
    };
public:
    Window(uint32_t width, uint32_t height, const TCHAR* name);
    ~Window();
    Window( const Window& ) = delete;
    Window& operator=( const Window& ) = delete;
    void SetTitle( const TCHAR* title );
    static std::optional<int> ProcessMessages() noexcept;

    auto& resize_signal() { return resize_signal_; }
    HWND handle() { return  hwnd_; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }

    void ToogleFullScreen();

    //render::GfxDriver* graphics() { return gfx_.get(); }

private:
    static LRESULT CALLBACK HandleMsgSetup( HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam ) noexcept;
    static LRESULT CALLBACK HandleMsgThunk( HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam ) noexcept;
    LRESULT HandleMsg( HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam ) noexcept;
    void Resize(uint32_t width, uint32_t height);

private:
    uint32_t width_; //client area
    uint32_t height_;
    HWND hwnd_;
    //std::unique_ptr<render::GfxDriver> gfx_;

    Signal<uint32_t, uint32_t> resize_signal_;
    std::string cmd_line_;

    bool full_screen_ = false;
    RECT window_rect_;
};

}
