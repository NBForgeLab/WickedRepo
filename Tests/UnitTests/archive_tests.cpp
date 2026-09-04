// Unit tests for the wiArchive module (wiArchive.h / wiArchive.cpp):
// typed round-trips, in-place read/write mode switching, the version header
// contract and file save/load.
#include "doctest.h"

#include "wiArchive.h"
#include "wiECS.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
	// Unique scratch file per test run, removed when the test ends.
	class TempFile
	{
	public:
		TempFile()
		{
			namespace fs = std::filesystem;
			std::string unique = std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count());
			path = (fs::temp_directory_path() / "wiarchive_unittest").string()
				+ unique + std::to_string(++counter) + ".bin";
		}
		~TempFile() { std::remove(path.c_str()); }

		std::string path;

	private:
		static unsigned counter;
	};
	unsigned TempFile::counter = 0;
}

TEST_CASE("wiArchive round-trips all explicitly supported scalar types")
{
	wiArchive archive;
	CHECK(archive.IsReadMode() == false);

	archive << true;
	archive << false;
	archive << 'A';
	archive << (unsigned char)200;
	archive << -42;
	archive << 42u;
	archive << -1234567890123LL;
	archive << 1234567890123ULL;
	archive << 3.14159f;
	archive << 2.718281828;

	archive.SetReadModeAndResetPos(true);
	CHECK(archive.IsReadMode() == true);

	bool b = false;
	char c = 0;
	unsigned char uc = 0;
	int i = 0;
	unsigned int u = 0;
	long long ll = 0;
	unsigned long long ull = 0;
	float f = 0;
	double d = 0;
	archive >> b; CHECK(b == true);
	archive >> b; CHECK(b == false);
	archive >> c; CHECK(c == 'A');
	archive >> uc; CHECK(uc == 200);
	archive >> i; CHECK(i == -42);
	archive >> u; CHECK(u == 42u);
	archive >> ll; CHECK(ll == -1234567890123LL);
	archive >> ull; CHECK(ull == 1234567890123ULL);
	archive >> f; CHECK(f == doctest::Approx(3.14159f));
	archive >> d; CHECK(d == doctest::Approx(2.718281828));
}

TEST_CASE("wiArchive round-trips DirectX math types, strings and vectors")
{
	wiArchive archive;

	const XMFLOAT3 point(1.5f, -2.5f, 3.5f);
	const XMFLOAT4 quat(0, 0.7071f, 0, 0.7071f);
	XMFLOAT4X4 matrix;
	XMStoreFloat4x4(&matrix, XMMatrixIdentity());
	archive << point;
	archive << quat;
	archive << matrix;
	archive << std::string("hello world");
	archive << std::string(""); // empty string
	archive << std::string("مرحبا"); // UTF-8 multi-byte payload

	const std::vector<int> numbers{ 1, -2, 3, -4, 5 };
	const std::vector<std::string> words{ "alpha", "beta" };
	archive << numbers;
	archive << words;

	archive.SetReadModeAndResetPos(true);

	XMFLOAT3 pointOut;
	XMFLOAT4 quatOut;
	XMFLOAT4X4 matrixOut;
	std::string stringOut, emptyOut, utf8Out;
	std::vector<int> numbersOut;
	std::vector<std::string> wordsOut;

	archive >> pointOut;
	CHECK(pointOut.x == point.x);
	CHECK(pointOut.y == point.y);
	CHECK(pointOut.z == point.z);
	archive >> quatOut;
	CHECK(quatOut.x == quat.x);
	CHECK(quatOut.y == quat.y);
	CHECK(quatOut.z == quat.z);
	CHECK(quatOut.w == quat.w);
	archive >> matrixOut;
	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 4; ++c)
		{
			CHECK(matrixOut.m[r][c] == matrix.m[r][c]);
		}
	}
	archive >> stringOut; CHECK(stringOut == "hello world");
	archive >> emptyOut; CHECK(emptyOut.empty());
	archive >> utf8Out; CHECK(utf8Out == "مرحبا");

	archive >> numbersOut;
	REQUIRE(numbersOut.size() == numbers.size());
	for (size_t i = 0; i < numbers.size(); ++i) CHECK(numbersOut[i] == numbers[i]);

	archive >> wordsOut;
	REQUIRE(wordsOut.size() == words.size());
	CHECK(wordsOut[0] == "alpha");
	CHECK(wordsOut[1] == "beta");
}

TEST_CASE("wiArchive stamps the current version and validates it on load")
{
	wiArchive archive;
	// The archive begins with the writer's version; a version bump in the
	// engine must be a conscious change, so pin it here:
	CHECK(archive.GetVersion() == 5077);
	// The version is the first payload in the stream:
	CHECK(archive.GetSize() >= sizeof(uint64_t));

	// Saving and reopening yields the same version:
	TempFile file;
	CHECK(archive.SaveFile(file.path));

	wiArchive loaded(file.path, true);
	REQUIRE(loaded.IsOpen());
	CHECK(loaded.GetVersion() == 5077);
	CHECK(loaded.GetSourceFileName() == file.path);
}

