#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldAwakeningActors.h"
#include "PerfectWorldAwakeningBlocks.h"
#include "PerfectWorldAwakeningBridge.h"
#include "PerfectWorldAwakeningCatalogue.h"
#include "PerfectWorldAwakeningLayout.h"

class AActor;
class UWorld;

/**
 * R9-06: the supervised tutorial floor of Nabapara Vidyalaya (prefix Aw / PWAwakening). See PerfectWorldAwakeningLayout.h for the
 * geometry, PerfectWorldAwakeningBridge.h for the seam to the world and the ids expected from R9-02/03.
 *
 * HOOKS FOR THE INTEGRATOR (none applied here):
 *   1. Kolkata lane, straight after the campus (PerfectWorldKolkataLane.cpp, BuildKolkataLane):
 *          PWSchool::SpawnSchoolCampus(World, Scene);
 *          PWAwakening::SpawnAwakeningFloor(World);                 // R9-06: the virtual floor and its forecourt terminal
 *      The terminal ("Enter the virtual floor") stands on the forecourt at (Teaching.X1 - 700, Forecourt.Y1 - 90); the floor itself is
 *      a sealed room 40 m under the playground, so nothing of it shows from the campus.
 *   2. The bridge: install the real one once per world, after the lane-life world exists (R9-02/R9-08):
 *          PWAwakening::AwBridge::Install(&World, MakeShared<FRealAwakeningBridge>(...));
 *      Without one, the terminal answers "The supervised floor is not open right now." and nothing moves.
 *   3. Acceptance: PerfectWorldAcceptanceLaneScenario.cpp BuildScenario names the scenario (see PerfectWorldAcceptanceAwakeningFloor.h).
 *   4. Text scale / reduced motion: FAwFloorConfig::TextScale and ReducedMotion; reduced motion already follows
 *      UPerfectWorldPresentationSettings. When a text-scale setting exists, call APWAwakeningFloorActor::SetTextScale from it.
 *   5. Safe-area withdrawal and the entry are presentation; the real bridge must answer SubmitChoice for awakening.choice.withdraw_to_safety
 *      in every phase.
 */
namespace PWAwakening
{
/** What SpawnAwakeningFloor made. */
struct FAwSpawnResult
{
    bool bBuilt = false;
    int32 Instances = 0;
    int32 Components = 0;
    AActor* BlocksActor = nullptr;
    APWAwakeningFloorActor* Floor = nullptr;
    int32 Pads = 0;
    int32 Movers = 0;
    int32 TextSigns = 0;
    int32 Lights = 0;
    int32 Figures = 0;
};

/**
 * Builds the floor and the terminal from the campus plan and spawns every actor: the static blocks, the director, the pads, the moving
 * parts, the labels, the status boards, the lights and (when asked) the people. Call once, after SpawnSchoolCampus. Safe to call in a
 * throw-away test world; the cube mesh and BasicShapeMaterial come from the engine content.
 */
FAwSpawnResult SpawnAwakeningFloor(UWorld& World, const FAwFloorConfig& Config = FAwFloorConfig());
} // namespace PWAwakening

using PWAwakening::SpawnAwakeningFloor;
