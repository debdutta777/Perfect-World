#include "UI/SPWSchoolPlanner.h"

#include <algorithm>

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "OpportunityReceipts.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UI/PWQuestText.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// The planner's data side (words, the plan model, the recap and its session store) and its widget. The projection terminal
// (SPWSchoolProjection.*) shares the words through PWSchoolPlanner::Text. Every key mirrors Data/Text/en/phase2_a6.json; a test compares them.

namespace
{
struct FA6TextEntry
{
    const TCHAR* Key;
    const TCHAR* Value;
};

const FA6TextEntry GA6Text[] = {
    {TEXT("phase2.a6.planner.title"), TEXT("Week planner")},
    {TEXT("phase2.a6.planner.free_note"), TEXT("Planning costs no time. Nothing changes in your day until you press Resolve this week.")},
    {TEXT("phase2.a6.planner.unavailable"), TEXT("The week cannot be read right now. Go back, resume and try again.")},
    {TEXT("phase2.a6.planner.hint"), TEXT("Tab or the d-pad moves between controls. Page Up and Page Down scroll. Esc or B goes back.")},
    {TEXT("phase2.a6.common.back"), TEXT("Back")},
    {TEXT("phase2.a6.date_fmt"), TEXT("{0} {1} {2}")},
    {TEXT("phase2.a6.clock_fmt"), TEXT("{0} · {1}")},
    {TEXT("phase2.a6.dayname.0"), TEXT("Sun")}, {TEXT("phase2.a6.dayname.1"), TEXT("Mon")}, {TEXT("phase2.a6.dayname.2"), TEXT("Tue")},
    {TEXT("phase2.a6.dayname.3"), TEXT("Wed")}, {TEXT("phase2.a6.dayname.4"), TEXT("Thu")}, {TEXT("phase2.a6.dayname.5"), TEXT("Fri")},
    {TEXT("phase2.a6.dayname.6"), TEXT("Sat")},
    {TEXT("phase2.a6.month.1"), TEXT("Jan")}, {TEXT("phase2.a6.month.2"), TEXT("Feb")}, {TEXT("phase2.a6.month.3"), TEXT("Mar")},
    {TEXT("phase2.a6.month.4"), TEXT("Apr")}, {TEXT("phase2.a6.month.5"), TEXT("May")}, {TEXT("phase2.a6.month.6"), TEXT("Jun")},
    {TEXT("phase2.a6.month.7"), TEXT("Jul")}, {TEXT("phase2.a6.month.8"), TEXT("Aug")}, {TEXT("phase2.a6.month.9"), TEXT("Sep")},
    {TEXT("phase2.a6.month.10"), TEXT("Oct")}, {TEXT("phase2.a6.month.11"), TEXT("Nov")}, {TEXT("phase2.a6.month.12"), TEXT("Dec")},
    {TEXT("phase2.a6.weekdays.mon_fri"), TEXT("Mon to Fri")},
    {TEXT("phase2.a6.orders.heading"), TEXT("Standing orders")},
    {TEXT("phase2.a6.orders.empty"), TEXT("No standing orders. Every lesson stays yours to play.")},
    {TEXT("phase2.a6.orders.edit"), TEXT("Edit")},
    {TEXT("phase2.a6.orders.remove"), TEXT("Remove")},
    {TEXT("phase2.a6.order.attend_fmt"), TEXT("{0}: go to school; lessons {1}")},
    {TEXT("phase2.a6.order.skip_fmt"), TEXT("{0}: stay out of school")},
    {TEXT("phase2.a6.mode.attend"), TEXT("attend only")},
    {TEXT("phase2.a6.mode.routine"), TEXT("routine where you have mastered it")},
    {TEXT("phase2.a6.mode.foreground"), TEXT("played by you")},
    {TEXT("phase2.a6.mode.chip.attend"), TEXT("Attend")},
    {TEXT("phase2.a6.mode.chip.routine"), TEXT("Routine")},
    {TEXT("phase2.a6.mode.chip.foreground"), TEXT("Play it myself")},
    {TEXT("phase2.a6.editor.heading"), TEXT("Add or change an order")},
    {TEXT("phase2.a6.editor.days"), TEXT("Days")},
    {TEXT("phase2.a6.editor.attend"), TEXT("Go to school on these days")},
    {TEXT("phase2.a6.editor.mode"), TEXT("Lessons")},
    {TEXT("phase2.a6.editor.save"), TEXT("Save order")},
    {TEXT("phase2.a6.editor.hint"), TEXT("Saving changes the preview below, not your day. You can remove an order at any time.")},
    {TEXT("phase2.a6.editor.saved"), TEXT("Order saved. No time was spent.")},
    {TEXT("phase2.a6.editor.removed"), TEXT("Order removed. No time was spent.")},
    {TEXT("phase2.a6.editor.no_days"), TEXT("Choose at least one day first.")},
    {TEXT("phase2.a6.scene.heading"), TEXT("Scene days")},
    {TEXT("phase2.a6.scene.empty"), TEXT("No scene days are nominated.")},
    {TEXT("phase2.a6.scene.note"), TEXT("You play a scene day yourself: compression stops at its first entry.")},
    {TEXT("phase2.a6.week.heading"), TEXT("The week, as compression would run it")},
    {TEXT("phase2.a6.week.empty"), TEXT("Nothing is planned for the coming days.")},
    {TEXT("phase2.a6.week.problem_fmt"), TEXT("This week cannot be planned yet. {0}")},
    {TEXT("phase2.a6.week.stop_none"), TEXT("Nothing in this plan needs you: it would resolve in full.")},
    {TEXT("phase2.a6.week.stop_fmt"), TEXT("Compression would stop before {0}, {1}. You play it; then resolve again.")},
    {TEXT("phase2.a6.week.scene_day"), TEXT("Scene day")},
    {TEXT("phase2.a6.entry.line_fmt"), TEXT("{0}  {1}")},
    {TEXT("phase2.a6.entry.time_range_fmt"), TEXT("{0} to {1}")},
    {TEXT("phase2.a6.entry.school"), TEXT("Arrive at school")},
    {TEXT("phase2.a6.entry.lesson_fmt"), TEXT("Lesson: {0}")},
    {TEXT("phase2.a6.entry.lesson_unknown"), TEXT("a lesson")},
    {TEXT("phase2.a6.entry.sleep"), TEXT("Sleep")},
    {TEXT("phase2.a6.entry.spend"), TEXT("Time to spend")},
    {TEXT("phase2.a6.entry.activity"), TEXT("An activity you chose")},
    {TEXT("phase2.a6.activity.attend_class"), TEXT("Attend class")},
    {TEXT("phase2.a6.activity.eat_meal"), TEXT("Eat a meal")},
    {TEXT("phase2.a6.activity.exercise_run"), TEXT("Go for a run")},
    {TEXT("phase2.a6.activity.household_chores"), TEXT("Household chores")},
    {TEXT("phase2.a6.activity.rest"), TEXT("Rest")},
    {TEXT("phase2.a6.activity.sleep_night"), TEXT("Sleep")},
    {TEXT("phase2.a6.activity.study_at_home"), TEXT("Study at home")},
    {TEXT("phase2.a6.alt.unknown"), TEXT("something you knew about")},
    {TEXT("phase2.a6.mark.order"), TEXT("Done for you by your standing order.")},
    {TEXT("phase2.a6.mark.you_play"), TEXT("You play this one.")},
    {TEXT("phase2.a6.mark.first"), TEXT("Your first time at this lesson.")},
    {TEXT("phase2.a6.mark.foreground"), TEXT("Your order says you play each lesson yourself.")},
    {TEXT("phase2.a6.mark.routine_not_ready"), TEXT("Routine is not ready for this lesson yet.")},
    {TEXT("phase2.a6.mark.scene_day"), TEXT("A scene day.")},
    {TEXT("phase2.a6.mark.choice"), TEXT("A choice only you can make.")},
    {TEXT("phase2.a6.mark.choice_spent"), TEXT("A choice only you can make. This week's one interruption is already used.")},
    {TEXT("phase2.a6.mark.stops"), TEXT("Compression stops here.")},
    {TEXT("phase2.a6.mark.displaced_fmt"), TEXT("Passes over {0}.")},
    {TEXT("phase2.a6.mark.displaced_item_fmt"), TEXT("{0} ({1} min)")},
    {TEXT("phase2.a6.resolve.button"), TEXT("Resolve this week")},
    {TEXT("phase2.a6.resolve.nothing"), TEXT("Nothing to resolve yet.")},
    {TEXT("phase2.a6.resolve.note"), TEXT("Resolving spends the game time the plan shows and stops before anything that needs you.")},
    {TEXT("phase2.a6.recap.heading"), TEXT("What actually happened")},
    {TEXT("phase2.a6.recap.none"), TEXT("No recap yet. A recap shows only what was really resolved in this session; it is never rebuilt afterwards.")},
    {TEXT("phase2.a6.recap.nothing_ran"), TEXT("Nothing was resolved and no game time passed.")},
    {TEXT("phase2.a6.recap.done_fmt"), TEXT("Commitments resolved: {0}.")},
    {TEXT("phase2.a6.recap.stop_none"), TEXT("The whole plan was resolved.")},
    {TEXT("phase2.a6.recap.stop_first_fmt"), TEXT("Stopped before your first time at {0}. Play it, then resolve again.")},
    {TEXT("phase2.a6.recap.stop_choice_fmt"), TEXT("Stopped before a choice only you can make: {0}.")},
    {TEXT("phase2.a6.recap.stop_spent_fmt"), TEXT("Stopped before {0}. This week's one interruption was already used, so it is a scene day.")},
    {TEXT("phase2.a6.recap.stop_unknown"), TEXT("Stopped before the next commitment.")},
    {TEXT("phase2.a6.recap.passed_fmt"), TEXT("Time passed between commitments: {0} min.")},
    {TEXT("phase2.a6.recap.interrupts_fmt"), TEXT("Interruptions used this week: {0} of 1.")},
    {TEXT("phase2.a6.recap.arrived"), TEXT("Arrived at school")},
    {TEXT("phase2.a6.recap.on_time"), TEXT("You arrived on time.")},
    {TEXT("phase2.a6.recap.late"), TEXT("You arrived late.")},
    {TEXT("phase2.a6.recap.very_late"), TEXT("You arrived very late.")},
    {TEXT("phase2.a6.recap.absent"), TEXT("You were marked absent.")},
    {TEXT("phase2.a6.recap.learning"), TEXT("Learning recorded.")},
    {TEXT("phase2.a6.recap.reserved_fmt"), TEXT("Time reserved: {0} min.")},
    {TEXT("phase2.a6.recap.displaced_fmt"), TEXT("Passed over, as you knew at the time: {0}.")},
    {TEXT("phase2.a6.recap.more_fmt"), TEXT("{0} and {1} more")},
    {TEXT("phase2.a6.recap.summarised"), TEXT("Recorded in the day's totals.")},
    {TEXT("phase2.a6.refusal.unknown"), TEXT("That could not be done. Nothing changed.")},
    {TEXT("phase2.a6.refusal.school.week.bad_order"), TEXT("That order is not valid. Choose days from Monday to Saturday.")},
    {TEXT("phase2.a6.refusal.school.week.unknown_order"), TEXT("That order no longer exists.")},
    {TEXT("phase2.a6.refusal.school.week.too_many_orders"), TEXT("There are already as many orders as there are school days.")},
    {TEXT("phase2.a6.refusal.school.week.overlapping_orders"), TEXT("Two orders cannot cover the same day. Edit or remove the other one first.")},
    {TEXT("phase2.a6.refusal.school.week.missing_catalogue"), TEXT("The school timetable is not available.")},
    {TEXT("phase2.a6.refusal.school.week.bad_plan"), TEXT("The week's plan is not valid.")},
    {TEXT("phase2.a6.refusal.school.week.missing_binding"), TEXT("A lesson in the timetable has no subject set up yet.")},
    {TEXT("phase2.a6.refusal.school.week.duplicate_binding"), TEXT("A lesson in the timetable is set up twice.")},
    {TEXT("phase2.a6.refusal.school.week.busy"), TEXT("A lesson is open. Finish or leave it first.")},
    {TEXT("phase2.a6.refusal.school.week.bad_lesson"), TEXT("A lesson in the plan does not match the timetable.")},
    {TEXT("phase2.a6.refusal.school.week.duplicate_lesson"), TEXT("A lesson appears twice in the plan.")},
    {TEXT("phase2.a6.refusal.school.week.clock_refused"), TEXT("The clock could not move. Nothing changed.")},
    {TEXT("phase2.a6.refusal.week.overlap"), TEXT("School time collides with something already planned. Nothing changed.")},
    {TEXT("phase2.a6.refusal.week.world_busy"), TEXT("You are asleep, in the middle of an activity or committed to something. Finish that first.")},
    {TEXT("phase2.a6.refusal.week.no_school"), TEXT("There is no school to go to.")},
    {TEXT("phase2.a6.refusal.week.beyond_calendar"), TEXT("That runs past the end of the school calendar.")},
    {TEXT("phase2.a6.refusal.week.not_routine"), TEXT("This plan includes something that cannot be compressed.")},
    {TEXT("phase2.a6.refusal.week.invalid_world"), TEXT("The day is out of step with the clock.")},
    {TEXT("phase2.a6.refusal.week.unknown_activity"), TEXT("The plan names an activity that does not exist.")},
    {TEXT("phase2.a6.refusal.week.wrong_place"), TEXT("An activity in the plan does not suit where it happens.")},
    {TEXT("phase2.a6.refusal.week.day_command_refused"), TEXT("The day would not accept a step of the plan. Nothing changed.")},
    {TEXT("phase2.a6.refusal.week.clock_refused"), TEXT("The clock could not move. Nothing changed.")},
    {TEXT("phase2.a6.refusal.week.missing_catalogue"), TEXT("The school timetable is not available.")},
    {TEXT("phase2.a6.projection.title"), TEXT("What the school terminal has seen")},
    {TEXT("phase2.a6.projection.note"), TEXT("Unknown means not yet seen. It never means unsuitable, and nothing here limits what you may try.")},
    {TEXT("phase2.a6.projection.unavailable"), TEXT("The terminal cannot be read right now. Go back, resume and try again.")},
    {TEXT("phase2.a6.projection.nothing_seen"), TEXT("Nothing has been observed yet, so every path is unknown.")},
    {TEXT("phase2.a6.projection.paths"), TEXT("Paths")},
    {TEXT("phase2.a6.projection.facets"), TEXT("What has been seen")},
    {TEXT("phase2.a6.projection.unknown_heading"), TEXT("Not yet seen (unknown, not weak)")},
    {TEXT("phase2.a6.projection.unknown_none"), TEXT("Every part of this path has been seen at least once.")},
    {TEXT("phase2.a6.projection.unknown_invite"), TEXT("You can find out by trying it.")},
    {TEXT("phase2.a6.projection.evidence_fmt"), TEXT("Recorded observations that bear on this path: {0}.")},
    {TEXT("phase2.a6.projection.hint"), TEXT("Up and Down choose a path. Esc or B goes back.")},
    {TEXT("phase2.a6.projection.row_fmt"), TEXT("{0}: {1}")},
    {TEXT("phase2.a6.band.insufficient"), TEXT("Unknown so far")},
    {TEXT("phase2.a6.band.developing"), TEXT("Early evidence")},
    {TEXT("phase2.a6.band.plausible"), TEXT("Plausible")},
    {TEXT("phase2.a6.band.strong"), TEXT("Strong evidence")},
    {TEXT("phase2.a6.coverage.unknown"), TEXT("Nothing seen yet")},
    {TEXT("phase2.a6.coverage.sparse"), TEXT("Seen a little")},
    {TEXT("phase2.a6.coverage.partial"), TEXT("Seen in part")},
    {TEXT("phase2.a6.coverage.broad"), TEXT("Seen widely")},
    {TEXT("phase2.a6.facet_state.unobserved"), TEXT("Unknown")},
    {TEXT("phase2.a6.facet_state.observed"), TEXT("Seen, not yet shown")},
    {TEXT("phase2.a6.facet_state.shown"), TEXT("Shown")},
    {TEXT("phase2.a6.facet_state.established"), TEXT("Established")},
    {TEXT("phase2.a6.facet.analysis"), TEXT("Analysis")},
    {TEXT("phase2.a6.facet.interpretation"), TEXT("Interpretation")},
    {TEXT("phase2.a6.facet.fault_finding"), TEXT("Fault finding")},
    {TEXT("phase2.a6.facet.teamwork"), TEXT("Teamwork")},
    {TEXT("phase2.a6.facet.hands_on"), TEXT("Hands-on work")},
    {TEXT("phase2.a6.facet.physical"), TEXT("Physical performance")},
    {TEXT("phase2.a6.facet.accounts"), TEXT("Accounts")},
    {TEXT("phase2.a6.facet.scheduling"), TEXT("Scheduling")},
    {TEXT("phase2.a6.facet.creative"), TEXT("Creative work")},
    {TEXT("phase2.a6.facet.steadiness"), TEXT("Steadiness")},
    {TEXT("phase2.a6.path.hunter"), TEXT("Hunting")},
    {TEXT("phase2.a6.path.scholar"), TEXT("Research")},
    {TEXT("phase2.a6.path.engineer"), TEXT("Engineering")},
    {TEXT("phase2.a6.path.enterprise"), TEXT("Enterprise")},
    {TEXT("phase2.a6.path.athlete"), TEXT("Athletics")},
    {TEXT("phase2.a6.path.clinician"), TEXT("Clinical work")},
    {TEXT("phase2.a6.path.public_leader"), TEXT("Public leadership")},
    {TEXT("phase2.a6.path.logistician"), TEXT("Logistics")},
    {TEXT("phase2.a6.path.artist"), TEXT("The arts")},
    {TEXT("phase2.a6.path.ecologist"), TEXT("Ecology")},
};

TFunction<bool(const FString&, FText&)>& A6Provider()
{
    static TFunction<bool(const FString&, FText&)> Provider;
    return Provider;
}

TFunction<void()>& A6OpenHandler()
{
    static TFunction<void()> Handler;
    return Handler;
}

FPWRecapModel& A6RecapStore()
{
    static FPWRecapModel Store;
    return Store;
}

uint32& A6RecapRevision()
{
    static uint32 Revision = 0;
    return Revision;
}

FText A6Fmt(const TCHAR* Key, const FText& A)
{
    return FText::Format(PWSchoolPlanner::Text(Key), A);
}

FText A6Fmt(const TCHAR* Key, const FText& A, const FText& B)
{
    return FText::Format(PWSchoolPlanner::Text(Key), A, B);
}

int64 A6CampaignDayOf(const PerfectWorld::GameInstant Instant)
{
    return PerfectWorld::GameClock::DayNumber(Instant) - PerfectWorld::GameClock::DayNumber(0);
}

int32 A6MinuteOf(const PerfectWorld::GameInstant Instant)
{
    const PerfectWorld::CalendarTime Calendar = PerfectWorld::GameClock::ToCalendar(Instant);
    return Calendar.Hour * 60 + Calendar.Minute;
}

FText A6ActivityName(const std::string& ActivityId)
{
    const FText Found = PWSchoolPlanner::FindText(FString(TEXT("phase2.a6.")) + PWLaneLife::ToFString(ActivityId));
    return Found.IsEmpty() ? PWSchoolPlanner::Text(TEXT("phase2.a6.entry.activity")) : Found;
}

FText A6LessonName(const PerfectWorld::SchoolLessonBinding& Binding)
{
    FString Subject = PWLaneLife::ToFString(Binding.Subject).Replace(TEXT("_"), TEXT(" "));
    if (!Subject.IsEmpty())
    {
        Subject[0] = FChar::ToUpper(Subject[0]);
    }
    const FText Neutral = Subject.IsEmpty() ? PWSchoolPlanner::Text(TEXT("phase2.a6.entry.lesson_unknown")) : FText::FromString(Subject);
    return PWSchoolPlanner::TextOr(Binding.TitleTextKey, Neutral);
}

const PerfectWorld::SchoolWeekLesson* A6FindLesson(const PerfectWorld::SchoolWeekPlan& Plan, const int32 Day, const int32 Entry)
{
    for (const PerfectWorld::SchoolWeekLesson& Lesson : Plan.Lessons)
    {
        if (Lesson.Day == Day && Lesson.Entry == Entry) return &Lesson;
    }
    return nullptr;
}

/** What an entry is called, in the plan's own terms. */
FText A6EntryWhat(const PerfectWorld::SchoolWeekPlan& Plan, const int32 Day, const int32 Entry)
{
    if (Day < 0 || Day >= static_cast<int32>(Plan.Week.Days.size())) return FText::GetEmpty();
    const auto& Entries = Plan.Week.Days[Day].Entries;
    if (Entry < 0 || Entry >= static_cast<int32>(Entries.size())) return FText::GetEmpty();
    const PerfectWorld::WeekEntry& Item = Entries[Entry];
    switch (Item.Kind)
    {
    case PerfectWorld::WeekEntryKind::School: return PWSchoolPlanner::Text(TEXT("phase2.a6.entry.school"));
    case PerfectWorld::WeekEntryKind::Sleep: return PWSchoolPlanner::Text(TEXT("phase2.a6.entry.sleep"));
    case PerfectWorld::WeekEntryKind::Spend: return PWSchoolPlanner::Text(TEXT("phase2.a6.entry.spend"));
    case PerfectWorld::WeekEntryKind::Activity: break;
    }
    if (const PerfectWorld::SchoolWeekLesson* Lesson = A6FindLesson(Plan, Day, Entry))
    {
        return A6Fmt(TEXT("phase2.a6.entry.lesson_fmt"), A6LessonName(Lesson->Binding));
    }
    return A6ActivityName(Item.ActivityId);
}

FText A6AlternativeName(const std::string& Id)
{
    const FText Found = PWSchoolPlanner::FindText(FString(TEXT("phase2.a6.")) + PWLaneLife::ToFString(Id));
    return Found.IsEmpty() ? PWSchoolPlanner::Text(TEXT("phase2.a6.alt.unknown")) : Found;
}

FText A6JoinNames(const TArray<FText>& Names, const int32 Omitted)
{
    FText Joined = FText::Join(FText::FromString(TEXT(", ")), Names);
    return Omitted > 0 ? A6Fmt(TEXT("phase2.a6.recap.more_fmt"), Joined, FText::AsNumber(Omitted)) : Joined;
}

int32 A6EntryMinutes(const PerfectWorld::WeekEntry& Item, const PerfectWorld::ActivityCatalogue& Activities)
{
    if (Item.Kind == PerfectWorld::WeekEntryKind::Activity)
    {
        const PerfectWorld::ActivityDef* Def = Activities.Find(Item.ActivityId);
        return Def != nullptr ? Def->DurationMinutes : 0;
    }
    if (Item.Kind == PerfectWorld::WeekEntryKind::Sleep || Item.Kind == PerfectWorld::WeekEntryKind::Spend) return Item.Minutes;
    return 0;
}
} // namespace

