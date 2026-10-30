#include "UI/SPWClassInvestigate.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <algorithm>
#include <variant>

// The shared A3 lesson shell (words, bridge, frame, screen chrome and the pressable rows) and the StructureInvestigate lesson.
// The Coordinate lesson is in SPWClassCoordinate.cpp. Nothing here decides anything: see the header.

#define LOCTEXT_NAMESPACE "PerfectWorldClassA3"

namespace
{
/** One English line. The key is also the localisation key and the Data/Text/en/phase2_a3.json key. */
struct FA3IEntry
{
    const TCHAR* Key;
    FText Text;
};
#define A3T(Key, Text) FA3IEntry{TEXT(Key), LOCTEXT(Key, Text)}

const TMap<FString, FText>& A3IStrings()
{
    static const TMap<FString, FText> Map = []()
    {
        const FA3IEntry Entries[] = {
            // ---- common ----
            A3T("phase2.a3.common.lesson", "Lesson"),
            A3T("phase2.a3.common.back", "Back"),
            A3T("phase2.a3.common.leave", "Leave early"),
            A3T("phase2.a3.common.start", "Start the lesson"),
            A3T("phase2.a3.common.record", "Record the period"),
            A3T("phase2.a3.common.record_hint", "Closing the period charges its bell time once and records what you learned."),
            A3T("phase2.a3.common.period_line", "Period {0}  -  {1} to {2}"),
            A3T("phase2.a3.common.room_line", "Room {0}"),
            A3T("phase2.a3.common.role_line", "Your part: {0}"),
            A3T("phase2.a3.common.mode_heading", "How will you take this lesson?"),
            A3T("phase2.a3.common.goal_heading", "Where will you put your effort?"),
            A3T("phase2.a3.common.quotes_note", "Nothing is charged by choosing. Every step shows its price before you commit, and you are charged exactly that price."),
            A3T("phase2.a3.common.leave_note", "Leaving early uses up the period, charges the time you spent and earns no learning."),
            A3T("phase2.a3.common.mode.foreground", "Work the exercise yourself"),
            A3T("phase2.a3.common.mode.routine", "Use your established routine"),
            A3T("phase2.a3.common.mode.attend", "Sit through the lesson"),
            A3T("phase2.a3.common.mode.foreground.hint", "A live exercise with a time budget. At most two a day, and a lesson you start cannot be taken back."),
            A3T("phase2.a3.common.mode.routine.hint", "For a kind of exercise you have mastered: it counts as a sound exercise, with the normal learning, when you record the period."),
            A3T("phase2.a3.common.mode.attend.hint", "Supervised instruction with the normal learning. It does not count as a new exercise."),
            A3T("phase2.a3.common.mode.unavailable.foreground", "You have used both foreground lessons for today."),
            A3T("phase2.a3.common.mode.unavailable.routine", "Routine is open once you have mastered this kind of exercise."),
            A3T("phase2.a3.common.units", "Time left: {0} of {1} units"),
            A3T("phase2.a3.common.cost.free", "Free"),
            A3T("phase2.a3.common.cost.one", "1 unit"),
            A3T("phase2.a3.common.cost.many", "{0} units"),
            A3T("phase2.a3.common.cost.short", "Needs {0}, {1} left"),
            A3T("phase2.a3.common.method.known", "Method: {0}. You have it, so prices are standard."),
            A3T("phase2.a3.common.method.unfamiliar", "Method: {0}. It is new to you, which costs extra on some steps."),
            A3T("phase2.a3.common.grade.sound", "Sound"),
            A3T("phase2.a3.common.grade.partial", "Partly sound"),
            A3T("phase2.a3.common.grade.unsound", "Not sound"),
            A3T("phase2.a3.common.outcome_heading", "How it went"),
            A3T("phase2.a3.common.outcome.grade", "Result: {0}"),
            A3T("phase2.a3.common.outcome.units", "Time used: {0} units"),
            A3T("phase2.a3.common.outcome.secure", "You have now mastered this kind of exercise. Routine will be open to you."),
            A3T("phase2.a3.common.outcome.no_faults", "Nothing went wrong in your method."),
            A3T("phase2.a3.common.outcome.routine", "Resolved from your mastery of this kind of exercise. Recording the period counts it as a sound exercise with the normal learning."),
            A3T("phase2.a3.common.outcome.attend", "You sat the lesson. It counts as supervised instruction, not as a new exercise."),
            A3T("phase2.a3.common.outcome.pending", "The core has not reported a result for this lesson."),
            A3T("phase2.a3.common.opened", "The lesson has started."),
            A3T("phase2.a3.common.exercise_done", "The exercise is finished. Record the period to close it."),
            A3T("phase2.a3.common.recorded", "The period is recorded."),
            A3T("phase2.a3.common.left", "You left the lesson early. The period is used and nothing was learned."),
            A3T("phase2.a3.common.closed_note", "This lesson is over. Go back to the school day."),
            A3T("phase2.a3.common.other_lesson", "Another lesson is in progress. Finish it or leave it first."),
            A3T("phase2.a3.common.unavailable", "This lesson cannot be read right now. Go back and try again."),
            A3T("phase2.a3.common.no_exercise", "There is no exercise running for this lesson."),
            A3T("phase2.a3.common.hints", "Up and Down choose. Enter or A acts. G or RB changes the focus, M or LB how you take the lesson. Esc or B goes back."),
            // ---- core refusals (school.session.* and structure.*) ----
            A3T("phase2.a3.issue.unknown", "That is not possible right now."),
            A3T("phase2.a3.issue.school.session.closed_day", "There is no lesson today."),
            A3T("phase2.a3.issue.school.session.period_used", "This period has already been used."),
            A3T("phase2.a3.issue.school.session.busy", "Something else is going on right now."),
            A3T("phase2.a3.issue.school.session.not_at_bell", "The lesson starts at the bell. Come back then."),
            A3T("phase2.a3.issue.school.session.not_attending", "You are not signed in for this period."),
            A3T("phase2.a3.issue.school.session.history_full", "The school record is full for this year."),
            A3T("phase2.a3.issue.school.session.invalid_state", "The lesson record could not be read."),
            A3T("phase2.a3.issue.school.session.bad_binding", "This lesson is not set up correctly."),
            A3T("phase2.a3.issue.school.session.period_content_mismatch", "This lesson does not match the timetable."),
            A3T("phase2.a3.issue.school.session.unknown_family", "This kind of exercise is not available."),
            A3T("phase2.a3.issue.school.session.missing_catalogue", "The school content is not loaded."),
            A3T("phase2.a3.issue.school.session.foreground_limit", "You have used both foreground lessons for today."),
            A3T("phase2.a3.issue.school.session.not_secure", "Routine is open once you have mastered this kind of exercise."),
            A3T("phase2.a3.issue.school.session.stale_session", "That lesson was already closed or restarted. Reopen it."),
            A3T("phase2.a3.issue.school.session.ready_to_close", "The exercise is finished. Record the period."),
            A3T("phase2.a3.issue.school.session.wrong_action", "That step does not belong to this lesson."),
            A3T("phase2.a3.issue.school.session.unfinished", "Finish the exercise first, or leave early."),
            A3T("phase2.a3.issue.school.session.already_resolved", "The exercise is finished and already counted. Record the period."),
            A3T("phase2.a3.issue.school.session.bad_elapsed", "That amount of time cannot be charged."),
            A3T("phase2.a3.issue.school.session.bad_mode", "That way of taking the lesson is not available."),
            A3T("phase2.a3.issue.structure.units_short", "There is not enough time left for that."),
            A3T("phase2.a3.issue.structure.already_done", "You have already done that."),
            A3T("phase2.a3.issue.structure.wrong_beat", "That is not the step you are on."),
            A3T("phase2.a3.issue.structure.bad_choice", "That choice is incomplete."),
            A3T("phase2.a3.issue.structure.unknown_item", "That is not part of this exercise."),
            A3T("phase2.a3.issue.structure.not_active", "There is no exercise running."),
            A3T("phase2.a3.issue.structure.busy", "An exercise is already running."),
            A3T("phase2.a3.issue.structure.invalid_state", "The exercise record could not be read."),
            // ---- investigate ----
            A3T("phase2.a3.investigate.subtitle", "Find the fault: run tests, read the evidence, then name the cause or report honestly."),
            A3T("phase2.a3.investigate.beat.isolate", "Step 1: Isolate. Run the tests you can afford; each one rules causes in or out."),
            A3T("phase2.a3.investigate.beat.conclude", "Step 2: Conclude. Name the cause, or report that the fault is not isolated. Reporting is free."),
            A3T("phase2.a3.investigate.evidence_heading", "Evidence so far"),
            A3T("phase2.a3.investigate.evidence", "{0}: {1}. {2}"),
            A3T("phase2.a3.investigate.evidence.positive", "positive"),
            A3T("phase2.a3.investigate.evidence.negative", "negative"),
            A3T("phase2.a3.investigate.evidence.causes", "Still possible: {0}."),
            A3T("phase2.a3.investigate.evidence.causes_none", "No cause still fits."),
            A3T("phase2.a3.investigate.test.implicates", "Would implicate: {0}"),
            A3T("phase2.a3.investigate.test.kind_line", "{0}. {1}"),
            A3T("phase2.a3.investigate.test.done_positive", "Run: positive. The fault is among the causes it implicates."),
            A3T("phase2.a3.investigate.test.done_negative", "Run: negative. The causes it implicates are ruled out."),
            A3T("phase2.a3.investigate.test.ran", "Ran {0}."),
            A3T("phase2.a3.investigate.kind.observe", "Observe"),
            A3T("phase2.a3.investigate.kind.measure", "Measure"),
            A3T("phase2.a3.investigate.kind.swap", "Swap a part"),
            A3T("phase2.a3.investigate.cause.standing", "Still possible"),
            A3T("phase2.a3.investigate.cause.ruled_out", "Ruled out by the evidence"),
            A3T("phase2.a3.investigate.cause.guess", "Other causes still stand, so naming this one is a guess."),
            A3T("phase2.a3.investigate.cause.action", "Diagnose: {0}"),
            A3T("phase2.a3.investigate.report.action", "Report that the fault is not isolated"),
            A3T("phase2.a3.investigate.report.hint", "An honest report costs nothing and ends the exercise."),
            A3T("phase2.a3.investigate.report.isolated", "The evidence already leaves only one cause standing."),
            A3T("phase2.a3.investigate.symptom.wrong_output", "Symptom: wrong output"),
            A3T("phase2.a3.investigate.symptom.no_response", "Symptom: no response"),
            A3T("phase2.a3.investigate.symptom.intermittent", "Symptom: it comes and goes"),
            A3T("phase2.a3.investigate.role.inspector", "Your part: inspector (observing tests are cheaper)"),
            A3T("phase2.a3.investigate.role.experimenter", "Your part: experimenter (measuring tests are cheaper)"),
            A3T("phase2.a3.investigate.role.repairer", "Your part: repairer (swapping tests are cheaper)"),
            A3T("phase2.a3.investigate.role.critic", "Your part: critic (an honest report is your job)"),
            A3T("phase2.a3.investigate.scarce.time", "Short on: time (a shorter budget)"),
            A3T("phase2.a3.investigate.scarce.equipment", "Short on: instruments (measuring tests cost 1 more)"),
            A3T("phase2.a3.investigate.scarce.parts", "Short on: spare parts (swapping tests cost 1 more)"),
            A3T("phase2.a3.investigate.scarce.access", "Short on: access (observing tests cost 1 more)"),
            A3T("phase2.a3.investigate.goal.inspect", "Inspect closely"),
            A3T("phase2.a3.investigate.goal.measure", "Take readings"),
            A3T("phase2.a3.investigate.goal.swap_parts", "Swap parts"),
            A3T("phase2.a3.investigate.goal.inspect.hint", "Observing tests are cheaper this lesson."),
            A3T("phase2.a3.investigate.goal.measure.hint", "Measuring tests are cheaper this lesson."),
            A3T("phase2.a3.investigate.goal.swap_parts.hint", "Swapping tests are cheaper this lesson."),
            A3T("phase2.a3.investigate.outcome.diagnosed", "You named: {0}."),
            A3T("phase2.a3.investigate.outcome.reported", "You reported that the fault was not isolated."),
            A3T("phase2.a3.investigate.outcome.tests", "Tests run: {0}. Wasted: {1}. Causes still standing at the end: {2}."),
            A3T("phase2.a3.investigate.fail.wrong_cause", "The cause you named was not the fault."),
            A3T("phase2.a3.investigate.fail.unsupported", "You named a cause while other causes still stood: the evidence did not support it."),
            A3T("phase2.a3.investigate.fail.redundant", "You ran a test whose answer the earlier tests had already settled."),
            A3T("phase2.a3.investigate.fail.abandoned", "You reported that the fault was not isolated when the evidence had isolated it."),
            // ---- coordinate ----
            A3T("phase2.a3.coordinate.subtitle", "Plan the group's work: give out the tasks, talk to your classmates, settle what binds you, hand over together."),
            A3T("phase2.a3.coordinate.beat.assign", "Step 1: Assign. Give every task to someone who suits it and can carry it."),
            A3T("phase2.a3.coordinate.beat.exchange", "Step 2: Exchange. Talk to classmates one at a time to learn what they know."),
            A3T("phase2.a3.coordinate.beat.settle", "Step 3: Settle. Choose how the group deals with what binds it."),
            A3T("phase2.a3.coordinate.beat.submit", "Step 4: Submit. Check that the parts join, or hand in as it stands."),
            A3T("phase2.a3.coordinate.role.chair", "Your part: chair (assigning is cheaper)"),
            A3T("phase2.a3.coordinate.role.mediator", "Your part: mediator (conversations are cheaper)"),
            A3T("phase2.a3.coordinate.role.treasurer", "Your part: treasurer (settling is cheaper)"),
            A3T("phase2.a3.coordinate.role.scribe", "Your part: scribe (the hand-over check is cheaper)"),
            A3T("phase2.a3.coordinate.scarce.time", "Short on: time (a shorter budget)"),
            A3T("phase2.a3.coordinate.scarce.attention", "Short on: attention (each conversation costs 1 more)"),
            A3T("phase2.a3.coordinate.scarce.space", "Short on: space (settling costs 1 more)"),
            A3T("phase2.a3.coordinate.scarce.materials", "Short on: materials (the hand-over check costs 1 more)"),
            A3T("phase2.a3.coordinate.goal.organise_work", "Organise the work"),
            A3T("phase2.a3.coordinate.goal.hear_everyone", "Hear everyone"),
            A3T("phase2.a3.coordinate.goal.settle_constraint", "Settle the constraint"),
            A3T("phase2.a3.coordinate.goal.submit_cleanly", "Submit cleanly"),
            A3T("phase2.a3.coordinate.goal.organise_work.hint", "Assigning the tasks is cheaper this lesson."),
            A3T("phase2.a3.coordinate.goal.hear_everyone.hint", "Conversations with classmates are cheaper this lesson."),
            A3T("phase2.a3.coordinate.goal.settle_constraint.hint", "Settling the constraint is cheaper this lesson."),
            A3T("phase2.a3.coordinate.goal.submit_cleanly.hint", "The hand-over check is cheaper this lesson."),
            A3T("phase2.a3.coordinate.members_heading", "The group"),
            A3T("phase2.a3.coordinate.member.line", "{0}: can carry {1}, carrying {2}"),
            A3T("phase2.a3.coordinate.member.you", "{0} (you)"),
            A3T("phase2.a3.coordinate.member.over", "over their limit"),
            A3T("phase2.a3.coordinate.tasks_heading", "Tasks (press a task to give it to the next person)"),
            A3T("phase2.a3.coordinate.task.line", "Load {0}. Suits: {1}"),
            A3T("phase2.a3.coordinate.task.unassigned", "Not given to anyone yet"),
            A3T("phase2.a3.coordinate.task.assigned", "Given to {0}"),
            A3T("phase2.a3.coordinate.task.unsuited", "{0}, but this task does not suit them"),
            A3T("phase2.a3.coordinate.task.cycle", "Next person"),
            A3T("phase2.a3.coordinate.assign.commit", "Commit this assignment"),
            A3T("phase2.a3.coordinate.assign.incomplete", "Give every task to someone first."),
            A3T("phase2.a3.coordinate.assign.done", "The work is assigned."),
            A3T("phase2.a3.coordinate.exchange.talk", "Talk to {0}"),
            A3T("phase2.a3.coordinate.exchange.heard", "You have already spoken to {0}."),
            A3T("phase2.a3.coordinate.exchange.done", "Spoke to {0}."),
            A3T("phase2.a3.coordinate.exchange.finish", "Finish the exchange"),
            A3T("phase2.a3.coordinate.exchange.finish_hint", "Moves on to settling. Finishing costs nothing."),
            A3T("phase2.a3.coordinate.facts_heading", "What you know"),
            A3T("phase2.a3.coordinate.fact.line", "{0}: {1}"),
            A3T("phase2.a3.coordinate.facts_none", "Nobody has told you anything yet."),
            A3T("phase2.a3.coordinate.resolution.leave", "Leave it as it is"),
            A3T("phase2.a3.coordinate.resolution.rebalance", "Rebalance the work"),
            A3T("phase2.a3.coordinate.resolution.reschedule", "Reschedule"),
            A3T("phase2.a3.coordinate.resolution.trim_scope", "Trim the scope"),
            A3T("phase2.a3.coordinate.resolution.ask_extension", "Ask for an extension"),
            A3T("phase2.a3.coordinate.resolution.leave.hint", "Costs nothing, and settles nothing."),
            A3T("phase2.a3.coordinate.resolution.rebalance.hint", "Move load between people."),
            A3T("phase2.a3.coordinate.resolution.reschedule.hint", "Move who does what, and when."),
            A3T("phase2.a3.coordinate.resolution.trim_scope.hint", "Cut what the group promised to do."),
            A3T("phase2.a3.coordinate.resolution.ask_extension.hint", "Ask the teacher for more time."),
            A3T("phase2.a3.coordinate.settle.done", "You chose: {0}."),
            A3T("phase2.a3.coordinate.submit.check", "Check the hand-over, then submit"),
            A3T("phase2.a3.coordinate.submit.plain", "Submit as it stands"),
            A3T("phase2.a3.coordinate.submit.check_hint", "Looks for places where the parts do not join."),
            A3T("phase2.a3.coordinate.submit.plain_hint", "Costs nothing. Anything that does not join stays unfound."),
            A3T("phase2.a3.coordinate.submit.done", "The group plan is handed in."),
            A3T("phase2.a3.coordinate.outcome.counts", "Overloaded people: {0}. Tasks with the wrong person: {1}. Key facts never heard: {2}. Wasted conversations: {3}."),
            A3T("phase2.a3.coordinate.fail.overloaded", "Someone was given more work than they can carry."),
            A3T("phase2.a3.coordinate.fail.mismatched", "A task went to someone it does not suit."),
            A3T("phase2.a3.coordinate.fail.uninformed", "A classmate held a key fact that you never heard."),
            A3T("phase2.a3.coordinate.fail.unsettled", "What binds the group was left, or settled the wrong way."),
            A3T("phase2.a3.coordinate.fail.seam", "The parts did not join, and the hand-over was not checked."),
            A3T("phase2.a3.coordinate.fail.wasted", "Effort went on a conversation or a check that found nothing."),
        };
        TMap<FString, FText> Built;
        for (const FA3IEntry& Entry : Entries)
        {
            Built.Add(FString(Entry.Key), Entry.Text);
        }
        return Built;
    }();
    return Map;
}
#undef A3T

constexpr int32 A3IMinutesPerUnit = 5;

PerfectWorld::SchoolIssue A3IIssue(const char* Code)
{
    return PerfectWorld::SchoolIssue{Code, {}};
}

bool A3IModeAvailable(const PerfectWorld::SchoolPeriodView& Period, const PerfectWorld::SchoolLessonMode Mode)
{
    switch (Mode)
    {
    case PerfectWorld::SchoolLessonMode::Foreground: return Period.ForegroundAvailable;
    case PerfectWorld::SchoolLessonMode::Routine: return Period.RoutineAvailable;
    case PerfectWorld::SchoolLessonMode::Attend: return Period.AttendAvailable;
    }
    return false;
}

const PerfectWorld::StructureOutcome* A3IBaseOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome)
{
    if (const auto* Plain = std::get_if<PerfectWorld::StructureOutcome>(&Outcome)) return Plain;
    if (const auto* Investigate = std::get_if<PerfectWorld::InvestigateOutcome>(&Outcome)) return Investigate;
    if (const auto* Coordinate = std::get_if<PerfectWorld::CoordinateOutcome>(&Outcome)) return Coordinate;
    return nullptr;
}
} // namespace

