#include "AshenSimulationSubsystem.h"

#include "AshenEntityActor.h"
#include "AshenResourceActor.h"
#include "ashen/core/Catalog.hpp"
#include "ashen/core/Simulation.hpp"

#include "Engine/World.h"
#include "Stats/Stats.h"

#include <cmath>
#include <utility>
#include <vector>

DEFINE_LOG_CATEGORY_STATIC(LogAshenSimulation, Log, All);

class FAshenSimulationRuntime final
{
public:
    ashen::core::Simulation Simulation{};
};

namespace
{
constexpr float FixedStepSeconds = 1.0f / static_cast<float>(ashen::core::kTicksPerSecond);
constexpr int32 MaxCatchUpSteps = 8;

bool ContainsInvalidId(const TArray<int32>& EntityIds)
{
    return EntityIds.ContainsByPredicate([](const int32 Id)
    {
        return Id <= 0;
    });
}

EAshenEntityArchetype ToArchetype(const ashen::core::EntityType Type)
{
    using ashen::core::EntityType;
    switch (Type)
    {
    case EntityType::Worker:
        return EAshenEntityArchetype::Worker;
    case EntityType::Vanguard:
        return EAshenEntityArchetype::Vanguard;
    case EntityType::Skirmisher:
        return EAshenEntityArchetype::Skirmisher;
    case EntityType::Command:
        return EAshenEntityArchetype::Command;
    case EntityType::Barracks:
        return EAshenEntityArchetype::Barracks;
    case EntityType::Turret:
        return EAshenEntityArchetype::Turret;
    }
    return EAshenEntityArchetype::Worker;
}

ashen::core::Vec2 ToCorePosition(const FVector& WorldPosition)
{
    return {
        static_cast<int32>(std::lround(WorldPosition.X / UAshenSimulationSubsystem::RenderScale *
                                      ashen::core::kWorldScale)),
        static_cast<int32>(std::lround(WorldPosition.Y / UAshenSimulationSubsystem::RenderScale *
                                      ashen::core::kWorldScale)),
    };
}
}

void UAshenSimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Accumulator = 0.0f;
}

void UAshenSimulationSubsystem::Deinitialize()
{
    delete Runtime;
    Runtime = nullptr;
    EntityActors.Reset();
    ResourceActors.Reset();
    Super::Deinitialize();
}

void UAshenSimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (InWorld.IsGameWorld())
    {
        StartMatch();
    }
}

void UAshenSimulationSubsystem::Tick(const float DeltaTime)
{
    if (Runtime == nullptr || !bGameplayEnabled)
    {
        return;
    }

    Accumulator = FMath::Min(Accumulator + DeltaTime, FixedStepSeconds * MaxCatchUpSteps);
    int32 Steps = 0;
    while (Accumulator >= FixedStepSeconds && Steps < MaxCatchUpSteps)
    {
        Runtime->Simulation.step();
        UpdateEnemyCommander();
        Accumulator -= FixedStepSeconds;
        ++Steps;
    }

    if (Steps > 0)
    {
        SyncWorldActors();
    }
}

TStatId UAshenSimulationSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAshenSimulationSubsystem, STATGROUP_Tickables);
}

bool UAshenSimulationSubsystem::IssueMove(const TArray<int32>& EntityIds, const FVector& WorldTarget)
{
    if (Runtime == nullptr || EntityIds.IsEmpty() || ContainsInvalidId(EntityIds))
    {
        return false;
    }

    ashen::core::Command Command{};
    Command.player = ashen::core::PlayerId::One;
    Command.type = ashen::core::CommandType::Move;
    Command.target = ToCorePosition(WorldTarget);
    Command.entities.reserve(static_cast<size_t>(EntityIds.Num()));
    for (const int32 Id : EntityIds)
    {
        Command.entities.push_back(ashen::core::EntityId{static_cast<uint32>(Id)});
    }
    return Runtime->Simulation.execute_now(std::move(Command)).ok;
}

bool UAshenSimulationSubsystem::IssueAttack(const TArray<int32>& EntityIds, const int32 TargetEntityId)
{
    if (Runtime == nullptr || EntityIds.IsEmpty() || ContainsInvalidId(EntityIds) || TargetEntityId <= 0)
    {
        return false;
    }

    ashen::core::Command Command{};
    Command.player = ashen::core::PlayerId::One;
    Command.type = ashen::core::CommandType::Attack;
    Command.target_entity = ashen::core::EntityId{static_cast<uint32>(TargetEntityId)};
    Command.entities.reserve(static_cast<size_t>(EntityIds.Num()));
    for (const int32 Id : EntityIds)
    {
        Command.entities.push_back(ashen::core::EntityId{static_cast<uint32>(Id)});
    }
    return Runtime->Simulation.execute_now(std::move(Command)).ok;
}

