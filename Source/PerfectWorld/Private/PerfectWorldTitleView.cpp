#include "PerfectWorldTitleView.h"

#include "Camera/CameraComponent.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldPresentationSettings.h"
#include "UObject/UObjectBase.h"

namespace
{
// The rest frame. Tune these first when a capture shows the shopfront or the street sitting badly.
// The camera stands on the road 5 m behind the Shanti Chai block at eye height for a tall adult, and looks up the lane and
// across to the +Y row. With a 56 degree horizontal field the shopfront (X 600 to 1300, Y 520) lands between roughly 66% and
// 86% of the screen width, and the facades run back to the left behind the menu.
constexpr double TvRestX = -500.0;
constexpr double TvRestY = -190.0;
constexpr double TvEyeAboveRoad = 160.0;
constexpr double TvRestYaw = 12.0;
constexpr double TvRestPitch = 2.0;
constexpr float TvFov = 56.0f;

// The drift: a shallow truck along the lane and a small swing of yaw on different slow periods, so the frame never repeats
// quickly and never reads as motion sickness. The loop length is the common multiple of the two periods.
constexpr double TvDriftTruckCm = 140.0;
constexpr double TvDriftTruckPeriod = 80.0;
constexpr double TvDriftYawDegrees = 1.2;
constexpr double TvDriftYawPeriod = 50.0;
constexpr double TvDriftLoopSeconds = 400.0;
} // namespace

namespace PWTitleView
{
FTvPose RestPose()
{
    FTvPose Pose;
    Pose.Location = FVector(TvRestX, TvRestY, PWLane::GroundZ + TvEyeAboveRoad);
    Pose.Rotation = FRotator(TvRestPitch, TvRestYaw, 0.0);
    Pose.FovDegrees = TvFov;
    return Pose;
}

FTvPose DriftPose(const float Seconds)
{
    FTvPose Pose = RestPose();
    const double T = FMath::Fmod(static_cast<double>(FMath::Max(0.0f, Seconds)), TvDriftLoopSeconds);
    Pose.Location.X += TvDriftTruckCm * FMath::Sin(T * 2.0 * UE_DOUBLE_PI / TvDriftTruckPeriod);
    Pose.Rotation.Yaw += TvDriftYawDegrees * FMath::Sin(T * 2.0 * UE_DOUBLE_PI / TvDriftYawPeriod);
    return Pose;
}

double MaxDriftDistance()
{
    return TvDriftTruckCm;
}

float MaxDriftYaw()
{
    return static_cast<float>(TvDriftYawDegrees);
}
} // namespace PWTitleView

APWTitleView::APWTitleView()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetCanBeDamaged(false);
    if (UCameraComponent* Lens = GetCameraComponent())
    {
        // The engine's camera actor letterboxes to 16:9; the title view fills whatever window it is given.
        Lens->SetConstraintAspectRatio(false);
        Lens->SetFieldOfView(PWTitleView::RestPose().FovDegrees);
    }
}

APWTitleView* APWTitleView::Find(const UWorld* World)
{
    if (World == nullptr)
    {
        return nullptr;
    }
    for (TActorIterator<APWTitleView> It(const_cast<UWorld*>(World)); It; ++It)
    {
        if (IsValid(*It))
        {
            return *It;
        }
    }
    return nullptr;
}

APWTitleView* APWTitleView::Present(APlayerController& Controller)
{
    UWorld* World = Controller.GetWorld();
    if (World == nullptr)
    {
        return nullptr;
    }
    APWTitleView* View = Find(World);
    if (View == nullptr)
    {
        const PWTitleView::FTvPose Rest = PWTitleView::RestPose();
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.ObjectFlags |= RF_Transient;
        View = World->SpawnActor<APWTitleView>(Rest.Location, Rest.Rotation, Params);
        if (View == nullptr)
        {
            return nullptr;
        }
        View->ApplyPose(Rest);
    }
    // The pawn is hidden first so that not one frame shows it from the title camera.
    View->HoldPawn(Controller.GetPawn());
    if (!View->IsViewedBy(Controller))
    {
        Controller.SetViewTarget(View);
    }
    View->SetPresenting(true);
    return View;
}

void APWTitleView::Dismiss(APlayerController& Controller)
{
    APWTitleView* View = Find(Controller.GetWorld());
    if (View == nullptr)
    {
        return;
    }
    View->SetPresenting(false);
    View->ReleasePawn();
    if (View->IsViewedBy(Controller))
    {
        // A null pawn sends the view back to the controller itself, which is the engine's own fallback.
        Controller.SetViewTarget(Controller.GetPawn());
    }
}

bool APWTitleView::IsViewedBy(const APlayerController& Controller) const
{
    return Controller.GetViewTarget() == this;
}

void APWTitleView::ApplyPose(const PWTitleView::FTvPose& Pose)
{
    SetActorLocationAndRotation(Pose.Location, Pose.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    if (UCameraComponent* Lens = GetCameraComponent())
    {
        Lens->SetFieldOfView(Pose.FovDegrees);
    }
}

void APWTitleView::ApplyMotionPreference(const bool bReducedMotion)
{
    if (bReducedMotion)
    {
        // Stop at once and show the complete composed frame; nothing keeps moving behind the menu.
        DriftSeconds = 0.0f;
        ApplyPose(PWTitleView::RestPose());
        SetActorTickEnabled(false);
        return;
    }
    SetActorTickEnabled(bPresenting);
}

void APWTitleView::SetPresenting(const bool bValue)
{
    bPresenting = bValue;
    ApplyMotionPreference(UPerfectWorldPresentationSettings::Get().IsReducedMotion());
}

void APWTitleView::HoldPawn(APawn* Pawn)
{
    if (Pawn == nullptr || HeldPawn.Get() == Pawn)
    {
        return;
    }
    ReleasePawn();
    HeldPawn = Pawn;
    bHeldPawnWasHidden = Pawn->IsHidden();
    Pawn->SetActorHiddenInGame(true);
}

void APWTitleView::ReleasePawn()
{
    if (APawn* Pawn = HeldPawn.Get())
    {
        Pawn->SetActorHiddenInGame(bHeldPawnWasHidden);
    }
    HeldPawn.Reset();
    bHeldPawnWasHidden = false;
}

void APWTitleView::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPresenting || UPerfectWorldPresentationSettings::Get().IsReducedMotion())
    {
        return;
    }
    DriftSeconds = static_cast<float>(FMath::Fmod(static_cast<double>(DriftSeconds) + DeltaSeconds, TvDriftLoopSeconds));
    ApplyPose(PWTitleView::DriftPose(DriftSeconds));
}

void APWTitleView::BeginPlay()
{
    Super::BeginPlay();
    MotionHandle = UPerfectWorldPresentationSettings::Get().OnMotionChanged.AddUObject(this, &APWTitleView::HandleMotionChanged);
}

void APWTitleView::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UObjectInitialized() && !IsEngineExitRequested())
    {
        UPerfectWorldPresentationSettings::Get().OnMotionChanged.Remove(MotionHandle);
    }
    MotionHandle.Reset();
    ReleasePawn();
    Super::EndPlay(EndPlayReason);
}

void APWTitleView::HandleMotionChanged()
{
    ApplyMotionPreference(UPerfectWorldPresentationSettings::Get().IsReducedMotion());
}
