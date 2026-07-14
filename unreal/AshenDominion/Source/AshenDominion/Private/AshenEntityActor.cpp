#include "AshenEntityActor.h"

#include "AshenMaterials.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"

AAshenEntityActor::AAshenEntityActor()
{
    PrimaryActorTick.bCanEverTick = false;

    EntityMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EntityMesh"));
    SetRootComponent(EntityMesh);
    EntityMesh->SetMobility(EComponentMobility::Movable);
    EntityMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    EntityMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    EntityMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    SelectionMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionMarker"));
    SelectionMarker->SetupAttachment(EntityMesh);
    SelectionMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SelectionMarker->SetVisibility(false);

    FactionLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FactionLight"));
    FactionLight->SetupAttachment(EntityMesh);
    FactionLight->SetCastShadows(false);
    FactionLight->SetAttenuationRadius(180.0f);
    FactionLight->SetIntensity(650.0f);
}

void AAshenEntityActor::InitializeEntity(const int32 InEntityId, const uint8 InOwnerIndex,
                                         const EAshenEntityArchetype InArchetype, const float Radius)
{
    EntityId = InEntityId;
    OwnerIndex = InOwnerIndex;
    Archetype = InArchetype;

    const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
    VisualHeight = 70.0f;
    switch (Archetype)
    {
    case EAshenEntityArchetype::Worker:
        VisualHeight = 62.0f;
        break;
    case EAshenEntityArchetype::Vanguard:
        MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
        VisualHeight = 88.0f;
        break;
    case EAshenEntityArchetype::Skirmisher:
        MeshPath = TEXT("/Engine/BasicShapes/Cone.Cone");
        VisualHeight = 76.0f;
        break;
    case EAshenEntityArchetype::Command:
        MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
        VisualHeight = 190.0f;
        break;
    case EAshenEntityArchetype::Barracks:
        MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
        VisualHeight = 120.0f;
        break;
    case EAshenEntityArchetype::Turret:
        VisualHeight = 150.0f;
        break;
    }

    if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
    {
        EntityMesh->SetStaticMesh(Mesh);
    }
    if (UStaticMesh* MarkerMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
    {
        SelectionMarker->SetStaticMesh(MarkerMesh);
    }

    const FLinearColor FactionColor = OwnerIndex == 0
                                          ? FLinearColor(0.30f, 0.075f, 0.012f)
                                          : FLinearColor(0.26f, 0.008f, 0.014f);
    Ashen::Materials::Apply(EntityMesh, this, FactionColor, 0.62f);
    Ashen::Materials::Apply(SelectionMarker, this, FLinearColor(0.95f, 0.54f, 0.07f), 0.35f);

    const float Diameter = FMath::Max(20.0f, Radius * 2.0f);
    EntityMesh->SetRelativeScale3D({Diameter / 100.0f, Diameter / 100.0f, VisualHeight / 100.0f});
    SelectionMarker->SetRelativeScale3D({Diameter * 1.35f / 100.0f, Diameter * 1.35f / 100.0f, 0.025f});
    SelectionMarker->SetRelativeLocation({0.0f, 0.0f, -VisualHeight * 0.5f + 2.0f});
    FactionLight->SetRelativeLocation({0.0f, 0.0f, VisualHeight * 0.45f});
    FactionLight->SetLightColor(OwnerIndex == 0 ? FLinearColor(0.95f, 0.68f, 0.22f)
                                               : FLinearColor(0.78f, 0.06f, 0.08f));
    FactionLight->SetAttenuationRadius(FMath::Max(150.0f, Diameter * 3.0f));
    EntityMesh->SetCustomDepthStencilValue(OwnerIndex == 0 ? 1 : 2);

#if WITH_EDITOR
    SetActorLabel(FString::Printf(TEXT("AshenEntity_%d_%d"), EntityId, static_cast<int32>(Archetype)));
#endif
}

void AAshenEntityActor::ApplySimulationState(const FVector& GroundPosition, const float HealthFraction)
{
    SetActorLocation({GroundPosition.X, GroundPosition.Y, VisualHeight * 0.5f});
    HealthLightIntensity = FMath::Lerp(180.0f, 650.0f, FMath::Clamp(HealthFraction, 0.0f, 1.0f));
    FactionLight->SetIntensity(bSelected ? 1'400.0f : HealthLightIntensity);
}

void AAshenEntityActor::SetSelected(const bool bInSelected)
{
    bSelected = bInSelected;
    SelectionMarker->SetVisibility(bInSelected);
    EntityMesh->SetRenderCustomDepth(bInSelected);
    FactionLight->SetIntensity(bInSelected ? 1'400.0f : HealthLightIntensity);
}
