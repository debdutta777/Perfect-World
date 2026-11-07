#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/**
 * R9-06: awakening_floor. At the forecourt terminal of Nabapara Vidyalaya (the scenario's one declared teleport) the real pawn
 * enters the supervised virtual floor through the real interaction prompt and, in the FIRST-person camera, completes all three
 * situations by helping (help your classmate, protect with the barrier, coordinate the carriers), checks the bridge saw each choice and
 * each consequence is on the blocks, walks to the return pad and steps back onto the campus. Then, in the THIRD-person camera, a fresh
 * session: withdraw to the safe area at the crossing, withdraw again at the corridor, and decline the confrontation at the delivery,
 * then return. Screenshots at the terminal, the arrival, after every station, in the safe area and back on the campus.
 *
 * The bridge: when the integrator has installed none (R9-02 is not in the base) the scenario installs PWAwakening::FAwScriptedBridge
 * and says so in its first step's detail; with a real bridge the second pass is skipped with a note (a completed session cannot
 * replay). PerfectWorldAcceptanceLaneScenario.cpp's BuildScenario names it:
 *
 *     if (Name == TEXT("awakening_floor")) { AppendAwakeningFloorSteps(OutSteps); return true; }
 *
 * and includes this header. Run (Development package, off-screen):
 *     PerfectWorld.exe -RenderOffScreen -PWAcceptance=awakening_floor -PWAcceptanceOut=D:/PWAcc/aw -PWAcceptanceStart=new
 */
void AppendAwakeningFloorSteps(TArray<FStepDef>& Out);
} // namespace PWAcceptance
