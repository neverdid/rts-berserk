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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAshenCoreNavigationAndOrdersTest,
    "Ashen.Core.NavigationAndOrders",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAshenCoreNavigationAndOrdersTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    using namespace ashen::core;

    SimulationConfig Config{};
    Config.seed_starting_forces = false;
    Simulation First{Config};
    Simulation Second{Config};
    const EntityId FirstUnit = First.spawn_entity(PlayerId::One, EntityType::Worker, world(760, 100));
    const EntityId SecondUnit = Second.spawn_entity(PlayerId::One, EntityType::Worker, world(760, 100));

    const Command CrossRiver{.player = PlayerId::One,
                             .type = CommandType::AttackMove,
                             .entities = {FirstUnit},
                             .target = world(1'160, 100)};
    Command MirroredCrossRiver = CrossRiver;
    MirroredCrossRiver.entities = {SecondUnit};
    TestTrue(TEXT("Attack-move accepts a destination across the river"), First.execute_now(CrossRiver).ok);
    TestTrue(TEXT("Equivalent attack-move is accepted"), Second.execute_now(MirroredCrossRiver).ok);

    const Command QueuedMove{.player = PlayerId::One,
                             .type = CommandType::Move,
                             .entities = {FirstUnit},
                             .target = world(1'160, 220),
                             .queue = true};
    Command MirroredQueuedMove = QueuedMove;
    MirroredQueuedMove.entities = {SecondUnit};
    TestTrue(TEXT("Shift-style queued move is accepted"), First.execute_now(QueuedMove).ok);
    TestTrue(TEXT("Equivalent queued move is accepted"), Second.execute_now(MirroredQueuedMove).ok);

    First.run(600);
    Second.run(600);
    TestTrue(TEXT("Unit completes both orders"),
             First.find_entity(FirstUnit) != nullptr && First.find_entity(FirstUnit)->position == world(1'160, 220));
    TestTrue(TEXT("Navigation and queued orders remain deterministic"), First.state_hash() == Second.state_hash());
    return true;
}

#endif