// ---- Words ----

namespace PWSchoolPlanner
{
FText ClockText(const int32 MinuteOfDay)
{
    const int32 Clamped = FMath::Clamp(MinuteOfDay, 0, 1439);
    return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Clamped / 60, Clamped % 60));
}

FText DayText(const int64 CampaignDay)
{
    const PerfectWorld::CalendarTime Calendar = PerfectWorld::GameClock::ToCalendar(PerfectWorld::CalendarDayStartInstant(CampaignDay) + 12 * 60);
    return FText::Format(Text(TEXT("phase2.a6.date_fmt")), Text(*FString::Printf(TEXT("phase2.a6.dayname.%d"), static_cast<int32>(Calendar.DayOfWeek))),
                         FText::AsNumber(Calendar.Day), Text(*FString::Printf(TEXT("phase2.a6.month.%d"), Calendar.Month)));
}

FText WeekdaysText(const uint8 Weekdays)
{
    if (Weekdays == PerfectWorld::AttendanceSchoolWeekdays) return Text(TEXT("phase2.a6.weekdays.mon_fri"));
    TArray<FText> Names;
    for (int32 Day = 0; Day < 7; ++Day)
    {
        if ((Weekdays >> Day) & 1) Names.Add(Text(*FString::Printf(TEXT("phase2.a6.dayname.%d"), Day)));
    }
    return FText::Join(FText::FromString(TEXT(", ")), Names);
}

