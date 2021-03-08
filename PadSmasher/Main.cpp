#include <Windows.h>
#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <random>

#include "ArgParse.h"
#include "PEFunc.h"

std::vector<uint8_t> LoadFile(const std::string& FileName)
{
	auto FilePath = std::filesystem::current_path();
	FilePath += FileName;

	std::ifstream ReadFile(FilePath);
	if (!ReadFile.is_open())
		throw std::runtime_error("Unable to open file: " + FilePath.string());

	std::vector<uint8_t> File;
	std::copy(std::istream_iterator<uint8_t>(ReadFile), std::istream_iterator<uint8_t>(), std::back_inserter(File));

	ReadFile.close();

	return File;
}

void WriteFile(const std::vector<uint8_t>& File, const std::string& FileName)
{
	auto FilePath = std::filesystem::current_path();
	FilePath += FileName + ".padsmashed";

	std::ofstream WriteFile;
	WriteFile.open(FilePath, std::ios::out | std::ios::binary);
	WriteFile.write((const char*)File.data(), File.size());
	WriteFile.close();
}

// Currently, a limitation of this implementation is that it can only parse on the
// bit size it was compiled in. IE x86_64 vs x86_32. Should change this later
std::vector<PEFunc> ParseMapFile(const std::string& FileName, const std::vector<uint8_t>& PEFile)
{
	std::vector<PEFunc> FunctionsParsed;
	const uintptr_t ImagePtr = (const uintptr_t)PEFile.data();

	auto DosHeader = (PIMAGE_DOS_HEADER)ImagePtr;
	auto NTHeader = (PIMAGE_NT_HEADERS)(ImagePtr + DosHeader->e_lfanew);
	auto Section = (PIMAGE_SECTION_HEADER)(ImagePtr + DosHeader->e_lfanew + sizeof IMAGE_NT_HEADERS);
	auto SectionNum = NTHeader->FileHeader.NumberOfSections;
	uintptr_t ImageBase = NTHeader->OptionalHeader.ImageBase;

	// Trimmed filename that removes the extension
	// Files should only end in either .dll or .exe, so safe to assume
	auto MapFileName = FileName.substr(0, FileName.length() - 3);
	MapFileName += "map";
	auto MapFile = LoadFile(MapFileName);
	if (MapFile.empty())
		throw new std::runtime_error("Unable to locate .map file for " + FileName);

	// Treat the file as one giant string
	std::stringstream SS((char*)MapFile.data());

	std::string Line;
	Line.reserve(4096);

	bool SymbolsFound = false;
	size_t SkipCount = 0;

	// FIXME: std::getline performs unnecessary copies of data constantly,
	// Would be preferable to just create std::string_view on the data
	// we've already loaded into memory
	while (std::getline(SS, Line))
	{
		// Parsing .map files are extremely messy as the file format is incredibly old
		// and not well defined either. Expect strange logic below. MSDN unfortunately
		// provide no documentation so we are left writing our own parser.
		// https://www.codeproject.com/Articles/3472/Finding-Crash-Information-Using-the-MAP-File

		// Loop until we fine the line "Publics by Value" while skipping lines less than the minimum
		// size for function defines
		if ((!SymbolsFound && Line.find("Publics by Value") == std::string::npos) || Line.length() <= 21)
			continue;

		SymbolsFound = true;

		// Skip two lines of white space
		if (SkipCount < 2)
		{
			SkipCount++;
			continue;
		}

		// Now we're at the lines that matter. Map files lay out functions in the formate below
		// Section:Offset      MangeledName               RVA+Base     Lib:ObjectFile
		// 
		// 0001:00000000       _WinMain@16                00401000 f   MAPFILE.obj
		//
		// As you can see, this is *incredibly* bad and really should be replaced by a better
		// format, but there is no alternative.

		PEFunc Function{};

		// Skips Section:Offset and whitepace
		std::string SymbolStr = Line.substr(21);
		// Substr to the first whitespace to get only the MangeledName
		SymbolStr = SymbolStr.substr(0, SymbolStr.find_first_of(' '));
		Function.Name = SymbolStr;

		// Skips Section:Offset, following whitespace, and MangeledName
		std::string RVAString = Line.substr(21 + SymbolStr.length());
		// Skips until we reach the first non whitespace
		RVAString = RVAString.substr(RVAString.find_first_not_of(' '));
		// Substr to the first whitespace to get only the RVA+Base
		RVAString = RVAString.substr(0, RVAString.find_first_of(' '));
		// Convert to address
		Function.RVAAddr = std::strtoull(RVAString.c_str(), nullptr, 16);

		// If the function somehow points to reserved space in the file header,
		// skip it. Seems like an unecessary check but you'd be shocked
		if (Function.RVAAddr < ImageBase + 0x1000)
			continue;

		// Converts RVAs to a raw offset off the file buffer
		Function.RawAddr = ImagePtr + RVAToRaw(Function.RVAAddr - ImageBase, Section, SectionNum);

		// Fun part below, counting the number of pad bytes that we have to play with.
		auto OpCode = (uint8_t*)Function.RawAddr - 1;

		// Used as a trap to debugger but also as function padding, this is where
		// we replace with plenty of fun to mess with static dissassemblers
		// https://x86.puri.sm/html/file_module_x86_id_142.html
		// Functions are usually 16 byte aligned which helps with cache efficiency
		// of x86 processors, however they can be more if the compiler was provided
		// with an override of default settings. An example is provided below

		/*
			std::string *__cdecl std::forward<std::string&>(std::string *_Arg)
			_Arg              = dword ptr   8
			                  push    ebp                       55
							  mov     ebp, esp                  8B EC
							  mov     eax, [ebp+_Arg]           8B 45 08
							  pop     ebp                       5D
							  retn                              C3
							                                    CC
																CC
																CC
																CC
																CC
																CC
																CC
																CC
		*/

		while (*OpCode != 0xCC)
		{
			++Function.PadLen;

			OpCode = (uint8_t*)(Function.RawAddr - 1 - Function.PadLen);
		}

		// If there's no pad and the function is natrually 16 byte aligned,
		// there is nothing for us to do here.
		if (Function.PadLen < 1)
			continue;

		Function.PadStart = Function.RawAddr - Function.PadLen;

		FunctionsParsed.push_back(Function);
		printf("Symbol: %s RVA: 0x%llx Raw: 0x%llx Pad: 0x%llx PadLen: %llu\n", SymbolStr.c_str(), Function.RVAAddr, Function.RawAddr, Function.PadStart, Function.PadLen);
	}

	return FunctionsParsed;
}

