#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneNpcBindings.h"

class UWorld;
namespace PWAcceptance { struct FStepDef; void AppendAmbientLaneSteps(TArray<FStepDef>& Out); }
namespace PWLane
{
/** Shared core polylines, road Y80, north footpath Y340, thresholds in the two open side alleys. */
FNpBindingProvider NpAmbientBindings();
/** Idempotent registration of three scheduled people; the vendor owns her single cart. */
bool SpawnAmbientLife(UWorld& World);
}
