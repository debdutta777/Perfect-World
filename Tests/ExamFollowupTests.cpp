#include "WorldExamFollowup.h"

#include "P405ExamTestSupport.h"
#include "LegacyRecovery.h"
#include "KnownCommitments.h"

#include <iostream>

using namespace PerfectWorld;

namespace
{
constexpr std::string_view P407UMentor = "school_followup.p2.p4_withdrawn_school_mentor";
WorldSim P407UWithdrawn(const P405World& w);

P407UFollowupView P407URow(const WorldSim& sim, std::string_view id)
{
    const auto rows = sim.P407UFollowups();
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.Action.Id == id; });
    P2CExpect(found != rows.end(), "the authoritative next step exists");
    return *found;
}

P407UFollowupCommand P407UCommand(const WorldSim& sim, P407UFollowupAction kind, std::string_view id = P407UMentor)
{
    const auto row = P407URow(sim, id);
    return {kind, std::string(id), row.At, {}, row.P502Revision};
}

void P502Tasks(const WorldSim& source)
{
    const auto fresh = [&] {
        return WorldSim(source.State(), MaxEventsPerAdvance, source.SchoolCatalogues().Day, {},
            source.SchoolContent(), source.AwakeningContent(), source.ExaminationContent());
    };
    std::size_t tasks = 0;
    for (const auto& offered : source.P407UFollowups())
    {
        if (offered.P502Choices.empty()) continue;
        ++tasks;
        for (const auto& choice : offered.P502Choices)
        {
            auto sim = fresh();
            const auto id = offered.Action.Id;
            auto premature = P407UCommand(sim, P407UFollowupAction::P502Resolve, id);
            premature.P502ChoiceId = choice.Id;
            S04Refused(sim, premature, "p502.not_active", "booking alone cannot resolve a task");
            S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book, id), "reserve task time");
            const auto oldStart = P407UCommand(sim, P407UFollowupAction::P502Start, id);
            S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Cancel, id), "cancel without task completion");
            S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book, id), "rebook at the same instant");
            S04Refused(sim, oldStart, "p502.stale", "same-instant rebooking invalidates old task commands");
            const auto at = P407URow(sim, id).At;
            S04Accept(sim, AdvanceTime{at + 1 - sim.State().Clock.Now()}, "miss an optional task in each outcome");
            const auto replacement = P407URow(sim, id);
            P2CExpect(replacement.CanBook && replacement.At > at, "every outcome can recover a missed appointment");
            S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book, id), "replace the missed task booking");
            if (replacement.At > sim.State().Clock.Now())
                S04Accept(sim, AdvanceTime{replacement.At - sim.State().Clock.Now()}, "reach task appointment");
            const auto arrived = S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Attend, id), "record arrival only");
            P2CExpect(arrived.From == arrived.To && !P407URow(sim, id).P502Record,
                "attendance has no task consequence or time charge");
            S04Accept(sim, P407UCommand(sim, P407UFollowupAction::P502Start, id), "start the task through the command path");
            auto oldResolve = P407UCommand(sim, P407UFollowupAction::P502Resolve, id);
            oldResolve.P502ChoiceId = choice.Id;
            S04Accept(sim, P407UCommand(sim, P407UFollowupAction::P502Leave, id), "leave without awards");
            P2CExpect(!P407URow(sim, id).P502Record, "leaving never completes a task");
            S04Accept(sim, P407UCommand(sim, P407UFollowupAction::P502Resume, id), "resume the same task");
            S04Refused(sim, oldResolve, "p502.stale", "an old callback after leave/resume is inert");
            auto resolve = P407UCommand(sim, P407UFollowupAction::P502Resolve, id);
            resolve.P502ChoiceId = "admission.money.licence.retake";
            S04Refused(sim, resolve, "p502.choice", "forged awards and retakes cannot be task choices");
            auto wrongTime = resolve;
            ++wrongTime.ExpectedAt;
            S04Refused(sim, wrongTime, "p407u.stale", "a stale task time is inert");
            resolve.P502ChoiceId = choice.Id;
            const auto before = sim.State();
            const auto paid = S04Accept(sim, resolve, "a valid task choice resolves once");
            const auto done = P407URow(sim, id);
            P2CExpect(done.P502Stage == P502TaskStage::Completed && done.P502Record &&
                done.P502Record->Id == choice.Id && done.P502Record->Records == choice.Records &&
                paid.To - paid.From == offered.Action.DurationMinutes,
                "the selected plan or practical step has its observable record and exact time cost");
            P2CExpect(SerialiseReceipts(before.DayLoop.Receipts) != SerialiseReceipts(sim.State().DayLoop.Receipts),
                "task resolution goes through the authoritative receipt owner");
            P2CExpect(before.Commerce == sim.State().Commerce && before.Examination == sim.State().Examination &&
                SerialiseAwakeningState(before) == SerialiseAwakeningState(sim.State()),
                "an enquiry never grants money, admission, a licence, a sitting or invented learning");
            S04Refused(sim, resolve, "p502.completed", "duplicate resolution changes nothing");
            S04Refused(sim, P407UCommand(sim, P407UFollowupAction::Book, id), "p502.completed", "completed tasks cannot rebook rewards");
            S04Accept(sim, SchoolPlanDay{}, "task completion leaves ordinary school planning usable");
        }
    }
    P2CExpect(tasks == 1, "each frozen outcome has exactly one consequential task");
}

