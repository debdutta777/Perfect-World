#pragma once

#include "CoreMinimal.h"
#include "NpcBehaviour.h"

struct FNpPresenceRecord;

/** R5-05 presentation-only bridge. No reducer, save, hash or animation completion state. */
bool R55IsWorkPilot(FName PersonId);
FText R55NpcReasonText(FName Key);
bool R55ResolvePilot(FNpPresenceRecord& Record, PerfectWorld::GameInstant Now);
