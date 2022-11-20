
#include "App.h"
#include "Lux/VM.h"
#include "Common/Util.h"
#include "Common/Log.h"
#include "Jobs/JobSystem.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
    LPSTR lpCmdLine, int nCmdShow) 
{
    using namespace glacier;

    jobs::JobSystem::Instance()->Initialize(4);
    App app{};

    app.Init(lpCmdLine, "Script/preload.lua", "Script/main.lua");
    int ret = app.Run();
    app.Finalize();

    jobs::JobSystem::Instance()->WaitUntilFinish();

    return 0;
}