FText ModeText(const PerfectWorld::SchoolLessonMode Mode)
{
    switch (Mode)
    {
    case PerfectWorld::SchoolLessonMode::Routine: return Text(TEXT("phase2.a6.mode.routine"));
    case PerfectWorld::SchoolLessonMode::Foreground: return Text(TEXT("phase2.a6.mode.foreground"));
    case PerfectWorld::SchoolLessonMode::Attend: break;
    }
    return Text(TEXT("phase2.a6.mode.attend"));
}

FText OrderTitle(const uint8 Weekdays, const bool bAttendSchool, const PerfectWorld::SchoolLessonMode Mode)
{
    return bAttendSchool ? A6Fmt(TEXT("phase2.a6.order.attend_fmt"), WeekdaysText(Weekdays), ModeText(Mode))
                         : A6Fmt(TEXT("phase2.a6.order.skip_fmt"), WeekdaysText(Weekdays));
}

FText RefusalText(const FString& Code)
{
    const FText Found = FindText(FString(TEXT("phase2.a6.refusal.")) + Code);
    return Found.IsEmpty() ? Text(TEXT("phase2.a6.refusal.unknown")) : Found;
}

FText FindText(const FString& Key)
{
    if (Key.IsEmpty()) return FText::GetEmpty();
    FText Provided;
    if (A6Provider() && A6Provider()(Key, Provided) && !Provided.IsEmpty()) return Provided;
    for (const FA6TextEntry& Entry : GA6Text)
    {
        if (Key.Equals(Entry.Key, ESearchCase::CaseSensitive)) return FText::FromString(Entry.Value);
    }
    return FText::GetEmpty();
}

