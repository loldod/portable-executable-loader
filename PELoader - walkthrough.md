# stage 0 - load base + running entry point
### things to check:
- can I cast vector of bytes as PIMAGE_DOS_HEADER? - yes
- how do i store the dll in memory so windows can execute it? - VirtualAlloc
- how much do i need to allocate? - SizeOfImage (from IMAGE_OPTIONAL_HEADER)
- how mapping sections works? - for each section (IMAGE_SECTION_HEADER), copy data from PointerToRawData to VirtualAddress.
- how to run AddressOfEntryPoint? - changing signature to original and just running it

### TODO:
- validate input file is valid DLL
- creating virtual memory
- copying the base of the dll (headers)
- map sections into memory
- calling entry point
- throw exceptions

# stage 1 - support for imports
## thing to check:
- where imports are stored? - .idata section
- how to get IMAGE_IMPORT_DESCRIPTOR pointer - its the .idata section pointer inside the data directories
- how to iterate over import descriptor - ++
- PIMAGE_IMPORT_DESCRIPTOR format, FirstThunk and OriginalFirstThunk
- how to free all the libraries - iterating again in the end and freelibrary for each library in dll
### TODO:
- use DbgHelp.h for shorter code
- get PIMAGE_IMPORT_DESCRIPTOR
- load library for each descriptor and replace functions pointers
- free libraries in the freeLibrary function

# stage 2 - support for relocations
it originally support for imports but I got mistaken
### things to check:
- .reloc section, PIMAGE_BASE_RELOCATION, entries structure
### TODO:
- iterate over PIMAGE_BASE_RELOCATIONs
- iterate over the entries in that structure
- fix entry address 

# stage 3 - support for exports
### things to check:
- I mapped the sections, shouldnt GetProcAddress work? - no, dont know why

### TODO:
- implementing GetProcAddress logic

# full volatile - without using LoadLibrary

### testing:
- for resolving api set, I tried GetApiSetModuleBaseName from apiquery2.h but linker can't find this function implementation so I don't think this function is used widely.
- tried to load system dlls like kernelbase.dll and got some error on entry point at RtlpCaptureContext function, I think that I am not supposed to load them.
- I decided to get the handle to loaded system libraries (they are already loaded, so I will not use LoadLibrary).
### things to check:
- how LoadLibrary locates DLLs from disk - SearchPathW
- where I should store the libraries addresses - maybe global mapping of name to address
- why some dlls doesnt exists - "Windows API sets"
### TODO:
- turning functions to static
- implementing my GetModuleHandle function
- loading dlls recursively:
	1. mapping dlls to some mapping of module name -> handle
	2. don't free library if other loaded pe uses it, so implement reference counter map also
- clean code - fix old casting way, make functions parameters consts if possible, change typing to winapi typing (std::byte* -> PBYTE for example).