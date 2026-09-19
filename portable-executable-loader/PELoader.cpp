#pragma comment(lib, "dbghelp.lib")

#include "PELoader.h"
#include <DbgHelp.h>

typedef BOOL(WINAPI* DLLMAIN)(HINSTANCE, DWORD, LPVOID);

PIMAGE_NT_HEADERS PELoader::getImageNtHeaders(HMODULE libraryModule) {
	PIMAGE_DOS_HEADER imageDosHeader = (PIMAGE_DOS_HEADER)libraryModule;
	if (IMAGE_DOS_SIGNATURE != imageDosHeader->e_magic) {
		throw InvalidDLLException("DOS header magic is invalid!");
	}

	PIMAGE_NT_HEADERS imageNtHeaders = (PIMAGE_NT_HEADERS)((std::byte*)libraryModule + imageDosHeader->e_lfanew);
	if (IMAGE_NT_SIGNATURE != imageNtHeaders->Signature) {
		throw InvalidDLLException("NT header magic is invalid!");
	}
	
	return imageNtHeaders;
}

std::byte* PELoader::allocateVirtualImage(PIMAGE_NT_HEADERS imageNtHeaders) {
	std::byte* virtualImage = (std::byte*)VirtualAlloc(
		(LPVOID)imageNtHeaders->OptionalHeader.ImageBase,
		imageNtHeaders->OptionalHeader.SizeOfImage,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_EXECUTE_READWRITE
	);
	if (virtualImage == NULL) {
		throw ImageAllocationExcepetion("Cannot allocate virtual image");
	}
	return virtualImage;
}

void PELoader::mapImageHeaders(std::byte* sourceImage, std::byte* destinationImage, PIMAGE_NT_HEADERS imageNtHeaders) {
	std::memcpy(destinationImage, sourceImage, imageNtHeaders->OptionalHeader.SizeOfHeaders);
}

void PELoader::mapImageSections(std::byte* sourceImage, std::byte* destinationImage, PIMAGE_NT_HEADERS imageNtHeaders) {
	PIMAGE_SECTION_HEADER imageSectionHeaders = IMAGE_FIRST_SECTION(imageNtHeaders);
	for (int i = 0; i < imageNtHeaders->FileHeader.NumberOfSections; i++) {
		IMAGE_SECTION_HEADER currentSectionHeader = imageSectionHeaders[i];
		std::memcpy(
			destinationImage + currentSectionHeader.VirtualAddress,
			sourceImage + currentSectionHeader.PointerToRawData,
			currentSectionHeader.SizeOfRawData
		);
	}
}

void PELoader::loadFunctionImport(std::byte* image, HMODULE importedLibrary, PIMAGE_THUNK_DATA importAddressTable) {
	PIMAGE_IMPORT_BY_NAME importByName = (PIMAGE_IMPORT_BY_NAME)(image + importAddressTable->u1.AddressOfData);

	FARPROC importedFunction = GetProcAddress(importedLibrary, importByName->Name);
	if (importedFunction == NULL) {
		throw ImportedFunctionNotFoundException("Could not find imported function in library");
	}

	importAddressTable->u1.Function = (LONGLONG)importedFunction;
}

void PELoader::loadLibraryImport(std::byte* image, PIMAGE_IMPORT_DESCRIPTOR importDescriptor) {
	char* libraryName = (char*)(image + importDescriptor->Name);
	PIMAGE_THUNK_DATA importAddressTable = (PIMAGE_THUNK_DATA)(image + importDescriptor->FirstThunk);

	HMODULE importedLibrary = LoadLibraryA(libraryName);
	if (importedLibrary == NULL) {
		throw ImportedFunctionNotFoundException("Could not load imported library");
	}

	while (importAddressTable->u1.AddressOfData != NULL) {
		loadFunctionImport(image, importedLibrary, importAddressTable);
		importAddressTable++;
	}
}

void PELoader::loadImageImports(std::byte* image, PIMAGE_NT_HEADERS imageNtHeaders) {
	IMAGE_DATA_DIRECTORY importDirectory = (IMAGE_DATA_DIRECTORY)imageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	PIMAGE_IMPORT_DESCRIPTOR currentImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(image + importDirectory.VirtualAddress);
	
	while (currentImportDescriptor->Name != NULL) {
		loadLibraryImport(image, currentImportDescriptor);
		currentImportDescriptor++;
	}
}

void PELoader::loadImageExports(std::byte* image, PIMAGE_NT_HEADERS imageNtHeaders) {
	IMAGE_DATA_DIRECTORY exportDirectory = (IMAGE_DATA_DIRECTORY)imageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	PIMAGE_EXPORT_DIRECTORY imageExportDirectory = (PIMAGE_EXPORT_DIRECTORY)(image + exportDirectory.VirtualAddress);

	int i = 1;
}

