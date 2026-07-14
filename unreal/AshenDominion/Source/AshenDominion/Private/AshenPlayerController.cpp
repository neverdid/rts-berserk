#include "AshenPlayerController.h"

#include "AshenEntityActor.h"
#include "AshenResourceActor.h"
#include "AshenSimulationSubsystem.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

AAshenPlayerController::AAshenPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void AAshenPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction(TEXT("Select"), IE_Pressed, this, &AAshenPlayerController::BeginSelection);
    InputComponent->BindAction(TEXT("Select"), IE_Released, this, &AAshenPlayerController::EndSelection);
    InputComponent->BindAction(TEXT("Command"), IE_Pressed, this, &AAshenPlayerController::CommandUnderCursor);
    InputComponent->BindAction(TEXT("TrainPrimary"), IE_Pressed, this, &AAshenPlayerController::TrainPrimary);
    InputComponent->BindAction(TEXT("TrainSecondary"), IE_Pressed, this, &AAshenPlayerController::TrainSecondary);
}

int32 AAshenPlayerController::GetSelectedCount() const
{
    int32 Count = 0;
    for (const TWeakObjectPtr<AAshenEntityActor>& Actor : SelectedActors)
    {
        Count += Actor.IsValid() ? 1 : 0;
    }
    return Count;
}

bool AAshenPlayerController::GetSelectionBox(FVector2D& OutMin, FVector2D& OutMax) const
{
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    if (!bSelecting || !GetMousePosition(MouseX, MouseY))
    {
        return false;
    }

    const FVector2D Current(MouseX, MouseY);
    if (FVector2D::Distance(SelectionStart, Current) < 6.0f)
    {
        return false;
    }

    OutMin = {FMath::Min(SelectionStart.X, Current.X), FMath::Min(SelectionStart.Y, Current.Y)};
    OutMax = {FMath::Max(SelectionStart.X, Current.X), FMath::Max(SelectionStart.Y, Current.Y)};
    return true;
}

void AAshenPlayerController::BeginSelection()
{
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    bSelecting = GetMousePosition(MouseX, MouseY);
    SelectionStart = {MouseX, MouseY};
}

void AAshenPlayerController::EndSelection()
{
    if (!bSelecting)
    {
        return;
    }

    float MouseX = 0.0f;
    float MouseY = 0.0f;
    const bool bHasMousePosition = GetMousePosition(MouseX, MouseY);
    bSelecting = false;
    if (!bHasMousePosition)
    {
        return;
    }

    const FVector2D SelectionEnd(MouseX, MouseY);
    const bool bAddToSelection = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
    if (FVector2D::Distance(SelectionStart, SelectionEnd) < 8.0f)
    {
        SelectUnderCursor(bAddToSelection);
        return;
    }

    if (!bAddToSelection)
    {
        ClearSelection();
    }

    FBox2D SelectionBox(ForceInit);
    SelectionBox += SelectionStart;
    SelectionBox += SelectionEnd;
    for (TActorIterator<AAshenEntityActor> It(GetWorld()); It; ++It)
    {
        AAshenEntityActor* Entity = *It;
        FVector2D ScreenPosition;
        if (Entity->GetOwnerIndex() != 0 || !ProjectWorldLocationToScreen(Entity->GetActorLocation(), ScreenPosition) ||
            !SelectionBox.IsInside(ScreenPosition))
        {
            continue;
        }

        const bool bAlreadySelected = SelectedActors.ContainsByPredicate(
            [Entity](const TWeakObjectPtr<AAshenEntityActor>& Item)
            {
                return Item.Get() == Entity;
            });
        if (!bAlreadySelected)
        {
            SelectedActors.Add(Entity);
            Entity->SetSelected(true);
        }
    }
}

void AAshenPlayerController::SelectUnderCursor(const bool bAddToSelection)
{
    FHitResult Hit;
    const bool bHit = GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit);
    if (!bAddToSelection)
    {
        ClearSelection();
    }

    AAshenEntityActor* Entity = bHit ? Cast<AAshenEntityActor>(Hit.GetActor()) : nullptr;
    if (Entity == nullptr || Entity->GetOwnerIndex() != 0)
    {
        return;
    }

    const bool bAlreadySelected = SelectedActors.ContainsByPredicate([Entity](const TWeakObjectPtr<AAshenEntityActor>& Item)
    {
        return Item.Get() == Entity;
    });
    if (!bAlreadySelected)
    {
        SelectedActors.Add(Entity);
        Entity->SetSelected(true);
    }
}

void AAshenPlayerController::CommandUnderCursor()
{
    PruneSelection();
    const TArray<int32> Ids = SelectedIds();
    if (Ids.IsEmpty())
    {
        return;
    }

    FHitResult Hit;
    if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
    {
        return;
    }

    UAshenSimulationSubsystem* Sim = Simulation();
    if (Sim == nullptr)
    {
        return;
    }

    if (const AAshenEntityActor* Target = Cast<AAshenEntityActor>(Hit.GetActor()))
    {
        if (Target->GetOwnerIndex() != 0)
        {
            Sim->IssueAttack(Ids, Target->GetEntityId());
            return;
        }
    }
    if (const AAshenResourceActor* Resource = Cast<AAshenResourceActor>(Hit.GetActor()))
    {
        Sim->IssueGather(Ids, Resource->GetResourceId());
        return;
    }
    Sim->IssueMove(Ids, Hit.ImpactPoint);
}

void AAshenPlayerController::TrainPrimary()
{
    PruneSelection();
    if (!SelectedActors.IsEmpty() && Simulation() != nullptr)
    {
        Simulation()->IssueTrain(SelectedActors[0]->GetEntityId(), false);
    }
}

void AAshenPlayerController::TrainSecondary()
{
    PruneSelection();
    if (!SelectedActors.IsEmpty() && Simulation() != nullptr)
    {
        Simulation()->IssueTrain(SelectedActors[0]->GetEntityId(), true);
    }
}

void AAshenPlayerController::ClearSelection()
{
    for (const TWeakObjectPtr<AAshenEntityActor>& Actor : SelectedActors)
    {
        if (Actor.IsValid())
        {
            Actor->SetSelected(false);
        }
    }
    SelectedActors.Reset();
}

void AAshenPlayerController::PruneSelection()
{
    SelectedActors.RemoveAll([](const TWeakObjectPtr<AAshenEntityActor>& Actor)
    {
        return !Actor.IsValid();
    });
}

TArray<int32> AAshenPlayerController::SelectedIds() const
{
    TArray<int32> Ids;
    Ids.Reserve(SelectedActors.Num());
    for (const TWeakObjectPtr<AAshenEntityActor>& Actor : SelectedActors)
    {
        if (Actor.IsValid())
        {
            Ids.Add(Actor->GetEntityId());
        }
    }
    return Ids;
}

UAshenSimulationSubsystem* AAshenPlayerController::Simulation() const
{
    return GetWorld() == nullptr ? nullptr : GetWorld()->GetSubsystem<UAshenSimulationSubsystem>();
}
