#pragma once

#include "GameFramework/PlayerController.h"
#include "AshenPlayerController.generated.h"

class AAshenEntityActor;
struct FInputKeyEventArgs;
class UAshenSimulationSubsystem;

enum class EAshenCommandMode : uint8
{
    None,
    AttackMove,
    Patrol,
    RallyPoint,
};

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
    FString GetCommandModeLabel() const;
    int32 GetPrimarySelectedEntityId() const;
    int32 GetActiveControlGroup() const noexcept { return ActiveControlGroup; }
    bool GetCommandFeedback(FVector& OutLocation, bool& bOutHostile, float& OutStrength) const;

private:
    void BeginSelection();
    void EndSelection();
    void SelectUnderCursor(bool bAddToSelection);
    void SelectSameTypeUnderCursor(bool bAddToSelection);
    void CommandUnderCursor();
    void ExecutePendingCommand();
    void BeginAttackMove();
    void BeginPatrol();
    void BeginRallyPoint();
    void StopSelected();
    void HoldSelected();
    void TrainPrimary();
    void TrainSecondary();
    void DeploySkirmish();
    void ToggleFrontEnd();
    void HandleFrontEndClick();
    void ClearSelection();
    void PruneSelection();
    void AddToSelection(AAshenEntityActor* Entity);
    void HandleControlGroup(int32 GroupIndex);
    void FocusSelection();
    void RegisterCommandFeedback(const FVector& Location, bool bHostile);
    bool IsQueueModifierDown() const;
    TArray<int32> SelectedUnitIds() const;
    TArray<int32> SelectedWorkerIds() const;
    int32 SelectedBuildingId() const;
    UAshenSimulationSubsystem* Simulation() const;

    TArray<TWeakObjectPtr<AAshenEntityActor>> SelectedActors;
    TArray<TWeakObjectPtr<AAshenEntityActor>> ControlGroups[10];
    FVector2D SelectionStart = FVector2D::ZeroVector;
    FVector LastCommandLocation = FVector::ZeroVector;
    double LastCommandFeedbackTime = -10.0;
    double LastControlGroupTime = -10.0;
    int32 LastControlGroup = -1;
    int32 ActiveControlGroup = -1;
    EAshenCommandMode PendingCommand = EAshenCommandMode::None;
    bool bSelecting = false;
    bool bFrontEndVisible = true;
    bool bLastCommandHostile = false;
};
