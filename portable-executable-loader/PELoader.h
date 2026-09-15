#pragma once

#include <Windows.h>
#include <vector>

class PELoader
{
public:
	HMODULE loadLibrary(std::vector<std::byte> dllBuffer);
	void freeLibrary(HMODULE loadAddress);
	FARPROC GetProcAddress(HMODULE moduleAddress, LPCSTR funcName);
};

