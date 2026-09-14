# AGENTS.md

Glacier is a personal toy renderer / game engine: Windows-only, C++20 (MSVC), D3D12 backend, CMake + vcpkg, single executable target `renderer` (entry `renderer/main.cpp` → `App` → Lua `Script/preload.lua` / `main.lua`). Use a more specific `AGENTS.md` when one exists deeper in the tree.

## Build

```powershell
# configure (VS generator; adjust -G for your VS version)
cmake -S . -B build/cmake -G "Visual Studio 18 2026" -A x64

# build - works from a plain shell
cmake --build build/cmake --config Debug --target renderer
cmake --build build/cmake --config Release --target renderer
```

- Needs `VCPKG_ROOT` (or an explicit `CMAKE_TOOLCHAIN_FILE`); the deps in `vcpkg.json` install on first configure; triplet is `x64-windows-static`.
- Success = exit code 0 plus `renderer.vcxproj -> ...\bin\Debug\renderer.exe`. A full Debug build takes ~2-4 minutes; output goes to `bin/<Config>/renderer.exe`.
- Faster incremental loop: the Ninja tree `cmake-build-debug` (CLion). Ninja calls `cl.exe` directly, so the MSVC environment must be loaded first or you get `fatal error C1083: cannot open include file 'vector'`:

```powershell
$envDump = cmd /c 'call "<VS install>\VC\Auxiliary\Build\vcvars64.bat" >nul && set'
foreach ($line in $envDump) { if ($line -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
cmake --build cmake-build-debug --target renderer
```

- Re-run `cmake -S . -B <build dir>` manually if a `CMakeLists.txt` / `vcpkg.json` change or new file is not picked up.
- The `C4267` / `C4244` / `C4305` / `C4018` / `C4172` warning baseline is pre-existing: don't clean up unrelated warnings, and don't add new ones.
- Never commit build or runtime output: `bin/`, `build/`, `cmake-build-*/`, `vcpkg_installed/`, `renderer/imgui.ini`, `game.log`.

## Verification

- No unit tests, no CI. Verification = it compiles, plus running it and looking at the result (needs Windows + a D3D12 GPU and opens a window), so default to building only.
- HLSL is compiled at runtime (`D3DCompileFromFile`, `renderer/Render/Backend/D3D12/Shader.cpp`) and errors only reach `OutputDebugStringA` - always run the app after touching shaders.
- Scenes: `Script/pbrscene.lua` (scene `pbr`, loaded by default) and `Script/physcene.lua` (scene `physics`); switch in `Script/main.lua`.
- The exe re-locates its working directory to the folder holding `Script/preload.lua` and `Assets/Shader/`, so the launch directory does not matter as long as those exist.

## Layout

| Path | Contents |
| --- | --- |
| `renderer/App.*`, `Window.*`, `main.cpp` | process entry, frame loop, input |
| `renderer/Core/`, `Component/`, `Behaviour/` | `GameObject`/`Component`/`Behaviour`/`Scene`/`Transform` framework and built-in components |
| `renderer/Render/Base/` | graphics abstractions: `GfxDriver`, `CommandBuffer`, `Shader`, `PipelineState`, `Resource`, ... |
| `renderer/Render/Backend/D3D12/` | D3D12 implementation of every `Base/` abstraction |
| `renderer/Render/Graph/` | `RenderGraph` (`AddPass`, resource handles) |
| `renderer/Render/PostProcess/` | FXAA, TAA, MSAA, GTAO, Exposure, ToneMapping |
| `renderer/Render/Mesh/`, `renderer/Geometry/` | `Mesh`/`Model`/`Primitive`/`BezierMesh`, geometry primitives |
| `renderer/Physics/` | in-house physics: dynamic BVH broadphase, GJK/EPA narrowphase, contact solver, colliders |
| `renderer/Jobs/`, `Concurrent/` | fiber job system, concurrent containers |
| `renderer/Lux/` | Lua binding framework |
| `renderer/Script/` | Lua bootstrap and scene scripts |
| `renderer/Assets/` | runtime assets: `Shader/*.hlsl`, models, textures |
| `renderer/Common/`, `Math/`, `Algorithm/`, `Input/`, `Log/`, `Exception/`, `Inspect/` | infrastructure: PCH, math, allocators, input, logging, exceptions, profiler |
| `renderer/3rdParty/` | vendored lua/lpeg built as C++ - don't edit |

