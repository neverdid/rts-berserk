#include "AshenHUD.h"

#include "AshenEntityActor.h"
#include "AshenPlayerController.h"
#include "AshenSimulationSubsystem.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
const FLinearColor Ink(0.012f, 0.015f, 0.016f, 0.94f);
const FLinearColor Iron(0.065f, 0.075f, 0.074f, 0.96f);
const FLinearColor Bone(0.82f, 0.78f, 0.67f, 1.0f);
const FLinearColor DimBone(0.48f, 0.47f, 0.43f, 1.0f);
const FLinearColor Bronze(0.78f, 0.43f, 0.10f, 1.0f);
const FLinearColor Blood(0.58f, 0.035f, 0.045f, 1.0f);
constexpr float WorldWidth = 3'840.0f;
constexpr float WorldHeight = 2'160.0f;

bool ContainsPoint(const FVector2D& Min, const FVector2D& Max, const FVector2D& Point)
{
    return Point.X >= Min.X && Point.X <= Max.X && Point.Y >= Min.Y && Point.Y <= Max.Y;
}
}

void AAshenHUD::DrawHUD()
{
    Super::DrawHUD();
    if (Canvas == nullptr || GetWorld() == nullptr || GEngine == nullptr)
    {
        return;
    }

    const UAshenSimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UAshenSimulationSubsystem>();
    const AAshenPlayerController* Controller = Cast<AAshenPlayerController>(GetOwningPlayerController());
    if (Simulation == nullptr || Controller == nullptr)
    {
        return;
    }

    if (Controller->IsFrontEndVisible())
    {
        DrawFrontEnd(*Controller);
        return;
    }

    DrawOrderRoute(*Controller, *Simulation);
    DrawCommandFeedback(*Controller);
    DrawBattleHud(*Controller, *Simulation);
    DrawSelectionMarquee(*Controller);
    if (Simulation->IsMatchOver())
    {
        DrawMatchResult(*Simulation);
    }
}