// ================================================================================ words, layout

namespace PWClassA3
{
bool Compact(const float Width, const float Height)
{
    return Height < 760.0f || Width < 760.0f;
}

float PanelWidth(const float Width, const float)
{
    return FMath::Clamp(Width - 32.0f, 280.0f, 1100.0f);
}

float PanelMaxHeight(const float, const float Height)
{
    return FMath::Max(120.0f, Height - 32.0f);
}

FText Str(const TCHAR* Key)
{
    return Str(FString(Key));
}

FText Str(const FString& Key)
{
    const FText* Found = A3IStrings().Find(Key);
    return Found != nullptr ? *Found : FText::GetEmpty();
}

FString FromUtf8(const std::string& Text)
{
    return FString(UTF8_TO_TCHAR(Text.c_str()));
}

FText TextFromUtf8(const std::string& Text)
{
    return FText::FromString(FromUtf8(Text));
}

FText ClockText(const int32 MinuteOfDay)
{
    return FText::FromString(FString::Printf(TEXT("%02d:%02d"), MinuteOfDay / 60, MinuteOfDay % 60));
}

FText IssueText(const PerfectWorld::SchoolIssue& Issue)
{
    const FText Found = Str(TEXT("phase2.a3.issue.") + FromUtf8(Issue.Code));
    return Found.IsEmpty() ? Str(TEXT("phase2.a3.issue.unknown")) : Found;
}

FText ModeText(const PerfectWorld::SchoolLessonMode Mode)
{
    switch (Mode)
    {
    case PerfectWorld::SchoolLessonMode::Foreground: return Str(TEXT("phase2.a3.common.mode.foreground"));
    case PerfectWorld::SchoolLessonMode::Routine: return Str(TEXT("phase2.a3.common.mode.routine"));
    case PerfectWorld::SchoolLessonMode::Attend: break;
    }
    return Str(TEXT("phase2.a3.common.mode.attend"));
}

FText ModeHint(const PerfectWorld::SchoolLessonMode Mode)
{
    switch (Mode)
    {
    case PerfectWorld::SchoolLessonMode::Foreground: return Str(TEXT("phase2.a3.common.mode.foreground.hint"));
    case PerfectWorld::SchoolLessonMode::Routine: return Str(TEXT("phase2.a3.common.mode.routine.hint"));
    case PerfectWorld::SchoolLessonMode::Attend: break;
    }
    return Str(TEXT("phase2.a3.common.mode.attend.hint"));
}

FText GradeText(const PerfectWorld::StructureGrade Grade)
{
    switch (Grade)
    {
    case PerfectWorld::StructureGrade::Sound: return Str(TEXT("phase2.a3.common.grade.sound"));
    case PerfectWorld::StructureGrade::Partial: return Str(TEXT("phase2.a3.common.grade.partial"));
    case PerfectWorld::StructureGrade::Unsound: break;
    }
    return Str(TEXT("phase2.a3.common.grade.unsound"));
}

FText CostText(const int32 Units)
{
    if (Units <= 0) return Str(TEXT("phase2.a3.common.cost.free"));
    if (Units == 1) return Str(TEXT("phase2.a3.common.cost.one"));
    return FText::Format(Str(TEXT("phase2.a3.common.cost.many")), FText::AsNumber(Units));
}

FText UnitsText(const int32 Left, const int32 Total)
{
    return FText::Format(Str(TEXT("phase2.a3.common.units")), FText::AsNumber(Left), FText::AsNumber(Total));
}

// ================================================================================ the core bridge

FCoreLessonBridge::FCoreLessonBridge(PerfectWorld::SchoolSessionsState& InSessions, PerfectWorld::DayLoopState& InDay,
                                     const PerfectWorld::SchoolSessionCatalogues& InCatalogues, PerfectWorld::SchoolLessonBinding InBinding,
                                     const int32 InSkillMilli, const uint64 InWorldSeed,
                                     std::vector<PerfectWorld::ReceiptCandidate> InAlternatives)
    : Sessions(InSessions)
    , Day(InDay)
    , Catalogues(InCatalogues)
    , Binding(std::move(InBinding))
    , SkillMilli(InSkillMilli)
    , WorldSeed(InWorldSeed)
    , Alternatives(std::move(InAlternatives))
{
}

PerfectWorld::SchoolPeriodView FCoreLessonBridge::QueryPeriod() const
{
    return PerfectWorld::QuerySchoolPeriod(Sessions, Day, Binding, Catalogues);
}

PerfectWorld::SchoolLessonView FCoreLessonBridge::QueryLesson() const
{
    return PerfectWorld::QuerySchoolLesson(Sessions);
}

PerfectWorld::SchoolSessionResult FCoreLessonBridge::OpenLesson(const PerfectWorld::SchoolLessonMode Mode, const PerfectWorld::SchoolLessonGoal& Goal)
{
    return PerfectWorld::OpenSchoolLesson(Sessions, Day, Catalogues, Binding, Mode, Goal, SkillMilli, WorldSeed, Alternatives);
}

PerfectWorld::SchoolLessonQuote FCoreLessonBridge::QuoteAction(const uint64 Session, const PerfectWorld::SchoolLessonAction& Action) const
{
    return PerfectWorld::QuoteSchoolLessonAction(Sessions, Catalogues, Session, Action);
}

PerfectWorld::SchoolSessionResult FCoreLessonBridge::ChooseAction(const uint64 Session, const PerfectWorld::SchoolLessonAction& Action)
{
    return PerfectWorld::ChooseSchoolLesson(Sessions, Catalogues, Session, Action);
}

PerfectWorld::SchoolSessionResult FCoreLessonBridge::CloseLesson(const uint64 Session, const bool bCancel, const int32 ElapsedMinutes)
{
    return PerfectWorld::CloseSchoolLesson(Sessions, Day, Catalogues, Session, bCancel, ElapsedMinutes);
}

// ================================================================================ the frame

FFrame BuildFrame(const ILessonBridge& Bridge)
{
    FFrame Out;
    Out.Binding = Bridge.GetBinding();
    Out.Period = Bridge.QueryPeriod();
    Out.View = Bridge.QueryLesson();
    if (Out.Period.EndMinute > Out.Period.StartMinute)
    {
        Out.PeriodMinutes = Out.Period.EndMinute - Out.Period.StartMinute;
    }
    Out.Title = Bridge.ResolveText(FromUtf8(Out.Binding.TitleTextKey));
    Out.Brief = Bridge.ResolveText(FromUtf8(Out.Binding.BriefTextKey));
    Out.Role = Bridge.ResolveText(FromUtf8(Out.Binding.RoleTextKey));
    if (Out.View.Run.has_value())
    {
        const PerfectWorld::SchoolLessonRun& Run = *Out.View.Run;
        Out.bHasRun = true;
        Out.bOtherLesson = !(Run.Binding == Out.Binding);
        Out.bReadyToClose = Run.ReadyToClose;
        Out.Session = Run.Session;
        Out.RunMode = Run.Mode;
        Out.Outcome = Run.Outcome;
    }
    return Out;
}
} // namespace PWClassA3

