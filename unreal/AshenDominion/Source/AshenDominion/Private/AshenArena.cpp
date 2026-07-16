#include "AshenArena.h"

#include "AshenMaterials.h"
#include "AshenWorldLayout.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UObjectGlobals.h"

namespace
{
constexpr float MapWidth = Ashen::WorldLayout::Width;
constexpr float MapHeight = Ashen::WorldLayout::Height;
constexpr float RiverCenterX = Ashen::WorldLayout::CenterX;

float SmoothRange(const float Minimum, const float Maximum, const float Value)
{
    const float Alpha =
        FMath::Clamp((Value - Minimum) / FMath::Max(Maximum - Minimum, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
    return Alpha * Alpha * (3.0f - 2.0f * Alpha);
}

const TArray<FVector2D> &DirectRoute()
{
    static const TArray<FVector2D> Route{{600.0f, 1'400.0f}, {1'200.0f, 1'400.0f}, {1'900.0f, 1'400.0f},
                                         {2'400.0f, 1'400.0f}, {3'100.0f, 1'400.0f}, {4'200.0f, 1'400.0f}};
    return Route;
}

const TArray<FVector2D> &NorthRoute()
{
    static const TArray<FVector2D> Route{
        {600.0f, 1'400.0f}, {780.0f, 1'180.0f}, {900.0f, 930.0f}, {900.0f, 650.0f}, {940.0f, 420.0f},
        {1'180.0f, 340.0f}, {1'680.0f, 340.0f}, {2'050.0f, 600.0f}, {2'240.0f, 760.0f},
        {2'560.0f, 760.0f}, {2'780.0f, 650.0f}, {3'140.0f, 760.0f}, {3'580.0f, 960.0f},
        {4'020.0f, 1'180.0f}, {4'200.0f, 1'400.0f},
    };
    return Route;
}

const TArray<FVector2D> &SouthRoute()
{
    static const TArray<FVector2D> Route{
        {600.0f, 1'400.0f}, {780.0f, 1'620.0f}, {1'220.0f, 1'840.0f}, {1'660.0f, 2'040.0f},
        {2'020.0f, 2'150.0f}, {2'240.0f, 2'040.0f}, {2'560.0f, 2'040.0f}, {2'750.0f, 2'200.0f},
        {3'120.0f, 2'460.0f}, {3'620.0f, 2'460.0f}, {3'860.0f, 2'380.0f}, {3'900.0f, 2'150.0f},
        {3'900.0f, 1'870.0f}, {4'020.0f, 1'620.0f}, {4'200.0f, 1'400.0f},
    };
    return Route;
}

float DistanceToSegment(const FVector2D &Point, const FVector2D &Start, const FVector2D &End)
{
    const FVector2D Segment = End - Start;
    const float SegmentLengthSquared = Segment.SizeSquared();
    if (SegmentLengthSquared <= UE_KINDA_SMALL_NUMBER)
    {
        return FVector2D::Distance(Point, Start);
    }
    const float Alpha = FMath::Clamp(FVector2D::DotProduct(Point - Start, Segment) / SegmentLengthSquared, 0.0f, 1.0f);
    return FVector2D::Distance(Point, Start + Segment * Alpha);
}

float DistanceToRoute(const FVector2D &Point, const TArray<FVector2D> &Route)
{
    float Distance = TNumericLimits<float>::Max();
    for (int32 Index = 1; Index < Route.Num(); ++Index)
    {
        Distance = FMath::Min(Distance, DistanceToSegment(Point, Route[Index - 1], Route[Index]));
    }
    return Distance;
}

float EllipseFalloff(const FVector2D &Point, const FVector2D &Center, const FVector2D &Radius)
{
    const float NormalizedX = (Point.X - Center.X) / Radius.X;
    const float NormalizedY = (Point.Y - Center.Y) / Radius.Y;
    return FMath::Exp(-(NormalizedX * NormalizedX + NormalizedY * NormalizedY) * 1.45f);
}

float TerrainHeightAt(const float X, const float Y)
{
    const float ClampedX = FMath::Clamp(X, 0.0f, MapWidth);
    const float ClampedY = FMath::Clamp(Y, 0.0f, MapHeight);
    const FVector2D Point(ClampedX, ClampedY);
    const float DirectDistance = DistanceToRoute(Point, DirectRoute());
    const float FlankDistance = FMath::Min(DistanceToRoute(Point, NorthRoute()), DistanceToRoute(Point, SouthRoute()));
    const float HumanBaseDistance = FVector2D::Distance(Point, {Ashen::WorldLayout::HumanBaseX, MapHeight * 0.5f});
    const float MonsterBaseDistance = FVector2D::Distance(Point, {Ashen::WorldLayout::MonsterBaseX, MapHeight * 0.5f});
    const float ClearingMask =
        FMath::Max(FMath::Max(1.0f - SmoothRange(165.0f, 330.0f, DirectDistance),
                             1.0f - SmoothRange(105.0f, 235.0f, FlankDistance)),
                   FMath::Max(1.0f - SmoothRange(390.0f, 610.0f, HumanBaseDistance),
                              1.0f - SmoothRange(390.0f, 610.0f, MonsterBaseDistance)));

    const float BorderDistance =
        FMath::Min(FMath::Min(ClampedX, MapWidth - ClampedX), FMath::Min(ClampedY, MapHeight - ClampedY));
    const float EdgeRise = (1.0f - SmoothRange(95.0f, 520.0f, BorderDistance)) * 155.0f;
    const float BroadUndulation = FMath::Sin(ClampedX * 0.0031f + ClampedY * 0.0023f) * 17.0f +
                                  FMath::Sin(ClampedX * 0.0074f - ClampedY * 0.0048f) * 9.0f;
    const float Mountain = EllipseFalloff(Point, {1'420.0f, 800.0f}, {650.0f, 520.0f}) * 330.0f +
                           EllipseFalloff(Point, {1'720.0f, 1'050.0f}, {420.0f, 360.0f}) * 150.0f;
    const float GravewoodRise = EllipseFalloff(Point, {3'380.0f, 2'020.0f}, {700.0f, 540.0f}) * 46.0f;

    const float RiverX = RiverCenterX + FMath::Sin((ClampedY / MapHeight) * 8.64f) * 58.0f;
    const float RiverDistance = FMath::Abs(ClampedX - RiverX);
    const float RiverCut = (1.0f - SmoothRange(115.0f, 280.0f, RiverDistance)) * 42.0f;
    const float Terrain = EdgeRise + (BroadUndulation + Mountain + GravewoodRise) * (1.0f - ClearingMask * 0.94f);
    return Terrain - RiverCut;
}

float RenderTerrainHeightAt(const float X, const float Y)
{
    const float OutsideDistance = FMath::Max(FMath::Max(-X, X - MapWidth), FMath::Max(-Y, Y - MapHeight));
    return TerrainHeightAt(X, Y) + SmoothRange(0.0f, 1'050.0f, OutsideDistance) * 310.0f;
}

Ashen::Materials::FSurfaceStyle SurfaceStyle(const FLinearColor &BaseColor, const FLinearColor &SecondaryColor,
                                             const FLinearColor &AccentColor, const float Roughness,
                                             const float MacroScale = 360.0f, const float DetailScale = 72.0f,
                                             const float DetailStrength = 0.16f, const float Specular = 0.25f)
{
    return {BaseColor,   SecondaryColor, AccentColor, Roughness, MacroScale,
            DetailScale, DetailStrength, Specular,    0.92f};
}

void ConfigureInstances(UInstancedStaticMeshComponent *Component, USceneComponent *Parent, UStaticMesh *Mesh,
                        const bool bCastShadow = true)
{
    Component->SetupAttachment(Parent);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCastShadow(bCastShadow);
    Component->SetStaticMesh(Mesh);
}

void AddFlatSegment(UInstancedStaticMeshComponent *Component, const FVector2D &Start, const FVector2D &End,
                    const float Width, const float Height, const float Z)
{
    const FVector2D Delta = End - Start;
    const float Length = Delta.Size();
    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
    const FVector Position((Start.X + End.X) * 0.5f, (Start.Y + End.Y) * 0.5f, Z);
    Component->AddInstance(
        FTransform(FRotator(0.0f, Yaw, 0.0f), Position, FVector(Length / 100.0f, Width / 100.0f, Height / 100.0f)));
}

void AddCylinderBetween(UInstancedStaticMeshComponent *Component, const FVector &Start, const FVector &End,
                        const float Radius)
{
    const FVector Delta = End - Start;
    const float Length = Delta.Size();
    if (Length <= UE_KINDA_SMALL_NUMBER)
    {
        return;
    }

    const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Delta / Length);
    Component->AddInstance(
        FTransform(Rotation, (Start + End) * 0.5f, FVector(Radius / 50.0f, Radius / 50.0f, Length / 100.0f)));
}

bool IsInGameplayClearing(const FVector2D &Point)
{
    const FVector2D HumanBase(Ashen::WorldLayout::HumanBaseX, Ashen::WorldLayout::CenterY);
    const FVector2D MonsterBase(Ashen::WorldLayout::MonsterBaseX, Ashen::WorldLayout::CenterY);
    if (FVector2D::Distance(Point, HumanBase) < 530.0f || FVector2D::Distance(Point, MonsterBase) < 530.0f)
    {
        return true;
    }

    return DistanceToRoute(Point, DirectRoute()) < 235.0f || DistanceToRoute(Point, NorthRoute()) < 145.0f ||
           DistanceToRoute(Point, SouthRoute()) < 145.0f;
}
} // namespace

AAshenArena::AAshenArena()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    UStaticMesh *Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    UStaticMesh *Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh *Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh *Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    UStaticMesh *Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
    Ground->SetupAttachment(SceneRoot);
    Ground->SetMobility(EComponentMobility::Static);
    Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Ground->SetCollisionResponseToAllChannels(ECR_Block);
    Ground->SetRelativeLocation({MapWidth * 0.5f, MapHeight * 0.5f, 0.0f});
    Ground->SetRelativeScale3D({MapWidth / 100.0f, MapHeight / 100.0f, 1.0f});
    Ground->SetStaticMesh(Plane);
    Ground->SetCastShadow(false);
    Ground->SetVisibility(false);

    Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainGeometry"));
    Terrain->SetupAttachment(SceneRoot);
    Terrain->SetMobility(EComponentMobility::Static);
    Terrain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Terrain->SetCastShadow(true);

    Roadbed = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Roadbed"));
    RoadStones = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadStones"));
    RoadRuts = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadRuts"));
    RiverBanks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RiverBanks"));
    Reeds = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Reeds"));
    BridgeTimbers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BridgeTimbers"));
    BridgeIron = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BridgeIron"));
    TreeTrunks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeTrunks"));
    TreeCrowns = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeCrowns"));
    TreeCrownsShadow = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeCrownsShadow"));
    TreeCanopies = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeCanopies"));
    DeadBranches = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DeadBranches"));
    GrassTufts = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GrassTufts"));
    Rocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rocks"));
    MountainRocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MountainRocks"));
    MineMouths = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MineMouths"));
    MineTimbers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MineTimbers"));
    ForestRoots = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ForestRoots"));
    HumanWalls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HumanWalls"));
    HumanTowers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HumanTowers"));
    HumanRoofs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HumanRoofs"));
    HumanFoundations = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HumanFoundations"));
    HumanTrim = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HumanTrim"));
    HumanBanners = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HumanBanners"));
    MonsterMasses = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MonsterMasses"));
    MonsterSpikes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MonsterSpikes"));
    MonsterRibs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MonsterRibs"));
    MonsterSinew = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MonsterSinew"));
    BonePalisade = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BonePalisade"));
    RitualStones = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RitualStones"));
    MythicArches = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MythicArches"));
    BrazierBowls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BrazierBowls"));
    EmberCores = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("EmberCores"));

    ConfigureInstances(Roadbed, SceneRoot, Cube, false);
    ConfigureInstances(RoadStones, SceneRoot, Cube, false);
    ConfigureInstances(RoadRuts, SceneRoot, Cube, false);
    ConfigureInstances(RiverBanks, SceneRoot, Sphere);
    ConfigureInstances(Reeds, SceneRoot, Cone, false);
    ConfigureInstances(BridgeTimbers, SceneRoot, Cube);
    ConfigureInstances(BridgeIron, SceneRoot, Cube);
    ConfigureInstances(TreeTrunks, SceneRoot, Cylinder);
    ConfigureInstances(TreeCrowns, SceneRoot, Cone);
    ConfigureInstances(TreeCrownsShadow, SceneRoot, Cone);
    ConfigureInstances(TreeCanopies, SceneRoot, Sphere);
    ConfigureInstances(DeadBranches, SceneRoot, Cylinder);
    ConfigureInstances(GrassTufts, SceneRoot, Cone, false);
    ConfigureInstances(Rocks, SceneRoot, Sphere);
    ConfigureInstances(MountainRocks, SceneRoot, Sphere);
    ConfigureInstances(MineMouths, SceneRoot, Cube);
    ConfigureInstances(MineTimbers, SceneRoot, Cylinder);
    ConfigureInstances(ForestRoots, SceneRoot, Cylinder);
    ConfigureInstances(HumanWalls, SceneRoot, Cube);
    ConfigureInstances(HumanTowers, SceneRoot, Cylinder);
    ConfigureInstances(HumanRoofs, SceneRoot, Cone);
    ConfigureInstances(HumanFoundations, SceneRoot, Cube);
    ConfigureInstances(HumanTrim, SceneRoot, Cube);
    ConfigureInstances(HumanBanners, SceneRoot, Cube, false);
    ConfigureInstances(MonsterMasses, SceneRoot, Sphere);
    ConfigureInstances(MonsterSpikes, SceneRoot, Cone);
    ConfigureInstances(MonsterRibs, SceneRoot, Cylinder);
    ConfigureInstances(MonsterSinew, SceneRoot, Sphere);
    ConfigureInstances(BonePalisade, SceneRoot, Cone);
    ConfigureInstances(RitualStones, SceneRoot, Cylinder);
    ConfigureInstances(MythicArches, SceneRoot, Cylinder);
    ConfigureInstances(BrazierBowls, SceneRoot, Cylinder);
    ConfigureInstances(EmberCores, SceneRoot, Sphere, false);

    BuildRiver();
    BuildRoadsAndBridges();
    BuildFortifications();
    BuildVegetation();
    BuildLandmarks();

    const TArray<FVector> LightLocations{
        {895.0f, 1'280.0f, 102.0f}, {895.0f, 1'520.0f, 102.0f}, {3'905.0f, 1'290.0f, 92.0f},
        {3'905.0f, 1'510.0f, 92.0f}, {RiverCenterX, Ashen::WorldLayout::CenterY, 84.0f},
    };
    for (int32 Index = 0; Index < LightLocations.Num(); ++Index)
    {
        UPointLightComponent *Light =
            CreateDefaultSubobject<UPointLightComponent>(FName(*FString::Printf(TEXT("AccentLight_%02d"), Index)));
        Light->SetupAttachment(SceneRoot);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetRelativeLocation(LightLocations[Index]);
        Light->SetAttenuationRadius(Index == 4 ? 410.0f : 285.0f);
        Light->SetIntensity(Index == 4 ? 760.0f : 1'050.0f);
        Light->SetSourceRadius(9.0f);
        Light->SetCastShadows(false);
        if (Index < 2)
        {
            Light->SetLightColor(FLinearColor(1.0f, 0.47f, 0.12f));
        }
        else if (Index < 4)
        {
            Light->SetLightColor(FLinearColor(0.86f, 0.025f, 0.018f));
        }
        else
        {
            Light->SetLightColor(FLinearColor(0.12f, 0.56f, 0.52f));
        }
        AccentLights.Add(Light);
    }

    MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
    MoonLight->SetupAttachment(SceneRoot);
    MoonLight->SetRelativeRotation({-56.0f, -32.0f, 0.0f});
    MoonLight->SetLightColor(FLinearColor(0.68f, 0.75f, 0.88f));
    MoonLight->SetIntensity(6.6f);
    MoonLight->SetCastShadows(true);
    MoonLight->SetForwardShadingPriority(1);
    MoonLight->SetLightSourceAngle(4.0f);
    MoonLight->SetShadowAmount(0.52f);
    MoonLight->SetIndirectLightingIntensity(1.25f);
    MoonLight->SetVolumetricScatteringIntensity(0.72f);
    MoonLight->bAtmosphereSunLight = true;

    FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("OvercastFillLight"));
    FillLight->SetupAttachment(SceneRoot);
    FillLight->SetRelativeRotation({-52.0f, 146.0f, 0.0f});
    FillLight->SetLightColor(FLinearColor(0.42f, 0.48f, 0.46f));
    FillLight->SetIntensity(1.8f);
    FillLight->SetCastShadows(false);
    FillLight->SetForwardShadingPriority(0);
    FillLight->SetVolumetricScatteringIntensity(0.18f);

    Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
    Atmosphere->SetupAttachment(SceneRoot);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->SetIntensity(1.42f);
    SkyLight->SetLightColor(FLinearColor(0.66f, 0.70f, 0.72f));

    Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
    Fog->SetupAttachment(SceneRoot);
    Fog->SetFogDensity(0.0085f);
    Fog->SetFogHeightFalloff(0.18f);
    Fog->SetFogInscatteringColor(FLinearColor(0.07f, 0.085f, 0.085f));
    Fog->SetStartDistance(360.0f);
    Fog->SetVolumetricFog(true);

    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
    PostProcess->SetupAttachment(SceneRoot);
    PostProcess->bUnbound = true;
    PostProcess->Settings.bOverride_VignetteIntensity = true;
    PostProcess->Settings.VignetteIntensity = 0.18f;
    PostProcess->Settings.bOverride_ColorSaturation = true;
    PostProcess->Settings.ColorSaturation = FVector4(0.88f, 0.92f, 0.88f, 1.0f);
    PostProcess->Settings.bOverride_ColorContrast = true;
    PostProcess->Settings.ColorContrast = FVector4(1.04f, 1.04f, 1.03f, 1.0f);
    PostProcess->Settings.bOverride_AutoExposureBias = true;
    PostProcess->Settings.AutoExposureBias = 1.55f;
    PostProcess->Settings.bOverride_BloomIntensity = true;
    PostProcess->Settings.BloomIntensity = 0.24f;
    PostProcess->Settings.bOverride_AmbientOcclusionIntensity = true;
    PostProcess->Settings.AmbientOcclusionIntensity = 0.72f;
    PostProcess->Settings.bOverride_AmbientOcclusionQuality = true;
    PostProcess->Settings.AmbientOcclusionQuality = 85.0f;
}

void AAshenArena::BuildTerrain()
{
    constexpr int32 Columns = 177;
    constexpr int32 Rows = 113;
    constexpr float TerrainMargin = 1'300.0f;
    constexpr float SampleOffset = 18.0f;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;
    Vertices.Reserve(Columns * Rows);
    Normals.Reserve(Columns * Rows);
    UVs.Reserve(Columns * Rows);
    VertexColors.Reserve(Columns * Rows);
    Tangents.Reserve(Columns * Rows);
    Triangles.Reserve((Columns - 1) * (Rows - 1) * 6);

    for (int32 Row = 0; Row < Rows; ++Row)
    {
        const float Y = -TerrainMargin +
                        static_cast<float>(Row) * (MapHeight + TerrainMargin * 2.0f) / static_cast<float>(Rows - 1);
        for (int32 Column = 0; Column < Columns; ++Column)
        {
            const float X = -TerrainMargin + static_cast<float>(Column) * (MapWidth + TerrainMargin * 2.0f) /
                                                 static_cast<float>(Columns - 1);
            const float Height = RenderTerrainHeightAt(X, Y);
            const float DeltaX =
                RenderTerrainHeightAt(X + SampleOffset, Y) - RenderTerrainHeightAt(X - SampleOffset, Y);
            const float DeltaY =
                RenderTerrainHeightAt(X, Y + SampleOffset) - RenderTerrainHeightAt(X, Y - SampleOffset);
            const FVector Normal(-DeltaX, -DeltaY, SampleOffset * 2.0f);

            Vertices.Emplace(X, Y, Height);
            Normals.Add(Normal.GetSafeNormal());
            UVs.Emplace(X / 520.0f, Y / 520.0f);
            VertexColors.Emplace(0.78f + FMath::Clamp(Height / 320.0f, 0.0f, 0.18f), 0.82f, 0.76f, 1.0f);
            Tangents.Emplace(FVector(1.0f, 0.0f, DeltaX / (SampleOffset * 2.0f)), false);
        }
    }

    for (int32 Row = 0; Row < Rows - 1; ++Row)
    {
        for (int32 Column = 0; Column < Columns - 1; ++Column)
        {
            const int32 A = Row * Columns + Column;
            const int32 B = A + 1;
            const int32 C = A + Columns;
            const int32 D = C + 1;
            Triangles.Append({A, C, B, B, C, D});
        }
    }

    Terrain->ClearAllMeshSections();
    Terrain->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false, false);
}

void AAshenArena::BuildRiver()
{
    constexpr int32 SegmentCount = 16;
    constexpr float RiverWidth = 290.0f;
    UStaticMesh *Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        const float Y0 = static_cast<float>(Index) * MapHeight / SegmentCount - 10.0f;
        const float Y1 = static_cast<float>(Index + 1) * MapHeight / SegmentCount + 10.0f;
        const float X0 = RiverCenterX + FMath::Sin(static_cast<float>(Index) * 0.72f) * 58.0f;
        const float X1 = RiverCenterX + FMath::Sin(static_cast<float>(Index + 1) * 0.72f) * 58.0f;
        const FVector2D WaterDelta(X1 - X0, Y1 - Y0);
        const float WaterLength = WaterDelta.Size();
        const float WaterYaw = FMath::RadiansToDegrees(FMath::Atan2(WaterDelta.Y, WaterDelta.X));
        UStaticMeshComponent *WaterSegment =
            CreateDefaultSubobject<UStaticMeshComponent>(FName(*FString::Printf(TEXT("WaterSegment_%02d"), Index)));
        WaterSegment->SetupAttachment(SceneRoot);
        WaterSegment->SetMobility(EComponentMobility::Static);
        WaterSegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WaterSegment->SetCastShadow(false);
        WaterSegment->SetStaticMesh(Plane);
        WaterSegment->SetRelativeTransform(FTransform(FRotator(0.0f, WaterYaw, 0.0f),
                                                      FVector((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, 4.0f),
                                                      FVector(WaterLength / 100.0f, RiverWidth / 100.0f, 1.0f)));
        WaterSegments.Add(WaterSegment);

        const FVector2D SegmentDirection = FVector2D(X1 - X0, Y1 - Y0).GetSafeNormal();
        const FVector2D SegmentNormal(-SegmentDirection.Y, SegmentDirection.X);
        const FVector2D Direction = SegmentDirection;
        const FVector2D Normal = SegmentNormal;
        for (int32 Bank = -1; Bank <= 1; Bank += 2)
        {
            for (int32 Cluster = 0; Cluster < 3; ++Cluster)
            {
                const float Along = 0.18f + static_cast<float>(Cluster) * 0.32f;
                const FVector2D BankPoint =
                    FVector2D(X0, Y0) + FVector2D(X1 - X0, Y1 - Y0) * Along +
                    Normal * ((RiverWidth * 0.54f + Cluster * 11.0f) * static_cast<float>(Bank));
                const float Yaw = static_cast<float>((Index * 37 + Cluster * 29 + Bank * 11) % 180);
                const float BankHeight = TerrainHeightAt(BankPoint.X, BankPoint.Y);
                RiverBanks->AddInstance(
                    FTransform(FRotator(static_cast<float>((Cluster - 1) * 7), Yaw, static_cast<float>(Bank * 4)),
                               FVector(BankPoint.X, BankPoint.Y, BankHeight + 13.0f),
                               FVector(0.64f + Cluster * 0.16f, 0.27f + (Index % 3) * 0.05f, 0.22f + Cluster * 0.03f)));

                if ((Index + Cluster) % 2 == 0)
                {
                    const FVector2D ReedPoint = BankPoint - Normal * (24.0f * static_cast<float>(Bank));
                    Reeds->AddInstance(
                        FTransform(FRotator(0.0f, Yaw + 31.0f, static_cast<float>(Bank * 5)),
                                   FVector(ReedPoint.X, ReedPoint.Y, TerrainHeightAt(ReedPoint.X, ReedPoint.Y) + 17.0f),
                                   FVector(0.10f, 0.10f, 0.36f + static_cast<float>((Index + Cluster) % 3) * 0.08f)));
                }
            }
        }
    }

    // The central cursed-iron node sits on a raised ritual island.
    RiverBanks->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(RiverCenterX, Ashen::WorldLayout::CenterY, 9.0f),
                   FVector(2.15f, 1.55f, 0.18f)));
}

