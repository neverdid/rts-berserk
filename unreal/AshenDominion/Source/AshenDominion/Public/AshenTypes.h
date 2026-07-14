#pragma once

#include "CoreMinimal.h"
#include "AshenTypes.generated.h"

UENUM(BlueprintType)
enum class EAshenEntityArchetype : uint8
{
    Worker,
    Vanguard,
    Skirmisher,
    Command,
    Barracks,
    Turret,
};

USTRUCT(BlueprintType)
struct FAshenPlayerView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Ashen")
    int32 Ore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Ashen")
    int32 SupplyUsed = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Ashen")
    int32 SupplyCap = 0;
};
