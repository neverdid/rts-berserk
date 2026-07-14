#include "AshenArena.h"

#include "AshenMaterials.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"

AAshenArena::AAshenArena()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
    Ground->SetupAttachment(SceneRoot);
    Ground->SetMobility(EComponentMobility::Static);
    Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Ground->SetCollisionResponseToAllChannels(ECR_Block);
    Ground->SetRelativeLocation({1'920.0f, 1'080.0f, 0.0f});
    Ground->SetRelativeScale3D({38.4f, 21.6f, 1.0f});
    if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
    {
        Ground->SetStaticMesh(Plane);
    }

    BoundaryMonoliths = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BoundaryMonoliths"));
    BoundaryMonoliths->SetupAttachment(SceneRoot);
    BoundaryMonoliths->SetMobility(EComponentMobility::Static);
    BoundaryMonoliths->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RitualStones = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RitualStones"));
    RitualStones->SetupAttachment(SceneRoot);
    RitualStones->SetMobility(EComponentMobility::Static);
    RitualStones->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
    {
        BoundaryMonoliths->SetStaticMesh(Cube);
    }
    if (UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
    {
        RitualStones->SetStaticMesh(Cylinder);
    }

    for (int32 Index = 0; Index < 16; ++Index)
    {
        const float X = 120.0f + Index * 240.0f;
        const float HeightScale = 1.15f + static_cast<float>((Index * 7) % 5) * 0.16f;
        const FRotator Rotation(0.0f, static_cast<float>((Index * 23) % 90), 0.0f);
        BoundaryMonoliths->AddInstance(FTransform(Rotation, {X, 55.0f, HeightScale * 50.0f},
                                                  {0.48f, 0.72f, HeightScale}));
        BoundaryMonoliths->AddInstance(FTransform(Rotation, {X, 2'105.0f, HeightScale * 50.0f},
                                                  {0.48f, 0.72f, HeightScale}));
    }
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float Y = 240.0f + Index * 240.0f;
        const float HeightScale = 1.2f + static_cast<float>((Index * 3) % 4) * 0.2f;
        BoundaryMonoliths->AddInstance(FTransform({0.0f, static_cast<float>(Index * 11), 0.0f},
                                                  {55.0f, Y, HeightScale * 50.0f},
                                                  {0.72f, 0.48f, HeightScale}));
        BoundaryMonoliths->AddInstance(FTransform({0.0f, static_cast<float>(Index * 11), 0.0f},
                                                  {3'785.0f, Y, HeightScale * 50.0f},
                                                  {0.72f, 0.48f, HeightScale}));
    }

    for (int32 Index = 0; Index < 12; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / 12.0f;
        const FVector Position(1'920.0f + FMath::Cos(Angle) * 310.0f,
                               1'080.0f + FMath::Sin(Angle) * 220.0f, 38.0f);
        RitualStones->AddInstance(FTransform({0.0f, FMath::RadiansToDegrees(Angle), 0.0f}, Position,
                                             {0.34f, 0.34f, 0.76f}));
    }

    MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
    MoonLight->SetupAttachment(SceneRoot);
    MoonLight->SetRelativeRotation({-48.0f, -32.0f, 0.0f});
    MoonLight->SetLightColor(FLinearColor(0.62f, 0.72f, 0.88f));
    MoonLight->SetIntensity(5.0f);
    MoonLight->SetCastShadows(true);
    MoonLight->bAtmosphereSunLight = true;

    Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
    Atmosphere->SetupAttachment(SceneRoot);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetIntensity(0.45f);

    Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
    Fog->SetupAttachment(SceneRoot);
    Fog->SetFogDensity(0.016f);
    Fog->SetFogHeightFalloff(0.25f);
    Fog->SetFogInscatteringColor(FLinearColor(0.07f, 0.08f, 0.09f));
    Fog->SetStartDistance(500.0f);
}

void AAshenArena::BeginPlay()
{
    Super::BeginPlay();
    Ashen::Materials::Apply(Ground, this, FLinearColor(0.035f, 0.045f, 0.04f), 0.92f);
    Ashen::Materials::Apply(BoundaryMonoliths, this, FLinearColor(0.015f, 0.02f, 0.03f), 0.88f);
    Ashen::Materials::Apply(RitualStones, this, FLinearColor(0.18f, 0.015f, 0.01f), 0.76f);
}
