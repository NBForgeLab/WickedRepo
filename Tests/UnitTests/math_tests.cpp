// Unit tests for the wiMath module (wiMath.h / wiMath.cpp).
// Covers scalar interpolation helpers, 2D collision, vector distance and
// closest-point queries, triangle geometry, barycentrics, Halton sequence,
// quaternion conversion, bezier evaluation and color/normal packing.
#include "doctest.h"

#include "wiMath.h"

#include <cmath>

namespace
{
	constexpr float EPS = 1e-5f;

	bool near(float a, float b, float eps = EPS)
	{
		return std::fabs(a - b) <= eps;
	}

	bool near3(const XMFLOAT3& a, const XMFLOAT3& b, float eps = EPS)
	{
		return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
	}
}

TEST_CASE("wiMath::Lerp interpolates and extrapolates its endpoints")
{
	CHECK(near(wiMath::Lerp(2.0f, 10.0f, 0.25f), 4.0f));
	CHECK(near(wiMath::Lerp(2.0f, 10.0f, 0.0f), 2.0f));
	CHECK(near(wiMath::Lerp(2.0f, 10.0f, 1.0f), 10.0f));

	const XMFLOAT3 a(0, 0, 0), b(4, 8, 12);
	CHECK(near3(wiMath::Lerp(a, b, 0.5f), XMFLOAT3(2, 4, 6)));
}

TEST_CASE("wiMath::InverseLerp is the inverse of Lerp")
{
	CHECK(near(wiMath::InverseLerp(2.0f, 10.0f, 4.0f), 0.25f));
	CHECK(near(wiMath::InverseLerp(2.0f, 10.0f, 2.0f), 0.0f));
	CHECK(near(wiMath::InverseLerp(2.0f, 10.0f, 10.0f), 1.0f));
}

TEST_CASE("wiMath::Clamp saturates to the given range")
{
	CHECK(near(wiMath::Clamp(5.0f, 0.0f, 10.0f), 5.0f));
	CHECK(near(wiMath::Clamp(-1.0f, 0.0f, 10.0f), 0.0f));
	CHECK(near(wiMath::Clamp(11.0f, 0.0f, 10.0f), 10.0f));
}

TEST_CASE("wiMath::SmoothStep follows the quintic curve with fixed endpoints")
{
	// 3t^2-style error would show here: this implementation is the quintic
	// t^3 (t (6t - 15) + 10) applied to the clamped, re-ranged amount.
	CHECK(near(wiMath::SmoothStep(0.0f, 1.0f, 0.0f), 0.0f));
	CHECK(near(wiMath::SmoothStep(0.0f, 1.0f, 1.0f), 1.0f));
	CHECK(near(wiMath::SmoothStep(0.0f, 1.0f, 0.5f), 0.5f));
	CHECK(near(wiMath::SmoothStep(0.0f, 1.0f, 0.25f), 0.10351f, 1e-4f));
	// Amounts outside [0,1] are clamped first:
	CHECK(near(wiMath::SmoothStep(0.0f, 1.0f, -5.0f), 0.0f));
	CHECK(near(wiMath::SmoothStep(0.0f, 1.0f, 5.0f), 1.0f));
}

TEST_CASE("wiMath::Collision2D detects overlap, edge-touch and separation")
{
	const XMFLOAT2 posA(0, 0), sizeA(10, 10);

	// Fully overlapping and partially overlapping boxes collide:
	CHECK(wiMath::Collision2D(posA, sizeA, XMFLOAT2(5, 5), XMFLOAT2(10, 10)));
	// Edge-touching counts as a collision (strict '<' comparisons):
	CHECK(wiMath::Collision2D(posA, sizeA, XMFLOAT2(10, 0), XMFLOAT2(5, 5)));
	// Separated on any single axis is not a collision:
	CHECK_FALSE(wiMath::Collision2D(posA, sizeA, XMFLOAT2(11, 0), XMFLOAT2(5, 5)));
	CHECK_FALSE(wiMath::Collision2D(posA, sizeA, XMFLOAT2(0, 11), XMFLOAT2(5, 5)));
}

