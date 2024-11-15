#pragma once

// Util.h: some standalone functions

// Loads a string from Win32 resources
std::wstring LoadStringFromResource(unsigned id);

// Wrapper for OutputDebugString() + std::format()
template<typename... Args>
inline void PrintLnDebug(const std::wformat_string<Args...> fmt, Args&&... args)
{
	OutputDebugString(std::vformat(fmt.get(), std::make_wformat_args(args...)).c_str());
	OutputDebugString(L"\n");
}
