#include "ArgParse.h"

CmdLineArgs::CmdLineArgs(int argc, char* argv[])
{
	std::vector<const char*> LastArg;

	for (size_t i = 0; i < argc; i++)
	{
		char* Arg = argv[i];

		size_t Len = strlen(Arg);
		if (Len == 0)
			continue;

		if (Arg[0] == '-') // starts with -
		{
			if (Len <= 1)
				continue;

			if (!LastArg.empty())
				Args.push_back(LastArg);

			LastArg = { Arg + 1 }; // Ignore the dash
		}
		else // option arg
		{
			if (LastArg.size() < 2) // Only allow one arg per pair
				LastArg.push_back(Arg);
		}
	}

	if (!LastArg.empty())
		Args.push_back(LastArg);
}

const char* CmdLineArgs::GetArgValue(const char* Arg) const
{
	for (const auto& CArg : Args)
		if (!_strcmpi(Arg, CArg[0]))
			return (CArg.size() > 1) ? CArg[1] : nullptr;

	return nullptr;
}

bool CmdLineArgs::DoesArgExist(const char* Arg) const
{
	for (const auto& CArg : Args)
		if (!_strcmpi(Arg, CArg[0]))
			return true;

	return false;
}
