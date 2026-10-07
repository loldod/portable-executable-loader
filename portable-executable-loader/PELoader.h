#pragma once

#include <Windows.h>
#include <vector>
#include "exceptions.h"

#define RELOCATION_ENTRY_TYPE_OFFSET 12
#define RELOCATION_ENTRY_OFFSET_MASK 0x0FFF

class PELoader
{
public:
	HMODULE loadLibrary(const std::byte* dllBuffer);
	void freeLibrary(HMODULE loadAddress);
	FARPROC getProcAddress(HMODULE moduleAddress, LPCSTR funcName);

private:
	PIMAGE_NT_HEADERS getImageNtHeaders(const HMODULE libraryModule);
	std::byte* allocateVirtualImage(const PIMAGE_NT_HEADERS imageNtHeaders);
	void mapImageHeaders(const std::byte* sourceImage, std::byte* destinationImage, const PIMAGE_NT_HEADERS imageNtHeaders);
	void mapImageSections(const std::byte* sourceImage, std::byte* destinationImage, const PIMAGE_NT_HEADERS imageNtHeaders);
	
	void loadFunctionImport(std::byte* image, const HMODULE importedLibrary, const PIMAGE_THUNK_DATA importAddressTable);
	void loadLibraryImport(std::byte* image, const PIMAGE_IMPORT_DESCRIPTOR importDescriptor);
	void loadImageImports(std::byte* image, const PIMAGE_NT_HEADERS imageNtHeaders);
	
	void applyRelocationFixes(std::byte* image, const PIMAGE_NT_HEADERS imageNtHeaders);

	BOOL runEntryPoint(const HMODULE libraryModule, const PIMAGE_NT_HEADERS imageNtHeaders, DWORD fdwReason);
	
	void freeImportedLibraries(std::byte* image, const PIMAGE_NT_HEADERS imageNtHeaders);
};


