#include "AshenResourceActor.h"

#include "AshenMaterials.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"

AAshenResourceActor::AAshenResourceActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Movable);

    ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
    ResourceMesh->SetupAttachment(SceneRoot);
    ResourceMesh->SetMobility(EComponentMobility::Movable);
    ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ResourceMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    ResourceMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    ResourceLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ResourceLight"));
    ResourceLight->SetupAttachment(SceneRoot);
    ResourceLight->SetCastShadows(false);
    ResourceLight->SetLightColor(FLinearColor(0.95f, 0.28f, 0.04f));
    ResourceLight->SetIntensity(1'250.0f);
    ResourceLight->SetAttenuationRadius(260.0f);
}

void AAshenResourceActor::InitializeResource(const int32 InResourceId, const float Radius)
{
    ResourceId = InResourceId;
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (Sphere != nullptr)
    {
        ResourceMesh->SetStaticMesh(Sphere);
    }
    Ashen::Materials::Apply(ResourceMesh, this, FLinearColor(0.12f, 0.018f, 0.012f), 0.52f);

    const float Diameter = FMath::Max(35.0f, Radius * 2.0f);
    ResourceMesh->SetRelativeLocation({0.0f, 0.0f, Diameter * 0.22f});
    ResourceMesh->SetRelativeScale3D({Diameter * 0.82f / 100.0f, Diameter * 0.68f / 100.0f,
                                      Diameter * 0.34f / 100.0f});

    if (Cone != nullptr)
    {
        const FVector2D CrystalOffsets[] = {
            {-0.24f, -0.16f}, {0.04f, 0.18f}, {0.26f, -0.08f}, {-0.05f, -0.28f}, {0.31f, 0.25f},
        };
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(CrystalOffsets); ++Index)
        {
            UStaticMeshComponent* Crystal = NewObject<UStaticMeshComponent>(
                this, FName(*FString::Printf(TEXT("CursedCrystal_%d"), Index)));
            AddInstanceComponent(Crystal);
            Crystal->SetupAttachment(SceneRoot);
            Crystal->SetMobility(EComponentMobility::Movable);
            Crystal->SetStaticMesh(Cone);
            Crystal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            const float HeightScale = 0.46f + static_cast<float>(Index % 3) * 0.18f;
            Crystal->SetRelativeLocation({CrystalOffsets[Index].X * Diameter, CrystalOffsets[Index].Y * Diameter,
                                          Diameter * (0.25f + HeightScale * 0.42f)});
            Crystal->SetRelativeRotation({static_cast<float>((Index % 2 == 0 ? 1 : -1) * (8 + Index * 2)),
                                          static_cast<float>(Index * 67), static_cast<float>(Index * 5 - 9)});
            Crystal->SetRelativeScale3D({Diameter * 0.15f / 100.0f, Diameter * 0.15f / 100.0f, HeightScale});
            Crystal->RegisterComponent();
            Ashen::Materials::Apply(Crystal, this,
                                    Index % 2 == 0 ? FLinearColor(0.55f, 0.045f, 0.01f)
                                                   : FLinearColor(0.30f, 0.015f, 0.006f),
                                    0.30f);
            CrystalMeshes.Add(Crystal);
        }
    }

    ResourceLight->SetRelativeLocation({0.0f, 0.0f, Diameter * 0.78f});
    ResourceLight->SetAttenuationRadius(FMath::Clamp(Diameter * 3.2f, 260.0f, 520.0f));
#if WITH_EDITOR
    SetActorLabel(FString::Printf(TEXT("CursedIron_%d"), ResourceId));
#endif
}

void AAshenResourceActor::ApplySimulationState(const FVector& GroundPosition)
{
    SetActorLocation(GroundPosition);
}

void AAshenResourceActor::SetFogState(const EAshenVisibility Visibility)
{
    const bool bDiscovered = Visibility != EAshenVisibility::Hidden;
    SetActorHiddenInGame(!bDiscovered);
    ResourceMesh->SetCollisionEnabled(bDiscovered ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    ResourceLight->SetVisibility(Visibility == EAshenVisibility::Visible);
}
