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
#include "PerfectWorldKeyBindings.h"
#include "PerfectWorldKeyBindingsContext.h"
#include "PerfectWorldMapSubsystem.h"
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

/** The quests key asks the front end HUD to open the quest window; ignored while a menu, a conversation or a reading panel is up. */
void QwRequestQuests(const TWeakObjectPtr<const APawn>& Pawn)
{
    const APawn* Owner = Pawn.Get();
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
    QuestsAction = MakeAction(TEXT("IA_Quests"), EInputActionValueType::Boolean);
    MapAction = MakeAction(TEXT("IA_ToggleMap"), EInputActionValueType::Boolean);

    WalkingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Walking"));

    // The keys come from the player's key bindings (PerfectWorldKeyBindings.h): the defaults are the keys this code was once written
    // with, and the Controls settings tab changes them. A new action takes its keys from the store the same way: create it above, set
    // its field in MakeKbActions, bind its handler in BindEnhancedInput.
    const auto MakeKbActions = [this]()
    {
        FKbContextActions Actions;
        Actions.Move = MoveAction.Get();
        Actions.Look = LookAction.Get();
        Actions.LookStick = LookStickAction.Get();
        Actions.Run = RunAction.Get();
        Actions.Jump = JumpAction.Get();
        Actions.ToggleCamera = ToggleCameraAction.Get();
        Actions.ZoomWheel = ZoomWheelAction.Get();
        Actions.ZoomHeld = ZoomHeldAction.Get();
        Actions.Interact = InteractAction.Get();
        Actions.CycleTarget = CycleTargetAction.Get();
        Actions.Quests = QuestsAction.Get();
        Actions.Map = MapAction.Get();
        return Actions;
    };
    KbPopulateWalkingContext(*WalkingContext, MakeKbActions(), *this, FKbBindingStore::Get().Current());
    PushMapCloseKeys();

    // Rebuild the mappings in place whenever the player changes a binding, so a new key works at once.
    FKbBindingStore::Get().OnChanged.AddWeakLambda(this, [this, MakeKbActions]()
    {
        if (WalkingContext == nullptr)
        {
            return;
        }
        KbPopulateWalkingContext(*WalkingContext, MakeKbActions(), *this, FKbBindingStore::Get().Current());
        PushMapCloseKeys();
        const APlayerController* PlayerController = Cast<APlayerController>(GetController());
        ULocalPlayer* LocalPlayer = PlayerController != nullptr ? PlayerController->GetLocalPlayer() : nullptr;
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
        {
            FModifyContextOptions Options;
            Options.bForceImmediately = true;
            Subsystem->RequestRebuildControlMappings(Options);
        }
    });
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
    Input.BindAction(CycleTargetAction, ETriggerEvent::Started, this, &APerfectWorldPlayerCharacter::OnCycleTargetStarted);
    Input.BindActionInstanceLambda(QuestsAction.Get(), ETriggerEvent::Started,
        [Pawn = TWeakObjectPtr<const APawn>(this)](const FInputActionInstance&) { QwRequestQuests(Pawn); });
    Input.BindAction(MapAction, ETriggerEvent::Started, this, &APerfectWorldPlayerCharacter::OnToggleMapStarted);
}

void APerfectWorldPlayerCharacter::OnToggleMapStarted()
{
    if (IsInputBlocked())
    {
        return;
    }
    if (UPWMapSubsystem* Map = UPWMapSubsystem::Get(this))
    {
        Map->ToggleWorldMap();
    }
}