// ================================================================================ the shell

namespace
{
bool A3IIsBack(const FKey& Key)
{
    return Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
}
} // namespace

FVector2D SPWClassLessonShell::GetSpace() const
{
    return Space.Get(FVector2D(1920.0, 1080.0));
}

bool SPWClassLessonShell::IsCompactNow() const
{
    const FVector2D S = GetSpace();
    return PWClassA3::Compact(static_cast<float>(S.X), static_cast<float>(S.Y));
}

void SPWClassLessonShell::ConstructShell(const PWClassA3::FShellArgs& Args)
{
    Bridge = Args.Bridge;
    Space = Args.Space;
    OnClose = Args.OnClose;
    OnLessonClosed = Args.OnLessonClosed;
    ReturnFocus = Args.ReturnFocus;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();

    BodyBox = SNew(SVerticalBox);

    const TSharedRef<SWidget> Back = PWUI::MakeButton(
        PWClassA3::Str(TEXT("phase2.a3.common.back")), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            RequestClose();
            return FReply::Handled();
        }), true, 20, 0.0f);
    BackButton = Back;

    const TSharedRef<SWidget> Leave = PWUI::MakeButton(
        PWClassA3::Str(TEXT("phase2.a3.common.leave")), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            ExecuteClose(true);
            return FReply::Handled();
        }),
        TAttribute<bool>::CreateSPLambda(this, [this]() { return CanLeaveEarly(); }), 20, 0.0f);

    const TSharedRef<SWidget> Header =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return Frame.Title.IsEmpty() ? PWClassA3::Str(TEXT("phase2.a3.common.lesson")) : Frame.Title; })
            .Font_Lambda([this]() { return PWUI::Font(IsCompactNow() ? 22 : 30, true); })
            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
            .AutoWrapText(true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(ShellSubtitle(), PWUI::ETextRole::Meta, Style.TextSecondary, true)
        ];

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride_Lambda([this]()
        {
            const FVector2D S = GetSpace();
            return FOptionalSize(PWClassA3::PanelWidth(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        .MaxDesiredHeight_Lambda([this]()
        {
            const FVector2D S = GetSpace();
            return FOptionalSize(PWClassA3::PanelMaxHeight(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        [
            SNew(SBorder)
            .Tag(FName(PWClassA3::PanelTag))
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
                    .Padding_Lambda([this]() { return IsCompactNow() ? FMargin(20.0f, 10.0f) : FMargin(40.0f, 24.0f); })
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
                            SAssignNew(BodyScroll, SScrollBox)
                            .ScrollBarStyle(&Style.ScrollBar)
                            .AllowOverscroll(EAllowOverscroll::No)
                            + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
                            [
                                BodyBox.ToSharedRef()
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SBorder)
                            .Tag(FName(PWClassA3::BannerTag))
                            .BorderImage(&Style.BrushInset)
                            .Padding(FMargin(12.0f, 8.0f))
                            .Visibility_Lambda([this]() { return Banner.IsEmpty() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
                            [
                                SNew(STextBlock)
                                .Text_Lambda([this]() { return Banner; })
                                .Font(PWUI::Font(16, PWUI::EFontWeight::Medium))
                                .ColorAndOpacity_Lambda([this]() { return FSlateColor(bBannerError ? PWUI::FStyle::Get().Danger : PWUI::FStyle::Get().TextPrimary); })
                                .AutoWrapText(true)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox)
                            .Visibility_Lambda([this]() { return IsCompactNow() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                            [
                                PWUI::MakeText(PWClassA3::Str(TEXT("phase2.a3.common.hints")), PWUI::ETextRole::Meta, Style.TextSecondary, true)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
                            [
                                SNew(SBox)
                                .WidthOverride(220.0f)
                                .Visibility_Lambda([this]() { return CanLeaveEarly() ? EVisibility::Visible : EVisibility::Collapsed; })
                                [
                                    Leave
                                ]
                            ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(220.0f)[Back]
                            ]
                        ]
                    ]
                ]
            ]
        ]
    ];

    RefreshModel();
}

bool SPWClassLessonShell::CanLeaveEarly() const
{
    return Bridge.IsValid() && Frame.bHasRun && !Frame.bOtherLesson && !bLessonClosed
        && !(Frame.RunMode == PerfectWorld::SchoolLessonMode::Foreground && Frame.bReadyToClose);
}

int32 SPWClassLessonShell::LeaveMinutes() const
{
    const int32 Spent = Frame.bExposureActive ? FMath::Max(0, Frame.UnitsTotal - Frame.UnitsLeft) : 0;
    return FMath::Clamp(Spent * A3IMinutesPerUnit, 0, FMath::Max(0, Frame.PeriodMinutes - 1));
}

void SPWClassLessonShell::ClampSelections()
{
    GoalIndex = FMath::Clamp(GoalIndex, 0, FMath::Max(0, ShellGoalCount() - 1));
    if (Frame.bHasRun || A3IModeAvailable(Frame.Period, Mode))
    {
        return;
    }
    for (const PerfectWorld::SchoolLessonMode Candidate : {PerfectWorld::SchoolLessonMode::Foreground, PerfectWorld::SchoolLessonMode::Routine,
                                                           PerfectWorld::SchoolLessonMode::Attend})
    {
        if (A3IModeAvailable(Frame.Period, Candidate))
        {
            Mode = Candidate;
            return;
        }
    }
}

int32 SPWClassLessonShell::FocusedActionIndex() const
{
    for (int32 Index = 0; Index < Actions.Num(); ++Index)
    {
        const TSharedPtr<SWidget>& Widget = Actions[Index].Widget;
        if (Widget.IsValid() && (Widget->HasKeyboardFocus() || Widget->HasFocusedDescendants()))
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

void SPWClassLessonShell::RefreshModel()
{
    const bool bHadFocus = bWantFocus || HasFocusedDescendants();
    const int32 PreviousIndex = FocusedActionIndex();
    if (Bridge.IsValid())
    {
        Frame = PWClassA3::BuildFrame(*Bridge);
        bool bActive = false;
        int32 Left = 0;
        int32 Total = 0;
        ShellReadExposure(Frame.View, bActive, Left, Total);
        Frame.bExposureActive = bActive && Frame.bHasRun && !Frame.bOtherLesson;
        Frame.UnitsLeft = Frame.bExposureActive ? Left : 0;
        Frame.UnitsTotal = Frame.bExposureActive ? Total : 0;
    }
    else
    {
        Frame = PWClassA3::FFrame();
    }
    ClampSelections();
    ShellFrameChanged();
    RebuildBody();
    RestoreFocusAfterRebuild(PreviousIndex, bHadFocus);
}

void SPWClassLessonShell::RestoreFocusAfterRebuild(const int32 PreviousIndex, const bool bHadFocus)
{
    if (!bHadFocus)
    {
        return;
    }
    bWantFocus = false;
    // The nearest enabled action at or after the old position, else the nearest before it, else Back.
    TWeakPtr<SWidget> Target = BackButton;
    const int32 Start = FMath::Clamp(PreviousIndex == INDEX_NONE ? 0 : PreviousIndex, 0, FMath::Max(0, Actions.Num() - 1));
    bool bFound = false;
    for (int32 Index = Start; Index < Actions.Num() && !bFound; ++Index)
    {
        if (Actions[Index].bEnabled) { Target = Actions[Index].Widget; bFound = true; }
    }
    for (int32 Index = Start - 1; Index >= 0 && !bFound; --Index)
    {
        if (Actions[Index].bEnabled) { Target = Actions[Index].Widget; bFound = true; }
    }
    PWUI::FocusWhenReady(AsShared(), Target);
}

void SPWClassLessonShell::FocusInitial()
{
    bWantFocus = true;
    RestoreFocusAfterRebuild(INDEX_NONE, true);
}

void SPWClassLessonShell::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    // A resize or a text-scale change crossing the compact line rebuilds the body at the other density.
    if (IsCompactNow() != bBuiltCompact)
    {
        RefreshModel();
    }
}

void SPWClassLessonShell::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
    SCompoundWidget::OnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);
    if (!BodyScroll.IsValid())
    {
        return;
    }
    for (int32 PathIndex = NewWidgetPath.Widgets.Num() - 1; PathIndex >= 0; --PathIndex)
    {
        const TSharedRef<SWidget>& Widget = NewWidgetPath.Widgets[PathIndex].Widget;
        for (const FActionSlot& Slot : Actions)
        {
            if (Slot.Widget.Get() == &Widget.Get())
            {
                BodyScroll->ScrollDescendantIntoView(Slot.Widget, false);
                return;
            }
        }
    }
}

FReply SPWClassLessonShell::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (A3IIsBack(Key))
    {
        // A held key auto-repeats; this screen goes back on the first press only.
        if (!InKeyEvent.IsRepeat())
        {
            RequestClose();
        }
        return FReply::Handled();
    }
    const bool bIdle = !Frame.bHasRun && !bLessonClosed;
    if (bIdle && (Key == EKeys::G || Key == EKeys::Gamepad_RightShoulder))
    {
        if (!InKeyEvent.IsRepeat())
        {
            CycleGoal(1);
        }
        return FReply::Handled();
    }
    if (bIdle && (Key == EKeys::M || Key == EKeys::Gamepad_LeftShoulder))
    {
        if (!InKeyEvent.IsRepeat())
        {
            CycleMode(1);
        }
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SPWClassLessonShell::RequestClose()
{
    // The host may drop its last reference to this screen from inside OnClose.
    const TSharedRef<SWidget> KeepAlive = AsShared();
    const TWeakPtr<SWidget> Return = ReturnFocus;
    OnClose.ExecuteIfBound();
    const TSharedPtr<SWidget> Target = Return.Pin();
    if (Target.IsValid() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetKeyboardFocus(Target, EFocusCause::SetDirectly);
    }
}

void SPWClassLessonShell::ShowBanner(const FText& Text, const bool bError)
{
    Banner = Text;
    bBannerError = bError;
}

bool SPWClassLessonShell::SetMode(const PerfectWorld::SchoolLessonMode InMode)
{
    if (Frame.bHasRun || bLessonClosed || InMode == Mode || !A3IModeAvailable(Frame.Period, InMode))
    {
        return false;
    }
    Mode = InMode;
    RefreshModel();
    return true;
}

bool SPWClassLessonShell::SetGoalIndex(const int32 Index)
{
    if (Frame.bHasRun || bLessonClosed || Index == GoalIndex || Index < 0 || Index >= ShellGoalCount())
    {
        return false;
    }
    GoalIndex = Index;
    RefreshModel();
    return true;
}

void SPWClassLessonShell::CycleMode(const int32 Delta)
{
    const PerfectWorld::SchoolLessonMode Order[3] = {PerfectWorld::SchoolLessonMode::Foreground, PerfectWorld::SchoolLessonMode::Routine,
                                                     PerfectWorld::SchoolLessonMode::Attend};
    int32 Position = 0;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (Order[Index] == Mode) Position = Index;
    }
    for (int32 Step = 1; Step <= 3; ++Step)
    {
        const int32 Next = ((Position + Delta * Step) % 3 + 3) % 3;
        if (SetMode(Order[Next]) || Order[Next] == Mode)
        {
            return;
        }
    }
}

void SPWClassLessonShell::CycleGoal(const int32 Delta)
{
    const int32 Count = ShellGoalCount();
    if (Count > 0 && Mode == PerfectWorld::SchoolLessonMode::Foreground)
    {
        SetGoalIndex(((GoalIndex + Delta) % Count + Count) % Count);
    }
}

PerfectWorld::SchoolSessionResult SPWClassLessonShell::ExecuteOpen()
{
    PerfectWorld::SchoolSessionResult Result;
    if (!Bridge.IsValid())
    {
        Result.Issue = A3IIssue("school.session.missing_catalogue");
        ShowBanner(PWClassA3::IssueText(Result.Issue), true);
        return Result;
    }
    Result = Bridge->OpenLesson(Mode, CurrentGoal());
    if (Result.Ok()) ShowBanner(PWClassA3::Str(TEXT("phase2.a3.common.opened")), false);
    else ShowBanner(PWClassA3::IssueText(Result.Issue), true);
    RefreshModel();
    return Result;
}

PerfectWorld::SchoolSessionResult SPWClassLessonShell::ExecuteClose(const bool bCancel)
{
    const TSharedRef<SWidget> KeepAlive = AsShared();
    PerfectWorld::SchoolSessionResult Result;
    if (!Bridge.IsValid() || !Frame.bHasRun || Frame.bOtherLesson || bLessonClosed)
    {
        Result.Issue = A3IIssue("school.session.stale_session");
        ShowBanner(PWClassA3::IssueText(Result.Issue), true);
        return Result;
    }
    Result = Bridge->CloseLesson(Frame.Session, bCancel, bCancel ? LeaveMinutes() : 0);
    if (!Result.Ok())
    {
        ShowBanner(PWClassA3::IssueText(Result.Issue), true);
        RefreshModel();
        return Result;
    }
    bLessonClosed = true;
    bClosedByLeaving = bCancel;
    ShowBanner(PWClassA3::Str(bCancel ? TEXT("phase2.a3.common.left") : TEXT("phase2.a3.common.recorded")), false);
    RefreshModel();
    OnLessonClosed.ExecuteIfBound(bCancel);
    return Result;
}

PerfectWorld::SchoolSessionResult SPWClassLessonShell::ChooseAction(const PerfectWorld::SchoolLessonAction& Action, const FText& SuccessLine)
{
    PerfectWorld::SchoolSessionResult Result;
    if (!Bridge.IsValid())
    {
        Result.Issue = A3IIssue("school.session.missing_catalogue");
    }
    else
    {
        Result = Bridge->ChooseAction(Frame.Session, Action);
    }
    if (!Result.Ok()) ShowBanner(PWClassA3::IssueText(Result.Issue), true);
    else if (Result.ReadyToClose) ShowBanner(PWClassA3::Str(TEXT("phase2.a3.common.exercise_done")), false);
    else ShowBanner(SuccessLine, false);
    RefreshModel();
    return Result;
}

// ---- Actions on screen ----

TArray<FString> SPWClassLessonShell::GetActionKeys() const
{
    TArray<FString> Keys;
    for (const FActionSlot& Slot : Actions) Keys.Add(Slot.Key);
    return Keys;
}

bool SPWClassLessonShell::HasAction(const FString& Key) const
{
    return Actions.ContainsByPredicate([&Key](const FActionSlot& Slot) { return Slot.Key == Key; });
}

bool SPWClassLessonShell::IsActionEnabled(const FString& Key) const
{
    const FActionSlot* Found = Actions.FindByPredicate([&Key](const FActionSlot& Slot) { return Slot.Key == Key; });
    return Found != nullptr && Found->bEnabled;
}

bool SPWClassLessonShell::PressAction(const FString& Key)
{
    const FActionSlot* Found = Actions.FindByPredicate([&Key](const FActionSlot& Slot) { return Slot.Key == Key; });
    if (Found == nullptr || !Found->bEnabled || !Found->Handler)
    {
        return false;
    }
    // The handler rebuilds the rows, which clears Actions: run a copy.
    const TFunction<void()> Handler = Found->Handler;
    Handler();
    return true;
}

void SPWClassLessonShell::AddActionRow(const TSharedRef<SVerticalBox>& Into, const FString& Key, const FText& Title, const FText& Detail,
                                       const FText& Cost, const ECostTone Tone, const bool bEnabled, TFunction<void()> Handler,
                                       const bool bSelected)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FLinearColor CostColour = Tone == ECostTone::Free ? Style.Good : (Tone == ECostTone::Short ? Style.Danger : Style.TextSecondary);
    const TSharedRef<SVerticalBox> Words = SNew(SVerticalBox);
    Words->AddSlot().AutoHeight()
    [
        PWUI::MakeText(Title, TextSize(18, 16), Style.TextPrimary, true, true)
    ];
    if (!Detail.IsEmpty())
    {
        Words->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Detail, PWUI::ETextRole::Meta, Style.TextSecondary, true)
        ];
    }
    const TSharedRef<SWidget> Content =
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            Words
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Cost, PWUI::ETextRole::Control, CostColour, true)
        ];
    const TSharedRef<SWidget> Button = PWUI::MakeContentButton(
        Content, PWUI::EButtonKind::Card,
        FOnClicked::CreateSPLambda(this, [this, Key]()
        {
            PressAction(Key);
            return FReply::Handled();
        }),
        bSelected, bEnabled, FMargin(14.0f, 10.0f));
    Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
    [
        SNew(SBox).Tag(FName(PWClassA3::RowTag))[Button]
    ];
    FActionSlot Slot;
    Slot.Key = Key;
    Slot.Widget = Button;
    Slot.bEnabled = bEnabled;
    Slot.Handler = MoveTemp(Handler);
    Actions.Add(MoveTemp(Slot));
}

void SPWClassLessonShell::AddPrimaryAction(const TSharedRef<SVerticalBox>& Into, const FString& Key, const FText& Label, TFunction<void()> Handler)
{
    const TSharedRef<SWidget> Button = PWUI::MakeButton(
        Label, PWUI::EButtonKind::Primary,
        FOnClicked::CreateSPLambda(this, [this, Key]()
        {
            PressAction(Key);
            return FReply::Handled();
        }), true, 20, 0.0f);
    Into->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 6.0f)
    [
        Button
    ];
    FActionSlot Slot;
    Slot.Key = Key;
    Slot.Widget = Button;
    Slot.bEnabled = true;
    Slot.Handler = MoveTemp(Handler);
    Actions.Add(MoveTemp(Slot));
}

