#include "App.h"
#include <algorithm>
#include <strsafe.h>
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Common/Util.h"
#include "render/camera.h"
#include "Core/GameObject.h"
#include "core/scene.h"
#include "core/behaviour.h"
#include "input/input.h"
#include "Scene/PbrScene.h"
#include "Scene/PhysicsDemo.h"
#include "physics/world.h"
#include "Render/ForwardRenderer.h"
#include "Render/DeferredRenderer.h"
#include "Render/Base/GfxDriver.h"
#include "Render/Backend/D3D12/GfxDriver.h"
#include "Inspect/Profiler.h"
#include "Render/Material.h"
#include "Render/LightManager.h"
#include "jobs/JobSystem.h"
#include "Common/Log.h"
#include "Lux/Lux.h"

namespace glacier {

LUX_IMPL(App, App)
LUX_CTOR(App)
LUX_FUNC(App, Self)
LUX_FUNC(App, Setup)
LUX_IMPL_END

App* App::self_ = nullptr;

App::App()
{
    self_ = this;
}

void App::Init(const char* cmd_line, const char* preload_script, const char* main_script) {
    cmd_line_ = cmd_line;
    vm_.Init(cmd_line, preload_script, main_script);
}

void App::Setup(std::unique_ptr<Window>&& window,
    render::GfxDriver* gfx, std::unique_ptr<render::Renderer>&& renderer) {
    ASSERT(!wnd_);
    ASSERT(!gfx_);
    ASSERT(!renderer_);

    wnd_ = std::move(window);
    gfx_ = gfx;
    renderer_ = std::move(renderer);

    renderer_->Setup();

    wnd_->resize_signal().Connect([this](uint32_t width, uint32_t height) {
        renderer_->OnResize(width, height);
        });

    Input::Instance()->mouse().SetWindow(wnd_->handle());
    Profiler::Instance();
}

void App::Finalize() {
    Profiler::Instance()->PrintAll();
    GameObjectManager::Instance()->OnExit();
    SceneManager::Instance()->ClearAll();

    if (gfx_) {
        gfx_->OnDestroy();
        gfx_ = nullptr;
    }

}

bool App::HandleInput(float dt) {
    auto& mouse = Input::Instance()->mouse();
    if (mouse.IsRightDown()) {
        if (!mouse.IsRelativeMode()) {
            mouse.SetMode(Mouse::Mode::kRelative);
        }
    } else {
        if (mouse.IsRelativeMode()) {
            mouse.SetMode(Mouse::Mode::kAbsolute);
        }
    }

    auto& keyboard = Input::Instance()->keyboard();
    auto& state = keyboard.GetState();
    if (state.Escape) {
        gfx_->OnDestroy();
        return true;
    }

    if (state.Space) {
        pause_ = !pause_;
    }

    if (keyboard.IsJustKeyDown(Keyboard::F11)) {
        wnd_->ToogleFullScreen();
    }
    
    return false;
}

void App::DoFrame(float dt) {
    gfx_->BeginFrame();

    GameObjectManager::Instance()->CleanDead();
    //int counter = 0;
    //auto job1 = jobs::JobSystem::Instance()->Schedule([&counter]() {
    //    auto now = std::chrono::steady_clock::now();
    //    for (int i = 0; i < 10000; ++i) {
    //        counter++;
    //    }
    //});

    //auto job2 = jobs::JobSystem::Instance()->Schedule([&counter]() {
    //    auto now = std::chrono::steady_clock::now();
    //    for (int i = 0; i < 10000; ++i) {
    //        counter++;
    //    }
    //});
    SceneManager::Instance()->Update(renderer_.get());
    Input::Instance()->keyboard().Update();

    if (!pause_) {
        {
            PerfSample("Physics Update");
            physics::World::Instance()->Advance(dt);
        }

        {
            PerfSample("Behavior Update");
            BehaviourManager::Instance()->Update(dt);
        }
        //animation update
        BehaviourManager::Instance()->LateUpdate(dt);
    }

    {
        PerfSample("Render");
        renderer_->Render(dt);
    }
    

    Input::Instance()->EndFrame();

    //job1.WaitComplete();
    //job2.WaitComplete();

    gfx_->EndFrame();
}

App::~App()
{

}

//void App::OnStart() {
//    auto physics_scene = std::make_unique<render::PhysicsDemo>("physics");
//    auto pbr_scene = std::make_unique<render::PbrScene>("pbr");
//    SceneManager::Instance()->Add(std::move(physics_scene));
//    SceneManager::Instance()->Add(std::move(pbr_scene));
//
//    SceneManager::Instance()->Load("pbr", SceneLoadMode::kSingle);
//}

int App::Run() {
    // OnStart();

    while(true) {
        // process all messages pending, but to not block for new messages
        if(const auto ecode = Window::ProcessMessages()) {
            // if return optional has value, means we're quitting so return exit code
            return *ecode;
        }
        // execute the game logic
        const auto dt = (float)timer_.DeltaTime() * time_scale_;
        timer_.Mark();
        if (HandleInput(dt)) {
            return 0;
        }

        DoFrame(dt);
    }

    return 0;
}

}
