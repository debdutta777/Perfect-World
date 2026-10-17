#include "PerfectWorldBlockTestGameMode.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldKolkataLane.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldLaneCareInterior.h"
#include "PerfectWorldLaneInteriors.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldPlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// Early morning: the sun sits low and rakes along the lane (+X) from behind the start camera, a little toward +Y,
// so the right-hand facades catch it warmly while the left-hand row and the ground floors stay in shade.
constexpr float EnMorningSunPitch = -15.0f;
constexpr float EnMorningSunYaw = 30.0f;

// Light haze for depth, not murk. Tune these two first if the distance looks too clear or too milky.
constexpr float EnMorningFogDensity = 0.012f;
constexpr float EnMorningFogHeightFalloff = 0.25f;
} // namespace

APerfectWorldBlockTestGameMode::APerfectWorldBlockTestGameMode()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        BlockMesh = Cube.Object;
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Paint.Succeeded())
    {
        BlockMaterial = Paint.Object;
    }
    // The player walks (first person by default, third person on V); input is read through Enhanced Input.
    DefaultPawnClass = APerfectWorldPlayerCharacter::StaticClass();
    PlayerControllerClass = APerfectWorldPlayerController::StaticClass();
    // The front end (title, creation flow, pause menu) is drawn over this scene by the HUD.
    HUDClass = APerfectWorldFrontEndHUD::StaticClass();
}

APawn* APerfectWorldBlockTestGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
    // The player used to hover 2 m above the road at the world origin. Keep that X, Y and facing, but stand on the road:
    // the capsule's centre sits half its height plus 2 cm above the road top, so it settles onto the surface at once.
    FVector Location = SpawnTransform.GetLocation();
    Location.Z = PWLane::GroundZ + PWPlayer::CapsuleHalfHeight + 2.0;
    const FRotator Facing(0.0, SpawnTransform.Rotator().Yaw, 0.0);
    return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, FTransform(Facing, Location));
}

void APerfectWorldBlockTestGameMode::StartPlay()
{
    BuildSkyAndSun();
    BuildLane();
    Super::StartPlay();
}

bool APerfectWorldBlockTestGameMode::PlacePlayerAtOpeningHome(APlayerController& Player)
{
    UGameInstance* EnInstance = GetGameInstance();
    const UPerfectWorldCharacterCreatorSubsystem* EnCreator = EnInstance != nullptr
        ? EnInstance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>() : nullptr;
    APawn* EnPawn = Player.GetPawn();
    if (EnCreator == nullptr || !EnCreator->HasLifeStarted() || EnPawn == nullptr)
    {
        return false;
    }
    const FPWLaneOriginView EnOrigin = EnCreator->GetLaneOrigin();
    if (!EnOrigin.bResolved)
    {
        return false;
    }

    // LA integration: these are the two built interior frames today. All family origins use the opening shell;
    // replace its frame lookup with the authored building binding when the other family shells are assembled.
    const PWLane::FLaneFrame EnFrame = PWLane::MakeLaneFrame(EnOrigin.bGrewUpInCare ? -1400.0 : -300.0,
        -400.0, FVector::ForwardVector, -FVector::RightVector);
    FVector EnLocation;
    FVector EnLookAt;
    if (EnOrigin.bGrewUpInCare)
    {
        bool EnFoundBed = false;
        for (const PWLane::FHomeSeatSpec& EnSeat : PWLane::CareSeatSpecs())
        {
            if (EnSeat.Id == FName(TEXT("seat.nabapara.care_bed")))
            {
                EnLocation = PWLane::CareWorldPoint(EnFrame, EnSeat.StandU, EnSeat.StandV, PWPlayer::CapsuleHalfHeight);
                // Face into the dormitory aisle towards its doorway, away from the back wall.
                EnLookAt = PWLane::CareWorldPoint(EnFrame, PWLane::CareLayout::DormDoorU, PWLane::CareLayout::AcrossV,
                                                PWPlayer::CapsuleHalfHeight);
                EnFoundBed = true;
                break;
            }
        }
        if (!EnFoundBed) return false;
    }
    else
    {
        // U 735, V 180: 73 cm from the shoe rack, beyond the front door's V 105 swing (+ capsule radius).
        EnLocation = PWLane::HomeWorldPoint(EnFrame, PWLane::HomeFrontDoorU + 35.0, 180.0, PWPlayer::CapsuleHalfHeight);
        EnLookAt = PWLane::HomeWorldPoint(EnFrame, PWLane::HomeLivingPartitionU, PWLane::HomeLivingDoorV,
                                        PWPlayer::CapsuleHalfHeight);
    }
    const FRotator EnFacing(0.0, (EnLookAt - EnLocation).Rotation().Yaw, 0.0);
    if (!EnPawn->SetActorLocationAndRotation(EnLocation, EnFacing, false, nullptr, ETeleportType::TeleportPhysics))
    {
        return false;
    }
    if (UPawnMovementComponent* EnMovement = EnPawn->GetMovementComponent())
    {
        EnMovement->StopMovementImmediately();
    }
    Player.SetControlRotation(EnFacing);
    return true;
}

void APerfectWorldBlockTestGameMode::BuildLane()
{
    UWorld* World = GetWorld();
    if (World == nullptr || BlockMesh == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Block scene: no world or no cube mesh; nothing built."));
        return;
    }
    if (BlockMaterial == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Block scene: BasicShapeMaterial not found; the lane will be unpainted."));
    }
    PWLane::BuildKolkataLane(*World, *BlockMesh, BlockMaterial);
}

void APerfectWorldBlockTestGameMode::BuildSkyAndSun()
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    // The engine Entry map is empty and unlit: add an atmosphere, a sun that
    // drives it, and a sky light that captures it in real time.
    World->SpawnActor<ASkyAtmosphere>();

    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(EnMorningSunPitch, EnMorningSunYaw, 0.0f));
    if (Sun != nullptr)
    {
        if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
        {
            SunLight->SetMobility(EComponentMobility::Movable);
            SunLight->SetAtmosphereSunLight(true);
        }
    }

    ASkyLight* Sky = World->SpawnActor<ASkyLight>();
    if (Sky != nullptr)
    {
        if (USkyLightComponent* SkyLight = Sky->GetLightComponent())
        {
            SkyLight->SetMobility(EComponentMobility::Movable);
            SkyLight->bRealTimeCapture = true;
            SkyLight->MarkRenderStateDirty();
            SkyLight->RecaptureSky();
        }
    }

    // A thin, low-lying haze so the far blocks fall back into the morning air.
    AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector(0.0, 0.0, PWLane::GroundZ), FRotator::ZeroRotator);
    if (Fog != nullptr)
    {
        if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
        {
            FogComponent->SetFogDensity(EnMorningFogDensity);
            FogComponent->SetFogHeightFalloff(EnMorningFogHeightFalloff);
            FogComponent->SetStartDistance(1000.0f);
            FogComponent->SetFogMaxOpacity(0.75f);
        }
    }
}
