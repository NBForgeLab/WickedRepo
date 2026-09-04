// Memory leak detection for the engine's CPU modules, built on the MSVC
// debug CRT heap (_CrtMemCheckpoint / _CrtMemDifference).
//
// Each test runs a steady-state operation cycle N times between two heap
// checkpoints and asserts the live-byte delta stays within a small
// tolerance. This catches real leaks: forgotten subscriptions, entities
// never returned to the free list, archive buffers never freed, sockets
// never closed. The live-BYTE delta is the truthful signal; the debug CRT's
// per-block count churns with transient allocations in debug builds and is
// printed for information only.
//
// Only compiled in Debug: the CRT debug heap does not exist in release.
// The timed-loop rule from performance_tests.cpp applies here too — no
// assertions inside the measured window.
#ifdef _DEBUG

#include "doctest.h"

#include <crtdbg.h>

#include "wiArchive.h"
#include "wiECS.h"
#include "wiEvent.h"
#include "wiFadeManager.h"
#include "wiJobSystem.h"
#include "wiNetwork.h"

#include <atomic>
#include <functional>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
	// Runs the workload between two CRT heap checkpoints and returns the
	// delta in live allocation count and live bytes. Transient allocations
	// (freed again inside the workload) do not show up.
	struct HeapDelta
	{
		long allocationCount = 0;
		long totalBytes = 0;
	};

	HeapDelta MeasureHeapDelta(const std::function<void()>& workload)
	{
		_CrtMemState before = {};
		_CrtMemState after = {};
		_CrtMemState diff = {};
		_CrtMemCheckpoint(&before);
		workload();
		_CrtMemCheckpoint(&after);
		_CrtMemDifference(&diff, &before, &after);
		HeapDelta delta;
		delta.allocationCount = diff.lTotalCount;
		delta.totalBytes = diff.lSizes[_NORMAL_BLOCK];
		return delta;
	}

	void ReportLeak(const char* name, const HeapDelta& delta, long byteTolerance)
	{
		std::printf("[leak] %-40s %+8ld bytes (tolerance %ld), %+ld count churn (informational)\n",
			name, delta.totalBytes, byteTolerance, delta.allocationCount);
	}
}

TEST_CASE("Leak: event subscribe and unsubscribe cycles return all memory")
{
	wiEvent::Handle keep;
	const HeapDelta delta = MeasureHeapDelta([&] {
		for (int i = 0; i < 20000; ++i)
		{
			keep = wiEvent::Subscribe(7654, [](uint64_t) {});
			wiEvent::FireEvent(7654, 0);
		}
		keep.internal_state.reset(); // final handle outlives the loop
	});
	wiEvent::FireEvent(7654, 0); // must not touch freed callbacks

	ReportLeak("event 20k subscribe/fire/unsubscribe", delta, 256);
	CHECK(delta.totalBytes <= 256);
}

TEST_CASE("Leak: entity create/remove cycles return all memory")
{
	const HeapDelta delta = MeasureHeapDelta([&] {
		for (int i = 0; i < 50000; ++i)
		{
			const wiECS::Entity entity = wiECS::CreateEntity();
			wiECS::ecs.OnComponentAdded(entity);
			wiECS::ecs.OnComponentRemoved(entity);
		}
	});

	// The free list may retain a handful of recycled handles (by design);
	// the per-cycle allocations must all be reclaimed.
	ReportLeak("ecs 50k create/remove cycles", delta, 256);
	CHECK(delta.totalBytes <= 256);
}

TEST_CASE("Leak: archive save and load cycles return all memory")
{
	const std::string path = (std::filesystem::temp_directory_path()
		/ "wicked_leak_test.archive").string();
	const std::string payload = "leak probe payload - 64 bytes of repeated text 1234567890abcdef";

	const HeapDelta delta = MeasureHeapDelta([&] {
		for (int i = 0; i < 200; ++i)
		{
			wiArchive writeArchive(path, false);
			writeArchive << payload;
			writeArchive.Close();

			wiArchive readArchive(path, true);
			std::string out;
			readArchive >> out;
		}
	});

	std::remove(path.c_str());
	ReportLeak("archive 200 save/load cycles", delta, 256);
	CHECK(delta.totalBytes <= 256);
}

TEST_CASE("Leak: job system execute/wait cycles reach steady state")
{
	// Warmup so thread-local and one-time allocations land before the
	// checkpoints:
	{
		wiJobSystem::context warmup;
		for (int i = 0; i < 1000; ++i)
		{
			wiJobSystem::Execute(warmup, [](wiJobArgs) {});
		}
		wiJobSystem::Wait(warmup);
	}

	std::atomic<int> executed{ 0 };
	const HeapDelta delta = MeasureHeapDelta([&] {
		for (int round = 0; round < 200; ++round)
		{
			wiJobSystem::context ctx;
			for (int i = 0; i < 100; ++i)
			{
				wiJobSystem::Execute(ctx, [&](wiJobArgs) { executed.fetch_add(1, std::memory_order_relaxed); });
			}
			wiJobSystem::Wait(ctx);
		}
	});

	ReportLeak("jobsystem 20k execute/wait cycles", delta, 1024);
	CHECK(delta.totalBytes <= 1024);
}

TEST_CASE("Leak: network socket create/destroy cycles return all memory")
{
	const HeapDelta delta = MeasureHeapDelta([&] {
		for (int i = 0; i < 200; ++i)
		{
			wiNetwork::Socket sock;
			REQUIRE(wiNetwork::CreateSocket(&sock));
			REQUIRE(wiNetwork::Destroy(&sock));
		}
	});

	ReportLeak("network 200 socket create/destroy", delta, 256);
	CHECK(delta.totalBytes <= 256);
}

TEST_CASE("Leak: screen fade restart cycles are allocation free")
{
	const HeapDelta delta = MeasureHeapDelta([&] {
		wiFadeManager fade;
		for (int i = 0; i < 100000; ++i)
		{
			fade.Update(0.016f);
			if (fade.state == wiFadeManager::FADE_FINISHED)
			{
				fade.Start(0.5f, wiColor(0, 0, 0, 255), [] {});
			}
		}
	});

	ReportLeak("fademanager 100k update/restart cycles", delta, 0);
	CHECK(delta.totalBytes == 0);
}

#endif // _DEBUG
