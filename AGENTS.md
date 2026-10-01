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
- Never commit build or runtime output: `bin/`, `build/`, `cmake-build-*/`, `vcpkg_installed/`, `renderer/imgui.ini`, `game.log`, and the `*.gclip` animation caches the importer writes next to a model.

## Verification

- No unit tests, no CI. Verification = it compiles, plus running it and looking at the result (needs Windows + a D3D12 GPU and opens a window), so default to building only.
- HLSL is compiled at runtime (`D3DCompileFromFile`, `renderer/Render/Backend/D3D12/Shader.cpp`) and errors only reach `OutputDebugStringA` - always run the app after touching shaders.
- Scenes: `Script/*.lua` registered in `Script/main.lua` (`pbrscene` → `pbr`, `physcene` → `physics`, `animscene` → `anim`, `ybotscene` → `ybot`); the single `sm:Load(...)` call left uncommented at the end picks the scene that runs, `ybot` by default.
- Animation only shows itself at runtime - a model bound to the wrong skeleton, a foot sliding or a lost root motion has to be watched. `anim` is the feature tour (node animation, additive layer, crossfade, instanced skinned draws), `ybot` the playable third person character.
- `build/probe/` (untracked scratch, gitignored) holds the probes the animation work was verified with: headless C++ tests (`anim_test.cpp`, `pose_test.cpp`, `skin_test.cpp`, `locomotion_test.cpp`) and scripts that drive the window with real input and capture frames (`drive_app.ps1`, `scene_regression.ps1`). Reuse them when touching animation.
- The exe re-locates its working directory to the folder holding `Script/preload.lua` and `Assets/Shader/`, so the launch directory does not matter as long as those exist.

## Layout

| Path | Contents |
| --- | --- |
| `renderer/App.*`, `Window.*`, `main.cpp` | process entry, frame loop, input |
| `renderer/Core/` | `GameObject`/`Component`/`Behaviour`/`Scene`/`Transform` framework |
| `renderer/Component/`, `renderer/Behaviour/` | built-in components and behaviours: camera controllers, `Fall`, and the Y Bot demo (`PlayerController`, `LocomotionBlend.h`, `ThirdPersonCamera`, `DemoHud`, `YBotDemo`) |
| `renderer/Animation/` | animation runtime: `AnimationClip`/`NodeTrack`/`AnimationKeyframe` keyframes, `Skeleton`/`SkeletonPose`, `Animator` (playback, weight layers, crossfade, 1D blend tree, events, root motion), `AnimationClipCache` (the `*.gclip` file) |
| `renderer/Render/Base/` | graphics abstractions: `GfxDriver`, `CommandBuffer`, `Shader`, `PipelineState`, `Resource`, ... |
| `renderer/Render/Backend/D3D12/` | D3D12 implementation of every `Base/` abstraction |
| `renderer/Render/Graph/` | `RenderGraph` (`AddPass`, resource handles) |
| `renderer/Render/PostProcess/` | FXAA, TAA, MSAA, GTAO, Exposure, ToneMapping |
| `renderer/Render/Mesh/`, `renderer/Geometry/` | `Mesh`/`Model`/`Primitive`/`BezierMesh`/`SkinnedMeshRenderer`, geometry primitives |
| `renderer/Render/Skinning/` | GPU skinning: `GpuSkinning` (compute pass + shared pool of skinned vertices), `BoneMatrixPool` (bone matrices of every skinned mesh), `InstanceBuffer` (instanced draws) |
| `renderer/Physics/` | in-house physics: dynamic BVH broadphase, GJK/EPA narrowphase, contact solver, colliders |
| `renderer/Jobs/`, `Concurrent/` | fiber job system, concurrent containers |
| `renderer/Lux/` | Lua binding framework |
| `renderer/Script/` | Lua bootstrap and scene scripts |
| `renderer/Assets/` | runtime assets: `Shader/*.hlsl`/`*.hlsli` (skinning in `SkinningCS.hlsl` and `Common/Skinning.hlsli`), models and textures (`Model/anim/` demo rigs, `Model/YBot/`) |
| `tools/` | hand-run Blender asset scripts, not part of the build (`mixamo_to_gltf.py` merges one Mixamo character and its FBX clips into a single glTF) |
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
- Animation code lives in `renderer/Animation/`, namespace `glacier`. A model and the clips that drive it must come from one file: `Model::Load` builds the meshes, materials, `Skeleton` and `AnimationClip`s together and caches them, and `Model::CreateGameObject` attaches an `Animator` (plus a `SkinnedMeshRenderer` for skinned meshes) per instance. Imported clips are shared, so treat them as immutable.
- Bones are addressed by node index, never by name: the skeleton order is the imported node order, parents first, and node names may repeat. A clip remembers the skeleton it was bound to and only falls back to names when another skeleton plays it (retargeting).
- Skinning has two paths: the compute pass (`GpuSkinning` + `Assets/Shader/SkinningCS.hlsl`, whose thread count must match `GpuSkinning::kThreads`) and the `GLACIER_SKINNING` variant of the geometry passes, which reads `_BoneMatrices` through `Assets/Shader/Common/Skinning.hlsli`. A compute-skinned mesh cannot be drawn in an instance batch, so `SkinnedMeshRenderer::SetInstancing(true)` turns GPU skinning off (a batch draws the bind pose and skins every instance in the vertex shader).
- The skinning pools are fixed size: `GpuSkinning` 32MB (~599k skinned vertices), `BoneMatrixPool` 32768 matrices over 3 frames in flight, `InstanceBuffer` 4096 instances per frame. A mesh that does not fit silently takes the slower path (vertex-shader skinning / bind pose), so keep the budget in mind when adding characters.
- Commits: English imperative, capitalized, one thing per commit (`Add TAA`, `Fix speculer ao`, `Refactor Postprocess`). Branch prefix `codex/`.