void APerfectWorldPlayerCharacter::PushMapCloseKeys() const
{
    UPWMapSubsystem* Map = UPWMapSubsystem::Get(this);
    if (Map == nullptr)
    {
        return;
    }
    TArray<FKey> Keys;
    for (const EKbDevice Device : {EKbDevice::Keyboard, EKbDevice::Gamepad})
    {
        const FKey Key = FKbBindingStore::Get().Current().Get(EKbAction::Map, Device);
        if (Key.IsValid())
        {
            Keys.Add(Key);
        }
    }
    Map->SetMapCloseKeys(Keys);
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
    // The same key bindings as the Enhanced Input path, read once here; this fallback does not follow later changes.
    const FKbBindingSet& Keys = FKbBindingStore::Get().Current();
    const auto KeyOf = [&Keys](EKbAction Action, EKbDevice Device) { return Keys.Get(Action, Device); };
    const auto BindKeyAxis = [&BindAxis](const FKey& Key, int32 Which)
    {
        if (Key.IsValid())
        {
            BindAxis(Key, Which);
        }
    };
    BindKeyAxis(KeyOf(EKbAction::MoveForward, EKbDevice::Keyboard), LegacyForwardKey);
    BindKeyAxis(KeyOf(EKbAction::MoveBack, EKbDevice::Keyboard), LegacyBackKey);
    BindKeyAxis(KeyOf(EKbAction::MoveRight, EKbDevice::Keyboard), LegacyRightKey);
    BindKeyAxis(KeyOf(EKbAction::MoveLeft, EKbDevice::Keyboard), LegacyLeftKey);
    BindAxis(EKeys::Gamepad_LeftX, LegacyPadMoveX);
    BindAxis(EKeys::Gamepad_LeftY, LegacyPadMoveY);
    BindAxis(EKeys::MouseX, LegacyMouseX);
    BindAxis(EKeys::MouseY, LegacyMouseY);
    BindAxis(EKeys::Gamepad_RightX, LegacyPadLookX);
    BindAxis(EKeys::Gamepad_RightY, LegacyPadLookY);
    BindAxis(EKeys::MouseWheelAxis, LegacyWheel);

    // Both of an action's keys, skipping one that is unbound.
    const auto KeysOf = [&KeyOf](EKbAction Action)
    {
        TArray<FKey, TInlineAllocator<2>> Found;
        for (const EKbDevice Device : {EKbDevice::Keyboard, EKbDevice::Gamepad})
        {
            const FKey Key = KeyOf(Action, Device);
            if (Key.IsValid() && !KbIsWheelKey(Key))
            {
                Found.Add(Key);
            }
        }
        return Found;
    };
    for (const FKey& Key : KeysOf(EKbAction::Jump))
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnJumpStarted);
        Input.BindKey(Key, IE_Released, this, &APerfectWorldPlayerCharacter::OnJumpCompleted);
    }
    for (const FKey& Key : KeysOf(EKbAction::Run))
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnLegacyRunPressed);
        Input.BindKey(Key, IE_Released, this, &APerfectWorldPlayerCharacter::OnLegacyRunReleased);
    }
    for (const FKey& Key : KeysOf(EKbAction::ToggleCamera))
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnToggleCamera);
    }
    for (const FKey& Key : KeysOf(EKbAction::Interact))
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnInteractStarted); // IE_Pressed ignores repeats
    }
    for (const FKey& Key : KeysOf(EKbAction::CycleTarget))
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnCycleTargetStarted);
    }
    for (const FKey& Key : KeysOf(EKbAction::ZoomIn))
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnLegacyZoomIn);
    }
    for (const FKey& Key : KeysOf(EKbAction::ZoomOut))
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnLegacyZoomOut);
    }
    for (const FKey& Key : KeysOf(EKbAction::Map))
    {
        Input.BindKey(Key, IE_Pressed, this, &APerfectWorldPlayerCharacter::OnToggleMapStarted);
    }
    for (const FKey& Key : KeysOf(EKbAction::Quests))
    {
        FInputKeyBinding Binding(FInputChord(Key), IE_Pressed);
        Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([Pawn = TWeakObjectPtr<const APawn>(this)]() { QwRequestQuests(Pawn); });
        Input.KeyBindings.Emplace(MoveTemp(Binding));
    }
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
