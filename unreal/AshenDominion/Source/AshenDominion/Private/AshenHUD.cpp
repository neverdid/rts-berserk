#include "AshenHUD.h"

#include "AshenEntityActor.h"
#include "AshenPlayerController.h"
#include "AshenSimulationSubsystem.h"
#include "AshenWorldLayout.h"

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
const FLinearColor Verdigris(0.12f, 0.47f, 0.43f, 1.0f);
const FLinearColor ValidGreen(0.18f, 0.68f, 0.32f, 1.0f);

bool ContainsPoint(const FVector2D& Min, const FVector2D& Max, const FVector2D& Point)
{
    return Point.X >= Min.X && Point.X <= Max.X && Point.Y >= Min.Y && Point.Y <= Max.Y;
}

FString StanceLabel(const EAshenStance Stance)
{
    switch (Stance)
    {
    case EAshenStance::Aggressive:
        return TEXT("AGGRESSIVE");
    case EAshenStance::Defensive:
        return TEXT("DEFENSIVE");
    case EAshenStance::Hold:
        return TEXT("STAND GROUND");
    }
    return TEXT("AGGRESSIVE");
}
}

void AAshenHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!bShowHUD || Canvas == nullptr || GetWorld() == nullptr || GEngine == nullptr)
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
    DrawPlacementPreview(*Controller);
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
    const bool bCompact = Width < 980.0f || Height < 650.0f;
    const FAshenPlayerView Player = Simulation.GetPlayerView(0);
    const int32 Selected = Controller.GetSelectedCount();
    const int32 PrimaryEntityId = Controller.GetPrimarySelectedEntityId();
    const FAshenEntityView Entity = Simulation.GetEntityView(PrimaryEntityId);

    const float TopX = 14.0f;
    const float TopY = 14.0f;
    const float TopWidth = Width - 28.0f;
    DrawRect(Ink, TopX, TopY, TopWidth, 72.0f);
    DrawRect(Bronze, TopX, TopY, 4.0f, 72.0f);
    DrawText(TEXT("VOWFALL"), Bone, 30.0f, 25.0f, GEngine->GetMediumFont(), 1.0f, false);
    DrawText(bCompact ? TEXT("CINDER COMPACT") : TEXT("CINDER COMPACT  //  BLACK-IRON FORD"),
             DimBone, 30.0f, 54.0f, GEngine->GetSmallFont(), 0.76f, false);

    const float MetricsX = FMath::Max(bCompact ? 242.0f : 360.0f, Width - (bCompact ? 410.0f : 520.0f));
    const float MetricWidth = (Width - MetricsX - 22.0f) / 4.0f;
    const FString MetricLabels[] = {TEXT("IRON"), TEXT("WARHOST"), TEXT("RESOLVE"), TEXT("RUIN TIDE")};
    const FString MetricValues[] = {
        FString::FromInt(Player.Ore),
        FString::Printf(TEXT("%d/%d"), Player.SupplyUsed, Player.SupplyCap),
        FString::Printf(TEXT("%d%%"), Player.Resolve),
        FString::Printf(TEXT("%d%%"), Simulation.GetRuinTide()),
    };
    const FLinearColor MetricColors[] = {Blood, FLinearColor(0.32f, 0.41f, 0.36f), Bronze, Verdigris};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const float X = MetricsX + static_cast<float>(Index) * MetricWidth;
        DrawRect(MetricColors[Index], X, 32.0f, 3.0f, 34.0f);
        DrawText(MetricLabels[Index], DimBone, X + 9.0f, 27.0f, GEngine->GetSmallFont(), 0.61f, false);
        DrawText(MetricValues[Index], Bone, X + 9.0f, 48.0f, GEngine->GetSmallFont(), 0.91f, false);
    }

    const float ObjectiveWidth = FMath::Min(610.0f, Width - 40.0f);
    const float ObjectiveX = (Width - ObjectiveWidth) * 0.5f;
    DrawRect(FLinearColor(0.008f, 0.011f, 0.012f, 0.88f), ObjectiveX, 94.0f, ObjectiveWidth, 28.0f);
    DrawText(Simulation.GetObjectiveText(), DimBone, ObjectiveX + 14.0f, 101.0f,
             GEngine->GetSmallFont(), 0.72f, false);

    DrawTacticalMap(Simulation);

    FVector2D GridMin;
    FVector2D GridMax;
    Controller.GetCommandButtonRect(0, GridMin, GridMax);
    FVector2D LastMin;
    FVector2D LastMax;
    Controller.GetCommandButtonRect(8, LastMin, LastMax);
    const float TacticalWidth = FMath::Clamp(Width * 0.235f, 152.0f, 238.0f);
    const float PanelX = 20.0f + TacticalWidth + 14.0f;
    const float PanelY = Height - 158.0f;
    const float PanelWidth = FMath::Max(118.0f, GridMin.X - PanelX - 14.0f);
    const float PanelHeight = 138.0f;
    DrawRect(Ink, PanelX, PanelY, PanelWidth, PanelHeight);
    DrawRect(Selected > 0 ? Bronze : FLinearColor(0.18f, 0.19f, 0.18f), PanelX, PanelY, 4.0f, PanelHeight);

    const FString Header = Selected > 1
                               ? FString::Printf(TEXT("WAR BAND  //  %d SELECTED"), Selected)
                               : Selected == 1 ? Entity.Label.ToUpper() : TEXT("NO WAR BAND SELECTED");
    DrawText(Header, Selected > 0 ? Bone : DimBone, PanelX + 16.0f, PanelY + 12.0f,
             GEngine->GetSmallFont(), 0.86f, false);
    FString Status = Selected > 0 ? Simulation.GetEntityOrderLabel(PrimaryEntityId)
                                  : TEXT("THE CROSSING AWAITS YOUR COMMAND");
    if (Controller.GetActiveControlGroup() >= 0)
    {
        Status += FString::Printf(TEXT("  //  GROUP %d"), Controller.GetActiveControlGroup());
    }
    DrawText(Status, DimBone, PanelX + 16.0f, PanelY + 34.0f, GEngine->GetSmallFont(), 0.69f, false);

    if (Selected > 0)
    {
        const float BarX = PanelX + 16.0f;
        const float BarWidth = FMath::Max(60.0f, PanelWidth - 32.0f);
        const float Health = Entity.MaxHitPoints > 0 ? static_cast<float>(Entity.HitPoints) / Entity.MaxHitPoints : 0.0f;
        DrawRect(FLinearColor(0.025f, 0.029f, 0.028f), BarX, PanelY + 58.0f, BarWidth, 8.0f);
        DrawRect(ValidGreen, BarX, PanelY + 58.0f, BarWidth * Health, 8.0f);
        DrawText(FString::Printf(TEXT("HP %d/%d"), Entity.HitPoints, Entity.MaxHitPoints), DimBone,
                 BarX, PanelY + 70.0f, GEngine->GetSmallFont(), 0.62f, false);
        if (Entity.bUnderConstruction)
        {
            DrawRect(Iron, BarX, PanelY + 92.0f, BarWidth, 7.0f);
            DrawRect(Bronze, BarX, PanelY + 92.0f, BarWidth * Entity.ConstructionProgress, 7.0f);
            DrawText(FString::Printf(TEXT("CONSTRUCTION  %d%%"),
                                     FMath::RoundToInt(Entity.ConstructionProgress * 100.0f)),
                     Bone, BarX, PanelY + 105.0f, GEngine->GetSmallFont(), 0.65f, false);
        }
        else if (Entity.QueueCount > 0)
        {
            DrawRect(Iron, BarX, PanelY + 92.0f, BarWidth, 7.0f);
            DrawRect(Bronze, BarX, PanelY + 92.0f, BarWidth * Entity.QueueProgress, 7.0f);
            DrawText(FString::Printf(TEXT("MUSTER QUEUE  %d"), Entity.QueueCount), Bone,
                     BarX, PanelY + 105.0f, GEngine->GetSmallFont(), 0.65f, false);
        }
        else
        {
            DrawText(FString::Printf(TEXT("RESOLVE %d%%  //  %s"), Entity.Resolve, *StanceLabel(Entity.Stance)),
                     DimBone, BarX, PanelY + 96.0f, GEngine->GetSmallFont(), 0.65f, false);
        }
    }

    float MouseX = -1.0f;
    float MouseY = -1.0f;
    Controller.GetMousePosition(MouseX, MouseY);
    for (int32 Slot = 0; Slot < 9; ++Slot)
    {
        FVector2D Min;
        FVector2D Max;
        if (!Controller.GetCommandButtonRect(Slot, Min, Max))
        {
            continue;
        }
        const FString Label = Controller.GetCommandButtonLabel(Slot);
        const bool bEnabled = Controller.IsCommandButtonEnabled(Slot);
        const bool bHovered = ContainsPoint(Min, Max, FVector2D(MouseX, MouseY));
        DrawRect(bHovered && bEnabled ? FLinearColor(0.16f, 0.095f, 0.035f, 0.98f)
                                      : bEnabled ? Iron : FLinearColor(0.025f, 0.029f, 0.029f, 0.82f),
                 Min.X, Min.Y, Max.X - Min.X, Max.Y - Min.Y);
        if (!Label.IsEmpty())
        {
            DrawRect(bEnabled ? Bronze : FLinearColor(0.19f, 0.19f, 0.17f), Min.X, Max.Y - 2.0f,
                     Max.X - Min.X, 2.0f);
            DrawText(Controller.GetCommandButtonHotkey(Slot), bEnabled ? Bronze : DimBone,
                     Min.X + 6.0f, Min.Y + 3.0f, GEngine->GetSmallFont(), 0.58f, false);
            DrawText(Label, bEnabled ? Bone : DimBone, Min.X + 6.0f, Min.Y + 19.0f,
                     GEngine->GetSmallFont(), 0.61f, false);
        }
    }

    const FString ResearchLine = !Player.ActiveResearch.IsEmpty()
                                     ? FString::Printf(TEXT("ARCHIVE  //  %s  %d%%"), *Player.ActiveResearch.ToUpper(),
                                                       FMath::RoundToInt(Player.ResearchProgress * 100.0f))
                                     : FString::Printf(TEXT("TIER %d  //  RELICS %d  //  POWER %s"), Player.TechTier,
                                                       Player.ControlledRelics,
                                                       Player.PowerCooldownSeconds <= 0.0f
                                                           ? TEXT("READY")
                                                           : *FString::Printf(TEXT("%.0fs"), Player.PowerCooldownSeconds));
    DrawText(ResearchLine, DimBone, GridMin.X, GridMin.Y - 19.0f,
             GEngine->GetSmallFont(), 0.62f, false);

    if (!Simulation.GetLastCommandMessage().IsEmpty())
    {
        const float MessageWidth = FMath::Min(520.0f, Width - 40.0f);
        const float MessageX = (Width - MessageWidth) * 0.5f;
        DrawRect(FLinearColor(0.008f, 0.011f, 0.012f, 0.90f), MessageX, PanelY - 33.0f, MessageWidth, 26.0f);
        DrawText(Simulation.GetLastCommandMessage(), Bone, MessageX + 12.0f, PanelY - 27.0f,
                 GEngine->GetSmallFont(), 0.68f, false);
    }

    const FString CommandMode = Controller.GetCommandModeLabel();
    if (!CommandMode.IsEmpty())
    {
        const float ModeWidth = FMath::Min(520.0f, Width - 40.0f);
        const float ModeX = (Width - ModeWidth) * 0.5f;
        DrawRect(FLinearColor(0.025f, 0.018f, 0.014f, 0.96f), ModeX, 102.0f, ModeWidth, 38.0f);
        DrawRect(Blood, ModeX, 102.0f, 4.0f, 38.0f);
        DrawText(CommandMode, Bone, ModeX + 18.0f, 113.0f, GEngine->GetSmallFont(), 0.88f, false);
    }
}

