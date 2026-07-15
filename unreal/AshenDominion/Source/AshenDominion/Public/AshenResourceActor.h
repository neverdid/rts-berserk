#pragma once

#include "GameFramework/Actor.h"
#include "AshenResourceActor.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ASHENDOMINION_API AAshenResourceActor final : public AActor
{
    GENERATED_BODY()

public:
    AAshenResourceActor();
    void InitializeResource(int32 InResourceId, float Radius);
    void ApplySimulationState(const FVector& GroundPosition);

    UFUNCTION(BlueprintPure, Category = "Ashen")
    int32 GetResourceId() const noexcept { return ResourceId; }

private:
    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UStaticMeshComponent> ResourceMesh;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> CrystalMeshes;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UPointLightComponent> ResourceLight;

    int32 ResourceId = 0;
};
