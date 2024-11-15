#include "framework.h"
#include "Util.h"

// Util.cpp: some standalone functions

std::wstring LoadStringFromResource(unsigned id)
{
	wchar_t buffer[1024];
	if (!LoadString(GetModuleHandle(nullptr), id, buffer, sizeof(buffer) / sizeof(wchar_t))) {
		return std::format(L"(String not found: {})", id);
	}
	return std::wstring(buffer);
}
