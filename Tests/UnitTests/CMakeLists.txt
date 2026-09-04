# Headless CPU unit tests for the engine (no GPU, no window, no network
# access beyond a localhost UDP loopback). These exercise only the CPU-only
# modules of the engine; the interactive visual scenarios live in the
# sibling Tests target.

set(UNIT_TEST_SOURCES
	main.cpp
	TestStubs.cpp
	TestStubs.h
	math_tests.cpp
	intersect_tests.cpp
	archive_tests.cpp
	containers_tests.cpp
	jobsystem_tests.cpp
	event_tests.cpp
	random_tests.cpp
	rectpacker_tests.cpp
	network_tests.cpp
	timer_tests.cpp
	ecs_tests.cpp
	version_tests.cpp
	startupargs_tests.cpp
	spinlock_tests.cpp
	fademanager_tests.cpp
	ggcontract_tests.cpp
	performance_tests.cpp
	leak_tests.cpp
)

# CPU-only engine modules under test. Selected so the test binary never
# links a graphics backend.
set(ENGINE_SOURCES
	../../WickedEngine/wiArchive.cpp
	../../WickedEngine/wiECS.cpp
	../../WickedEngine/wiEvent.cpp
	../../WickedEngine/wiIntersect.cpp
	../../WickedEngine/wiJobSystem.cpp
	../../WickedEngine/wiMath.cpp
	../../WickedEngine/wiRandom.cpp
	../../WickedEngine/wiRectPacker.cpp
	../../WickedEngine/wiTimer.cpp
	../../WickedEngine/wiStartupArguments.cpp
	../../WickedEngine/wiVersion.cpp
	../../WickedEngine/wiFadeManager.cpp
)

if (WIN32)
	list(APPEND ENGINE_SOURCES ../../WickedEngine/wiNetwork_Windows.cpp)
else()
	list(APPEND ENGINE_SOURCES ../../WickedEngine/wiNetwork_Linux.cpp)
endif()

add_executable(UnitTests ${UNIT_TEST_SOURCES} ${ENGINE_SOURCES})

target_include_directories(UnitTests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../WickedEngine
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest
)

if (MSVC)
	target_compile_options(UnitTests PRIVATE /W3 /permissive- /wd5285)
	# The engine that ships inside GameGuru MAX (ReleaseForGG) is built with
	# these defines; the unit tests validate that exact configuration.
	target_compile_definitions(UnitTests PRIVATE _CRT_SECURE_NO_WARNINGS GGREDUCED GGREDUCEDEDITOR)
endif()

if (WIN32)
	target_link_libraries(UnitTests PRIVATE ws2_32)
else()
	find_package(Threads REQUIRED)
	target_link_libraries(UnitTests PRIVATE Threads::Threads)
endif()

set_target_properties(UnitTests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	FOLDER "Tests"
)

add_test(NAME UnitTests COMMAND UnitTests)
