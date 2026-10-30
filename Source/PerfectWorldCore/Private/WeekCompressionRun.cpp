#include "WeekCompression.h"

#include "WeekCompressionInternal.h"
#include "WorldReducer.h"

#include <algorithm>
#include <map>
#include <utility>

// The two resolvers (ROADMAP M3-02). One engine runs a plan's pieces through ApplyDayCommand and
// keeps the world clock in step through the reducer; the only differences between the two ways
// are how finely a span of time is spent and whether firsts and uncovered choices stop it.
namespace PerfectWorld
{
namespace
{
DayCommand WeekRunSimple(DayCommandKind kind, std::int64_t minutes = 0)
{
    DayCommand command;
    command.Kind = kind;
    command.Minutes = minutes;
    return command;
}

class WeekRunEngine
{
public:
    WeekRunEngine(const WorldState& world, const WeekPlan& plan, const DayLoopCatalogues& cats, std::int64_t chunk,
                  bool compressing, std::int32_t limit)
        : Input_(world), World_(world), Plan_(plan), Cats_(cats), Chunk_(std::clamp<std::int64_t>(chunk, 1, WeekCompressedChunkMinutes)),
          Compressing_(compressing), Limit_(limit)
    {
    }

    WeekResult Run();

private:
    bool Fail(WeekError error, const WeekUnit& unit, std::string detail);
    bool Apply(const WeekUnit& unit, const DayCommand& command, DayLoopResult* out = nullptr);
    bool SyncClock(const WeekUnit& unit);
    bool Spend(const WeekUnit& unit, std::int64_t minutes, DayPassKind kind);
    bool RunUnit(const WeekUnit& unit);
    bool RunActivity(const WeekUnit& unit, std::int64_t& value);
    bool RunSchool(const WeekUnit& unit, std::int64_t& value);
    WeekStop StopBefore(const WeekEntry& entry) const;
    void Finish(const WeekUnits& units);

