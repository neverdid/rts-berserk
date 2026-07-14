#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ashen/core/Simulation.hpp"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAshenCoreBootsInUnrealTest,
    "Ashen.Core.BootsInUnreal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAshenCoreBootsInUnrealTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);

    ashen::core::Simulation First{};
    ashen::core::Simulation Second{};

    TestEqual(TEXT("Default match seeds twelve entities"), static_cast<int32>(First.entities().size()), 12);
    TestEqual(TEXT("Default match seeds three resource fields"), static_cast<int32>(First.resources().size()), 3);

    First.run(240);
    Second.run(240);

    TestEqual(TEXT("Simulation advances at a deterministic tick"), static_cast<int64>(First.tick()), int64{240});
    TestTrue(TEXT("Equivalent matches produce the same state hash"), First.state_hash() == Second.state_hash());
    return true;
}

#endif