FText TextOr(const std::string& Key, const FText& Neutral)
{
    const FText Found = FindText(PWLaneLife::ToFString(Key));
    return Found.IsEmpty() ? Neutral : Found;
}

FText Text(const TCHAR* Key)
{
    const FText Found = FindText(FString(Key));
    return Found.IsEmpty() ? FText::FromString(TEXT("-")) : Found;
}

void SetTextProvider(TFunction<bool(const FString&, FText&)> Provider)
{
    A6Provider() = MoveTemp(Provider);
}

int32 BuiltInTextCount() { return UE_ARRAY_COUNT(GA6Text); }
const TCHAR* BuiltInTextKey(const int32 Index) { return GA6Text[Index].Key; }
const TCHAR* BuiltInTextValue(const int32 Index) { return GA6Text[Index].Value; }

void SetOpenHandler(TFunction<void()> Handler) { A6OpenHandler() = MoveTemp(Handler); }

bool RequestOpen()
{
    if (!A6OpenHandler()) return false;
    A6OpenHandler()();
    return true;
}

// ---- The recap store ----

const FPWRecapModel& LastRecap() { return A6RecapStore(); }

void StoreRecap(const FPWRecapModel& Recap)
{
    A6RecapStore() = Recap;
    ++A6RecapRevision();
}

void StoreResolution(const PerfectWorld::SchoolWeekPlan& Plan, const PerfectWorld::SchoolWeekResult& Result)
{
    const FPWRecapModel Recap = BuildRecap(Plan, Result);
    if (Recap.bPresent) StoreRecap(Recap);
}

void ClearRecap()
{
    A6RecapStore() = FPWRecapModel();
    ++A6RecapRevision();
}

uint32 RecapRevision() { return A6RecapRevision(); }

// ---- The plan model ----

FPWPlannerModel BuildModel(const FPWPlannerPreview& Preview, const FPWRecapModel& Recap, const FText& Clock)
{
    using namespace PerfectWorld;
    FPWPlannerModel Model;
    Model.bAvailable = true;
    Model.Clock = Clock;
    Model.Recap = Recap;
    for (const SchoolStandingOrder& Order : Preview.Standing.Orders)
    {
        FPWOrderRow& Row = Model.Orders.AddDefaulted_GetRef();
        Row.Id = PWLaneLife::ToFString(Order.Id);
        Row.Weekdays = Order.Weekdays;
        Row.bAttendSchool = Order.AttendSchool;
        Row.Mode = Order.LessonMode;
        Row.Title = OrderTitle(Order.Weekdays, Order.AttendSchool, Order.LessonMode);
    }
    for (const std::int64_t Day : Preview.Standing.SceneDays)
    {
        FPWSceneDayRow& Row = Model.SceneDays.AddDefaulted_GetRef();
        Row.Day = Day;
        Row.Date = DayText(Day);
    }
    if (!Preview.Build.Ok())
    {
        Model.PlanProblem = A6Fmt(TEXT("phase2.a6.week.problem_fmt"), RefusalText(PWLaneLife::ToFString(Preview.Build.Issue.Code)));
        return Model;
    }
    Model.bPlanOk = true;
    const SchoolWeekPlan& Plan = Preview.Build.Plan;
    const int64 FirstDay = A6CampaignDayOf(Preview.Now);
    const GameInstant Midnight = GameClock::StartOfDay(Preview.Now);
    bool bStopFound = false;
    for (int32 D = 0; D < static_cast<int32>(Plan.Week.Days.size()); ++D)
    {
        FPWPlanDayRow& Day = Model.Days.AddDefaulted_GetRef();
        Day.Day = D;
        Day.Heading = DayText(FirstDay + D);
        Day.bSceneDay = std::binary_search(Preview.Standing.SceneDays.begin(), Preview.Standing.SceneDays.end(), static_cast<std::int64_t>(FirstDay + D));
        const auto& Entries = Plan.Week.Days[D].Entries;
        for (int32 E = 0; E < static_cast<int32>(Entries.size()); ++E)
        {
            const WeekEntry& Item = Entries[E];
            FPWPlanEntryRow& Row = Day.Entries.AddDefaulted_GetRef();
            Row.Day = D;
            Row.Entry = E;
            Row.What = A6EntryWhat(Plan, D, E);
            const int32 Minutes = A6EntryMinutes(Item, Preview.Activities);
            Row.Time = Minutes > 0 ? A6Fmt(TEXT("phase2.a6.entry.time_range_fmt"), ClockText(Item.StartMinute), ClockText((Item.StartMinute + Minutes) % 1440))
                                   : ClockText(Item.StartMinute);
            Row.bDecidedByOrder = Item.DecidedByOrder;
            Row.bYouPlay = Item.FirstOfKind || Item.NeedsChoice;
            if (Item.DecidedByOrder) Row.Notes.Add(Text(TEXT("phase2.a6.mark.order")));
            const SchoolWeekLesson* Lesson = A6FindLesson(Plan, D, E);
            bool bReasoned = false;
            if (Item.FirstOfKind && Day.bSceneDay && E == 0)
            {
                Row.Notes.Add(Text(TEXT("phase2.a6.mark.scene_day")));
                bReasoned = true;
            }
            if (Item.FirstOfKind && Lesson != nullptr)
            {
                const bool bSeenBefore = std::any_of(Preview.Sessions.Periods.begin(), Preview.Sessions.Periods.end(),
                    [&](const SchoolPeriodRecord& Record) { return Record.BindingId == Lesson->Binding.Id && Record.Completed; });
                const TCHAR* Why = !bSeenBefore ? TEXT("phase2.a6.mark.first")
                    : Lesson->Mode == SchoolLessonMode::Foreground ? TEXT("phase2.a6.mark.foreground")
                    : TEXT("phase2.a6.mark.routine_not_ready");
                Row.Notes.Add(Text(Why));
                bReasoned = true;
            }
            if (Item.NeedsChoice)
            {
                Row.Notes.Add(Text(Plan.Week.InterruptsUsed >= WeekMaxInterrupts ? TEXT("phase2.a6.mark.choice_spent") : TEXT("phase2.a6.mark.choice")));
                bReasoned = true;
            }
            if (Row.bYouPlay && !bReasoned) Row.Notes.Add(Text(TEXT("phase2.a6.mark.you_play")));
            if (Row.bYouPlay && !bStopFound)
            {
                bStopFound = true;
                Row.bStopsHere = true;
                Row.Notes.Add(Text(TEXT("phase2.a6.mark.stops")));
                Model.bStopsAtAll = true;
                Model.StopSummary = A6Fmt(TEXT("phase2.a6.week.stop_fmt"), Row.What, Day.Heading);
            }
            // Only alternatives the player already knew, available, overlapping this window (the receipt rule, applied by the core).
            if (Minutes > 0 && Item.Kind == WeekEntryKind::Activity)
            {
                std::vector<ReceiptCandidate> Candidates = Item.Alternatives;
                Candidates.insert(Candidates.end(), Preview.Known.begin(), Preview.Known.end());
                ReceiptCommitment Commitment;
                Commitment.ActivityId = Item.ActivityId;
                Commitment.Start = Midnight + static_cast<GameInstant>(D) * MinutesPerDay + Item.StartMinute;
                Commitment.DecidedAt = std::min(Preview.Now, Commitment.Start);
                Commitment.DurationMinutes = Minutes;
                for (const ReceiptDisplaced& Passed : DecideDisplaced(Commitment, Candidates))
                {
                    FPWDisplacedRow& Alt = Row.Displaced.AddDefaulted_GetRef();
                    Alt.Name = A6AlternativeName(Passed.Id);
                    Alt.OverlapMinutes = Passed.OverlapMinutes;
                }
            }
            ++Model.EntryCount;
        }
    }
    if (Model.EntryCount > 0 && !bStopFound) Model.StopSummary = Text(TEXT("phase2.a6.week.stop_none"));
    return Model;
}

