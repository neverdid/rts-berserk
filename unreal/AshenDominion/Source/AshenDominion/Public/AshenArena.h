#pragma once

#include "GameFramework/Actor.h"
#include "AshenArena.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class USceneComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMeshComponent;

UCLASS()
class ASHENDOMINION_API AAshenArena final : public AActor
{
    GENERATED_BODY()

public:
    AAshenArena();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UStaticMeshComponent> Ground;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UInstancedStaticMeshComponent> BoundaryMonoliths;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UInstancedStaticMeshComponent> RitualStones;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UDirectionalLightComponent> MoonLight;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<USkyAtmosphereComponent> Atmosphere;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<USkyLightComponent> SkyLight;

    UPROPERTY(VisibleAnywhere, Category = "Ashen")
    TObjectPtr<UExponentialHeightFogComponent> Fog;
};
