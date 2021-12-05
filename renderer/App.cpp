#include "App.h"
#include <algorithm>
#include <strsafe.h>
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Common/Util.h"
#include "common/perflog.h"
#include "render/camera.h"
#include "Core/GameObject.h"
#include "core/scene.h"
#include "core/behaviour.h"
#include "input/input.h"
#include "Scene/PbrScene.h"
#include "Scene/PhysicsDemo.h"
#include "physics/world.h"
#include "Render/PbrRenderer.h"
#include "Render/Backend/D3D11/GfxDriver.h"

namespace glacier {
App::App( const std::string& commandLine ) :
    cmd_line_(commandLine),
    wnd_(1280, 720, TEXT("Engine Renderer"))
{
    render::GfxDriverD3D11::Instance()->Init(wnd_.handle(), wnd_.width(), wnd_.height());

    renderer_ = std::make_unique<render::PbrRenderer>(render::GfxDriverD3D11::Instance());
    wnd_.resize_signal().Connect([this] (uint32_t width, uint32_t height) {
        renderer_->OnResize(width, height);
    });

    Input::Instance()->mouse().SetWindow(wnd_.handle());
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
    SceneManager::Instance()->Update(renderer_.get());
    Input::Instance()->keyboard().Update();

    if (!pause_) {
        physics::World::Instance()->Advance(dt);
        BehaviourManager::Instance()->Update(dt);
        //animation update
        BehaviourManager::Instance()->LateUpdate(dt);
        GameObjectManager::Instance()->DrawGizmos();
    }

    renderer_->Render();

    GameObjectManager::Instance()->CleanDead();
}

App::~App()
{
    GameObjectManager::Instance()->OnExit();
    //gfx_->Destroy();
}

void App::OnStart() {
    auto physics_scene = std::make_unique<render::PhysicsDemo>("physics");
    auto pbr_scene = std::make_unique<render::PbrScene>("pbr");
    SceneManager::Instance()->Add(std::move(physics_scene));
    SceneManager::Instance()->Add(std::move(pbr_scene));

    SceneManager::Instance()->Load("physics", SceneLoadMode::kSingle);

    renderer_->Setup();
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
        const auto dt = (float)timer_.Mark() * time_scale_;
        if (HandleInput(dt)) {
            return 0;
        }

        DoFrame(dt);
    }
}

}
