// Unit tests for the wiIntersect module (wiIntersect.h / wiIntersect.cpp):
// AABB, SPHERE, RAY, Frustum and Hitbox2D primitives and their intersection
// semantics, including documented boundary behavior.
#include "doctest.h"

#include "wiIntersect.h"
#include "wiMath.h"

#include <cmath>

namespace
{
	constexpr float EPS = 1e-5f;

	bool near3v(const XMFLOAT3& a, const XMFLOAT3& b, float eps = EPS)
	{
		return std::fabs(a.x - b.x) <= eps
			&& std::fabs(a.y - b.y) <= eps
			&& std::fabs(a.z - b.z) <= eps;
	}

	AABB unitBox()
	{
		AABB box;
		box.createFromHalfWidth(XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
		return box;
	}
}

TEST_CASE("AABB constructed from center and halfwidth has the right metrics")
{
	const AABB box = unitBox();

	CHECK(near3v(box.getCenter(), XMFLOAT3(0, 0, 0)));
	CHECK(near3v(box.getHalfWidth(), XMFLOAT3(1, 1, 1)));
	CHECK(near3v(box.getMin(), XMFLOAT3(-1, -1, -1)));
	CHECK(near3v(box.getMax(), XMFLOAT3(1, 1, 1)));
	// Volume-style area (per this fork's implementation):
	CHECK(std::fabs(box.getArea() - 8.0f) < EPS);
	// Radius is the largest halfwidth:
	CHECK(std::fabs(box.getRadius() - 1.0f) < EPS);

	// Non-cubic box:
	AABB slab;
	slab.createFromHalfWidth(XMFLOAT3(10, 20, 30), XMFLOAT3(4, 2, 1));
	CHECK(near3v(slab.getCenter(), XMFLOAT3(10, 20, 30)));
	CHECK(near3v(slab.getHalfWidth(), XMFLOAT3(4, 2, 1)));
	CHECK(std::fabs(slab.getArea() - (8.0f * 4.0f * 2.0f)) < EPS);
}

TEST_CASE("AABB::corner enumerates the eight box corners")
{
	const AABB box = unitBox();
	CHECK(near3v(box.corner(0), XMFLOAT3(-1, -1, -1)));
	CHECK(near3v(box.corner(6), XMFLOAT3(1, 1, 1)));
	CHECK(near3v(box.corner(7), XMFLOAT3(1, -1, 1)));
	CHECK(near3v(box.corner(3), XMFLOAT3(-1, -1, 1)));
}

TEST_CASE("AABB::Merge produces the enclosing box")
{
	AABB a, b;
	a.createFromHalfWidth(XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
	b.createFromHalfWidth(XMFLOAT3(3, 0, 0), XMFLOAT3(1, 1, 1));

	const AABB merged = AABB::Merge(a, b);
	CHECK(near3v(merged.getMin(), XMFLOAT3(-1, -1, -1)));
	CHECK(near3v(merged.getMax(), XMFLOAT3(4, 1, 1)));
}

TEST_CASE("AABB::operator* scales the box extents")
{
	const AABB scaled = unitBox() * 2.0f;
	CHECK(near3v(scaled.getMin(), XMFLOAT3(-2, -2, -2)));
	CHECK(near3v(scaled.getMax(), XMFLOAT3(2, 2, 2)));
}

TEST_CASE("AABB::intersects classifies containment, overlap, touch and separation")
{
	AABB a, b, contained, touching, separate;
	a.createFromHalfWidth(XMFLOAT3(0, 0, 0), XMFLOAT3(2, 2, 2));       // [-2,2]
	b.createFromHalfWidth(XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));       // [-1,1]
	contained.createFromHalfWidth(XMFLOAT3(1, 1, 1), XMFLOAT3(0.5f, 0.5f, 0.5f)); // inside a
	touching.createFromHalfWidth(XMFLOAT3(4, 0, 0), XMFLOAT3(2, 2, 2)); // starts exactly at a._max.x = 2
	separate.createFromHalfWidth(XMFLOAT3(10, 0, 0), XMFLOAT3(1, 1, 1));

	// The INSIDE verdict is reported from the larger box's perspective:
	CHECK(a.intersects(a) == AABB::INTERSECTION_TYPE::INSIDE);
	CHECK(a.intersects(b) == AABB::INTERSECTION_TYPE::INSIDE);
	CHECK(a.intersects(contained) == AABB::INTERSECTION_TYPE::INSIDE);
	// From the smaller box's perspective the larger one merely overlaps:
	CHECK(b.intersects(a) == AABB::INTERSECTION_TYPE::INTERSECTS);
	// Boxes sharing exactly one face plane count as INTERSECTS (not OUTSIDE):
	CHECK(a.intersects(touching) == AABB::INTERSECTION_TYPE::INTERSECTS);
	CHECK(a.intersects(separate) == AABB::INTERSECTION_TYPE::OUTSIDE);
}

TEST_CASE("AABB::intersects2D ignores the Z axis")
{
	AABB a, zSeparated;
	a.createFromHalfWidth(XMFLOAT3(0, 0, 0), XMFLOAT3(5, 5, 1));
	zSeparated.createFromHalfWidth(XMFLOAT3(1, 0, 100), XMFLOAT3(1, 1, 1));

	// Overlapping in X/Y but far apart in Z still counts in 2D:
	CHECK(a.intersects2D(zSeparated) == AABB::INTERSECTION_TYPE::INSIDE);
}

TEST_CASE("AABB point containment is inclusive of the box faces")
{
	const AABB box = unitBox();
	CHECK(box.intersects(XMFLOAT3(0, 0, 0)));
	CHECK(box.intersects(XMFLOAT3(1, 1, 1)));
	CHECK(box.intersects(XMFLOAT3(-1, -1, -1)));
	CHECK_FALSE(box.intersects(XMFLOAT3(1.0001f, 0, 0)));
	CHECK_FALSE(box.intersects(XMFLOAT3(0, -5, 0)));
}

TEST_CASE("AABB::intersects(RAY) hits, misses and rejects rays behind the box")
{
	const AABB box = unitBox();

	// Ray straight through the box:
	RAY through(XMFLOAT3(0, 0, -5), XMFLOAT3(0, 0, 1));
	CHECK(box.intersects(through));

	// Ray starting inside the box:
	RAY inside(XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 1));
	CHECK(box.intersects(inside));

