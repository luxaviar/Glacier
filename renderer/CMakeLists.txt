set(RENDERER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB_RECURSE RENDERER_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${RENDERER_ROOT}/*.cpp"
)
list(FILTER RENDERER_ENGINE_SOURCES EXCLUDE REGEX "3rdParty")
list(FILTER RENDERER_ENGINE_SOURCES EXCLUDE REGEX "[/\\\\]\\.vscode[/\\\\]")

file(GLOB RENDERER_LPEG_SOURCES CONFIGURE_DEPENDS
    "${RENDERER_ROOT}/3rdParty/lpeg/*.c"
)
file(GLOB RENDERER_LUA_SOURCES CONFIGURE_DEPENDS
    "${RENDERER_ROOT}/3rdParty/lua/*.c"
)

set(RENDERER_THIRDPARTY_SOURCES
    ${RENDERER_LPEG_SOURCES}
    ${RENDERER_LUA_SOURCES}
)

set_source_files_properties(
    ${RENDERER_LPEG_SOURCES}
    ${RENDERER_LUA_SOURCES}
    PROPERTIES LANGUAGE CXX
)

file(GLOB_RECURSE RENDERER_HEADERS CONFIGURE_DEPENDS
    "${RENDERER_ROOT}/*.h"
    "${RENDERER_ROOT}/*.hh"
    "${RENDERER_ROOT}/*.hpp"
    "${RENDERER_ROOT}/*.inl"
)
list(FILTER RENDERER_HEADERS EXCLUDE REGEX "[/\\\\]\\.vscode[/\\\\]")
list(FILTER RENDERER_HEADERS EXCLUDE REGEX "[/\\\\]3rdParty[/\\\\]assimp[/\\\\]")
list(FILTER RENDERER_HEADERS EXCLUDE REGEX "[/\\\\]3rdParty[/\\\\]fmt[/\\\\]")
list(FILTER RENDERER_HEADERS EXCLUDE REGEX "[/\\\\]3rdParty[/\\\\]stb_image[/\\\\]")
file(GLOB_RECURSE RENDERER_SHADERS CONFIGURE_DEPENDS
    "${RENDERER_ROOT}/Assets/*.hlsl"
    "${RENDERER_ROOT}/Assets/*.hlsli"
)

add_executable(renderer WIN32
    ${RENDERER_ENGINE_SOURCES}
    ${RENDERER_THIRDPARTY_SOURCES}
    ${RENDERER_HEADERS}
)

source_group(TREE "${RENDERER_ROOT}" FILES
    ${RENDERER_ENGINE_SOURCES}
    ${RENDERER_THIRDPARTY_SOURCES}
    ${RENDERER_HEADERS}
)

target_include_directories(renderer PRIVATE
    "${RENDERER_ROOT}"
    "${RENDERER_ROOT}/3rdParty"
    "${RENDERER_ROOT}/3rdParty/lua"
)

if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
    target_include_directories(renderer BEFORE PRIVATE
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
    )
endif()

target_compile_definitions(renderer PRIVATE
    GLACIER_REVERSE_Z
    _CONSOLE
    UNICODE
    _UNICODE
    HAS_DIRECTXTEX
    HAS_DIRECTXTK
    IMGUI_DEFINE_MATH_OPERATORS
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Debug>:IS_DEBUG=true>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Release>:IS_DEBUG=false>
)

if(MSVC)
    target_compile_options(renderer PRIVATE
        /W3
        /sdl
        /permissive-
        /utf-8
        /wd4996
    )
endif()

target_precompile_headers(renderer PRIVATE
    "${RENDERER_ROOT}/Common/pch.h"
)

set_target_properties(renderer PROPERTIES
    DEBUG_POSTFIX ""
    VS_DEBUGGER_WORKING_DIRECTORY "${RENDERER_ROOT}"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
)

find_package(assimp CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(Stb REQUIRED)

target_include_directories(renderer BEFORE PRIVATE
    "${Stb_INCLUDE_DIR}"
)

target_link_libraries(renderer PRIVATE
    assimp::assimp
    Microsoft::DirectXTex
    Microsoft::DirectXTK
    imgui::imgui
    imguizmo::imguizmo
    dxgi
    dxguid
    d3dcompiler
    d3d11
    d3d12
)