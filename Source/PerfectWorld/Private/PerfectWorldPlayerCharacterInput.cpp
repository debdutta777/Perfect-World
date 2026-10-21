#include "PerfectWorldPlayerCharacter.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldReadingPresenter.h"
#include "UI/PWQuestText.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWPlayerInput, Log, All);

namespace
{
/** Which legacy axis a bound key or stick is, passed as the delegate payload. */
enum ELegacyAxis : int32
{
    LegacyForwardKey,
    LegacyBackKey,
    LegacyRightKey,
    LegacyLeftKey,
    LegacyPadMoveX,
    LegacyPadMoveY,
    LegacyMouseX,
    LegacyMouseY,
    LegacyPadLookX,
    LegacyPadLookY,
    LegacyWheel
};

constexpr float PlayLookPitchLimit = 80.0f; // degrees up and down; keeps the horizon stable and the view from flipping

/** J or the left shoulder button asks the front end HUD to open the quest window; ignored while a menu or a conversation is up. */
void QwRequestQuests(const TWeakObjectPtr<const APawn>& Pawn)
{
    const APawn* Owner = Pawn.Get();
    // A reading panel is not a menu (it does not block the world), but it owns the shoulder buttons while it is open.
    const UPWReadingPresenter* Reading = UPWReadingPresenter::Get(Owner);
    if (Owner != nullptr && !UPWInteractionComponent::IsWorldInputBlocked(Owner) && (Reading == nullptr || !Reading->IsOpen()))
    {
        PWQuest::RequestToggle();
    }
}
} // namespace

bool APerfectWorldPlayerCharacter::WantsEnhancedInput() const
{
    const APlayerController* PlayerController = Cast<APlayerController>(GetController());
    return PlayerController != nullptr && Cast<UEnhancedPlayerInput>(PlayerController->PlayerInput) != nullptr;
}

void APerfectWorldPlayerCharacter::BuildInputAssets()
{
    if (WalkingContext != nullptr)
    {
        return;
    }

    auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type) -> UInputAction*
    {
        UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
        Action->ValueType = Type;
        return Action;
    };
    MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
    LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
    LookStickAction = MakeAction(TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
    RunAction = MakeAction(TEXT("IA_Run"), EInputActionValueType::Boolean);
    JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
    ToggleCameraAction = MakeAction(TEXT("IA_ToggleCamera"), EInputActionValueType::Boolean);
    ZoomWheelAction = MakeAction(TEXT("IA_ZoomWheel"), EInputActionValueType::Axis1D);
    ZoomHeldAction = MakeAction(TEXT("IA_ZoomHeld"), EInputActionValueType::Axis1D);
    InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
    CycleTargetAction = MakeAction(TEXT("IA_CycleTarget"), EInputActionValueType::Boolean);

    WalkingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Walking"));

    auto Swizzle = [this]() -> UInputModifier* { return NewObject<UInputModifierSwizzleAxis>(this); }; // X into Y
    auto Negate = [this]() -> UInputModifier* { return NewObject<UInputModifierNegate>(this); };
    auto StickDeadZone = [this]() -> UInputModifier*
    {
        UInputModifierDeadZone* Zone = NewObject<UInputModifierDeadZone>(this);
        Zone->LowerThreshold = 0.2f;
        Zone->UpperThreshold = 1.0f;
        Zone->Type = EDeadZoneType::Radial;
        return Zone;
    };

    // Move: a key gives 1 on X (right) or Y (forward); swizzle moves X into Y, negate flips the sign.
    WalkingContext->MapKey(MoveAction, EKeys::W).Modifiers.Add(Swizzle());
    FEnhancedActionKeyMapping& Back = WalkingContext->MapKey(MoveAction, EKeys::S);
    Back.Modifiers.Add(Swizzle());
    Back.Modifiers.Add(Negate());
    WalkingContext->MapKey(MoveAction, EKeys::A).Modifiers.Add(Negate());
    WalkingContext->MapKey(MoveAction, EKeys::D);
    WalkingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D).Modifiers.Add(StickDeadZone());

    // Look: the mouse delta, and the right stick as a rate (handled in OnLookStick).
    WalkingContext->MapKey(LookAction, EKeys::Mouse2D);
    WalkingContext->MapKey(LookStickAction, EKeys::Gamepad_Right2D).Modifiers.Add(StickDeadZone());

    WalkingContext->MapKey(RunAction, EKeys::LeftShift);
    WalkingContext->MapKey(RunAction, EKeys::Gamepad_LeftThumbstick);

    WalkingContext->MapKey(JumpAction, EKeys::SpaceBar);
    WalkingContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);

    WalkingContext->MapKey(ToggleCameraAction, EKeys::V);
    WalkingContext->MapKey(ToggleCameraAction, EKeys::Gamepad_FaceButton_Top);

    // Third-person distance: wheel notches, or the D-pad held.
    WalkingContext->MapKey(ZoomWheelAction, EKeys::MouseWheelAxis);
    WalkingContext->MapKey(ZoomHeldAction, EKeys::Gamepad_DPad_Up);
    WalkingContext->MapKey(ZoomHeldAction, EKeys::Gamepad_DPad_Down).Modifiers.Add(Negate());

    // Interaction: E or X acts on the target; Tab or the right shoulder button picks the next one. Neither key is used above
    // (A jumps, Y switches camera, the D-pad zooms, L3 runs, the menu button belongs to the front end HUD). B is left free
    // because the conversation panel uses it to leave.
    WalkingContext->MapKey(InteractAction, EKeys::E);
    WalkingContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Left);
    // Quests: J or the left shoulder button (LB / L1) opens the quest window. The action has no member of its own; the mapping context
    // keeps it, and BindEnhancedInput finds it by name.
    UInputAction* QuestsAction = MakeAction(TEXT("IA_Quests"), EInputActionValueType::Boolean);
    WalkingContext->MapKey(QuestsAction, EKeys::J);
    WalkingContext->MapKey(QuestsAction, EKeys::Gamepad_LeftShoulder);
    WalkingContext->MapKey(CycleTargetAction, EKeys::Tab);
    WalkingContext->MapKey(CycleTargetAction, EKeys::Gamepad_RightShoulder);
}