void P502RecoveryAndConflicts(const P405World& w)
{
    auto initial = P407UWithdrawn(w);
    const auto bind = [&](WorldState state, const SchoolFollowupContent& content) {
        return WorldSim(std::move(state), MaxEventsPerAdvance, w.Base.W.School.Cats.Day, {},
            w.Base.W.School.School(), w.Base.W.Bind(), {&w.Runners, &content});
    };
    auto lessons = w.Followups;
    for (auto& action : lessons.Actions)
        if (action.Id == P407UMentor) action.Minute = w.Base.W.School.Cats.Day.School->Blocks.front().StartMinute;
    auto classSim = bind(initial.State(), lessons);
    const auto afterClass = P407URow(classSim, P407UMentor);
    P2CExpect(afterClass.CanBook &&
        std::find(afterClass.P502Conflicts.begin(), afterClass.P502Conflicts.end(), "school.lessons") != afterClass.P502Conflicts.end() &&
        afterClass.At >= GameClock::StartOfDay(afterClass.At) + w.Base.W.School.Cats.Day.School->Blocks.back().EndMinute,
        "lesson conflicts disclose a free slot after dismissal");

    auto promises = w.Followups;
    for (auto& action : promises.Actions)
        if (action.Id == P407UMentor) action.Minute = 18 * 60;
    auto promiseState = initial.State();
    promiseState.Conversations[std::string(HouseholdAdultId)].Facts = {std::string(FactRotaSweepAndCook)};
    auto promiseSim = bind(promiseState, promises);
    const auto afterPromise = P407URow(promiseSim, P407UMentor);
    P2CExpect(afterPromise.CanBook &&
        std::find(afterPromise.P502Conflicts.begin(), afterPromise.P502Conflicts.end(), FactRotaSweepAndCook) != afterPromise.P502Conflicts.end() &&
        afterPromise.At == GameClock::StartOfDay(afterPromise.At) + 19 * 60,
        "a daily household promise discloses a later free hour instead of hiding the offer");

    auto sim = P407UWithdrawn(w);
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book), "reserve before a competing activity");
    const auto at = P407URow(sim, P407UMentor).At;
    S04Accept(sim, AdvanceTime{at - sim.State().Clock.Now()}, "reach reserved task time");
    S04Accept(sim, SchoolPlanDay{}, "plan ordinary activities at the appointment");
    DayCommand commit;
    commit.Kind = DayCommandKind::Commit;
    commit.ActivityId = "activity.rest";
    commit.PlaceKind = "home";
    S04Accept(sim, WorldDayCommand{commit}, "an ordinary activity remains optional");
    const auto occupied = P407URow(sim, P407UMentor);
    P2CExpect(!occupied.CanAttend && !occupied.P502Conflicts.empty() && occupied.P502AlternativeAt > at,
        "an existing activity prevents collision and discloses another slot");
    S04Refused(sim, P407UCommand(sim, P407UFollowupAction::Attend), "p407u.busy", "an activity cannot overlap task arrival");
    DayCommand cancel;
    cancel.Kind = DayCommandKind::Cancel;
    S04Accept(sim, WorldDayCommand{cancel}, "ordinary cancellation releases the activity");
    DayCommand plan;
    plan.Kind = DayCommandKind::PlanDay;
    plan.Plan = {{static_cast<std::int32_t>(at - GameClock::StartOfDay(at)), "activity.rest"}};
    S04Accept(sim, WorldDayCommand{plan}, "make a timed intention at the reserved task time");
    const auto planned = P407URow(sim, P407UMentor);
    P2CExpect(!planned.CanAttend && !planned.P502Conflicts.empty() && planned.P502AlternativeAt > at,
        "a timed ordinary intention discloses another task slot");
    plan.Plan.clear();
    S04Accept(sim, WorldDayCommand{plan}, "release the timed intention");
    S04Accept(sim, ConversationOpen{std::string(HouseholdAdultId)}, "open an existing conversation session");
    const auto talking = P407URow(sim, P407UMentor);
    P2CExpect(!talking.CanAttend && !talking.P502Conflicts.empty() && talking.P502AlternativeAt > at,
        "an open conversation discloses the competing session and another slot");
    S04Refused(sim, P407UCommand(sim, P407UFollowupAction::Attend), "time.conversation_open",
        "task attendance cannot collide with an open conversation");
    S04Accept(sim, ConversationLeave{std::string(HouseholdAdultId)}, "close the conversation session");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Attend), "arrive after the activity closes");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::P502Start), "start before cancelling the task itself");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Cancel), "cancel an active task without awards");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book), "rebook a cancelled active task");
    P2CExpect(!P407URow(sim, P407UMentor).Attended && !P407URow(sim, P407UMentor).P502Record,
        "cancel and rebook require a new arrival and keep consequences empty");

    const auto legacyRow = P407URow(initial, P407UMentor);
    S04Accept(initial, AdvanceTime{legacyRow.At + legacyRow.Action.DurationMinutes - initial.State().Clock.Now()},
        "seed historical paid attendance at its legitimate finish instant");
    auto legacyState = initial.State();
    const auto key = std::string(P407UFollowupFlagPrefix) + legacyRow.Action.AppointmentId;
    legacyState.Flags[key + ".book." + std::to_string(legacyRow.At)] = true;
    legacyState.Flags[key + ".done." + std::to_string(legacyRow.At)] = true;
    auto legacy = w.Sim(legacyState);
    P2CExpect(P407URow(legacy, P407UMentor).P502Stage == P502TaskStage::LegacyAttendance &&
        !P407URow(legacy, P407UMentor).P502Record, "old paid attendance never invents task completion");
    S04Accept(legacy, P407UCommand(legacy, P407UFollowupAction::Cancel), "release the legacy reservation");
    S04Accept(legacy, P407UCommand(legacy, P407UFollowupAction::Book), "book a fresh task after legacy attendance");
    P2CExpect(P407URow(legacy, P407UMentor).At > legacyRow.At &&
        P407URow(legacy, P407UMentor).P502Stage == P502TaskStage::Offered &&
        legacy.State().Flags.at(key + ".done." + std::to_string(legacyRow.At)),
        "legacy history survives recovery and cannot be selected as the new task instant");
}