TEST_CASE("wiMath::GetNextPowerOfTwo rounds up to the next power of two")
{
	CHECK(wiMath::GetNextPowerOfTwo(1u) == 1u);
	CHECK(wiMath::GetNextPowerOfTwo(2u) == 2u);
	CHECK(wiMath::GetNextPowerOfTwo(3u) == 4u);
	CHECK(wiMath::GetNextPowerOfTwo(4u) == 4u);
	CHECK(wiMath::GetNextPowerOfTwo(5u) == 8u);
	CHECK(wiMath::GetNextPowerOfTwo(17u) == 32u);
	CHECK(wiMath::GetNextPowerOfTwo(65536u) == 65536u);
	CHECK(wiMath::GetNextPowerOfTwo(65537u) == 131072u);
	// Characterized edge: zero wraps around to zero.
	CHECK(wiMath::GetNextPowerOfTwo(0u) == 0u);
}

TEST_CASE("wiMath vector helpers compute lengths, distances and midpoints")
{
	const XMFLOAT2 v2(3, 4);
	CHECK(near(wiMath::Length(v2), 5.0f));

	const XMFLOAT3 a(0, 0, 0), b(3, 4, 12);
	CHECK(near(wiMath::Distance(a, b), 13.0f));
	CHECK(near(wiMath::DistanceSquared(a, b), 169.0f));
	CHECK(near(wiMath::Distance(a, b), wiMath::Distance(XMLoadFloat3(&a), XMLoadFloat3(&b))));
	// Estimated distance is an approximation within a few percent:
	const float estimated = wiMath::DistanceEstimated(a, b);
	CHECK(estimated > 12.0f);
	CHECK(estimated < 14.0f);

	CHECK(near3(wiMath::getVectorHalfWayPoint(a, XMFLOAT3(2, 4, 6)), XMFLOAT3(1, 2, 3)));
	CHECK(near3(wiMath::Max(XMFLOAT3(1, 5, 3), XMFLOAT3(2, 2, 4)), XMFLOAT3(2, 5, 4)));
	CHECK(near3(wiMath::Min(XMFLOAT3(1, 5, 3), XMFLOAT3(2, 2, 4)), XMFLOAT3(1, 2, 3)));
}

TEST_CASE("wiMath closest-point queries project onto the line and clamp on segments")
{
	const XMVECTOR A = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR B = XMVectorSet(10, 0, 0, 1);
	const XMVECTOR P = XMVectorSet(5, 3, 0, 1);

	// Perpendicular foot on the infinite line:
	XMFLOAT3 onLine;
	XMStoreFloat3(&onLine, wiMath::ClosestPointOnLine(A, B, P));
	CHECK(near3(onLine, XMFLOAT3(5, 0, 0)));

	// A point beyond the segment is extrapolated on the line, clamped on the segment:
	const XMVECTOR Beyond = XMVectorSet(15, 3, 0, 1);
	XMFLOAT3 extrapolated, clamped;
	XMStoreFloat3(&extrapolated, wiMath::ClosestPointOnLine(A, B, Beyond));
	CHECK(near3(extrapolated, XMFLOAT3(15, 0, 0)));
	XMStoreFloat3(&clamped, wiMath::ClosestPointOnLineSegment(A, B, Beyond));
	CHECK(near3(clamped, XMFLOAT3(10, 0, 0)));

	// GetClosestPointToLine shares the clamping switch:
	XMFLOAT3 genericClamped;
	XMStoreFloat3(&genericClamped, wiMath::GetClosestPointToLine(A, B, Beyond, true));
	CHECK(near3(genericClamped, XMFLOAT3(10, 0, 0)));

	// Beyond the segment end the closest point is B itself:
	CHECK(near(wiMath::GetPointSegmentDistance(Beyond, A, B), std::sqrt(34.0f)));
	CHECK(near(wiMath::GetPointSegmentDistance(XMVectorSet(5, 3, 0, 1), A, B), 3.0f));
}

