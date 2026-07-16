#include "AshenGameMode.h"

#include "AshenArena.h"
#include "AshenCameraPawn.h"
#include "AshenHUD.h"
#include "AshenPlayerController.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
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
    if (!bCaptureFrontEnd && !bCaptureBattle)
    {
        return;
    }

    if (bCaptureBattle)
    {
        FTimerHandle StartHandle;
        GetWorldTimerManager().SetTimer(StartHandle, [this]()
        {
            if (AAshenPlayerController* Controller = Cast<AAshenPlayerController>(GetWorld()->GetFirstPlayerController()))
            {
                Controller->StartSkirmish();
            }
        }, 0.35f, false);
    }

    const float CaptureDelay = bCaptureBattle ? 8.0f : 2.5f;
    FTimerHandle CaptureHandle;
    GetWorldTimerManager().SetTimer(CaptureHandle, [bCaptureBattle]()
    {
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/Automation"));
        IFileManager::Get().MakeDirectory(*Directory, true);
        const FString Filename = FPaths::Combine(Directory,
                                                  bCaptureBattle ? TEXT("Battle.png") : TEXT("FrontEnd.png"));
        FScreenshotRequest::RequestScreenshot(Filename, true, false);
    }, CaptureDelay, false);

    FTimerHandle ExitHandle;
    GetWorldTimerManager().SetTimer(ExitHandle, []()
    {
        FPlatformMisc::RequestExit(false);
    }, CaptureDelay + 1.5f, false);
#endif
}
