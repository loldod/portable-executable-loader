# stage 0
## things to check:
- can I cast vector of bytes as PIMAGE_DOS_HEADER? - yes
- how do i store the dll in memory so windows can execute it? - VirtualAlloc
- how much do i need to allocate? - SizeOfImage (from IMAGE_OPTIONAL_HEADER)
- how mapping sections works? - for each section (IMAGE_SECTION_HEADER), copy data from PointerToRawData to VirtualAddress.
- how to run AddressOfEntryPoint? - changing signature to original and just running it

## TODO:
- validate input file is valid DLL
- creating virtual memory
- copying the base of the dll (headers)
- map sections into memory
- calling entry point
- throw exceptions

#