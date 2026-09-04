// Unit tests for the wiRandom module (wiRandom.h / wiRandom.cpp).
// The generator is seeded from std::random_device and offers no external
// seeding control, so tests verify range contracts and statistical sanity
// rather than deterministic sequences.
#include "doctest.h"

#include "wiRandom.h"

#include <cstdint>

TEST_CASE("getRandom(int) honors inclusive bounds")
{
	for (int i = 0; i < 10000; ++i)
	{
		const int value = wiRandom::getRandom(-5, 5);
		CHECK(value >= -5);
		CHECK(value <= 5);
	}
}

TEST_CASE("getRandom(int maxValue) covers the [0, maxValue] range")
{
	bool lowHalfSeen = false;
	bool highHalfSeen = false;
	for (int i = 0; i < 10000; ++i)
	{
		const int value = wiRandom::getRandom(100);
		CHECK(value >= 0);
		CHECK(value <= 100);
		lowHalfSeen = lowHalfSeen || (value <= 50);
		highHalfSeen = highHalfSeen || (value >= 51);
	}
	// With 10000 draws over 101 values, missing an entire half is impossible
	// in practice; a broken generator (always zero, always max) fails here.
	CHECK(lowHalfSeen);
	CHECK(highHalfSeen);
}

TEST_CASE("getRandom(unsigned int) honors inclusive bounds")
{
	for (int i = 0; i < 10000; ++i)
	{
		const uint32_t value = wiRandom::getRandom(10u, 20u);
		CHECK(value >= 10u);
		CHECK(value <= 20u);
	}
	for (int i = 0; i < 1000; ++i)
	{
		const uint32_t value = wiRandom::getRandom(0u);
		CHECK(value == 0u);
	}
}

TEST_CASE("getRandom(uint64_t) honors inclusive bounds")
{
	for (int i = 0; i < 10000; ++i)
	{
		const uint64_t value = wiRandom::getRandom(
			(uint64_t)1ull << 62, ((uint64_t)1ull << 62) + 1000ull);
		CHECK(value >= (1ull << 62));
		CHECK(value <= (1ull << 62) + 1000ull);
	}
}