void PELoader::applyRelocationFixes(std::byte* image, PIMAGE_NT_HEADERS imageNtHeaders) {
	ULONGLONG baseAddressDifference = (ULONGLONG)image - imageNtHeaders->OptionalHeader.ImageBase;
	IMAGE_DATA_DIRECTORY relocationDirectory = (IMAGE_DATA_DIRECTORY)imageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	PIMAGE_BASE_RELOCATION imageBaseRelocation = (PIMAGE_BASE_RELOCATION)(image + relocationDirectory.VirtualAddress);

	if (baseAddressDifference == 0 || relocationDirectory.VirtualAddress == NULL) {
		return;
	}

	while (imageBaseRelocation->VirtualAddress != NULL) {
		DWORD relocationEntriesCount = (imageBaseRelocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
		WORD* relocationEntries = (WORD*)((std::byte*)imageBaseRelocation + sizeof(PIMAGE_BASE_RELOCATION));

		for (int i = 0; i < relocationEntriesCount; i++) {
			WORD entryType = relocationEntries[i] >> 12;
			WORD entryOffset = relocationEntries[i] & 0x0FFF;

			if (entryType == IMAGE_REL_BASED_ABSOLUTE) {
				continue;
			}

			std::byte* relocationAddress = image + imageBaseRelocation->VirtualAddress + entryOffset;

			if (entryType == IMAGE_REL_BASED_DIR64) {
				*(ULONG_PTR*)(relocationAddress) += baseAddressDifference;
			}
			else if (entryType == IMAGE_REL_BASED_HIGHLOW) {
				*(DWORD*)(relocationAddress) += baseAddressDifference;
			}
		}

		imageBaseRelocation += imageBaseRelocation->SizeOfBlock;
	}
}

BOOL PELoader::runEntryPoint(HMODULE libraryModule, PIMAGE_NT_HEADERS imageNtHeaders, DWORD fdwReason) {
	DWORD entryPointRva = imageNtHeaders->OptionalHeader.AddressOfEntryPoint;
	if (entryPointRva != NULL) {
		DLLMAIN dllEntryPoint = (DLLMAIN)((std::byte*)libraryModule + entryPointRva);
		return dllEntryPoint(
			(HINSTANCE)((std::byte*)libraryModule),
			fdwReason,
			NULL
		);
	}
	return TRUE;
}

void PELoader::freeImportedLibraries(std::byte* image, PIMAGE_NT_HEADERS imageNtHeaders) {
	IMAGE_DATA_DIRECTORY importDirectory = (IMAGE_DATA_DIRECTORY)imageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	PIMAGE_IMPORT_DESCRIPTOR currentImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(image + importDirectory.VirtualAddress);
	char* libraryName = NULL;
	HMODULE libraryPtr = NULL;

	while (currentImportDescriptor->Name != NULL) {
		libraryName = (char*)(image + currentImportDescriptor->Name);
		libraryPtr = GetModuleHandleA(libraryName);
		if (libraryPtr) {
			FreeLibrary(libraryPtr);
		}
		currentImportDescriptor++;
	}
}

HMODULE PELoader::loadLibrary(std::vector<std::byte> dllBuffer) {
	std::byte* bufferImagePtr = dllBuffer.data();
	PIMAGE_NT_HEADERS imageNtHeaders = getImageNtHeaders((HMODULE)bufferImagePtr);
	
	std::byte* virtualImage = allocateVirtualImage(imageNtHeaders);
	mapImageHeaders(bufferImagePtr, virtualImage, imageNtHeaders);
	mapImageSections(bufferImagePtr, virtualImage, imageNtHeaders);
	
	applyRelocationFixes(virtualImage, imageNtHeaders);
	loadImageImports(virtualImage, imageNtHeaders);
	loadImageExports(virtualImage, imageNtHeaders);

	runEntryPoint((HMODULE)virtualImage, imageNtHeaders, DLL_PROCESS_ATTACH);

	return (HMODULE)virtualImage;
}

void PELoader::freeLibrary(HMODULE loadAddress) {
	PIMAGE_NT_HEADERS imageNtHeaders = getImageNtHeaders(loadAddress);

	runEntryPoint(loadAddress, imageNtHeaders, DLL_PROCESS_DETACH);
	freeImportedLibraries((std::byte*)loadAddress, imageNtHeaders);
	VirtualFree(loadAddress, 0, MEM_RELEASE);
}