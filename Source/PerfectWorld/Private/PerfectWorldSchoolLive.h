#pragma once

// S03/S05: the live bridges of the Phase 2 school over UPWLaneLifeSubsystem's one WorldSim (the pattern of PerfectWorldCommerceLive.h).
// Reads go through the portable views (QuerySchoolPeriod, BuildSchoolDayView, QuerySchoolAcademicRecord, BuildSchoolWeekPlan ...) over
// WorldState::SchoolSessions / SchoolWork / SchoolWeek. Every mutation is a real reducer command submitted through
// UPWLaneLifeSubsystem::SubmitSchoolCommand (school.lesson.open/choose/close, school.work.apply, school.week.order/resolve); the result shown is
// the sim's own LastSchool*Result(), and a refusal's code goes back unchanged. No rule math lives here.

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

/** The school state of a world (the WorldState members) and whether the reducer routes the school commands. */
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
