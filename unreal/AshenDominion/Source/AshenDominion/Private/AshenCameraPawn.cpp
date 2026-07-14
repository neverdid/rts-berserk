#include "AshenCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

AAshenCameraPawn::AAshenCameraPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
    CameraArm->SetupAttachment(SceneRoot);
    CameraArm->SetUsingAbsoluteRotation(true);
    CameraArm->SetRelativeRotation({-58.0f, -42.0f, 0.0f});
    CameraArm->TargetArmLength = 2'250.0f;
    CameraArm->bDoCollisionTest = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
    Camera->FieldOfView = 52.0f;

    SetActorLocation({820.0f, 1'080.0f, 0.0f});
}

void AAshenCameraPawn::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    float EdgeForward = 0.0f;
    float EdgeRight = 0.0f;
    if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        int32 ViewportWidth = 0;
        int32 ViewportHeight = 0;
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
        if (ViewportWidth > 0 && ViewportHeight > 0 && PlayerController->GetMousePosition(MouseX, MouseY))
        {
            constexpr float EdgeSize = 18.0f;
            EdgeRight = MouseX <= EdgeSize ? -1.0f : (MouseX >= ViewportWidth - EdgeSize ? 1.0f : 0.0f);
            EdgeForward = MouseY <= EdgeSize ? 1.0f : (MouseY >= ViewportHeight - EdgeSize ? -1.0f : 0.0f);
        }
    }

    const FRotator YawRotation(0.0f, CameraArm->GetComponentRotation().Yaw, 0.0f);
    const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
    const float MoveForward = FMath::Clamp(ForwardInput + EdgeForward, -1.0f, 1.0f);
    const float MoveRight = FMath::Clamp(RightInput + EdgeRight, -1.0f, 1.0f);
    const FVector Delta = (Forward * MoveForward + Right * MoveRight) * 900.0f * DeltaSeconds;
    AddActorWorldOffset(Delta, false);

    FVector Location = GetActorLocation();
    Location.X = FMath::Clamp(Location.X, 0.0f, 3'840.0f);
    Location.Y = FMath::Clamp(Location.Y, 0.0f, 2'160.0f);
    Location.Z = 0.0f;
    SetActorLocation(Location);

    CameraArm->TargetArmLength = FMath::FInterpTo(CameraArm->TargetArmLength, DesiredArmLength,
                                                  DeltaSeconds, 10.0f);
}

void AAshenCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("CameraForward"), this, &AAshenCameraPawn::SetForwardInput);
    PlayerInputComponent->BindAxis(TEXT("CameraRight"), this, &AAshenCameraPawn::SetRightInput);
    PlayerInputComponent->BindAxis(TEXT("CameraZoom"), this, &AAshenCameraPawn::AddZoomInput);
}

void AAshenCameraPawn::SetForwardInput(const float Value)
{
    ForwardInput = Value;
}

void AAshenCameraPawn::SetRightInput(const float Value)
{
    RightInput = Value;
}

void AAshenCameraPawn::AddZoomInput(const float Value)
{
    DesiredArmLength = FMath::Clamp(DesiredArmLength - Value * 260.0f, 750.0f, 3'600.0f);
}
