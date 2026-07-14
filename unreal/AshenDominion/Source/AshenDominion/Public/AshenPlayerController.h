#pragma once

#include "GameFramework/PlayerController.h"
#include "AshenPlayerController.generated.h"

class AAshenEntityActor;
struct FInputKeyEventArgs;
class UAshenSimulationSubsystem;

UCLASS()
class ASHENDOMINION_API AAshenPlayerController final : public APlayerController
{
    GENERATED_BODY()

public:
    AAshenPlayerController();
    virtual void SetupInputComponent() override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;

    UFUNCTION(BlueprintPure, Category = "Ashen")
    int32 GetSelectedCount() const;

    UFUNCTION(BlueprintPure, Category = "Ashen")
    bool IsFrontEndVisible() const noexcept { return bFrontEndVisible; }

    bool GetSelectionBox(FVector2D& OutMin, FVector2D& OutMax) const;
    bool GetFrontEndPrimaryButton(FVector2D& OutMin, FVector2D& OutMax) const;

private:
    void BeginSelection();
    void EndSelection();
    void SelectUnderCursor(bool bAddToSelection);
    void CommandUnderCursor();
    void TrainPrimary();
    void TrainSecondary();
    void DeploySkirmish();
    void ToggleFrontEnd();
    void HandleFrontEndClick();
    void ClearSelection();
    void PruneSelection();
    TArray<int32> SelectedIds() const;
    UAshenSimulationSubsystem* Simulation() const;

    TArray<TWeakObjectPtr<AAshenEntityActor>> SelectedActors;
    FVector2D SelectionStart = FVector2D::ZeroVector;
    bool bSelecting = false;
    bool bFrontEndVisible = true;
};