void P502ReceiptsAndReplay(const P405World& w)
{
    const auto seed = P407UWithdrawn(w).State();
    auto content = w.Followups;
    const auto selected = P407URow(w.Sim(seed), P407UMentor).Action;
    for (auto& action : content.Actions)
    {
        if (action.Id == "school_followup.p2.withdrawn")
        {
            action.DayOffset = selected.DayOffset;
            action.Minute = selected.Minute;
        }
        if (action.Id == "school_followup.p2.p4_withdrawn_home_rest")
        {
            action.DayOffset = 7;
            action.Minute = selected.Minute;
        }
    }
    auto sim = WorldSim(seed, MaxEventsPerAdvance, w.Base.W.School.Cats.Day, {},
        w.Base.W.School.School(), w.Base.W.Bind(), {&w.Runners, &content});
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book), "book before a disclosed sacrifice");
    const auto at = P407URow(sim, P407UMentor).At;
    S04Accept(sim, AdvanceTime{at - sim.State().Clock.Now()}, "reach receipt task");
    const auto offered = P407URow(sim, P407UMentor);
    P2CExpect(std::any_of(offered.P502Alternatives.begin(), offered.P502Alternatives.end(),
        [](const auto& a) { return a.Id == "school_followup.p2.withdrawn"; }) &&
        std::none_of(offered.P502Alternatives.begin(), offered.P502Alternatives.end(),
        [](const auto& a) { return a.Id == "school_followup.p2.p4_withdrawn_home_rest"; }),
        "the disclosure includes an overlapping choice and excludes an offer before its authored day");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Attend), "arrive for receipt task");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::P502Start), "start receipt task");
    const auto before = ReceiptGrandTotals(sim.State().DayLoop.Receipts);
    auto resolve = P407UCommand(sim, P407UFollowupAction::P502Resolve);
    resolve.P502ChoiceId = offered.P502Choices.front().Id;
    S04Accept(sim, resolve, "resolve with authoritative displacement rules");
    const auto after = ReceiptGrandTotals(sim.State().DayLoop.Receipts);
    P2CExpect(after.Commitments == before.Commitments + 1 &&
        after.CommittedMinutes == before.CommittedMinutes + selected.DurationMinutes &&
        after.Displaced > before.Displaced, "exactly one receipt records the disclosed sacrifice");
    auto options = w.Replay();
    options.Examination.Followups = &content;
    const auto replay = ReplayCommandLogFromSnapshot(seed, sim.Log(), options);
    P2CExpect(replay.Complete && replay.Hash == HashWorldState(sim.State()),
        "task commands replay with the same record, receipt, condition and time");
}

