#pragma once

#include "window.h"
#include "Inspect/timer.h"
#include "Lux/Vm.h"

namespace glacier {

namespace render {
class Renderer;
class GfxDriver;
}

class App {
public:
    static App* Self() { return self_; }

    App();
    ~App();

    void Setup(std::unique_ptr<Window>&& window,
        render::GfxDriver* gfx, std::unique_ptr<render::Renderer>&& renderer);

    void Init(const char*, const char* preload_script, const char* main_script);
    int Run();
    void Finalize();

    render::Renderer* GetRenderer() const { return renderer_.get(); }
    LuaVM& VM() { return vm_; }

private:
    //void OnStart();
    void DoFrame( float dt );
    bool HandleInput( float dt );

    static App* self_;

    render::GfxDriver* gfx_ = nullptr;
    std::string cmd_line_;
    float time_scale_ = 1.0f;
    bool pause_ = false;
    Timer timer_;
    LuaVM vm_;

    std::unique_ptr<render::Renderer> renderer_;
    std::unique_ptr<Window> wnd_; //make sure wnd_ destroyed first (due to WinMsgProc)
};

}