void AAshenArena::BuildRoadsAndBridges()
{
    auto AddRoad = [this](const TArray<FVector2D> &Points, const float Width)
    {
        for (int32 Index = 1; Index < Points.Num(); ++Index)
        {
            const FVector2D Direction = (Points[Index] - Points[Index - 1]).GetSafeNormal();
            const FVector2D Normal(-Direction.Y, Direction.X);
            AddFlatSegment(Roadbed, Points[Index - 1], Points[Index], Width, 4.0f, 4.5f);
            AddFlatSegment(RoadStones, Points[Index - 1], Points[Index], Width * 0.58f, 2.0f, 7.0f);
            for (const float Side : {-1.0f, 1.0f})
            {
                const FVector2D Offset = Normal * Side * Width * 0.18f;
                AddFlatSegment(RoadRuts, Points[Index - 1] + Offset, Points[Index] + Offset, 5.0f, 1.0f, 8.4f);
            }
        }
    };
    AddRoad(DirectRoute(), 178.0f);
    AddRoad(NorthRoute(), 116.0f);
    AddRoad(SouthRoute(), 116.0f);

    for (const float BridgeY : {Ashen::WorldLayout::NorthCrossingY, Ashen::WorldLayout::SouthCrossingY})
    {
        for (int32 Plank = -6; Plank <= 6; ++Plank)
        {
            const float X = RiverCenterX + static_cast<float>(Plank) * 23.0f;
            BridgeTimbers->AddInstance(
                FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(X, BridgeY, 18.0f), FVector(0.205f, 1.62f, 0.12f)));
        }
        for (const float RailOffset : {-92.0f, 92.0f})
        {
            BridgeIron->AddInstance(FTransform(FRotator::ZeroRotator,
                                               FVector(RiverCenterX, BridgeY + RailOffset, 36.0f),
                                               FVector(3.35f, 0.075f, 0.11f)));
            for (const float PostOffset : {-135.0f, 0.0f, 135.0f})
            {
                BridgeIron->AddInstance(FTransform(FRotator::ZeroRotator,
                                                   FVector(RiverCenterX + PostOffset, BridgeY + RailOffset, 56.0f),
                                                   FVector(0.07f, 0.07f, 0.52f)));
            }
        }
    }

    // A narrow ritual ford keeps the central iron island contestable and mirrors the navigation portal.
    for (int32 Stone = -6; Stone <= 6; ++Stone)
    {
        const float Offset = static_cast<float>(Stone);
        RoadStones->AddInstance(
            FTransform(FRotator(0.0f, static_cast<float>((Stone * 17) % 13), 0.0f),
                       FVector(RiverCenterX + Offset * 25.0f,
                               Ashen::WorldLayout::CentralCrossingY + static_cast<float>(Stone % 2) * 9.0f, 12.0f),
                       FVector(0.22f, 0.66f + static_cast<float>((Stone + 6) % 3) * 0.08f, 0.10f)));
    }
}

