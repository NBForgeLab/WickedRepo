// Unit tests for the wiFadeManager module (wiFadeManager.h / wiFadeManager.cpp):
// the fade-in / hold / fade-out state machine, opacity progression and the
// onFade callback timing. Purely CPU-side, no renderer involved.
//
// The tests step Update() a fixed number of times instead of using elapsed
// wall time: t is computed BEFORE the timer advances (the very first update
// sees t=0), and the FADE_MID hold lasts exactly one update.
#include "doctest.h"

#include "wiFadeManager.h"

#include "wiColor.h"

namespace
{
	constexpr float target = 0.1f; // fade duration in seconds
	constexpr float step = 0.01f;  // update delta

	// Steps the fade until its state changes, with a generous safety bound.
	// (Float accumulation makes exact step counts unreliable, so the tests
	// key off state transitions instead.)
	void StepUntilStateChange(wiFadeManager& fade, int maxSteps = 1000)
	{
		const auto startingState = fade.state;
		for (int i = 0; i < maxSteps && fade.state == startingState; ++i)
		{
			fade.Update(step);
		}
	}
}

TEST_CASE("wiFadeManager starts inactive and Clear resets everything")
{
	wiFadeManager fade;
	CHECK_FALSE(fade.IsActive());
	CHECK(fade.state == wiFadeManager::FADE_FINISHED);
	CHECK(fade.opacity == 0.0f);

	fade.Start(target, wiColor(0, 0, 0, 255), [] {});
	CHECK(fade.IsActive());

	fade.Clear();
	CHECK_FALSE(fade.IsActive());
	CHECK(fade.opacity == 0.0f);
	CHECK(fade.state == wiFadeManager::FADE_FINISHED);
}

TEST_CASE("wiFadeManager progresses the opacity through the fade-in")
{
	int fadeCallbackCount = 0;
	wiFadeManager fade;
	fade.Start(target, wiColor(0, 0, 0, 255), [&] { ++fadeCallbackCount; });

	// The first update computes t=0, so the opacity is still zero:
	fade.Update(step);
	CHECK(fade.state == wiFadeManager::FADE_IN);
	CHECK(fade.opacity == 0.0f);

	// Mid-way through the fade-in the opacity is between 0 and 1:
	int guard = 0;
	while (fade.opacity < 0.4f && guard++ < 1000)
	{
		fade.Update(step);
	}
	CHECK(fade.state == wiFadeManager::FADE_IN);
	CHECK(fade.opacity > 0.25f);
	CHECK(fade.opacity < 0.75f);
	CHECK(fadeCallbackCount == 0);
}

TEST_CASE("wiFadeManager holds faded for one update, fires onFade once, fades out")
{
	int fadeCallbackCount = 0;
	wiFadeManager fade;
	fade.Start(target, wiColor(0, 0, 0, 255), [&] { ++fadeCallbackCount; });

	StepUntilStateChange(fade);
	CHECK(fade.state == wiFadeManager::FADE_MID);
	CHECK(fade.IsFaded());
	CHECK(fade.opacity == 1.0f);
	CHECK(fadeCallbackCount == 0);

	// A single update leaves the hold and fires the callback exactly once:
	fade.Update(step);
	CHECK(fade.state == wiFadeManager::FADE_OUT);
	CHECK(fadeCallbackCount == 1);

	// The fade-out runs all the way back down to zero:
	StepUntilStateChange(fade);
	CHECK(fade.state == wiFadeManager::FADE_FINISHED);
	CHECK_FALSE(fade.IsActive());
	CHECK(fade.opacity == 0.0f);

	// Further updates do nothing and never re-fire the callback:
	fade.Update(step);
	CHECK(fade.state == wiFadeManager::FADE_FINISHED);
	CHECK(fadeCallbackCount == 1);
}

TEST_CASE("wiFadeManager updates are clamped so a huge dt cannot skip the fade")
{
	wiFadeManager fade;
	fade.Start(1.0f, wiColor(0, 0, 0, 255), [] {});

	// dt is clamped to 33ms per update: even giant steps advance the fade
	// by at most one clamped step each:
	fade.Update(10.0f);
	fade.Update(10.0f);
	CHECK(fade.state == wiFadeManager::FADE_IN);
	CHECK(fade.opacity < 0.1f);
	CHECK(fade.timer == doctest::Approx(0.066f).epsilon(0.01f));
}

TEST_CASE("wiFadeManager zero-length fade skips straight to the callback")
{
	int fadeCallbackCount = 0;
	wiFadeManager fade;
	fade.Start(0.0f, wiColor(0, 0, 0, 255), [&] { ++fadeCallbackCount; });

	fade.Update(step);

	// Characterized behavior: with a zero target time the callback fires
	// immediately and the manager ends up finished, not faded:
	CHECK(fadeCallbackCount == 1);
	CHECK(fade.state == wiFadeManager::FADE_FINISHED);
	CHECK_FALSE(fade.IsFaded());
}

TEST_CASE("wiFadeManager restarts cleanly from any state")
{
	int firstCallback = 0;
	int secondCallback = 0;
	wiFadeManager fade;
	fade.Start(target, wiColor(0, 0, 0, 255), [&] { ++firstCallback; });
	StepUntilStateChange(fade);
	CHECK(fade.state == wiFadeManager::FADE_MID);

	// Restart while the first fade is holding:
	const uint32_t secondColor = wiColor(255, 0, 0, 255).rgba;
	fade.Start(target, wiColor(255, 0, 0, 255), [&] { ++secondCallback; });
	CHECK(fade.state == wiFadeManager::FADE_IN);
	CHECK(fade.timer == 0.0f);
	CHECK(fade.color.rgba == secondColor);

	// Run the new fade to completion; the abandoned fade's callback must
	// never fire:
	StepUntilStateChange(fade); // FADE_IN -> FADE_MID
	fade.Update(step);          // FADE_MID -> FADE_OUT (fires the new callback)
	StepUntilStateChange(fade); // FADE_OUT -> FADE_FINISHED
	CHECK(fade.state == wiFadeManager::FADE_FINISHED);
	CHECK(firstCallback == 0);
	CHECK(secondCallback == 1);
}