void SPWClassLessonShell::AddHeading(const TSharedRef<SVerticalBox>& Into, const FText& Text)
{
    Into->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 6.0f)
    [
        PWUI::MakeText(Text, PWUI::ETextRole::SectionHeading, PWUI::FStyle::Get().Accent, true)
    ];
}

void SPWClassLessonShell::AddNote(const TSharedRef<SVerticalBox>& Into, const FText& Text, const bool bMuted)
{
    if (Text.IsEmpty())
    {
        return;
    }
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
    [
        SNew(SBorder)
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(12.0f, 8.0f))
        [
            PWUI::MakeText(Text, TextSize(17, 15), bMuted ? Style.TextSecondary : Style.TextPrimary, false, true)
        ]
    ];
}

void SPWClassLessonShell::AddUnitsMeter(const TSharedRef<SVerticalBox>& Into)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SHorizontalBox> Pips = SNew(SHorizontalBox);
    for (int32 Pip = 0; Pip < 9; ++Pip)
    {
        Pips->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f).VAlign(VAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(22.0f)
            .HeightOverride(8.0f)
            .Visibility_Lambda([this, Pip]() { return Pip < Frame.UnitsTotal ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            [
                SNew(SImage)
                .Image_Lambda([this, Pip]() -> const FSlateBrush*
                {
                    return Pip < Frame.UnitsLeft ? &PWUI::FStyle::Get().BrushPipOn : &PWUI::FStyle::Get().BrushPipOff;
                })
            ]
        ];
    }
    Into->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 6.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 14.0f, 0.0f)
        [
            Pips
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            PWUI::MakeText(TAttribute<FText>::CreateSPLambda(this, [this]() { return PWClassA3::UnitsText(Frame.UnitsLeft, Frame.UnitsTotal); }),
                           17, Style.TextPrimary, true, true)
        ]
    ];
}