void AAshenArena::BuildFortifications()
{
    constexpr float HumanX = Ashen::WorldLayout::HumanBaseX;
    constexpr float BaseY = Ashen::WorldLayout::CenterY;
    for (const float Y : {1'110.0f, 1'690.0f})
    {
        HumanWalls->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(HumanX - 15.0f, Y, 62.0f), FVector(4.8f, 0.28f, 1.22f)));
        HumanFoundations->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(HumanX - 15.0f, Y, 19.0f), FVector(5.02f, 0.38f, 0.38f)));
        HumanTrim->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(HumanX - 15.0f, Y, 126.0f), FVector(4.9f, 0.34f, 0.10f)));
    }
    HumanWalls->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(330.0f, BaseY, 62.0f), FVector(0.28f, 5.55f, 1.22f)));
    HumanFoundations->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(330.0f, BaseY, 19.0f), FVector(0.38f, 5.72f, 0.38f)));
    HumanTrim->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(330.0f, BaseY, 126.0f), FVector(0.34f, 5.64f, 0.10f)));
    for (const float Y : {1'175.0f, 1'625.0f})
    {
        HumanWalls->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(870.0f, Y, 62.0f), FVector(0.28f, 1.35f, 1.22f)));
        HumanFoundations->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(870.0f, Y, 19.0f), FVector(0.38f, 1.48f, 0.38f)));
    }
    for (const FVector2D Corner : {FVector2D(330.0f, 1'110.0f), FVector2D(330.0f, 1'690.0f),
                                   FVector2D(870.0f, 1'110.0f), FVector2D(870.0f, 1'690.0f)})
    {
        HumanTowers->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(Corner.X, Corner.Y, 95.0f), FVector(0.58f, 0.58f, 1.9f)));
        HumanRoofs->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(Corner.X, Corner.Y, 222.0f), FVector(0.78f, 0.78f, 0.82f)));
        HumanFoundations->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(Corner.X, Corner.Y, 22.0f), FVector(0.72f, 0.72f, 0.44f)));
        HumanTrim->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(Corner.X, Corner.Y, 158.0f), FVector(0.72f, 0.72f, 0.12f)));
    }
    for (int32 Crenel = 0; Crenel < 9; ++Crenel)
    {
        const float X = 210.0f + static_cast<float>(Crenel) * 96.0f;
        for (const float Y : {1'110.0f, 1'690.0f})
        {
            HumanWalls->AddInstance(
                FTransform(FRotator::ZeroRotator, FVector(X, Y, 137.0f), FVector(0.24f, 0.36f, 0.28f)));
        }
    }

    for (const float GateY : {1'275.0f, 1'525.0f})
    {
        HumanTowers->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(870.0f, GateY, 102.0f), FVector(0.49f, 0.49f, 2.04f)));
        HumanRoofs->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(870.0f, GateY, 232.0f), FVector(0.66f, 0.66f, 0.74f)));
        HumanTrim->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(870.0f, GateY, 167.0f), FVector(0.61f, 0.61f, 0.11f)));
        HumanBanners->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(902.0f, GateY, 136.0f), FVector(0.035f, 0.28f, 0.62f)));
        BrazierBowls->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(895.0f, GateY, 72.0f), FVector(0.23f, 0.23f, 0.14f)));
        EmberCores->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(895.0f, GateY, 88.0f), FVector(0.14f, 0.14f, 0.18f)));
    }
    HumanWalls->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(870.0f, BaseY, 184.0f), FVector(0.33f, 1.02f, 0.18f)));
    HumanTrim->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(870.0f, BaseY, 171.0f), FVector(0.38f, 1.12f, 0.10f)));

    const FVector2D MonsterBase(Ashen::WorldLayout::MonsterBaseX, BaseY);
    for (int32 Index = 0; Index < 18; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / 18.0f;
        const float Radius = Index % 2 == 0 ? 320.0f : 286.0f;
        const FVector Position(MonsterBase.X + FMath::Cos(Angle) * Radius, MonsterBase.Y + FMath::Sin(Angle) * Radius,
                               62.0f + static_cast<float>(Index % 3) * 8.0f);
        const float Facing = FMath::RadiansToDegrees(Angle) + 90.0f;
        BonePalisade->AddInstance(FTransform(FRotator(-8.0f, Facing, 0.0f), Position,
                                             FVector(0.26f, 0.26f, 1.35f + static_cast<float>(Index % 4) * 0.18f)));
    }
    for (int32 Index = 0; Index < 7; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / 7.0f;
        const FVector Position(MonsterBase.X + FMath::Cos(Angle) * 210.0f, MonsterBase.Y + FMath::Sin(Angle) * 205.0f,
                               46.0f);
        MonsterMasses->AddInstance(
            FTransform(FRotator(0.0f, Index * 31.0f, 0.0f), Position, FVector(0.9f, 0.62f, 0.48f)));
        MonsterSpikes->AddInstance(FTransform(FRotator(-12.0f, Index * 51.0f, 0.0f),
                                              Position + FVector(0.0f, 0.0f, 100.0f), FVector(0.32f, 0.32f, 1.28f)));
        const FVector RibTop(MonsterBase.X - 38.0f + FMath::Cos(Angle) * 92.0f,
                             MonsterBase.Y + FMath::Sin(Angle) * 78.0f, 176.0f + static_cast<float>(Index % 2) * 24.0f);
        AddCylinderBetween(MonsterRibs, Position + FVector(0.0f, 0.0f, 34.0f), RibTop, 8.0f);
        MonsterSinew->AddInstance(FTransform(FRotator(0.0f, Index * 23.0f, 0.0f), RibTop - FVector(0.0f, 0.0f, 23.0f),
                                             FVector(0.42f, 0.30f, 0.28f)));
    }
    MonsterSinew->AddInstance(FTransform(FRotator::ZeroRotator, FVector(MonsterBase.X - 35.0f, MonsterBase.Y, 132.0f),
                                         FVector(1.18f, 0.86f, 0.72f)));
    for (const float HearthY : {1'290.0f, 1'510.0f})
    {
        BrazierBowls->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(3'905.0f, HearthY, 65.0f), FVector(0.28f, 0.28f, 0.18f)));
        EmberCores->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(3'905.0f, HearthY, 84.0f), FVector(0.17f, 0.17f, 0.22f)));
    }
}

void AAshenArena::BuildVegetation()
{
    FRandomStream Random(0xA51E2026);

    for (int32 Index = 0; Index < 205; ++Index)
    {
        const FVector2D Point(Random.FRandRange(90.0f, MapWidth - 90.0f), Random.FRandRange(90.0f, MapHeight - 90.0f));
        if (IsInGameplayClearing(Point) || FMath::Abs(Point.X - RiverCenterX) < 230.0f)
        {
            continue;
        }

        const float Height = Random.FRandRange(125.0f, 235.0f);
        const float TrunkRadius = Random.FRandRange(9.0f, 16.0f);
        const float GroundHeight = TerrainHeightAt(Point.X, Point.Y);
        TreeTrunks->AddInstance(
            FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), Random.FRandRange(-5.0f, 5.0f)),
                       FVector(Point.X, Point.Y, GroundHeight + Height * 0.5f),
                       FVector(TrunkRadius / 50.0f, TrunkRadius / 50.0f, Height / 100.0f)));

        const bool bDeadTree = Index % 5 == 0;
        if (!bDeadTree)
        {
            const bool bBroadleaf = Index % 7 == 0;
            if (bBroadleaf)
            {
                const FVector CrownCenter(Point.X, Point.Y, GroundHeight + Height + 24.0f);
                for (int32 Lobe = 0; Lobe < 4; ++Lobe)
                {
                    const float Angle = static_cast<float>(Lobe) * PI * 0.5f + Random.FRandRange(-0.2f, 0.2f);
                    TreeCanopies->AddInstance(
                        FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), Random.FRandRange(-9.0f, 9.0f)),
                                   CrownCenter + FVector(FMath::Cos(Angle) * 34.0f, FMath::Sin(Angle) * 31.0f,
                                                         static_cast<float>(Lobe % 2) * 17.0f),
                                   FVector(Random.FRandRange(0.62f, 0.88f), Random.FRandRange(0.56f, 0.82f),
                                           Random.FRandRange(0.48f, 0.72f))));
                }
            }
            else
            {
                const float CrownWidth = Random.FRandRange(0.76f, 1.12f);
                TreeCrownsShadow->AddInstance(
                    FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), 0.0f),
                               FVector(Point.X, Point.Y, GroundHeight + Height * 0.73f + 34.0f),
                               FVector(CrownWidth * 1.14f, CrownWidth, Random.FRandRange(1.15f, 1.46f))));
                TreeCrowns->AddInstance(
                    FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), 0.0f),
                               FVector(Point.X + Random.FRandRange(-5.0f, 5.0f),
                                       Point.Y + Random.FRandRange(-5.0f, 5.0f), GroundHeight + Height + 20.0f),
                               FVector(CrownWidth * 0.88f, CrownWidth * 0.82f, Random.FRandRange(1.22f, 1.62f))));
                TreeCrowns->AddInstance(
                    FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), 0.0f),
                               FVector(Point.X, Point.Y, GroundHeight + Height + 78.0f),
                               FVector(CrownWidth * 0.58f, CrownWidth * 0.56f, Random.FRandRange(0.72f, 1.02f))));
            }
        }
        else
        {
            const FVector Crown(Point.X, Point.Y, GroundHeight + Height * 0.78f);
            AddCylinderBetween(DeadBranches, Crown, Crown + FVector(52.0f, 22.0f, 55.0f), TrunkRadius * 0.42f);
            AddCylinderBetween(DeadBranches, Crown, Crown + FVector(-44.0f, -30.0f, 42.0f), TrunkRadius * 0.36f);
            AddCylinderBetween(DeadBranches, Crown + FVector(0.0f, 0.0f, 24.0f), Crown + FVector(16.0f, -48.0f, 78.0f),
                               TrunkRadius * 0.28f);
        }
    }

    // Gravewood is a dense sightline landmark, while the authored route remains clear enough for unit reading.
    for (int32 Index = 0; Index < 165; ++Index)
    {
        const FVector2D Point(Random.FRandRange(2'820.0f, 4'120.0f), Random.FRandRange(1'560.0f, 2'660.0f));
        const float ForestMask = FMath::Square((Point.X - 3'430.0f) / 720.0f) +
                                 FMath::Square((Point.Y - 2'080.0f) / 590.0f);
        if (ForestMask > 1.0f || IsInGameplayClearing(Point))
        {
            continue;
        }

        const float GroundHeight = TerrainHeightAt(Point.X, Point.Y);
        const float Height = Random.FRandRange(165.0f, 285.0f);
        const float TrunkRadius = Random.FRandRange(11.0f, 19.0f);
        TreeTrunks->AddInstance(
            FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), Random.FRandRange(-7.0f, 7.0f)),
                       FVector(Point.X, Point.Y, GroundHeight + Height * 0.5f),
                       FVector(TrunkRadius / 50.0f, TrunkRadius / 50.0f, Height / 100.0f)));

        if (Index % 4 == 0)
        {
            const FVector Crown(Point.X, Point.Y, GroundHeight + Height * 0.72f);
            AddCylinderBetween(DeadBranches, Crown, Crown + FVector(62.0f, 28.0f, 66.0f), TrunkRadius * 0.42f);
            AddCylinderBetween(DeadBranches, Crown, Crown + FVector(-54.0f, -36.0f, 51.0f), TrunkRadius * 0.34f);
        }
        else
        {
            const float CrownWidth = Random.FRandRange(0.88f, 1.32f);
            TreeCrownsShadow->AddInstance(
                FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), 0.0f),
                           FVector(Point.X, Point.Y, GroundHeight + Height * 0.72f + 35.0f),
                           FVector(CrownWidth * 1.20f, CrownWidth * 1.08f, Random.FRandRange(1.28f, 1.70f))));
            TreeCrowns->AddInstance(
                FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), 0.0f),
                           FVector(Point.X, Point.Y, GroundHeight + Height + 38.0f),
                           FVector(CrownWidth, CrownWidth * 0.92f, Random.FRandRange(1.42f, 1.92f))));
        }

        if (Index % 3 == 0)
        {
            const FVector Root(Point.X, Point.Y, GroundHeight + 8.0f);
            for (int32 RootIndex = 0; RootIndex < 3; ++RootIndex)
            {
                const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
                const FVector End = Root + FVector(FMath::Cos(Angle) * Random.FRandRange(48.0f, 82.0f),
                                                   FMath::Sin(Angle) * Random.FRandRange(48.0f, 82.0f), -3.0f);
                AddCylinderBetween(ForestRoots, Root, End, Random.FRandRange(4.0f, 7.0f));
            }
        }
    }

    for (int32 Index = 0; Index < 520; ++Index)
    {
        const FVector2D Point(Random.FRandRange(45.0f, MapWidth - 45.0f), Random.FRandRange(45.0f, MapHeight - 45.0f));
        if (IsInGameplayClearing(Point) || FMath::Abs(Point.X - RiverCenterX) < 175.0f)
        {
            continue;
        }
        GrassTufts->AddInstance(
            FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), Random.FRandRange(-7.0f, 7.0f)),
                       FVector(Point.X, Point.Y, TerrainHeightAt(Point.X, Point.Y) + 10.0f),
                       FVector(Random.FRandRange(0.055f, 0.11f), Random.FRandRange(0.055f, 0.11f),
                               Random.FRandRange(0.16f, 0.34f))));
    }

    for (int32 Index = 0; Index < 70; ++Index)
    {
        const FVector2D Point(Random.FRandRange(60.0f, MapWidth - 60.0f), Random.FRandRange(60.0f, MapHeight - 60.0f));
        if (IsInGameplayClearing(Point))
        {
            continue;
        }
        Rocks->AddInstance(
            FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), Random.FRandRange(-18.0f, 18.0f)),
                       FVector(Point.X, Point.Y, TerrainHeightAt(Point.X, Point.Y) + Random.FRandRange(9.0f, 20.0f)),
                       FVector(Random.FRandRange(0.28f, 0.72f), Random.FRandRange(0.22f, 0.58f),
                               Random.FRandRange(0.18f, 0.42f))));
    }
}

