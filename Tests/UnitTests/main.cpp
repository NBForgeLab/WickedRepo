// UnitTests entry point.
// doctest provides the assertion framework; this main also initializes the
// engine CPU services exactly once (the same contract the engine startup
// fulfills) before any test runs. The process exit code is non-zero when any
// test case fails, which is what the CI and local gates rely on.
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include "wiJobSystem.h"
#include "wiNetwork.h"

int main(int argc, char** argv)
{
	doctest::Context context(argc, argv);

	// The job system spawns its worker threads on Initialize() and must be
	// initialized exactly once per process, like the engine itself does.
	wiJobSystem::Initialize();
	wiNetwork::Initialize();

	return context.run();
}
