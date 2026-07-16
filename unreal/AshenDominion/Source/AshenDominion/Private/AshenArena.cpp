#include "AshenArena.h"

#include "AshenMaterials.h"

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
constexpr float MapWidth = 3'840.0f;
constexpr float MapHeight = 2'160.0f;
constexpr float RiverCenterX = MapWidth * 0.5f;

float SmoothRange(const float Minimum, const float Maximum, const float Value)
{
    const float Alpha =
        FMath::Clamp((Value - Minimum) / FMath::Max(Maximum - Minimum, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
    return Alpha * Alpha * (3.0f - 2.0f * Alpha);
}

float TerrainHeightAt(const float X, const float Y)
{
    const float ClampedX = FMath::Clamp(X, 0.0f, MapWidth);
    const float ClampedY = FMath::Clamp(Y, 0.0f, MapHeight);
    const float LaneAlpha = FMath::Clamp((ClampedX - 520.0f) / 2'800.0f, 0.0f, 1.0f);
    const float LaneArc = FMath::Sin(LaneAlpha * PI) * 400.0f;
    const float LaneDistance =
        FMath::Min(FMath::Abs(ClampedY - (1'080.0f - LaneArc)), FMath::Abs(ClampedY - (1'080.0f + LaneArc)));
    const float HumanBaseDistance = FVector2D::Distance({ClampedX, ClampedY}, {500.0f, 1'080.0f});
    const float MonsterBaseDistance = FVector2D::Distance({ClampedX, ClampedY}, {3'340.0f, 1'080.0f});
    const float ClearingMask = FMath::Max(1.0f - SmoothRange(250.0f, 480.0f, LaneDistance),
                                          FMath::Max(1.0f - SmoothRange(370.0f, 590.0f, HumanBaseDistance),
                                                     1.0f - SmoothRange(370.0f, 590.0f, MonsterBaseDistance)));

    const float BorderDistance =
        FMath::Min(FMath::Min(ClampedX, MapWidth - ClampedX), FMath::Min(ClampedY, MapHeight - ClampedY));
    const float EdgeRise = (1.0f - SmoothRange(80.0f, 470.0f, BorderDistance)) * 118.0f;
    const float BroadUndulation = FMath::Sin(ClampedX * 0.0037f + ClampedY * 0.0021f) * 15.0f +
                                  FMath::Sin(ClampedX * 0.0081f - ClampedY * 0.0052f) * 7.5f;
    const float NorthRidge =
        FMath::Exp(-FVector2D::DistSquared({ClampedX, ClampedY}, {1'080.0f, 170.0f}) / 520'000.0f) * 72.0f;
    const float SouthRidge =
        FMath::Exp(-FVector2D::DistSquared({ClampedX, ClampedY}, {2'760.0f, 1'990.0f}) / 470'000.0f) * 88.0f;

    const float RiverX = RiverCenterX + FMath::Sin((ClampedY / MapHeight) * 8.64f) * 58.0f;
    const float RiverDistance = FMath::Abs(ClampedX - RiverX);
    const float RiverCut = (1.0f - SmoothRange(95.0f, 250.0f, RiverDistance)) * 34.0f;
    const float Terrain = EdgeRise + (BroadUndulation + NorthRidge + SouthRidge) * (1.0f - ClearingMask * 0.9f);
    return Terrain - RiverCut;
}

float RenderTerrainHeightAt(const float X, const float Y)
{
    const float OutsideDistance = FMath::Max(FMath::Max(-X, X - MapWidth), FMath::Max(-Y, Y - MapHeight));
    return TerrainHeightAt(X, Y) + SmoothRange(0.0f, 900.0f, OutsideDistance) * 240.0f;
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
    const FVector2D HumanBase(500.0f, 1'080.0f);
    const FVector2D MonsterBase(3'340.0f, 1'080.0f);
    if (FVector2D::Distance(Point, HumanBase) < 510.0f || FVector2D::Distance(Point, MonsterBase) < 510.0f)
    {
        return true;
    }

    const bool bMainLane = Point.X > 700.0f && Point.X < 3'140.0f && FMath::Abs(Point.Y - 1'080.0f) < 215.0f;
    const bool bNorthBridgeLane = FMath::Abs(Point.Y - 680.0f) < 145.0f && FMath::Abs(Point.X - RiverCenterX) < 560.0f;
    const bool bSouthBridgeLane =
        FMath::Abs(Point.Y - 1'480.0f) < 145.0f && FMath::Abs(Point.X - RiverCenterX) < 560.0f;
    return bMainLane || bNorthBridgeLane || bSouthBridgeLane;
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
    BoundaryMonoliths = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BoundaryMonoliths"));
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
    ConfigureInstances(BoundaryMonoliths, SceneRoot, Cube);
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
        {795.0f, 960.0f, 102.0f},    {795.0f, 1'200.0f, 102.0f},      {3'045.0f, 970.0f, 92.0f},
        {3'045.0f, 1'190.0f, 92.0f}, {RiverCenterX, 1'080.0f, 84.0f},
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
    constexpr int32 Columns = 149;
    constexpr int32 Rows = 109;
    constexpr float TerrainMargin = 1'100.0f;
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
    constexpr int32 SegmentCount = 12;
    constexpr float RiverWidth = 265.0f;
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
        FTransform(FRotator::ZeroRotator, FVector(RiverCenterX, 1'080.0f, 9.0f), FVector(2.15f, 1.55f, 0.18f)));
}

void AAshenArena::BuildRoadsAndBridges()
{
    const TArray<FVector2D> NorthRoad{
        {520.0f, 1'080.0f}, {760.0f, 1'065.0f},   {1'000.0f, 1'020.0f}, {1'220.0f, 900.0f},   {1'420.0f, 760.0f},
        {1'620.0f, 700.0f}, {1'790.0f, 680.0f},   {2'050.0f, 680.0f},   {2'240.0f, 715.0f},   {2'430.0f, 770.0f},
        {2'650.0f, 930.0f}, {2'860.0f, 1'050.0f}, {3'100.0f, 1'070.0f}, {3'320.0f, 1'080.0f},
    };
    const TArray<FVector2D> SouthRoad{
        {520.0f, 1'080.0f},   {760.0f, 1'095.0f},   {1'000.0f, 1'140.0f}, {1'220.0f, 1'260.0f}, {1'420.0f, 1'410.0f},
        {1'620.0f, 1'465.0f}, {1'790.0f, 1'480.0f}, {2'050.0f, 1'480.0f}, {2'240.0f, 1'445.0f}, {2'430.0f, 1'390.0f},
        {2'650.0f, 1'230.0f}, {2'860.0f, 1'120.0f}, {3'100.0f, 1'090.0f}, {3'320.0f, 1'080.0f},
    };

    auto AddRoad = [this](const TArray<FVector2D> &Points)
    {
        for (int32 Index = 1; Index < Points.Num(); ++Index)
        {
            const FVector2D Direction = (Points[Index] - Points[Index - 1]).GetSafeNormal();
            const FVector2D Normal(-Direction.Y, Direction.X);
            AddFlatSegment(Roadbed, Points[Index - 1], Points[Index], 132.0f, 4.0f, 4.5f);
            AddFlatSegment(RoadStones, Points[Index - 1], Points[Index], 76.0f, 2.0f, 7.0f);
            for (const float Side : {-1.0f, 1.0f})
            {
                const FVector2D Offset = Normal * Side * 23.0f;
                AddFlatSegment(RoadRuts, Points[Index - 1] + Offset, Points[Index] + Offset, 5.0f, 1.0f, 8.4f);
            }
        }
    };
    AddRoad(NorthRoad);
    AddRoad(SouthRoad);

    for (const float BridgeY : {680.0f, 1'480.0f})
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
                       FVector(RiverCenterX + Offset * 25.0f, 1'080.0f + static_cast<float>(Stone % 2) * 9.0f, 12.0f),
                       FVector(0.22f, 0.66f + static_cast<float>((Stone + 6) % 3) * 0.08f, 0.10f)));
    }
}

void AAshenArena::BuildFortifications()
{
    constexpr float HumanX = 500.0f;
    constexpr float BaseY = 1'080.0f;
    for (const float Y : {790.0f, 1'370.0f})
    {
        HumanWalls->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(HumanX - 15.0f, Y, 62.0f), FVector(4.8f, 0.28f, 1.22f)));
        HumanFoundations->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(HumanX - 15.0f, Y, 19.0f), FVector(5.02f, 0.38f, 0.38f)));
        HumanTrim->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(HumanX - 15.0f, Y, 126.0f), FVector(4.9f, 0.34f, 0.10f)));
    }
    HumanWalls->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(230.0f, BaseY, 62.0f), FVector(0.28f, 5.55f, 1.22f)));
    HumanFoundations->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(230.0f, BaseY, 19.0f), FVector(0.38f, 5.72f, 0.38f)));
    HumanTrim->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(230.0f, BaseY, 126.0f), FVector(0.34f, 5.64f, 0.10f)));
    for (const float Y : {855.0f, 1'305.0f})
    {
        HumanWalls->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(770.0f, Y, 62.0f), FVector(0.28f, 1.35f, 1.22f)));
        HumanFoundations->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(770.0f, Y, 19.0f), FVector(0.38f, 1.48f, 0.38f)));
    }
    for (const FVector2D Corner : {FVector2D(230.0f, 790.0f), FVector2D(230.0f, 1'370.0f), FVector2D(770.0f, 790.0f),
                                   FVector2D(770.0f, 1'370.0f)})
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
        const float X = 110.0f + static_cast<float>(Crenel) * 96.0f;
        for (const float Y : {790.0f, 1'370.0f})
        {
            HumanWalls->AddInstance(
                FTransform(FRotator::ZeroRotator, FVector(X, Y, 137.0f), FVector(0.24f, 0.36f, 0.28f)));
        }
    }

    for (const float GateY : {955.0f, 1'205.0f})
    {
        HumanTowers->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(770.0f, GateY, 102.0f), FVector(0.49f, 0.49f, 2.04f)));
        HumanRoofs->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(770.0f, GateY, 232.0f), FVector(0.66f, 0.66f, 0.74f)));
        HumanTrim->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(770.0f, GateY, 167.0f), FVector(0.61f, 0.61f, 0.11f)));
        HumanBanners->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(802.0f, GateY, 136.0f), FVector(0.035f, 0.28f, 0.62f)));
        BrazierBowls->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(795.0f, GateY, 72.0f), FVector(0.23f, 0.23f, 0.14f)));
        EmberCores->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(795.0f, GateY, 88.0f), FVector(0.14f, 0.14f, 0.18f)));
    }
    HumanWalls->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(770.0f, BaseY, 184.0f), FVector(0.33f, 1.02f, 0.18f)));
    HumanTrim->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(770.0f, BaseY, 171.0f), FVector(0.38f, 1.12f, 0.10f)));

    const FVector2D MonsterBase(3'340.0f, BaseY);
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
    for (const float HearthY : {970.0f, 1'190.0f})
    {
        BrazierBowls->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(3'045.0f, HearthY, 65.0f), FVector(0.28f, 0.28f, 0.18f)));
        EmberCores->AddInstance(
            FTransform(FRotator::ZeroRotator, FVector(3'045.0f, HearthY, 84.0f), FVector(0.17f, 0.17f, 0.22f)));
    }
}

void AAshenArena::BuildVegetation()
{
    FRandomStream Random(0xA51E2026);

    for (int32 Index = 0; Index < 148; ++Index)
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

    for (int32 Index = 0; Index < 360; ++Index)
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

    for (int32 Index = 0; Index < 52; ++Index)
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
    for (int32 Index = 0; Index < 16; ++Index)
    {
        const float X = 120.0f + static_cast<float>(Index) * 240.0f;
        const float HeightScale = 1.15f + static_cast<float>((Index * 7) % 5) * 0.16f;
        const FRotator Rotation(0.0f, static_cast<float>((Index * 23) % 90), 0.0f);
        BoundaryMonoliths->AddInstance(FTransform(Rotation, {X, 55.0f, TerrainHeightAt(X, 55.0f) + HeightScale * 50.0f},
                                                  {0.48f, 0.72f, HeightScale}));
        BoundaryMonoliths->AddInstance(FTransform(
            Rotation, {X, 2'105.0f, TerrainHeightAt(X, 2'105.0f) + HeightScale * 50.0f}, {0.48f, 0.72f, HeightScale}));
        if (Index % 2 == 0)
        {
            for (const float Y : {105.0f, 2'055.0f})
            {
                Rocks->AddInstance(FTransform(
                    FRotator(static_cast<float>((Index % 3) * 7), static_cast<float>(Index * 31), 0.0f),
                    FVector(X + 42.0f, Y, TerrainHeightAt(X + 42.0f, Y) + 29.0f), FVector(0.92f, 0.56f, 0.58f)));
            }
        }
    }
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float Y = 240.0f + static_cast<float>(Index) * 240.0f;
        const float HeightScale = 1.2f + static_cast<float>((Index * 3) % 4) * 0.2f;
        BoundaryMonoliths->AddInstance(FTransform({0.0f, static_cast<float>(Index * 11), 0.0f},
                                                  {55.0f, Y, TerrainHeightAt(55.0f, Y) + HeightScale * 50.0f},
                                                  {0.72f, 0.48f, HeightScale}));
        BoundaryMonoliths->AddInstance(FTransform({0.0f, static_cast<float>(Index * 11), 0.0f},
                                                  {3'785.0f, Y, TerrainHeightAt(3'785.0f, Y) + HeightScale * 50.0f},
                                                  {0.72f, 0.48f, HeightScale}));
    }

    for (int32 Index = 0; Index < 12; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / 12.0f;
        const FVector Position(RiverCenterX + FMath::Cos(Angle) * 205.0f, 1'080.0f + FMath::Sin(Angle) * 135.0f, 48.0f);
        RitualStones->AddInstance(
            FTransform({0.0f, FMath::RadiansToDegrees(Angle), 0.0f}, Position, {0.25f, 0.25f, 0.95f}));
    }

    for (const float Yaw : {0.0f, 120.0f, 240.0f})
    {
        const FVector Direction = FRotator(0.0f, Yaw, 0.0f).RotateVector(FVector::ForwardVector);
        const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction);
        const FVector Center(RiverCenterX, 1'080.0f, 18.0f);
        const FVector Left = Center - Side * 106.0f + Direction * 24.0f;
        const FVector Right = Center + Side * 106.0f + Direction * 24.0f;
        AddCylinderBetween(MythicArches, Left, Left + FVector(0.0f, 0.0f, 116.0f), 9.0f);
        AddCylinderBetween(MythicArches, Right, Right + FVector(0.0f, 0.0f, 116.0f), 9.0f);
        AddCylinderBetween(MythicArches, Left + FVector(0.0f, 0.0f, 116.0f), Right + FVector(0.0f, 0.0f, 116.0f), 8.0f);
    }
    BrazierBowls->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(RiverCenterX, 1'080.0f, 44.0f), FVector(0.36f, 0.36f, 0.20f)));
    EmberCores->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(RiverCenterX, 1'080.0f, 66.0f), FVector(0.19f, 0.19f, 0.25f)));
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
    Ashen::Materials::ApplySurface(BoundaryMonoliths, this, FoundationStone);
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
