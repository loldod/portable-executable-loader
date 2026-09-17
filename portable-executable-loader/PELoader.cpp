#include "PELoader.h"
#include <Windows.h>

typedef BOOL(WINAPI* DLLMAIN)(HINSTANCE, DWORD, LPVOID);

HMODULE PELoader::loadLibrary(std::vector<std::byte> dllBuffer) {
	if (dllBuffer.size() < sizeof(PIMAGE_DOS_HEADER)) {
		return NULL;
	}

	std::byte* bufferImagePtr = dllBuffer.data();

	PIMAGE_DOS_HEADER imageDosHeader = (PIMAGE_DOS_HEADER)bufferImagePtr;
	if (IMAGE_DOS_SIGNATURE != imageDosHeader->e_magic) { return NULL; }

	PIMAGE_NT_HEADERS imageNtHeaders = (PIMAGE_NT_HEADERS)((std::byte*)bufferImagePtr + imageDosHeader->e_lfanew);
	if (IMAGE_NT_SIGNATURE != imageNtHeaders->Signature) { return NULL; }

	
	std::byte* virtualImage = (std::byte*)VirtualAlloc(
		(LPVOID)imageNtHeaders->OptionalHeader.ImageBase, 
		imageNtHeaders->OptionalHeader.SizeOfImage,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_EXECUTE_READWRITE
	);

	if (virtualImage == NULL) { return NULL; }

	std::memcpy(virtualImage, bufferImagePtr, imageNtHeaders->OptionalHeader.SizeOfHeaders);

	PIMAGE_SECTION_HEADER imageSectionHeaders = IMAGE_FIRST_SECTION(imageNtHeaders);
	for (int i = 0; i < imageNtHeaders->FileHeader.NumberOfSections; i++) {
		IMAGE_SECTION_HEADER currentSectionHeader = imageSectionHeaders[i];
		std::memcpy(
			virtualImage + currentSectionHeader.VirtualAddress,
			bufferImagePtr + currentSectionHeader.PointerToRawData,
			currentSectionHeader.SizeOfRawData
		);
	}

	DWORD entryPointRva = imageNtHeaders->OptionalHeader.AddressOfEntryPoint;
	if (entryPointRva != NULL) {
		DLLMAIN dllEntryPoint = (DLLMAIN)((std::byte*)(virtualImage + entryPointRva));
		dllEntryPoint(
			(HINSTANCE)virtualImage,
			DLL_PROCESS_ATTACH,
			NULL
		);
	}

	return (HMODULE)virtualImage;
}

void PELoader::freeLibrary(HMODULE loadAddress) {
	if (loadAddress != NULL) {
		VirtualFree(loadAddress, 0, MEM_RELEASE);
	}
}