// ---- The recap of what really ran ----

FPWRecapModel BuildRecap(const PerfectWorld::SchoolWeekPlan& Plan, const PerfectWorld::SchoolWeekResult& Result)
{
    using namespace PerfectWorld;
    FPWRecapModel Recap;
    if (!Result.Ok()) return Recap;
    Recap.bPresent = true;
    std::vector<const DayLoopEvent*> Completed;
    for (const DayLoopEvent& Event : Result.ActivityEvents)
    {
        if (Event.Kind == DayLoopEventKind::Completed) Completed.push_back(&Event);
    }
    std::size_t NextEvent = 0;
    int64 PassedMinutes = 0;
    int32 Resolved = 0;
    for (const WeekResult& Segment : Result.Segments)
    {
        for (const WeekEntryOutcome& Outcome : Segment.Outcomes)
        {
            if (Outcome.Kind == WeekEntryKind::Spend)
            {
                PassedMinutes += Outcome.EndedAt - Outcome.StartedAt;
                continue;
            }
            FPWRecapRow& Row = Recap.Rows.AddDefaulted_GetRef();
            ++Resolved;
            const bool bTimed = Outcome.EndedAt > Outcome.StartedAt;
            const FText Range = bTimed ? A6Fmt(TEXT("phase2.a6.entry.time_range_fmt"), ClockText(A6MinuteOf(Outcome.StartedAt)), ClockText(A6MinuteOf(Outcome.EndedAt)))
                                       : ClockText(A6MinuteOf(Outcome.StartedAt));
            Row.Time = A6Fmt(TEXT("phase2.a6.entry.line_fmt"), DayText(A6CampaignDayOf(Outcome.StartedAt)), Range);
            if (Outcome.Kind == WeekEntryKind::School)
            {
                Row.What = Text(TEXT("phase2.a6.recap.arrived"));
                const TCHAR* Arrival = Outcome.Value == static_cast<int64>(AttendanceOutcome::OnTime) ? TEXT("phase2.a6.recap.on_time")
                    : Outcome.Value == static_cast<int64>(AttendanceOutcome::Late) ? TEXT("phase2.a6.recap.late")
                    : Outcome.Value == static_cast<int64>(AttendanceOutcome::VeryLate) ? TEXT("phase2.a6.recap.very_late")
                    : TEXT("phase2.a6.recap.absent");
                Row.Notes.Add(Text(Arrival));
            }
            else if (Outcome.Kind == WeekEntryKind::Sleep)
            {
                Row.What = Text(TEXT("phase2.a6.entry.sleep"));
            }
            else
            {
                // An activity outcome carries plan coordinates (CompressSchoolWeek resolves it through the DayLoop itself).
                const SchoolWeekLesson* Lesson = A6FindLesson(Plan, Outcome.Day, Outcome.Entry);
                Row.What = Lesson != nullptr ? A6Fmt(TEXT("phase2.a6.entry.lesson_fmt"), A6LessonName(Lesson->Binding)) : A6ActivityName(Outcome.What);
                // The receipt is the real one the day loop recorded for this completion, found by its own sequence number.
                const DayLoopEvent* Event = nullptr;
                for (std::size_t I = NextEvent; I < Completed.size(); ++I)
                {
                    if (Completed[I]->ActivityId == Outcome.What && Completed[I]->At == Outcome.EndedAt) { Event = Completed[I]; NextEvent = I + 1; break; }
                }
                if (Event != nullptr && !Event->Credited.empty()) Row.Notes.Add(Text(TEXT("phase2.a6.recap.learning")));
                if (Event != nullptr && Event->ReceiptSeq == 0) Row.Notes.Add(Text(TEXT("phase2.a6.recap.summarised")));
                if (Event != nullptr && Event->ReceiptSeq != 0)
                {
                    for (const ReceiptEntry& Receipt : Result.World.DayLoop.Receipts.Entries)
                    {
                        if (Receipt.Seq != Event->ReceiptSeq) continue;
                        Row.Notes.Add(FText::Format(Text(TEXT("phase2.a6.recap.reserved_fmt")), FText::AsNumber(Receipt.DurationMinutes)));
                        TArray<FText> Names;
                        for (const ReceiptDisplaced& Passed : Receipt.Displaced) Names.Add(A6AlternativeName(Passed.Id));
                        if (!Names.IsEmpty() || Receipt.OmittedCount > 0)
                        {
                            Row.Notes.Add(A6Fmt(TEXT("phase2.a6.recap.displaced_fmt"), A6JoinNames(Names, Receipt.OmittedCount)));
                        }
                        break;
                    }
                }
            }
            if (!Segment.Disclosures.empty()) Row.Notes.Insert(Text(TEXT("phase2.a6.recap.order")), 0);
        }
    }
    Recap.Headline = Resolved == 0 && PassedMinutes == 0 ? Text(TEXT("phase2.a6.recap.nothing_ran"))
                                                          : FText::Format(Text(TEXT("phase2.a6.recap.done_fmt")), FText::AsNumber(Resolved));
    if (PassedMinutes > 0) Recap.Passed = FText::Format(Text(TEXT("phase2.a6.recap.passed_fmt")), FText::AsNumber(PassedMinutes));
    const FText StopWhat = A6EntryWhat(Plan, Result.StopDay, Result.StopEntry);
    switch (Result.Stop)
    {
    case WeekStop::None: Recap.Stop = Text(TEXT("phase2.a6.recap.stop_none")); break;
    case WeekStop::FirstNeedsPlay: Recap.Stop = StopWhat.IsEmpty() ? Text(TEXT("phase2.a6.recap.stop_unknown")) : A6Fmt(TEXT("phase2.a6.recap.stop_first_fmt"), StopWhat); break;
    case WeekStop::ChoiceNeeded: Recap.Stop = StopWhat.IsEmpty() ? Text(TEXT("phase2.a6.recap.stop_unknown")) : A6Fmt(TEXT("phase2.a6.recap.stop_choice_fmt"), StopWhat); break;
    case WeekStop::InterruptBudgetSpent: Recap.Stop = StopWhat.IsEmpty() ? Text(TEXT("phase2.a6.recap.stop_unknown")) : A6Fmt(TEXT("phase2.a6.recap.stop_spent_fmt"), StopWhat); break;
    }
    Recap.Interrupts = FText::Format(Text(TEXT("phase2.a6.recap.interrupts_fmt")), FText::AsNumber(Result.InterruptsUsed));
    return Recap;
}

