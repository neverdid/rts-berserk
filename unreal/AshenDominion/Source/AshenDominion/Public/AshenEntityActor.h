#pragma once

#include "AshenTypes.h"
#include "GameFramework/Actor.h"
#include "AshenEntityActor.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class ASHENDOMINION_API AAshenEntityActor final : public AActor
{
    GENERATED_BODY()

public:
    AAshenEntityActor();

    void InitializeEntity(int32 InEntityId, uint8 InOwnerIndex, EAshenEntityArchetype InArchetype, float Radius);
    void ApplySimulationState(const FVector& GroundPosition, float HealthFraction);
    void SetSelected(bool bInSelected);

    UFUNCTION(BlueprintPure, Category = "Ashen")
    int32 GetEntityId() const noexcept { return EntityId; }

    UFUNCTION(BlueprintPure, Category = "Ashen")
    uint8 GetOwnerIndex() const noexcept { return OwnerIndex; }

    UFUNCTION(BlueprintPure, Category = "Ashen")
    EAshenEntityArchetype GetArchetype() const noexcept { return Archetype; }

private:
    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UStaticMeshComponent> EntityMesh;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UStaticMeshComponent> SelectionMarker;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UPointLightComponent> FactionLight;

    int32 EntityId = 0;
    uint8 OwnerIndex = 0;
    EAshenEntityArchetype Archetype = EAshenEntityArchetype::Worker;
    float VisualHeight = 40.0f;
    float HealthLightIntensity = 650.0f;
    bool bSelected = false;
};
