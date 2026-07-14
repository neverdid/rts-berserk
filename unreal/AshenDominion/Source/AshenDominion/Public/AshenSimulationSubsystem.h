#pragma once

#include "AshenTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "AshenSimulationSubsystem.generated.h"

class AAshenEntityActor;
class AAshenResourceActor;
class FAshenSimulationRuntime;

UCLASS()
class ASHENDOMINION_API UAshenSimulationSubsystem final : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    static constexpr float RenderScale = 2.0f;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    UFUNCTION(BlueprintCallable, Category = "Ashen|Commands")
    bool IssueMove(const TArray<int32>& EntityIds, const FVector& WorldTarget);

    UFUNCTION(BlueprintCallable, Category = "Ashen|Commands")
    bool IssueAttack(const TArray<int32>& EntityIds, int32 TargetEntityId);

    UFUNCTION(BlueprintCallable, Category = "Ashen|Commands")
    bool IssueGather(const TArray<int32>& EntityIds, int32 ResourceId);

    UFUNCTION(BlueprintCallable, Category = "Ashen|Commands")
    bool IssueTrain(int32 ProducerId, bool bSecondaryUnit);

    UFUNCTION(BlueprintPure, Category = "Ashen|State")
    FAshenPlayerView GetPlayerView(int32 PlayerIndex) const;

    UFUNCTION(BlueprintPure, Category = "Ashen|State")
    int64 GetSimulationTick() const;

    UFUNCTION(BlueprintPure, Category = "Ashen|State")
    int32 GetEntityCount() const;

    UFUNCTION(BlueprintPure, Category = "Ashen|State")
    EAshenEntityArchetype GetEntityArchetype(int32 EntityId) const;

    UFUNCTION(BlueprintCallable, Category = "Ashen|State")
    void SetGameplayEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Ashen|State")
    void RestartMatch();

    UFUNCTION(BlueprintPure, Category = "Ashen|State")
    bool IsGameplayEnabled() const noexcept { return bGameplayEnabled; }

    UFUNCTION(BlueprintPure, Category = "Ashen|State")
    bool IsMatchOver() const;

    UFUNCTION(BlueprintPure, Category = "Ashen|State")
    bool DidLocalPlayerWin() const;

private:
    void StartMatch();
    void PrimeOpeningEconomy();
    void UpdateEnemyCommander();
    void SyncWorldActors();
    FVector ToWorldPosition(int32 CoreX, int32 CoreY) const;

    FAshenSimulationRuntime* Runtime = nullptr;
    float Accumulator = 0.0f;
    int64 LastEnemyDecisionTick = -1;
    bool bGameplayEnabled = false;
    TMap<uint32, TWeakObjectPtr<AAshenEntityActor>> EntityActors;
    TMap<uint32, TWeakObjectPtr<AAshenResourceActor>> ResourceActors;
};