void APerfectWorldPlayerCharacter::BindEnhancedInput(UEnhancedInputComponent& Input)
{
    Input.BindAction(MoveAction, ETriggerEvent::Triggered, this, &APerfectWorldPlayerCharacter::OnMove);
    Input.BindAction(LookAction, ETriggerEvent::Triggered, this, &APerfectWorldPlayerCharacter::OnLook);
    Input.BindAction(LookStickAction, ETriggerEvent::Triggered, this, &APerfectWorldPlayerCharacter::OnLookStick);
    Input.BindAction(RunAction, ETriggerEvent::Triggered, this, &APerfectWorldPlayerCharacter::OnRunHeld);
    Input.BindAction(JumpAction, ETriggerEvent::Started, this, &APerfectWorldPlayerCharacter::OnJumpStarted);
    Input.BindAction(JumpAction, ETriggerEvent::Completed, this, &APerfectWorldPlayerCharacter::OnJumpCompleted);
    Input.BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &APerfectWorldPlayerCharacter::OnToggleCamera);
    Input.BindAction(ZoomWheelAction, ETriggerEvent::Triggered, this, &APerfectWorldPlayerCharacter::OnZoomWheel);
    Input.BindAction(ZoomHeldAction, ETriggerEvent::Triggered, this, &APerfectWorldPlayerCharacter::OnZoomHeld);
    // Started fires once per press; holding the key never repeats the action.
    Input.BindAction(InteractAction, ETriggerEvent::Started, this, &APerfectWorldPlayerCharacter::OnInteractStarted);
    // Quests (J, LB): found through the mapping context, so the character header gains no member.
    for (const FEnhancedActionKeyMapping& Mapping : WalkingContext->GetMappings())
    {
        if (Mapping.Action != nullptr && Mapping.Action->GetFName() == FName(TEXT("IA_Quests")))
        {
            Input.BindActionInstanceLambda(Mapping.Action, ETriggerEvent::Started,
                [Pawn = TWeakObjectPtr<const APawn>(this)](const FInputActionInstance&) { QwRequestQuests(Pawn); });
            break;
        }
    }
    Input.BindAction(CycleTargetAction, ETriggerEvent::Started, this, &APerfectWorldPlayerCharacter::OnCycleTargetStarted);
}

void APerfectWorldPlayerCharacter::AddWalkingContext()
{
    APlayerController* PlayerController = Cast<APlayerController>(Controller);
    if (PlayerController == nullptr)
    {
        return;
    }
    if (PlayerController->PlayerCameraManager != nullptr)
    {
        PlayerController->PlayerCameraManager->ViewPitchMin = -PlayLookPitchLimit;
        PlayerController->PlayerCameraManager->ViewPitchMax = PlayLookPitchLimit;
    }
    if (!WantsEnhancedInput())
    {
        return;
    }
    BuildInputAssets();
    ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
    {
        Subsystem->AddMappingContext(WalkingContext, 0);
    }
}

