#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/**
 * S03: school_lesson_round9. A new life starts at home; the real pawn walks to Nabapara Vidyalaya, the clock moves to the on-time window, the
 * pawn signs in (the day is planned and the arrival recorded by the portable core), walks to the classroom seat, the clock moves to a bell,
 * "Take your seat" opens the school-day panel (world input blocked, clock held), a lesson screen is opened from it, one lesson command is made,
 * the screen is closed (focus restored) and the core's state is checked.
 *
 * Until the reducer routes school.lesson.* (S03_notes.md), PWSchoolLive::CommandsRouted() is false: the open and the choice are then expected to
 * be REFUSED with PWSchoolLive::UnwiredCode and to change nothing, and the steps say so ("UNWIRED") in their result text. When it is true the
 * same steps open a foreground lesson, make a real choice, record the period and require the core's SchoolSessions to hold the lesson.
 *
 *     {TEXT("school_lesson_round9"), AppendSchoolLessonRound9Steps}
 */
void AppendSchoolLessonRound9Steps(TArray<FStepDef>& Out);
} // namespace PWAcceptance