WorldSim P407UWithdrawn(const P405World& w)
{
    auto sim = w.Sim(w.Base.Morning);
    S04Accept(sim, P402Cmd(TowerSessionAction::Enter), "sit before mentor follow-up");
    S04Accept(sim, P402Cmd(TowerSessionAction::Withdraw), "safe withdrawal yields a real result");
    return sim;
}

void P407UOutcome(WorldSim& sim, std::string_view outcome)
{
    P2CExpect(sim.ExaminationResult().Outcome == outcome, "fixture reaches requested outcome");
    const auto before = HashWorldState(sim.State());
    const auto rows = sim.P407UFollowups();
    P2CExpect(before == HashWorldState(sim.State()), "reading next steps is free and pure");
    std::size_t booked = 0;
    for (const auto& row : rows)
    {
        P2CExpect(row.Action.OfferId != "offer.tower.retake" && row.Action.Id != "school_followup.p2.p4_poor_retake_prep",
            "P413 no outcome exposes either legacy retake action");
        P2CExpect(row.CanBook, "every displayed next step has a resolved bookable slot");
        S04Accept(sim, P407UFollowupCommand{P407UFollowupAction::Book, row.Action.Id, row.At}, "offer books a real reservation");
        P2CExpect(P407URow(sim, row.Action.Id).Booked, "booking is visible in saved authority");
        S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Cancel, row.Action.Id), "optional appointment can be cancelled");
        if (row.Action.Outcome == outcome) ++booked;
    }
    P2CExpect(booked >= 1, "each outcome class exposes an actionable next step");
    P502Tasks(sim);
    S04Accept(sim, AdvanceTime{1}, "declining every offer leaves ordinary time usable");
    S04Accept(sim, SchoolPlanDay{}, "ordinary school planning remains usable after refusal");
    DayCommand P502Commit;
    P502Commit.Kind = DayCommandKind::Commit;
    P502Commit.ActivityId = "activity.rest";
    P502Commit.PlaceKind = "home";
    S04Accept(sim, WorldDayCommand{P502Commit}, "ordinary activities remain usable after refusal");
    DayCommand P502Cancel;
    P502Cancel.Kind = DayCommandKind::Cancel;
    S04Accept(sim, WorldDayCommand{P502Cancel}, "ordinary activity cancellation remains usable");
}

