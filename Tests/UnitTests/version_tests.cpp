// Unit tests for the wiVersion module (wiVersion.h / wiVersion.cpp).
#include "doctest.h"

#include "wiVersion.h"

#include <cstring>
#include <string>

TEST_CASE("wiVersion version string is composed from the individual components")
{
	const std::string expected = std::to_string(wiVersion::GetMajor())
		+ "." + std::to_string(wiVersion::GetMinor())
		+ "." + std::to_string(wiVersion::GetRevision());

	CHECK(std::string(wiVersion::GetVersionString()) == expected);
}

TEST_CASE("wiVersion::GetVersion encodes major, minor and revision")
{
	// The packed version must stay consistent with the individual getters:
	const long packed = wiVersion::GetVersion();
	CHECK(packed == wiVersion::GetMajor() * 100 + wiVersion::GetMinor() * 10 + wiVersion::GetRevision());
	CHECK(packed > 0);
}