TEST_CASE("wiMath::TriangleArea agrees for the vector and side-length overloads")
{
	CHECK(near(wiMath::TriangleArea(3.0f, 4.0f, 5.0f), 6.0f));

	const XMVECTOR A = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR B = XMVectorSet(4, 0, 0, 1);
	const XMVECTOR C = XMVectorSet(0, 3, 0, 1);
	CHECK(near(wiMath::TriangleArea(A, B, C), 6.0f));

	// Degenerate triangle has zero area:
	const XMVECTOR D = XMVectorSet(8, 0, 0, 1);
	CHECK(near(wiMath::TriangleArea(A, B, D), 0.0f));
}

TEST_CASE("wiMath::GetBarycentric recovers vertices and centroid")
{
	const XMVECTOR a = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR b = XMVectorSet(10, 0, 0, 1);
	const XMVECTOR c = XMVectorSet(0, 10, 0, 1);

	float u, v, w;
	wiMath::GetBarycentric(a, a, b, c, u, v, w);
	CHECK(near(u, 1.0f));
	CHECK(near(v, 0.0f));
	CHECK(near(w, 0.0f));

	wiMath::GetBarycentric(b, a, b, c, u, v, w);
	CHECK(near(u, 0.0f));
	CHECK(near(v, 1.0f));
	CHECK(near(w, 0.0f));

	wiMath::GetBarycentric(c, a, b, c, u, v, w);
	CHECK(near(u, 0.0f));
	CHECK(near(v, 0.0f));
	CHECK(near(w, 1.0f));

	const XMVECTOR centroid = XMVectorSet(10.0f / 3.0f, 10.0f / 3.0f, 0, 1);
	wiMath::GetBarycentric(centroid, a, b, c, u, v, w);
	CHECK(near(u, 1.0f / 3.0f, 1e-4f));
	CHECK(near(v, 1.0f / 3.0f, 1e-4f));
	CHECK(near(w, 1.0f / 3.0f, 1e-4f));
}

TEST_CASE("wiMath::GetAngle returns the angle in [0, 2pi)")
{
	const XMFLOAT2 x(1, 0);
	CHECK(near(wiMath::GetAngle(x, XMFLOAT2(0, 1)), XM_PIDIV2));
	CHECK(near(wiMath::GetAngle(x, XMFLOAT2(-1, 0)), XM_PI));
	// Negative angles wrap into the positive range:
	CHECK(near(wiMath::GetAngle(x, XMFLOAT2(0, -1)), 3.0f * XM_PIDIV2, 1e-4f));
	CHECK(near(wiMath::GetAngle(x, x), 0.0f));
}

TEST_CASE("wiMath::GetBarycentric clamps outside points onto the triangle edge")
{
	const XMVECTOR a = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR b = XMVectorSet(10, 0, 0, 1);
	const XMVECTOR c = XMVectorSet(0, 10, 0, 1);
	float u, v, w;

	// A point beyond the AB edge projects onto that edge with clamping:
	wiMath::GetBarycentric(XMVectorSet(15, 0, 0, 1), a, b, c, u, v, w, true);
	CHECK(near(u, 0.0f, 1e-4f));
	CHECK(near(v, 1.0f, 1e-4f));
	CHECK(near(w, 0.0f, 1e-4f));

	// Unclamped, the same point reports coordinates outside [0,1]:
	wiMath::GetBarycentric(XMVectorSet(15, 0, 0, 1), a, b, c, u, v, w, false);
	const bool outsideUnitRange = (u < 0.0f) || (v > 1.0f);
	CHECK(outsideUnitRange);
}

