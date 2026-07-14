#pragma once

#include "AshenTypes.h"
#include "GameFramework/Actor.h"
#include "AshenEntityActor.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
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
    UStaticMeshComponent* CreatePart(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
                                     const FRotator& Rotation, const FLinearColor& Color, float Roughness);
    void BuildHumanVisuals(float Diameter);
    void BuildMonsterVisuals(float Diameter);
    void UpdateHealthDisplay(float HealthFraction);
    bool IsBuilding() const noexcept;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UStaticMeshComponent> EntityMesh;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> DetailMeshes;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UStaticMeshComponent> SelectionMarker;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UStaticMeshComponent> HealthBack;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UStaticMeshComponent> HealthFill;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UPointLightComponent> FactionLight;

    int32 EntityId = 0;
    uint8 OwnerIndex = 0;
    EAshenEntityArchetype Archetype = EAshenEntityArchetype::Worker;
    float VisualHeight = 80.0f;
    float HealthBarWidth = 50.0f;
    float LastHealthFraction = 1.0f;
    float HealthLightIntensity = 380.0f;
    FVector LastGroundPosition = FVector::ZeroVector;
    bool bHasGroundPosition = false;
    bool bSelected = false;
};
