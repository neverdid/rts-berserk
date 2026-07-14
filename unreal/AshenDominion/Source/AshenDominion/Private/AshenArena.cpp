#include "AshenArena.h"

#include "AshenMaterials.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/UObjectGlobals.h"

namespace
{
constexpr float MapWidth = 3'840.0f;
constexpr float MapHeight = 2'160.0f;
constexpr float RiverCenterX = MapWidth * 0.5f;

void ConfigureInstances(UInstancedStaticMeshComponent* Component, USceneComponent* Parent, UStaticMesh* Mesh,
                        const bool bCastShadow = true)
{
    Component->SetupAttachment(Parent);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCastShadow(bCastShadow);
    Component->SetStaticMesh(Mesh);
}

void AddFlatSegment(UInstancedStaticMeshComponent* Component, const FVector2D& Start, const FVector2D& End,
                    const float Width, const float Height, const float Z)
{
    const FVector2D Delta = End - Start;
    const float Length = Delta.Size();
    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
    const FVector Position((Start.X + End.X) * 0.5f, (Start.Y + End.Y) * 0.5f, Z);
    Component->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), Position,
                                      FVector(Length / 100.0f, Width / 100.0f, Height / 100.0f)));
}

void AddCylinderBetween(UInstancedStaticMeshComponent* Component, const FVector& Start, const FVector& End,
                        const float Radius)
{
    const FVector Delta = End - Start;
    const float Length = Delta.Size();
    if (Length <= UE_KINDA_SMALL_NUMBER)
    {
        return;
    }

    const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Delta / Length);
    Component->AddInstance(FTransform(Rotation, (Start + End) * 0.5f,
                                      FVector(Radius / 50.0f, Radius / 50.0f, Length / 100.0f)));
}

bool IsInGameplayClearing(const FVector2D& Point)
{
    const FVector2D HumanBase(500.0f, 1'080.0f);
    const FVector2D MonsterBase(3'340.0f, 1'080.0f);
    if (FVector2D::Distance(Point, HumanBase) < 510.0f || FVector2D::Distance(Point, MonsterBase) < 510.0f)
    {
        return true;
    }

    const bool bMainLane = Point.X > 700.0f && Point.X < 3'140.0f && FMath::Abs(Point.Y - 1'080.0f) < 215.0f;
    const bool bNorthBridgeLane = FMath::Abs(Point.Y - 680.0f) < 145.0f &&
                                  FMath::Abs(Point.X - RiverCenterX) < 560.0f;
    const bool bSouthBridgeLane = FMath::Abs(Point.Y - 1'480.0f) < 145.0f &&
                                  FMath::Abs(Point.X - RiverCenterX) < 560.0f;
    return bMainLane || bNorthBridgeLane || bSouthBridgeLane;
}
}