// ---- The body ----

void SPWClassLessonShell::RebuildBody()
{
    Actions.Reset();
    BodyBox->ClearChildren();
    bBuiltCompact = IsCompactNow();
    const TSharedRef<SVerticalBox> Into = BodyBox.ToSharedRef();
    if (!Bridge.IsValid())
    {
        AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.unavailable")), false);
        return;
    }
    BuildHeader(Into);
    if (bLessonClosed)
    {
        AddNote(Into, PWClassA3::Str(bClosedByLeaving ? TEXT("phase2.a3.common.left") : TEXT("phase2.a3.common.recorded")), false);
        AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.closed_note")));
        return;
    }
    if (Frame.bOtherLesson)
    {
        AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.other_lesson")), false);
    }
    else if (!Frame.bHasRun)
    {
        BuildIdle(Into);
    }
    else if (Frame.bReadyToClose)
    {
        BuildFinished(Into);
    }
    else
    {
        ShellBuildPlay(Into);
    }
}

void SPWClassLessonShell::BuildHeader(const TSharedRef<SVerticalBox>& Into)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const PerfectWorld::SchoolPeriodView& Period = Frame.Period;
    if (Period.Period >= 0 && Period.EndMinute > Period.StartMinute)
    {
        FText Line = FText::Format(PWClassA3::Str(TEXT("phase2.a3.common.period_line")), FText::AsNumber(Period.Period + 1),
                                   PWClassA3::ClockText(Period.StartMinute), PWClassA3::ClockText(Period.EndMinute));
        if (!Period.Room.empty())
        {
            Line = FText::Format(INVTEXT("{0}  -  {1}"), Line,
                                 FText::Format(PWClassA3::Str(TEXT("phase2.a3.common.room_line")), PWClassA3::TextFromUtf8(Period.Room)));
        }
        Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            PWUI::MakeText(Line, PWUI::ETextRole::Meta, Style.Accent, true)
        ];
    }
    if (!Frame.Role.IsEmpty())
    {
        Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            PWUI::MakeText(FText::Format(PWClassA3::Str(TEXT("phase2.a3.common.role_line")), Frame.Role), PWUI::ETextRole::Meta, Style.TextSecondary, true)
        ];
    }
    AddNote(Into, Frame.Brief);
}