bool UAshenSimulationSubsystem::IssueGather(const TArray<int32>& EntityIds, const int32 ResourceId)
{
    if (Runtime == nullptr || EntityIds.IsEmpty() || ContainsInvalidId(EntityIds) || ResourceId <= 0)
    {
        return false;
    }

    ashen::core::Command Command{};
    Command.player = ashen::core::PlayerId::One;
    Command.type = ashen::core::CommandType::Gather;
    Command.resource = ashen::core::ResourceId{static_cast<uint32>(ResourceId)};
    Command.entities.reserve(static_cast<size_t>(EntityIds.Num()));
    for (const int32 Id : EntityIds)
    {
        Command.entities.push_back(ashen::core::EntityId{static_cast<uint32>(Id)});
    }
    return Runtime->Simulation.execute_now(std::move(Command)).ok;
}

bool UAshenSimulationSubsystem::IssueTrain(const int32 ProducerId, const bool bSecondaryUnit)
{
    if (Runtime == nullptr || ProducerId <= 0)
    {
        return false;
    }

    const auto* Producer = Runtime->Simulation.find_entity(
        ashen::core::EntityId{static_cast<uint32>(ProducerId)});
    if (Producer == nullptr || Producer->owner != ashen::core::PlayerId::One)
    {
        return false;
    }

    ashen::core::EntityType UnitType = ashen::core::EntityType::Worker;
    if (Producer->type == ashen::core::EntityType::Barracks)
    {
        UnitType = bSecondaryUnit ? ashen::core::EntityType::Skirmisher : ashen::core::EntityType::Vanguard;
    }

    ashen::core::Command Command{};
    Command.player = ashen::core::PlayerId::One;
    Command.type = ashen::core::CommandType::Train;
    Command.producer = ashen::core::EntityId{static_cast<uint32>(ProducerId)};
    Command.train_type = UnitType;
    return Runtime->Simulation.execute_now(std::move(Command)).ok;
}

FAshenPlayerView UAshenSimulationSubsystem::GetPlayerView(const int32 PlayerIndex) const
{
    FAshenPlayerView View{};
    if (Runtime == nullptr)
    {
        return View;
    }

    const auto Player = PlayerIndex == 1 ? ashen::core::PlayerId::Two : ashen::core::PlayerId::One;
    const auto& State = Runtime->Simulation.player(Player);
    View.Ore = State.ore;
    View.SupplyUsed = State.supply_used;
    View.SupplyCap = State.supply_cap;
    return View;
}

int64 UAshenSimulationSubsystem::GetSimulationTick() const
{
    return Runtime == nullptr ? 0 : static_cast<int64>(Runtime->Simulation.tick());
}

int32 UAshenSimulationSubsystem::GetEntityCount() const
{
    return Runtime == nullptr ? 0 : static_cast<int32>(Runtime->Simulation.entities().size());
}

EAshenEntityArchetype UAshenSimulationSubsystem::GetEntityArchetype(const int32 EntityId) const
{
    if (Runtime != nullptr && EntityId > 0)
    {
        if (const auto* Entity = Runtime->Simulation.find_entity(
                ashen::core::EntityId{static_cast<uint32>(EntityId)}))
        {
            return ToArchetype(Entity->type);
        }
    }
    return EAshenEntityArchetype::Worker;
}

void UAshenSimulationSubsystem::SetGameplayEnabled(const bool bEnabled)
{
    const bool bWasEnabled = bGameplayEnabled;
    bGameplayEnabled = bEnabled;
    Accumulator = 0.0f;
    if (bEnabled && !bWasEnabled && Runtime != nullptr && Runtime->Simulation.tick() == 0)
    {
        PrimeOpeningEconomy();
    }
}

void UAshenSimulationSubsystem::RestartMatch()
{
    for (const TPair<uint32, TWeakObjectPtr<AAshenEntityActor>>& Pair : EntityActors)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->Destroy();
        }
    }
    for (const TPair<uint32, TWeakObjectPtr<AAshenResourceActor>>& Pair : ResourceActors)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->Destroy();
        }
    }
    EntityActors.Reset();
    ResourceActors.Reset();
    StartMatch();
}

