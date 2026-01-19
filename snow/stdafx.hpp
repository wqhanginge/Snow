#pragma once

#define WIN32_LEAN_AND_MEAN

#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <thread>

#include <tchar.h>
#include <Windows.h>
#include <shellapi.h>

#include <wrl/client.h>
#include <wincodec.h>
#include <dxgi1_6.h>
#include <d3d11_4.h>
#include <d2d1_3.h>
#include <d2d1_3helper.h>
#include <dcomp.h>

#include "resource.h"

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dcomp.lib")


#define BEGINCASECODE   {
#define ENDCASECODE     }


template<typename Ty>
constexpr const Ty& clamp(const Ty& val, const Ty& left, const Ty& right) {
    return std::min<Ty>(std::max<Ty>(val, left), right);
}
