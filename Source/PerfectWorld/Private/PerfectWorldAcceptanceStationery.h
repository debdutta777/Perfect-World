#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldLaneStationeryShop.h"

class APWDoorActor;
class UWorld;

namespace PWAcceptance
{
/** AppendAninditaMiraSteps (PerfectWorldAcceptanceAninditaMira.cpp): the scenario "anindita_mira". The integrator's BuildScenario names it. */
void AppendAninditaMiraSteps(TArray<FStepDef>& OutSteps);

/**
 * The steps stationery_shift and anindita_mira share (PerfectWorldAcceptanceStationery.cpp): the shop on a Tuesday afternoon,
 * entered through its real door. Named, not anonymous, so a second scenario file can use them.
 */
namespace Stationery
{
/** The building exactly as PerfectWorldKolkataLane.cpp recovers it from the recorded frontage (side +1, index 2, X -100 to 600). */
PWLane::FLaneBuilding Building();
/** One of the shop's five anchors by public id, in world space. */
bool FindAnchor(const TCHAR* PublicId, PWLane::FStationeryAnchor& Out);
APWDoorActor* FindFrontDoor(UWorld* World);

/** Moves the lane clock to the next Tuesday at Hour:Minute through time.advance and AdoptSavedWorld; fails if the target is not in the future. */
FStepDef JumpClockStep(const FString& StepName, int32 Hour, int32 Minute);
/** The scenario's one teleport: 150 cm in front of the shop threshold on the footpath, facing it. */
FStepDef DeclaredStartStep();
/** Walks the real pawn to a shop anchor (floor-plan distance within ToleranceCm). */
FStepDef WalkToAnchorStep(const FString& StepName, const TCHAR* AnchorId, double ToleranceCm, double TimeoutSeconds);
/** Opens the shop's front door with the interact key's call (passes at once when it is already open). */
FStepDef OpenDoorStep();
FStepDef FaceAnchorStep(const FString& StepName, const TCHAR* AnchorId);
/** Waits for the person to be presented, then passes when they stand within ToleranceCm (XY) of the anchor. */
FStepDef ObserveStep(const FString& StepName, const TCHAR* PersonId, const TCHAR* AnchorId, double ToleranceCm);
} // namespace Stationery
} // namespace PWAcceptance
