#include "WeekCompression.h"

#include "WeekCompressionInternal.h"

#include <algorithm>
#include <utility>

// The week plan: validation, the pieces a week is resolved in, and the week as a time-contract
// TimePlan (ROADMAP M3-02). Pure.
namespace PerfectWorld
{
namespace
{
ClockMode WeekPlanModeOf(ActivityClockMode mode)
{
    switch (mode)
    {
    case ActivityClockMode::FreeRoam: return ClockMode::FreeRoam;
    case ActivityClockMode::Scene: return ClockMode::Scene;
    case ActivityClockMode::Mission: return ClockMode::Mission;
    case ActivityClockMode::Compressed: return ClockMode::Compressed;
    }
    return ClockMode::Scene;
}

InterruptPolicy WeekPlanInterruptOf(ActivityInterrupt interrupt)
{
    switch (interrupt)
    {
    case ActivityInterrupt::None: return InterruptPolicy::None;
    case ActivityInterrupt::AtBeat: return InterruptPolicy::AtBeat;
    case ActivityInterrupt::Immediate: return InterruptPolicy::Immediate;
    }
    return InterruptPolicy::AtBeat;
}

std::string_view WeekPlanSpendId(DayPassKind kind)
{
    switch (kind)
    {
    case DayPassKind::Wait: return "week.wait";
    case DayPassKind::Rest: return "week.rest";
    case DayPassKind::Walk: return "week.walk";
    }
    return "week.wait";
}

bool WeekPlanPassKindValid(DayPassKind kind)
{
    return kind == DayPassKind::Wait || kind == DayPassKind::Rest || kind == DayPassKind::Walk;
}

/** Appends the awake time between the cursor and `start` as gap pieces of at most a day. */
void WeekPlanAddGaps(std::vector<WeekUnit>& units, const WeekEntry& entry, std::int32_t day, std::int32_t index,
                     std::int32_t flat, bool& opensDay, GameInstant cursor, GameInstant start)
{
    for (GameInstant at = cursor; at < start;)
    {
        const std::int64_t minutes = std::min<std::int64_t>(start - at, WeekCompressedChunkMinutes);
        WeekUnit gap;
        gap.Kind = WeekUnitKind::Gap;
        gap.Day = day;
        gap.Entry = index;
        gap.Flat = flat;
        gap.OpensDay = opensDay;
        gap.Start = at;
        gap.Minutes = minutes;
        gap.Source = &entry;
        units.push_back(gap);
        opensDay = false;
        at += minutes;
    }
}

/** Checks one entry's own fields. Returns an issue with Error None when sound. */
WeekPlanIssue WeekPlanCheckEntry(const WeekEntry& entry, const DayLoopCatalogues& cats, std::int32_t day, std::int32_t index,
                                 const ActivityDef*& def)
{
    def = nullptr;
    if (entry.StartMinute < 0 || entry.StartMinute >= MinutesPerDay)
        return WeekMakeIssue(WeekError::BadMinute, day, index, "the start minute is outside the day");
    if ((entry.NeedsChoice && entry.DecidedByOrder) || entry.DecidedByOrder != !entry.OrderId.empty() ||
        (!entry.OrderId.empty() && !IsReceiptIdWellFormed(entry.OrderId)))
        return WeekMakeIssue(WeekError::BadFlags, day, index,
                             "a standing order needs a well-formed order id, and a choice an order covers is not left open");
    switch (entry.Kind)
    {
    case WeekEntryKind::Activity:
    {
        def = cats.Activities->Find(entry.ActivityId);
        if (def == nullptr)
            return WeekMakeIssue(WeekError::UnknownActivity, day, index, entry.ActivityId);
        if (!IsReceiptIdWellFormed(entry.PlaceKind))
            return WeekMakeIssue(WeekError::BadEntry, day, index, "the place kind is not a token");
        if (def->Place != ActivityAnyPlace && def->Place != entry.PlaceKind)
            return WeekMakeIssue(WeekError::WrongPlace, day, index, entry.ActivityId + " is done in a " + def->Place);
        if (entry.Alternatives.size() > DayLoopMaxAlternatives)
            return WeekMakeIssue(WeekError::BadEntry, day, index, "too many alternatives");
        for (const ReceiptCandidate& candidate : entry.Alternatives)
        {
            if (!IsReceiptCandidateValid(candidate))
                return WeekMakeIssue(WeekError::BadEntry, day, index, "an alternative is not valid");
        }
        break;
    }
    case WeekEntryKind::School:
        if (cats.School == nullptr)
            return WeekMakeIssue(WeekError::NoSchool, day, index, "there is no bell schedule");
        if (entry.DepartureMinute < 0 || entry.DepartureMinute > AttendanceStaysUntilDismissal)
            return WeekMakeIssue(WeekError::BadEntry, day, index, "the departure minute is outside the day");
        break;
    case WeekEntryKind::Sleep:
        if (entry.Minutes < 1 || entry.Minutes > DayLoopMaxSleepMinutes)
            return WeekMakeIssue(WeekError::BadEntry, day, index, "sleep lasts 1 to 1440 minutes");
        break;
    case WeekEntryKind::Spend:
        if (entry.Minutes < 1 || entry.Minutes > DayLoopMaxPassMinutes || !WeekPlanPassKindValid(entry.Pass))
            return WeekMakeIssue(WeekError::BadEntry, day, index, "a spend needs a known kind and 1 to 1440 minutes");
        break;
    default:
        return WeekMakeIssue(WeekError::BadEntry, day, index, "unknown entry kind");
    }
    return {};
}

WeekUnitKind WeekPlanUnitKindOf(WeekEntryKind kind)
{
    switch (kind)
    {
    case WeekEntryKind::Activity: return WeekUnitKind::Activity;
    case WeekEntryKind::School: return WeekUnitKind::School;
    case WeekEntryKind::Sleep: return WeekUnitKind::Sleep;
    case WeekEntryKind::Spend: return WeekUnitKind::Spend;
    }
    return WeekUnitKind::Spend;
}
} // namespace

std::string_view WeekEntryKindName(WeekEntryKind kind) noexcept
{
    switch (kind)
    {
    case WeekEntryKind::Activity: return "activity";
    case WeekEntryKind::School: return "school";
    case WeekEntryKind::Sleep: return "sleep";
    case WeekEntryKind::Spend: return "spend";
    }
    return "activity";
}

std::string_view WeekErrorCode(WeekError error) noexcept
{
    switch (error)
    {
    case WeekError::None: return {};
    case WeekError::InvalidWorld: return "week.invalid_world";
    case WeekError::WorldBusy: return "week.world_busy";
    case WeekError::MissingCatalogue: return "week.missing_catalogue";
    case WeekError::NoDays: return "week.no_days";
    case WeekError::TooManyDays: return "week.too_many_days";
    case WeekError::TooManyEntries: return "week.too_many_entries";
    case WeekError::UnknownActivity: return "week.unknown_activity";
    case WeekError::WrongPlace: return "week.wrong_place";
    case WeekError::BadMinute: return "week.bad_minute";
    case WeekError::BadEntry: return "week.bad_entry";
    case WeekError::Overlap: return "week.overlap";
    case WeekError::BadFlags: return "week.bad_flags";
    case WeekError::BadInterruptCount: return "week.bad_interrupt_count";
    case WeekError::NoSchool: return "week.no_school";
    case WeekError::NotRoutine: return "week.not_routine";
    case WeekError::BeyondCalendar: return "week.beyond_calendar";
    case WeekError::DayCommandRefused: return "week.day_command_refused";
    case WeekError::ClockRefused: return "week.clock_refused";
    }
    return {};
}

std::string_view WeekStopName(WeekStop stop) noexcept
{
    switch (stop)
    {
    case WeekStop::None: return "none";
    case WeekStop::FirstNeedsPlay: return "first_needs_play";
    case WeekStop::ChoiceNeeded: return "choice_needed";
    case WeekStop::InterruptBudgetSpent: return "interrupt_budget_spent";
    }
    return "none";
}

WeekPlanIssue WeekMakeIssue(WeekError error, std::int32_t day, std::int32_t entry, std::string detail)
{
    WeekPlanIssue issue;
    issue.Error = error;
    issue.Day = day;
    issue.Entry = entry;
    issue.Detail = std::move(detail);
    return issue;
}

std::string WeekEntryWhat(const WeekEntry& entry)
{
    return entry.Kind == WeekEntryKind::Activity ? entry.ActivityId : std::string(WeekEntryKindName(entry.Kind));
}

ClockMode WeekUnitMode(const WeekUnit& unit)
{
    if (unit.Kind == WeekUnitKind::Activity && unit.Def != nullptr)
        return WeekPlanModeOf(unit.Def->Clock);
    return unit.Kind == WeekUnitKind::Sleep ? ClockMode::Scene : ClockMode::FreeRoam;
}

InteractionDeclaration WeekUnitDeclaration(const WeekUnit& unit)
{
    InteractionDeclaration decl;
    decl.Mode = WeekUnitMode(unit);
    decl.CostMinutes = static_cast<std::int32_t>(unit.Minutes);
    decl.Interrupt = InterruptPolicy::Immediate;
    decl.Resolve = ResolutionBoundary::OnComplete;
    decl.Beats = 1;
    switch (unit.Kind)
    {
    case WeekUnitKind::Gap: decl.Id = "week.gap"; break;
    case WeekUnitKind::Spend: decl.Id = std::string(WeekPlanSpendId(unit.Source->Pass)); break;
    case WeekUnitKind::School: decl.Id = "week.arrive_school"; break; // never planned: it takes no time
    case WeekUnitKind::Sleep: decl.Id = "week.sleep"; break;
    case WeekUnitKind::Activity:
        decl.Id = unit.Def->Id;
        decl.Interrupt = WeekPlanInterruptOf(unit.Def->Interrupt);
        break;
    }
    return decl;
}

WeekUnits WeekBuildUnits(const WorldState& world, const WeekPlan& plan, const DayLoopCatalogues& cats)
{
    WeekUnits built;
    const auto refuse = [&built](WeekPlanIssue issue) {
        built.Issue = std::move(issue);
        built.Units.clear();
        return std::move(built);
    };
    if (!IsWorldDayInStep(world))
        return refuse(WeekMakeIssue(WeekError::InvalidWorld, -1, -1, "the player's day is not in step with the world clock"));
    const DayLoopState& day0 = world.DayLoop;
    if (day0.Asleep || day0.Runner.Active() || day0.HasCommitment)
        return refuse(WeekMakeIssue(WeekError::WorldBusy, -1, -1, "finish the commitment and wake before planning a week"));
    if (cats.Activities == nullptr)
        return refuse(WeekMakeIssue(WeekError::MissingCatalogue, -1, -1, "no activity catalogue"));
    if (plan.Days.empty())
        return refuse(WeekMakeIssue(WeekError::NoDays, -1, -1));
    if (plan.Days.size() > static_cast<std::size_t>(WeekMaxDays))
        return refuse(WeekMakeIssue(WeekError::TooManyDays, -1, -1));
    if (plan.InterruptsUsed < 0 || plan.InterruptsUsed > WeekMaxInterrupts)
        return refuse(WeekMakeIssue(WeekError::BadInterruptCount, -1, -1));

    GameInstant cursor = day0.Now;
    built.StartInstant = cursor;
    const GameInstant firstDayStart = GameClock::StartOfDay(cursor);
    std::int32_t flat = 0;
    // The morning plan must be made before time can be spent: at the start, at each calendar day's first
    // commitment, and at the first commitment after waking (waking forgets the plan).
    bool afterWake = true;
    for (std::size_t d = 0; d < plan.Days.size(); ++d)
    {
        const std::vector<WeekEntry>& entries = plan.Days[d].Entries;
        const std::int32_t day = static_cast<std::int32_t>(d);
        std::size_t activities = 0;
        for (const WeekEntry& entry : entries)
            activities += entry.Kind == WeekEntryKind::Activity ? 1 : 0;
        if (entries.size() > WeekMaxEntriesPerDay || activities > DayLoopMaxPlanItems)
            return refuse(WeekMakeIssue(WeekError::TooManyEntries, day, -1));
        bool opensDay = true;
        for (std::size_t e = 0; e < entries.size(); ++e, ++flat)
        {
            const WeekEntry& entry = entries[e];
            const std::int32_t index = static_cast<std::int32_t>(e);
            const ActivityDef* def = nullptr;
            if (WeekPlanIssue problem = WeekPlanCheckEntry(entry, cats, day, index, def); !problem.Ok())
                return refuse(std::move(problem));
            const GameInstant start = firstDayStart + static_cast<GameInstant>(d) * MinutesPerDay + entry.StartMinute;
            if (start < cursor)
                return refuse(WeekMakeIssue(WeekError::Overlap, day, index, "it starts before the previous commitment ends"));
            bool opens = opensDay || afterWake;
            WeekPlanAddGaps(built.Units, entry, day, index, flat, opens, cursor, start);

            WeekUnit unit;
            unit.Kind = WeekPlanUnitKindOf(entry.Kind);
            unit.Day = day;
            unit.Entry = index;
            unit.Flat = flat;
            unit.OpensDay = opens;
            unit.Start = start;
            unit.Source = &entry;
            unit.Def = def;
            unit.Minutes = entry.Kind == WeekEntryKind::Activity ? def->DurationMinutes
                           : entry.Kind == WeekEntryKind::School ? 0
                                                                  : entry.Minutes;
            opensDay = false;
            afterWake = entry.Kind == WeekEntryKind::Sleep;
            cursor = start + unit.Minutes;
            if (cursor > DayLoopMaxInstant)
                return refuse(WeekMakeIssue(WeekError::BeyondCalendar, day, index));
            built.Units.push_back(unit);
        }
    }
    built.EntryCount = flat;
    built.EndInstant = cursor;
    return built;
}

WeekTimePlanResult BuildWeekTimePlan(const WorldState& world, const WeekPlan& plan, const DayLoopCatalogues& catalogues,
                                     std::int32_t entryLimit)
{
    WeekTimePlanResult result;
    const WeekUnits built = WeekBuildUnits(world, plan, catalogues);
    if (!built.Issue.Ok())
    {
        result.Issue = built.Issue;
        return result;
    }
    result.Plan.Seed = plan.Seed;
    GameInstant end = built.StartInstant;
    for (const WeekUnit& unit : built.Units)
    {
        if (entryLimit >= 0 && unit.Flat >= entryLimit)
            break;
        if (unit.Minutes == 0)
            continue; // arriving at school takes no time: it is not an interaction the contract charges
        PlannedInteraction item;
        item.Decl = WeekUnitDeclaration(unit);
        item.StartMinute = unit.Start - built.StartInstant;
        if (unit.Kind != WeekUnitKind::Gap)
        {
            item.FirstOfKind = unit.Source->FirstOfKind;
            item.DecidedByOrder = unit.Source->DecidedByOrder;
        }
        result.Plan.Items.push_back(std::move(item));
        end = unit.Start + unit.Minutes;
    }
    result.Plan.SpanMinutes = end - built.StartInstant;
    if (result.Plan.SpanMinutes >= 1)
    {
        if (const TimeContractError error = ValidatePlan(result.Plan); error != TimeContractError::None)
            result.Issue = WeekMakeIssue(WeekError::NotRoutine, -1, -1, std::string(TimeContractErrorCode(error)));
    }
    return result;
}

WeekPlanIssue ValidateWeekPlan(const WorldState& world, const WeekPlan& plan, const DayLoopCatalogues& catalogues)
{
    return BuildWeekTimePlan(world, plan, catalogues, -1).Issue;
}
} // namespace PerfectWorld
