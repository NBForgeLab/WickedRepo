// Unit tests for the wiTimer module (wiTimer.h / wiTimer.cpp).
#include "doctest.h"

#include "wiTimer.h"

#include <thread>

namespace
{
	void busySleepMs(unsigned ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
}

TEST_CASE("wiTimer measures elapsed seconds and milliseconds consistently")
{
	wiTimer timer;
	busySleepMs(60);

	const double seconds = timer.elapsed_seconds();
	const double milliseconds = timer.elapsed_milliseconds();

	// Generous bounds keep the test robust on loaded CI machines while still
	// catching a frozen or inverted clock:
	CHECK(seconds >= 0.05);
	CHECK(seconds <= 5.0);
	CHECK(milliseconds >= 50.0);
	CHECK(milliseconds <= 5000.0);
	// ms and s report the same underlying clock; the two calls read the
	// clock at different instants, so allow a 1 ms absolute window:
	CHECK(milliseconds >= seconds * 1000.0);
	CHECK(milliseconds <= seconds * 1000.0 + 1.0);
}

TEST_CASE("wiTimer::record restarts the measured interval")
{
	wiTimer timer;
	busySleepMs(60);
	const double first = timer.elapsed_seconds();
	CHECK(first >= 0.05);

	timer.record();
	const double afterRecord = timer.elapsed_seconds();
	CHECK(afterRecord < first);
	CHECK(afterRecord <= 0.05);
}