## Common tasks

- **Animated / skinned models**: load with `Model.GenerateGameObject(cmd_buffer, "assets\\model\\anim\\DemoSkin.gltf", false, 1.0)`; the importer attaches the `Animator` and the `SkinnedMeshRenderer`, reachable from Lua as `go:GetAnimator()` / `go:GetSkinnedMeshRenderer()`. Playback: `PlayClip(name)`, `CrossFade(name, seconds)`, `SetWeight(name, weight, additive)`; 1D blend tree: `ClearBlendClips()` + `AddBlendClip(name, threshold)` + `SetBlendParameter(v)`; per-clip rate and shared cycle: `SetClipSpeed` / `SetClipPhase`; events: `AddEvent(clip_name, time, name)` + `SetEventCallback`; root motion: `SetRootMotion(true)` with `SetRootMotionBone("Root/Armature/Hips")` on a Mixamo rig, then read `root_motion_position` / `root_motion_rotation` and move the entity yourself. See `Script/animscene.lua` and `Script/ybotscene.lua` + `Behaviour/PlayerController.cpp`.
- **Importing animation assets**: clips are converted in `Model::ImportAnimations` and cached to `<model>.gclip` next to the source, so a cache is reused while the source file is unchanged - delete it or bump `AnimationClipCache::kVersion` when the conversion changes. Mixamo packs: `blender -b --python tools/mixamo_to_gltf.py -- <folder> <out.gltf> [model.fbx]` merges the character and one FBX per clip (renamed after its file) into a single glTF and writes textures next to it, because the importer cannot read embedded ones.
- **Skinning render path**: `SkinnedMeshRenderer::UpdateRenderData` runs once per frame and fills the matrices every pass uploads from; `BoneMatrixPool::BeginFrame` and `GpuSkinning::Dispatch` run before the passes draw. A new geometry pass that should draw skinned meshes needs the `GLACIER_SKINNING` variant (see `PrePass.hlsl`).
- **New post-process / pass**: follow `renderer/Render/PostProcess/FXAA.{h,cpp}`; register passes with `render_graph_.AddPass(name, setup, execute)` and mark programs with `Program::AddPass(passName)` in `DeferredRenderer` / `ForwardRenderer` (`Setup()` and `Execute()`); load the shader via `PostProcessMaterial("<name>", TEXT("<FileName>"))`.
- **Shaders**: files live at `Assets/Shader/<Name>.hlsl` and are loaded by base file name only (case-sensitive match). Default entry points are `main_vs` / `main_hs` / `main_ds` / `main_gs` / `main_ps` / `main_cs` (`DefaultShaderEntry` in `renderer/Render/Base/Enums.h`); shared code goes in `Assets/Shader/Common/*.hlsli`; `GLACIER_REVERSE_Z` is injected automatically.
- **Lua bindings**: use `LUX_IMPL` / `LUX_IMPL_INHERIT`, `LUX_CTOR`, `LUX_FUNC` / `LUX_PROP`, `LUX_IMPL_END` in the class `.cpp` (`renderer/Behaviour/Fall.cpp`, `renderer/App.cpp`); global functions and constants via `LUX_GLOBAL_FUNC` / `LUX_CONSTANT`; access from Lua as `require("Glacier.<ClassName>")`. Registration is static and duplicate names `exit(-1)` (`renderer/Lux/Register.cpp`).
- **New components**: derive from `Component` (`renderer/Core/Component.h`: `OnAwake` / `OnEnable` / `OnDisable` / `OnDestroy`) or `Behaviour` (adds `Update` / `LateUpdate` / `DrawOverlay` - the latter draws into the editor frame and takes neither mouse nor keyboard, which is what an in-game HUD wants, see `Behaviour/DemoHud`), attach with `game_object.AddComponent<T>(...)`; implement `DrawInspector()` / `OnDrawGizmos()` when the editor needs them.
- **New source files**: `renderer/CMakeLists.txt` globs `renderer/**` with `CONFIGURE_DEPENDS` (excluding `3rdParty`, `.vscode`), so adding files usually needs no CMake edit.
- **Physics changes**: verify by switching `Script/main.lua` to `sm:Load("physics", SceneLoadMode.kSingle)` and watching stacking stability and penetration.
