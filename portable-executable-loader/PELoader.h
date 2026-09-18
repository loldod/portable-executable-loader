#pragma once

#include <Windows.h>
#include <vector>
#include "exceptions.h"

class PELoader
{
public:
	HMODULE loadLibrary(std::vector<std::byte> dllBuffer);
	void freeLibrary(HMODULE loadAddress);
	FARPROC GetProcAddress(HMODULE moduleAddress, LPCSTR funcName);

private:
	PIMAGE_NT_HEADERS getImageNtHeaders(HMODULE libraryModule);
	std::byte* allocateVirtualImage(PIMAGE_NT_HEADERS imageNtHeaders);
	void mapImageHeaders(std::byte* sourceImage, std::byte* destinationImage, PIMAGE_NT_HEADERS imageNtHeaders);
	void mapImageSections(std::byte* sourceImage, std::byte* destinationImage, PIMAGE_NT_HEADERS imageNtHeaders);
	BOOL runEntryPoint(HMODULE libraryModule, PIMAGE_NT_HEADERS imageNtHeaders, DWORD fdwReason);
};


