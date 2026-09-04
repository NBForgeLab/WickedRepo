// Unit tests for the wiRectPacker module (wiRectPacker.h / wiRectPacker.cpp):
// rectangle placement validity, invariants within bins and oversized input
// rejection.
#include "doctest.h"

#include "wiRectPacker.h"

#include <vector>

namespace
{
	// Verifies that every rectangle ends up inside its bin and no two
	// rectangles in the same bin overlap.
	void checkPlacement(const std::vector<wiRectPacker::bin>& bins)
	{
		REQUIRE(!bins.empty());
		for (const wiRectPacker::bin& b : bins)
		{
			REQUIRE(!b.rects.empty());
			CHECK(b.size.w > 0);
			CHECK(b.size.h > 0);
			for (const wiRectPacker::rect_xywh* r : b.rects)
			{
				CHECK(r->x >= 0);
				CHECK(r->y >= 0);
				CHECK(r->x + r->w <= b.size.w);
				CHECK(r->y + r->h <= b.size.h);
			}
			for (size_t i = 0; i < b.rects.size(); ++i)
			{
				for (size_t j = i + 1; j < b.rects.size(); ++j)
				{
					const wiRectPacker::rect_xywh* a = b.rects[i];
					const wiRectPacker::rect_xywh* c = b.rects[j];
					const bool overlaps =
						a->x < c->x + c->w && a->x + a->w > c->x &&
						a->y < c->y + c->h && a->y + a->h > c->y;
					CHECK_FALSE(overlaps);
				}
			}
		}
	}
}

TEST_CASE("pack places a mixed set of rectangles without overlap")
{
	std::vector<wiRectPacker::rect_xywh> rects = {
		wiRectPacker::rect_xywh(0, 0, 64, 64),
		wiRectPacker::rect_xywh(0, 0, 32, 32),
		wiRectPacker::rect_xywh(0, 0, 128, 16),
		wiRectPacker::rect_xywh(0, 0, 16, 128),
		wiRectPacker::rect_xywh(0, 0, 48, 48),
		wiRectPacker::rect_xywh(0, 0, 8, 8),
	};
	std::vector<wiRectPacker::rect_xywh*> pointers;
	for (wiRectPacker::rect_xywh& r : rects) pointers.push_back(&r);

	std::vector<wiRectPacker::bin> bins;
	const bool ok = wiRectPacker::pack(pointers.data(), (int)pointers.size(), 512, bins);
	CHECK(ok);

	// Every input rectangle was placed exactly once:
	size_t placed = 0;
	for (const wiRectPacker::bin& b : bins) placed += b.rects.size();
	CHECK(placed == rects.size());

	checkPlacement(bins);
}

TEST_CASE("pack fails when a rectangle exceeds max_side")
{
	std::vector<wiRectPacker::rect_xywh> rects = {
		wiRectPacker::rect_xywh(0, 0, 64, 64),
		wiRectPacker::rect_xywh(0, 0, 32, 32),
	};
	std::vector<wiRectPacker::rect_xywh*> pointers;
	for (wiRectPacker::rect_xywh& r : rects) pointers.push_back(&r);

	std::vector<wiRectPacker::bin> bins;
	CHECK_FALSE(wiRectPacker::pack(pointers.data(), (int)pointers.size(), 32, bins));
}

TEST_CASE("pack fits four equal squares into a single square bin")
{
	std::vector<wiRectPacker::rect_xywh> rects = {
		wiRectPacker::rect_xywh(0, 0, 8, 8),
		wiRectPacker::rect_xywh(0, 0, 8, 8),
		wiRectPacker::rect_xywh(0, 0, 8, 8),
		wiRectPacker::rect_xywh(0, 0, 8, 8),
	};
	std::vector<wiRectPacker::rect_xywh*> pointers;
	for (wiRectPacker::rect_xywh& r : rects) pointers.push_back(&r);

	std::vector<wiRectPacker::bin> bins;
	CHECK(wiRectPacker::pack(pointers.data(), (int)pointers.size(), 32, bins));
	checkPlacement(bins);

	// 4 * 64 area units always land in one bin for a 32x32 max side:
	CHECK(bins.size() == 1);
}

TEST_CASE("rect_wh::fits classifies no-fit, fit and perfect fit")
{
	const wiRectPacker::rect_wh bin(16, 16);
	CHECK(wiRectPacker::rect_wh(16, 16).fits(bin) == 2); // perfect
	CHECK(wiRectPacker::rect_wh(8, 16).fits(bin) == 1);  // fits
	CHECK(wiRectPacker::rect_wh(17, 8).fits(bin) == 0);  // too wide
	CHECK(wiRectPacker::rect_wh(8, 17).fits(bin) == 0);  // too tall
}