AAshenArena::AAshenArena()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
    Ground->SetupAttachment(SceneRoot);
    Ground->SetMobility(EComponentMobility::Static);
    Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Ground->SetCollisionResponseToAllChannels(ECR_Block);
    Ground->SetRelativeLocation({MapWidth * 0.5f, MapHeight * 0.5f, 0.0f});
    Ground->SetRelativeScale3D({MapWidth / 100.0f, MapHeight / 100.0f, 1.0f});
    Ground->SetStaticMesh(Plane);

    GroundMottle = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GroundMottle"));
    Roadbed = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Roadbed"));
    RoadStones = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadStones"));
    WaterRipples = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WaterRipples"));
    RiverBanks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RiverBanks"));
    BridgeTimbers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BridgeTimbers"));
    BridgeIron = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BridgeIron"));
    TreeTrunks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeTrunks"));
    TreeCrowns = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeCrowns"));
    DeadBranches = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DeadBranches"));
    GrassTufts = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GrassTufts"));
    Rocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rocks"));
    HumanWalls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HumanWalls"));
    HumanTowers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HumanTowers"));
    HumanRoofs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HumanRoofs"));
    MonsterMasses = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MonsterMasses"));
    MonsterSpikes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MonsterSpikes"));
    BonePalisade = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BonePalisade"));
    BoundaryMonoliths = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BoundaryMonoliths"));
    RitualStones = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RitualStones"));

    ConfigureInstances(GroundMottle, SceneRoot, Plane, false);
    ConfigureInstances(Roadbed, SceneRoot, Cube, false);
    ConfigureInstances(RoadStones, SceneRoot, Cube, false);
    ConfigureInstances(WaterRipples, SceneRoot, Plane, false);
    ConfigureInstances(RiverBanks, SceneRoot, Sphere);
    ConfigureInstances(BridgeTimbers, SceneRoot, Cube);
    ConfigureInstances(BridgeIron, SceneRoot, Cube);
    ConfigureInstances(TreeTrunks, SceneRoot, Cylinder);
    ConfigureInstances(TreeCrowns, SceneRoot, Cone);
    ConfigureInstances(DeadBranches, SceneRoot, Cylinder);
    ConfigureInstances(GrassTufts, SceneRoot, Cone, false);
    ConfigureInstances(Rocks, SceneRoot, Sphere);
    ConfigureInstances(HumanWalls, SceneRoot, Cube);
    ConfigureInstances(HumanTowers, SceneRoot, Cylinder);
    ConfigureInstances(HumanRoofs, SceneRoot, Cone);
    ConfigureInstances(MonsterMasses, SceneRoot, Sphere);
    ConfigureInstances(MonsterSpikes, SceneRoot, Cone);
    ConfigureInstances(BonePalisade, SceneRoot, Cone);
    ConfigureInstances(BoundaryMonoliths, SceneRoot, Cube);
    ConfigureInstances(RitualStones, SceneRoot, Cylinder);

    BuildRiver();
    BuildRoadsAndBridges();
    BuildFortifications();
    BuildVegetation();
    BuildLandmarks();

    MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
    MoonLight->SetupAttachment(SceneRoot);
    MoonLight->SetRelativeRotation({-32.0f, -28.0f, 0.0f});
    MoonLight->SetLightColor(FLinearColor(0.55f, 0.68f, 0.88f));
    MoonLight->SetIntensity(8.5f);
    MoonLight->SetCastShadows(true);
    MoonLight->bAtmosphereSunLight = true;

    Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
    Atmosphere->SetupAttachment(SceneRoot);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetIntensity(0.92f);
    SkyLight->SetLightColor(FLinearColor(0.58f, 0.66f, 0.72f));

    Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
    Fog->SetupAttachment(SceneRoot);
    Fog->SetFogDensity(0.011f);
    Fog->SetFogHeightFalloff(0.22f);
    Fog->SetFogInscatteringColor(FLinearColor(0.055f, 0.07f, 0.075f));
    Fog->SetStartDistance(420.0f);

    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
    PostProcess->SetupAttachment(SceneRoot);
    PostProcess->bUnbound = true;
    PostProcess->Settings.bOverride_VignetteIntensity = true;
    PostProcess->Settings.VignetteIntensity = 0.24f;
    PostProcess->Settings.bOverride_ColorSaturation = true;
    PostProcess->Settings.ColorSaturation = FVector4(0.82f, 0.88f, 0.84f, 1.0f);
    PostProcess->Settings.bOverride_ColorContrast = true;
    PostProcess->Settings.ColorContrast = FVector4(1.08f, 1.06f, 1.04f, 1.0f);
    PostProcess->Settings.bOverride_AutoExposureBias = true;
    PostProcess->Settings.AutoExposureBias = 2.20f;
}