void P407UOutcomes(const P405World& w)
{
    auto withdrawn = P407UWithdrawn(w);
    P407UOutcome(withdrawn, "withdrawn");
    for (auto kind : {TowerSessionAction::Decline, TowerSessionAction::Defer})
    {
        auto sim = w.Sim(w.Base.Morning);
        S04Accept(sim, P402Cmd(kind), "optional examination decision");
        P407UOutcome(sim, kind == TowerSessionAction::Decline ? "declined" : "deferred");
    }
    auto strong = w.Sim(w.Base.Morning);
    S04Accept(strong, P402Cmd(TowerSessionAction::Enter), "sit before a strong result");
    for (const auto route : P402Climb) P405Floor(strong, w.Runners, route);
    P407UOutcome(strong, "strong");
    auto poorState = w.Base.Morning;
    poorState.CountdownText.clear();
    poorState.Flags[std::string(AwakeningCountdownUnavailableFlag)] = true;
    auto poor = w.Sim(poorState);
    S04Accept(poor, P402Cmd(TowerSessionAction::Enter), "zero preparation can sit");
    P405Floor(poor, w.Runners, P402Climb[0], true);
    P407UOutcome(poor, "poor");
    auto middle = w.Sim(poorState);
    S04Accept(middle, P402Cmd(TowerSessionAction::Enter), "sit before a middle result");
    P405Floor(middle, w.Runners, P402Climb[0]);
    P405Floor(middle, w.Runners, P402Climb[1]);
    P405Floor(middle, w.Runners, P402Climb[2], true);
    P407UOutcome(middle, "middle");
}

void P407USession(const P405World& w)
{
    constexpr std::string_view P502AttendanceId = "school_followup.p2.withdrawn";
    const auto initial = P407UWithdrawn(w).State();
    auto sim = w.Sim(initial); // The replay log starts at this terminal snapshot.
    auto book = P407UCommand(sim, P407UFollowupAction::Book, P502AttendanceId);
    auto stale = book;
    ++stale.ExpectedAt;
    S04Refused(sim, stale, "p407u.stale", "a changed disclosure cannot book");
    S04Refused(sim, P407UFollowupCommand{P407UFollowupAction::Book, "school_followup.p2.p4_strong_mentor", book.ExpectedAt},
        "p407u.unavailable", "a forged ineligible result action cannot book");
    const auto now = sim.State().Clock.Now();
    S04Accept(sim, book, "book the school mentor");
    P2CExpect(sim.State().Clock.Now() == now, "booking charges no time");
    S04Refused(sim, book, "p407u.booked", "duplicate booking refuses atomically");
    S04Refused(sim, P407UCommand(sim, P407UFollowupAction::Attend, P502AttendanceId), "p407u.not_due", "early attendance refuses");
    auto loaded = w.Sim(P402Reload(w.Base, sim.State(), "booking survives v13 save with existing flags and hash"));
    P2CExpect(P407URow(loaded, P502AttendanceId).Booked && P407URow(loaded, P502AttendanceId).At == book.ExpectedAt,
        "Continue retains the exact appointment");
    S04Accept(sim, AdvanceTime{book.ExpectedAt - now}, "ordinary time reaches the appointment");
    const auto row = P407URow(sim, P502AttendanceId);
    P2CExpect(row.CanAttend && row.Booked && !row.Attended, "the appointment is now attendable");
    const auto attend = P407UCommand(sim, P407UFollowupAction::Attend, P502AttendanceId);
    const auto paid = S04Accept(sim, attend, "attend the mentor session");
    P2CExpect(paid.To - paid.From == row.Action.DurationMinutes && P407URow(sim, P502AttendanceId).Attended,
        "attendance spends the authored minutes and records completion");
    S04Refused(sim, attend, "p407u.attended", "a duplicate attendance cannot charge twice");
    S04Refused(sim, book, "p407u.attended", "a completed session cannot be booked again");
    loaded = w.Sim(P402Reload(w.Base, sim.State(), "completed session survives existing save envelope"));
    P2CExpect(P407URow(loaded, P502AttendanceId).Attended, "Continue retains attendance");
    S04Refused(loaded, attend, "p407u.attended", "reload does not permit a second attendance");
    const auto replay = ReplayCommandLogFromSnapshot(initial, sim.Log(), w.Replay());
    P2CExpect(replay.Complete && replay.Hash == HashWorldState(sim.State()), "booking and attendance replay identically");
    S04Refused(sim, SetWorldFlag{std::string(P407UFollowupFlagPrefix) + "forged", true},
        "flag.reserved", "generic flags cannot forge appointment authority");
}

