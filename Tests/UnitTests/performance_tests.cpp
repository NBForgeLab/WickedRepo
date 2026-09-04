// Performance regression tests for the engine's CPU modules.
//
// Philosophy: these budgets exist to catch ORDER-OF-MAGNITUDE regressions
// (a data structure turning O(n) into O(n^2), an accidental copy, a lock in
// a hot path), not micro-optimizations. Budgets are deliberately generous
// so shared CI machines stay stable; anything within budget prints its
// measured time so regressions show up as drift long before failure.
//
// Timed loops must not contain CHECK/REQUIRE calls (doctest allocations and
// per-iteration reporting would poison the measurement); results accumulate
// and are asserted once after the clock stops.
#include "doctest.h"

#include "wiArchive.h"
#include "wiContainers.h"
#include "wiECS.h"
#include "wiIntersect.h"
#include "wiJobSystem.h"
#include "wiMath.h"
#include "wiRectPacker.h"
#include "wiFadeManager.h"
#include "wiColor.h"

#include <atomic>
#include <functional>
#include <chrono>
#include <cstdio>
#include <vector>

namespace
{
	using Clock = std::chrono::steady_clock;

	double MeasureMillis(const std::function<void()>& workload)
	{
		const auto start = Clock::now();
		workload();
		const auto end = Clock::now();
		return std::chrono::duration<double, std::milli>(end - start).count();
	}

	void ReportBudget(const char* name, double measuredMillis, double budgetMillis)
	{
		std::printf("[perf] %-38s %8.1f ms  (budget %8.1f ms)\n", name, measuredMillis, budgetMillis);
	}
}

TEST_CASE("Performance: job system dispatches and drains 100k jobs")
{
	wiJobSystem::context ctx;
	std::atomic<int> executed{ 0 };

	const double millis = MeasureMillis([&] {
		for (int i = 0; i < 100000; ++i)
		{
			wiJobSystem::Execute(ctx, [&](wiJobArgs) { executed.fetch_add(1, std::memory_order_relaxed); });
		}
		wiJobSystem::Wait(ctx);
	});

	CHECK(executed.load() == 100000);
	ReportBudget("jobsystem 100k execute+wait", millis, 5000.0);
	CHECK(millis < 5000.0);
}

TEST_CASE("Performance: job system dispatches 100k jobs in 512 groups")
{
	wiJobSystem::context ctx;
	std::atomic<int> executed{ 0 };

	const double millis = MeasureMillis([&] {
		wiJobSystem::Dispatch(ctx, 100000, 196, [&](wiJobArgs) { executed.fetch_add(1, std::memory_order_relaxed); });
		wiJobSystem::Wait(ctx);
	});

	CHECK(executed.load() == 100000);
	ReportBudget("jobsystem 100k dispatch grouped", millis, 5000.0);
	CHECK(millis < 5000.0);
}

TEST_CASE("Performance: ring buffer sustains 1M push/pop round trips")
{
	wiContainers::ThreadSafeRingBuffer<int, 256> buffer;
	int sink = 0;

	const double millis = MeasureMillis([&] {
		for (int i = 0; i < 1000000; ++i)
		{
			while (!buffer.push_back(i)) { buffer.pop_front(sink); }
			buffer.pop_front(sink);
		}
	});

	CHECK(sink == 999999);
	ReportBudget("ringbuffer 1M push/pop", millis, 1000.0);
	CHECK(millis < 1000.0);
}

TEST_CASE("Performance: archive round-trips a 4 MB payload")
{
	const std::vector<float> payload(1000000, 1.5f);

	// Warmup so allocator growth does not count against the budget:
	{
		wiArchive warmup;
		warmup << payload;
	}

	std::vector<float> out;
	const double millis = MeasureMillis([&] {
		wiArchive archive;
		archive << payload;
		archive.SetReadModeAndResetPos(true);
		archive >> out;
	});
	CHECK(out.size() == payload.size());

	ReportBudget("archive 4MB round-trip", millis, 2000.0);
	CHECK(millis < 2000.0);
}

