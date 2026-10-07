#include "PELoader.h"

typedef BOOL(WINAPI* DLLMAIN)(HINSTANCE, DWORD, LPVOID);

PIMAGE_NT_HEADERS PELoader::getImageNtHeaders(const HMODULE libraryModule) {
	auto imageDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(libraryModule);
	if (IMAGE_DOS_SIGNATURE != imageDosHeader->e_magic) {
		throw InvalidDLLException("DOS header magic is invalid!");
	}

	auto imageNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<PBYTE>(libraryModule) + imageDosHeader->e_lfanew);
	if (IMAGE_NT_SIGNATURE != imageNtHeaders->Signature) {
		throw InvalidDLLException("NT header magic is invalid!");
	}
	
	return imageNtHeaders;
}

PBYTE PELoader::allocateVirtualImage(const PIMAGE_NT_HEADERS imageNtHeaders) {
	auto virtualImage = reinterpret_cast<PBYTE>(
		VirtualAlloc(
			reinterpret_cast<LPVOID>(imageNtHeaders->OptionalHeader.ImageBase), // allocate at preferred address 
			imageNtHeaders->OptionalHeader.SizeOfImage,
			MEM_COMMIT | MEM_RESERVE,
			PAGE_EXECUTE_READWRITE
		)
	);
	if (virtualImage == NULL) {
		virtualImage = reinterpret_cast<PBYTE>(
				VirtualAlloc(
				NULL, // allocate at random address
				imageNtHeaders->OptionalHeader.SizeOfImage,
				MEM_COMMIT | MEM_RESERVE,
				PAGE_EXECUTE_READWRITE
			)
		);
		if (virtualImage == NULL) {
			throw ImageAllocationExcepetion("Cannot allocate virtual image");
		}
	}
	return virtualImage;
}

void PELoader::mapImageHeaders(const PBYTE sourceImage, PBYTE destinationImage, const PIMAGE_NT_HEADERS imageNtHeaders) {
	std::memcpy(destinationImage, sourceImage, imageNtHeaders->OptionalHeader.SizeOfHeaders);
}