void AAshenArena::BuildRiver()
{
    constexpr int32 SegmentCount = 12;
    constexpr float RiverWidth = 265.0f;
    UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        const float Y0 = static_cast<float>(Index) * MapHeight / SegmentCount - 10.0f;
        const float Y1 = static_cast<float>(Index + 1) * MapHeight / SegmentCount + 10.0f;
        const float X0 = RiverCenterX + FMath::Sin(static_cast<float>(Index) * 0.72f) * 58.0f;
        const float X1 = RiverCenterX + FMath::Sin(static_cast<float>(Index + 1) * 0.72f) * 58.0f;
        const FVector2D WaterDelta(X1 - X0, Y1 - Y0);
        const float WaterLength = WaterDelta.Size();
        const float WaterYaw = FMath::RadiansToDegrees(FMath::Atan2(WaterDelta.Y, WaterDelta.X));
        UStaticMeshComponent* WaterSegment = CreateDefaultSubobject<UStaticMeshComponent>(
            FName(*FString::Printf(TEXT("WaterSegment_%02d"), Index)));
        WaterSegment->SetupAttachment(SceneRoot);
        WaterSegment->SetMobility(EComponentMobility::Static);
        WaterSegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WaterSegment->SetCastShadow(false);
        WaterSegment->SetStaticMesh(Plane);
        WaterSegment->SetRelativeTransform(FTransform(
            FRotator(0.0f, WaterYaw, 0.0f), FVector((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, 4.0f),
            FVector(WaterLength / 100.0f, RiverWidth / 100.0f, 1.0f)));
        WaterSegments.Add(WaterSegment);

        const FVector2D SegmentDirection = FVector2D(X1 - X0, Y1 - Y0).GetSafeNormal();
        const FVector2D SegmentNormal(-SegmentDirection.Y, SegmentDirection.X);
        for (int32 Ripple = -1; Ripple <= 1; ++Ripple)
        {
            const float Along = 0.22f + static_cast<float>(Ripple + 1) * 0.25f;
            const FVector2D Center = FVector2D(X0, Y0) + FVector2D(X1 - X0, Y1 - Y0) * Along +
                                     SegmentNormal * static_cast<float>(Ripple) * 54.0f;
            const FVector2D Half = SegmentDirection * (34.0f + static_cast<float>((Index + Ripple + 3) % 3) * 13.0f);
            AddFlatSegment(WaterRipples, Center - Half, Center + Half, 4.5f, 1.0f, 5.2f);
        }

        const FVector2D Direction = SegmentDirection;
        const FVector2D Normal = SegmentNormal;
        for (int32 Bank = -1; Bank <= 1; Bank += 2)
        {
            const FVector2D Mid((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f);
            const FVector2D BankPoint = Mid + Normal * (RiverWidth * 0.53f * static_cast<float>(Bank));
            const float Yaw = static_cast<float>((Index * 37 + Bank * 11) % 90);
            RiverBanks->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f),
                                               FVector(BankPoint.X, BankPoint.Y, 16.0f),
                                               FVector(1.0f, 0.34f, 0.26f)));
        }
    }

    // The central cursed-iron node sits on a raised ritual island.
    RiverBanks->AddInstance(FTransform(FRotator::ZeroRotator, FVector(RiverCenterX, 1'080.0f, 9.0f),
                                       FVector(2.15f, 1.55f, 0.18f)));
}

