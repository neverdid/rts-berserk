#include "AshenGameMode.h"

#include "AshenArena.h"
#include "AshenCameraPawn.h"
#include "AshenHUD.h"
#include "AshenPlayerController.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

AAshenGameMode::AAshenGameMode()
{
    DefaultPawnClass = AAshenCameraPawn::StaticClass();
    PlayerControllerClass = AAshenPlayerController::StaticClass();
    HUDClass = AAshenHUD::StaticClass();
}

void AAshenGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorld()->SpawnActor<AAshenArena>();

#if !UE_BUILD_SHIPPING
    const bool bCaptureFrontEnd = FParse::Param(FCommandLine::Get(), TEXT("AshenCaptureFrontEnd"));
    const bool bCaptureBattle = FParse::Param(FCommandLine::Get(), TEXT("AshenCaptureBattle"));
    const bool bCaptureWorld = FParse::Param(FCommandLine::Get(), TEXT("AshenCaptureWorld"));
    if (!bCaptureFrontEnd && !bCaptureBattle && !bCaptureWorld)
    {
        return;
    }

    if (bCaptureBattle || bCaptureWorld)
    {
        FTimerHandle StartHandle;
        GetWorldTimerManager().SetTimer(
            StartHandle,
            [this, bCaptureWorld]()
            {
                if (AAshenPlayerController *Controller =
                        Cast<AAshenPlayerController>(GetWorld()->GetFirstPlayerController()))
                {
                    Controller->StartSkirmish();
                    if (bCaptureWorld)
                    {
                        if (AHUD *HUD = Controller->GetHUD())
                        {
                            HUD->bShowHUD = false;
                        }
                        if (AAshenCameraPawn *Camera = Cast<AAshenCameraPawn>(Controller->GetPawn()))
                        {
                            Camera->FrameWorld();
                        }
                    }
                }
            },
            0.35f, false);
    }

    const float CaptureDelay = bCaptureBattle || bCaptureWorld ? 8.0f : 2.5f;
    FTimerHandle CaptureHandle;
    GetWorldTimerManager().SetTimer(
        CaptureHandle,
        [bCaptureBattle, bCaptureWorld]()
        {
            const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/Automation"));
            IFileManager::Get().MakeDirectory(*Directory, true);
            const TCHAR *ScreenshotName =
                bCaptureWorld ? TEXT("World.png") : (bCaptureBattle ? TEXT("Battle.png") : TEXT("FrontEnd.png"));
            const FString Filename = FPaths::Combine(Directory, ScreenshotName);
            FScreenshotRequest::RequestScreenshot(Filename, true, false);
        },
        CaptureDelay, false);

    FTimerHandle ExitHandle;
    GetWorldTimerManager().SetTimer(
        ExitHandle, []() { FPlatformMisc::RequestExit(false); }, CaptureDelay + 1.5f, false);
#endif
}
