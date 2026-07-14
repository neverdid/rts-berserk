#include "AshenHUD.h"

#include "AshenPlayerController.h"
#include "AshenSimulationSubsystem.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void AAshenHUD::DrawHUD()
{
    Super::DrawHUD();
    if (Canvas == nullptr || GetWorld() == nullptr)
    {
        return;
    }

    const UAshenSimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UAshenSimulationSubsystem>();
    if (Simulation == nullptr)
    {
        return;
    }

    const FAshenPlayerView Player = Simulation->GetPlayerView(0);
    const AAshenPlayerController* Controller = Cast<AAshenPlayerController>(GetOwningPlayerController());
    const int32 Selected = Controller == nullptr ? 0 : Controller->GetSelectedCount();

    DrawRect(FLinearColor(0.015f, 0.018f, 0.02f, 0.93f), 24.0f, 22.0f, 610.0f, 76.0f);
    DrawRect(FLinearColor(0.72f, 0.48f, 0.14f, 1.0f), 24.0f, 96.0f, 610.0f, 2.0f);
    DrawText(TEXT("ASHEN DOMINION  //  NATIVE WAR TABLE"), FLinearColor(0.88f, 0.80f, 0.63f),
             42.0f, 34.0f, GEngine->GetMediumFont(), 1.0f, false);

    const FString State = FString::Printf(TEXT("ORE  %d     SUPPLY  %d / %d     SELECTED  %d     TICK  %lld"),
                                           Player.Ore, Player.SupplyUsed, Player.SupplyCap, Selected,
                                           Simulation->GetSimulationTick());
    DrawText(State, FLinearColor(0.82f, 0.84f, 0.82f), 42.0f, 68.0f, GEngine->GetSmallFont(), 1.0f, false);

    FVector2D SelectionMin;
    FVector2D SelectionMax;
    if (Controller != nullptr && Controller->GetSelectionBox(SelectionMin, SelectionMax))
    {
        const float Width = SelectionMax.X - SelectionMin.X;
        const float Height = SelectionMax.Y - SelectionMin.Y;
        const FLinearColor Border(0.94f, 0.61f, 0.14f, 0.95f);
        DrawRect(FLinearColor(0.72f, 0.36f, 0.06f, 0.12f), SelectionMin.X, SelectionMin.Y, Width, Height);
        DrawRect(Border, SelectionMin.X, SelectionMin.Y, Width, 1.5f);
        DrawRect(Border, SelectionMin.X, SelectionMax.Y - 1.5f, Width, 1.5f);
        DrawRect(Border, SelectionMin.X, SelectionMin.Y, 1.5f, Height);
        DrawRect(Border, SelectionMax.X - 1.5f, SelectionMin.Y, 1.5f, Height);
    }
}
