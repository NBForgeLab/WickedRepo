// GameGuru integration contract tests.
//
// GameGuru MAX links the Wicked Engine static library built from THIS
// repository with GGREDUCED;GGREDUCEDEDITOR (the ReleaseForGG configuration
// in WickedEngine_Windows.vcxproj). The unit test target therefore compiles
// the engine sources with those same defines, and this suite pins the
// engine-side API that the GameGuru integration layer (wickedcalls.cpp)
// relies on. If a test here fails or stops compiling, the shipped GameGuru
// build is affected.
#include "doctest.h"

#include "wiJobSystem.h"

#include <atomic>

// The GG defines must be active for this target to mean anything:
#ifdef GGREDUCED
static_assert(true, "GGREDUCED active");
#else
static_assert(false, "Unit tests must compile with GGREDUCED (the GameGuru shipping configuration)");
#endif

TEST_CASE("GG contract: wiJobSystem::WaitSleep drains the context like Wait")
{
	// WaitSleep is a GameGuru fork addition (GGREDUCED only); the frame
	// loop in master.cpp relies on it yielding before helping with work.
	wiJobSystem::context ctx;
	std::atomic<int> executed{ 0 };

	for (int i = 0; i < 100; ++i)
	{
		wiJobSystem::Execute(ctx, [&](wiJobArgs) { executed.fetch_add(1); });
	}
	wiJobSystem::WaitSleep(ctx, 1);

	CHECK(executed.load() == 100);
	CHECK_FALSE(wiJobSystem::IsBusy(ctx));
	CHECK(ctx.counter.load() == 0);
}

TEST_CASE("GG contract: WaitSleep completes even when all work already finished")
{
	wiJobSystem::context ctx;
	wiJobSystem::WaitSleep(ctx, 1); // nothing was ever queued
	CHECK_FALSE(wiJobSystem::IsBusy(ctx));
}
