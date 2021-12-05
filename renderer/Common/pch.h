#pragma once

#include <stdexcept>
#include <memory>
#include <cassert>

// target Windows 7 or later
//#define _WIN32_WINNT 0x0601
#include <sdkddkver.h>
// The following #defines disable a bunch of unused windows stuff. If you 
// get weird errors when trying to do some windows stuff, try removing some
// (or all) of these defines (it will increase build time though).
#ifndef FULL_WINTARD
#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
//define NOSYSMETRICS
#define NOMENUS
//#define NOICONS
#define NOSYSCOMMANDS
#define NORASTEROPS
#define OEMRESOURCE
#define NOATOM
//#define NOCLIPBOARD
#define NOCOLOR
//#define NOCTLMGR
#define NODRAWTEXT
#define NOKERNEL
//#define NONLS
#define NOMEMMGR
#define NOMETAFILE
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NORPC
#define NOPROXYSTUB
#define NOIMAGE
#define NOTAPE
#endif

#define NOMINMAX

//#define STRICT

#include <Windows.h>
#include <wrl.h>
using namespace Microsoft::WRL;

#pragma comment(lib,"dxgi.lib")

//When using runtime compiled HLSL shaders 
//copy the D3dcompiler_47.dll to the same folder as the binary executable when distributing your project.
//C:\Program Files (x86)\Windows Kits\10\Redist\D3D\.
#pragma comment(lib,"D3DCompiler.lib")