void AAshenArena::BuildRoadsAndBridges()
{
    const TArray<FVector2D> NorthRoad{
        {520.0f, 1'080.0f}, {980.0f, 1'050.0f}, {1'420.0f, 760.0f}, {1'790.0f, 680.0f},
        {2'050.0f, 680.0f}, {2'430.0f, 770.0f}, {2'860.0f, 1'050.0f}, {3'320.0f, 1'080.0f},
    };
    const TArray<FVector2D> SouthRoad{
        {520.0f, 1'080.0f}, {980.0f, 1'120.0f}, {1'420.0f, 1'410.0f}, {1'790.0f, 1'480.0f},
        {2'050.0f, 1'480.0f}, {2'430.0f, 1'390.0f}, {2'860.0f, 1'120.0f}, {3'320.0f, 1'080.0f},
    };

    auto AddRoad = [this](const TArray<FVector2D>& Points)
    {
        for (int32 Index = 1; Index < Points.Num(); ++Index)
        {
            AddFlatSegment(Roadbed, Points[Index - 1], Points[Index], 112.0f, 4.0f, 4.5f);
            AddFlatSegment(RoadStones, Points[Index - 1], Points[Index], 68.0f, 2.0f, 7.0f);
        }
    };
    AddRoad(NorthRoad);
    AddRoad(SouthRoad);

    for (const float BridgeY : {680.0f, 1'480.0f})
    {
        for (int32 Plank = -6; Plank <= 6; ++Plank)
        {
            const float X = RiverCenterX + static_cast<float>(Plank) * 23.0f;
            BridgeTimbers->AddInstance(FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(X, BridgeY, 18.0f),
                                                  FVector(0.205f, 1.62f, 0.12f)));
        }
        for (const float RailOffset : {-92.0f, 92.0f})
        {
            BridgeIron->AddInstance(FTransform(FRotator::ZeroRotator,
                                               FVector(RiverCenterX, BridgeY + RailOffset, 36.0f),
                                               FVector(3.35f, 0.075f, 0.11f)));
        }
    }
}

void AAshenArena::BuildFortifications()
{
    constexpr float HumanX = 500.0f;
    constexpr float BaseY = 1'080.0f;
    for (const float Y : {790.0f, 1'370.0f})
    {
        HumanWalls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(HumanX - 15.0f, Y, 62.0f),
                                           FVector(4.8f, 0.28f, 1.22f)));
    }
    HumanWalls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(230.0f, BaseY, 62.0f),
                                       FVector(0.28f, 5.55f, 1.22f)));
    for (const float Y : {855.0f, 1'305.0f})
    {
        HumanWalls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(770.0f, Y, 62.0f),
                                           FVector(0.28f, 1.35f, 1.22f)));
    }
    for (const FVector2D Corner : {FVector2D(230.0f, 790.0f), FVector2D(230.0f, 1'370.0f),
                                   FVector2D(770.0f, 790.0f), FVector2D(770.0f, 1'370.0f)})
    {
        HumanTowers->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Corner.X, Corner.Y, 95.0f),
                                            FVector(0.58f, 0.58f, 1.9f)));
        HumanRoofs->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Corner.X, Corner.Y, 222.0f),
                                           FVector(0.78f, 0.78f, 0.82f)));
    }
    for (int32 Crenel = 0; Crenel < 9; ++Crenel)
    {
        const float X = 110.0f + static_cast<float>(Crenel) * 96.0f;
        for (const float Y : {790.0f, 1'370.0f})
        {
            HumanWalls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, 137.0f),
                                               FVector(0.24f, 0.36f, 0.28f)));
        }
    }

    const FVector2D MonsterBase(3'340.0f, BaseY);
    for (int32 Index = 0; Index < 18; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / 18.0f;
        const float Radius = Index % 2 == 0 ? 320.0f : 286.0f;
        const FVector Position(MonsterBase.X + FMath::Cos(Angle) * Radius,
                               MonsterBase.Y + FMath::Sin(Angle) * Radius,
                               62.0f + static_cast<float>(Index % 3) * 8.0f);
        const float Facing = FMath::RadiansToDegrees(Angle) + 90.0f;
        BonePalisade->AddInstance(FTransform(FRotator(-8.0f, Facing, 0.0f), Position,
                                             FVector(0.26f, 0.26f, 1.35f + static_cast<float>(Index % 4) * 0.18f)));
    }
    for (int32 Index = 0; Index < 7; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / 7.0f;
        const FVector Position(MonsterBase.X + FMath::Cos(Angle) * 210.0f,
                               MonsterBase.Y + FMath::Sin(Angle) * 205.0f, 46.0f);
        MonsterMasses->AddInstance(FTransform(FRotator(0.0f, Index * 31.0f, 0.0f), Position,
                                              FVector(0.9f, 0.62f, 0.48f)));
        MonsterSpikes->AddInstance(FTransform(FRotator(-12.0f, Index * 51.0f, 0.0f),
                                              Position + FVector(0.0f, 0.0f, 100.0f),
                                              FVector(0.32f, 0.32f, 1.28f)));
    }
}

