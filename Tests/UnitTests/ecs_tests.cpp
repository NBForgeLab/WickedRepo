// Unit tests for the wiECS module (wiECS.h / wiECS.cpp): entity handles,
// the component manager dense/sparse storage contract and sorted removal.
#include "doctest.h"

#include "wiECS.h"

#include <algorithm>
#include <vector>

TEST_CASE("ComponentManager stores, finds and removes components by entity")
{
	wiECS::ComponentManager<int> manager(1024);

	// Entities come from the global manager, as callers are expected to:
	const wiECS::Entity a = wiECS::CreateEntity();
	const wiECS::Entity b = wiECS::CreateEntity();
	const wiECS::Entity c = wiECS::CreateEntity();

	CHECK_FALSE(manager.Contains(a));
	manager.Create(a) = 100;
	manager.Create(b) = 200;
	manager.Create(c) = 300;

	REQUIRE(manager.GetCount() == 3);
	REQUIRE(manager.GetEntitiesCount() == 3);
	CHECK(manager.Contains(a));
	CHECK(manager.Contains(b));
	CHECK(manager.Contains(c));

	CHECK(*manager.GetComponent(a) == 100);
	CHECK(*manager.GetComponent(b) == 200);
	CHECK(*manager.GetComponent(c) == 300);

	// Sparse lookup is identity-preserving:
	const size_t indexB = manager.GetIndex(b);
	CHECK(manager.GetEntity(indexB) == b);
	CHECK(manager[indexB] == 200);

	manager.Remove(b);
	CHECK_FALSE(manager.Contains(b));
	CHECK(manager.GetComponent(b) == nullptr);
	CHECK(manager.GetCount() == 2);
	CHECK(*manager.GetComponent(a) == 100);
	CHECK(*manager.GetComponent(c) == 300);
}

TEST_CASE("ComponentManager::Remove_KeepSorted preserves entity order")
{
	wiECS::ComponentManager<int> manager(1024);
	const wiECS::Entity e1 = wiECS::CreateEntity();
	const wiECS::Entity e2 = wiECS::CreateEntity();
	const wiECS::Entity e3 = wiECS::CreateEntity();
	manager.Create(e1) = 1;
	manager.Create(e2) = 2;
	manager.Create(e3) = 3;

	manager.Remove_KeepSorted(e2);

	REQUIRE(manager.GetCount() == 2);
	// Dense arrays keep their relative order after a sorted removal:
	CHECK(manager.GetEntity(0) == e1);
	CHECK(manager.GetEntity(1) == e3);
	CHECK(*manager.GetComponent(e1) == 1);
	CHECK(*manager.GetComponent(e3) == 3);
	CHECK(manager.GetIndex(e3) == 1);
}

namespace
{
	// ComponentManager<T>::Serialize delegates to T::Serialize(archive, seri),
	// so the round-trip test uses a component shaped like the engine's own.
	struct TestComponent
	{
		int value = 0;

		void Serialize(wiArchive& archive, wiECS::EntitySerializer& seri)
		{
			(void)seri;
			if (archive.IsReadMode())
			{
				archive >> value;
			}
			else
			{
				archive << value;
			}
		}
	};
}

TEST_CASE("The global entity manager recycles handles after their components are removed")
{
	const wiECS::Entity entity = wiECS::CreateEntity();
	CHECK(entity != wiECS::INVALID_ENTITY);
	// State right after this creation; earlier suites may have reused
	// handles already, so the reuse count is measured relatively:
	const uint32_t reusedAfterCreate = wiECS::GetReusedEntityIDs();

	// A handle only returns to the free list when its last component goes:
	wiECS::ecs.OnComponentAdded(entity);
	wiECS::ecs.OnComponentAdded(entity);
	wiECS::ecs.OnComponentRemoved(entity);
	wiECS::ecs.OnComponentRemoved(entity);

	// The next creation must reuse the recycled handle:
	const wiECS::Entity recycled = wiECS::CreateEntity();
	CHECK(recycled == entity);
	CHECK(wiECS::GetReusedEntityIDs() == reusedAfterCreate + 1);
}