void AAshenHUD::DrawFrontEnd(const AAshenPlayerController& Controller)
{
    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const bool bCompact = Height < 650.0f;
    const float Margin = Width < 760.0f ? 28.0f : 64.0f;

    DrawRect(FLinearColor(0.004f, 0.006f, 0.007f, 0.62f), 0.0f, 0.0f, Width, Height);
    DrawRect(FLinearColor(0.012f, 0.016f, 0.017f, 0.76f), 0.0f, 0.0f, FMath::Min(520.0f, Width * 0.54f), Height);
    DrawRect(Blood, 0.0f, 0.0f, 5.0f, Height);

    const float TitleScale = bCompact ? 1.36f : 1.70f;
    const float TitleY = bCompact ? 34.0f : 62.0f;
    const float RuleY = bCompact ? 88.0f : 132.0f;
    const float ChapterY = bCompact ? 103.0f : 150.0f;
    const float StoryOneY = bCompact ? 141.0f : 190.0f;
    const float StoryTwoY = bCompact ? 163.0f : 213.0f;
    DrawText(TEXT("VOWFALL"), Bone, Margin, TitleY, GEngine->GetLargeFont(), TitleScale, false);
    DrawRect(Bronze, Margin, RuleY, 122.0f, 3.0f);
    DrawText(TEXT("THE BRIDGE OF NAMES"), Bronze, Margin, ChapterY,
             GEngine->GetMediumFont(), 0.95f, false);

    DrawText(TEXT("New iron remembers a name older than law."), DimBone, Margin, StoryOneY,
             GEngine->GetSmallFont(), 1.0f, false);
    DrawText(TEXT("At the ford, three mercies become a war."), DimBone, Margin, StoryTwoY,
             GEngine->GetSmallFont(), 1.0f, false);

    FVector2D ButtonMin;
    FVector2D ButtonMax;
    if (Controller.GetFrontEndPrimaryButton(ButtonMin, ButtonMax))
    {
        float MouseX = -1.0f;
        float MouseY = -1.0f;
        Controller.GetMousePosition(MouseX, MouseY);
        const bool bHovered = ContainsPoint(ButtonMin, ButtonMax, FVector2D(MouseX, MouseY));
        const float ButtonWidth = ButtonMax.X - ButtonMin.X;
        const float ButtonHeight = ButtonMax.Y - ButtonMin.Y;
        DrawRect(bHovered ? FLinearColor(0.18f, 0.095f, 0.035f, 0.98f) : Iron,
                 ButtonMin.X, ButtonMin.Y, ButtonWidth, ButtonHeight);
        for (int32 Stripe = 0; Stripe < 5; ++Stripe)
        {
            DrawRect(FLinearColor(0.23f, 0.18f, 0.11f, bHovered ? 0.22f : 0.12f),
                     ButtonMin.X + 4.0f, ButtonMin.Y + 8.0f + static_cast<float>(Stripe) * 10.0f,
                     ButtonWidth - 8.0f, 2.0f);
        }
        DrawRect(bHovered ? Bronze : FLinearColor(0.36f, 0.28f, 0.16f, 1.0f),
                 ButtonMin.X, ButtonMin.Y + ButtonHeight - 3.0f, ButtonWidth, 3.0f);
        DrawText(TEXT("BEGIN SKIRMISH"), bHovered ? Bone : FLinearColor(0.72f, 0.70f, 0.64f),
                 ButtonMin.X + 22.0f, ButtonMin.Y + 18.0f, GEngine->GetMediumFont(), 1.0f, false);

        const float LockedY = ButtonMax.Y + (bCompact ? 9.0f : 14.0f);
        const float LockedHeight = bCompact ? 39.0f : 46.0f;
        const float LockedGap = bCompact ? 46.0f : 54.0f;
        DrawRect(FLinearColor(0.025f, 0.029f, 0.029f, 0.86f), ButtonMin.X, LockedY, ButtonWidth, LockedHeight);
        DrawText(TEXT("STORY  //  THREE BEGINNINGS"), DimBone, ButtonMin.X + 18.0f, LockedY + 10.0f,
                 GEngine->GetSmallFont(), 0.95f, false);
        DrawText(TEXT("LOCKED"), FLinearColor(0.34f, 0.31f, 0.26f), ButtonMax.X - 70.0f, LockedY + 10.0f,
                 GEngine->GetSmallFont(), 0.85f, false);

        DrawRect(FLinearColor(0.025f, 0.029f, 0.029f, 0.86f), ButtonMin.X, LockedY + LockedGap,
                 ButtonWidth, LockedHeight);
        DrawText(TEXT("WARHOST  //  PVP"), DimBone, ButtonMin.X + 18.0f, LockedY + LockedGap + 10.0f,
                 GEngine->GetSmallFont(), 0.95f, false);
        DrawText(TEXT("LOCKED"), FLinearColor(0.34f, 0.31f, 0.26f), ButtonMax.X - 70.0f,
                 LockedY + LockedGap + 10.0f,
                 GEngine->GetSmallFont(), 0.85f, false);
    }

    if (Width >= 980.0f)
    {
        const float StoryX = Width - 410.0f;
        const float StoryY = Height - 165.0f;
        DrawRect(Blood, StoryX, StoryY, 3.0f, 104.0f);
        DrawText(TEXT("THE BRIDGE OF NAMES"), Bone, StoryX + 18.0f, StoryY,
                 GEngine->GetMediumFont(), 1.0f, false);
        DrawText(TEXT("Cinder Compact"), Bronze, StoryX + 18.0f, StoryY + 34.0f,
                 GEngine->GetSmallFont(), 0.95f, false);
        DrawText(TEXT("versus the Gloam Ascendancy"), DimBone, StoryX + 18.0f, StoryY + 58.0f,
                 GEngine->GetSmallFont(), 0.95f, false);
        DrawText(TEXT("Black-Iron Ford. One memory beneath it."), DimBone, StoryX + 18.0f, StoryY + 82.0f,
                 GEngine->GetSmallFont(), 0.95f, false);
    }

    const float FooterY = bCompact ? FMath::Min(442.0f, Height - 28.0f) : Height - 34.0f;
    DrawText(TEXT("VOWFALL  //  UNREAL DEVELOPMENT BUILD"), FLinearColor(0.29f, 0.30f, 0.29f),
             Margin, FooterY, GEngine->GetSmallFont(), 0.82f, false);
}

