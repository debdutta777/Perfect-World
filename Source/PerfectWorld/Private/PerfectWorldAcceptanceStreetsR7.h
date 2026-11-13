#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/**
 * R7-02: streets_round7. Starting at the player's home front door (the one declared teleport), the real pawn walks the plan's own routes
 * (NeighbourhoodPlan.h) along the lane, up the cross street, across the para road to the school gate (first person), on along the para road
 * and across the main road to the bus stop, then back from the bus stop to the school gate and home (third person), with a screenshot at
 * each milestone and an arrival assertion at each destination. A first step proves the streets exist (surface traces at plan coordinates)
 * and that the north alley's stopping barrier is gone (a Pawn-channel trace along the alley is unblocked).
 *
 * Hook (PerfectWorldAcceptanceLaneScenario.cpp, integrator-owned), in BuildScenario with the other scenarios:
 *     #include "PerfectWorldAcceptanceStreetsR7.h"
 *     if (Name == TEXT("streets_round7")) { AppendStreetsRound7Steps(OutSteps); return true; }
 */
void AppendStreetsRound7Steps(TArray<FStepDef>& Out);
} // namespace PWAcceptance