void PELoader::mapImageSections(const PBYTE sourceImage, PBYTE destinationImage, const PIMAGE_NT_HEADERS imageNtHeaders) {
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

void PELoader::loadFunctionImport(PBYTE image, const HMODULE importedLibrary, const PIMAGE_THUNK_DATA importAddressTable) {
	PIMAGE_IMPORT_BY_NAME importByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(image + importAddressTable->u1.AddressOfData);

	FARPROC importedFunction = GetProcAddress(importedLibrary, importByName->Name);
	if (importedFunction == NULL) {
		throw ImportedFunctionNotFoundException("Could not find imported function in library");
	}

	importAddressTable->u1.Function = reinterpret_cast<LONGLONG>(importedFunction);
}

void PELoader::loadLibraryImport(PBYTE image, const PIMAGE_IMPORT_DESCRIPTOR importDescriptor) {
	char* libraryName = reinterpret_cast<char*>(image + importDescriptor->Name);
	auto importAddressTable = reinterpret_cast<PIMAGE_THUNK_DATA>(image + importDescriptor->FirstThunk);

	HMODULE importedLibrary = LoadLibraryA(libraryName);
	if (importedLibrary == NULL) {
		throw ImportedFunctionNotFoundException("Could not load imported library");
	}

	while (importAddressTable->u1.AddressOfData != NULL) {
		loadFunctionImport(image, importedLibrary, importAddressTable);
		importAddressTable++;
	}
}

void PELoader::loadImageImports(PBYTE image, const PIMAGE_NT_HEADERS imageNtHeaders) {
	IMAGE_DATA_DIRECTORY importDirectory = imageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	PIMAGE_IMPORT_DESCRIPTOR currentImportDescriptor = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(image + importDirectory.VirtualAddress);
	
	while (currentImportDescriptor->Name != NULL) {
		loadLibraryImport(image, currentImportDescriptor);
		currentImportDescriptor++;
	}
}

void PELoader::applyRelocationFixes(PBYTE image, const PIMAGE_NT_HEADERS imageNtHeaders) {
	auto baseAddressDifference = reinterpret_cast<ULONGLONG>(image) - imageNtHeaders->OptionalHeader.ImageBase;
	IMAGE_DATA_DIRECTORY relocationDirectory = imageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	auto imageBaseRelocation = reinterpret_cast<PIMAGE_BASE_RELOCATION>(image + relocationDirectory.VirtualAddress);

	if (baseAddressDifference == 0 || relocationDirectory.VirtualAddress == NULL) {
		return;
	}

	while (imageBaseRelocation->VirtualAddress != NULL) {
		DWORD relocationEntriesCount = (imageBaseRelocation->SizeOfBlock - sizeof(PIMAGE_BASE_RELOCATION)) / sizeof(WORD);
		PWORD relocationEntries = reinterpret_cast<PWORD>(reinterpret_cast<PBYTE>(imageBaseRelocation) + sizeof(PIMAGE_BASE_RELOCATION));

		for (int i = 0; i < relocationEntriesCount; i++) {
			WORD entryType = relocationEntries[i] >> RELOCATION_ENTRY_TYPE_OFFSET;
			WORD entryOffset = relocationEntries[i] & RELOCATION_ENTRY_OFFSET_MASK;

			if (entryType == IMAGE_REL_BASED_ABSOLUTE) {
				continue;
			}

			PBYTE relocationAddress = image + imageBaseRelocation->VirtualAddress + entryOffset;

			if (entryType == IMAGE_REL_BASED_DIR64) {
				*reinterpret_cast<PULONG_PTR>(relocationAddress) += baseAddressDifference;
			}
			else if (entryType == IMAGE_REL_BASED_HIGHLOW) {
				*reinterpret_cast<PDWORD>(relocationAddress) += baseAddressDifference;
			}
		}

		imageBaseRelocation += imageBaseRelocation->SizeOfBlock;
	}
}

BOOL PELoader::runEntryPoint(const HMODULE libraryModule, const PIMAGE_NT_HEADERS imageNtHeaders, DWORD fdwReason) {
	DWORD entryPointRva = imageNtHeaders->OptionalHeader.AddressOfEntryPoint;
	if (entryPointRva != NULL) {
		DLLMAIN dllEntryPoint = reinterpret_cast<DLLMAIN>(reinterpret_cast<PBYTE>(libraryModule) + entryPointRva);
		return dllEntryPoint(
			reinterpret_cast<HINSTANCE>(libraryModule),
			fdwReason,
			NULL
		);
	}
	return TRUE;
}

void PELoader::freeImportedLibraries(PBYTE image, PIMAGE_NT_HEADERS imageNtHeaders) {
	IMAGE_DATA_DIRECTORY importDirectory = imageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	auto currentImportDescriptor = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(image + importDirectory.VirtualAddress);
	char* libraryName = NULL;
	HMODULE libraryPtr = NULL;

	while (currentImportDescriptor->Name != NULL) {
		libraryName = reinterpret_cast<char*>(image + currentImportDescriptor->Name);
		libraryPtr = GetModuleHandleA(libraryName);
		if (libraryPtr) {
			FreeLibrary(libraryPtr);
		}
		currentImportDescriptor++;
	}
}

HMODULE PELoader::loadLibrary(const PBYTE dllBuffer) {
	PIMAGE_NT_HEADERS imageNtHeaders = getImageNtHeaders(reinterpret_cast<HMODULE>(dllBuffer));
	
	PBYTE virtualImage = allocateVirtualImage(imageNtHeaders);
	mapImageHeaders(dllBuffer, virtualImage, imageNtHeaders);
	mapImageSections(dllBuffer, virtualImage, imageNtHeaders);
	
	applyRelocationFixes(virtualImage, imageNtHeaders);
	loadImageImports(virtualImage, imageNtHeaders);

	runEntryPoint(reinterpret_cast<HMODULE>(virtualImage), imageNtHeaders, DLL_PROCESS_ATTACH);

	return reinterpret_cast<HMODULE>(virtualImage);
}

void PELoader::freeLibrary(HMODULE loadAddress) {
	PIMAGE_NT_HEADERS imageNtHeaders = getImageNtHeaders(loadAddress);

	runEntryPoint(loadAddress, imageNtHeaders, DLL_PROCESS_DETACH);
	freeImportedLibraries(reinterpret_cast<PBYTE>(loadAddress), imageNtHeaders);
	VirtualFree(loadAddress, 0, MEM_RELEASE);
}

FARPROC PELoader::getProcAddress(HMODULE moduleAddress, LPCSTR funcName) {
	PIMAGE_NT_HEADERS imageNtHeaders = getImageNtHeaders(moduleAddress);
	PBYTE image = reinterpret_cast<PBYTE>(moduleAddress);
	IMAGE_DATA_DIRECTORY exportDirectory = imageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	auto imageExportDirectory = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(image + exportDirectory.VirtualAddress);
	
	auto exportFunctionsNamesRVA = reinterpret_cast<PDWORD>(image + imageExportDirectory->AddressOfNames);
	auto exportFunctionsAddressesRVA = reinterpret_cast<PDWORD>(image + imageExportDirectory->AddressOfFunctions);

	LPCSTR exportedFunctionName = NULL;

	for (int i = 0; i < imageExportDirectory->NumberOfNames; i++) {
		exportedFunctionName = reinterpret_cast<LPCSTR>(image + exportFunctionsNamesRVA[i]);

		if (strcmp(exportedFunctionName, funcName) == 0) {
			return reinterpret_cast<FARPROC>(image + exportFunctionsAddressesRVA[i]);
		}
	}

	return NULL;
}