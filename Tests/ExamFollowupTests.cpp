#include "WorldExamFollowup.h"

#include "P405ExamTestSupport.h"
#include "LegacyRecovery.h"
#include "KnownCommitments.h"
#include "WorldExamSittings.h"

#include <iostream>
#include <map>
#include <tuple>

using namespace PerfectWorld;

namespace
{
const ContentDb* P503Content = nullptr;
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
            // P503: the time charge runs the ordinary allowance settlement (WorldSim::Submit); the task adds no money of its own.
            const auto settled = SettleAllowance(before.Commerce, paid.From, paid.To);
            P2CExpect(settled.Ok(), "the task interval settles like any other time");
            P2CExpect(settled.State == sim.State().Commerce && before.Examination == sim.State().Examination &&
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
    // P503: known household promises are read from the bound content (WorldAwakeningCatalogues::Content), which the
    // shared fixture binding leaves empty; bind the real content so the promise is actually known.
    auto knowing = w.Base.W.Bind();
    knowing.Content = P503Content;
    auto promiseSim = WorldSim(promiseState, MaxEventsPerAdvance, w.Base.W.School.Cats.Day, {},
        w.Base.W.School.School(), knowing, {&w.Runners, &promises});
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

    // P5-03: pre-v14 paid attendance is exercised through a real schema-13 save in P503Legacy.
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
    // P5-15 cohort: two clean floors and a floor-3 ejection (3500) is now poor; a full climb with one recovered
    // mistake per floor (5547, rank 453, decile 5) is the middle witness.
    for (const auto route : P402Climb) P405MiddleFloor(middle, w.Runners, route);
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
    auto loaded = w.Sim(P402Reload(w.Base, sim.State(), "booking survives the current save envelope and hash"));
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
// ---- P5-03: durable follow-up records, the v13 flag migration and second-sitting identity ----

std::string P503Hex(std::uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    std::string text = "0x";
    for (int shift = 60; shift >= 0; shift -= 4) text.push_back(digits[(value >> shift) & 0xfU]);
    return text;
}

std::string P503Replace(std::string text, const std::string& from, const std::string& to)
{
    const auto at = text.find(from);
    P2CExpect(at != std::string::npos, "replacement anchor exists");
    return text.replace(at, from.size(), to);
}

/**
 * A schema-13 save of `world` (no follow-up records) holding `legacy` flags exactly as P4-07 spelled them: the v13 writer
 * differs from the v14 one only in the version and in the flags it held, and hash version 10 is unchanged, so the checksum
 * is the hash of the world with those flags.
 */
std::string P503V13(const P405World& w, WorldState world, const std::map<std::string, bool>& legacy, std::int32_t version = 13)
{
    P2CExpect(world.ExaminationFollowups.empty(), "a v13 world has no follow-up records");
    const std::string clean = P402SaveText(w.Base, world);
    P2CExpect(!clean.empty(), "the base world saves");
    const std::uint64_t cleanHash = HashWorldState(world);
    for (const auto& [key, value] : legacy) world.Flags[key] = value;
    std::string flags = "\"flags\": {";
    bool first = true;
    for (const auto& [key, value] : world.Flags)
    {
        flags += first ? "\n" : ",\n";
        first = false;
        flags += "      \"" + key + "\": " + (value ? "true" : "false");
    }
    flags += first ? "}" : "\n    }";
    const auto open = clean.find("\"flags\": {");
    const auto close = clean.find(",\n    \"gates\"", open);
    P2CExpect(open != std::string::npos && close != std::string::npos, "flags anchors");
    std::string text = clean.substr(0, open) + flags + clean.substr(close);
    text = P503Replace(text, "\"schemaVersion\": " + std::to_string(WorldSaveSchemaVersion) + ",",
        "\"schemaVersion\": " + std::to_string(version) + ",");
    return P503Replace(text, "\"stateHash\": \"" + P503Hex(cleanHash) + "\"", "\"stateHash\": \"" + P503Hex(HashWorldState(world)) + "\"");
}

WorldSaveError P503ErrorOf(const std::string& text)
{
    WorldSaveData data;
    return DeserializeWorldSave(text, data).Error;
}

P407UFollowupCommand P503Resolve(const WorldSim& sim, std::string_view id = P407UMentor, std::size_t choice = 0)
{
    auto command = P407UCommand(sim, P407UFollowupAction::P502Resolve, id);
    command.P502ChoiceId = P407URow(sim, id).P502Choices.at(choice).Id;
    return command;
}

void P503Reach(WorldSim& sim, GameInstant at, std::string_view what)
{
    if (at > sim.State().Clock.Now()) S04Accept(sim, AdvanceTime{at - sim.State().Clock.Now()}, what);
    P2CExpect(sim.State().Clock.Now() == at, "the clock reaches the appointment");
}

void P503Legacy(const P405World& w)
{
    constexpr std::string_view Attendance = "school_followup.p2.withdrawn";
    auto sim = P407UWithdrawn(w);
    const auto mentor = P407URow(sim, P407UMentor);
    const auto other = P407URow(sim, Attendance);
    const std::string prefix(P407UFollowupFlagPrefix);
    const std::string mentorKey = prefix + mentor.Action.AppointmentId;
    const std::string otherKey = prefix + other.Action.AppointmentId;
    // P4-07 history: the mentor session was booked and paid; the other was booked once, released, then booked again.
    P503Reach(sim, mentor.At + mentor.Action.DurationMinutes, "the legacy paid session has finished");
    const WorldState seed = sim.State();
    const auto later = std::to_string(sim.State().Clock.Now() + 24 * 60);
    const std::map<std::string, bool> legacy{
        {mentorKey + ".book." + std::to_string(mentor.At), true}, {mentorKey + ".done." + std::to_string(mentor.At), true},
        {otherKey + ".book." + std::to_string(other.At), false}, {otherKey + ".book." + later, true}};
    const std::string v13 = P503V13(w, seed, legacy);
    const std::string original = v13;
    WorldSaveData data;
    const auto loaded = DeserializeWorldSave(v13, data);
    if (!loaded.Succeeded()) std::cerr << loaded.Message << '\n';
    P2CExpect(loaded.Succeeded() && loaded.SourceVersion == 13 && !loaded.HashVerified && !loaded.Migrated && v13 == original,
        "a schema-13 save with P4-07 flags authenticates, migrates in memory and loads unverified");
    const auto& world = data.World;
    P2CExpect(world.Flags.lower_bound(prefix) == world.Flags.end() || !world.Flags.lower_bound(prefix)->first.starts_with(prefix),
        "no reserved follow-up flag survives migration");
    P2CExpect(world.ExaminationFollowups.size() == 2, "one record per appointment of the sitting");
    const auto record = [&](const std::string& appointment) {
        const auto it = std::find_if(world.ExaminationFollowups.begin(), world.ExaminationFollowups.end(),
            [&](const auto& r) { return r.AppointmentId == appointment; });
        P2CExpect(it != world.ExaminationFollowups.end(), "migrated record exists");
        return *it;
    };
    const auto paid = record(mentor.Action.AppointmentId);
    P2CExpect(paid.Sitting == 0 && paid.Revision == 1 && paid.Sessions.size() == 1 &&
        paid.Sessions[0] == P503FollowupSession{1, mentor.At, P503SessionState::Paid} && !paid.Completion,
        "old paid attendance migrates as paid attendance, never a task completion");
    const auto booked = record(other.Action.AppointmentId);
    P2CExpect(booked.Revision == 2 && booked.Sessions.size() == 2 &&
        booked.Sessions[0] == P503FollowupSession{1, other.At, P503SessionState::Released} &&
        booked.Sessions[1].State == P503SessionState::Booked && std::to_string(booked.Sessions[1].At) == later,
        "an old reservation stays a reservation and its released predecessor stays history");

    auto continued = w.Sim(world);
    auto row = P407URow(continued, P407UMentor);
    P2CExpect(row.P502Stage == P502TaskStage::LegacyAttendance && !row.P502Record && row.CanBook && row.At > mentor.At,
        "the mentor task shows legacy attendance and offers a fresh task session");
    P2CExpect(P407URow(continued, Attendance).Booked && std::to_string(P407URow(continued, Attendance).At) == later,
        "Continue retains the migrated reservation exactly");
    S04Refused(continued, P407UCommand(continued, P407UFollowupAction::Cancel), "p407u.not_booked",
        "legacy paid attendance is not a reservation to cancel");
    S04Accept(continued, P407UCommand(continued, P407UFollowupAction::Book), "book a fresh task after legacy attendance");
    P503Reach(continued, P407URow(continued, P407UMentor).At, "reach the fresh task");
    S04Accept(continued, P407UCommand(continued, P407UFollowupAction::Attend), "arrive at the fresh task");
    S04Accept(continued, P407UCommand(continued, P407UFollowupAction::P502Start), "start the fresh task");
    S04Accept(continued, P503Resolve(continued), "resolve the fresh task once");
    const auto& after = continued.State().ExaminationFollowups;
    const auto done = std::find_if(after.begin(), after.end(), [&](const auto& r) { return r.AppointmentId == mentor.Action.AppointmentId; });
    P2CExpect(done != after.end() && done->Sessions.size() == 2 && done->Sessions[0].State == P503SessionState::Paid &&
        done->Sessions[1].State == P503SessionState::Completed && done->Completion && done->Completion->Generation == 2,
        "the legacy history survives beside the one real completion");
    const auto resaved = P402Reload(w.Base, continued.State(), "the migrated life's first v14 save is certified");
    P2CExpect(resaved.ExaminationFollowups == continued.State().ExaminationFollowups, "the records round-trip exactly");

    // Everything P4-07 could not have written refuses the load; the file is never rewritten.
    const auto refused = [&](const std::map<std::string, bool>& flags, std::string_view what, const WorldState* base = nullptr) {
        const std::string text = P503V13(w, base != nullptr ? *base : seed, flags);
        const std::string copy = text;
        P2CExpect(P503ErrorOf(text) == WorldSaveError::InvalidWorld && text == copy, what);
    };
    const auto at = std::to_string(mentor.At);
    refused({{mentorKey + ".book." + at, true}, {mentorKey + ".book." + later, true}}, "two live reservations refuse");
    refused({{mentorKey + ".book." + at, true}, {mentorKey + ".done." + at, false}}, "a false attendance refuses");
    refused({{mentorKey + ".book." + at, false}, {mentorKey + ".done." + at, true}}, "an attendance without its reservation refuses");
    refused({{mentorKey + ".book." + later, true}, {mentorKey + ".done." + at, true}}, "a reservation besides an attendance refuses");
    refused({{mentorKey + ".book." + later, true}, {mentorKey + ".done." + later, true}}, "an attendance after the clock refuses");
    refused({{mentorKey + ".p502.s0.revision.1", true}}, "provisional P5-02 task flags refuse instead of being guessed");
    refused({{mentorKey + ".book.0" + at, true}}, "a non-canonical instant refuses");
    refused({{prefix + "forged", true}}, "an unknown reserved key refuses");
    refused({{prefix + "school_followup.p2.x.book." + at, true}}, "a non-appointment id refuses");
    refused({{mentorKey + ".book." + at, true}}, "an undecided world cannot own a follow-up", &w.Base.Morning);
    // v14 relabelling: the flags are no longer a legal spelling at all.
    P2CExpect(P503ErrorOf(P503V13(w, seed, legacy, 14)) == WorldSaveError::InvalidWorld,
        "a v14 world holding reserved follow-up flags is refused");
    WorldState carrying = seed;
    carrying.Flags[mentorKey + ".book." + at] = true;
    P2CExpect(P402SaveText(w.Base, carrying).empty(), "a live world still holding reserved flags is never written");
    // v13 never wrote records: a /3 examination under a v13 label is refused.
    std::string forged = P402SaveText(w.Base, continued.State());
    forged = P503Replace(forged, "\"schemaVersion\": 14,", "\"schemaVersion\": 13,");
    P2CExpect(forged.find("pw.examination/3") != std::string::npos && P503ErrorOf(forged) == WorldSaveError::InvalidWorld,
        "a v13 label cannot smuggle in follow-up records");
}

/** Book, arrive, start and leave the mentor task: the interrupted state every round trip starts from. */
WorldSim P503Interrupted(const P405World& w, const WorldState& seed)
{
    auto sim = w.Sim(seed);
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book), "book the mentor task");
    P503Reach(sim, P407URow(sim, P407UMentor).At, "reach the mentor task");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Attend), "arrive");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::P502Start), "start");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::P502Leave), "leave mid-task");
    return sim;
}

