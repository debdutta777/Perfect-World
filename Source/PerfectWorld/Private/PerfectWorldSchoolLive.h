#pragma once

// S03: the live bridges of the Phase 2 school over UPWLaneLifeSubsystem's one WorldSim (the pattern of PerfectWorldCommerceLive.h).
// Reads go through the portable views (QuerySchoolPeriod, BuildSchoolDayView, QuerySchoolAcademicRecord, BuildSchoolWeekPlan ...).
// Mutations of lessons, work and the week plan are NOT routed through the reducer in the core yet (no school.lesson.*, school.work.apply or
// school.week.* command, and WorldState has no SchoolSessions/SchoolWork/SchoolWeek member), so every such mutation here is refused with
// PWSchoolLive::UnwiredCode and a plain sentence. See S03_notes.md for the exact reducer wiring that replaces the S03Port functions.

#include "CoreMinimal.h"

#include "Assessment.h"
#include "SchoolLessonContent.h"
#include "SchoolSessions.h"
#include "SchoolWeek.h"
#include "SchoolWork.h"
#include "StructureCoordinate.h"
#include "StructureExplain.h"
#include "StructureInvestigate.h"
#include "StructureModelCatalogue.h"
#include "UI/SPWClassLesson.h"
#include "UI/SPWSchoolDay.h"
#include "UI/SPWSchoolPlanner.h"
#include "UI/SPWStudyDesk.h"

class UWorld;

namespace PWSchoolLive
{
/** The stable refusal code of every school mutation the reducer cannot take yet. */
inline const TCHAR* const UnwiredCode = TEXT("school.s03_unwired");
/** The sentence shown for it. Never claims anything was recorded. */
FText UnwiredText();

/** The school content a stream plays with; built once per stream and kept for the process (the core borrows it). Not movable. */
struct FS03Content
{
    PerfectWorld::ModelCatalogue Model;
    PerfectWorld::ExplainCatalogue Explain;
    PerfectWorld::InvestigateCatalogue Investigate;
    PerfectWorld::CoordinateCatalogue Coordinate;
    PerfectWorld::AssessmentCatalogue Papers;
    PerfectWorld::SchoolLessonContent Lessons;
    PerfectWorld::SchoolSessionCatalogues Cats;
    FString Stream;

    FS03Content() = default;
    FS03Content(const FS03Content&) = delete;
    FS03Content& operator=(const FS03Content&) = delete;
};

/** The stream the player chose in creation ("science" when none). */
FString StreamOf(UWorld& World);
/** Null with a reason when the content database or any catalogue is unavailable. */
const FS03Content* SharedContent(UWorld& World, FString& OutWhy);

/** S03Port: the one place the unrouted state lives. Replace these with the WorldState members once the reducer owns them. */
bool CommandsRouted();
const PerfectWorld::SchoolSessionsState& SessionsOf(const PerfectWorld::WorldState& State);
const PerfectWorld::SchoolWorkState& WorkOf(const PerfectWorld::WorldState& State);
const PerfectWorld::SchoolWeekState& StandingOf(const PerfectWorld::WorldState& State);

/** The binding the classroom seat opens: the period now in progress (or the next one) for today's timetable; null with no timetable. */
const PerfectWorld::SchoolLessonBinding* BindingForNow(UWorld& World);

TSharedRef<IPWSchoolDayWorld> MakeDayWorld(UWorld& World);
TSharedRef<PWClassLesson::IBridge> MakeLessonBridge(UWorld& World, const PerfectWorld::SchoolLessonBinding& Binding);
TSharedRef<PWA5::IStudyWorld> MakeStudyWorld(UWorld& World);
TSharedRef<IPWSchoolPlannerWorld> MakePlannerWorld(UWorld& World);
} // namespace PWSchoolLive
