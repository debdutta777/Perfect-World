#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/**
 * scale_round4 (R4-06, Docs/ROUND4_PLAN_2026-10-19.md): measures the round-4 cast, the door clearances, the seats, the work surfaces
 * and the tall flats' upstairs headroom, in both cameras and at the supported player-height extremes (145 and 195 cm).
 *
 * Integration (the integrator's, not done here):
 *   1. PerfectWorldAcceptanceLaneScenario.cpp: #include "PerfectWorldAcceptanceScaleR4.h", and in BuildScenario, before the final
 *          if (Name == TEXT("scale_round4")) { AppendScaleRound4Steps(OutSteps); return true; }
 *   2. PerfectWorldAcceptanceTests.cpp: add "scale_round4" to the scenario-name list if it keeps one.
 * The scenario needs a normal production start (-PWAcceptanceStart=new or continue); it changes no rule.
 */
void AppendScaleRound4Steps(TArray<FStepDef>& OutSteps);
} // namespace PWAcceptance