void AAshenArena::BuildVegetation()
{
    FRandomStream Random(0xA51E2026);

    for (int32 Index = 0; Index < 118; ++Index)
    {
        const FVector2D Point(Random.FRandRange(90.0f, MapWidth - 90.0f),
                              Random.FRandRange(90.0f, MapHeight - 90.0f));
        if (IsInGameplayClearing(Point) || FMath::Abs(Point.X - RiverCenterX) < 230.0f)
        {
            continue;
        }

        const float Height = Random.FRandRange(125.0f, 235.0f);
        const float TrunkRadius = Random.FRandRange(9.0f, 16.0f);
        TreeTrunks->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), Random.FRandRange(-5.0f, 5.0f)),
                                           FVector(Point.X, Point.Y, Height * 0.5f),
                                           FVector(TrunkRadius / 50.0f, TrunkRadius / 50.0f, Height / 100.0f)));

        const bool bDeadTree = Index % 4 == 0;
        if (!bDeadTree)
        {
            TreeCrowns->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), 0.0f),
                                               FVector(Point.X, Point.Y, Height + 52.0f),
                                               FVector(Random.FRandRange(0.72f, 1.08f),
                                                       Random.FRandRange(0.72f, 1.08f),
                                                       Random.FRandRange(1.2f, 1.8f))));
        }
        else
        {
            const FVector Crown(Point.X, Point.Y, Height * 0.78f);
            AddCylinderBetween(DeadBranches, Crown, Crown + FVector(52.0f, 22.0f, 55.0f), TrunkRadius * 0.42f);
            AddCylinderBetween(DeadBranches, Crown, Crown + FVector(-44.0f, -30.0f, 42.0f), TrunkRadius * 0.36f);
        }
    }

    for (int32 Index = 0; Index < 360; ++Index)
    {
        const FVector2D Point(Random.FRandRange(45.0f, MapWidth - 45.0f),
                              Random.FRandRange(45.0f, MapHeight - 45.0f));
        if (IsInGameplayClearing(Point) || FMath::Abs(Point.X - RiverCenterX) < 175.0f)
        {
            continue;
        }
        GrassTufts->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f),
                                                    Random.FRandRange(-7.0f, 7.0f)),
                                          FVector(Point.X, Point.Y, 10.0f),
                                          FVector(Random.FRandRange(0.055f, 0.11f),
                                                  Random.FRandRange(0.055f, 0.11f),
                                                  Random.FRandRange(0.16f, 0.34f))));
    }

    for (int32 Index = 0; Index < 52; ++Index)
    {
        const FVector2D Point(Random.FRandRange(60.0f, MapWidth - 60.0f),
                              Random.FRandRange(60.0f, MapHeight - 60.0f));
        if (IsInGameplayClearing(Point))
        {
            continue;
        }
        Rocks->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), Random.FRandRange(-18.0f, 18.0f)),
                                      FVector(Point.X, Point.Y, Random.FRandRange(9.0f, 20.0f)),
                                      FVector(Random.FRandRange(0.28f, 0.72f), Random.FRandRange(0.22f, 0.58f),
                                              Random.FRandRange(0.18f, 0.42f))));
    }

    for (int32 Index = 0; Index < 84; ++Index)
    {
        const FVector2D Point(Random.FRandRange(80.0f, MapWidth - 80.0f),
                              Random.FRandRange(80.0f, MapHeight - 80.0f));
        if (FMath::Abs(Point.X - RiverCenterX) < 220.0f)
        {
            continue;
        }
        GroundMottle->AddInstance(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), 0.0f),
                                             FVector(Point.X, Point.Y, 1.2f),
                                             FVector(Random.FRandRange(1.8f, 4.5f), Random.FRandRange(1.1f, 3.0f), 1.0f)));
    }
}

