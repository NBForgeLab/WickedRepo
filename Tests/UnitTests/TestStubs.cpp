#include "TestStubs.h"

#include <windows.h>

#include <cstdio>
#include <fstream>

namespace wiBackLog
{
	void post(const char* input)
	{
		// The engine posts human-readable diagnostics here; tests surface
		// them on stdout so they are visible in test output.
		std::printf("[wiBackLog] %s\n", input ? input : "");
	}
}

namespace wiHelper
{
	void messageBox(const std::string& msg, const std::string& caption)
	{
		std::printf("[wiHelper::messageBox] %s: %s\n", caption.c_str(), msg.c_str());
	}

	std::string GetDirectoryFromPath(const std::string& path)
	{
		const size_t lastSeparator = path.find_last_of("/\\");
		if (lastSeparator == std::string::npos)
		{
			return "";
		}
		return path.substr(0, lastSeparator + 1);
	}

	bool FileRead(const std::string& fileName, std::vector<uint8_t>& data)
	{
		std::ifstream file(fileName, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			return false;
		}
		const std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);
		data.resize(static_cast<size_t>(size));
		if (size > 0 && !file.read(reinterpret_cast<char*>(data.data()), size))
		{
			data.clear();
			return false;
		}
		return true;
	}

	bool FileWrite(const std::string& fileName, const uint8_t* data, size_t size)
	{
		std::ofstream file(fileName, std::ios::binary | std::ios::trunc);
		if (!file.is_open())
		{
			return false;
		}
		if (size > 0 && !file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size)))
		{
			return false;
		}
		return true;
	}

	void StringConvert(const std::wstring& from, std::string& to)
	{
		if (from.empty())
		{
			to.clear();
			return;
		}
		const int size = WideCharToMultiByte(CP_UTF8, 0, from.c_str(), (int)from.size(),
			nullptr, 0, nullptr, nullptr);
		to.resize((size_t)size);
		WideCharToMultiByte(CP_UTF8, 0, from.c_str(), (int)from.size(),
			&to[0], size, nullptr, nullptr);
	}
}
