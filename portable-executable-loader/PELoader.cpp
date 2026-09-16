#include "PELoader.h"
#include <Windows.h>



HMODULE PELoader::loadLibrary(std::vector<std::byte> dllBuffer) {
	if (dllBuffer.size() < sizeof(PIMAGE_DOS_HEADER)) {
		return NULL;
	}

	std::byte* libraryPtr = dllBuffer.data();

	PIMAGE_DOS_HEADER imageDosHeader = (PIMAGE_DOS_HEADER)libraryPtr;
	if (IMAGE_DOS_SIGNATURE != imageDosHeader->e_magic) { return NULL; }

	PIMAGE_NT_HEADERS imageNtHeaders = (PIMAGE_NT_HEADERS)((std::byte*)libraryPtr + imageDosHeader->e_lfanew);
	if (IMAGE_NT_SIGNATURE != imageNtHeaders->Signature) { return NULL; }

	LPVOID virtualImage = VirtualAlloc(
		NULL,
		imageNtHeaders->OptionalHeader.SizeOfImage,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);

	if (virtualImage == NULL) { return NULL; }

	std::memcpy(virtualImage, libraryPtr, imageNtHeaders->OptionalHeader.SizeOfHeaders);

	return (HMODULE)virtualImage;
}

void PELoader::freeLibrary(HMODULE loadAddress) {
	if (loadAddress != NULL) {
		VirtualFree(loadAddress, 0, MEM_RELEASE);
	}
}