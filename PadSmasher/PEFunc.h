#pragma once
#include <string>

struct PEFunc
{
	std::string Name;
	uintptr_t   RawAddr = 0;
	uintptr_t   RVAAddr = 0;
	uintptr_t   PadStart = 0;
	size_t      PadLen = 0;
};

// Converts RVAs to a raw pointer offset
// https://stackoverflow.com/questions/9955744/getting-offset-in-file-from-rva
uintptr_t RVAToRaw(uintptr_t RVA, PIMAGE_SECTION_HEADER SectionRaw, size_t Count)
{
	size_t i = 0;
	for (i = 0; i < Count; ++i)
	{
		const auto section_begin_rva = SectionRaw[i].VirtualAddress;
		const auto section_end_rva = section_begin_rva + SectionRaw[i].Misc.VirtualSize;
		if (section_begin_rva <= RVA && RVA <= section_end_rva)
			break;
	}

	return RVA - SectionRaw[i].VirtualAddress + SectionRaw[i].PointerToRawData;
}