`renderer/.vscode/.history/` holds stale editor-history copies of the sources; ignore it when searching.

## Conventions

- Namespace `glacier`, rendering code in `glacier::render`.
- One class per file, file name matches the class, PascalCase; headers use `#pragma once`.
- Members end with `_` (`gpu_time_`); struct fields mirroring HLSL buffers lead with `_` (`PerFrameData::_View`). Enumerators are `k`-prefixed (`kDynamic`); math types are `Vec2f`/`Vec3f`/`Vec4f`/`Mat4`/`Quaternion`.
- 4-space indent, Allman braces for classes/functions; `if`/`for` brace placement varies by file - match the file you edit.
- Includes are rooted at `renderer/` (`#include "Render/Base/GfxDriver.h"`); `3rdParty` and `3rdParty/lua` are also on the include path. Some legacy includes differ in case (`"render/camera.h"`); match real directory casing in new code.
- `renderer/Common/pch.h` is force-included (`Windows.h`, `<memory>`, ...), so new `.cpp` files don't need it - but still include what you use.
- Errors/logging: `ASSERT(cond)`, `GfxThrowIfFailed(hr)`, exception types in `renderer/Exception/`; log with `LOG_DEBUG` / `LOG_LOG` / `LOG_WARN` / `LOG_ERR` (`std::format` syntax).
- `IS_DEBUG` (`true`/`false`) plus `_DEBUG` / `NDEBUG` distinguish configurations; `GLACIER_REVERSE_Z` is defined globally - keep the depth convention consistent between C++ and HLSL.
- Constant buffers must match the HLSL layout exactly (alignment, float4 packing, padding) - see `PerFrameData` in `Renderer.h`.
- Commits: English imperative, capitalized, one thing per commit (`Add TAA`, `Fix speculer ao`, `Refactor Postprocess`). Branch prefix `codex/`.

## Common tasks

- **New post-process / pass**: follow `renderer/Render/PostProcess/FXAA.{h,cpp}`; register passes with `render_graph_.AddPass(name, setup, execute)` and mark programs with `Program::AddPass(passName)` in `DeferredRenderer` / `ForwardRenderer` (`Setup()` and `Execute()`); load the shader via `PostProcessMaterial("<name>", TEXT("<FileName>"))`.
- **Shaders**: files live at `Assets/Shader/<Name>.hlsl` and are loaded by base file name only (case-sensitive match). Default entry points are `main_vs` / `main_hs` / `main_ds` / `main_gs` / `main_ps` / `main_cs` (`DefaultShaderEntry` in `renderer/Render/Base/Enums.h`); shared code goes in `Assets/Shader/Common/*.hlsli`; `GLACIER_REVERSE_Z` is injected automatically.
- **Lua bindings**: use `LUX_IMPL` / `LUX_IMPL_INHERIT`, `LUX_CTOR`, `LUX_FUNC` / `LUX_PROP`, `LUX_IMPL_END` in the class `.cpp` (`renderer/Behaviour/Fall.cpp`, `renderer/App.cpp`); global functions and constants via `LUX_GLOBAL_FUNC` / `LUX_CONSTANT`; access from Lua as `require("Glacier.<ClassName>")`. Registration is static and duplicate names `exit(-1)` (`renderer/Lux/Register.cpp`).
- **New components**: derive from `Component` (`renderer/Core/Component.h`: `OnAwake` / `OnEnable` / `OnDisable` / `OnDestroy`) or `Behaviour` (adds `Update` / `LateUpdate`), attach with `game_object.AddComponent<T>(...)`; implement `DrawInspector()` / `OnDrawGizmos()` when the editor needs them.
- **New source files**: `renderer/CMakeLists.txt` globs `renderer/**` with `CONFIGURE_DEPENDS` (excluding `3rdParty`, `.vscode`), so adding files usually needs no CMake edit.
- **Physics changes**: verify by switching `Script/main.lua` to `sm:Load("physics", SceneLoadMode.kSingle)` and watching stacking stability and penetration.
