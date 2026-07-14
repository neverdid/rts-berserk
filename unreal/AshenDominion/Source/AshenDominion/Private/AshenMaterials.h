#pragma once

#include "Math/Color.h"

class UObject;
class UPrimitiveComponent;

namespace Ashen::Materials
{
void Apply(UPrimitiveComponent* Component, UObject* Outer, const FLinearColor& Color, float Roughness);
}
