
#include "App.h"
#include "Lux/VM.h"
#include "Common/Util.h"
#include "Common/Log.h"
#include "Jobs/JobSystem.h"
#include <filesystem>

namespace {

bool SetRuntimeWorkingDirectory() {
    constexpr size_t kModulePathCapacity = 32768;
    std::wstring module_path(kModulePathCapacity, L'\0');
    DWORD module_path_length = GetModuleFileNameW(
        nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (module_path_length == 0 || module_path_length >= module_path.size()) {
        return false;
    }

    module_path.resize(module_path_length);
    std::error_code error;
    std::filesystem::path executable_directory =
        std::filesystem::path(module_path).parent_path();
    std::filesystem::path current_directory = std::filesystem::current_path(error);
    if (error) {
        current_directory.clear();
    }

    const std::filesystem::path candidates[] = {
        current_directory,
        executable_directory,
        executable_directory.parent_path(),
        executable_directory.parent_path().parent_path(),
        executable_directory.parent_path().parent_path() / L"renderer"
    };

    for (const auto& candidate : candidates) {
        if (candidate.empty() ||
            !std::filesystem::exists(candidate / L"Script" / L"preload.lua", error) ||
            !std::filesystem::exists(candidate / L"Assets" / L"Shader", error)) {
            error.clear();
            continue;
        }

        return SetCurrentDirectoryW(candidate.c_str()) != FALSE;
    }

    return false;
}

}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
    LPSTR lpCmdLine, int nCmdShow) 
{
    using namespace glacier;

    SetRuntimeWorkingDirectory();
    jobs::JobSystem::Instance()->Initialize(4);
    App app{};

    app.Init(lpCmdLine, "Script/preload.lua", "Script/main.lua");
    int ret = app.Run();
    app.Finalize();

    jobs::JobSystem::Instance()->WaitUntilFinish();

    return 0;
}
