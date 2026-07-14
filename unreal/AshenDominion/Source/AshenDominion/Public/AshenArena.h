#pragma once

#include "GameFramework/Actor.h"
#include "AshenArena.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class UPostProcessComponent;
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
    void BuildRiver();
    void BuildRoadsAndBridges();
    void BuildFortifications();
    void BuildVegetation();
    void BuildLandmarks();

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UStaticMeshComponent> Ground;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> GroundMottle;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> Roadbed;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> RoadStones;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TArray<TObjectPtr<UStaticMeshComponent>> WaterSegments;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> WaterRipples;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> RiverBanks;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> BridgeTimbers;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> BridgeIron;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> TreeTrunks;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> TreeCrowns;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> DeadBranches;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> GrassTufts;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> Rocks;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> HumanWalls;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> HumanTowers;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> HumanRoofs;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> MonsterMasses;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> MonsterSpikes;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> BonePalisade;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> BoundaryMonoliths;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|World")
    TObjectPtr<UInstancedStaticMeshComponent> RitualStones;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|Atmosphere")
    TObjectPtr<UDirectionalLightComponent> MoonLight;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|Atmosphere")
    TObjectPtr<USkyAtmosphereComponent> Atmosphere;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|Atmosphere")
    TObjectPtr<USkyLightComponent> SkyLight;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|Atmosphere")
    TObjectPtr<UExponentialHeightFogComponent> Fog;

    UPROPERTY(VisibleAnywhere, Category = "Ashen|Atmosphere")
    TObjectPtr<UPostProcessComponent> PostProcess;
};
