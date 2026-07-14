#include "AshenMaterials.h"

#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

void Ashen::Materials::Apply(UPrimitiveComponent* Component, UObject* Outer,
                             const FLinearColor& Color, const float Roughness)
{
    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Component == nullptr || BaseMaterial == nullptr)
    {
        return;
    }

    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Outer);

    TArray<FMaterialParameterInfo> VectorParameters;
    TArray<FGuid> VectorIds;
    BaseMaterial->GetAllVectorParameterInfo(VectorParameters, VectorIds);
    for (const FMaterialParameterInfo& Parameter : VectorParameters)
    {
        Material->SetVectorParameterValueByInfo(Parameter, Color);
    }

    TArray<FMaterialParameterInfo> ScalarParameters;
    TArray<FGuid> ScalarIds;
    BaseMaterial->GetAllScalarParameterInfo(ScalarParameters, ScalarIds);
    for (const FMaterialParameterInfo& Parameter : ScalarParameters)
    {
        if (Parameter.Name == TEXT("Roughness"))
        {
            Material->SetScalarParameterValueByInfo(Parameter, Roughness);
        }
    }

    Component->SetMaterial(0, Material);
}