void AAshenArena::BuildLandmarks()
{
    FRandomStream Random(0xB1AC4F0D);
    for (int32 Index = 0; Index < 92; ++Index)
    {
        const FVector2D Point(Random.FRandRange(880.0f, 2'040.0f), Random.FRandRange(390.0f, 1'300.0f));
        const float MountainMask = FMath::Square((Point.X - 1'440.0f) / 660.0f) +
                                   FMath::Square((Point.Y - 820.0f) / 520.0f);
        if (MountainMask > 1.0f || IsInGameplayClearing(Point))
        {
            continue;
        }

        const float Scale = Random.FRandRange(0.55f, 1.35f);
        MountainRocks->AddInstance(
            FTransform(FRotator(Random.FRandRange(-18.0f, 18.0f), Random.FRandRange(0.0f, 180.0f),
                                Random.FRandRange(-13.0f, 13.0f)),
                       FVector(Point.X, Point.Y, TerrainHeightAt(Point.X, Point.Y) + Scale * 34.0f),
                       FVector(Scale, Scale * Random.FRandRange(0.55f, 0.90f), Scale * Random.FRandRange(0.42f, 0.82f))));
    }

    // Two concealed iron adits make the mountain route valuable without opening a free path into either base.
    for (const float MineX : {1'240.0f, 1'580.0f})
    {
        constexpr float MineY = 525.0f;
        const float GroundHeight = TerrainHeightAt(MineX, MineY);
        MineMouths->AddInstance(FTransform(FRotator::ZeroRotator, FVector(MineX, MineY, GroundHeight + 48.0f),
                                           FVector(0.72f, 0.18f, 0.92f)));
        const FVector Left(MineX - 48.0f, MineY - 13.0f, GroundHeight + 8.0f);
        const FVector Right(MineX + 48.0f, MineY - 13.0f, GroundHeight + 8.0f);
        AddCylinderBetween(MineTimbers, Left, Left + FVector(0.0f, 0.0f, 112.0f), 8.0f);
        AddCylinderBetween(MineTimbers, Right, Right + FVector(0.0f, 0.0f, 112.0f), 8.0f);
        AddCylinderBetween(MineTimbers, Left + FVector(0.0f, 0.0f, 106.0f),
                           Right + FVector(0.0f, 0.0f, 106.0f), 9.0f);
        for (int32 RockIndex = 0; RockIndex < 6; ++RockIndex)
        {
            const float Side = RockIndex % 2 == 0 ? -1.0f : 1.0f;
            MountainRocks->AddInstance(
                FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 180.0f), Random.FRandRange(-18.0f, 18.0f)),
                           FVector(MineX + Side * Random.FRandRange(62.0f, 108.0f),
                                   MineY + Random.FRandRange(-15.0f, 55.0f), GroundHeight + Random.FRandRange(18.0f, 36.0f)),
                           FVector(Random.FRandRange(0.38f, 0.78f), Random.FRandRange(0.30f, 0.62f),
                                   Random.FRandRange(0.28f, 0.56f))));
        }
    }

    // Gravewood's spirit caches are the rotational counterpart to the mountain mines.
    for (const FVector2D Cache : {FVector2D(3'220.0f, 2'440.0f), FVector2D(3'560.0f, 2'460.0f)})
    {
        for (int32 Index = 0; Index < 6; ++Index)
        {
            const float Angle = 2.0f * PI * static_cast<float>(Index) / 6.0f;
            const FVector Position(Cache.X + FMath::Cos(Angle) * 62.0f, Cache.Y + FMath::Sin(Angle) * 48.0f,
                                   TerrainHeightAt(Cache.X, Cache.Y) + 35.0f);
            RitualStones->AddInstance(
                FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f), Position, FVector(0.16f, 0.16f, 0.70f)));
        }
    }

    for (int32 Index = 0; Index < 12; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / 12.0f;
        const FVector Position(RiverCenterX + FMath::Cos(Angle) * 205.0f,
                               Ashen::WorldLayout::CenterY + FMath::Sin(Angle) * 135.0f, 48.0f);
        RitualStones->AddInstance(
            FTransform({0.0f, FMath::RadiansToDegrees(Angle), 0.0f}, Position, {0.25f, 0.25f, 0.95f}));
    }

    for (const float Yaw : {0.0f, 120.0f, 240.0f})
    {
        const FVector Direction = FRotator(0.0f, Yaw, 0.0f).RotateVector(FVector::ForwardVector);
        const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction);
        const FVector Center(RiverCenterX, Ashen::WorldLayout::CenterY, 18.0f);
        const FVector Left = Center - Side * 106.0f + Direction * 24.0f;
        const FVector Right = Center + Side * 106.0f + Direction * 24.0f;
        AddCylinderBetween(MythicArches, Left, Left + FVector(0.0f, 0.0f, 116.0f), 9.0f);
        AddCylinderBetween(MythicArches, Right, Right + FVector(0.0f, 0.0f, 116.0f), 9.0f);
        AddCylinderBetween(MythicArches, Left + FVector(0.0f, 0.0f, 116.0f), Right + FVector(0.0f, 0.0f, 116.0f), 8.0f);
    }
    BrazierBowls->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(RiverCenterX, Ashen::WorldLayout::CenterY, 44.0f),
                   FVector(0.36f, 0.36f, 0.20f)));
    EmberCores->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(RiverCenterX, Ashen::WorldLayout::CenterY, 66.0f),
                   FVector(0.19f, 0.19f, 0.25f)));
}