void SPWClassLessonShell::BuildIdle(const TSharedRef<SVerticalBox>& Into)
{
    if (!Frame.Period.AttendAvailable)
    {
        AddNote(Into, PWClassA3::IssueText(Frame.Period.Issue), false);
        return;
    }
    AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.common.mode_heading")));
    const struct { PerfectWorld::SchoolLessonMode Mode; const TCHAR* Key; const TCHAR* Unavailable; } Modes[] = {
        {PerfectWorld::SchoolLessonMode::Foreground, TEXT("mode.foreground"), TEXT("phase2.a3.common.mode.unavailable.foreground")},
        {PerfectWorld::SchoolLessonMode::Routine, TEXT("mode.routine"), TEXT("phase2.a3.common.mode.unavailable.routine")},
        {PerfectWorld::SchoolLessonMode::Attend, TEXT("mode.attend"), TEXT("")}};
    for (const auto& Entry : Modes)
    {
        const bool bAvailable = A3IModeAvailable(Frame.Period, Entry.Mode);
        const PerfectWorld::SchoolLessonMode Picked = Entry.Mode;
        AddActionRow(Into, Entry.Key, PWClassA3::ModeText(Entry.Mode), bAvailable ? PWClassA3::ModeHint(Entry.Mode) : PWClassA3::Str(Entry.Unavailable),
                     FText::GetEmpty(), ECostTone::Plain, bAvailable, [this, Picked]() { SetMode(Picked); }, Mode == Entry.Mode);
    }
    if (Mode == PerfectWorld::SchoolLessonMode::Foreground)
    {
        AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.common.goal_heading")));
        for (int32 Index = 0; Index < ShellGoalCount(); ++Index)
        {
            AddActionRow(Into, FString::Printf(TEXT("goal.%d"), Index), ShellGoalName(Index), ShellGoalHint(Index), FText::GetEmpty(),
                         ECostTone::Plain, true, [this, Index]() { SetGoalIndex(Index); }, GoalIndex == Index);
        }
        AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.quotes_note")));
    }
    AddPrimaryAction(Into, TEXT("open"), PWClassA3::Str(TEXT("phase2.a3.common.start")), [this]() { ExecuteOpen(); });
}

