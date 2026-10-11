#include "TimeContract.h"

#include <algorithm>

// The equivalence rule (ROADMAP M3-01 helper for M3-02; SYSTEMS §2.5 rule 2 and §8
// "granularity equivalence"). A routine plan is resolved with different step
// sizes through the one resolution path (ResolveInteraction), with every random
// draw keyed by (seed, day, index) rather than drawn from a running stream, so
// the ledger cannot depend on how finely time was stepped.
namespace PerfectWorld
{
namespace
{
constexpr std::uint64_t TimeContractFnvOffset = 0xcbf29ce484222325ULL;
constexpr std::uint64_t TimeContractFnvPrime = 0x100000001b3ULL;
constexpr std::int64_t TimeContractWeekMinutes = 7 * MinutesPerDay;

void TimeContractFoldBytes(std::uint64_t& hash, std::string_view bytes)
{
    for (const char c : bytes)
    {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= TimeContractFnvPrime;
    }
}

void TimeContractFoldInt(std::uint64_t& hash, std::uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8)
    {
        hash ^= (value >> shift) & 0xffu;
        hash *= TimeContractFnvPrime;
    }
}

std::uint64_t TimeContractMix(std::uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

/** The window size each granularity steps by. */
std::int64_t TimeContractWindow(TimeGranularity granularity, std::int64_t span)
{
    switch (granularity)
    {
    case TimeGranularity::Scenes:
    case TimeGranularity::Day: return MinutesPerDay;
    case TimeGranularity::Week: return TimeContractWeekMinutes;
    case TimeGranularity::Span: return span;
    }
    return span;
}

/** One routine interaction resolved to its end through the same step function a played one uses. */
bool TimeContractResolveItem(const PlannedInteraction& item, std::int64_t chunk, TimeResolution& out)
{
    TimeContractError error = TimeContractError::None;
    InteractionRun run = BeginInteraction(item.Decl, error);
    if (error != TimeContractError::None)
        return false;
    std::int64_t left = item.Decl.CostMinutes;
    while (run.Outcome == TimeOutcome::Running && left > 0)
    {
        const std::int64_t step = chunk > 0 ? std::min(chunk, left) : left;
        if (StepInteraction(run, {TimeEventKind::Progress, step}) != StepResult::Consumed)
            return false;
        left -= step;
    }
    if (run.Outcome == TimeOutcome::Running && StepInteraction(run, {TimeEventKind::Finish, 0}) != StepResult::Consumed)
        return false;
    out = DescribeInteraction(run);
    return out.Outcome == TimeOutcome::Completed;
}

void TimeContractFoldItem(TimeLedger& ledger, const TimePlan& plan, std::size_t index, const TimeResolution& resolution)
{
    const PlannedInteraction& item = plan.Items[index];
    ledger.ChargedByMode[static_cast<std::size_t>(item.Decl.Mode)] += resolution.ChargedMinutes;
    ledger.TotalCharged += resolution.ChargedMinutes;
    ledger.Completed += 1;
    ledger.OutcomeBeats += resolution.OutcomeBeats;
    ledger.DecidedByOrder += item.DecidedByOrder ? 1 : 0;
    TimeContractFoldInt(ledger.InteractionHash, TimeKeyedDraw(plan.Seed, item.StartMinute / MinutesPerDay, static_cast<std::int64_t>(index)));
    TimeContractFoldBytes(ledger.InteractionHash, item.Decl.Id);
    TimeContractFoldInt(ledger.InteractionHash, static_cast<std::uint64_t>(resolution.ChargedMinutes));
    TimeContractFoldInt(ledger.InteractionHash, static_cast<std::uint64_t>(resolution.OutcomeBeats));
}
} // namespace

std::uint64_t TimeKeyedDraw(std::uint64_t seed, std::int64_t day, std::int64_t index) noexcept
{
    return TimeContractMix(TimeContractMix(seed ^ static_cast<std::uint64_t>(day)) ^ (static_cast<std::uint64_t>(index) + 0x5bd1e995ULL));
}

TimeContractError ValidatePlan(const TimePlan& plan)
{
    if (plan.SpanMinutes < 1 || plan.SpanMinutes > MaxSingleAdvanceMinutes)
        return TimeContractError::PlanSpanInvalid;
    std::int64_t previousStart = 0;
    std::int64_t previousEnd = 0;
    for (const PlannedInteraction& item : plan.Items)
    {
        if (const TimeContractError error = ValidateDeclaration(item.Decl); error != TimeContractError::None)
            return error;
        if (item.Decl.Mode == ClockMode::Mission || item.Decl.Mode == ClockMode::Compressed)
            return TimeContractError::PlanNotRoutine;
        if (item.StartMinute < previousStart)
            return TimeContractError::PlanOutOfOrder;
        if (item.StartMinute < previousEnd)
            return TimeContractError::PlanOverlap;
        previousStart = item.StartMinute;
        previousEnd = item.StartMinute + item.Decl.CostMinutes;
        if (previousEnd > plan.SpanMinutes)
            return TimeContractError::PlanOutOfSpan;
    }
    return TimeContractError::None;
}

TimePlanResult ResolvePlan(const TimePlan& plan, TimeGranularity granularity)
{
    TimePlanResult result;
    result.Error = ValidatePlan(plan);
    if (result.Ok() && granularity != TimeGranularity::Scenes &&
        std::any_of(plan.Items.begin(), plan.Items.end(), [](const PlannedInteraction& item) { return item.FirstOfKind; }))
        result.Error = TimeContractError::PlanNeedsPlay; // a first is never auto-resolved
    if (!result.Ok())
        return result;

    TimeLedger& ledger = result.Ledger;
    ledger.DayDrawHash = TimeContractFnvOffset;
    ledger.InteractionHash = TimeContractFnvOffset;
    const std::int64_t window = TimeContractWindow(granularity, plan.SpanMinutes);
    const std::int64_t chunk = granularity == TimeGranularity::Scenes ? TimeSceneChunkMinutes : 0;
    std::size_t next = 0;
    for (std::int64_t from = 0; from < plan.SpanMinutes; from += window)
    {
        const std::int64_t to = std::min(plan.SpanMinutes, from + window);
        ++ledger.Steps;
        // Interactions that begin in this window, in plan order; one may run past its end, as a long sleep does.
        for (; next < plan.Items.size() && plan.Items[next].StartMinute < to; ++next)
        {
            TimeResolution resolution;
            if (!TimeContractResolveItem(plan.Items[next], chunk, resolution))
            {
                result.Error = TimeContractError::InvalidState;
                return result;
            }
            TimeContractFoldItem(ledger, plan, next, resolution);
            if (granularity == TimeGranularity::Scenes)
                ++ledger.Steps;
        }
        // Each day that begins in this window draws its own key, in day order.
        for (std::int64_t day = (from + MinutesPerDay - 1) / MinutesPerDay; day * MinutesPerDay < to; ++day)
            TimeContractFoldInt(ledger.DayDrawHash, TimeKeyedDraw(plan.Seed, day, -1));
    }
    ledger.EndMinute = plan.SpanMinutes;
    return result;
}

std::string_view CompareLedgers(const TimeLedger& a, const TimeLedger& b) noexcept
{
    if (a.EndMinute != b.EndMinute) return "end_minute";
    if (a.ChargedByMode != b.ChargedByMode) return "charged_by_mode";
    if (a.TotalCharged != b.TotalCharged) return "total_charged";
    if (a.Completed != b.Completed) return "completed";
    if (a.OutcomeBeats != b.OutcomeBeats) return "outcome_beats";
    if (a.DecidedByOrder != b.DecidedByOrder) return "decided_by_order";
    if (a.DayDrawHash != b.DayDrawHash) return "day_draw_hash";
    if (a.InteractionHash != b.InteractionHash) return "interaction_hash";
    return {};
}

EquivalenceReport CompareGranularities(const TimePlan& plan)
{
    EquivalenceReport report;
    const TimePlanResult base = ResolvePlan(plan, TimeGranularity::Scenes);
    if (!base.Ok())
    {
        report.Error = base.Error;
        return report;
    }
    static constexpr std::array<std::pair<TimeGranularity, std::string_view>, 3> Others{{
        {TimeGranularity::Day, "day"}, {TimeGranularity::Week, "week"}, {TimeGranularity::Span, "span"}}};
    for (const auto& [granularity, name] : Others)
    {
        const TimePlanResult other = ResolvePlan(plan, granularity);
        if (!other.Ok())
        {
            report.Error = other.Error;
            return report;
        }
        if (const std::string_view differs = CompareLedgers(base.Ledger, other.Ledger); !differs.empty())
        {
            report.Difference = std::string(name) + ": " + std::string(differs);
            return report;
        }
    }
    return report;
}
} // namespace PerfectWorld
