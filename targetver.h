#pragma once

// Try to target Windows 7 (which is the earliest officially supported by SDK and VS2022)
#include <winsdkver.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7                   
#include <SDKDDKVer.h>