FPWPlannerModel ReadModel(IPWSchoolPlannerWorld& World)
{
    FPWPlannerPreview Preview;
    FString Why;
    if (!World.ReadPreview(Preview, Why))
    {
        FPWPlannerModel Unavailable;
        Unavailable.Recap = LastRecap();
        return Unavailable;
    }
    const FText Clock = A6Fmt(TEXT("phase2.a6.clock_fmt"), DayText(A6CampaignDayOf(Preview.Now)), ClockText(A6MinuteOf(Preview.Now)));
    return BuildModel(Preview, LastRecap(), Clock);
}
} // namespace PWSchoolPlanner

// ---- The widget ----

namespace
{
/** A quiet inset line that says why a list or the window is empty. */
TSharedRef<SWidget> A6Note(const FText& Words, const TCHAR* Tag)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SBorder)
        .Tag(FName(Tag))
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(14.0f, 10.0f))
        [
            PWUI::MakeText(Words, 16, Style.TextSecondary, false, true)
        ];
}

TSharedRef<SWidget> A6Heading(const FText& Words)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SBox)
        .Padding(FMargin(0.0f, 14.0f, 0.0f, 6.0f))
        [
            PWUI::MakeText(Words, PWUI::ETextRole::SectionHeading, Style.Accent, true)
        ];
}

TSharedRef<SWidget> A6EntryWidget(const FPWPlanEntryRow& Row)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
    Lines->AddSlot().AutoHeight()
    [
        PWUI::MakeText(A6Fmt(TEXT("phase2.a6.entry.line_fmt"), Row.Time, Row.What), 17, Style.TextPrimary, true, true)
    ];
    for (const FText& Note : Row.Notes)
    {
        Lines->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Note, PWUI::ETextRole::Meta, Row.bStopsHere || Row.bYouPlay ? Style.Accent : Style.TextSecondary, true)
        ];
    }
    if (!Row.Displaced.IsEmpty())
    {
        TArray<FText> Names;
        for (const FPWDisplacedRow& Alt : Row.Displaced)
        {
            Names.Add(A6Fmt(TEXT("phase2.a6.mark.displaced_item_fmt"), Alt.Name, FText::AsNumber(Alt.OverlapMinutes)));
        }
        Lines->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(A6Fmt(TEXT("phase2.a6.mark.displaced_fmt"), A6JoinNames(Names, 0)), PWUI::ETextRole::Meta, Style.TextSecondary, true)
        ];
    }
    return SNew(SBox)
        .Tag(FName(PWSchoolPlanner::EntryTag))
        .Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
        [
            SNew(SBorder)
            .BorderImage(&Style.BrushInset)
            .Padding(FMargin(12.0f, 8.0f))
            [
                Lines
            ]
        ];
}

bool A6IsBack(const FKey& Key)
{
    return Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_LeftShoulder;
}

FString A6GeneratedOrderId(const uint8 Weekdays)
{
    static const TCHAR* const Names[] = {TEXT("sun"), TEXT("mon"), TEXT("tue"), TEXT("wed"), TEXT("thu"), TEXT("fri"), TEXT("sat")};
    FString Id = TEXT("order.school");
    for (int32 Day = 0; Day < 7; ++Day)
    {
        if ((Weekdays >> Day) & 1) Id += FString(TEXT("_")) + Names[Day];
    }
    return Id;
}
} // namespace

void SPWSchoolPlanner::Construct(const FArguments& InArgs)
{
    World = InArgs._World;
    Space = InArgs._Space;
    OnClose = InArgs._OnClose;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    // Reading is free: building the window submits nothing.
    Model = World.IsValid() ? PWSchoolPlanner::ReadModel(*World) : FPWPlannerModel();
    const auto IsCompact = [this]()
    {
        const FVector2D S = GetSpace();
        return PWQuest::Compact(static_cast<float>(S.X), static_cast<float>(S.Y));
    };

    const TSharedRef<SWidget> Back = PWUI::MakeButton(
        PWSchoolPlanner::Text(TEXT("phase2.a6.common.back")), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            RequestClose();
            return FReply::Handled();
        }), true, 20, 0.0f);
    BackButton = Back;
    const TSharedRef<SWidget> Resolve = PWUI::MakeButton(
        PWSchoolPlanner::Text(TEXT("phase2.a6.resolve.button")), PWUI::EButtonKind::Primary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            ResolveWeek();
            return FReply::Handled();
        }),
        TAttribute<bool>::CreateSPLambda(this, [this]() { return Model.bPlanOk && Model.EntryCount > 0; }), 20, 0.0f);
    ResolveButton = Resolve;

    const TSharedRef<SWidget> Header =
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(PWSchoolPlanner::Text(TEXT("phase2.a6.planner.title")))
            .Font_Lambda([IsCompact]() { return PWUI::Font(IsCompact() ? 22 : 32, true); })
            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(16.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return Model.Clock; })
            .Font(PWUI::Font(15, PWUI::EFontWeight::Medium))
            .ColorAndOpacity(FSlateColor(Style.TextSecondary))
            .Justification(ETextJustify::Right)
            .AutoWrapText(true)
        ];

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride_Lambda([this]()
        {
            const FVector2D S = GetSpace();
            return FOptionalSize(PWQuest::PanelWidth(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        .MaxDesiredHeight_Lambda([this]()
        {
            const FVector2D S = GetSpace();
            return FOptionalSize(PWQuest::PanelMaxHeight(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        [
            SNew(SBorder)
            .Tag(FName(PWSchoolPlanner::PanelTag))
            .BorderImage(&Style.BrushPanel)
            .Padding(0.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SPWCornerBrackets)
                ]
                + SOverlay::Slot()
                [
                    SNew(SBox)
                    .Padding_Lambda([IsCompact]() { return IsCompact() ? FMargin(24.0f, 10.0f) : FMargin(40.0f, 28.0f); })
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            Header
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 8.0f)
                        [
                            PWUI::MakeRule()
                        ]
                        + SVerticalBox::Slot().FillHeight(1.0f)
                        [
                            SAssignNew(BodyHolder, SBox)
                            [
                                BuildBody()
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox)
                            .Visibility_Lambda([IsCompact]() { return IsCompact() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                            [
                                PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.planner.hint")), PWUI::ETextRole::Meta, Style.TextSecondary, true)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
                            [
                                SNew(SBox).WidthOverride(260.0f)[Resolve]
                            ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(180.0f)[Back]
                            ]
                        ]
                    ]
                ]
            ]
        ]
    ];
    PWUI::FocusWhenReady(AsShared(), BackButton);
}

FVector2D SPWSchoolPlanner::GetSpace() const
{
    return Space.Get(FVector2D(1920.0, 1080.0));
}

TSharedRef<SWidget> SPWSchoolPlanner::BuildBody()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    BodyScroll.Reset();
    if (!Model.bAvailable)
    {
        return A6Note(PWSchoolPlanner::Text(TEXT("phase2.a6.planner.unavailable")), PWSchoolPlanner::UnavailableTag);
    }
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    Column->AddSlot().AutoHeight()
    [
        PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.planner.free_note")), 16, Style.TextSecondary, false, true)
    ];
    if (!Notice.IsEmpty())
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Notice, 17, Style.Accent, true, true)
        ];
    }
    Column->AddSlot().AutoHeight()[BuildRecapSection()];
    Column->AddSlot().AutoHeight()[BuildOrders()];
    Column->AddSlot().AutoHeight()[BuildEditor()];
    Column->AddSlot().AutoHeight()[BuildSceneDays()];
    Column->AddSlot().AutoHeight()[BuildWeek()];
    return SAssignNew(BodyScroll, SScrollBox)
        .ScrollBarStyle(&Style.ScrollBar)
        .AllowOverscroll(EAllowOverscroll::No)
        + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
        [
            Column
        ];
}

TSharedRef<SWidget> SPWSchoolPlanner::BuildRecapSection()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FPWRecapModel& Recap = Model.Recap;
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    Column->AddSlot().AutoHeight()[A6Heading(PWSchoolPlanner::Text(TEXT("phase2.a6.recap.heading")))];
    if (!Recap.bPresent)
    {
        Column->AddSlot().AutoHeight()[A6Note(PWSchoolPlanner::Text(TEXT("phase2.a6.recap.none")), PWSchoolPlanner::NoRecapTag)];
        return Column;
    }
    const TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
    Lines->AddSlot().AutoHeight()[PWUI::MakeText(Recap.Headline, 17, Style.TextPrimary, true, true)];
    Lines->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[PWUI::MakeText(Recap.Stop, 16, Style.Accent, true, true)];
    if (!Recap.Passed.IsEmpty())
    {
        Lines->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[PWUI::MakeText(Recap.Passed, PWUI::ETextRole::Meta, Style.TextSecondary, true)];
    }
    Lines->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 6.0f)[PWUI::MakeText(Recap.Interrupts, PWUI::ETextRole::Meta, Style.TextSecondary, true)];
    for (const FPWRecapRow& Row : Recap.Rows)
    {
        const TSharedRef<SVerticalBox> Cell = SNew(SVerticalBox);
        Cell->AddSlot().AutoHeight()[PWUI::MakeText(A6Fmt(TEXT("phase2.a6.entry.line_fmt"), Row.Time, Row.What), 16, Style.TextPrimary, true, true)];
        for (const FText& Note : Row.Notes)
        {
            Cell->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)[PWUI::MakeText(Note, PWUI::ETextRole::Meta, Style.TextSecondary, true)];
        }
        Lines->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(SBox).Tag(FName(PWSchoolPlanner::RecapRowTag))
            [
                SNew(SBorder).BorderImage(&Style.BrushInset).Padding(FMargin(12.0f, 8.0f))[Cell]
            ]
        ];
    }
    Column->AddSlot().AutoHeight()[SNew(SBox).Tag(FName(PWSchoolPlanner::RecapTag))[Lines]];
    return Column;
}