TEST_CASE("Concurrent entity creation hands out unique handles")
{
	constexpr int threadsCount = 4;
	constexpr int perThread = 2000;
	std::vector<std::vector<wiECS::Entity>> results(threadsCount);
	std::vector<std::thread> threads;
	for (int t = 0; t < threadsCount; ++t)
	{
		threads.emplace_back([&results, t, perThread] {
			results[t].reserve(perThread);
			for (int i = 0; i < perThread; ++i)
			{
				results[t].push_back(wiECS::CreateEntity());
			}
		});
	}
	for (std::thread& t : threads)
	{
		t.join();
	}

	// Every handle must be valid and globally unique:
	std::vector<wiECS::Entity> all;
	all.reserve(threadsCount * perThread);
	for (const auto& list : results)
	{
		for (wiECS::Entity e : list)
		{
			CHECK(e != wiECS::INVALID_ENTITY);
			all.push_back(e);
		}
	}
	std::sort(all.begin(), all.end());
	CHECK(std::adjacent_find(all.begin(), all.end()) == all.end());
}

TEST_CASE("ComponentManager::Merge moves all components into the destination")
{
	wiECS::ComponentManager<int> dst(1024);
	dst.Create(wiECS::CreateEntity()) = 1;

	wiECS::ComponentManager<int> src(1024);
	const wiECS::Entity s1 = wiECS::CreateEntity();
	const wiECS::Entity s2 = wiECS::CreateEntity();
	src.Create(s1) = 10;
	src.Create(s2) = 20;

	dst.Merge(src);

	CHECK(dst.GetCount() == 3);
	CHECK(src.GetCount() == 0);
	CHECK(src.GetEntitiesCount() == 0);
	CHECK(*dst.GetComponent(s1) == 10);
	CHECK(*dst.GetComponent(s2) == 20);
	// Sparse lookups stay intact after the merge:
	CHECK(dst.GetIndex(s2) == 2);
}

TEST_CASE("ComponentManager::Copy replaces the destination contents")
{
	wiECS::ComponentManager<int> dst(1024);
	dst.Create(wiECS::CreateEntity()) = 99;

	wiECS::ComponentManager<int> src(1024);
	const wiECS::Entity s1 = wiECS::CreateEntity();
	src.Create(s1) = 42;

	dst.Copy(src);

	CHECK(dst.GetCount() == 1);
	CHECK(*dst.GetComponent(s1) == 42);
	// The copied-out entity is gone:
	for (size_t i = 0; i < dst.GetEntitiesCount(); ++i)
	{
		CHECK(dst.GetEntity(i) == s1);
	}
}

TEST_CASE("ComponentManager grows its sparse array for large entity ids")
{
	// A small initial reserve forces the sparse array to grow on demand:
	wiECS::ComponentManager<int> manager(8);
	const wiECS::Entity far_away = 500;
	CHECK_FALSE(manager.Contains(far_away));
	manager.Create(far_away) = 7;
	CHECK(manager.Contains(far_away));
	CHECK(*manager.GetComponent(far_away) == 7);
	CHECK(manager.GetIndex(far_away) == 0);
}

TEST_CASE("ComponentManager serializes entities and components through wiArchive")
{
	wiECS::ComponentManager<TestComponent> writer(1024);
	writer.Create(7).value = 70;
	writer.Create(9).value = 90;

	wiArchive archive;
	wiECS::EntitySerializer seri;
	writer.Serialize(archive, seri);

	archive.SetReadModeAndResetPos(true);

	wiECS::ComponentManager<TestComponent> reader(1024);
	wiECS::EntitySerializer readSeri;
	reader.Serialize(archive, readSeri);

	REQUIRE(reader.GetCount() == 2);
	REQUIRE(reader.GetEntitiesCount() == 2);

	// Entity handles may be remapped on load, but both components must come
	// back with their values and both handles must be valid:
	std::vector<int> values;
	for (size_t i = 0; i < reader.GetCount(); ++i)
	{
		const wiECS::Entity entity = reader.GetEntity(i);
		CHECK(entity != wiECS::INVALID_ENTITY);
		values.push_back(reader.GetComponent(entity)->value);
	}
	std::sort(values.begin(), values.end());
	CHECK(values[0] == 70);
	CHECK(values[1] == 90);
}
