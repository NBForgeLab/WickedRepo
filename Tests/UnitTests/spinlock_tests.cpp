// Unit tests for the wiSpinLock utility (wiSpinLock.h): try/lock/unlock
// semantics and mutual exclusion under thread contention.
#include "doctest.h"

#include "wiSpinLock.h"

#include <thread>
#include <vector>

TEST_CASE("wiSpinLock try_lock acquires and releases")
{
	wiSpinLock spinlock;

	CHECK(spinlock.try_lock());
	// A held spinlock cannot be acquired again:
	CHECK_FALSE(spinlock.try_lock());

	spinlock.unlock();
	CHECK(spinlock.try_lock());
	spinlock.unlock();
}

TEST_CASE("wiSpinLock lock/unlock round trip")
{
	wiSpinLock spinlock;
	spinlock.lock();
	spinlock.unlock();
	// Lock is free again:
	CHECK(spinlock.try_lock());
	spinlock.unlock();
}

TEST_CASE("wiSpinLock provides mutual exclusion under contention")
{
	wiSpinLock spinlock;
	long shared = 0; // deliberately non-atomic: only the lock protects it
	constexpr int threadsCount = 4;
	constexpr int iterations = 50000;

	std::vector<std::thread> threads;
	for (int t = 0; t < threadsCount; ++t)
	{
		threads.emplace_back([&] {
			for (int i = 0; i < iterations; ++i)
			{
				spinlock.lock();
				shared++;
				spinlock.unlock();
			}
		});
	}
	for (std::thread& t : threads)
	{
		t.join();
	}

	CHECK(shared == (long)threadsCount * iterations);
}