TSharedRef<SWidget> SPWSchoolPlanner::BuildOrders()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    Column->AddSlot().AutoHeight()[A6Heading(PWSchoolPlanner::Text(TEXT("phase2.a6.orders.heading")))];
    if (Model.Orders.IsEmpty())
    {
        Column->AddSlot().AutoHeight()[A6Note(PWSchoolPlanner::Text(TEXT("phase2.a6.orders.empty")), PWSchoolPlanner::OrderRowTag)];
        return Column;
    }
    for (const FPWOrderRow& Order : Model.Orders)
    {
        const FString Id = Order.Id;
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(SBox).Tag(FName(PWSchoolPlanner::OrderRowTag))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    PWUI::MakeText(Order.Title, 17, Style.TextPrimary, false, true)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeButton(PWSchoolPlanner::Text(TEXT("phase2.a6.orders.edit")), PWUI::EButtonKind::Secondary,
                        FOnClicked::CreateSPLambda(this, [this, Id]()
                        {
                            LoadDraft(Id);
                            return FReply::Handled();
                        }), true, 16, 0.0f)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeButton(PWSchoolPlanner::Text(TEXT("phase2.a6.orders.remove")), PWUI::EButtonKind::Secondary,
                        FOnClicked::CreateSPLambda(this, [this, Id]()
                        {
                            RemoveOrder(Id);
                            return FReply::Handled();
                        }), true, 16, 0.0f)
                ]
            ]
        ];
    }
    return Column;
}

TSharedRef<SWidget> SPWSchoolPlanner::BuildEditor()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SHorizontalBox> Days = SNew(SHorizontalBox);
    for (int32 Weekday = 1; Weekday <= 6; ++Weekday)
    {
        Days->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 6.0f)
        [
            PWUI::MakeButton(PWSchoolPlanner::Text(*FString::Printf(TEXT("phase2.a6.dayname.%d"), Weekday)), PWUI::EButtonKind::Chip,
                FOnClicked::CreateSPLambda(this, [this, Weekday]()
                {
                    ToggleDraftDay(Weekday);
                    return FReply::Handled();
                }), true, 16, 0.0f,
                TAttribute<bool>::CreateSPLambda(this, [this, Weekday]() { return ((DraftWeekdays >> Weekday) & 1) != 0; }))
        ];
    }
    const TSharedRef<SHorizontalBox> Modes = SNew(SHorizontalBox);
    const auto AddMode = [this, &Modes](const TCHAR* Key, const PerfectWorld::SchoolLessonMode Mode)
    {
        Modes->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 6.0f)
        [
            PWUI::MakeButton(PWSchoolPlanner::Text(Key), PWUI::EButtonKind::Chip,
                FOnClicked::CreateSPLambda(this, [this, Mode]()
                {
                    SetDraftMode(Mode);
                    return FReply::Handled();
                }),
                TAttribute<bool>::CreateSPLambda(this, [this]() { return bDraftAttend; }), 16, 0.0f,
                TAttribute<bool>::CreateSPLambda(this, [this, Mode]() { return DraftMode == Mode; }))
        ];
    };
    AddMode(TEXT("phase2.a6.mode.chip.attend"), PerfectWorld::SchoolLessonMode::Attend);
    AddMode(TEXT("phase2.a6.mode.chip.routine"), PerfectWorld::SchoolLessonMode::Routine);
    AddMode(TEXT("phase2.a6.mode.chip.foreground"), PerfectWorld::SchoolLessonMode::Foreground);
    const TSharedRef<SWidget> Save = PWUI::MakeButton(
        PWSchoolPlanner::Text(TEXT("phase2.a6.editor.save")), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            SaveDraft();
            return FReply::Handled();
        }), true, 18, 0.0f);
    SaveButton = Save;
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[A6Heading(PWSchoolPlanner::Text(TEXT("phase2.a6.editor.heading")))]
        + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.editor.days")), PWUI::ETextRole::Meta, Style.TextSecondary, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[Days]
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeButton(PWSchoolPlanner::Text(TEXT("phase2.a6.editor.attend")), PWUI::EButtonKind::Chip,
                FOnClicked::CreateSPLambda(this, [this]()
                {
                    SetDraftAttend(!bDraftAttend);
                    return FReply::Handled();
                }), true, 16, 0.0f,
                TAttribute<bool>::CreateSPLambda(this, [this]() { return bDraftAttend; }))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.editor.mode")), PWUI::ETextRole::Meta, Style.TextSecondary, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[Modes]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[SNew(SBox).MinDesiredWidth(180.0f)[Save]]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.editor.hint")), PWUI::ETextRole::Meta, Style.TextSecondary, true)
        ];
}

TSharedRef<SWidget> SPWSchoolPlanner::BuildSceneDays()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    Column->AddSlot().AutoHeight()[A6Heading(PWSchoolPlanner::Text(TEXT("phase2.a6.scene.heading")))];
    if (Model.SceneDays.IsEmpty())
    {
        Column->AddSlot().AutoHeight()[A6Note(PWSchoolPlanner::Text(TEXT("phase2.a6.scene.empty")), PWSchoolPlanner::SceneDayTag)];
        return Column;
    }
    Column->AddSlot().AutoHeight()[PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.scene.note")), PWUI::ETextRole::Meta, Style.TextSecondary, true)];
    for (const FPWSceneDayRow& Day : Model.SceneDays)
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            SNew(SBox).Tag(FName(PWSchoolPlanner::SceneDayTag))[PWUI::MakeText(Day.Date, 17, Style.TextPrimary, true, true)]
        ];
    }
    return Column;
}

