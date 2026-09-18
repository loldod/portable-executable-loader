#pragma once

#include <Windows.h>
#include <vector>
#include "exceptions.h"
#pragma comment(lib, "dbghelp.lib")

class PELoader
{
public:
	HMODULE loadLibrary(std::vector<std::byte> dllBuffer);
	void freeLibrary(HMODULE loadAddress);
	FARPROC getProcAddress(HMODULE moduleAddress, LPCSTR funcName);

private:
	PIMAGE_NT_HEADERS getImageNtHeaders(HMODULE libraryModule);
	std::byte* allocateVirtualImage(PIMAGE_NT_HEADERS imageNtHeaders);
	void mapImageHeaders(std::byte* sourceImage, std::byte* destinationImage, PIMAGE_NT_HEADERS imageNtHeaders);
	void mapImageSections(std::byte* sourceImage, std::byte* destinationImage, PIMAGE_NT_HEADERS imageNtHeaders);
	
	void loadFunctionImport(std::byte* image, HMODULE importedLibrary, PIMAGE_THUNK_DATA importAddressTable);
	void loadLibraryImport(std::byte* image, PIMAGE_IMPORT_DESCRIPTOR importDescriptor);
	void loadImageImports(std::byte* image, PIMAGE_NT_HEADERS imageNtHeaders);
	
	BOOL runEntryPoint(HMODULE libraryModule, PIMAGE_NT_HEADERS imageNtHeaders, DWORD fdwReason);
	
	void freeImportedLibraries(std::byte* image, PIMAGE_NT_HEADERS imageNtHeaders);
};