	// Ray passing beside the box:
	RAY beside(XMFLOAT3(5, 0, -5), XMFLOAT3(0, 0, 1));
	CHECK_FALSE(box.intersects(beside));

	// NOTE: the slab test has no hit-distance guard, so a box that lies
	// entirely behind the ray origin still reports a hit. This documents the
	// existing line-intersection semantics of this fork's ray tests.
	RAY away(XMFLOAT3(0, 0, 5), XMFLOAT3(0, 0, 1));
	CHECK(box.intersects(away));

	// Axis-aligned ray hits boxes off-center on the other axes:
	RAY offAxis(XMFLOAT3(0.5f, -0.5f, -5), XMFLOAT3(0, 0, 1));
	CHECK(box.intersects(offAxis));
}

TEST_CASE("AABB::transform moves, scales and rotates the box bounds")
{
	const AABB box = unitBox();

	// Translation shifts min and max:
	const AABB moved = box.transform(XMMatrixTranslation(10, 0, 0));
	CHECK(near3v(moved.getMin(), XMFLOAT3(9, -1, -1)));
	CHECK(near3v(moved.getMax(), XMFLOAT3(11, 1, 1)));

	// 90 degree rotation around Y swaps X and Z extents:
	AABB slab;
	slab.createFromHalfWidth(XMFLOAT3(0, 0, 0), XMFLOAT3(2, 1, 0.5f));
	const AABB rotated = slab.transform(XMMatrixRotationY(XM_PIDIV2));
	CHECK(near3v(rotated.getHalfWidth(), XMFLOAT3(0.5f, 1, 2), 1e-4f));

	// Uniform scaling doubles the extents:
	const AABB scaled = box.transform(XMMatrixScaling(2, 2, 2));
	CHECK(near3v(scaled.getHalfWidth(), XMFLOAT3(2, 2, 2)));
}