void AAshenHUD::DrawBattleHud(const AAshenPlayerController& Controller,
                              const UAshenSimulationSubsystem& Simulation)
{
    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const bool bCompact = Height < 650.0f;
    const float SafeWidth = bCompact ? Width * 0.78f : Width;
    const float SafeHeight = bCompact ? Height * 0.78f : Height;
    const FAshenPlayerView Player = Simulation.GetPlayerView(0);
    const int32 Selected = Controller.GetSelectedCount();
    const int32 PrimaryEntityId = Controller.GetPrimarySelectedEntityId();

    const float LeftWidth = FMath::Min(410.0f, (SafeWidth - 64.0f) * 0.52f);
    DrawRect(Ink, 20.0f, 18.0f, LeftWidth, 66.0f);
    DrawRect(Bronze, 20.0f, 18.0f, 4.0f, 66.0f);
    DrawText(TEXT("VOWFALL"), Bone, 38.0f, 29.0f, GEngine->GetMediumFont(), 1.0f, false);
    DrawText(bCompact ? TEXT("CINDER COMPACT  //  BLACK-IRON FORD")
                      : TEXT("CINDER COMPACT  //  THE BRIDGE OF NAMES"),
             DimBone, 38.0f, 57.0f,
             GEngine->GetSmallFont(), 0.84f, false);

    const float ResourceWidth = FMath::Min(350.0f, (SafeWidth - 64.0f) * 0.42f);
    const float ResourceX = SafeWidth - ResourceWidth - 20.0f;
    DrawRect(Ink, ResourceX, 18.0f, ResourceWidth, 66.0f);
    DrawRect(Bronze, ResourceX, 82.0f, ResourceWidth, 2.0f);
    DrawRect(FLinearColor(0.57f, 0.16f, 0.055f), ResourceX + 18.0f, 35.0f, 13.0f, 13.0f);
    DrawText(FString::Printf(TEXT("CURSED IRON  %d"), Player.Ore), Bone, ResourceX + 42.0f, 30.0f,
             GEngine->GetSmallFont(), 0.94f, false);
    DrawRect(FLinearColor(0.33f, 0.40f, 0.37f), ResourceX + 18.0f, 59.0f, 13.0f, 13.0f);
    DrawText(FString::Printf(TEXT("WARHOST  %d / %d"), Player.SupplyUsed, Player.SupplyCap), Bone,
             ResourceX + 42.0f, 54.0f, GEngine->GetSmallFont(), 0.94f, false);

    DrawTacticalMap();

    const float TacticalWidth = FMath::Min(238.0f, SafeWidth * 0.24f);
    const float PanelX = 20.0f + TacticalWidth + 14.0f;
    const float PanelWidth = FMath::Max(220.0f, SafeWidth - PanelX - 20.0f);
    const float PanelHeight = 104.0f;
    const float PanelY = SafeHeight - PanelHeight - 20.0f;
    DrawRect(Ink, PanelX, PanelY, PanelWidth, PanelHeight);
    DrawRect(Selected > 0 ? Bronze : FLinearColor(0.18f, 0.19f, 0.18f), PanelX, PanelY, 4.0f, PanelHeight);
    DrawText(Selected > 0 ? TEXT("WAR BAND READY") : TEXT("NO WAR BAND SELECTED"),
             Selected > 0 ? Bone : DimBone, PanelX + 20.0f, PanelY + 14.0f,
             GEngine->GetMediumFont(), 0.9f, false);
    FString Status = Selected > 0
                         ? FString::Printf(TEXT("%d CINDER COMPACT  //  %s"), Selected,
                                           *Simulation.GetEntityOrderLabel(PrimaryEntityId))
                         : TEXT("THE CROSSING AWAITS YOUR COMMAND");
    if (Controller.GetActiveControlGroup() >= 0)
    {
        Status += FString::Printf(TEXT("  //  GROUP %d"), Controller.GetActiveControlGroup());
    }
    DrawText(Status, DimBone, PanelX + 20.0f, PanelY + 42.0f, GEngine->GetSmallFont(), 0.82f, false);

    const bool bBuildingSelected = PrimaryEntityId > 0 &&
        (Simulation.GetEntityArchetype(PrimaryEntityId) == EAshenEntityArchetype::Command ||
         Simulation.GetEntityArchetype(PrimaryEntityId) == EAshenEntityArchetype::Barracks ||
         Simulation.GetEntityArchetype(PrimaryEntityId) == EAshenEntityArchetype::Turret);
    DrawRect(Iron, PanelX + 12.0f, PanelY + 68.0f, PanelWidth - 24.0f, 25.0f);
    DrawText(bBuildingSelected ? TEXT("Q  TRAIN I     E  TRAIN II     R  RALLY")
                               : (bCompact ? TEXT("A  ADVANCE    S  STOP    H  HOLD    P  PATROL")
                                           : TEXT("A  ADVANCE    S  STOP    H  HOLD    P  PATROL    SHIFT  QUEUE")),
             Selected > 0 ? Bone : DimBone, PanelX + 22.0f, PanelY + 72.0f,
             GEngine->GetSmallFont(), bCompact ? 0.68f : 0.78f, false);

    const FString CommandMode = Controller.GetCommandModeLabel();
    if (!CommandMode.IsEmpty())
    {
        const float ModeWidth = FMath::Min(420.0f, SafeWidth - 40.0f);
        const float ModeX = (SafeWidth - ModeWidth) * 0.5f;
        DrawRect(FLinearColor(0.025f, 0.018f, 0.014f, 0.96f), ModeX, 102.0f, ModeWidth, 38.0f);
        DrawRect(Blood, ModeX, 102.0f, 4.0f, 38.0f);
        DrawText(CommandMode, Bone, ModeX + 18.0f, 113.0f, GEngine->GetSmallFont(), 0.88f, false);
    }
}

