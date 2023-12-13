#pragma once

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <wrl/client.h>
#include <cstdint>

#include <d3d12.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>

#include "ThrowIfFailed.h"

#ifndef _DEBUG
#define assert(ignore)((void)0)
#endif // !(_DEBUG)

