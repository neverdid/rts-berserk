#if WITH_DEV_AUTOMATION_TESTS

#include "AshenArena.h"
#include "AshenEnvironmentKit.h"
#include "AshenWorldLayout.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UObjectGlobals.h"
#include "ashen/core/Catalog.hpp"
#include "ashen/core/Simulation.hpp"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAshenCoreBootsInUnrealTest, "Ashen.Core.BootsInUnreal",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAshenCoreBootsInUnrealTest::RunTest(const FString &Parameters)
{
    static_cast<void>(Parameters);

    ashen::core::Simulation First{};
    ashen::core::Simulation Second{};

    TestEqual(TEXT("Default match seeds ten entities"), static_cast<int32>(First.entities().size()), 10);
    TestEqual(TEXT("Default match seeds seven resource fields"), static_cast<int32>(First.resources().size()), 7);
    TestEqual(TEXT("Default match seeds two contestable relics"), static_cast<int32>(First.control_points().size()), 2);

    First.run(240);
    Second.run(240);

    TestEqual(TEXT("Simulation advances at a deterministic tick"), static_cast<int64>(First.tick()), int64{240});
    TestTrue(TEXT("Equivalent matches produce the same state hash"), First.state_hash() == Second.state_hash());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAshenCoreNavigationAndOrdersTest, "Ashen.Core.NavigationAndOrders",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAshenCoreNavigationAndOrdersTest::RunTest(const FString &Parameters)
{
    static_cast<void>(Parameters);
    using namespace ashen::core;

    SimulationConfig Config{};
    Config.seed_starting_forces = false;
    Simulation First{Config};
    Simulation Second{Config};
    const EntityId FirstUnit = First.spawn_entity(PlayerId::One, EntityType::Worker, world(1'000, 700));
    const EntityId SecondUnit = Second.spawn_entity(PlayerId::One, EntityType::Worker, world(1'000, 700));

    const Command CrossRiver{
        .player = PlayerId::One, .type = CommandType::AttackMove, .entities = {FirstUnit}, .target = world(1'400, 700)};
    Command MirroredCrossRiver = CrossRiver;
    MirroredCrossRiver.entities = {SecondUnit};
    TestTrue(TEXT("Attack-move accepts a destination across the river"), First.execute_now(CrossRiver).ok);
    TestTrue(TEXT("Equivalent attack-move is accepted"), Second.execute_now(MirroredCrossRiver).ok);

    const Command QueuedMove{.player = PlayerId::One,
                             .type = CommandType::Move,
                             .entities = {FirstUnit},
                             .target = world(1'480, 700),
                             .queue = true};
    Command MirroredQueuedMove = QueuedMove;
    MirroredQueuedMove.entities = {SecondUnit};
    TestTrue(TEXT("Shift-style queued move is accepted"), First.execute_now(QueuedMove).ok);
    TestTrue(TEXT("Equivalent queued move is accepted"), Second.execute_now(MirroredQueuedMove).ok);

    First.run(600);
    Second.run(600);
    TestTrue(TEXT("Unit completes both orders"),
             First.find_entity(FirstUnit) != nullptr && First.find_entity(FirstUnit)->position == world(1'480, 700));
    TestTrue(TEXT("Navigation and queued orders remain deterministic"), First.state_hash() == Second.state_hash());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAshenWorldVisualFoundationTest, "Ashen.World.VisualFoundation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAshenWorldVisualFoundationTest::RunTest(const FString &Parameters)
{
    static_cast<void>(Parameters);

    AAshenArena *Arena = GetMutableDefault<AAshenArena>();
    TestNotNull(TEXT("Arena class default object is available"), Arena);
    if (Arena == nullptr)
    {
        return false;
    }

    const UProceduralMeshComponent *Terrain =
        Cast<UProceduralMeshComponent>(Arena->GetDefaultSubobjectByName(TEXT("TerrainGeometry")));
    TestNotNull(TEXT("Arena owns its sculpted terrain component"), Terrain);
    TestTrue(TEXT("Visual terrain cannot intercept deterministic RTS input"),
             Terrain != nullptr && Terrain->GetCollisionEnabled() == ECollisionEnabled::NoCollision);

    const UStaticMeshComponent *InteractionGround =
        Cast<UStaticMeshComponent>(Arena->GetDefaultSubobjectByName(TEXT("Ground")));
    TestNotNull(TEXT("Arena retains an invisible interaction plane"), InteractionGround);
    TestTrue(TEXT("Interaction plane blocks world traces"),
             InteractionGround != nullptr &&
                 InteractionGround->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
    TestFalse(TEXT("Interaction plane never renders over authored terrain"),
              InteractionGround != nullptr && InteractionGround->IsVisible());

    const UInstancedStaticMeshComponent *Mountain =
        Cast<UInstancedStaticMeshComponent>(Arena->GetDefaultSubobjectByName(TEXT("MountainRocks")));
    const UInstancedStaticMeshComponent *Mines =
        Cast<UInstancedStaticMeshComponent>(Arena->GetDefaultSubobjectByName(TEXT("MineMouths")));
    const UInstancedStaticMeshComponent *Gravewood =
        Cast<UInstancedStaticMeshComponent>(Arena->GetDefaultSubobjectByName(TEXT("ForestRoots")));
    TestTrue(TEXT("Northwest massif owns a substantial rock silhouette"),
             Mountain != nullptr && Mountain->GetInstanceCount() >= 24);
    TestTrue(TEXT("Production art never supplies deterministic collision"),
             Mountain != nullptr && Mountain->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    TestEqual(TEXT("The concealed route owns two mine entrances"), Mines != nullptr ? Mines->GetInstanceCount() : 0, 2);
    TestTrue(TEXT("Gravewood owns a dedicated root layer"), Gravewood != nullptr && Gravewood->GetInstanceCount() > 0);
    TestNull(TEXT("Legacy perimeter monoliths stay removed"),
             Arena->GetDefaultSubobjectByName(TEXT("BoundaryMonoliths")));
    TestEqual(TEXT("Expanded battlefield width remains authoritative"), Ashen::WorldLayout::Width, 4'800.0f);
    TestEqual(TEXT("Expanded battlefield height remains authoritative"), Ashen::WorldLayout::Height, 2'800.0f);

    const UMaterialInterface *Surface =
        LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_VowfallSurface.M_VowfallSurface"));
    const UMaterialInterface *Water =
        LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/M_VowfallWater.M_VowfallWater"));
    TestNotNull(TEXT("Painterly surface master material is available to the project"), Surface);
    TestNotNull(TEXT("Water master material is available to the project"), Water);

    if (Surface != nullptr)
    {
        TArray<FMaterialParameterInfo> TextureParameters;
        TArray<FGuid> TextureIds;
        Surface->GetAllTextureParameterInfo(TextureParameters, TextureIds);
        const auto HasTextureParameter = [&TextureParameters](const FName Name) {
            return TextureParameters.ContainsByPredicate(
                [Name](const FMaterialParameterInfo &Parameter) { return Parameter.Name == Name; });
        };
        TestTrue(TEXT("Surface material accepts production albedo textures"),
                 HasTextureParameter(TEXT("AlbedoTexture")));
        TestTrue(TEXT("Surface material accepts production normal textures"),
                 HasTextureParameter(TEXT("NormalTexture")));
        TestTrue(TEXT("Surface material accepts packed AO and roughness textures"),
                 HasTextureParameter(TEXT("PackedTexture")));

        TArray<FMaterialParameterInfo> ScalarParameters;
        TArray<FGuid> ScalarIds;
        Surface->GetAllScalarParameterInfo(ScalarParameters, ScalarIds);
        const auto HasScalarParameter = [&ScalarParameters](const FName Name) {
            return ScalarParameters.ContainsByPredicate(
                [Name](const FMaterialParameterInfo &Parameter) { return Parameter.Name == Name; });
        };
        TestTrue(TEXT("Texture blending remains an explicit material control"),
                 HasScalarParameter(TEXT("TextureBlend")));
        TestTrue(TEXT("Texture tiling remains an explicit material control"),
                 HasScalarParameter(TEXT("TextureTiling")));
    }

    const UAshenEnvironmentKitSettings *KitSettings = GetDefault<UAshenEnvironmentKitSettings>();
    TestNotNull(TEXT("Environment-kit settings are registered"), KitSettings);
    TestTrue(TEXT("Licensed source content remains under the external boundary"),
             KitSettings != nullptr && KitSettings->ProductionContentRoot.StartsWith(TEXT("/Game/External/")));

    const TConstArrayView<Ashen::EnvironmentKit::FMeshSpec> MeshSpecs = Ashen::EnvironmentKit::MeshSpecs();
    TestEqual(TEXT("Every visual proxy category owns a semantic production slot"), MeshSpecs.Num(),
              static_cast<int32>(EAshenEnvironmentMeshSlot::Count));
    TSet<uint8> MeshSlots;
    UStaticMesh *FallbackCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    for (const Ashen::EnvironmentKit::FMeshSpec &Spec : MeshSpecs)
    {
        MeshSlots.Add(static_cast<uint8>(Spec.Slot));
        TestTrue(FString::Printf(TEXT("%s has a canonical external path"), Spec.DisplayName),
                 Ashen::EnvironmentKit::ObjectPath(Spec).StartsWith(TEXT("/Game/External/")));
        TestTrue(FString::Printf(TEXT("%s has a source-controlled Vowfall path"), Spec.DisplayName),
                 Ashen::EnvironmentKit::SourceObjectPath(Spec).StartsWith(
                     TEXT("/Game/Art/Environment/VowfallKit/")));
        TestNotNull(FString::Printf(TEXT("%s has an original Vowfall fallback mesh"), Spec.DisplayName),
                    Ashen::EnvironmentKit::FindSourceMesh(Spec.Slot));
        TestNotNull(FString::Printf(TEXT("%s retains a source-safe fallback"), Spec.DisplayName),
                    Ashen::EnvironmentKit::ResolveMesh(Spec.Slot, FallbackCube));
    }
    TestEqual(TEXT("Environment mesh slots are unique"), MeshSlots.Num(), MeshSpecs.Num());

    const TConstArrayView<Ashen::EnvironmentKit::FSurfaceSpec> SurfaceSpecs =
        Ashen::EnvironmentKit::SurfaceSpecs();
    TestEqual(TEXT("Every textured surface owns a canonical production slot"), SurfaceSpecs.Num(),
              static_cast<int32>(EAshenEnvironmentSurface::Count) - 1);
    TSet<uint8> SurfaceSlots;
    for (const Ashen::EnvironmentKit::FSurfaceSpec &Spec : SurfaceSpecs)
    {
        SurfaceSlots.Add(static_cast<uint8>(Spec.Slot));
        TestTrue(FString::Printf(TEXT("%s has a canonical albedo path"), Spec.DisplayName),
                 Ashen::EnvironmentKit::TextureObjectPath(Spec, TEXT("_BC")).StartsWith(TEXT("/Game/External/")));
    }
    TestEqual(TEXT("Environment surface slots are unique"), SurfaceSlots.Num(), SurfaceSpecs.Num());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAshenCoreCompetitiveSliceInUnrealTest, "Ashen.Core.CompetitiveVerticalSlice",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAshenCoreCompetitiveSliceInUnrealTest::RunTest(const FString &Parameters)
{
    static_cast<void>(Parameters);
    using namespace ashen::core;

    SimulationConfig Config{};
    Config.seed_starting_forces = false;
    Config.navigation_obstacles.clear();
    Simulation Match{Config};
    const EntityId Keep = Match.spawn_entity(PlayerId::One, EntityType::Command, world(200, 400));
    const EntityId Worker = Match.spawn_entity(PlayerId::One, EntityType::Worker, world(350, 400));
    const ResourceId Iron = Match.add_resource(world(350, 270), 1'200);

    Command Build{};
    Build.player = PlayerId::One;
    Build.type = CommandType::Build;
    Build.entities = {Worker};
    Build.target = world(475, 400);
    Build.building_type = EntityType::Barracks;
    TestTrue(TEXT("Worker accepts a valid barracks site"), Match.execute_now(Build).ok);
    Match.run(380);

    const Entity *Barracks = nullptr;
    for (const Entity &Candidate : Match.entities())
    {
        if (Candidate.owner == PlayerId::One && Candidate.type == EntityType::Barracks)
        {
            Barracks = &Candidate;
            break;
        }
    }
    TestNotNull(TEXT("Construction creates a barracks"), Barracks);
    TestTrue(TEXT("Worker completes the barracks"), Barracks != nullptr && !Barracks->under_construction);

    Command Gather{};
    Gather.player = PlayerId::One;
    Gather.type = CommandType::Gather;
    Gather.entities = {Worker};
    Gather.resource = Iron;
    TestTrue(TEXT("Builder returns to the opening economy"), Match.execute_now(Gather).ok);
    Match.run(1'600);

    Command Research{};
    Research.player = PlayerId::One;
    Research.type = CommandType::Research;
    Research.producer = Keep;
    Research.research = ResearchId::TierTwo;
    TestTrue(TEXT("Command keep begins the Black-Iron Age"), Match.execute_now(Research).ok);
    Match.run(research_definition(ResearchId::TierTwo).research_ticks);
    TestTrue(TEXT("Tier-two doctrine completes"), Match.has_research(PlayerId::One, ResearchId::TierTwo));

    const ControlPointId Relic = Match.add_control_point(world(700, 400));
    static_cast<void>(Relic);
    static_cast<void>(Match.spawn_entity(PlayerId::One, EntityType::Vanguard, world(700, 400)));
    Match.run(160);
    TestTrue(TEXT("A lone war band captures an uncontested relic"),
             !Match.control_points().empty() && Match.control_points().back().owner == PlayerId::One);

    Command Power{};
    Power.player = PlayerId::One;
    Power.type = CommandType::ActivatePower;
    TestTrue(TEXT("Faction power activates after sufficient economy"), Match.execute_now(Power).ok);
    TestTrue(TEXT("Faction power begins its cooldown"), Match.player(PlayerId::One).power_cooldown_ticks > 0);

    const EntityId HiddenEnemy = Match.spawn_entity(PlayerId::Two, EntityType::Vanguard, world(1'650, 930));
    TestFalse(TEXT("A distant enemy is concealed by fog"),
              Match.is_entity_visible_to(*Match.find_entity(HiddenEnemy), PlayerId::One));
    static_cast<void>(Match.spawn_entity(PlayerId::One, EntityType::Worker, world(1'520, 930)));
    TestTrue(TEXT("A scout reveals the distant enemy"),
             Match.is_entity_visible_to(*Match.find_entity(HiddenEnemy), PlayerId::One));
    return true;
}

#endif
