#include "AshenPlayerController.h"

#include "AshenEntityActor.h"
#include "AshenResourceActor.h"
#include "AshenSimulationSubsystem.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
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
    InputComponent->BindAction(TEXT("Deploy"), IE_Pressed, this, &AAshenPlayerController::DeploySkirmish);
    InputComponent->BindAction(TEXT("Menu"), IE_Pressed, this, &AAshenPlayerController::ToggleFrontEnd);
}

bool AAshenPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (Params.Event == IE_Pressed)
    {
        if (bFrontEndVisible && (Params.Key == EKeys::Enter || Params.Key == EKeys::SpaceBar))
        {
            DeploySkirmish();
            return true;
        }
        if (bFrontEndVisible && Params.Key == EKeys::LeftMouseButton)
        {
            HandleFrontEndClick();
            return true;
        }
        if (!bFrontEndVisible && Params.Key == EKeys::Escape)
        {
            ToggleFrontEnd();
            return true;
        }
    }
    return Super::InputKey(Params);
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

bool AAshenPlayerController::GetFrontEndPrimaryButton(FVector2D& OutMin, FVector2D& OutMax) const
{
    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;
    GetViewportSize(ViewportWidth, ViewportHeight);
    if (ViewportWidth <= 0 || ViewportHeight <= 0)
    {
        return false;
    }

    const bool bCompact = ViewportHeight < 650;
    const float Margin = ViewportWidth < 760 ? 28.0f : 64.0f;
    const float Width = FMath::Min(360.0f, static_cast<float>(ViewportWidth) - Margin * 2.0f);
    const float Y = bCompact ? 270.0f : FMath::Clamp(static_cast<float>(ViewportHeight) * 0.56f, 310.0f,
                                                     static_cast<float>(ViewportHeight) - 210.0f);
    OutMin = {Margin, Y};
    OutMax = {Margin + Width, Y + (bCompact ? 50.0f : 58.0f)};
    return true;
}

void AAshenPlayerController::BeginSelection()
{
    if (bFrontEndVisible)
    {
        HandleFrontEndClick();
        return;
    }

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
    if (bFrontEndVisible)
    {
        return;
    }

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
    if (bFrontEndVisible)
    {
        return;
    }
    PruneSelection();
    if (!SelectedActors.IsEmpty() && Simulation() != nullptr)
    {
        Simulation()->IssueTrain(SelectedActors[0]->GetEntityId(), false);
    }
}

void AAshenPlayerController::TrainSecondary()
{
    if (bFrontEndVisible)
    {
        return;
    }
    PruneSelection();
    if (!SelectedActors.IsEmpty() && Simulation() != nullptr)
    {
        Simulation()->IssueTrain(SelectedActors[0]->GetEntityId(), true);
    }
}

void AAshenPlayerController::DeploySkirmish()
{
    if (!bFrontEndVisible)
    {
        return;
    }

    bFrontEndVisible = false;
    if (UAshenSimulationSubsystem* Sim = Simulation())
    {
        if (Sim->IsMatchOver())
        {
            Sim->RestartMatch();
        }
        Sim->SetGameplayEnabled(true);
    }
}

void AAshenPlayerController::ToggleFrontEnd()
{
    if (bFrontEndVisible)
    {
        return;
    }

    ClearSelection();
    bFrontEndVisible = true;
    if (UAshenSimulationSubsystem* Sim = Simulation())
    {
        Sim->SetGameplayEnabled(false);
    }
}

void AAshenPlayerController::HandleFrontEndClick()
{
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    FVector2D ButtonMin;
    FVector2D ButtonMax;
    if (!GetMousePosition(MouseX, MouseY) || !GetFrontEndPrimaryButton(ButtonMin, ButtonMax))
    {
        return;
    }

    const FVector2D Mouse(MouseX, MouseY);
    if (Mouse.X >= ButtonMin.X && Mouse.X <= ButtonMax.X && Mouse.Y >= ButtonMin.Y && Mouse.Y <= ButtonMax.Y)
    {
        DeploySkirmish();
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
