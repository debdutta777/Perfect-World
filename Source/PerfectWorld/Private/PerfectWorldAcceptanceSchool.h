#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/**
 * S03/S05: school_lesson_round9. A new life starts at home; the real pawn walks to Nabapara Vidyalaya, the clock moves to the on-time window,
 * the pawn signs in (the day is planned and the arrival recorded by the portable core), walks to the classroom seat, the clock moves to a bell,
 * "Take your seat" opens the school-day panel (world input blocked, clock held), a foreground lesson is opened from it (school.lesson.open),
 * real choices are made (school.lesson.choose), the period is recorded or left early (school.lesson.close, which charges the time), the panel is
 * closed (input and clock restored), and the core's SchoolSessions must hold the period record. There are no unrouted paths: a refusal fails the step.
 *
 *     {TEXT("school_lesson_round9"), AppendSchoolLessonRound9Steps}
 */
void AppendSchoolLessonRound9Steps(TArray<FStepDef>& Out);
} // namespace PWAcceptance
