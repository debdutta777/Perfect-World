#pragma once

#include "PerfectWorldLaneKitFrontages.h"
#include "LaneTallFlatsLayout.h"

namespace PWLane
{
/** Exact catalogue match; the integrator calls this before the generic kit frontage branch. */
bool IsTallFlatsUpstairs(const FLaneBuilding& Building);
/** Replaces BuildKitFrontage: ground stays furnished, ceiling is the holed slab, one upper home is furnished.
 * Higher storeys and the unused west half of the first floor remain sealed. No upper door is emitted. */
void BuildTallFlatsUpstairs(FLaneScene& Scene, const FLaneBuilding& Building);
}

namespace PWAcceptance
{
struct FStepDef;
/** BuildScenario("tall_flats_upstairs") dispatches here; integration owns the registration. */
void AppendTallFlatsUpstairsSteps(TArray<FStepDef>& OutSteps);
}