TEST_CASE("wiMath ray-triangle variants agree on a shared hit case")
{
	const XMVECTOR V0 = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR V1 = XMVectorSet(1, 0, 0, 1);
	const XMVECTOR V2 = XMVectorSet(0, 1, 0, 1);
	const XMVECTOR origin = XMVectorSet(0.25f, 0.25f, -1, 1);
	const XMVECTOR direction = XMVectorSet(0, 0, 1, 0);

	float distNew = 0, distOld = 0;
	XMFLOAT2 baryNew(0, 0), baryOld(0, 0);
	CHECK(wiMath::RayTriangleIntersects(origin, direction, V0, V1, V2, distNew, baryNew));
	CHECK(wiMath::RayTriangleIntersects_OLD(origin, direction, V0, V1, V2, distOld, baryOld));
	CHECK(near(distNew, distOld, 1e-4f));
	CHECK(near(baryNew.x, baryOld.x, 1e-4f));
	CHECK(near(baryNew.y, baryOld.y, 1e-4f));
}

TEST_CASE("wiMath::Distance has a matching 2D overload")
{
	const XMFLOAT2 a(0, 0), b(3, 4);
	CHECK(near(wiMath::Distance(a, b), 5.0f));
}

TEST_CASE("wiMath::GetHaltonSequence returns low-discrepancy samples and wraps")
{
	const XMFLOAT4& first = wiMath::GetHaltonSequence(0);
	CHECK(near(first.x, 0.5f));
	CHECK(near(first.y, 1.0f / 3.0f, 1e-6f));
	CHECK(near(first.z, 0.2f, 1e-6f));

	// All samples stay in [0,1) and the sequence wraps via modulo:
	for (int i = 0; i < 512; ++i)
	{
		const XMFLOAT4& s = wiMath::GetHaltonSequence(i);
		CHECK(s.x >= 0.0f);
		CHECK(s.x < 1.0f);
		CHECK(s.y >= 0.0f);
		CHECK(s.y < 1.0f);
		CHECK(s.z >= 0.0f);
		CHECK(s.z < 1.0f);
	}
	const XMFLOAT4& wrapped = wiMath::GetHaltonSequence(256);
	CHECK(near(wrapped.x, 0.5f));
}

TEST_CASE("wiMath::QuaternionToRollPitchYaw converts known quaternions")
{
	// Identity has no rotation:
	const XMFLOAT3 identityAngles = wiMath::QuaternionToRollPitchYaw(XMFLOAT4(0, 0, 0, 1));
	CHECK(near3(identityAngles, XMFLOAT3(0, 0, 0), 1e-4f));

	// 60 degree rotation around X yields a 60 degree roll:
	const float half = XM_PI / 6.0f; // 30 degrees, half of 60
	const XMFLOAT3 rollAngles = wiMath::QuaternionToRollPitchYaw(XMFLOAT4(sinf(half), 0, 0, cosf(half)));
	CHECK(near(rollAngles.x, XM_PI / 3.0f, 1e-4f));
	CHECK(near(rollAngles.y, 0.0f, 1e-4f));
	CHECK(near(rollAngles.z, 0.0f, 1e-4f));
}

TEST_CASE("wiMath::getCubicHermiteSplinePos passes through its endpoints")
{
	const XMFLOAT3 start(1, 2, 3), end(5, 6, 7);
	const XMFLOAT3 startTangent(4, 4, 4), endTangent(-2, -2, -2);

	// The curve must interpolate its endpoints regardless of the tangents:
	CHECK(near3(wiMath::getCubicHermiteSplinePos(start, end, startTangent, endTangent, 0.0f), start));
	CHECK(near3(wiMath::getCubicHermiteSplinePos(start, end, startTangent, endTangent, 1.0f), end));
	// At t=0.5 the endpoint and tangent coefficients all collapse to 0.5*1:
	const XMFLOAT3 mid = wiMath::getCubicHermiteSplinePos(start, end, startTangent, endTangent, 0.5f);
	CHECK(near3(mid, XMFLOAT3(3.75f, 4.75f, 5.75f)));
}

