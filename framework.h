// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <objbase.h>
#include <shlwapi.h>
#include <wininet.h>
#include <wincodec.h>
#include <d2d1.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <crtdbg.h>

// C++ stdlib
#include <string>
#include <format>
#include <functional>
#include <algorithm>
#include <mutex>
#include <cmath>
#include <numbers>