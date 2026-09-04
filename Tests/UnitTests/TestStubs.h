#pragma once
// The CPU modules under test call into two engine service namespaces
// (wiBackLog for logging, wiHelper for filesystem and dialogs) whose real
// implementations pull in the renderer, WinRT and third-party UI stacks.
// Only the handful of functions actually referenced by the modules compiled
// into this target are implemented here, faithfully (real file I/O, real
// string handling), so the unit test binary stays free of graphics
// dependencies and runs headless.

#include <cstdint>
#include <string>
#include <vector>

namespace wiBackLog
{
	void post(const char* input);
}

namespace wiHelper
{
	void messageBox(const std::string& msg, const std::string& caption = "Warning!");
	std::string GetDirectoryFromPath(const std::string& path);
	bool FileRead(const std::string& fileName, std::vector<uint8_t>& data);
	bool FileWrite(const std::string& fileName, const uint8_t* data, size_t size);
	void StringConvert(const std::wstring& from, std::string& to);
}
