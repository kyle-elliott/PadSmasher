#pragma once

#include <vector>

class CmdLineArgs
{
	std::vector<std::vector<const char*>> Args;

public:
	CmdLineArgs(int argc, char* argv[]);

	const char* GetArgValue(const char* Arg) const;
	bool DoesArgExist(const char* Arg) const;
};
