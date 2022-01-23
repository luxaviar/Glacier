#pragma once

#include "window.h"
#include "Inspect/timer.h"

namespace glacier {

namespace render {
class Renderer;
class GfxDriver;
}

class App {
public:
    static App* Self() { return self_; }

    App(const std::string& commandLine = "");
    ~App();

    int Go();

    render::Renderer* GetRenderer() const { return renderer_.get(); }

private:
    void OnStart();
    void DoFrame( float dt );
    bool HandleInput( float dt );

    static App* self_;

    render::GfxDriver* gfx_ = nullptr;
    std::string cmd_line_;
    float time_scale_ = 1.0f;
    bool pause_ = false;
    Timer timer_;
    //std::unique_ptr<render::GfxDriver> gfx_;
    std::unique_ptr<render::Renderer> renderer_;
    Window wnd_; //make sure wnd_ destroyed first (due to WinMsgProc)
};

}