void SPWClassLessonShell::BuildFinished(const TSharedRef<SVerticalBox>& Into)
{
    AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.common.outcome_heading")));
    if (Frame.RunMode == PerfectWorld::SchoolLessonMode::Foreground)
    {
        if (const PerfectWorld::StructureOutcome* Base = A3IBaseOutcome(Frame.Outcome))
        {
            AddNote(Into, FText::Format(PWClassA3::Str(TEXT("phase2.a3.common.outcome.grade")), PWClassA3::GradeText(Base->Grade)), false);
            AddNote(Into, FText::Format(PWClassA3::Str(TEXT("phase2.a3.common.outcome.units")), FText::AsNumber(Base->UnitsSpent)));
            for (const FText& Line : ShellOutcomeDetail(Frame.Outcome))
            {
                AddNote(Into, Line);
            }
            if (Base->Failures == 0)
            {
                AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.outcome.no_faults")));
            }
            if (Base->BecameSecure)
            {
                AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.outcome.secure")), false);
            }
        }
        else
        {
            AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.outcome.pending")));
        }
    }
    else if (Frame.RunMode == PerfectWorld::SchoolLessonMode::Routine)
    {
        AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.outcome.routine")));
    }
    else
    {
        AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.outcome.attend")));
    }
    AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.record_hint")));
    AddPrimaryAction(Into, TEXT("record"), PWClassA3::Str(TEXT("phase2.a3.common.record")), [this]() { ExecuteClose(false); });
}

// ================================================================================ the Investigate lesson

namespace
{
const TCHAR* const A3IKindKeys[] = {TEXT("observe"), TEXT("measure"), TEXT("swap")};
const TCHAR* const A3IGoalKeys[] = {TEXT("inspect"), TEXT("measure"), TEXT("swap_parts")};
const PerfectWorld::InvestigateGoal A3IGoals[] = {PerfectWorld::InvestigateGoal::Inspect, PerfectWorld::InvestigateGoal::Measure,
                                                  PerfectWorld::InvestigateGoal::SwapParts};

FString A3IWord(const std::string_view Name)
{
    return FString(static_cast<int32>(Name.size()), Name.data());
}

bool A3IHas(const std::vector<std::string>& Ids, const std::string& Id)
{
    return std::find(Ids.begin(), Ids.end(), Id) != Ids.end();
}

FText A3ICauseName(const PerfectWorld::InvestigateCaseDef& Def, const std::string& Id)
{
    for (const PerfectWorld::InvestigateCause& Cause : Def.Causes)
    {
        if (Cause.Id == Id) return PWClassA3::TextFromUtf8(Cause.Name);
    }
    return FText::GetEmpty();
}

FText A3IJoin(const TArray<FText>& Parts)
{
    return FText::Join(INVTEXT(", "), Parts);
}
} // namespace

namespace PWClassInvestigate
{
FModel BuildModel(const PWClassA3::ILessonBridge& Bridge, const PWClassA3::FFrame& Frame)
{
    using namespace PerfectWorld;
    FModel Out;
    const InvestigateCatalogue* Catalogue = Bridge.GetCatalogues().Structures.Investigate;
    const InvestigateExposure& Exposure = Frame.View.Investigate.Exposure;
    if (Catalogue == nullptr || !Frame.bHasRun || Frame.bOtherLesson || Frame.bReadyToClose || !Exposure.Active())
    {
        return Out;
    }
    const InvestigateCaseDef* Def = Catalogue->FindCase(Exposure.CaseId);
    if (Def == nullptr)
    {
        return Out;
    }
    Out.bActive = true;
    Out.CaseId = PWClassA3::FromUtf8(Def->Id);
    Out.CaseName = PWClassA3::TextFromUtf8(Def->Name);
    Out.CaseBrief = PWClassA3::TextFromUtf8(Def->Brief);
    Out.UnitsLeft = Exposure.UnitsLeft;
    Out.UnitsTotal = Exposure.UnitsTotal;
    Out.Symptom = PWClassA3::Str(TEXT("phase2.a3.investigate.symptom.") + A3IWord(InvestigateSymptomNames[static_cast<size_t>(Def->Symptom)]));
    Out.Role = PWClassA3::Str(TEXT("phase2.a3.investigate.role.") + A3IWord(InvestigateRoleNames[static_cast<size_t>(Def->Role)]));
    Out.Scarce = PWClassA3::Str(TEXT("phase2.a3.investigate.scarce.") + A3IWord(InvestigateScarceNames[static_cast<size_t>(Def->Scarce)]));
    Out.Goal = PWClassA3::Str(TEXT("phase2.a3.investigate.goal.") + A3IWord(InvestigateGoalNames[static_cast<size_t>(Exposure.Goal)]));
    FText MethodName = PWClassA3::TextFromUtf8(Def->MethodId);
    if (const auto* Family = Catalogue->FindFamily(Def->FamilyId))
    {
        for (const StructureMethod& Method : Family->Methods)
        {
            if (Method.Id == Def->MethodId) MethodName = PWClassA3::TextFromUtf8(Method.Name);
        }
    }
    Out.Method = FText::Format(PWClassA3::Str(Exposure.MethodKnown ? TEXT("phase2.a3.common.method.known") : TEXT("phase2.a3.common.method.unfamiliar")),
                               MethodName);

    const std::vector<std::string> Remaining = InvestigateRemaining(*Def, Exposure.TestsRun);
    Out.Standing = static_cast<int32>(Remaining.size());

    for (const InvestigateTest& Test : Def->Tests)
    {
        FTestRow Row;
        Row.Id = PWClassA3::FromUtf8(Test.Id);
        Row.Name = PWClassA3::TextFromUtf8(Test.Name);
        Row.Kind = PWClassA3::Str(TEXT("phase2.a3.investigate.kind.") + FString(A3IKindKeys[static_cast<size_t>(Test.Kind)]));
        TArray<FText> Names;
        for (const std::string& CauseId : Test.Implicates) Names.Add(A3ICauseName(*Def, CauseId));
        if (!Names.IsEmpty())
        {
            Row.Implicates = FText::Format(PWClassA3::Str(TEXT("phase2.a3.investigate.test.implicates")), A3IJoin(Names));
        }
        Row.bRun = A3IHas(Exposure.TestsRun, Test.Id);
        // Only a test that has been run shows what it read; before that the model does not know it.
        Row.bPositive = Row.bRun && InvestigateTestPositive(*Def, Test);
        InvestigateAction Action;
        Action.Kind = InvestigateActionKind::RunTest;
        Action.Test = Test.Id;
        const SchoolLessonQuote Quote = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(Action));
        Row.Units = Quote.Units;
        Row.UnitsAfter = Quote.UnitsAfter;
        Row.Issue = Quote.Issue;
        Out.Tests.Add(MoveTemp(Row));
    }

    for (const InvestigateCause& Cause : Def->Causes)
    {
        FCauseRow Row;
        Row.Id = PWClassA3::FromUtf8(Cause.Id);
        Row.Name = PWClassA3::TextFromUtf8(Cause.Name);
        Row.bStanding = A3IHas(Remaining, Cause.Id);
        Row.bGuess = Row.bStanding && Remaining.size() > 1;
        InvestigateAction Action;
        Action.Kind = InvestigateActionKind::Diagnose;
        Action.Cause = Cause.Id;
        Row.Issue = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(Action)).Issue;
        Out.Causes.Add(MoveTemp(Row));
    }

    InvestigateAction Report;
    Report.Kind = InvestigateActionKind::ReportInconclusive;
    Out.ReportIssue = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(Report)).Issue;

    std::vector<std::string> Prefix;
    for (const std::string& RunId : Exposure.TestsRun)
    {
        Prefix.push_back(RunId);
        const auto Found = std::find_if(Def->Tests.begin(), Def->Tests.end(), [&RunId](const InvestigateTest& T) { return T.Id == RunId; });
        if (Found == Def->Tests.end()) continue;
        const bool bPositive = InvestigateTestPositive(*Def, *Found);
        TArray<FText> Names;
        for (const std::string& Id : InvestigateRemaining(*Def, Prefix)) Names.Add(A3ICauseName(*Def, Id));
        const FText Causes = Names.IsEmpty() ? PWClassA3::Str(TEXT("phase2.a3.investigate.evidence.causes_none"))
                                             : FText::Format(PWClassA3::Str(TEXT("phase2.a3.investigate.evidence.causes")), A3IJoin(Names));
        Out.Evidence.Add(FText::Format(PWClassA3::Str(TEXT("phase2.a3.investigate.evidence")), PWClassA3::TextFromUtf8(Found->Name),
                                       PWClassA3::Str(bPositive ? TEXT("phase2.a3.investigate.evidence.positive")
                                                                : TEXT("phase2.a3.investigate.evidence.negative")),
                                       Causes));
    }
    return Out;
}
} // namespace PWClassInvestigate

