#pragma once

#include "Math/Color.h"

class UObject;
class UPrimitiveComponent;

namespace Ashen::Materials
{
struct FSurfaceStyle
{
    FLinearColor BaseColor;
    FLinearColor SecondaryColor;
    FLinearColor AccentColor;
    float Roughness = 0.9f;
    float MacroScale = 360.0f;
    float DetailScale = 72.0f;
    float DetailStrength = 0.16f;
    float Specular = 0.25f;
    float AmbientOcclusion = 0.92f;
};

void Apply(UPrimitiveComponent *Component, UObject *Outer, const FLinearColor &Color, float Roughness);
void ApplySurface(UPrimitiveComponent *Component, UObject *Outer, const FSurfaceStyle &Style);
void ApplyWater(UPrimitiveComponent *Component, UObject *Outer, const FLinearColor &ShallowColor,
                const FLinearColor &DeepColor, float Opacity = 0.78f);
} // namespace Ashen::Materials