TEST_CASE("AABB serializes through wiArchive and restores its bounds")
{
	AABB box;
	box.createFromHalfWidth(XMFLOAT3(3, 4, 5), XMFLOAT3(1, 2, 3));
	box.layerMask = 0x12345678;
	box.userdata = 0x0A0B0C0D;

	wiArchive archive;
	wiECS::EntitySerializer seri;
	box.Serialize(archive, seri);
	archive.SetReadModeAndResetPos(true);

	AABB restored;
	wiECS::EntitySerializer readSeri;
	restored.Serialize(archive, readSeri);

	CHECK(near3v(restored.getMin(), XMFLOAT3(2, 2, 2)));
	CHECK(near3v(restored.getMax(), XMFLOAT3(4, 6, 8)));
	CHECK(restored.layerMask == 0x12345678);
	CHECK(restored.userdata == 0x0A0B0C0D);
}

TEST_CASE("AABB::transform accepts the packed matrix overload")
{
	const AABB box = unitBox();
	XMFLOAT4X4 translation;
	XMStoreFloat4x4(&translation, XMMatrixTranslation(0, 5, 0));

	const AABB moved = box.transform(translation);
	CHECK(near3v(moved.getCenter(), XMFLOAT3(0, 5, 0)));
}

TEST_CASE("SPHERE::intersects uses distance to the closest point in the box")
{
	const AABB box = unitBox();

	// Center inside the box:
	CHECK(SPHERE(XMFLOAT3(0, 0, 0), 0.1f).intersects(box));
	// Overlapping the face plane:
	CHECK(SPHERE(XMFLOAT3(1.5f, 0, 0), 0.75f).intersects(box));
	// Distance to the box equals the radius exactly is NOT a hit (strict <):
	CHECK_FALSE(SPHERE(XMFLOAT3(2, 0, 0), 1.0f).intersects(box));
	// Just past the face:
	CHECK(SPHERE(XMFLOAT3(2, 0, 0), 1.1f).intersects(box));
	// Clear miss:
	CHECK_FALSE(SPHERE(XMFLOAT3(5, 5, 5), 1.0f).intersects(box));
}

TEST_CASE("SPHERE::intersects(SPHERE) treats exact touch as intersecting")
{
	CHECK(SPHERE(XMFLOAT3(0, 0, 0), 1).intersects(SPHERE(XMFLOAT3(2, 0, 0), 1))); // d == r1 + r2
	CHECK(SPHERE(XMFLOAT3(0, 0, 0), 1).intersects(SPHERE(XMFLOAT3(1, 0, 0), 1)));
	CHECK_FALSE(SPHERE(XMFLOAT3(0, 0, 0), 1).intersects(SPHERE(XMFLOAT3(2.1f, 0, 0), 1)));
}

TEST_CASE("RAY::intersects(SPHERE) tests line distance (documented semantics)")
{
	// Perpendicular ray through the sphere:
	RAY through(XMFLOAT3(0, 5, 0), XMFLOAT3(0, -1, 0));
	CHECK(through.intersects(SPHERE(XMFLOAT3(0, 0, 0), 1)));

	// Line passes beside the sphere:
	RAY beside(XMFLOAT3(5, 5, 0), XMFLOAT3(0, -1, 0));
	CHECK_FALSE(beside.intersects(SPHERE(XMFLOAT3(0, 0, 0), 1)));

	// NOTE: the current implementation uses XMVector3LinePointDistance, i.e.
	// the distance to the *infinite line*. A ray pointing straight away from
	// a sphere that lies behind it still reports an intersection. This test
	// documents that existing semantics explicitly.
	RAY away(XMFLOAT3(0, 0, 5), XMFLOAT3(0, 0, 1));
	CHECK(away.intersects(SPHERE(XMFLOAT3(0, 0, 0), 1)));
}

