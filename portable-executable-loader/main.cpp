
#include <fstream>
#include <iostream>

#include "PELoader.h"

int main() {
	PELoader loader = PELoader();
	std::vector<std::byte> dllBuffer = std::vector<std::byte>();

	std::ifstream file("C:\\Projects\\Dll1.dll", std::ios::binary);

	char fileByte = 0;
	while (file.get(fileByte)) {
		dllBuffer.push_back((std::byte)fileByte);
	}
	HMODULE test = loader.loadLibrary(dllBuffer);

	std::cout << "test: " << test << std::endl;
	PIMAGE_DOS_HEADER imageDosHeader = (PIMAGE_DOS_HEADER)test;
	std::cout << "test2: " << imageDosHeader->e_magic;

	loader.freeLibrary(test);

	return 0;
}