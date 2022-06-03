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
#include "Render/Backend/D3D11/GfxDriver.h"
#include "Render/Backend/D3D12/GfxDriver.h"
#include "Inspect/Profiler.h"
#include "Render/Material.h"
#include "Render/LightManager.h"
#include "jobs/JobSystem.h"
#include "Common/Log.h"

namespace glacier {

App* App::self_ = nullptr;

App::App( const std::string& commandLine ) :
    cmd_line_(commandLine),
    wnd_(1280, 720, TEXT("Engine Viewer"))
{
    self_ = this;
    gfx_ = render::D3D12GfxDriver::Instance();
    gfx_->Init(wnd_.handle(), wnd_.width(), wnd_.height(), render::TextureFormat::kR8G8B8A8_UNORM);

    renderer_ = std::make_unique<render::DeferredRenderer>(gfx_, render::AntiAliasingType::kTAA);
    //renderer_ = std::make_unique<render::ForwardRenderer>(gfx_, render::MSAAType::k4x);

    wnd_.resize_signal().Connect([this] (uint32_t width, uint32_t height) {
        renderer_->OnResize(width, height);
    });

    Input::Instance()->mouse().SetWindow(wnd_.handle());
    Profiler::Instance();
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
        wnd_.ToogleFullScreen();
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
    renderer_->Setup();
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
        renderer_->Render();
    }
    

    Input::Instance()->EndFrame();

    //job1.WaitComplete();
    //job2.WaitComplete();

    gfx_->EndFrame();
}

App::~App()
{
    Profiler::Instance()->PrintAll();
    GameObjectManager::Instance()->OnExit();
    gfx_->OnDestroy();
}

void App::OnStart() {
    auto physics_scene = std::make_unique<render::PhysicsDemo>("physics");
    auto pbr_scene = std::make_unique<render::PbrScene>("pbr");
    SceneManager::Instance()->Add(std::move(physics_scene));
    SceneManager::Instance()->Add(std::move(pbr_scene));

    SceneManager::Instance()->Load("physics", SceneLoadMode::kSingle);
}

int App::Go() {
    OnStart();

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
