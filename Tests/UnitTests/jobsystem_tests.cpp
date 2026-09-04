// Unit tests for the wiJobSystem module (wiJobSystem.h / wiJobSystem.cpp):
// group math, dispatch semantics, job args invariants and wait behavior.
// wiJobSystem::Initialize() and wiNetwork::Initialize() run once in main().
#include "doctest.h"

#include "wiJobSystem.h"

#include <array>
#include <atomic>
#include <cstring>

TEST_CASE("wiJobSystem exposes at least one worker thread after Initialize")
{
	CHECK(wiJobSystem::GetThreadCount() >= 1);
}

TEST_CASE("DispatchGroupCount computes the ceiling of the group division")
{
	CHECK(wiJobSystem::DispatchGroupCount(0, 64) == 0);
	CHECK(wiJobSystem::DispatchGroupCount(1, 64) == 1);
	CHECK(wiJobSystem::DispatchGroupCount(64, 64) == 1);
	CHECK(wiJobSystem::DispatchGroupCount(65, 64) == 2);
	CHECK(wiJobSystem::DispatchGroupCount(1000, 64) == 16);
	CHECK(wiJobSystem::DispatchGroupCount(128, 7) == 19);
}

TEST_CASE("Dispatch with zero jobs or zero group size is a no-op")
{
	wiJobSystem::context ctx;
	std::atomic<int> executed{ 0 };
	wiJobSystem::Dispatch(ctx, 0, 64, [&](wiJobArgs) { executed.fetch_add(1); });
	wiJobSystem::Dispatch(ctx, 10, 0, [&](wiJobArgs) { executed.fetch_add(1); });

	CHECK_FALSE(wiJobSystem::IsBusy(ctx));
	wiJobSystem::Wait(ctx);
	CHECK(executed.load() == 0);
	CHECK(ctx.counter.load() == 0);
}

TEST_CASE("Execute schedules every task exactly once and Wait drains the context")
{
	wiJobSystem::context ctx;
	std::atomic<int> executed{ 0 };

	for (int i = 0; i < 512; ++i)
	{
		wiJobSystem::Execute(ctx, [&](wiJobArgs) { executed.fetch_add(1); });
	}
	// IsBusy is intentionally not asserted here: trivial jobs can drain
	// before the check runs, making that observation racy by design.

	wiJobSystem::Wait(ctx);

	CHECK_FALSE(wiJobSystem::IsBusy(ctx));
	CHECK(executed.load() == 512);
	CHECK(ctx.counter.load() == 0);
}

TEST_CASE("Dispatch partitions jobs into groups with correct wiJobArgs invariants")
{
	constexpr uint32_t jobCount = 1000;
	constexpr uint32_t groupSize = 64;
	const uint32_t groupCount = wiJobSystem::DispatchGroupCount(jobCount, groupSize);

	// Per-group observed job bounds, to verify contiguous partitioning:
	std::array<std::atomic<uint32_t>, 16> minSeen;
	std::array<std::atomic<uint32_t>, 16> maxSeen;
	for (uint32_t g = 0; g < groupCount; ++g)
	{
		minSeen[g] = 0xFFFFFFFFu;
		maxSeen[g] = 0;
	}
	std::atomic<uint32_t> executed{ 0 };
	std::atomic<uint32_t> firstJobCount{ 0 };
	std::atomic<uint32_t> lastJobCount{ 0 };

	wiJobSystem::context ctx;
	wiJobSystem::Dispatch(ctx, jobCount, groupSize, [&](wiJobArgs args) {
		executed.fetch_add(1);
		CHECK(args.jobIndex < jobCount);
		CHECK(args.groupID == args.jobIndex / groupSize);
		CHECK(args.groupIndex == args.jobIndex % groupSize);
		if (args.isFirstJobInGroup) firstJobCount.fetch_add(1);
		if (args.isLastJobInGroup) lastJobCount.fetch_add(1);

		const uint32_t g = args.groupID;
		uint32_t expected = minSeen[g].load();
		while (args.jobIndex < expected && !minSeen[g].compare_exchange_weak(expected, args.jobIndex)) {}
		expected = maxSeen[g].load();
		while (args.jobIndex > expected && !maxSeen[g].compare_exchange_weak(expected, args.jobIndex)) {}
	});

	wiJobSystem::Wait(ctx);

	CHECK(executed.load() == jobCount);
	// Exactly one first and one last job per group:
	CHECK(firstJobCount.load() == groupCount);
	CHECK(lastJobCount.load() == groupCount);

	// Each group covered its contiguous slice [g*size, min(g*size+size-1, end)):
	for (uint32_t g = 0; g < groupCount; ++g)
	{
		const uint32_t expectedFirst = g * groupSize;
		const uint32_t expectedLast = std::min(expectedFirst + groupSize - 1, jobCount - 1);
		CHECK(minSeen[g].load() == expectedFirst);
		CHECK(maxSeen[g].load() == expectedLast);
	}

	CHECK_FALSE(wiJobSystem::IsBusy(ctx));
	CHECK(ctx.counter.load() == 0);
}