void AAshenHUD::DrawOrderRoute(const AAshenPlayerController& Controller,
                               const UAshenSimulationSubsystem& Simulation)
{
    const int32 EntityId = Controller.GetPrimarySelectedEntityId();
    const TArray<FVector> Route = Simulation.GetEntityRoute(EntityId);
    if (EntityId <= 0 || Route.IsEmpty())
    {
        return;
    }

    const AAshenEntityActor* SelectedActor = nullptr;
    for (TActorIterator<AAshenEntityActor> It(GetWorld()); It; ++It)
    {
        if (It->GetEntityId() == EntityId)
        {
            SelectedActor = *It;
            break;
        }
    }
    if (SelectedActor == nullptr)
    {
        return;
    }

    FVector2D Previous;
    if (!GetOwningPlayerController()->ProjectWorldLocationToScreen(SelectedActor->GetActorLocation(), Previous))
    {
        return;
    }
    const FColor RouteColor = FLinearColor(0.78f, 0.43f, 0.10f, 0.72f).ToFColor(true);
    const FColor WaypointColor = FLinearColor(0.88f, 0.58f, 0.18f, 0.9f).ToFColor(true);
    for (const FVector& Waypoint : Route)
    {
        FVector2D Screen;
        if (!GetOwningPlayerController()->ProjectWorldLocationToScreen(Waypoint, Screen))
        {
            continue;
        }
        Draw2DLine(Previous.X, Previous.Y, Screen.X, Screen.Y, RouteColor);
        constexpr float Mark = 4.0f;
        Draw2DLine(Screen.X, Screen.Y - Mark, Screen.X + Mark, Screen.Y, WaypointColor);
        Draw2DLine(Screen.X + Mark, Screen.Y, Screen.X, Screen.Y + Mark, WaypointColor);
        Draw2DLine(Screen.X, Screen.Y + Mark, Screen.X - Mark, Screen.Y, WaypointColor);
        Draw2DLine(Screen.X - Mark, Screen.Y, Screen.X, Screen.Y - Mark, WaypointColor);
        Previous = Screen;
    }
}

void AAshenHUD::DrawCommandFeedback(const AAshenPlayerController& Controller)
{
    FVector Location;
    bool bHostile = false;
    float Strength = 0.0f;
    FVector2D Screen;
    if (!Controller.GetCommandFeedback(Location, bHostile, Strength) ||
        !GetOwningPlayerController()->ProjectWorldLocationToScreen(Location, Screen))
    {
        return;
    }

    const float Radius = 8.0f + (1.0f - Strength) * 15.0f;
    FLinearColor Color = bHostile ? Blood : Bronze;
    Color.A = Strength;
    const FColor FeedbackColor = Color.ToFColor(true);
    Draw2DLine(Screen.X, Screen.Y - Radius, Screen.X + Radius, Screen.Y, FeedbackColor);
    Draw2DLine(Screen.X + Radius, Screen.Y, Screen.X, Screen.Y + Radius, FeedbackColor);
    Draw2DLine(Screen.X, Screen.Y + Radius, Screen.X - Radius, Screen.Y, FeedbackColor);
    Draw2DLine(Screen.X - Radius, Screen.Y, Screen.X, Screen.Y - Radius, FeedbackColor);
}

