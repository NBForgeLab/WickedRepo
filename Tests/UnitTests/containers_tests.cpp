// Unit tests for wiContainers (wiContainers.h): the thread-safe ring buffer
// FIFO contract, capacity behavior, wrap-around and concurrent use.
#include "doctest.h"

#include "wiContainers.h"

#include <atomic>
#include <thread>
#include <vector>

TEST_CASE("ThreadSafeRingBuffer preserves FIFO order")
{
	// The ring buffer sacrifices one slot to distinguish full from empty,
	// so a capacity of 8 holds up to 7 items:
	wiContainers::ThreadSafeRingBuffer<int, 8> buffer;

	for (int i = 0; i < 7; ++i)
	{
		CHECK(buffer.push_back(i));
	}
	int value = -1;
	for (int i = 0; i < 7; ++i)
	{
		REQUIRE(buffer.pop_front(value));
		CHECK(value == i);
	}
	// Drained completely:
	CHECK_FALSE(buffer.pop_front(value));
}

TEST_CASE("ThreadSafeRingBuffer refuses to grow beyond its capacity")
{
	wiContainers::ThreadSafeRingBuffer<int, 4> buffer;

	// Capacity 4 holds up to 3 items:
	for (int i = 0; i < 3; ++i)
	{
		CHECK(buffer.push_back(i));
	}
	// Full buffer rejects further pushes without losing data:
	CHECK_FALSE(buffer.push_back(99));
	int value = -1;
	CHECK(buffer.pop_front(value));
	CHECK(value == 0);
}

TEST_CASE("ThreadSafeRingBuffer wraps around after exhausting capacity")
{
	wiContainers::ThreadSafeRingBuffer<int, 4> buffer;

	for (int round = 0; round < 3; ++round)
	{
		for (int i = 0; i < 3; ++i)
		{
			CHECK(buffer.push_back(round * 3 + i));
		}
		int value = -1;
		for (int i = 0; i < 3; ++i)
		{
			REQUIRE(buffer.pop_front(value));
			CHECK(value == round * 3 + i);
		}
	}
}

TEST_CASE("ThreadSafeRingBuffer stays consistent under concurrent producers and consumers")
{
	wiContainers::ThreadSafeRingBuffer<int, 256> buffer;
	constexpr int producersCount = 2;
	constexpr int perProducer = 20000;
	std::atomic<int> pushed{ 0 };
	std::atomic<int> popped{ 0 };
	std::atomic<int> finishedProducers{ 0 };

	auto producer = [&] {
		for (int i = 0; i < perProducer; ++i)
		{
			while (!buffer.push_back(i))
			{
				// spin until there is room
			}
			pushed.fetch_add(1);
		}
		finishedProducers.fetch_add(1);
	};
	auto consumer = [&] {
		int value;
		for (;;)
		{
			if (buffer.pop_front(value))
			{
				popped.fetch_add(1);
			}
			else if (finishedProducers.load() == producersCount)
			{
				// All producers are done and the buffer is drained.
				break;
			}
			else
			{
				std::this_thread::yield();
			}
		}
	};

	std::vector<std::thread> threads;
	threads.emplace_back(producer);
	threads.emplace_back(producer);
	threads.emplace_back(consumer);
	for (auto& t : threads)
	{
		t.join();
	}

	// No item is ever lost or duplicated across the lock-free seam:
	CHECK(pushed.load() == producersCount * perProducer);
	CHECK(popped.load() == producersCount * perProducer);
}