TEST_CASE("Execute jobs beyond the queue capacity still all run")
{
	// The internal ring buffer holds 256 jobs; Execute spins (and works)
	// when full, so every queued task must survive a heavy burst.
	wiJobSystem::context ctx;
	std::atomic<int> executed{ 0 };

	for (int i = 0; i < 2000; ++i)
	{
		wiJobSystem::Execute(ctx, [&](wiJobArgs) { executed.fetch_add(1); });
	}
	wiJobSystem::Wait(ctx);

	CHECK(executed.load() == 2000);
	CHECK_FALSE(wiJobSystem::IsBusy(ctx));
}

TEST_CASE("Dispatch jobs may schedule their own nested work")
{
	wiJobSystem::context outer;
	std::atomic<int> executed{ 0 };

	wiJobSystem::Dispatch(outer, 32, 8, [&](wiJobArgs) {
		wiJobSystem::context inner;
		wiJobSystem::Execute(inner, [&](wiJobArgs) { executed.fetch_add(1); });
		wiJobSystem::Wait(inner);
	});
	wiJobSystem::Wait(outer);

	CHECK(executed.load() == 32);
}

TEST_CASE("Independent contexts can be waited on independently")
{
	wiJobSystem::context ctxA;
	wiJobSystem::context ctxB;
	std::atomic<int> executedA{ 0 };
	std::atomic<int> executedB{ 0 };

	for (int i = 0; i < 100; ++i)
	{
		wiJobSystem::Execute(ctxA, [&](wiJobArgs) { executedA.fetch_add(1); });
		wiJobSystem::Execute(ctxB, [&](wiJobArgs) { executedB.fetch_add(1); });
	}
	wiJobSystem::Wait(ctxA);
	CHECK(executedA.load() == 100);
	wiJobSystem::Wait(ctxB);
	CHECK(executedB.load() == 100);
}

TEST_CASE("Dispatch provides shared group memory only when requested")
{
	wiJobSystem::context ctx;
	// Violations counted; both must stay at zero:
	std::atomic<int> nullSeenWithSize{ 0 };
	std::atomic<int> nonNullSeenWithoutSize{ 0 };

	// No shared memory requested:
	wiJobSystem::Dispatch(ctx, 8, 4, [&](wiJobArgs args) {
		if (args.sharedmemory != nullptr) nonNullSeenWithoutSize.fetch_add(1);
	});
	// 64 bytes of shared stack memory requested:
	wiJobSystem::Dispatch(ctx, 8, 4, [&](wiJobArgs args) {
		if (args.sharedmemory == nullptr)
		{
			nullSeenWithSize.fetch_add(1);
			return;
		}
		unsigned char* bytes = static_cast<unsigned char*>(args.sharedmemory);
		// Group jobs execute serially, so writing here is race-free:
		std::memset(bytes, 0xAB, 64);
		for (int i = 0; i < 64; ++i)
		{
			CHECK(bytes[i] == 0xAB);
		}
	}, 64);

	wiJobSystem::Wait(ctx);
	CHECK(nullSeenWithSize.load() == 0);
	CHECK(nonNullSeenWithoutSize.load() == 0);
}