void P503RoundTrips(const P405World& w)
{
    const WorldState seed = P407UWithdrawn(w).State();
    // Uninterrupted.
    auto straight = P503Interrupted(w, seed);
    S04Accept(straight, P407UCommand(straight, P407UFollowupAction::P502Resume), "resume without saving");
    S04Accept(straight, P503Resolve(straight), "resolve without saving");

    // Interrupted by Continue in a fresh process while paused, and again while active.
    auto paused = P503Interrupted(w, seed);
    const auto oldResume = P407UCommand(paused, P407UFollowupAction::P502Resume);
    const auto pausedText = P402SaveText(w.Base, paused.State());
    P2CExpect(pausedText.find("pw.examination/3") != std::string::npos, "an open task is saved as typed records");
    auto reloaded = w.Sim(P402Reload(w.Base, paused.State(), "a paused task survives save and load verified"));
    const auto row = P407URow(reloaded, P407UMentor);
    P2CExpect(row.P502Stage == P502TaskStage::Paused && row.P502CanResume && row.P502Revision == oldResume.P502ExpectedRevision &&
        row.At == oldResume.ExpectedAt, "the reloaded task has its exact stage, slot and revision");
    const WorldState snapshot = reloaded.State();
    S04Accept(reloaded, oldResume, "a disclosure read before saving still matches after Continue");
    auto active = w.Sim(P402Reload(w.Base, reloaded.State(), "an active task survives save and load verified"));
    S04Accept(active, P503Resolve(active), "resolve after the second Continue");

    P2CExpect(HashWorldState(active.State()) == HashWorldState(straight.State()) &&
        active.State().ExaminationFollowups == straight.State().ExaminationFollowups &&
        SerialiseReceipts(active.State().DayLoop.Receipts) == SerialiseReceipts(straight.State().DayLoop.Receipts) &&
        active.State().Clock.Now() == straight.State().Clock.Now(),
        "equivalent reload and uninterrupted runs give equal records, receipts, time and hash");

    // Fresh binding and replay: the uninterrupted log from the seed, and the post-Continue log from its own snapshot.
    const auto replay = ReplayCommandLogFromSnapshot(seed, straight.Log(), w.Replay());
    P2CExpect(replay.Complete && replay.Hash == HashWorldState(straight.State()), "the task log replays from a fresh binding");
    auto rebound = w.Sim(snapshot);
    for (const auto& command : reloaded.Log()) S04Accept(rebound, command.Payload, "rebound replay accepts the same command");
    P2CExpect(HashWorldState(rebound.State()) == HashWorldState(reloaded.State()), "a fresh binding of the loaded world replays identically");

    // Duplicate completion: live, after Continue, and through a forged save.
    S04Refused(straight, P503Resolve(straight), "p502.completed", "a second resolution changes nothing");
    auto done = w.Sim(P402Reload(w.Base, straight.State(), "the completed task survives save and load verified"));
    const auto completed = P407URow(done, P407UMentor);
    P2CExpect(completed.P502Stage == P502TaskStage::Completed && completed.P502Record &&
        completed.P502Record->Id == completed.P502Choices.front().Id, "Continue shows the snapshotted record");
    S04Refused(done, P407UCommand(done, P407UFollowupAction::Book), "p502.completed", "a completed task cannot rebook a reward");
    const auto hashBefore = HashWorldState(done.State());
    S04Refused(done, P407UFollowupCommand{P407UFollowupAction::P502Resolve, std::string(P407UMentor), completed.At,
        completed.P502Choices.back().Id, completed.P502Revision}, "p502.completed", "no second choice after Continue");
    P2CExpect(HashWorldState(done.State()) == hashBefore, "refused duplicates leave the world unchanged");

    // Forged records are never written, and a damaged text never loads.
    const auto& good = straight.State();
    const auto forged = [&](auto mutate, std::string_view what) {
        WorldState bad = good;
        mutate(bad.ExaminationFollowups);
        P2CExpect(!P503FollowupsValidInWorld(bad) && SerialiseExamState(bad).empty() && P402SaveText(w.Base, bad).empty(), what);
    };
    const auto mentorRecord = [](std::vector<P503FollowupRecord>& records) -> P503FollowupRecord& {
        for (auto& r : records)
            if (r.Completion) return r;
        P2CExpect(false, "the completed record exists");
        return records.front();
    };
    forged([&](auto& r) { auto& m = mentorRecord(r); m.Sessions.push_back(m.Sessions.back()); m.Sessions.back().Generation = ++m.Revision; },
        "a second completed session is refused");
    forged([&](auto& r) { mentorRecord(r).Completion->ChoiceId = "admission.money"; }, "a forged choice is refused");
    forged([&](auto& r) { mentorRecord(r).Completion.reset(); }, "a completed session without its completion is refused");
    forged([&](auto& r) { mentorRecord(r).Completion->Records.clear(); }, "a completion without records is refused");
    forged([&](auto& r) { mentorRecord(r).Completion->CompletedAt = good.Clock.Now() + 1; }, "a future completion is refused");
    forged([&](auto& r) { auto& m = mentorRecord(r); m.Revision = m.Sessions.back().Generation - 1; }, "a revision behind its sessions is refused");
    forged([&](auto& r) { auto& m = mentorRecord(r); m.Sessions.front().State = P503SessionState::Booked; },
        "an open session that is not the newest is refused");
    forged([&](auto& r) { mentorRecord(r).Sitting = 1; }, "a record for a sitting not yet held is refused");
    forged([&](auto& r) { mentorRecord(r).AppointmentId = "school_followup.p2.withdrawn"; }, "a non-appointment record is refused");
    forged([&](auto& r) { auto copy = mentorRecord(r); r.push_back(copy); }, "a duplicate record (and repeat reward) is refused");
    forged([&](auto& r) { auto& m = mentorRecord(r); m.Sessions.front().At = P409SittingAt(0) - 1; }, "a session before its sitting is refused");

    const std::string text = P402SaveText(w.Base, good);
    const auto examAt = text.find("pw.examination/3");
    P2CExpect(examAt != std::string::npos, "the completed world saves with typed records");
    for (std::size_t cut = 0; cut < text.size(); cut += (cut + 1500 > examAt && cut < text.size() - 0) ? 1 : 17)
        P2CExpect(P503ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "every truncation of a follow-up save is refused");
    const auto session = text.find("session=", examAt);
    const auto lineEnd = text.find("\\n", session);
    std::string badState = text;
    badState.replace(lineEnd - 1, 1, "9");
    P2CExpect(P503ErrorOf(badState) == WorldSaveError::InvalidWorld, "an unknown session state is refused");
    std::string padded = text;
    padded.insert(text.find(",", session) + 1, "0");
    P2CExpect(P503ErrorOf(padded) == WorldSaveError::InvalidWorld, "a non-canonical session number is refused");
    std::string reordered = text;
    reordered = P503Replace(reordered, "followups=", "followups=0");
    P2CExpect(P503ErrorOf(reordered) == WorldSaveError::InvalidWorld, "a damaged record count is refused");
    S04Refused(done, SetWorldFlag{std::string(P407UFollowupFlagPrefix) + "appointment.p2.p4_withdrawn_school_mentor.book.1", true},
        "flag.reserved", "generic flags cannot forge follow-up authority");
}

