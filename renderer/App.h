#pragma once

#include "window.h"
#include "common/timer.h"

namespace glacier {

namespace render {
class Renderer;
class GfxDriver;
}

class App {
public:
    App(const std::string& commandLine = "");
    ~App();

    int Go();

private:
    void OnStart();
    void DoFrame( float dt );
    bool HandleInput( float dt );

    std::string cmd_line_;
    float time_scale_ = 1.0f;
    bool pause_ = false;
    Timer timer_;
    //std::unique_ptr<render::GfxDriver> gfx_;
    std::unique_ptr<render::Renderer> renderer_;
    Window wnd_; //make sure wnd_ destroyed first (due to WinMsgProc)
};

}