TSharedRef<SWidget> SPWSchoolPlanner::BuildWeek()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    Column->AddSlot().AutoHeight()[A6Heading(PWSchoolPlanner::Text(TEXT("phase2.a6.week.heading")))];
    if (!Model.bPlanOk)
    {
        Column->AddSlot().AutoHeight()[A6Note(Model.PlanProblem, PWSchoolPlanner::ProblemTag)];
        return Column;
    }
    if (Model.EntryCount == 0)
    {
        Column->AddSlot().AutoHeight()[A6Note(PWSchoolPlanner::Text(TEXT("phase2.a6.week.empty")), PWSchoolPlanner::ProblemTag)];
        return Column;
    }
    Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWUI::MakeText(Model.StopSummary, 16, Model.bStopsAtAll ? Style.Accent : Style.Good, true, true)];
    Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.resolve.note")), PWUI::ETextRole::Meta, Style.TextSecondary, true)];
    for (const FPWPlanDayRow& Day : Model.Days)
    {
        if (Day.Entries.IsEmpty()) continue;
        const TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
        Rows->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 4.0f)
        [
            PWUI::MakeText(Day.bSceneDay ? A6Fmt(TEXT("phase2.a6.entry.line_fmt"), Day.Heading, PWSchoolPlanner::Text(TEXT("phase2.a6.week.scene_day"))) : Day.Heading,
                           17, Style.TextPrimary, true, true)
        ];
        for (const FPWPlanEntryRow& Entry : Day.Entries)
        {
            Rows->AddSlot().AutoHeight()[A6EntryWidget(Entry)];
        }
        Column->AddSlot().AutoHeight()[SNew(SBox).Tag(FName(PWSchoolPlanner::DayTag))[Rows]];
    }
    return Column;
}

// ---- Behaviour. Nothing below runs on open, browse or close; only Save, Remove and Resolve reach the bridge. ----

void SPWSchoolPlanner::Refresh()
{
    Model = World.IsValid() ? PWSchoolPlanner::ReadModel(*World) : FPWPlannerModel();
}

void SPWSchoolPlanner::Rebuild(const TWeakPtr<SWidget>& FocusTarget)
{
    if (!BodyHolder.IsValid()) return;
    const float Offset = BodyScroll.IsValid() ? BodyScroll->GetScrollOffset() : 0.0f;
    BodyHolder->SetContent(BuildBody());
    if (BodyScroll.IsValid()) BodyScroll->SetScrollOffset(Offset);
    PWUI::FocusWhenReady(AsShared(), FocusTarget.IsValid() ? FocusTarget : BackButton);
}

void SPWSchoolPlanner::ToggleDraftDay(const int32 Weekday)
{
    if (Weekday < 0 || Weekday > 6) return;
    DraftWeekdays = static_cast<uint8>(DraftWeekdays ^ (1u << Weekday));
}

void SPWSchoolPlanner::SetDraftAttend(const bool bAttend)
{
    bDraftAttend = bAttend;
}

void SPWSchoolPlanner::SetDraftMode(const PerfectWorld::SchoolLessonMode Mode)
{
    DraftMode = Mode;
}

void SPWSchoolPlanner::LoadDraft(const FString& OrderId)
{
    for (const FPWOrderRow& Order : Model.Orders)
    {
        if (Order.Id != OrderId) continue;
        DraftId = Order.Id;
        DraftWeekdays = Order.Weekdays;
        bDraftAttend = Order.bAttendSchool;
        DraftMode = Order.Mode;
        return;
    }
}

FPWPlannerOutcome SPWSchoolPlanner::SaveDraft()
{
    FPWPlannerOutcome Outcome;
    if (!World.IsValid() || DraftWeekdays == 0)
    {
        Outcome.Code = TEXT("ui.no_days");
        Notice = PWSchoolPlanner::Text(TEXT("phase2.a6.editor.no_days"));
        Rebuild(SaveButton);
        return Outcome;
    }
    PerfectWorld::SchoolStandingOrder Order;
    Order.Id = std::string(TCHAR_TO_UTF8(*(DraftId.IsEmpty() ? A6GeneratedOrderId(DraftWeekdays) : DraftId)));
    Order.Weekdays = DraftWeekdays;
    Order.AttendSchool = bDraftAttend;
    Order.LessonMode = bDraftAttend ? DraftMode : PerfectWorld::SchoolLessonMode::Attend;
    Outcome = World->SetOrder(Order, false);
    Notice = Outcome.bAccepted ? PWSchoolPlanner::Text(TEXT("phase2.a6.editor.saved")) : PWSchoolPlanner::RefusalText(Outcome.Code);
    if (Outcome.bAccepted)
    {
        DraftId.Reset();
        DraftWeekdays = 0;
    }
    Refresh();
    Rebuild(SaveButton);
    return Outcome;
}

FPWPlannerOutcome SPWSchoolPlanner::RemoveOrder(const FString& OrderId)
{
    FPWPlannerOutcome Outcome;
    if (!World.IsValid())
    {
        return Outcome;
    }
    PerfectWorld::SchoolStandingOrder Order;
    Order.Id = std::string(TCHAR_TO_UTF8(*OrderId));
    Outcome = World->SetOrder(Order, true);
    Notice = Outcome.bAccepted ? PWSchoolPlanner::Text(TEXT("phase2.a6.editor.removed")) : PWSchoolPlanner::RefusalText(Outcome.Code);
    if (DraftId == OrderId)
    {
        DraftId.Reset();
        DraftWeekdays = 0;
    }
    Refresh();
    Rebuild(BackButton);
    return Outcome;
}

FPWPlannerOutcome SPWSchoolPlanner::ResolveWeek()
{
    FPWPlannerOutcome Outcome;
    if (!World.IsValid() || !Model.bPlanOk || Model.EntryCount == 0)
    {
        Outcome.Code = TEXT("ui.nothing_to_resolve");
        Notice = PWSchoolPlanner::Text(TEXT("phase2.a6.resolve.nothing"));
        Rebuild(ResolveButton);
        return Outcome;
    }
    ++ResolveCount;
    const FPWPlannerResolution Resolution = World->Resolve();
    Outcome = Resolution.Outcome;
    if (Outcome.bAccepted && Resolution.Result.Ok())
    {
        PWSchoolPlanner::StoreResolution(Resolution.Plan, Resolution.Result);
        Notice = FText::GetEmpty();
    }
    else
    {
        Notice = PWSchoolPlanner::RefusalText(Outcome.Code);
    }
    Refresh();
    Rebuild(ResolveButton);
    return Outcome;
}

void SPWSchoolPlanner::ScrollBody(const float Delta)
{
    if (BodyScroll.IsValid())
    {
        BodyScroll->SetScrollOffset(FMath::Clamp(BodyScroll->GetScrollOffset() + Delta, 0.0f, BodyScroll->GetScrollOffsetOfEnd()));
    }
}

void SPWSchoolPlanner::RequestClose()
{
    OnClose.ExecuteIfBound();
}

FReply SPWSchoolPlanner::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    constexpr float A6ScrollStep = 72.0f;
    const FKey Key = InKeyEvent.GetKey();
    if (A6IsBack(Key))
    {
        // A held key auto-repeats; this window goes back on the first press only.
        if (!InKeyEvent.IsRepeat())
        {
            RequestClose();
        }
        return FReply::Handled();
    }
    if (Key == EKeys::PageUp || Key == EKeys::PageDown)
    {
        ScrollBody(Key == EKeys::PageUp ? -4.0f * A6ScrollStep : 4.0f * A6ScrollStep);
        return FReply::Handled();
    }
    if (Key == EKeys::Gamepad_RightStick_Up || Key == EKeys::Gamepad_RightStick_Down)
    {
        ScrollBody(Key == EKeys::Gamepad_RightStick_Up ? -A6ScrollStep : A6ScrollStep);
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}