void P503SecondSitting(const P405World& w)
{
    auto sim = w.Sim(w.Base.Morning);
    S04Accept(sim, P402Cmd(TowerSessionAction::Defer), "defer the single sitting");
    const auto rows = sim.P407UFollowups();
    const auto task = std::find_if(rows.begin(), rows.end(), [](const auto& r) { return !r.P502Choices.empty(); });
    const auto common = std::find_if(rows.begin(), rows.end(), [](const auto& r) {
        // P503: not at school; the authored school calendar has no attendance day in 2044 (reported, not hidden here).
        return r.P502Choices.empty() && r.Action.Outcome == "all" && r.Action.Destination != "place.school" && r.CanBook; });
    P2CExpect(task != rows.end() && common != rows.end(), "the deferred week has a task and a shared appointment");
    const std::string taskId = task->Action.Id;
    const std::string commonId = common->Action.Id;
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book, commonId), "book a shared appointment in the deferred week");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book, taskId), "book the deferred task");
    P503Reach(sim, P407URow(sim, taskId).At, "reach the deferred task");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Attend, taskId), "arrive at the deferred task");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::P502Start, taskId), "start the deferred task");
    S04Accept(sim, P503Resolve(sim, taskId), "complete the deferred task");
    const auto firstRecords = sim.State().ExaminationFollowups;

    for (int guard = 0; guard < 8 && sim.State().Clock.Now() < P409SittingAt(1); ++guard)
        S04Accept(sim, AdvanceTime{P409SittingAt(1) - sim.State().Clock.Now()}, "a year passes toward the deferred sitting");
    P2CExpect(sim.State().Clock.Now() == P409SittingAt(1), "the deferred candidate reaches the next sitting");
    S04Accept(sim, P402Cmd(TowerSessionAction::Decline), "decline the deferred sitting");
    P2CExpect(P409CurrentSitting(sim.State()) == 1 && sim.ExaminationResult().Outcome == "declined", "sitting 1 holds the result");

    const auto second = P407URow(sim, commonId);
    P2CExpect(!second.Booked && !second.Attended && second.P502Revision == 0 && second.CanBook &&
        second.At >= GameClock::StartOfDay(P409SittingAt(1)),
        "the shared appointment starts fresh for sitting 1 and schedules from that sitting");
    const auto rows1 = sim.P407UFollowups();
    const auto task1 = std::find_if(rows1.begin(), rows1.end(), [](const auto& r) { return !r.P502Choices.empty(); });
    P2CExpect(task1 != rows1.end() && task1->Action.Id != taskId && task1->P502Stage == P502TaskStage::Offered &&
        task1->P502Revision == 0 && !task1->Booked, "the declined task is offered fresh at the second sitting");
    P2CExpect(std::none_of(rows1.begin(), rows1.end(), [&](const auto& r) { return r.Action.Id == taskId; }),
        "the deferred week's task does not reopen");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book, commonId), "book the shared appointment for sitting 1");
    const auto& records = sim.State().ExaminationFollowups;
    P2CExpect(records.size() == firstRecords.size() + 1 &&
        std::equal(firstRecords.begin(), firstRecords.end(), records.begin()) &&
        records.back().Sitting == 1 && records.back().AppointmentId == common->Action.AppointmentId &&
        records.back().Sessions.size() == 1 && records.back().Sessions[0].Generation == 1,
        "the second-sitting booking is a new record; sitting 0's history and completion are untouched");
    const auto text = P402SaveText(w.Base, sim.State());
    P2CExpect(text.find("pw.examination/3\\nsittings=1\\n") != std::string::npos, "the save names the earlier sitting and its records");
    auto loaded = w.Sim(P402Reload(w.Base, sim.State(), "two sittings of follow-ups survive save and load verified"));
    P2CExpect(loaded.State().ExaminationFollowups == records && P407URow(loaded, commonId).Booked, "Continue keeps both sittings");
    // The deferred week's completion is still the only one; a forged second completion of it in sitting 1 is refused.
    WorldState repeat = loaded.State();
    auto copy = *std::find_if(repeat.ExaminationFollowups.begin(), repeat.ExaminationFollowups.end(),
        [](const auto& r) { return r.Completion.has_value(); });
    copy.Sitting = 1;
    for (auto& session : copy.Sessions) session.At = P409SittingAt(1);
    copy.Completion->CompletedAt = P409SittingAt(1);
    P2CExpect(repeat.Clock.Now() >= P409SittingAt(1), "the forged instants are not in the future");
    repeat.ExaminationFollowups.push_back(copy);
    std::sort(repeat.ExaminationFollowups.begin(), repeat.ExaminationFollowups.end(),
        [](const auto& a, const auto& b) { return std::tie(a.Sitting, a.AppointmentId) < std::tie(b.Sitting, b.AppointmentId); });
    P2CExpect(!P503FollowupsValidInWorld(repeat) && P402SaveText(w.Base, repeat).empty(),
        "a task completed after one sitting cannot be rewarded again after another");
    for (auto& record : repeat.ExaminationFollowups)
        if (record.Sitting == 1 && record.Completion)
        {
            record.Completion.reset();
            record.Sessions.back().State = P503SessionState::Cancelled;
        }
    P2CExpect(P503FollowupsValidInWorld(repeat), "the same second-sitting history without the repeat reward is valid");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root");
    const P2CFixture fixture(argv[1]);
    P503Content = &fixture.Content;
    const P402World base(fixture);
    const P405World real(base, fixture.Content);
    P407UOutcomes(real);
    P407USession(real);
    P407USlotsAndRollback(real);
    P502RecoveryAndConflicts(real);
    P502ReceiptsAndReplay(real);
    P503Legacy(real);
    P503RoundTrips(real);
    P503SecondSitting(real);
    std::cout << "P407U follow-up assertions passed\n";
    return 0;
}
