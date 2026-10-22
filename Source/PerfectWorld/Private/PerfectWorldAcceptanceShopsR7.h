#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/**
 * Scenario "shops_round7" (R7-04): the staffed neighbourhood counters, walked with the real pawn in both cameras. Run it twice with
 * --start=new then --start=continue in a fresh process (the sweet-shop agreement must survive the restart). Integration: in
 * PerfectWorldAcceptanceLaneScenario.cpp, beside "tailor_workday", add
 *     if (Name == TEXT("shops_round7")) { AppendShopsRound7Steps(OutSteps); return true; }
 * and include this header.
 */
void AppendShopsRound7Steps(TArray<FStepDef>& OutSteps);
}