bool UAshenSimulationSubsystem::IsMatchOver() const
{
    return Runtime != nullptr && Runtime->Simulation.status() != ashen::core::MatchStatus::Playing;
}

bool UAshenSimulationSubsystem::DidLocalPlayerWin() const
{
    return Runtime != nullptr && Runtime->Simulation.winner().has_value() &&
           Runtime->Simulation.winner().value() == ashen::core::PlayerId::One;
}

void UAshenSimulationSubsystem::StartMatch()
{
    delete Runtime;
    Runtime = new FAshenSimulationRuntime();
    Accumulator = 0.0f;
    LastEnemyDecisionTick = -1;
    bGameplayEnabled = false;
    UE_LOG(LogAshenSimulation, Display,
           TEXT("Match started: %d entities, %d resource fields, %d fixed ticks/sec"),
           static_cast<int32>(Runtime->Simulation.entities().size()),
           static_cast<int32>(Runtime->Simulation.resources().size()), ashen::core::kTicksPerSecond);
    SyncWorldActors();
}

void UAshenSimulationSubsystem::PrimeOpeningEconomy()
{
    if (Runtime == nullptr)
    {
        return;
    }

    using namespace ashen::core;
    std::vector<EntityId> Workers;
    for (const Entity& EntityState : Runtime->Simulation.entities())
    {
        if (EntityState.owner == PlayerId::One && EntityState.type == EntityType::Worker)
        {
            Workers.push_back(EntityState.id);
        }
    }

    const ResourceNode* ChosenResource = nullptr;
    for (const ResourceNode& Resource : Runtime->Simulation.resources())
    {
        if (ChosenResource == nullptr || Resource.position.x < ChosenResource->position.x)
        {
            ChosenResource = &Resource;
        }
    }
    if (Workers.empty() || ChosenResource == nullptr)
    {
        return;
    }

    Command Gather{};
    Gather.player = PlayerId::One;
    Gather.type = CommandType::Gather;
    Gather.entities = std::move(Workers);
    Gather.resource = ChosenResource->id;
    static_cast<void>(Runtime->Simulation.execute_now(std::move(Gather)));
    UE_LOG(LogAshenSimulation, Display, TEXT("Opening workers assigned to cursed iron"));
}