void SPWClassInvestigate::Construct(const FArguments& InArgs)
{
    PWClassA3::FShellArgs Args;
    Args.Bridge = InArgs._Bridge;
    Args.Space = InArgs._Space;
    Args.OnClose = InArgs._OnClose;
    Args.OnLessonClosed = InArgs._OnLessonClosed;
    Args.ReturnFocus = InArgs._ReturnFocus;
    ConstructShell(Args);
}

FText SPWClassInvestigate::ShellSubtitle() const
{
    return PWClassA3::Str(TEXT("phase2.a3.investigate.subtitle"));
}

int32 SPWClassInvestigate::ShellGoalCount() const
{
    return UE_ARRAY_COUNT(A3IGoals);
}

FText SPWClassInvestigate::ShellGoalName(const int32 Index) const
{
    return PWClassA3::Str(TEXT("phase2.a3.investigate.goal.") + FString(A3IGoalKeys[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]));
}

FText SPWClassInvestigate::ShellGoalHint(const int32 Index) const
{
    return PWClassA3::Str(TEXT("phase2.a3.investigate.goal.") + FString(A3IGoalKeys[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]) + TEXT(".hint"));
}

PerfectWorld::SchoolLessonGoal SPWClassInvestigate::ShellGoalAt(const int32 Index) const
{
    return PerfectWorld::SchoolLessonGoal(A3IGoals[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]);
}

void SPWClassInvestigate::ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const
{
    const PerfectWorld::InvestigateExposure& Exposure = View.Investigate.Exposure;
    bActive = Exposure.Active();
    Left = Exposure.UnitsLeft;
    Total = Exposure.UnitsTotal;
}

void SPWClassInvestigate::ShellFrameChanged()
{
    Model = HasBridge() ? PWClassInvestigate::BuildModel(GetBridge(), GetFrame()) : PWClassInvestigate::FModel();
}

void SPWClassInvestigate::ShellBuildPlay(const TSharedRef<SVerticalBox>& Into)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    if (!Model.bActive)
    {
        AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.no_exercise")), false);
        return;
    }
    AddHeading(Into, Model.CaseName);
    AddNote(Into, Model.CaseBrief, false);
    for (const FText& Badge : {Model.Symptom, Model.Role, Model.Scarce, Model.Goal, Model.Method})
    {
        Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
        [
            PWUI::MakeText(Badge, PWUI::ETextRole::Meta, Style.TextSecondary, true)
        ];
    }
    AddUnitsMeter(Into);

    AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.investigate.beat.isolate")));
    for (const PWClassInvestigate::FTestRow& Row : Model.Tests)
    {
        const FString Id = Row.Id;
        FText Detail = FText::Format(PWClassA3::Str(TEXT("phase2.a3.investigate.test.kind_line")), Row.Kind, Row.Implicates);
        FText Cost = PWClassA3::CostText(Row.Units);
        ECostTone Tone = Row.Units == 0 ? ECostTone::Free : ECostTone::Plain;
        bool bEnabled = Row.Issue.Ok();
        if (Row.bRun)
        {
            Detail = PWClassA3::Str(Row.bPositive ? TEXT("phase2.a3.investigate.test.done_positive") : TEXT("phase2.a3.investigate.test.done_negative"));
            Cost = FText::GetEmpty();
            Tone = ECostTone::Plain;
            bEnabled = false;
        }
        else if (Row.Issue.Code == "structure.units_short")
        {
            Cost = FText::Format(PWClassA3::Str(TEXT("phase2.a3.common.cost.short")), FText::AsNumber(Row.Units), FText::AsNumber(Model.UnitsLeft));
            Tone = ECostTone::Short;
        }
        AddActionRow(Into, TEXT("test.") + Id, Row.Name, Detail, Cost, Tone, bEnabled, [this, Id]() { ExecuteRunTest(Id); });
    }
    if (!Model.Evidence.IsEmpty())
    {
        AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.investigate.evidence_heading")));
        for (const FText& Line : Model.Evidence)
        {
            AddNote(Into, Line, false);
        }
    }

    AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.investigate.beat.conclude")));
    for (const PWClassInvestigate::FCauseRow& Row : Model.Causes)
    {
        const FString Id = Row.Id;
        const FText Detail = !Row.bStanding ? PWClassA3::Str(TEXT("phase2.a3.investigate.cause.ruled_out"))
                           : (Row.bGuess ? PWClassA3::Str(TEXT("phase2.a3.investigate.cause.guess"))
                                         : PWClassA3::Str(TEXT("phase2.a3.investigate.cause.standing")));
        AddActionRow(Into, TEXT("diagnose.") + Id, FText::Format(PWClassA3::Str(TEXT("phase2.a3.investigate.cause.action")), Row.Name), Detail,
                     PWClassA3::CostText(0), ECostTone::Free, Row.Issue.Ok(), [this, Id]() { ExecuteDiagnose(Id); });
    }
    AddActionRow(Into, TEXT("report"), PWClassA3::Str(TEXT("phase2.a3.investigate.report.action")),
                 Model.Standing == 1 ? PWClassA3::Str(TEXT("phase2.a3.investigate.report.isolated")) : PWClassA3::Str(TEXT("phase2.a3.investigate.report.hint")),
                 PWClassA3::CostText(0), ECostTone::Free, Model.ReportIssue.Ok(), [this]() { ExecuteReportInconclusive(); });
    AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.leave_note")));
}

TArray<FText> SPWClassInvestigate::ShellOutcomeDetail(const PerfectWorld::SchoolLessonOutcome& Outcome) const
{
    TArray<FText> Lines;
    const auto* Result = std::get_if<PerfectWorld::InvestigateOutcome>(&Outcome);
    if (Result == nullptr)
    {
        return Lines;
    }
    FText Diagnosed = FText::GetEmpty();
    if (HasBridge() && GetBridge().GetCatalogues().Structures.Investigate != nullptr)
    {
        if (const PerfectWorld::InvestigateCaseDef* Def = GetBridge().GetCatalogues().Structures.Investigate->FindCase(Result->CaseId))
        {
            Diagnosed = A3ICauseName(*Def, Result->Diagnosed);
        }
    }
    Lines.Add(Result->Diagnosed.empty() ? PWClassA3::Str(TEXT("phase2.a3.investigate.outcome.reported"))
                                        : FText::Format(PWClassA3::Str(TEXT("phase2.a3.investigate.outcome.diagnosed")), Diagnosed));
    Lines.Add(FText::Format(PWClassA3::Str(TEXT("phase2.a3.investigate.outcome.tests")), FText::AsNumber(Result->TestsRun),
                            FText::AsNumber(Result->RedundantTests), FText::AsNumber(Result->RemainingCauses)));
    const struct { uint8 Bit; const TCHAR* Key; } Failures[] = {
        {PerfectWorld::InvestigateFailWrongCause, TEXT("phase2.a3.investigate.fail.wrong_cause")},
        {PerfectWorld::InvestigateFailUnsupported, TEXT("phase2.a3.investigate.fail.unsupported")},
        {PerfectWorld::InvestigateFailRedundant, TEXT("phase2.a3.investigate.fail.redundant")},
        {PerfectWorld::InvestigateFailAbandoned, TEXT("phase2.a3.investigate.fail.abandoned")}};
    for (const auto& Failure : Failures)
    {
        if ((Result->Failures & Failure.Bit) != 0) Lines.Add(PWClassA3::Str(Failure.Key));
    }
    return Lines;
}

PerfectWorld::SchoolSessionResult SPWClassInvestigate::ExecuteRunTest(const FString& TestId)
{
    PerfectWorld::InvestigateAction Action;
    Action.Kind = PerfectWorld::InvestigateActionKind::RunTest;
    Action.Test = std::string(TCHAR_TO_UTF8(*TestId));
    FText Name = FText::GetEmpty();
    for (const PWClassInvestigate::FTestRow& Row : Model.Tests)
    {
        if (Row.Id == TestId) Name = Row.Name;
    }
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), FText::Format(PWClassA3::Str(TEXT("phase2.a3.investigate.test.ran")), Name));
}

PerfectWorld::SchoolSessionResult SPWClassInvestigate::ExecuteDiagnose(const FString& CauseId)
{
    PerfectWorld::InvestigateAction Action;
    Action.Kind = PerfectWorld::InvestigateActionKind::Diagnose;
    Action.Cause = std::string(TCHAR_TO_UTF8(*CauseId));
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), FText::GetEmpty());
}

PerfectWorld::SchoolSessionResult SPWClassInvestigate::ExecuteReportInconclusive()
{
    PerfectWorld::InvestigateAction Action;
    Action.Kind = PerfectWorld::InvestigateActionKind::ReportInconclusive;
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), FText::GetEmpty());
}


#undef LOCTEXT_NAMESPACE
