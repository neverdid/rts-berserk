#pragma once

#include "GameFramework/GameModeBase.h"
#include "AshenGameMode.generated.h"

UCLASS()
class ASHENDOMINION_API AAshenGameMode final : public AGameModeBase
{
    GENERATED_BODY()

public:
    AAshenGameMode();
    virtual void BeginPlay() override;
};
