#include "WorldReducer.h"

#include "SchoolLessonContent.h"

#include <utility>

// S04: school.work.apply and school.week.* through WorldSim (guards in WorldSchool.cpp). A paper reserves its 90-minute
// block at Begin (every time check a charge needs must pass then), keeps the world at StartedAt while it is played, and is
// charged once at Submit or Cancel through ChargeTime. The week compresses on a copy, then commits its day loop and lesson
// sessions while the world clock is moved by this sim's own ChargeTime, so events are published, allowance settles once at
// Submit's commit gate and Seq counts this one command.
namespace PerfectWorld
{
namespace
{
Rejection S04WRefusal(std::string code, std::string message)
{
    return Rejection{std::move(code), std::move(message)};
}

Rejection S04WFromIssue(const SchoolIssue& issue)
{
    return Rejection{issue.Code, issue.Detail.empty() ? std::string("The school rules refused that action.") : issue.Detail};
}

/** S04 seam, filled by R9-04: the one growth owner's Awakening.Stats.Skills as (SkillId, LevelMilli); none while unseeded. */
AssessmentSkills S04WorkSkills(const WorldState& world)
{
    AssessmentSkills skills;
    skills.reserve(world.Awakening.Stats.Skills.size());
    for (const SkillLevel& skill : world.Awakening.Stats.Skills)
        skills.push_back({skill.SkillId, skill.LevelMilli});
    return skills;
}

bool S04SameClock(const GameClock& a, const GameClock& b)
{
    const GameClockSave left = a.CaptureSave();
    const GameClockSave right = b.CaptureSave();
    if (left.Now != right.Now || left.NextHandle != right.NextHandle || left.Pending.size() != right.Pending.size())
        return false;
    for (std::size_t index = 0; index < left.Pending.size(); ++index)
    {
        const ScheduledEvent& x = left.Pending[index];
        const ScheduledEvent& y = right.Pending[index];
        if (x.Handle != y.Handle || x.When != y.When || x.Id != y.Id)
            return false;
    }
    return true;
}

/** The world consequences of the compressed week (clock, flags, Gates) must be what this sim's own charge produced. */
bool S04SameConsequences(const WorldState& charged, const WorldState& compressed)
{
    if (!S04SameClock(charged.Clock, compressed.Clock) || charged.Flags != compressed.Flags ||
        charged.Gates.size() != compressed.Gates.size())
        return false;
    auto other = compressed.Gates.begin();
    for (const auto& [id, gate] : charged.Gates)
    {
        if (id != other->first || gate.Rank != other->second.Rank || gate.Status != other->second.Status ||
            gate.StatusSince != other->second.StatusSince || gate.OnsetCount != other->second.OnsetCount)
            return false;
        ++other;
    }
    return true;
}
} // namespace

std::optional<Rejection> WorldSim::ApplySchoolWorkCommand(const SchoolWorkCommand& command, CommandResult& result)
{
    if (command.Kind == SchoolWorkActionKind::Begin)
    {
        // The reservation: the whole paper block must be chargeable now (awake, no activity, no counter, inside the calendar).
        if (std::optional<Rejection> refusal = ValidateTimeCharge(AssessmentPaperMinutes))
            return refusal;
    }
    SchoolWorkState work = State_.SchoolWork;
    SchoolWorkResult applied = ApplySchoolWork(work, *Lessons_.Papers, command, S04WorkSkills(State_), State_.Clock.Now());
    if (!applied.Ok())
        return S04WFromIssue(applied.Issue);
    if (applied.WorldMinutes > 0)
    {
        if (std::optional<Rejection> refusal = ValidateTimeCharge(applied.WorldMinutes))
            return refusal;
        const GameInstant target = State_.Clock.Now() + applied.WorldMinutes;
        ChargeTime(applied.WorldMinutes, result);
        if (result.Partial || State_.Clock.Now() != target)
            return S04WRefusal("school.work.charge_interrupted", "Settle the scheduled time before handing the paper in.");
    }
    State_.SchoolWork = std::move(work);
    StagedWork_ = std::move(applied);
    return std::nullopt;
}

std::optional<Rejection> WorldSim::ApplySchoolWeekCommand(const Command& command, CommandResult& result)
{
    if (const auto* order = std::get_if<SchoolWeekOrder>(&command.Payload))
    {
        SchoolWeekState week = State_.SchoolWeek;
        const SchoolIssue issue = SetSchoolStandingOrder(week, order->Order, order->Remove);
        if (!issue.Ok())
            return S04WFromIssue(issue);
        State_.SchoolWeek = std::move(week);
        return std::nullopt;
    }
    const auto* resolve = std::get_if<SchoolWeekResolve>(&command.Payload);
    if (resolve == nullptr)
        return S04WRefusal("command.unknown", "Unknown command.");
    const SchoolSessionCatalogues cats = SchoolCatalogues();
    const std::vector<SchoolLessonBinding> bindings = SchoolLessonBindings(*Lessons_.Lessons, School_.School->Stream);
    const SchoolWeekBuild built = BuildSchoolWeekPlan(State_, State_.SchoolWeek, State_.SchoolSessions, resolve->Base, bindings, cats);
    if (!built.Ok())
        return S04WFromIssue(built.Issue);
    SchoolWeekResult compressed = CompressSchoolWeek(State_, State_.SchoolSessions, built.Plan, cats);
    if (!compressed.Ok())
        return S04WFromIssue(compressed.Issue);
    const GameInstant now = State_.Clock.Now();
    const GameInstant end = compressed.World.Clock.Now();
    if (end < now || compressed.World.DayLoop.Now != end)
        return S04WRefusal("school.invalid_state", "The compressed week left the day and the clock apart.");
    // R9-04: the compressed copy cannot open the countdown (its helper sims are unbound), so a routed week may not reach the
    // 18:00 opening on Awakening Day: resolve up to the evening, let it open, then plan the ten days.
    if (CountdownRouted() && State_.CountdownText.empty() && now < CountdownStartInstant() && end >= CountdownStartInstant())
        return S04WRefusal(std::string(CountdownErrorCode(CountdownError::NotAtStart)),
                           "The week reaches the countdown's 18:00 opening; resolve the days before Awakening Day evening first.");
    if (end != now)
    {
        ChargeTime(end - now, result);
        if (result.Partial)
            return S04WRefusal("school.week.charge_interrupted", "Settle the scheduled time before resolving the week.");
    }
    if (!S04SameConsequences(State_, compressed.World))
        return S04WRefusal("school.week.world_mismatch", "The week's world events differ from the world clock's own.");
    // R9-04: the week's per-instance completions grow the one owner exactly once (never the segments' totals as well).
    if (std::optional<Rejection> refusal = CreditActivities(compressed.ActivityEvents))
        return refusal;
    // Only the day and the sessions come from the copy; commerce settles at the commit gate over the same interval.
    State_.DayLoop = compressed.World.DayLoop;
    State_.SchoolSessions = compressed.Sessions;
    // R9's ApplyActivityCredits input: the raw per-instance events, never the segments' aggregate credits as well.
    for (const DayLoopEvent& event : compressed.ActivityEvents)
        result.Events.push_back({0, event.At, event});
    StagedWeek_ = std::move(compressed);
    return std::nullopt;
}
} // namespace PerfectWorld