int main(int argc, char* argv[])
{
	CmdLineArgs Args(argc, argv);

	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	freopen("CON", "w", stdout);

	auto FileName = std::string(Args.GetArgValue("file"));
	if (FileName.empty())
		throw new std::runtime_error("Failed to provide a file name!");

	// Loaded buffer of the PEFile
	auto PEFile = LoadFile(FileName);
	auto Functions = ParseMapFile(FileName, PEFile);

	std::random_device RD;
	std::mt19937 e2(RD());

	for (auto FunctionIter = Functions.begin(); FunctionIter < Functions.end(); ++FunctionIter)
	{
		const auto& Function = *FunctionIter;

		const auto& NextFunction = *std::next(FunctionIter);
		auto BytesLeft = Function.PadLen;
		auto CurrentOp = (uint8_t*)(Function.PadStart + Function.PadLen - BytesLeft);

		if (*(CurrentOp - 1) == 0xC3) // Check if last ins is ret
		{
			*(CurrentOp + Function.PadLen - 1) = 0xC3; // Set the last instruction in pad to ret
			
			// Create a nop sled that ends in the same return
			// https://en.wikipedia.org/wiki/NOP_slide
			for (size_t i = 0; i < Function.PadLen; i++)
				*(CurrentOp + i - 1) = 0x90; // Nop
			

			printf("%s: Wrote nop sled to 0x%llx\n", Function.Name.c_str(), (uintptr_t)CurrentOp);
		}
	}

	WriteFile(PEFile, FileName);

	return 0;
}
