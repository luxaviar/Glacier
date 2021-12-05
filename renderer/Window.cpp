
#include "window.h"
#include <sstream>
#include <filesystem>
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui.h"
#include "Common/Util.h"
#include "input/input.h"
#include "render/backend/d3d11/gfxdriver.h"

namespace glacier {

// Window Class Stuff
Window::WindowClass Window::WindowClass::wndClass;

Window::WindowClass::WindowClass() noexcept :
    hInst(GetModuleHandle(nullptr))
{
    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window 
    // to achieve 100% scaling while still allowing non-client window content to 
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof( wc );
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = HandleMsgSetup;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetInstance();
    wc.hIcon = LoadIcon(hInst, IDI_APPLICATION);
    /*static_cast<HICON>(LoadImage(
        GetInstance(),MAKEINTRESOURCE( IDI_ICON1 ),
        IMAGE_ICON,32,32,0
    ));*/
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = GetName();
    wc.hIconSm = LoadIcon(hInst, IDI_APPLICATION);
    /*static_cast<HICON>(LoadImage(
        GetInstance(),MAKEINTRESOURCE( IDI_ICON1 ),
        IMAGE_ICON,16,16,0
    ));*/
    RegisterClassEx( &wc );
}

Window::WindowClass::~WindowClass()
{
    UnregisterClass( wndClassName,GetInstance() );
}

const TCHAR* Window::WindowClass::GetName() noexcept
{
    return wndClassName;
}

HINSTANCE Window::WindowClass::GetInstance() noexcept
{
    return wndClass.hInst;
}

// Window Stuff
Window::Window(uint32_t width, uint32_t height,const TCHAR* name) :
    width_(width),
    height_(height)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // calculate window size based on desired client region size
    RECT wr;
    wr.left = 100;
    wr.right = width + wr.left;
    wr.top = 100;
    wr.bottom = height + wr.top;
    if (AdjustWindowRect(&wr, WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, FALSE) == 0) {
        WinThrowLastExcept();
    }

    // create window & get hWnd
    hwnd_ = CreateWindow(
        WindowClass::GetName(),name,
        WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU,
        CW_USEDEFAULT,CW_USEDEFAULT,wr.right - wr.left,wr.bottom - wr.top,
        nullptr,nullptr,WindowClass::GetInstance(),this
    );
    // check for error
    if (hwnd_ == nullptr) {
        WinThrowLastExcept();
    }
    // newly created windows start off as hidden
    ShowWindow(hwnd_, SW_SHOWDEFAULT);
}

Window::~Window() {
    DestroyWindow(hwnd_);
}

void Window::SetTitle(const TCHAR* title) {
    if (SetWindowText( hwnd_,title ) == 0) {
        WinThrowLastExcept();
    }
}

void Window::ToogleFullScreen() {
    full_screen_ = !full_screen_;

    if (full_screen_) // Switching to fullscreen.
    {
        // Store the current window dimensions so they can be restored 
        // when switching out of fullscreen state.
        ::GetWindowRect(hwnd_, &window_rect_);

        // Set the window style to a borderless window so the client area fills
        // the entire screen.
        UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);// | WS_THICKFRAME | WS_MAXIMIZEBOX);

        ::SetWindowLongW(hwnd_, GWL_STYLE, windowStyle);

        // Query the name of the nearest display device for the window.
        // This is required to set the fullscreen dimensions of the window
        // when using a multi-monitor setup.
        HMONITOR hMonitor = ::MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEX monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        ::GetMonitorInfo(hMonitor, &monitorInfo);

        ::SetWindowPos(hwnd_, HWND_TOP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ::ShowWindow(hwnd_, SW_MAXIMIZE);
    } else {
        // Restore all the window decorators.
        ::SetWindowLong(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW);

        ::SetWindowPos(hwnd_, HWND_NOTOPMOST,
            window_rect_.left,
            window_rect_.top,
            window_rect_.right - window_rect_.left,
            window_rect_.bottom - window_rect_.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ::ShowWindow(hwnd_, SW_NORMAL);
    }
}

std::optional<int> Window::ProcessMessages() noexcept {
    MSG msg;
    // while queue has messages, remove and dispatch them (but do not block on empty queue)
    while( PeekMessage( &msg,nullptr,0,0,PM_REMOVE ) )
    {
        // check for quit because peekmessage does not signal this via return val
        if( msg.message == WM_QUIT )
        {
            // return optional wrapping int (arg to PostQuitMessage is in wparam) signals quit
            return (int)msg.wParam;
        }

        // TranslateMessage will post auxilliary WM_CHAR messages from key msgs
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    // return empty optional when not quitting app
    return {};
}

LRESULT CALLBACK Window::HandleMsgSetup( HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam ) noexcept {
    // use create parameter passed in from CreateWindow() to store window class pointer at WinAPI side
    if (msg == WM_NCCREATE) {
        // extract ptr to window class from creation data
        const CREATESTRUCTW* const pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        Window* const pWnd = static_cast<Window*>(pCreate->lpCreateParams);
        // set WinAPI-managed user data to store ptr to window instance
        SetWindowLongPtr( hWnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(pWnd) );
        // set message proc to normal (non-setup) handler now that setup is finished
        SetWindowLongPtr( hWnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&Window::HandleMsgThunk) );
        // forward message to window instance handler
        return pWnd->HandleMsg( hWnd,msg,wParam,lParam );
    }
    // if we get a message before the WM_NCCREATE message, handle with default handler
    return DefWindowProc( hWnd,msg,wParam,lParam );
}

LRESULT CALLBACK Window::HandleMsgThunk( HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam ) noexcept {
    // retrieve ptr to window instance
    Window* const pWnd = reinterpret_cast<Window*>(GetWindowLongPtr( hWnd,GWLP_USERDATA ));
    // forward message to window instance handler
    return pWnd->HandleMsg( hWnd,msg,wParam,lParam );
}

LRESULT Window::HandleMsg( HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam ) noexcept {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }
    //const auto& imio = ImGui::GetIO();

    auto& keyboard = Input::Instance()->keyboard();
    auto& mouse = Input::Instance()->mouse();

    switch( msg )
    {
    // we don't want the DefProc to handle this message because
    // we want our destructor to destroy the window, so return 0 instead of break
    case WM_CLOSE:
        PostQuitMessage( 0 );
        return 0;
    case WM_SIZE: //TODO: when resizeing, the message is post rapidly, we need WM_ENTERSIZEMOVE & WM_EXITSIZEMOVE
    {
        if (wParam != SIZE_MINIMIZED) {
            RECT clientRect = {};
            ::GetClientRect(hWnd, &clientRect);

            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;

            Resize(width, height);
        }
    }
    break;

    /*********** KEYBOARD MESSAGES ***********/
    case WM_ACTIVATEAPP:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        keyboard.ProcessMessage(msg, wParam, lParam);
        break;

    /************* MOUSE MESSAGES ****************/
    case WM_ACTIVATE:
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_INPUT:
        mouse.ProcessMessage(msg, wParam, lParam);
        break;
    }

    return DefWindowProc( hWnd,msg,wParam,lParam );
}

void Window::Resize(uint32_t width, uint32_t height) {
    if (width_ != width || height_ != height) {
        width_ = width;
        height_ = height;

        resize_signal_.Emit(width, height);
    }
}

}