void APerfectWorldPlayerCharacter::RemoveWalkingContext(AController* FromController)
{
    const APlayerController* PlayerController = Cast<APlayerController>(FromController);
    ULocalPlayer* LocalPlayer = PlayerController != nullptr ? PlayerController->GetLocalPlayer() : nullptr;
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
    {
        if (WalkingContext != nullptr)
        {
            Subsystem->RemoveMappingContext(WalkingContext);
        }
    }
}

void APerfectWorldPlayerCharacter::NotifyControllerChanged()
{
    AController* OldController = PreviousController; // the base class overwrites this with the new controller
    Super::NotifyControllerChanged();
    if (OldController != nullptr && OldController != Controller)
    {
        RemoveWalkingContext(OldController);
    }
    AddWalkingContext();
}

void APerfectWorldPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (Enhanced != nullptr && WantsEnhancedInput())
    {
        BuildInputAssets();
        BindEnhancedInput(*Enhanced);
        AddWalkingContext(); // idempotent; covers a restart that did not go through NotifyControllerChanged
        UE_LOG(LogPWPlayerInput, Display, TEXT("Player input: Enhanced Input (input component %s, player input %s)."),
               *GetNameSafe(PlayerInputComponent->GetClass()), *GetNameSafe(Cast<APlayerController>(GetController())->PlayerInput->GetClass()));
        return;
    }

    UE_LOG(LogPWPlayerInput, Warning, TEXT("Player input: the Enhanced Input classes are not active (input component %s); using legacy key bindings."),
           *GetNameSafe(PlayerInputComponent != nullptr ? PlayerInputComponent->GetClass() : nullptr));
    if (PlayerInputComponent != nullptr)
    {
        BindLegacyInput(*PlayerInputComponent);
    }
}

void APerfectWorldPlayerCharacter::OnMove(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>(); // X right, Y forward
    ApplyMove(static_cast<float>(Axis.X), static_cast<float>(Axis.Y));
}

void APerfectWorldPlayerCharacter::OnLook(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>(); // X right, Y up, in mouse counts
    ApplyLookDegrees(static_cast<float>(Axis.X) * MouseSensitivity, static_cast<float>(Axis.Y) * MouseSensitivity);
}

void APerfectWorldPlayerCharacter::OnLookStick(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    const UWorld* World = GetWorld();
    const float Step = StickLookDegreesPerSecond * (World != nullptr ? World->GetDeltaSeconds() : 0.0f);
    ApplyLookDegrees(static_cast<float>(Axis.X) * Step, static_cast<float>(Axis.Y) * Step);
}

void APerfectWorldPlayerCharacter::OnRunHeld()
{
    bRunHeldThisFrame = true;
}

void APerfectWorldPlayerCharacter::OnJumpStarted()
{
    if (!IsInputBlocked())
    {
        Jump();
    }
}

void APerfectWorldPlayerCharacter::OnJumpCompleted()
{
    StopJumping();
}

void APerfectWorldPlayerCharacter::OnToggleCamera()
{
    if (!IsInputBlocked())
    {
        ToggleCameraMode();
    }
}

void APerfectWorldPlayerCharacter::OnInteractStarted()
{
    if (Interaction != nullptr && !IsInputBlocked())
    {
        Interaction->TryInteract();
    }
}

void APerfectWorldPlayerCharacter::OnCycleTargetStarted()
{
    if (Interaction != nullptr && !IsInputBlocked())
    {
        Interaction->CycleTarget();
    }
}

void APerfectWorldPlayerCharacter::OnZoomWheel(const FInputActionValue& Value)
{
    ApplyZoomCentimetres(-Value.Get<float>() * ZoomCentimetresPerWheelNotch); // wheel up moves the camera closer
}

void APerfectWorldPlayerCharacter::OnZoomHeld(const FInputActionValue& Value)
{
    const UWorld* World = GetWorld();
    const float Step = ZoomCentimetresPerSecond * (World != nullptr ? World->GetDeltaSeconds() : 0.0f);
    ApplyZoomCentimetres(-Value.Get<float>() * Step); // D-pad up moves the camera closer
}

// ---- Legacy fallback: the same bindings as plain key and axis bindings, as ADefaultPawn creates at runtime. ----