void AAshenHUD::DrawPlacementPreview(const AAshenPlayerController& Controller)
{
    FVector Location;
    EAshenEntityArchetype Building = EAshenEntityArchetype::Barracks;
    bool bValid = false;
    FVector2D Screen;
    if (!Controller.GetPlacementPreview(Location, Building, bValid) ||
        !GetOwningPlayerController()->ProjectWorldLocationToScreen(Location, Screen))
    {
        return;
    }

    const FLinearColor LinearColor = bValid ? ValidGreen : Blood;
    const FColor Color = LinearColor.ToFColor(true);
    const float Radius = Building == EAshenEntityArchetype::Barracks ? 34.0f : 25.0f;
    constexpr int32 Segments = 16;
    FVector2D Previous(Screen.X + Radius, Screen.Y);
    for (int32 Index = 1; Index <= Segments; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / Segments;
        const FVector2D Next(Screen.X + FMath::Cos(Angle) * Radius, Screen.Y + FMath::Sin(Angle) * Radius * 0.55f);
        Draw2DLine(Previous.X, Previous.Y, Next.X, Next.Y, Color);
        Previous = Next;
    }
    DrawText(bValid ? TEXT("SITE CLEAR") : TEXT("SITE BLOCKED"), LinearColor,
             Screen.X - 36.0f, Screen.Y + Radius * 0.72f, GEngine->GetSmallFont(), 0.66f, false);
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

void AAshenHUD::DrawTacticalMap(const UAshenSimulationSubsystem& Simulation)
{
    const float Width = Canvas->SizeX;
    const float Height = Canvas->SizeY;
    const float MapWidth = FMath::Clamp(Width * 0.235f, 152.0f, 238.0f);
    const float MapHeight = MapWidth * (Ashen::WorldLayout::Height / Ashen::WorldLayout::Width);
    const float MapX = 20.0f;
    const float MapY = Height - MapHeight - 20.0f;

    DrawRect(FLinearColor(0.01f, 0.018f, 0.016f, 0.96f), MapX, MapY, MapWidth, MapHeight);
    DrawRect(FLinearColor(0.035f, 0.075f, 0.055f, 0.8f), MapX + 3.0f, MapY + 3.0f,
             MapWidth - 6.0f, MapHeight - 6.0f);
    DrawRect(FLinearColor(0.075f, 0.080f, 0.074f, 0.96f), MapX + MapWidth * 0.20f,
             MapY + MapHeight * 0.10f, MapWidth * 0.19f, MapHeight * 0.31f);
    DrawRect(FLinearColor(0.018f, 0.052f, 0.027f, 0.98f), MapX + MapWidth * 0.64f,
             MapY + MapHeight * 0.63f, MapWidth * 0.22f, MapHeight * 0.28f);
    DrawRect(FLinearColor(0.02f, 0.16f, 0.19f, 0.95f), MapX + MapWidth * 0.465f, MapY + 3.0f,
             MapWidth * 0.07f, MapHeight - 6.0f);
    for (const float CrossingY : {Ashen::WorldLayout::NorthCrossingY, Ashen::WorldLayout::CentralCrossingY,
                                  Ashen::WorldLayout::SouthCrossingY})
    {
        DrawRect(FLinearColor(0.32f, 0.24f, 0.13f, 1.0f), MapX + MapWidth * 0.44f,
                 MapY + MapHeight * (CrossingY / Ashen::WorldLayout::Height) - 1.5f,
                 MapWidth * 0.12f, 3.0f);
    }

    const FAshenVisibilityGridView Visibility = Simulation.GetLocalVisibilityGrid();
    if (Visibility.Columns > 0 && Visibility.Rows > 0 &&
        Visibility.Cells.Num() == Visibility.Columns * Visibility.Rows)
    {
        const float CellWidth = MapWidth / static_cast<float>(Visibility.Columns);
        const float CellHeight = MapHeight / static_cast<float>(Visibility.Rows);
        for (int32 Row = 0; Row < Visibility.Rows; ++Row)
        {
            int32 Column = 0;
            while (Column < Visibility.Columns)
            {
                const EAshenVisibility State = Visibility.Cells[Row * Visibility.Columns + Column];
                int32 RunEnd = Column + 1;
                while (RunEnd < Visibility.Columns &&
                       Visibility.Cells[Row * Visibility.Columns + RunEnd] == State)
                {
                    ++RunEnd;
                }
                if (State != EAshenVisibility::Visible)
                {
                    const FLinearColor FogColor = State == EAshenVisibility::Hidden
                                                      ? FLinearColor(0.002f, 0.003f, 0.004f, 0.94f)
                                                      : FLinearColor(0.008f, 0.012f, 0.012f, 0.56f);
                    DrawRect(FogColor, MapX + static_cast<float>(Column) * CellWidth,
                             MapY + static_cast<float>(Row) * CellHeight,
                             static_cast<float>(RunEnd - Column) * CellWidth + 0.25f, CellHeight + 0.25f);
                }
                Column = RunEnd;
            }
        }
    }

    for (TActorIterator<AAshenEntityActor> It(GetWorld()); It; ++It)
    {
        const AAshenEntityActor* Entity = *It;
        if (Entity->GetOwnerIndex() != 0 && !Entity->IsFogVisible())
        {
            continue;
        }
        const FVector Position = Entity->GetActorLocation();
        const float DotSize = (Entity->GetArchetype() == EAshenEntityArchetype::Command ||
                               Entity->GetArchetype() == EAshenEntityArchetype::Barracks)
                                  ? 5.0f
                                  : 3.0f;
        const float DotX = MapX + FMath::Clamp(Position.X / Ashen::WorldLayout::Width, 0.0f, 1.0f) * MapWidth -
                           DotSize * 0.5f;
        const float DotY = MapY + FMath::Clamp(Position.Y / Ashen::WorldLayout::Height, 0.0f, 1.0f) * MapHeight -
                           DotSize * 0.5f;
        DrawRect(Entity->GetOwnerIndex() == 0 ? Bronze : Blood, DotX, DotY, DotSize, DotSize);
    }

    for (const FAshenControlPointView& Point : Simulation.GetControlPointViews())
    {
        if (Point.Visibility == EAshenVisibility::Hidden)
        {
            continue;
        }
        constexpr float DotSize = 6.0f;
        const float DotX = MapX + FMath::Clamp(Point.WorldPosition.X / Ashen::WorldLayout::Width, 0.0f, 1.0f) *
                                             MapWidth -
                           3.0f;
        const float DotY = MapY + FMath::Clamp(Point.WorldPosition.Y / Ashen::WorldLayout::Height, 0.0f, 1.0f) *
                                             MapHeight -
                           3.0f;
        FLinearColor PointColor = Point.OwnerIndex == 0 ? Bronze : Point.OwnerIndex == 1 ? Blood : Bone;
        if (Point.Visibility == EAshenVisibility::Explored)
        {
            PointColor *= 0.58f;
            PointColor.A = 0.82f;
        }
        DrawRect(PointColor, DotX, DotY, DotSize, DotSize);
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
