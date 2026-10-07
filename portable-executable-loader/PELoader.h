#pragma once

#include <Windows.h>
#include <vector>
#include "exceptions.h"

#define RELOCATION_ENTRY_TYPE_OFFSET 12
#define RELOCATION_ENTRY_OFFSET_MASK 0x0FFF

class PELoader
{
public:
	HMODULE loadLibrary(const PBYTE dllBuffer);
	void freeLibrary(HMODULE loadAddress);
	FARPROC getProcAddress(HMODULE moduleAddress, LPCSTR funcName);

private:
	PIMAGE_NT_HEADERS getImageNtHeaders(const HMODULE libraryModule);
	PBYTE allocateVirtualImage(const PIMAGE_NT_HEADERS imageNtHeaders);
	void mapImageHeaders(const PBYTE sourceImage, PBYTE destinationImage, const PIMAGE_NT_HEADERS imageNtHeaders);
	void mapImageSections(const PBYTE sourceImage, PBYTE destinationImage, const PIMAGE_NT_HEADERS imageNtHeaders);
	
	void loadFunctionImport(PBYTE image, const HMODULE importedLibrary, const PIMAGE_THUNK_DATA importAddressTable);
	void loadLibraryImport(PBYTE image, const PIMAGE_IMPORT_DESCRIPTOR importDescriptor);
	void loadImageImports(PBYTE image, const PIMAGE_NT_HEADERS imageNtHeaders);
	
	void applyRelocationFixes(PBYTE image, const PIMAGE_NT_HEADERS imageNtHeaders);

	BOOL runEntryPoint(const HMODULE libraryModule, const PIMAGE_NT_HEADERS imageNtHeaders, DWORD fdwReason);
	
	void freeImportedLibraries(PBYTE image, const PIMAGE_NT_HEADERS imageNtHeaders);
};