void APerfectWorldPlayerCharacter::BindLegacyInput(UInputComponent& Input)
{
    auto BindAxis = [this, &Input](const FKey& Key, int32 Which)
    {
        Input.BindAxisKey(Key).AxisDelegate.GetDelegateForManualSet().BindUObject(this, &APerfectWorldPlayerCharacter::OnLegacyAxis, Which);
    };
    BindAxis(EKeys::W, LegacyForwardKey);
    BindAxis(EKeys::S, LegacyBackKey);
    BindAxis(EKeys::D, LegacyRightKey);
    BindAxis(EKeys::A, LegacyLeftKey);
    BindAxis(EKeys::Gamepad_LeftX, LegacyPadMoveX);
    BindAxis(EKeys::Gamepad_LeftY, LegacyPadMoveY);
    BindAxis(EKeys::MouseX, LegacyMouseX);
    BindAxis(EKeys::MouseY, LegacyMouseY);
    BindAxis(EKeys::Gamepad_RightX, LegacyPadLookX);
    BindAxis(EKeys::Gamepad_RightY, LegacyPadLookY);
    BindAxis(EKeys::MouseWheelAxis, LegacyWheel);

    for (const FKey& Key : {EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom})
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnJumpStarted);
        Input.BindKey(Key, IE_Released, this, &APerfectWorldPlayerCharacter::OnJumpCompleted);
    }
    for (const FKey& Key : {EKeys::LeftShift, EKeys::Gamepad_LeftThumbstick})
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnLegacyRunPressed);
        Input.BindKey(Key, IE_Released, this, &APerfectWorldPlayerCharacter::OnLegacyRunReleased);
    }
    for (const FKey& Key : {EKeys::V, EKeys::Gamepad_FaceButton_Top})
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnToggleCamera);
    }
    for (const FKey& Key : {EKeys::E, EKeys::Gamepad_FaceButton_Left})
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnInteractStarted); // IE_Pressed ignores repeats
    }
    for (const FKey& Key : {EKeys::J, EKeys::Gamepad_LeftShoulder}) // quests
    {
        FInputKeyBinding Binding(FInputChord(Key), IE_Pressed);
        Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([Pawn = TWeakObjectPtr<const APawn>(this)]() { QwRequestQuests(Pawn); });
        Input.KeyBindings.Emplace(MoveTemp(Binding));
    }
    for (const FKey& Key : {EKeys::Tab, EKeys::Gamepad_RightShoulder})
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnCycleTargetStarted);
    }
    Input.BindKey(EKeys::Gamepad_DPad_Up, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnLegacyZoomIn);
    Input.BindKey(EKeys::Gamepad_DPad_Down, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnLegacyZoomOut);
}

void APerfectWorldPlayerCharacter::OnLegacyAxis(float Value, int32 Which)
{
    const UWorld* World = GetWorld();
    const float StickStep = StickLookDegreesPerSecond * (World != nullptr ? World->GetDeltaSeconds() : 0.0f);
    switch (Which)
    {
    case LegacyForwardKey: ApplyMove(0.0f, Value); break;
    case LegacyBackKey: ApplyMove(0.0f, -Value); break;
    case LegacyRightKey: ApplyMove(Value, 0.0f); break;
    case LegacyLeftKey: ApplyMove(-Value, 0.0f); break;
    case LegacyPadMoveX: ApplyMove(Value, 0.0f); break;
    case LegacyPadMoveY: ApplyMove(0.0f, Value); break;
    case LegacyMouseX: ApplyLookDegrees(Value * MouseSensitivity, 0.0f); break;
    case LegacyMouseY: ApplyLookDegrees(0.0f, Value * MouseSensitivity); break;
    case LegacyPadLookX: ApplyLookDegrees(Value * StickStep, 0.0f); break;
    case LegacyPadLookY: ApplyLookDegrees(0.0f, Value * StickStep); break;
    case LegacyWheel: ApplyZoomCentimetres(-Value * ZoomCentimetresPerWheelNotch); break;
    default: break;
    }
}

void APerfectWorldPlayerCharacter::OnLegacyRunPressed()
{
    bLegacyRunKeyDown = true;
}

void APerfectWorldPlayerCharacter::OnLegacyRunReleased()
{
    bLegacyRunKeyDown = false;
}

void APerfectWorldPlayerCharacter::OnLegacyZoomIn()
{
    ApplyZoomCentimetres(-ZoomCentimetresPerWheelNotch);
}

void APerfectWorldPlayerCharacter::OnLegacyZoomOut()
{
    ApplyZoomCentimetres(ZoomCentimetresPerWheelNotch);
}