void P407USlotsAndRollback(const P405World& w)
{
    auto sim = P407UWithdrawn(w);
    const auto row = P407URow(sim, P407UMentor);
    const auto second = std::find_if(w.Followups.Actions.begin(), w.Followups.Actions.end(),
        [](const auto& a) { return a.Id == "school_followup.p2.withdrawn"; });
    P2CExpect(second != w.Followups.Actions.end(), "existing A8 conversation");
    auto changed = w.Followups;
    for (auto& action : changed.Actions)
        if (action.Id == second->Id) { action.DayOffset = row.Action.DayOffset; action.Minute = row.Action.Minute; }
    auto collision = WorldSim(sim.State(), MaxEventsPerAdvance, w.Base.W.School.Cats.Day, {},
        w.Base.W.School.School(), w.Base.W.Bind(), {&w.Runners, &changed});
    S04Accept(collision, P407UCommand(collision, P407UFollowupAction::Book), "reserve mentor window");
    const auto other = P407URow(collision, second->Id);
    P2CExpect(other.CanBook && other.At > row.At && !other.P502Conflicts.empty() && other.P502AlternativeAt == other.At,
        "overlapping appointments disclose the conflict and a free school day");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book), "book before lateness");
    S04Accept(sim, AdvanceTime{row.At + 1 - sim.State().Clock.Now()}, "miss the optional appointment");
    const auto later = P407URow(sim, P407UMentor);
    P2CExpect(later.CanBook && later.At > row.At, "a missed appointment has a reachable new booking");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book), "replace the missed reservation");
    if (later.At > sim.State().Clock.Now())
        S04Accept(sim, AdvanceTime{later.At - sim.State().Clock.Now()}, "reach the replacement appointment");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Attend), "arrive at replacement task");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::P502Start), "start before interrupted resolution");
    S04Accept(sim, ScheduleWorldEvent{later.At + 1, "p407u/test/one"}, "first session event", CommandSource::System);
    S04Accept(sim, ScheduleWorldEvent{later.At + 2, "p407u/test/two"}, "second session event", CommandSource::System);
    auto capped = WorldSim(sim.State(), 1, w.Base.W.School.Cats.Day, {}, w.Base.W.School.School(),
        w.Base.W.Bind(), {&w.Runners, &w.Followups});
    auto P502Resolve = P407UCommand(capped, P407UFollowupAction::P502Resolve);
    P502Resolve.P502ChoiceId = P407URow(capped, P407UMentor).P502Choices.front().Id;
    S04Refused(capped, P502Resolve, "p407u.charge_interrupted",
        "an interrupted charge restores clock, flags, events and sequence");
    P2CExpect(P407URow(capped, P407UMentor).Attended && !P407URow(capped, P407UMentor).P502Record,
        "interruption preserves arrival but records no task completion");
    const auto school = w.Base.W.School.Cats.Day;
    auto P413Spent = P407UWithdrawn(w);
    auto P413Weekend = w.Followups;
    for (auto& P413Action : P413Weekend.Actions)
        if (P413Action.Id == "school_followup.p2.study_bridge") P413Action.DayOffset = 6;
    auto P413WeekendSim = WorldSim(P413Spent.State(), MaxEventsPerAdvance, w.Base.W.School.Cats.Day, {},
        w.Base.W.School.School(), w.Base.W.Bind(), {&w.Runners, &P413Weekend});
    const auto P413Bridge = P407URow(P413WeekendSim, "school_followup.p2.study_bridge");
    P2CExpect(CalendarRequiresAttendance(*school.Calendar, CalendarDayOfInstant(P413Bridge.At)),
        "P413 a weekend ordinary qualification suggestion moves to an open school day");
    for (const auto* P413Id : {"school_followup.p2.retake", "school_followup.p2.p4_poor_retake_prep"})
    {
        S04Refused(P413Spent, P407UFollowupCommand{P407UFollowupAction::Book, P413Id, 0}, "p407u.unavailable",
            "P413 hidden retake actions cannot be booked by submitting their IDs");
        S04Refused(P413Spent, P407UFollowupCommand{P407UFollowupAction::Attend, P413Id, 0}, "p407u.unavailable",
            "P413 hidden retake actions cannot be attended by submitting their IDs");
        S04Refused(P413Spent, P407UFollowupCommand{P407UFollowupAction::P502Resolve, P413Id, 0, "retake"}, "p407u.unavailable",
            "P502 hidden retake actions cannot resolve through task commands");
    }
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root");
    const P2CFixture fixture(argv[1]);
    const P402World base(fixture);
    const P405World real(base, fixture.Content);
    P407UOutcomes(real);
    P407USession(real);
    P407USlotsAndRollback(real);
    P502RecoveryAndConflicts(real);
    P502ReceiptsAndReplay(real);
    std::cout << "P407U follow-up assertions passed\n";
    return 0;
}
