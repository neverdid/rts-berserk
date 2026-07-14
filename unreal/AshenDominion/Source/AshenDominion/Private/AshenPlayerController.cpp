#include "AshenPlayerController.h"

#include "AshenCameraPawn.h"
#include "AshenEntityActor.h"
#include "AshenResourceActor.h"
#include "AshenSimulationSubsystem.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"

namespace
{
bool IsUnitArchetype(const EAshenEntityArchetype Archetype)
{
    return Archetype == EAshenEntityArchetype::Worker || Archetype == EAshenEntityArchetype::Vanguard ||
           Archetype == EAshenEntityArchetype::Skirmisher;
}

int32 ControlGroupFromKey(const FKey& Key)
{
    if (Key == EKeys::Zero) return 0;
    if (Key == EKeys::One) return 1;
    if (Key == EKeys::Two) return 2;
    if (Key == EKeys::Three) return 3;
    if (Key == EKeys::Four) return 4;
    if (Key == EKeys::Five) return 5;
    if (Key == EKeys::Six) return 6;
    if (Key == EKeys::Seven) return 7;
    if (Key == EKeys::Eight) return 8;
    if (Key == EKeys::Nine) return 9;
    return -1;
}
}

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
    if (!bFrontEndVisible && Params.Key == EKeys::LeftMouseButton && Params.Event == IE_DoubleClick)
    {
        SelectSameTypeUnderCursor(IsQueueModifierDown());
        return true;
    }

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
            if (PendingCommand != EAshenCommandMode::None)
            {
                PendingCommand = EAshenCommandMode::None;
                return true;
            }
            ToggleFrontEnd();
            return true;
        }
        if (!bFrontEndVisible && Params.Key == EKeys::LeftMouseButton && PendingCommand != EAshenCommandMode::None)
        {
            ExecutePendingCommand();
            return true;
        }
        if (!bFrontEndVisible && Params.Key == EKeys::A)
        {
            BeginAttackMove();
            return true;
        }
        if (!bFrontEndVisible && Params.Key == EKeys::P)
        {
            BeginPatrol();
            return true;
        }
        if (!bFrontEndVisible && Params.Key == EKeys::R)
        {
            BeginRallyPoint();
            return true;
        }
        if (!bFrontEndVisible && Params.Key == EKeys::S)
        {
            StopSelected();
            return true;
        }
        if (!bFrontEndVisible && Params.Key == EKeys::H)
        {
            HoldSelected();
            return true;
        }

        const int32 GroupIndex = ControlGroupFromKey(Params.Key);
        if (!bFrontEndVisible && GroupIndex >= 0)
        {
            HandleControlGroup(GroupIndex);
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

FString AAshenPlayerController::GetCommandModeLabel() const
{
    switch (PendingCommand)
    {
    case EAshenCommandMode::AttackMove:
        return TEXT("ATTACK-MOVE  //  CHOOSE GROUND OR ENEMY");
    case EAshenCommandMode::Patrol:
        return TEXT("PATROL  //  CHOOSE DESTINATION");
    case EAshenCommandMode::RallyPoint:
        return TEXT("RALLY POINT  //  CHOOSE DESTINATION");
    case EAshenCommandMode::None:
        return {};
    }
    return {};
}

int32 AAshenPlayerController::GetPrimarySelectedEntityId() const
{
    for (const TWeakObjectPtr<AAshenEntityActor>& Actor : SelectedActors)
    {
        if (Actor.IsValid())
        {
            return Actor->GetEntityId();
        }
    }
    return 0;
}

bool AAshenPlayerController::GetCommandFeedback(FVector& OutLocation, bool& bOutHostile,
                                                float& OutStrength) const
{
    if (GetWorld() == nullptr)
    {
        return false;
    }
    constexpr double FeedbackDuration = 0.72;
    const double Age = GetWorld()->GetTimeSeconds() - LastCommandFeedbackTime;
    if (Age < 0.0 || Age > FeedbackDuration)
    {
        return false;
    }
    OutLocation = LastCommandLocation;
    bOutHostile = bLastCommandHostile;
    OutStrength = 1.0f - static_cast<float>(Age / FeedbackDuration);
    return true;
}

void AAshenPlayerController::BeginSelection()
{
    if (bFrontEndVisible)
    {
        HandleFrontEndClick();
        return;
    }
    if (PendingCommand != EAshenCommandMode::None)
    {
        ExecutePendingCommand();
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

    FBox2D SelectionBox(ForceInit);
    SelectionBox += SelectionStart;
    SelectionBox += SelectionEnd;
    TArray<AAshenEntityActor*> Candidates;
    bool bContainsUnit = false;
    for (TActorIterator<AAshenEntityActor> It(GetWorld()); It; ++It)
    {
        AAshenEntityActor* Entity = *It;
        FVector2D ScreenPosition;
        if (Entity->GetOwnerIndex() != 0 || !ProjectWorldLocationToScreen(Entity->GetActorLocation(), ScreenPosition) ||
            !SelectionBox.IsInside(ScreenPosition))
        {
            continue;
        }
        Candidates.Add(Entity);
        bContainsUnit = bContainsUnit || IsUnitArchetype(Entity->GetArchetype());
    }
    if (!bAddToSelection)
    {
        ClearSelection();
    }
    for (AAshenEntityActor* Entity : Candidates)
    {
        if (!bContainsUnit || IsUnitArchetype(Entity->GetArchetype()))
        {
            AddToSelection(Entity);
        }
    }
    ActiveControlGroup = -1;
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

    AddToSelection(Entity);
    ActiveControlGroup = -1;
}

void AAshenPlayerController::SelectSameTypeUnderCursor(const bool bAddToSelection)
{
    FHitResult Hit;
    if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
    {
        return;
    }

    const AAshenEntityActor* Seed = Cast<AAshenEntityActor>(Hit.GetActor());
    if (Seed == nullptr || Seed->GetOwnerIndex() != 0)
    {
        return;
    }
    if (!bAddToSelection)
    {
        ClearSelection();
    }

    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;
    GetViewportSize(ViewportWidth, ViewportHeight);
    for (TActorIterator<AAshenEntityActor> It(GetWorld()); It; ++It)
    {
        AAshenEntityActor* Entity = *It;
        FVector2D ScreenPosition;
        if (Entity->GetOwnerIndex() == 0 && Entity->GetArchetype() == Seed->GetArchetype() &&
            ProjectWorldLocationToScreen(Entity->GetActorLocation(), ScreenPosition) && ScreenPosition.X >= 0.0f &&
            ScreenPosition.Y >= 0.0f && ScreenPosition.X <= static_cast<float>(ViewportWidth) &&
            ScreenPosition.Y <= static_cast<float>(ViewportHeight))
        {
            AddToSelection(Entity);
        }
    }
    ActiveControlGroup = -1;
}

void AAshenPlayerController::CommandUnderCursor()
{
    if (bFrontEndVisible)
    {
        return;
    }

    PruneSelection();
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

    PendingCommand = EAshenCommandMode::None;
    const bool bQueue = IsQueueModifierDown();
    const TArray<int32> UnitIds = SelectedUnitIds();
    if (const AAshenEntityActor* Target = Cast<AAshenEntityActor>(Hit.GetActor()))
    {
        if (Target->GetOwnerIndex() != 0 && !UnitIds.IsEmpty())
        {
            if (Sim->IssueAttack(UnitIds, Target->GetEntityId(), bQueue))
            {
                RegisterCommandFeedback(Target->GetActorLocation(), true);
            }
            return;
        }
    }
    if (const AAshenResourceActor* Resource = Cast<AAshenResourceActor>(Hit.GetActor()))
    {
        const TArray<int32> WorkerIds = SelectedWorkerIds();
        if (!WorkerIds.IsEmpty() && Sim->IssueGather(WorkerIds, Resource->GetResourceId(), bQueue))
        {
            RegisterCommandFeedback(Resource->GetActorLocation(), false);
        }
        return;
    }

    if (!UnitIds.IsEmpty())
    {
        if (Sim->IssueMove(UnitIds, Hit.ImpactPoint, bQueue))
        {
            RegisterCommandFeedback(Hit.ImpactPoint, false);
        }
        return;
    }
    const int32 BuildingId = SelectedBuildingId();
    if (BuildingId > 0 && Sim->IssueSetRallyPoint(BuildingId, Hit.ImpactPoint))
    {
        RegisterCommandFeedback(Hit.ImpactPoint, false);
    }
}

void AAshenPlayerController::ExecutePendingCommand()
{
    if (bFrontEndVisible || PendingCommand == EAshenCommandMode::None)
    {
        return;
    }

    PruneSelection();
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

    const EAshenCommandMode CommandMode = PendingCommand;
    PendingCommand = EAshenCommandMode::None;
    const bool bQueue = IsQueueModifierDown();
    bool bIssued = false;
    bool bHostile = false;
    FVector FeedbackLocation = Hit.ImpactPoint;
    if (CommandMode == EAshenCommandMode::RallyPoint)
    {
        const int32 BuildingId = SelectedBuildingId();
        bIssued = BuildingId > 0 && Sim->IssueSetRallyPoint(BuildingId, Hit.ImpactPoint);
    }
    else
    {
        const TArray<int32> UnitIds = SelectedUnitIds();
        if (UnitIds.IsEmpty())
        {
            return;
        }

        if (CommandMode == EAshenCommandMode::AttackMove)
        {
            if (const AAshenEntityActor* Target = Cast<AAshenEntityActor>(Hit.GetActor());
                Target != nullptr && Target->GetOwnerIndex() != 0)
            {
                bIssued = Sim->IssueAttack(UnitIds, Target->GetEntityId(), bQueue);
                bHostile = true;
                FeedbackLocation = Target->GetActorLocation();
            }
            else
            {
                bIssued = Sim->IssueAttackMove(UnitIds, Hit.ImpactPoint, bQueue);
                bHostile = true;
            }
        }
        else if (CommandMode == EAshenCommandMode::Patrol)
        {
            bIssued = Sim->IssuePatrol(UnitIds, Hit.ImpactPoint, bQueue);
        }
    }

    if (bIssued)
    {
        RegisterCommandFeedback(FeedbackLocation, bHostile);
    }
}

void AAshenPlayerController::BeginAttackMove()
{
    PruneSelection();
    PendingCommand = SelectedUnitIds().IsEmpty() ? EAshenCommandMode::None : EAshenCommandMode::AttackMove;
}

void AAshenPlayerController::BeginPatrol()
{
    PruneSelection();
    PendingCommand = SelectedUnitIds().IsEmpty() ? EAshenCommandMode::None : EAshenCommandMode::Patrol;
}

void AAshenPlayerController::BeginRallyPoint()
{
    PruneSelection();
    PendingCommand = SelectedBuildingId() <= 0 ? EAshenCommandMode::None : EAshenCommandMode::RallyPoint;
}

void AAshenPlayerController::StopSelected()
{
    PendingCommand = EAshenCommandMode::None;
    PruneSelection();
    if (UAshenSimulationSubsystem* Sim = Simulation())
    {
        const TArray<int32> UnitIds = SelectedUnitIds();
        if (!UnitIds.IsEmpty() && Sim->IssueStop(UnitIds))
        {
            FVector Center = FVector::ZeroVector;
            int32 Count = 0;
            for (const TWeakObjectPtr<AAshenEntityActor>& Actor : SelectedActors)
            {
                if (Actor.IsValid() && IsUnitArchetype(Actor->GetArchetype()))
                {
                    Center += Actor->GetActorLocation();
                    ++Count;
                }
            }
            RegisterCommandFeedback(Count > 0 ? Center / static_cast<float>(Count) : FVector::ZeroVector, false);
        }
    }
}

void AAshenPlayerController::HoldSelected()
{
    PendingCommand = EAshenCommandMode::None;
    PruneSelection();
    if (UAshenSimulationSubsystem* Sim = Simulation())
    {
        const TArray<int32> UnitIds = SelectedUnitIds();
        if (!UnitIds.IsEmpty() && Sim->IssueHold(UnitIds, IsQueueModifierDown()))
        {
            FVector Center = FVector::ZeroVector;
            int32 Count = 0;
            for (const TWeakObjectPtr<AAshenEntityActor>& Actor : SelectedActors)
            {
                if (Actor.IsValid() && IsUnitArchetype(Actor->GetArchetype()))
                {
                    Center += Actor->GetActorLocation();
                    ++Count;
                }
            }
            RegisterCommandFeedback(Count > 0 ? Center / static_cast<float>(Count) : FVector::ZeroVector, false);
        }
    }
}

void AAshenPlayerController::TrainPrimary()
{
    if (bFrontEndVisible)
    {
        return;
    }
    PruneSelection();
    const int32 BuildingId = SelectedBuildingId();
    if (BuildingId > 0 && Simulation() != nullptr)
    {
        Simulation()->IssueTrain(BuildingId, false);
    }
}

void AAshenPlayerController::TrainSecondary()
{
    if (bFrontEndVisible)
    {
        return;
    }
    PruneSelection();
    const int32 BuildingId = SelectedBuildingId();
    if (BuildingId > 0 && Simulation() != nullptr)
    {
        Simulation()->IssueTrain(BuildingId, true);
    }
}

void AAshenPlayerController::DeploySkirmish()
{
    if (!bFrontEndVisible)
    {
        return;
    }

    bFrontEndVisible = false;
    PendingCommand = EAshenCommandMode::None;
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
    PendingCommand = EAshenCommandMode::None;
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
    ActiveControlGroup = -1;
}

void AAshenPlayerController::PruneSelection()
{
    SelectedActors.RemoveAll([](const TWeakObjectPtr<AAshenEntityActor>& Actor)
    {
        return !Actor.IsValid();
    });
}

void AAshenPlayerController::AddToSelection(AAshenEntityActor* Entity)
{
    if (Entity == nullptr)
    {
        return;
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

void AAshenPlayerController::HandleControlGroup(const int32 GroupIndex)
{
    if (GroupIndex < 0 || GroupIndex >= 10)
    {
        return;
    }

    const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
    if (bControlDown)
    {
        PruneSelection();
        ControlGroups[GroupIndex] = SelectedActors;
        ActiveControlGroup = GroupIndex;
        LastControlGroup = GroupIndex;
        LastControlGroupTime = GetWorld() == nullptr ? 0.0 : GetWorld()->GetTimeSeconds();
        return;
    }

    const bool bAdd = IsQueueModifierDown();
    if (!bAdd)
    {
        ClearSelection();
    }
    ControlGroups[GroupIndex].RemoveAll([](const TWeakObjectPtr<AAshenEntityActor>& Actor)
    {
        return !Actor.IsValid();
    });
    for (const TWeakObjectPtr<AAshenEntityActor>& Actor : ControlGroups[GroupIndex])
    {
        if (Actor.IsValid())
        {
            AddToSelection(Actor.Get());
        }
    }
    ActiveControlGroup = GroupIndex;

    const double Now = GetWorld() == nullptr ? 0.0 : GetWorld()->GetTimeSeconds();
    if (LastControlGroup == GroupIndex && Now - LastControlGroupTime <= 0.36)
    {
        FocusSelection();
    }
    LastControlGroup = GroupIndex;
    LastControlGroupTime = Now;
}

void AAshenPlayerController::FocusSelection()
{
    FVector Center = FVector::ZeroVector;
    int32 Count = 0;
    for (const TWeakObjectPtr<AAshenEntityActor>& Actor : SelectedActors)
    {
        if (Actor.IsValid())
        {
            Center += Actor->GetActorLocation();
            ++Count;
        }
    }
    if (Count > 0)
    {
        if (AAshenCameraPawn* CameraPawn = Cast<AAshenCameraPawn>(GetPawn()))
        {
            CameraPawn->FocusOn(Center / static_cast<float>(Count));
        }
    }
}

void AAshenPlayerController::RegisterCommandFeedback(const FVector& Location, const bool bHostile)
{
    LastCommandLocation = Location;
    LastCommandLocation.Z = 8.0f;
    LastCommandFeedbackTime = GetWorld() == nullptr ? 0.0 : GetWorld()->GetTimeSeconds();
    bLastCommandHostile = bHostile;
}

bool AAshenPlayerController::IsQueueModifierDown() const
{
    return IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
}

TArray<int32> AAshenPlayerController::SelectedUnitIds() const
{
    TArray<int32> Ids;
    Ids.Reserve(SelectedActors.Num());
    for (const TWeakObjectPtr<AAshenEntityActor>& Actor : SelectedActors)
    {
        if (Actor.IsValid() && IsUnitArchetype(Actor->GetArchetype()))
        {
            Ids.Add(Actor->GetEntityId());
        }
    }
    return Ids;
}

TArray<int32> AAshenPlayerController::SelectedWorkerIds() const
{
    TArray<int32> Ids;
    for (const TWeakObjectPtr<AAshenEntityActor>& Actor : SelectedActors)
    {
        if (Actor.IsValid() && Actor->GetArchetype() == EAshenEntityArchetype::Worker)
        {
            Ids.Add(Actor->GetEntityId());
        }
    }
    return Ids;
}

int32 AAshenPlayerController::SelectedBuildingId() const
{
    for (const TWeakObjectPtr<AAshenEntityActor>& Actor : SelectedActors)
    {
        if (Actor.IsValid() && !IsUnitArchetype(Actor->GetArchetype()))
        {
            return Actor->GetEntityId();
        }
    }
    return 0;
}

UAshenSimulationSubsystem* AAshenPlayerController::Simulation() const
{
    return GetWorld() == nullptr ? nullptr : GetWorld()->GetSubsystem<UAshenSimulationSubsystem>();
}