TEST_CASE("CAPSULE::intersects detects overlapping and separated capsules")
{
	XMFLOAT3 position(0, 0, 0), normal(0, 0, 0);
	float penetration = 0;

	// Two parallel vertical capsules whose axes are 0.8 apart with radii
	// 0.5 + 0.5: they intersect with a 0.2 penetration depth.
	const CAPSULE a(XMFLOAT3(0, 0, 0), XMFLOAT3(0, 2, 0), 0.5f);
	const CAPSULE b(XMFLOAT3(0.8f, 0.5f, 0), XMFLOAT3(0.8f, 1.5f, 0), 0.5f);
	CHECK(a.intersects(b, position, normal, penetration));
	CHECK(std::fabs(penetration - 0.2f) < 1e-4f);
	// The contact normal is unit length:
	const float normalLength = std::sqrt(
		normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
	CHECK(std::fabs(normalLength - 1.0f) < 1e-4f);

	// Far apart:
	const CAPSULE far_away(XMFLOAT3(5, 0, 0), XMFLOAT3(5, 2, 0), 0.5f);
	CHECK_FALSE(a.intersects(far_away, position, normal, penetration));

	// Just touching (axis distance == r1 + r2) does not report penetration:
	const CAPSULE touching(XMFLOAT3(1, 0, 0), XMFLOAT3(1, 2, 0), 0.5f);
	CHECK_FALSE(a.intersects(touching, position, normal, penetration));
}

TEST_CASE("CAPSULE::getAABB encloses both end caps")
{
	const CAPSULE capsule(XMFLOAT3(0, 0, 0), XMFLOAT3(0, 4, 0), 1);
	const AABB box = capsule.getAABB();
	CHECK(near3v(box.getMin(), XMFLOAT3(-1, -1, -1)));
	CHECK(near3v(box.getMax(), XMFLOAT3(1, 5, 1)));
}

namespace
{
	// Left-handed view from (0,0,-5) toward the origin with a 90 degree
	// vertical FOV and square aspect: the frustum spans +/-45 degrees around
	// the Z axis, with near plane 0.1 and far plane 1000.
	Frustum makeSimpleFrustum()
	{
		// DirectXMath composes row-major, world -> view -> projection:
		const XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 1000.0f);
		const XMMATRIX view = XMMatrixLookAtLH(
			XMVectorSet(0, 0, -5, 1), XMVectorSet(0, 0, 0, 1), XMVectorSet(0, 1, 0, 1));
		Frustum frustum;
		frustum.Create(view * projection);
		return frustum;
	}
}

TEST_CASE("Frustum::Create plus CheckPoint classifies inside and outside points")
{
	const Frustum frustum = makeSimpleFrustum();

	CHECK(frustum.CheckPoint(XMFLOAT3(0, 0, 0)));       // on the view axis
	CHECK(frustum.CheckPoint(XMFLOAT3(3, 0, 5)));       // within 45 degrees at that depth
	CHECK_FALSE(frustum.CheckPoint(XMFLOAT3(100, 0, 5)));  // outside the side planes
	CHECK_FALSE(frustum.CheckPoint(XMFLOAT3(0, 0, -5)));   // at the camera, behind near plane
	CHECK_FALSE(frustum.CheckPoint(XMFLOAT3(0, 0, -100))); // behind the camera
}

TEST_CASE("Frustum::CheckSphere respects the radius as a tolerance")
{
	const Frustum frustum = makeSimpleFrustum();

	CHECK(frustum.CheckSphere(XMFLOAT3(0, 0, 0), 0.5f));
	CHECK(frustum.CheckSphere(XMFLOAT3(100, 0, 5), 0.5f) == false);
	// A large enough sphere reaches across a side plane:
	CHECK(frustum.CheckSphere(XMFLOAT3(9.5f, 0, 5), 2.0f));
}