TEST_CASE("wiMath::getQuadraticBezierPos evaluates the curve at its anchors")
{
	const XMFLOAT3 a(0, 0, 0), b(1, 0, 0), c(2, 0, 0);
	CHECK(near3(wiMath::getQuadraticBezierPos(a, b, c, 0.0f), a));
	CHECK(near3(wiMath::getQuadraticBezierPos(a, b, c, 0.5f), XMFLOAT3(1, 0, 0)));
	CHECK(near3(wiMath::getQuadraticBezierPos(a, b, c, 1.0f), c));
}

TEST_CASE("wiMath::ConstructTriangleEquilateral builds an equilateral triangle on the circle")
{
	XMFLOAT4 A, B, C;
	wiMath::ConstructTriangleEquilateral(1.0f, A, B, C);

	const XMVECTOR va = XMLoadFloat4(&A), vb = XMLoadFloat4(&B), vc = XMLoadFloat4(&C);
	CHECK(near(XMVectorGetX(XMVector3Length(va)), 1.0f));
	CHECK(near(XMVectorGetX(XMVector3Length(vb)), 1.0f));
	CHECK(near(XMVectorGetX(XMVector3Length(vc)), 1.0f));
	// Vertices are centered around the origin:
	const XMVECTOR sum = va + vb + vc;
	CHECK(near(XMVectorGetX(XMVector3Length(sum)), 0.0f, 1e-4f));
	// Side length of an equilateral triangle inscribed in a unit circle is sqrt(3):
	CHECK(near(XMVectorGetX(XMVector3Length(vb - va)), 1.7320508f, 1e-4f));
}

TEST_CASE("wiMath::RayTriangleIntersects returns hit data and rejects misses")
{
	const XMVECTOR V0 = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR V1 = XMVectorSet(1, 0, 0, 1);
	const XMVECTOR V2 = XMVectorSet(0, 1, 0, 1);

	float dist = 0;
	XMFLOAT2 bary(0, 0);

	// Straight hit through (0.25, 0.25) one unit in front of the triangle:
	const bool hit = wiMath::RayTriangleIntersects(
		XMVectorSet(0.25f, 0.25f, -1, 1), XMVectorSet(0, 0, 1, 0),
		V0, V1, V2, dist, bary);
	CHECK(hit);
	CHECK(near(dist, 1.0f));
	CHECK(near(bary.x, 0.25f));
	CHECK(near(bary.y, 0.25f));

	// Ray pointing away from the triangle:
	CHECK_FALSE(wiMath::RayTriangleIntersects(
		XMVectorSet(0.25f, 0.25f, 1, 1), XMVectorSet(0, 0, 1, 0),
		V0, V1, V2, dist, bary));

	// Ray parallel to the triangle plane:
	CHECK_FALSE(wiMath::RayTriangleIntersects(
		XMVectorSet(-1, 0.25f, 0, 1), XMVectorSet(1, 0, 0, 0),
		V0, V1, V2, dist, bary));

	// Ray that misses the triangle:
	CHECK_FALSE(wiMath::RayTriangleIntersects(
		XMVectorSet(2, 2, -1, 1), XMVectorSet(0, 0, 1, 0),
		V0, V1, V2, dist, bary));
}

TEST_CASE("wiMath compression helpers pack channels in byte order")
{
	// 0 maps to 127 (the +127.5 bias truncates), 1 maps to 255:
	CHECK(wiMath::CompressNormal(XMFLOAT3(0, 0, 1)) == (255u << 16 | 127u << 8 | 127u));
	// -1 maps to 0:
	CHECK(wiMath::CompressNormal(XMFLOAT3(-1, 0, 1)) == (255u << 16 | 127u << 8 | 0u));
	// Colors saturate into [0,1] before packing:
	CHECK(wiMath::CompressColor(XMFLOAT3(1, 1, 1)) == 0x00FFFFFF);
	CHECK(wiMath::CompressColor(XMFLOAT4(1, 1, 1, 1)) == 0xFFFFFFFF);
	CHECK(wiMath::CompressColor(XMFLOAT3(-1, 0.5f, 2)) == (255u << 16 | 127u << 8 | 0u));
}
