#ifndef __RENDER_CORE_H_
#define __RENDER_CORE_H_

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> 

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#if defined(CreateWindow)
#undef CreateWindow
#endif

#include <wrl.h>
using namespace Microsoft::WRL;

#include "d3dx12.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>
#include <DirectXColors.h>
#include <d3dcompiler.h>
#include <windowsx.h>
#include "../ext/DirectXTex/DirectXTex/DirectXTex.h"

using namespace DirectX;

#include <cstdint>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <thread>
#include <vector>
#include <queue>
#include <set>
#include <functional>

namespace fs = std::experimental::filesystem;

#include "Helpers.h"


#endif