TEST_CASE("Frustum::CheckBox classifies inside, intersecting and outside boxes")
{
	const Frustum frustum = makeSimpleFrustum();

	AABB centerBox;
	centerBox.createFromHalfWidth(XMFLOAT3(0, 0, 0), XMFLOAT3(0.5f, 0.5f, 0.5f));
	CHECK(frustum.CheckBox(centerBox) == Frustum::BOX_FRUSTUM_INSIDE);
	CHECK(frustum.CheckBoxFast(centerBox));

	AABB straddlingBox;
	// Straddles the right side plane: some corners inside, some outside.
	straddlingBox.createFromHalfWidth(XMFLOAT3(10.5f, 0, 5), XMFLOAT3(1, 1, 1));
	CHECK(frustum.CheckBox(straddlingBox) == Frustum::BOX_FRUSTUM_INTERSECTS);
	// CheckBoxFast only rejects boxes that are provably outside a whole
	// plane; a straddling box cannot be rejected cheaply:
	CHECK(frustum.CheckBoxFast(straddlingBox));

	AABB whollyOutsideBox;
	// Entirely beyond the right side plane at that depth:
	whollyOutsideBox.createFromHalfWidth(XMFLOAT3(50, 0, 5), XMFLOAT3(1, 1, 1));
	CHECK(frustum.CheckBox(whollyOutsideBox) == Frustum::BOX_FRUSTUM_OUTSIDE);
	CHECK_FALSE(frustum.CheckBoxFast(whollyOutsideBox));

	AABB behindBox;
	behindBox.createFromHalfWidth(XMFLOAT3(0, 0, -50), XMFLOAT3(1, 1, 1));
	CHECK(frustum.CheckBox(behindBox) == Frustum::BOX_FRUSTUM_OUTSIDE);
	CHECK_FALSE(frustum.CheckBoxFast(behindBox));
}

TEST_CASE("Frustum plane getters expose the six planes")
{
	const Frustum frustum = makeSimpleFrustum();
	const XMFLOAT4 nearPlane = frustum.getNearPlane();
	const XMFLOAT4 farPlane = frustum.getFarPlane();
	const XMFLOAT4 leftPlane = frustum.getLeftPlane();
	const XMFLOAT4 rightPlane = frustum.getRightPlane();
	const XMFLOAT4 topPlane = frustum.getTopPlane();
	const XMFLOAT4 bottomPlane = frustum.getBottomPlane();

	// Planes are normalized:
	for (const XMFLOAT4* plane : { &nearPlane, &farPlane, &leftPlane, &rightPlane, &topPlane, &bottomPlane })
	{
		const float length = std::sqrt(plane->x * plane->x + plane->y * plane->y + plane->z * plane->z);
		CHECK(std::fabs(length - 1.0f) < 1e-4f);
	}
}

TEST_CASE("Hitbox2D::intersects follows Collision2D semantics")
{
	const Hitbox2D a(XMFLOAT2(0, 0), XMFLOAT2(10, 10));
	CHECK(a.intersects(Hitbox2D(XMFLOAT2(5, 5), XMFLOAT2(10, 10))));
	CHECK(a.intersects(Hitbox2D(XMFLOAT2(10, 10), XMFLOAT2(10, 10)))); // edge touch
	CHECK_FALSE(a.intersects(Hitbox2D(XMFLOAT2(11, 0), XMFLOAT2(10, 10))));

	// A degenerate zero-size hitbox collides only when its single point
	// lies within the other box (boundary-inclusive):
	const Hitbox2D spot(XMFLOAT2(5, 5), XMFLOAT2(0, 0));
	CHECK(a.intersects(spot));
	CHECK_FALSE(spot.intersects(Hitbox2D(XMFLOAT2(6, 5), XMFLOAT2(0, 0))));
}