void AAshenArena::BuildLandmarks()
{
    for (int32 Index = 0; Index < 16; ++Index)
    {
        const float X = 120.0f + static_cast<float>(Index) * 240.0f;
        const float HeightScale = 1.15f + static_cast<float>((Index * 7) % 5) * 0.16f;
        const FRotator Rotation(0.0f, static_cast<float>((Index * 23) % 90), 0.0f);
        BoundaryMonoliths->AddInstance(FTransform(Rotation, {X, 55.0f, HeightScale * 50.0f},
                                                  {0.48f, 0.72f, HeightScale}));
        BoundaryMonoliths->AddInstance(FTransform(Rotation, {X, 2'105.0f, HeightScale * 50.0f},
                                                  {0.48f, 0.72f, HeightScale}));
    }
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float Y = 240.0f + static_cast<float>(Index) * 240.0f;
        const float HeightScale = 1.2f + static_cast<float>((Index * 3) % 4) * 0.2f;
        BoundaryMonoliths->AddInstance(FTransform({0.0f, static_cast<float>(Index * 11), 0.0f},
                                                  {55.0f, Y, HeightScale * 50.0f}, {0.72f, 0.48f, HeightScale}));
        BoundaryMonoliths->AddInstance(FTransform({0.0f, static_cast<float>(Index * 11), 0.0f},
                                                  {3'785.0f, Y, HeightScale * 50.0f}, {0.72f, 0.48f, HeightScale}));
    }

    for (int32 Index = 0; Index < 12; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / 12.0f;
        const FVector Position(RiverCenterX + FMath::Cos(Angle) * 205.0f,
                               1'080.0f + FMath::Sin(Angle) * 135.0f, 48.0f);
        RitualStones->AddInstance(FTransform({0.0f, FMath::RadiansToDegrees(Angle), 0.0f}, Position,
                                             {0.25f, 0.25f, 0.95f}));
    }
}

void AAshenArena::BeginPlay()
{
    Super::BeginPlay();

    Ashen::Materials::Apply(Ground, this, FLinearColor(0.038f, 0.055f, 0.043f), 0.96f);
    Ashen::Materials::Apply(GroundMottle, this, FLinearColor(0.078f, 0.105f, 0.064f), 0.98f);
    Ashen::Materials::Apply(Roadbed, this, FLinearColor(0.13f, 0.105f, 0.075f), 0.98f);
    Ashen::Materials::Apply(RoadStones, this, FLinearColor(0.21f, 0.20f, 0.17f), 0.9f);
    Ashen::Materials::Apply(WaterRipples, this, FLinearColor(0.11f, 0.32f, 0.36f), 0.08f);
    Ashen::Materials::Apply(RiverBanks, this, FLinearColor(0.085f, 0.095f, 0.085f), 0.93f);
    Ashen::Materials::Apply(BridgeTimbers, this, FLinearColor(0.16f, 0.095f, 0.045f), 0.84f);
    Ashen::Materials::Apply(BridgeIron, this, FLinearColor(0.16f, 0.17f, 0.16f), 0.48f);
    Ashen::Materials::Apply(TreeTrunks, this, FLinearColor(0.075f, 0.045f, 0.026f), 0.96f);
    Ashen::Materials::Apply(TreeCrowns, this, FLinearColor(0.055f, 0.12f, 0.062f), 0.98f);
    Ashen::Materials::Apply(DeadBranches, this, FLinearColor(0.09f, 0.064f, 0.045f), 0.97f);
    Ashen::Materials::Apply(GrassTufts, this, FLinearColor(0.10f, 0.18f, 0.075f), 0.99f);
    Ashen::Materials::Apply(Rocks, this, FLinearColor(0.12f, 0.13f, 0.12f), 0.94f);
    Ashen::Materials::Apply(HumanWalls, this, FLinearColor(0.22f, 0.23f, 0.22f), 0.91f);
    Ashen::Materials::Apply(HumanTowers, this, FLinearColor(0.25f, 0.25f, 0.23f), 0.89f);
    Ashen::Materials::Apply(HumanRoofs, this, FLinearColor(0.22f, 0.055f, 0.03f), 0.78f);
    Ashen::Materials::Apply(MonsterMasses, this, FLinearColor(0.16f, 0.018f, 0.024f), 0.72f);
    Ashen::Materials::Apply(MonsterSpikes, this, FLinearColor(0.29f, 0.028f, 0.035f), 0.66f);
    Ashen::Materials::Apply(BonePalisade, this, FLinearColor(0.42f, 0.38f, 0.29f), 0.86f);
    Ashen::Materials::Apply(BoundaryMonoliths, this, FLinearColor(0.025f, 0.03f, 0.035f), 0.9f);
    Ashen::Materials::Apply(RitualStones, this, FLinearColor(0.25f, 0.025f, 0.018f), 0.74f);

    if (UMaterialInterface* WaterMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/EngineMaterials/WaterMaterial.DefaultWaterMaterial")))
    {
        for (UStaticMeshComponent* WaterSegment : WaterSegments)
        {
            WaterSegment->SetMaterial(0, WaterMaterial);
        }
    }
    else
    {
        for (UStaticMeshComponent* WaterSegment : WaterSegments)
        {
            Ashen::Materials::Apply(WaterSegment, this, FLinearColor(0.018f, 0.095f, 0.12f), 0.12f);
        }
    }
}
