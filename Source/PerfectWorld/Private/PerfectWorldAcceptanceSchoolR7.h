#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/**
 * R7-03: school_round7. From the family home's front door (opened by the real door actor) the real pawn walks the neighbourhood plan's
 * route to Nabapara Vidyalaya's south gate, reads the timetable board, finds the gate closed before 09:30 (the core's own refusal),
 * has the clock moved to the on-time window with the core's commands, signs in at the register (on time, recorded by the portable core),
 * then walks into the office and into the classroom in BOTH cameras (first person, then third person) with screenshots, and crosses to
 * the playground. PerfectWorldAcceptanceLaneScenario.cpp's BuildScenario names it:
 *
 *     if (Name == TEXT("school_round7")) { AppendSchoolRound7Steps(OutSteps); return true; }
 */
void AppendSchoolRound7Steps(TArray<FStepDef>& Out);

/**
 * R7-03: school_round7_late, the second run. The same walk to the gate, then the clock is moved to 10:01 and the sign-in is recorded as
 * LATE by the core; the notice and a screenshot show the late result with its reason text.
 *
 *     if (Name == TEXT("school_round7_late")) { AppendSchoolRound7LateSteps(OutSteps); return true; }
 */
void AppendSchoolRound7LateSteps(TArray<FStepDef>& Out);
} // namespace PWAcceptance
