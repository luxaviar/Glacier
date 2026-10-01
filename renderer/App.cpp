#include "App.h"
#include <algorithm>
#include <strsafe.h>
#include <imgui.h>
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
#include "Render/Base/Enums.h"
#include "Render/Base/Renderable.h"
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

    // Nothing waits for the GPU at the end of a frame any more (see
    // GfxDriver::EndFrame), so the frames still in flight may be reading what
    // the teardown below is about to take apart. This is where that wait
    // belongs now: once, on the way out, instead of once per frame.
    if (gfx_) {
        gfx_->GetCommandQueue(render::CommandBufferType::kDirect)->Flush();
        gfx_->GetCommandQueue(render::CommandBufferType::kCompute)->Flush();
        gfx_->GetCommandQueue(render::CommandBufferType::kCopy)->Flush();
    }

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
    //escape is the way out of the app, unless a widget of the editor is being
    //typed into, where it is the way out of the edit
    if (state.Escape && !Input::IsTextInputActive()) {
        gfx_->OnDestroy();
        return true;
    }

    //P pauses the world; space belongs to the game, a demo character jumps with it
    if (keyboard.IsJustKeyDown(Keyboard::P)) {
        pause_ = !pause_;
    }

    if (keyboard.IsJustKeyDown(Keyboard::F11)) {
        wnd_->ToogleFullScreen();
    }

    //F2 writes what the frame looks like to ScreenCaptured.png, next to the
    //scripts the app runs from, which is how a run is checked without a viewer
    if (keyboard.IsJustKeyDown(Keyboard::F2)) {
        renderer_->CaptureScreen();
    }

    //F4 writes the profile of the frame that just ended to the log, which is
    //where a measurement worth keeping goes; the panel of the editor shows the
    //same tree, but only while the app is running
    if (keyboard.IsJustKeyDown(Keyboard::F4)) {
        Profiler::Instance()->PrintFrame();
        //the tree says where the CPU time went, the table where the GPU time
        //went; a frame that is waiting for the GPU shows up in both
        renderer_->stats()->PrintPassTimings();
    }
    
    return false;
}

void App::DoFrame(float dt) {
    //the profile is reported per frame, so the frame has to be marked; every
    //sample taken below, here or in the renderer, lands inside it
    auto profiler = Profiler::Instance();
    profiler->BeginFrame();

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
        //animation sampling happens here, in Animator::LateUpdate
        BehaviourManager::Instance()->LateUpdate(dt);
    }

    //the sampled poses are turned into render data (bone matrices and the
    //previous model matrix) exactly once per frame, so the passes only upload
    //what they need; it also runs while paused so the editor can move things
    //without the bones going stale
    {
        PerfSample("Update Render Data");
        render::RenderableManager::Instance()->UpdateRenderData();
    }

    {
        PerfSample("Render");
        renderer_->Render(dt);
    }
    

    Input::Instance()->EndFrame();

    //job1.WaitComplete();
    //job2.WaitComplete();

    gfx_->EndFrame();

    //the GPU side of the frame the profiler is about to close: the renderer
    //reads it out of its queries, and the report is where the two halves of a
    //frame are read together
    profiler->SetFrameGpuTime(renderer_->stats()->gpu_time() * 1000.0);

    //from the end of the frame before this one to the end of this one, which is
    //the loop iteration this frame belongs to; the dt the loop computed is the
    //interval before that, and using it here would report another frame's wall
    //time next to this frame's profile
    profiler->EndFrame(frame_timer_.DeltaTime());
    frame_timer_.Mark();
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
