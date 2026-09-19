
#include <fstream>
#include <iostream>

#include "PELoader.h"

int main() {
	// This file is only for testing

	PELoader loader = PELoader();
	std::vector<std::byte> dllBuffer = std::vector<std::byte>();

	std::ifstream file("C:\\Projects\\Dll1.dll", std::ios::binary);

	char fileByte = 0;
	while (file.get(fileByte)) {
		dllBuffer.push_back((std::byte)fileByte);
	}
	file.close();

	HMODULE libraryPtr;
	try {
		libraryPtr = loader.loadLibrary(dllBuffer);
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Error: " << e.what() << '\n';
		return 1;
	}

	PIMAGE_DOS_HEADER imageDosHeader = (PIMAGE_DOS_HEADER)libraryPtr;
	PIMAGE_NT_HEADERS imageNtHeaders = (PIMAGE_NT_HEADERS)((std::byte*)libraryPtr + imageDosHeader->e_lfanew);
	PIMAGE_SECTION_HEADER imageSectionHeaders = IMAGE_FIRST_SECTION(imageNtHeaders);
	DWORD exportDirectoryRVAPtr = imageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	PIMAGE_EXPORT_DIRECTORY imageExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((byte*)libraryPtr + exportDirectoryRVAPtr);
	DWORD functionsCount = imageExportDirectory->NumberOfNames;
	DWORD* exportFunctionsNamesRVA = (DWORD*)((byte*)libraryPtr + imageExportDirectory->AddressOfNames);

	char* functionName = nullptr;

	for (int i = 0; i < functionsCount; i++) {
		functionName = (char*)((byte*)libraryPtr + exportFunctionsNamesRVA[i]);
		std::cout << "function name " << i << ": " << functionName << std::endl;
	}

	typedef int(WINAPI* MULTIPLEFUNC)(int, int);
	MULTIPLEFUNC multiple = (MULTIPLEFUNC)loader.getProcAddress(libraryPtr, "multiply");
	std::cout << "multiple test: " << multiple(6,7);

	loader.freeLibrary(libraryPtr);

	return 0;
}