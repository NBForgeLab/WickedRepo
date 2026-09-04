# Wicked Engine — Unit Tests

Headless CPU unit tests for the engine. They require **no GPU, no window and
no human observation**: the binary runs anywhere, finishes in under a second,
and its process exit code is the CI verdict (non-zero = failure).

## Framework

[doctest](https://github.com/doctest/doctest) **v2.4.12**, vendored under
`third_party/doctest/` (pinned, MIT licensed, see `LICENSE.txt` there). It is
the lightest fully-featured C++ test framework available and keeps build times
low — engine unit tests recompile in seconds, not minutes.

## The GameGuru shipping configuration (GGREDUCED)

GameGuru MAX links this engine built with `GGREDUCED;GGREDUCEDEDITOR`
(the `ReleaseForGG` configuration). The unit test target therefore compiles
the engine sources with those **same defines** and `ggcontract_tests.cpp`
pins the engine-side API the GameGuru integration layer relies on (for
example `wiJobSystem::WaitSleep`, a GameGuru fork addition). A compile or
test failure here means the engine that ships inside GameGuru MAX changed
behavior.

## Layout

```
Tests/UnitTests/
├── main.cpp                 doctest entry point + one-time engine init
│                            (wiJobSystem::Initialize, wiNetwork::Initialize)
├── TestStubs.h/.cpp         Faithful narrow stand-ins for wiBackLog and
│                            wiHelper (real file I/O, stdout logging) so the
│                            test binary stays free of graphics dependencies
├── math_tests.cpp           wiMath: lerp/clamp/smoothstep, distances,
│                            closest-point queries, triangle area, barycentric,
│                            Halton, quaternion conversion, ray-triangle,
│                            normal/color packing
├── intersect_tests.cpp      AABB/SPHERE/CAPSULE/RAY/Frustum/Hitbox2D
│                            semantics, AABB serialization and transform,
│                            including documented boundary behavior
├── archive_tests.cpp        wiArchive round-trips, version header contract,
│                            file persistence, size tracking, mode cycling,
│                            wiECS entity serialization
├── containers_tests.cpp     ThreadSafeRingBuffer FIFO/capacity/wraparound +
│                            concurrent producer/consumer consistency
├── jobsystem_tests.cpp      Group math, dispatch partitioning, wiJobArgs
│                            invariants, shared group memory, wait semantics,
│                            queue-capacity stress, nested and parallel
│                            contexts
├── event_tests.cpp          Subscribe/FireEvent, once-only subscribers,
│                            handle-lifetime unsubscription
├── random_tests.cpp         Range contracts and statistical sanity
├── rectpacker_tests.cpp     Rectangle placement validity, no-overlap
│                            invariants, oversized input rejection
├── network_tests.cpp        Real UDP loopback traffic, receive timeouts,
│                            socket lifecycle (Windows backend)
├── timer_tests.cpp          Elapsed time and record() reset
├── fademanager_tests.cpp    Fade-in/hold/fade-out state machine, opacity
│                            progression and onFade callback timing
├── version_tests.cpp        Version string and packed version consistency
├── ggcontract_tests.cpp     GameGuru integration contract (GGREDUCED
│                            configuration, WaitSleep semantics)
├── performance_tests.cpp    CPU time budgets per hot module (jobsystem,
│                            ring buffer, archive 4MB round-trip, ray
│                            picking, frustum culls, ECS cycles, packer,
│                            screen fade) — catch order-of-magnitude
│                            regressions, print measured drift
├── leak_tests.cpp           CRT heap leak detection (Debug only) per
│                            module: subscribe/unsubscribe, entity
│                            cycles, archive save/load, job bursts,
│                            socket create/destroy, screen-fade restarts
├── startupargs_tests.cpp    Command line argument registration and lookup
├── spinlock_tests.cpp       try/lock/unlock semantics and mutual exclusion
│                            under thread contention
├── ecs_tests.cpp            ComponentManager create/find/remove/sorted
│                            removal, merge/copy, sparse growth, archive
│                            serialization round-trip, global handle
│                            recycling and concurrent entity creation
│
│  (math_tests additionally covers clamped barycentrics and the agreement
│   of both ray-triangle variants; spinlock covers lock contention.)
```

## Bugs this suite has caught

* `wiNetwork::Destroy` was declared in `wiNetwork.h` but never implemented in
  `wiNetwork_Windows.cpp` — any code calling the official API could not link.
  Fixed (releases the socket handle through the internal state), with tests.
* `wiVersion::GetVersion` was likewise declared but never defined. Fixed
  (`major*100 + minor*10 + revision`, matching the upstream convention).
* `wiMath::getCubicHermiteSplinePos` used the wrong tangent coefficient
  (`t^3 - 2t^2 + 1` instead of `t^3 - 2t^2 + t`) on the Y and Z axes,
  displacing every spline from its start position. Fixed and pinned by
  endpoint tests.

The engine `.cpp` files under test are compiled **directly into the test
binary** (see `UnitTests.vcxproj` and `CMakeLists.txt`). This keeps the loop
tight: adding a test never triggers a full engine rebuild.

## Running the tests

Visual Studio (Windows):

1. Open `WickedEngine.sln`, build the `UnitTests` project, run it — or from a
   developer prompt:
   ```
   msbuild WickedEngine.sln /t:UnitTests /p:Configuration=Release /p:Platform=x64
   BUILD\x64\Release\UnitTests\UnitTests.exe
   ```

CMake (Windows and Linux):

```
cmake -S . -B build -A x64        # or your usual generator
cmake --build build --target UnitTests --config Release
cd build && ctest -C Release -R UnitTests
```

Useful doctest filters: `UnitTests.exe -tc="*Frustum*"`, `-s` (show successful
assertions), `--no-skip=false`. `-tc` supports wildcards.

## Performance and leak testing

`performance_tests.cpp` holds generous time budgets (order-of-magnitude
regression gates, not micro-benchmarks). Each run prints the measured time;
a drift toward the budget is the early warning long before failure. The
timed loops contain no assertions — results are checked once after the clock
stops.

`leak_tests.cpp` (Debug builds only) brackets steady-state operation cycles
between MSVC CRT heap checkpoints (`_CrtMemCheckpoint`/`_CrtMemDifference`)
and gates on the live-byte delta; the live-block count churns with debug
transients and is printed as information only. Run the leak layer with
`UnitTests.exe -tc="*Leak*"` and the performance layer with
`-tc="*Performance*"`.

## CI

`appveyor.yml` builds the solution (which includes `UnitTests`) and then
**runs the test binary as a `test_script` step**; a failing test fails the
build. CTest (`add_test` in `CMakeLists.txt`) is registered for local
`ctest` runs.

## Conventions for new tests

1. One file per engine module, named `<module>_tests.cpp`.
2. Test names describe behavior: `"Dispatch partitions jobs into groups with
   correct wiJobArgs invariants"`.
3. `CHECK` for non-fatal assertions, `REQUIRE` when the test cannot continue
   meaningfully after a failure.
4. Document deliberately characterized quirks in a comment at the assertion
   site (e.g. `RAY::intersects(SPHERE)` tests infinite-line distance).
5. If a new test needs a currently-unlinked engine module, add the module's
   `.cpp` to both `UnitTests.vcxproj` and `CMakeLists.txt`; if it drags the
   renderer in, extend `TestStubs` with a faithful narrow stand-in instead.