TEST_CASE("wiArchive rejects archives below the compatibility barrier")
{
	namespace fs = std::filesystem;

	TempFile tooOld;
	{
		std::FILE* f = std::fopen(tooOld.path.c_str(), "wb");
		REQUIRE(f != nullptr);
		const uint64_t staleVersion = 21; // barrier is 22
		std::fwrite(&staleVersion, sizeof(staleVersion), 1, f);
		std::fclose(f);
	}
	wiArchive stale(tooOld.path, true);
	CHECK_FALSE(stale.IsOpen());

	TempFile tooNew;
	{
		std::FILE* f = std::fopen(tooNew.path.c_str(), "wb");
		REQUIRE(f != nullptr);
		const uint64_t futureVersion = 5078; // higher than the writer's version
		std::fwrite(&futureVersion, sizeof(futureVersion), 1, f);
		std::fclose(f);
	}
	wiArchive future(tooNew.path, true);
	CHECK_FALSE(future.IsOpen());
}

TEST_CASE("wiArchive persists and restores a mixed payload through a file")
{
	TempFile file;
	{
		// A file-linked write archive saves its payload automatically on
		// Close(); an unlinked default-constructed archive would not.
		wiArchive archive(file.path, false);
		archive << 12345;
		archive << std::string("persisted");
		archive << XMFLOAT3(9, 8, 7);
		archive.Close();
		// Close() saved and invalidated the buffer:
		CHECK_FALSE(archive.IsOpen());
	}

	wiArchive loaded(file.path, true);
	REQUIRE(loaded.IsOpen());
	int number = 0;
	std::string text;
	XMFLOAT3 point;
	loaded >> number;
	loaded >> text;
	loaded >> point;
	CHECK(number == 12345);
	CHECK(text == "persisted");
	CHECK(point.x == 9);
	CHECK(point.y == 8);
	CHECK(point.z == 7);
}

TEST_CASE("wiArchive tracks its payload size and cycles read/write modes")
{
	wiArchive archive;
	const size_t headerSize = archive.GetSize();
	CHECK(headerSize == sizeof(uint64_t)); // only the version so far

	archive << (unsigned long long)123;
	archive << 456;
	const size_t afterWrites = archive.GetSize();
	CHECK(afterWrites == headerSize + sizeof(uint64_t) + sizeof(int64_t));

	// Toggling back to write mode rewrites the version header and resets:
	archive.SetReadModeAndResetPos(false);
	CHECK(archive.IsReadMode() == false);
	CHECK(archive.GetSize() == headerSize);

	// And a full write -> read -> write cycle stays consistent:
	archive << std::string("cycle");
	archive.SetReadModeAndResetPos(true);
	std::string out;
	archive >> out;
	CHECK(out == "cycle");
}

TEST_CASE("wiArchive round-trips 64-bit vectors and preserves element order")
{
	wiArchive archive;
	const std::vector<unsigned long long> big{ 0ull, 18446744073709551615ull, 42ull };
	archive << big;

	archive.SetReadModeAndResetPos(true);
	std::vector<unsigned long long> out;
	archive >> out;
	REQUIRE(out.size() == 3);
	CHECK(out[0] == 0ull);
	CHECK(out[1] == 18446744073709551615ull);
	CHECK(out[2] == 42ull);
}

TEST_CASE("wiECS entities serialize through a wiArchive round-trip")
{
	wiECS::Entity a = 7, b = 9;

	wiArchive archive;
	wiECS::EntitySerializer writer;
	wiECS::SerializeEntity(archive, a, writer);
	wiECS::SerializeEntity(archive, b, writer);
	archive.SetReadModeAndResetPos(true);

	wiECS::Entity aOut = wiECS::INVALID_ENTITY, bOut = wiECS::INVALID_ENTITY;
	wiECS::EntitySerializer reader;
	wiECS::SerializeEntity(archive, aOut, reader);
	wiECS::SerializeEntity(archive, bOut, reader);

	// The reader remaps serialized handles onto fresh entities; both must be
	// valid, distinct, and stable across repeated reads of the same source id:
	CHECK(aOut != wiECS::INVALID_ENTITY);
	CHECK(bOut != wiECS::INVALID_ENTITY);
	CHECK(aOut != bOut);

	// Reading the same source id again within the same load remaps to the
	// same new entity (the remap table lives per EntitySerializer):
	wiArchive archive2;
	wiECS::Entity src = 42, out1 = wiECS::INVALID_ENTITY, out2 = wiECS::INVALID_ENTITY;
	{
		wiECS::EntitySerializer writeSeri;
		wiECS::SerializeEntity(archive2, src, writeSeri);
	}
	{
		wiECS::EntitySerializer reader2;
		archive2.SetReadModeAndResetPos(true);
		wiECS::SerializeEntity(archive2, out1, reader2);
		archive2.SetReadModeAndResetPos(true);
		wiECS::SerializeEntity(archive2, out2, reader2);
	}
	CHECK(out1 == out2);

	// A fresh EntitySerializer remaps independently onto a different entity:
	wiECS::Entity out3 = wiECS::INVALID_ENTITY;
	{
		wiECS::EntitySerializer reader3;
		archive2.SetReadModeAndResetPos(true);
		wiECS::SerializeEntity(archive2, out3, reader3);
	}
	CHECK(out3 != out1);
}
