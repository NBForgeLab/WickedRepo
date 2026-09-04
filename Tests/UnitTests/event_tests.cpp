// Unit tests for the wiEvent module (wiEvent.h / wiEvent.cpp): subscribe,
// fire, once-only subscribers and handle-lifetime based unsubscription.
#include "doctest.h"

#include "wiEvent.h"

#include <vector>

TEST_CASE("FireEvent delivers userdata to every subscriber in subscription order")
{
	std::vector<uint64_t> received;

	wiEvent::Handle first = wiEvent::Subscribe(9001, [&](uint64_t data) { received.push_back(data + 0); });
	wiEvent::Handle second = wiEvent::Subscribe(9001, [&](uint64_t data) { received.push_back(data + 1000); });
	REQUIRE(first.IsValid());
	REQUIRE(second.IsValid());

	wiEvent::FireEvent(9001, 5ull);

	REQUIRE(received.size() == 2);
	CHECK(received[0] == 5);
	CHECK(received[1] == 1005);

	// Other event ids are untouched:
	wiEvent::FireEvent(9002, 7ull);
	CHECK(received.size() == 2);
}

TEST_CASE("Destroying the subscription handle unsubscribes the callback")
{
	int calls = 0;
	{
		wiEvent::Handle handle = wiEvent::Subscribe(9002, [&](uint64_t) { ++calls; });
		wiEvent::FireEvent(9002, 0ull);
		CHECK(calls == 1);
		// handle goes out of scope here
	}
	wiEvent::FireEvent(9002, 0ull);
	CHECK(calls == 1);
}

TEST_CASE("Subscribe_Once callbacks fire exactly one time")
{
	int calls = 0;
	wiEvent::Subscribe_Once(9003, [&](uint64_t) { ++calls; });

	wiEvent::FireEvent(9003, 0ull);
	wiEvent::FireEvent(9003, 0ull);
	wiEvent::FireEvent(9003, 0ull);
	CHECK(calls == 1);
}

TEST_CASE("Firing an event without subscribers is safe")
{
	wiEvent::FireEvent(-777, 0ull);
	CHECK(true);
}

TEST_CASE("Persistent and once-only subscribers interleave on the same event id")
{
	std::vector<int> order;
	wiEvent::Handle persistent = wiEvent::Subscribe(9004, [&](uint64_t) { order.push_back(1); });
	wiEvent::Subscribe_Once(9004, [&](uint64_t) { order.push_back(2); });
	(void)persistent;

	wiEvent::FireEvent(9004, 0ull);
	wiEvent::FireEvent(9004, 0ull);

	// FireEvent runs once-only subscribers before the persistent ones:
	REQUIRE(order.size() == 3);
	CHECK(order[0] == 2);
	CHECK(order[1] == 1);
	CHECK(order[2] == 1);
}