    const WorldState& Input_;
    WorldState World_;
    const WeekPlan& Plan_;
    const DayLoopCatalogues& Cats_;
    std::int64_t Chunk_;
    bool Compressing_;
    std::int32_t Limit_;
    WeekResult Result_;
    std::map<std::string, std::int64_t> Credits_;
    std::map<Attribute, std::int64_t> Evidence_;
};

bool WeekRunEngine::Fail(WeekError error, const WeekUnit& unit, std::string detail)
{
    Result_ = WeekResult{};
    Result_.Issue = WeekMakeIssue(error, unit.Day, unit.Entry, std::move(detail));
    Result_.World = Input_;
    return false;
}

bool WeekRunEngine::SyncClock(const WeekUnit& unit)
{
    const GameInstant target = World_.DayLoop.Now;
    if (target == World_.Clock.Now())
        return true;
    if (target < World_.Clock.Now())
        return Fail(WeekError::ClockRefused, unit, "the day is behind the world clock");
    // The reducer fires whatever the clock passes (scheduled events, Gate onsets). It only needs the day
    // to be idle, which it is: the command that moved the day has completed or ended.
    WorldSim sim(std::move(World_));
    while (sim.State().Clock.Now() < target)
    {
        const CommandResult moved = sim.Submit({CommandSource::System, AdvanceTime{target - sim.State().Clock.Now()}});
        if (!moved.Accepted)
        {
            World_ = sim.State();
            return Fail(WeekError::ClockRefused, unit, moved.Why.Code);
        }
        ++Result_.ClockAdvances;
    }
    World_ = sim.State();
    if (!IsWorldDayInStep(World_))
        return Fail(WeekError::ClockRefused, unit, "the day and the world clock disagree after the advance");
    return true;
}

bool WeekRunEngine::Apply(const WeekUnit& unit, const DayCommand& command, DayLoopResult* out)
{
    DayLoopResult applied = ApplyDayCommand(World_.DayLoop, Cats_, command);
    if (!applied.Ok())
    {
        std::string detail = std::string(DayCommandName(command.Kind)) + ": " + std::string(DayLoopErrorCode(applied.Error));
        if (!applied.Detail.empty())
            detail += " (" + applied.Detail + ")";
        return Fail(WeekError::DayCommandRefused, unit, std::move(detail));
    }
    ++Result_.DayCommands;
    if (out != nullptr)
        *out = std::move(applied);
    return SyncClock(unit);
}

bool WeekRunEngine::Spend(const WeekUnit& unit, std::int64_t minutes, DayPassKind kind)
{
    for (std::int64_t left = minutes; left > 0;)
    {
        const std::int64_t step = std::min(left, Chunk_);
        DayCommand pass = WeekRunSimple(DayCommandKind::Pass, step);
        pass.Pass = kind;
        if (!Apply(unit, pass))
            return false;
        left -= step;
    }
    return true;
}

bool WeekRunEngine::RunActivity(const WeekUnit& unit, std::int64_t& value)
{
    const WeekEntry& entry = *unit.Source;
    DayCommand commit;
    commit.Kind = DayCommandKind::Commit;
    commit.ActivityId = entry.ActivityId;
    commit.PlaceKind = entry.PlaceKind;
    commit.Alternatives = entry.Alternatives;
    DayLoopResult done;
    if (!Apply(unit, commit) || !Apply(unit, WeekRunSimple(DayCommandKind::Start)) ||
        !Apply(unit, WeekRunSimple(DayCommandKind::Complete), &done))
        return false;
    for (const DayLoopEvent& event : done.Events)
    {
        if (event.Kind != DayLoopEventKind::Completed)
            continue;
        for (const SkillCredit& credit : event.Credited)
        {
            Credits_[credit.SkillId] += credit.MilliXp;
            value += credit.MilliXp;
        }
        for (const EvidenceCredit& credit : event.Evidence)
            Evidence_[credit.Attr] += credit.Milli;
    }
    return true;
}

bool WeekRunEngine::RunSchool(const WeekUnit& unit, std::int64_t& value)
{
    DayCommand arrive = WeekRunSimple(DayCommandKind::ArriveSchool);
    arrive.DepartureMinute = unit.Source->DepartureMinute;
    DayLoopResult done;
    if (!Apply(unit, arrive, &done))
        return false;
    for (const DayLoopEvent& event : done.Events)
    {
        if (event.Kind == DayLoopEventKind::ArrivedAtSchool)
            value = static_cast<std::int64_t>(event.Attendance);
    }
    return true;
}

bool WeekRunEngine::RunUnit(const WeekUnit& unit)
{
    if (unit.OpensDay)
    {
        DayCommand plan = WeekRunSimple(DayCommandKind::PlanDay);
        // The activities still to come that calendar day, from this entry on.
        const std::vector<WeekEntry>& entries = Plan_.Days[static_cast<std::size_t>(unit.Day)].Entries;
        for (std::size_t i = static_cast<std::size_t>(unit.Entry); i < entries.size(); ++i)
        {
            if (entries[i].Kind == WeekEntryKind::Activity)
                plan.Plan.push_back({entries[i].StartMinute, entries[i].ActivityId});
        }
        if (!Apply(unit, plan))
            return false;
    }
    const GameInstant before = World_.DayLoop.Now;
    std::int64_t value = 0;
    bool ok = false;
    switch (unit.Kind)
    {
    case WeekUnitKind::Gap: ok = Spend(unit, unit.Minutes, DayPassKind::Wait); break;
    case WeekUnitKind::Spend: ok = Spend(unit, unit.Minutes, unit.Source->Pass); break;
    case WeekUnitKind::Activity: ok = RunActivity(unit, value); break;
    case WeekUnitKind::School: ok = RunSchool(unit, value); break;
    case WeekUnitKind::Sleep:
        ok = Apply(unit, WeekRunSimple(DayCommandKind::GoToSleep)) && Apply(unit, WeekRunSimple(DayCommandKind::Wake, unit.Minutes));
        break;
    }
    if (!ok)
        return false;
    const std::int64_t charged = World_.DayLoop.Now - before;
    if (charged != unit.Minutes)
        return Fail(WeekError::DayCommandRefused, unit, "the day loop charged a different time than the plan declared");
    if (unit.Minutes > 0)
    {
        // The time contract's own resolution of the same declaration must charge what the day loop spent.
        const TimeResolution declared = ResolveInteraction(WeekUnitDeclaration(unit), {TimeEvent{TimeEventKind::Progress, unit.Minutes}});
        if (!declared.Ok() || declared.Outcome != TimeOutcome::Completed || declared.ChargedMinutes != charged)
            return Fail(WeekError::DayCommandRefused, unit, "the time contract charges this interaction differently from the day loop");
    }

    WeekLedger& ledger = Result_.Ledger;
    ledger.ChargedByMode[static_cast<std::size_t>(WeekUnitMode(unit))] += charged;
    ledger.TotalCharged += charged;
    ledger.Completed += unit.Minutes > 0 ? 1 : 0;
    if (unit.Kind == WeekUnitKind::Gap)
        return true;

    // The entry itself has been done (its gap came first).
    const WeekEntry& entry = *unit.Source;
    ++Result_.EntriesResolved;
    if (entry.DecidedByOrder)
    {
        ledger.DecidedByOrder += 1;
        Result_.Disclosures.push_back({unit.Day, unit.Entry, entry.Kind, WeekEntryWhat(entry), entry.OrderId, unit.Start});
    }
    Result_.Outcomes.push_back({unit.Day, unit.Entry, entry.Kind, WeekEntryWhat(entry), unit.Start, World_.DayLoop.Now, value});
    return true;
}

WeekStop WeekRunEngine::StopBefore(const WeekEntry& entry) const
{
    if (entry.FirstOfKind)
        return WeekStop::FirstNeedsPlay;
    if (entry.NeedsChoice && !entry.DecidedByOrder)
        return Plan_.InterruptsUsed < WeekMaxInterrupts ? WeekStop::ChoiceNeeded : WeekStop::InterruptBudgetSpent;
    return WeekStop::None;
}

void WeekRunEngine::Finish(const WeekUnits& units)
{
    WeekLedger& ledger = Result_.Ledger;
    ledger.StartInstant = units.StartInstant;
    ledger.EndInstant = World_.DayLoop.Now;
    for (const auto& [skill, milli] : Credits_)
        ledger.Credits.push_back({skill, static_cast<std::int32_t>(milli)});
    for (const auto& [attribute, milli] : Evidence_)
        ledger.Evidence.push_back({attribute, static_cast<std::int32_t>(milli)});
    Result_.InterruptsUsed = Plan_.InterruptsUsed + (Result_.Stop == WeekStop::ChoiceNeeded ? 1 : 0);
    Result_.World = std::move(World_);
}

WeekResult WeekRunEngine::Run()
{
    if (const WeekPlanIssue issue = ValidateWeekPlan(Input_, Plan_, Cats_); !issue.Ok())
    {
        Result_.Issue = issue;
        Result_.World = Input_;
        return std::move(Result_);
    }
    const WeekUnits units = WeekBuildUnits(Input_, Plan_, Cats_);
    std::int32_t entering = -1;
    for (const WeekUnit& unit : units.Units)
    {
        if (Limit_ >= 0 && unit.Flat >= Limit_)
            break;
        if (unit.Flat != entering)
        {
            entering = unit.Flat;
            const WeekStop stop = Compressing_ ? StopBefore(*unit.Source) : WeekStop::None;
            if (stop != WeekStop::None)
            {
                Result_.Stop = stop;
                Result_.StopDay = unit.Day;
                Result_.StopEntry = unit.Entry;
                break;
            }
        }
        if (!RunUnit(unit))
            return std::move(Result_);
    }
    Finish(units);
    return std::move(Result_);
}
} // namespace

WeekResult CompressWeek(const WorldState& world, const WeekPlan& plan, const DayLoopCatalogues& catalogues)
{
    return WeekRunEngine(world, plan, catalogues, WeekCompressedChunkMinutes, true, -1).Run();
}

WeekResult PlayWeekStepByStep(const WorldState& world, const WeekPlan& plan, const DayLoopCatalogues& catalogues,
                              const WeekStepOptions& options)
{
    return WeekRunEngine(world, plan, catalogues, options.ChunkMinutes, false, options.EntryLimit).Run();
}
} // namespace PerfectWorld
