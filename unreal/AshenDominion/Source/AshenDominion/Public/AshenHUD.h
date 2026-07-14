#pragma once

#include "GameFramework/HUD.h"
#include "AshenHUD.generated.h"

UCLASS()
class ASHENDOMINION_API AAshenHUD final : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
