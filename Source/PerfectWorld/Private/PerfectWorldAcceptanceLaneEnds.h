#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/**
 * R6-05: lane_ends. The community hall (west end) and the market hall (east end) of the lane, entered through their doorless archways.
 * After the one declared start on the lane axis near the west end, the real pawn walks: up to the community hall archway, through it,
 * along its centre aisle to the stage-front stand and its aisle to the notice board (first person), out again, and the same visit in
 * third person; then 8800 cm along the lane to the market hall, through its archway, down the cross aisle and along the back aisle to a
 * wall counter, and out (both cameras). Each visit asserts the pawn is inside the hall's footprint, takes a screenshot at the stand and
 * checks the posted hours board. A first step checks the boards exist with the portable text.
 *
 * Hook (PerfectWorldAcceptanceLaneScenario.cpp, integrator-owned), in BuildScenario with the other scenarios:
 *     #include "PerfectWorldAcceptanceLaneEnds.h"
 *     if (Name == TEXT("lane_ends")) { AppendLaneEndsSteps(OutSteps); return true; }
 */
void AppendLaneEndsSteps(TArray<FStepDef>& Out);
} // namespace PWAcceptance
