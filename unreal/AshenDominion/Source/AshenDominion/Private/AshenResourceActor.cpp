#include "AshenResourceActor.h"

#include "AshenMaterials.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"

AAshenResourceActor::AAshenResourceActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
    SetRootComponent(ResourceMesh);
    ResourceMesh->SetMobility(EComponentMobility::Movable);
    ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ResourceMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    ResourceMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    ResourceLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ResourceLight"));
    ResourceLight->SetupAttachment(ResourceMesh);
    ResourceLight->SetCastShadows(false);
    ResourceLight->SetLightColor(FLinearColor(0.95f, 0.28f, 0.04f));
    ResourceLight->SetIntensity(1'250.0f);
    ResourceLight->SetAttenuationRadius(260.0f);
}

void AAshenResourceActor::InitializeResource(const int32 InResourceId, const float Radius)
{
    ResourceId = InResourceId;
    if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
    {
        ResourceMesh->SetStaticMesh(Mesh);
    }
    Ashen::Materials::Apply(ResourceMesh, this, FLinearColor(0.28f, 0.028f, 0.006f), 0.38f);

    const float Diameter = FMath::Max(35.0f, Radius * 2.0f);
    GroundOffset = Diameter * 0.36f;
    ResourceMesh->SetRelativeScale3D({Diameter / 100.0f, Diameter / 100.0f, Diameter * 0.72f / 100.0f});
    ResourceLight->SetRelativeLocation({0.0f, 0.0f, Diameter * 0.5f});
#if WITH_EDITOR
    SetActorLabel(FString::Printf(TEXT("CursedIron_%d"), ResourceId));
#endif
}

void AAshenResourceActor::ApplySimulationState(const FVector& GroundPosition)
{
    SetActorLocation({GroundPosition.X, GroundPosition.Y, GroundPosition.Z + GroundOffset});
}