void UAshenSimulationSubsystem::UpdateEnemyCommander()
{
    if (Runtime == nullptr || Runtime->Simulation.status() != ashen::core::MatchStatus::Playing)
    {
        return;
    }

    const int64 Tick = static_cast<int64>(Runtime->Simulation.tick());
    if (Tick == LastEnemyDecisionTick)
    {
        return;
    }
    LastEnemyDecisionTick = Tick;

    using namespace ashen::core;
    std::vector<EntityId> Workers;
    std::vector<EntityId> Army;
    const Entity* CommandBuilding = nullptr;
    const Entity* Barracks = nullptr;
    const Entity* HumanCommand = nullptr;
    for (const Entity& EntityState : Runtime->Simulation.entities())
    {
        if (EntityState.owner == PlayerId::Two)
        {
            if (EntityState.type == EntityType::Worker)
            {
                Workers.push_back(EntityState.id);
            }
            else if (EntityState.type == EntityType::Vanguard || EntityState.type == EntityType::Skirmisher)
            {
                Army.push_back(EntityState.id);
            }
            else if (EntityState.type == EntityType::Command)
            {
                CommandBuilding = &EntityState;
            }
            else if (EntityState.type == EntityType::Barracks)
            {
                Barracks = &EntityState;
            }
        }
        else if (EntityState.type == EntityType::Command)
        {
            HumanCommand = &EntityState;
        }
    }

    if ((Tick == 1 || Tick % 420 == 0) && !Workers.empty())
    {
        const ResourceNode* ChosenResource = nullptr;
        for (const ResourceNode& Resource : Runtime->Simulation.resources())
        {
            if (ChosenResource == nullptr || Resource.position.x > ChosenResource->position.x)
            {
                ChosenResource = &Resource;
            }
        }
        if (ChosenResource != nullptr)
        {
            Command Gather{};
            Gather.player = PlayerId::Two;
            Gather.type = CommandType::Gather;
            Gather.entities = Workers;
            Gather.resource = ChosenResource->id;
            static_cast<void>(Runtime->Simulation.execute_now(std::move(Gather)));
        }
    }

    if (Tick > 0 && Tick % 160 == 0)
    {
        if (CommandBuilding != nullptr && Workers.size() < 5 && CommandBuilding->production_queue.size() < 2)
        {
            Command TrainWorker{};
            TrainWorker.player = PlayerId::Two;
            TrainWorker.type = CommandType::Train;
            TrainWorker.producer = CommandBuilding->id;
            TrainWorker.train_type = EntityType::Worker;
            static_cast<void>(Runtime->Simulation.execute_now(std::move(TrainWorker)));
        }

        if (Barracks != nullptr && Barracks->production_queue.size() < 2)
        {
            Command TrainArmy{};
            TrainArmy.player = PlayerId::Two;
            TrainArmy.type = CommandType::Train;
            TrainArmy.producer = Barracks->id;
            TrainArmy.train_type = (Tick / 160) % 3 == 0 ? EntityType::Skirmisher : EntityType::Vanguard;
            static_cast<void>(Runtime->Simulation.execute_now(std::move(TrainArmy)));
        }
    }

    if (Tick >= 1'600 && Tick % 600 == 400 && Army.size() >= 4 && HumanCommand != nullptr)
    {
        Command Assault{};
        Assault.player = PlayerId::Two;
        Assault.type = CommandType::Attack;
        Assault.entities = std::move(Army);
        Assault.target_entity = HumanCommand->id;
        static_cast<void>(Runtime->Simulation.execute_now(std::move(Assault)));
        UE_LOG(LogAshenSimulation, Display, TEXT("The Hollow Choir launches an assault at tick %lld"), Tick);
    }
}

void UAshenSimulationSubsystem::SyncWorldActors()
{
    UWorld* World = GetWorld();
    if (Runtime == nullptr || World == nullptr)
    {
        return;
    }

    TSet<uint32> LiveEntities;
    for (const auto& Entity : Runtime->Simulation.entities())
    {
        LiveEntities.Add(Entity.id.value);
        AAshenEntityActor* Actor = EntityActors.FindRef(Entity.id.value).Get();
        if (Actor == nullptr)
        {
            Actor = World->SpawnActor<AAshenEntityActor>();
            if (Actor == nullptr)
            {
                continue;
            }
            EntityActors.Add(Entity.id.value, Actor);
            Actor->InitializeEntity(static_cast<int32>(Entity.id.value), static_cast<uint8>(Entity.owner),
                                    ToArchetype(Entity.type),
                                    static_cast<float>(Entity.radius) / ashen::core::kWorldScale * RenderScale);
        }

        const float HealthFraction = Entity.max_hit_points > 0
                                         ? static_cast<float>(Entity.hit_points) / Entity.max_hit_points
                                         : 0.0f;
        Actor->ApplySimulationState(ToWorldPosition(Entity.position.x, Entity.position.y), HealthFraction);
    }

    TArray<uint32> EntityKeys;
    EntityActors.GetKeys(EntityKeys);
    for (const uint32 Id : EntityKeys)
    {
        if (!LiveEntities.Contains(Id))
        {
            if (AAshenEntityActor* Actor = EntityActors.FindRef(Id).Get())
            {
                Actor->Destroy();
            }
            EntityActors.Remove(Id);
        }
    }

    for (const auto& Resource : Runtime->Simulation.resources())
    {
        AAshenResourceActor* Actor = ResourceActors.FindRef(Resource.id.value).Get();
        if (Actor == nullptr)
        {
            Actor = World->SpawnActor<AAshenResourceActor>();
            if (Actor == nullptr)
            {
                continue;
            }
            ResourceActors.Add(Resource.id.value, Actor);
            Actor->InitializeResource(static_cast<int32>(Resource.id.value),
                                      static_cast<float>(Resource.radius) / ashen::core::kWorldScale * RenderScale);
        }
        Actor->ApplySimulationState(ToWorldPosition(Resource.position.x, Resource.position.y));
    }
}

FVector UAshenSimulationSubsystem::ToWorldPosition(const int32 CoreX, const int32 CoreY) const
{
    return {
        static_cast<float>(CoreX) / ashen::core::kWorldScale * RenderScale,
        static_cast<float>(CoreY) / ashen::core::kWorldScale * RenderScale,
        0.0f,
    };
}