TEST_CASE("Performance: 1M ray-triangle intersections for picking")
{
	const XMVECTOR V0 = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR V1 = XMVectorSet(1, 0, 0, 1);
	const XMVECTOR V2 = XMVectorSet(0, 1, 0, 1);
	int hits = 0;

	const double millis = MeasureMillis([&] {
		for (int i = 0; i < 1000000; ++i)
		{
			float dist = 0;
			XMFLOAT2 bary(0, 0);
			// Origin varies inside the triangle so every ray hits
			// (barycentric u + v must stay <= 1):
			const float ox = 0.1f + (i & 255) * 0.001f;
			if (wiMath::RayTriangleIntersects(
				XMVectorSet(ox, ox, -1, 1), XMVectorSet(0, 0, 1, 0), V0, V1, V2, dist, bary))
			{
				++hits;
			}
		}
	});

	CHECK(hits == 1000000);
	ReportBudget("raytriangle 1M intersections", millis, 2000.0);
	CHECK(millis < 2000.0);
}

TEST_CASE("Performance: 500k frustum box culls")
{
	const XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 1000.0f);
	const XMMATRIX view = XMMatrixLookAtLH(
		XMVectorSet(0, 0, -5, 1), XMVectorSet(0, 0, 0, 1), XMVectorSet(0, 1, 0, 1));
	Frustum frustum;
	frustum.Create(view * projection);

	AABB box;
	box.createFromHalfWidth(XMFLOAT3(0, 0, 0), XMFLOAT3(0.5f, 0.5f, 0.5f));
	int inside = 0;

	const double millis = MeasureMillis([&] {
		for (int i = 0; i < 500000; ++i)
		{
			// Translate the box per iteration so the compiler cannot hoist
			// the frustum work out of the loop:
			AABB moved = box;
			moved._min.x += (i & 15);
			moved._max.x += (i & 15);
			if (frustum.CheckBoxFast(moved)) ++inside;
		}
	});

	CHECK(inside > 0);
	ReportBudget("frustum 500k box culls", millis, 2000.0);
	CHECK(millis < 2000.0);
}

TEST_CASE("Performance: 100k entity creations and component removals")
{
	const double millis = MeasureMillis([&] {
		for (int i = 0; i < 100000; ++i)
		{
			const wiECS::Entity entity = wiECS::CreateEntity();
			wiECS::ecs.OnComponentAdded(entity);
			wiECS::ecs.OnComponentRemoved(entity);
		}
	});

	ReportBudget("ecs 100k create+remove cycles", millis, 2000.0);
	CHECK(millis < 2000.0);
}

TEST_CASE("Performance: packer places 500 rectangles repeatedly")
{
	std::vector<wiRectPacker::rect_xywh> rects;
	for (int i = 0; i < 500; ++i)
	{
		rects.push_back(wiRectPacker::rect_xywh(0, 0, 8 + (i % 56), 8 + ((i * 7) % 56)));
	}
	std::vector<wiRectPacker::rect_xywh*> pointers;
	for (wiRectPacker::rect_xywh& r : rects) pointers.push_back(&r);

	// Warmup (the packer reuses its node tree between attempts):
	std::vector<wiRectPacker::bin> warmupBins;
	wiRectPacker::pack(pointers.data(), (int)pointers.size(), 1024, warmupBins);

	const double millis = MeasureMillis([&] {
		for (int round = 0; round < 20; ++round)
		{
			std::vector<wiRectPacker::bin> bins;
			wiRectPacker::pack(pointers.data(), (int)pointers.size(), 1024, bins);
		}
	});

	ReportBudget("rectpacker 20x 500 rects", millis, 10000.0);
	CHECK(millis < 10000.0);
}

TEST_CASE("Performance: screen fade state machine runs 200k updates")
{
	wiFadeManager fade;
	fade.Start(1.0f, wiColor(0, 0, 0, 255), [] {});

	const double millis = MeasureMillis([&] {
		for (int i = 0; i < 200000; ++i)
		{
			fade.Update(0.016f);
			if (fade.state == wiFadeManager::FADE_FINISHED)
			{
				fade.Start(1.0f, wiColor(0, 0, 0, 255), [] {});
			}
		}
	});

	ReportBudget("fademanager 200k updates", millis, 1000.0);
	CHECK(millis < 1000.0);
}
