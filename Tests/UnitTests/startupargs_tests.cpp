// Unit tests for the wiStartupArguments module (wiStartupArguments.h /
// wiStartupArguments.cpp): argument registration and lookup.
#include "doctest.h"

#include "wiStartupArguments.h"

#include <string>
#include <vector>

namespace
{
	// Resets the process-global argument set; the wchar_t overload replaces
	// the whole set while the argv overload only inserts.
	void ResetArguments()
	{
		wiStartupArguments::Parse(L"");
	}
}

TEST_CASE("wiStartupArguments registers space-separated arguments")
{
	ResetArguments();
	wiStartupArguments::Parse(L"project=demo editorfromstandalone=1 -customflag");

	CHECK(wiStartupArguments::HasArgument("project=demo"));
	CHECK(wiStartupArguments::HasArgument("editorfromstandalone=1"));
	CHECK(wiStartupArguments::HasArgument("-customflag"));
	CHECK_FALSE(wiStartupArguments::HasArgument("project=nope"));
}

TEST_CASE("wiStartupArguments::Parse(argc, argv) skips the program name")
{
	ResetArguments();

	std::vector<std::string> storage = { "GameGuruMAX.exe", "project=testmap", "gameisexe=1" };
	std::vector<char*> argv;
	for (std::string& s : storage) argv.push_back(&s[0]);

	wiStartupArguments::Parse((int)argv.size(), argv.data());

	CHECK(wiStartupArguments::HasArgument("project=testmap"));
	CHECK(wiStartupArguments::HasArgument("gameisexe=1"));
	// The program name itself is not registered as an argument:
	CHECK_FALSE(wiStartupArguments::HasArgument("GameGuruMAX.exe"));
}

TEST_CASE("wiStartupArguments handles an empty argument list")
{
	ResetArguments();
	CHECK_FALSE(wiStartupArguments::HasArgument("anything"));
	wiStartupArguments::Parse(L"");
	CHECK_FALSE(wiStartupArguments::HasArgument("anything"));
}