void AAshenArena::BeginPlay()
{
    Super::BeginPlay();

    BuildTerrain();
    SkyLight->RecaptureSky();

    const auto Moor = SurfaceStyle({0.050f, 0.074f, 0.050f}, {0.105f, 0.118f, 0.078f}, {0.160f, 0.176f, 0.105f}, 0.96f,
                                   510.0f, 88.0f, 0.14f, 0.18f);
    const auto MoorPatch = SurfaceStyle({0.074f, 0.105f, 0.064f}, {0.13f, 0.15f, 0.085f}, {0.19f, 0.20f, 0.11f}, 0.98f,
                                        260.0f, 54.0f, 0.17f, 0.14f);
    const auto Mud = SurfaceStyle({0.095f, 0.066f, 0.040f}, {0.17f, 0.12f, 0.070f}, {0.25f, 0.205f, 0.135f}, 0.94f,
                                  220.0f, 42.0f, 0.22f, 0.18f);
    const auto RoadStone = SurfaceStyle({0.145f, 0.145f, 0.128f}, {0.25f, 0.235f, 0.20f}, {0.35f, 0.32f, 0.255f}, 0.91f,
                                        120.0f, 30.0f, 0.22f, 0.24f);
    const auto WetStone = SurfaceStyle({0.075f, 0.086f, 0.080f}, {0.145f, 0.15f, 0.135f}, {0.23f, 0.23f, 0.195f}, 0.80f,
                                       135.0f, 32.0f, 0.19f, 0.34f);
    const auto WeatheredWood = SurfaceStyle({0.090f, 0.047f, 0.024f}, {0.17f, 0.095f, 0.045f}, {0.28f, 0.18f, 0.095f},
                                            0.86f, 90.0f, 21.0f, 0.18f, 0.22f);
    const auto DarkIron = SurfaceStyle({0.055f, 0.060f, 0.058f}, {0.14f, 0.145f, 0.135f}, {0.24f, 0.23f, 0.20f}, 0.44f,
                                       95.0f, 24.0f, 0.12f, 0.58f);
    const auto MineDark = SurfaceStyle({0.004f, 0.005f, 0.005f}, {0.012f, 0.014f, 0.013f}, {0.025f, 0.026f, 0.023f},
                                       0.98f, 80.0f, 19.0f, 0.08f, 0.08f);
    const auto Bark = SurfaceStyle({0.050f, 0.028f, 0.015f}, {0.105f, 0.060f, 0.030f}, {0.18f, 0.115f, 0.055f}, 0.98f,
                                   75.0f, 18.0f, 0.20f, 0.14f);
    const auto Pine = SurfaceStyle({0.020f, 0.058f, 0.030f}, {0.045f, 0.105f, 0.052f}, {0.10f, 0.16f, 0.085f}, 0.99f,
                                   105.0f, 25.0f, 0.16f, 0.12f);
    const auto PineShadow = SurfaceStyle({0.012f, 0.030f, 0.018f}, {0.025f, 0.067f, 0.034f}, {0.055f, 0.105f, 0.055f},
                                         0.99f, 115.0f, 28.0f, 0.13f, 0.10f);
    const auto HumanStone = SurfaceStyle({0.16f, 0.17f, 0.16f}, {0.27f, 0.27f, 0.235f}, {0.38f, 0.36f, 0.29f}, 0.90f,
                                         130.0f, 29.0f, 0.19f, 0.25f);
    const auto FoundationStone = SurfaceStyle({0.075f, 0.078f, 0.073f}, {0.15f, 0.15f, 0.135f}, {0.23f, 0.22f, 0.18f},
                                              0.95f, 110.0f, 27.0f, 0.22f, 0.18f);
    const auto HumanRoof = SurfaceStyle({0.14f, 0.025f, 0.018f}, {0.26f, 0.050f, 0.025f}, {0.40f, 0.095f, 0.040f},
                                        0.76f, 95.0f, 22.0f, 0.16f, 0.30f);
    const auto Flesh = SurfaceStyle({0.09f, 0.008f, 0.014f}, {0.19f, 0.018f, 0.025f}, {0.34f, 0.035f, 0.042f}, 0.68f,
                                    115.0f, 24.0f, 0.24f, 0.30f);
    const auto Bone = SurfaceStyle({0.29f, 0.25f, 0.18f}, {0.45f, 0.39f, 0.28f}, {0.61f, 0.54f, 0.40f}, 0.88f, 100.0f,
                                   22.0f, 0.17f, 0.20f);
    const auto MythicStone = SurfaceStyle({0.030f, 0.042f, 0.042f}, {0.075f, 0.105f, 0.095f}, {0.13f, 0.205f, 0.17f},
                                          0.82f, 145.0f, 31.0f, 0.19f, 0.36f);

    Ashen::Materials::ApplySurface(Terrain, this, Moor);
    Ashen::Materials::ApplySurface(Roadbed, this, Mud);
    Ashen::Materials::ApplySurface(RoadStones, this, RoadStone);
    Ashen::Materials::ApplySurface(
        RoadRuts, this,
        SurfaceStyle({0.035f, 0.022f, 0.016f}, {0.07f, 0.045f, 0.028f}, {0.11f, 0.075f, 0.045f}, 0.72f));
    Ashen::Materials::ApplySurface(RiverBanks, this, WetStone);
    Ashen::Materials::ApplySurface(Reeds, this, Pine);
    Ashen::Materials::ApplySurface(BridgeTimbers, this, WeatheredWood);
    Ashen::Materials::ApplySurface(BridgeIron, this, DarkIron);
    Ashen::Materials::ApplySurface(TreeTrunks, this, Bark);
    Ashen::Materials::ApplySurface(TreeCrowns, this, Pine);
    Ashen::Materials::ApplySurface(TreeCrownsShadow, this, PineShadow);
    Ashen::Materials::ApplySurface(TreeCanopies, this, Pine);
    Ashen::Materials::ApplySurface(DeadBranches, this, Bark);
    Ashen::Materials::ApplySurface(GrassTufts, this, MoorPatch);
    Ashen::Materials::ApplySurface(Rocks, this, WetStone);
    Ashen::Materials::ApplySurface(MountainRocks, this, FoundationStone);
    Ashen::Materials::ApplySurface(MineMouths, this, MineDark);
    Ashen::Materials::ApplySurface(MineTimbers, this, WeatheredWood);
    Ashen::Materials::ApplySurface(ForestRoots, this, Bark);
    Ashen::Materials::ApplySurface(HumanWalls, this, HumanStone);
    Ashen::Materials::ApplySurface(HumanTowers, this, HumanStone);
    Ashen::Materials::ApplySurface(HumanRoofs, this, HumanRoof);
    Ashen::Materials::ApplySurface(HumanFoundations, this, FoundationStone);
    Ashen::Materials::ApplySurface(HumanTrim, this, DarkIron);
    Ashen::Materials::ApplySurface(HumanBanners, this, HumanRoof);
    Ashen::Materials::ApplySurface(MonsterMasses, this, Flesh);
    Ashen::Materials::ApplySurface(MonsterSpikes, this, Flesh);
    Ashen::Materials::ApplySurface(MonsterRibs, this, Bone);
    Ashen::Materials::ApplySurface(MonsterSinew, this, Flesh);
    Ashen::Materials::ApplySurface(BonePalisade, this, Bone);
    Ashen::Materials::ApplySurface(RitualStones, this, MythicStone);
    Ashen::Materials::ApplySurface(MythicArches, this, MythicStone);
    Ashen::Materials::ApplySurface(BrazierBowls, this, DarkIron);
    Ashen::Materials::Apply(EmberCores, this, FLinearColor(0.78f, 0.09f, 0.025f), 0.16f);

    for (UStaticMeshComponent *WaterSegment : WaterSegments)
    {
        Ashen::Materials::ApplyWater(WaterSegment, this, FLinearColor(0.035f, 0.20f, 0.19f),
                                     FLinearColor(0.004f, 0.036f, 0.045f), 0.80f);
    }
}
