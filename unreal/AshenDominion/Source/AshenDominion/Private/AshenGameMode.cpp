#include "AshenGameMode.h"

#include "AshenArena.h"
#include "AshenCameraPawn.h"
#include "AshenHUD.h"
#include "AshenPlayerController.h"

#include "Engine/World.h"

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
}