void AAshenHUD::DrawTacticalMap()
{
    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const bool bCompact = Height < 650.0f;
    const float SafeWidth = bCompact ? Width * 0.78f : Width;
    const float SafeHeight = bCompact ? Height * 0.78f : Height;
    const float MapWidth = FMath::Min(238.0f, SafeWidth * 0.24f);
    const float MapHeight = MapWidth * 0.5625f;
    const float MapX = 20.0f;
    const float MapY = SafeHeight - MapHeight - 20.0f;

    DrawRect(FLinearColor(0.01f, 0.018f, 0.016f, 0.96f), MapX, MapY, MapWidth, MapHeight);
    DrawRect(FLinearColor(0.035f, 0.075f, 0.055f, 0.8f), MapX + 3.0f, MapY + 3.0f,
             MapWidth - 6.0f, MapHeight - 6.0f);
    DrawRect(FLinearColor(0.02f, 0.16f, 0.19f, 0.95f), MapX + MapWidth * 0.47f, MapY + 3.0f,
             MapWidth * 0.06f, MapHeight - 6.0f);
    DrawRect(FLinearColor(0.32f, 0.24f, 0.13f, 1.0f), MapX + MapWidth * 0.44f,
             MapY + MapHeight * 0.28f, MapWidth * 0.12f, 3.0f);
    DrawRect(FLinearColor(0.32f, 0.24f, 0.13f, 1.0f), MapX + MapWidth * 0.44f,
             MapY + MapHeight * 0.68f, MapWidth * 0.12f, 3.0f);

    for (TActorIterator<AAshenEntityActor> It(GetWorld()); It; ++It)
    {
        const AAshenEntityActor* Entity = *It;
        const FVector Position = Entity->GetActorLocation();
        const float DotSize = (Entity->GetArchetype() == EAshenEntityArchetype::Command ||
                               Entity->GetArchetype() == EAshenEntityArchetype::Barracks)
                                  ? 5.0f
                                  : 3.0f;
        const float DotX = MapX + FMath::Clamp(Position.X / WorldWidth, 0.0f, 1.0f) * MapWidth - DotSize * 0.5f;
        const float DotY = MapY + FMath::Clamp(Position.Y / WorldHeight, 0.0f, 1.0f) * MapHeight - DotSize * 0.5f;
        DrawRect(Entity->GetOwnerIndex() == 0 ? Bronze : Blood, DotX, DotY, DotSize, DotSize);
    }

    DrawRect(Bronze, MapX, MapY, MapWidth, 2.0f);
    DrawRect(Bronze, MapX, MapY + MapHeight - 2.0f, MapWidth, 2.0f);
    DrawRect(Bronze, MapX, MapY, 2.0f, MapHeight);
    DrawRect(Bronze, MapX + MapWidth - 2.0f, MapY, 2.0f, MapHeight);
}

void AAshenHUD::DrawSelectionMarquee(const AAshenPlayerController& Controller)
{
    FVector2D SelectionMin;
    FVector2D SelectionMax;
    if (!Controller.GetSelectionBox(SelectionMin, SelectionMax))
    {
        return;
    }

    const float Width = SelectionMax.X - SelectionMin.X;
    const float Height = SelectionMax.Y - SelectionMin.Y;
    DrawRect(FLinearColor(0.72f, 0.36f, 0.06f, 0.12f), SelectionMin.X, SelectionMin.Y, Width, Height);
    DrawRect(Bronze, SelectionMin.X, SelectionMin.Y, Width, 1.5f);
    DrawRect(Bronze, SelectionMin.X, SelectionMax.Y - 1.5f, Width, 1.5f);
    DrawRect(Bronze, SelectionMin.X, SelectionMin.Y, 1.5f, Height);
    DrawRect(Bronze, SelectionMax.X - 1.5f, SelectionMin.Y, 1.5f, Height);
}

void AAshenHUD::DrawMatchResult(const UAshenSimulationSubsystem& Simulation)
{
    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float PanelWidth = FMath::Min(540.0f, Width - 48.0f);
    const float PanelHeight = 152.0f;
    const float X = (Width - PanelWidth) * 0.5f;
    const float Y = (Height - PanelHeight) * 0.5f;
    const bool bWon = Simulation.DidLocalPlayerWin();

    DrawRect(FLinearColor(0.005f, 0.007f, 0.008f, 0.94f), X, Y, PanelWidth, PanelHeight);
    DrawRect(bWon ? Bronze : Blood, X, Y, 5.0f, PanelHeight);
    DrawText(bWon ? TEXT("THE BRIDGE HOLDS") : TEXT("THE COMPACT LINE BREAKS"), Bone,
             X + 34.0f, Y + 32.0f, GEngine->GetLargeFont(), 1.05f, false);
    DrawText(bWon ? TEXT("The Gloam Ascendancy has been broken.")
                  : TEXT("The Gloam Ascendancy claims another night."),
             DimBone, X + 36.0f, Y + 91.0f, GEngine->GetSmallFont(), 1.0f, false);
}
