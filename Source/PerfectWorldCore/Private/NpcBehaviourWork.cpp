#include "NpcBehaviour.h"

#include <algorithm>
#include <array>

namespace PerfectWorld
{
namespace
{
struct R5BPhaseDef
{
    std::string_view Person, Anchor, Role;
    NpcWorkPhase Phase;
    std::string_view Action, Pause;
    NpcDecisionReason Reason;
};
constexpr R5BPhaseDef R5BTestPhase{"person.debashis_pal", "anchor.repair_workshop.workbench", "inspecting", NpcWorkPhase::Test,
    "work.test", "npc.pause.test_stopped", NpcDecisionReason::Bench};
constexpr std::array<R5BPhaseDef, 12> R5BPhases{{
    {"person.debashis_pal", "anchor.repair_workshop.workbench", "inspecting", NpcWorkPhase::Inspect,
     "work.inspect", "npc.pause.inspection_recorded", NpcDecisionReason::Bench},
    {"person.debashis_pal", "anchor.repair_workshop.workbench", "repairing", NpcWorkPhase::Repair,
     "work.repair", "npc.pause.tools_down", NpcDecisionReason::Bench},
    {"person.debashis_pal", "anchor.repair_workshop.finishing_table", "repairing", NpcWorkPhase::Repair,
     "work.repair", "npc.pause.item_secured", NpcDecisionReason::Finishing},
    {"person.debashis_pal", "anchor.repair_workshop.tool_rack", "stocking", NpcWorkPhase::ToolRestock,
     "work.stock", "npc.pause.tool_stowed", NpcDecisionReason::Tools},
    {"person.debashis_pal", "anchor.repair_workshop.staff_counter", "serving", NpcWorkPhase::Collect,
     "work.collect", "npc.pause.collection_recorded", NpcDecisionReason::Counter},
    {"person.mahua_saha", "anchor.hardware_store.staff_counter", "serving", NpcWorkPhase::Serve,
     "work.serve", "npc.pause.customer_served", NpcDecisionReason::Counter},
    {"person.mahua_saha", "anchor.hardware_store.weigh_station", "inspecting", NpcWorkPhase::Weigh,
     "work.weigh", "npc.pause.scale_cleared", NpcDecisionReason::Scale},
    {"person.mahua_saha", "anchor.hardware_store.stock_room", "stocking", NpcWorkPhase::StockRestock,
     "work.stock", "npc.pause.stock_secured", NpcDecisionReason::Stock},
    {"person.satyajit_dutta", "anchor.tailor_shop.staff_counter", "inspecting", NpcWorkPhase::Measure,
     "work.measure", "npc.pause.measure_recorded", NpcDecisionReason::Counter},
    {"person.satyajit_dutta", "anchor.tailor_shop.workbench", "repairing", NpcWorkPhase::Cut,
     "work.cut", "npc.pause.shears_down", NpcDecisionReason::Bench},
    {"person.satyajit_dutta", "anchor.tailor_shop.finishing_table", "repairing", NpcWorkPhase::Stitch,
     "work.stitch", "npc.pause.needle_secured", NpcDecisionReason::Finishing},
    {"person.satyajit_dutta", "anchor.tailor_shop.finishing_table", "inspecting", NpcWorkPhase::Press,
     "work.press", "npc.pause.iron_stood", NpcDecisionReason::Finishing},
}};

R5BPhaseDef R5BPhase(const PersonSchedule& person, const ScheduleResolution& block)
{
    if (block.Status == ScheduleStatus::Stay && block.Kind == ScheduleDoingKind::Role)
    {
        for (const auto& phase : R5BPhases)
        {
            if (person.PersonId != phase.Person || block.Zone != phase.Anchor || block.Doing != phase.Role) { continue; }
            // The authored final 17:00 inspection is the bench test, not the morning intake inspection.
            if (phase.Phase == NpcWorkPhase::Inspect && (block.SegmentStart + 360) % MinutesPerDay >= 1020)
            {
                return R5BTestPhase;
            }
            return phase;
        }
    }
    return {};
}

struct R5BWindow
{
    NpcInterruptionWindow Input;
    GameInstant Accepted = 0, BlockStart = 0, BlockEnd = 0;
};

GameInstant R5BWorked(const ScheduleResolution& block, GameInstant at, std::span<const R5BWindow> windows)
{
    auto elapsed = at - block.SegmentStart;
    for (const auto& window : windows)
    {
        const auto begin = std::max(block.SegmentStart, window.Accepted);
        const auto end = std::min(at, window.Input.ReleasedAt.value_or(ScheduleMaxInstant + 1));
        elapsed -= std::max<GameInstant>(0, end - begin);
    }
    return elapsed;
}

NpcIntentError R5BPrepareWindows(const PersonSchedule& person, const ScheduleTravelTable& travel,
                               std::span<const NpcInterruptionWindow> input, std::vector<R5BWindow>& output)
{
    if (input.size() > NpcMaxInterruptionWindows) { return NpcIntentError::InvalidInterruption; }
    for (const auto& window : input)
    {
        if ((window.State != NpcState::Chat && window.State != NpcState::Yield) || window.RequestedAt < 0 ||
            window.RequestedAt > ScheduleMaxInstant || (window.ReleasedAt &&
            (*window.ReleasedAt < window.RequestedAt || *window.ReleasedAt > ScheduleMaxInstant)) ||
            (!output.empty() && (!output.back().Input.ReleasedAt || *output.back().Input.ReleasedAt > window.RequestedAt)))
        {
            return NpcIntentError::InvalidInterruption;
        }
        const auto block = ResolveSchedule(person, travel, window.RequestedAt);
        const auto intent = ResolveNpcIntent(block, window.State == NpcState::Chat, false);
        if (!intent.Ok()) { return intent.Error; }
        auto accepted = window.RequestedAt;
        if (R5BPhase(person, block).Phase != NpcWorkPhase::None)
        {
            const auto remainder = R5BWorked(block, accepted, output) % NpcWorkStepMinutes;
            if (remainder != 0) { accepted = std::min(block.SegmentEnd, accepted + NpcWorkStepMinutes - remainder); }
        }
        output.push_back({window, accepted, block.SegmentStart, block.SegmentEnd});
    }
    return NpcIntentError::None;
}

std::int64_t R5BCompleted(const ScheduleResolution& block, GameInstant worked)
{
    return worked / NpcWorkStepMinutes + (worked == block.SegmentEnd - block.SegmentStart && worked % NpcWorkStepMinutes != 0);
}

NpcDecisionReason R5BBaseReason(const NpcIntent& intent)
{
    if (intent.State == NpcState::Travel) { return NpcDecisionReason::Walking; }
    if (intent.State == NpcState::Sleep) { return NpcDecisionReason::Asleep; }
    return NpcDecisionReason::Available;
}
}

NpcDecision ResolveNpcDecisionAt(const PersonSchedule& person, const ScheduleTravelTable& travel, GameInstant at,
                                std::span<const NpcInterruptionWindow> windows)
{
    NpcDecision result;
    const auto block = ResolveSchedule(person, travel, at);
    const auto base = ResolveNpcIntent(block, false, false);
    if (!base.Ok()) { result.Error = base.Error; return result; }
    std::vector<R5BWindow> prepared;
    result.Error = R5BPrepareWindows(person, travel, windows, prepared);
    if (!result.Ok()) { return result; }
    result.Intent = base.Intent;
    result.Reason = R5BBaseReason(result.Intent);
    result.Needs = ResolveNpcScheduleNeeds(person, travel, at);
    const auto phase = R5BPhase(person, block);
    if (phase.Phase != NpcWorkPhase::None)
    {
        const auto worked = R5BWorked(block, at, prepared);
        result.Work = {phase.Phase, phase.Pause, block.SegmentStart, block.SegmentEnd,
                       worked / NpcWorkStepMinutes, R5BCompleted(block, worked),
                       std::min(block.SegmentEnd, at + NpcWorkStepMinutes - worked % NpcWorkStepMinutes)};
        result.Intent.WorkActionId = phase.Action;
        result.Intent.ResumeWorkActionId = phase.Action;
        result.Reason = phase.Reason;
        result.Needs.Priority = NpcNeedPriority::Work;
        const auto minute = (at + 360) % MinutesPerDay;
        if ((person.PersonId == "person.debashis_pal" && minute >= 1110) ||
            (person.PersonId == "person.mahua_saha" && minute >= 1200)) { result.Reason = NpcDecisionReason::Closing; }
    }
    if (result.Needs.Priority >= NpcNeedPriority::Rest) { result.Reason = result.Needs.Reason; }
    for (const auto& window : prepared)
    {
        const auto end = window.Input.ReleasedAt.value_or(ScheduleMaxInstant + 1);
        if (window.Input.RequestedAt > at) { break; }
        if (at < window.Accepted && at < end)
        {
            result.DeferredUntil = window.Accepted;
            result.Reason = NpcDecisionReason::PausePending;
            result.Intent.Availability = PersonAvailability::Busy;
            result.Intent.DeclineReasonKey = NpcDecisionReasonKey(result.Reason);
        }
        else if (window.Accepted < end && at >= window.Accepted)
        {
            // Keep the resume disposition after release too; it never restores an expired phase.
            result.AcceptedAt = window.Accepted;
            result.ScheduleChanged = block.SegmentStart != window.BlockStart || block.SegmentEnd != window.BlockEnd;
            if (at < end)
            {
                const auto suspended = ResolveNpcIntent(block, window.Input.State == NpcState::Chat, window.Input.State == NpcState::Yield);
                if (!suspended.Ok()) { result = {}; result.Error = suspended.Error; return result; }
                result.Intent.State = suspended.Intent.State;
                result.Intent.ResumeState = base.Intent.State;
                result.Intent.WorkActionId = {};
                if (phase.Phase != NpcWorkPhase::None && window.Input.State == NpcState::Chat)
                {
                    result.Intent.Availability = PersonAvailability::Awake;
                    result.Intent.DeclineReasonKey = {};
                }
                if (window.Input.State == NpcState::Yield && result.Intent.State != NpcState::Sleep)
                {
                    result.Intent.Availability = PersonAvailability::Busy;
                    result.Intent.DeclineReasonKey = NpcReasonBlocked;
                }
                if (result.Intent.State != NpcState::Sleep)
                {
                    result.Reason = window.Input.State == NpcState::Chat ? NpcDecisionReason::Chat : NpcDecisionReason::Yield;
                }
                if (phase.Phase != NpcWorkPhase::None) { result.Work.SafeAt = at; }
            }
        }
    }
    return result;
}

std::optional<GameInstant> NextNpcDecisionBoundary(const PersonSchedule& person, const ScheduleTravelTable& travel,
                                                 GameInstant after, std::span<const NpcInterruptionWindow> windows)
{
    const auto decision = ResolveNpcDecisionAt(person, travel, after, windows);
    if (!decision.Ok()) { return {}; }
    const auto block = ResolveSchedule(person, travel, after);
    std::optional<GameInstant> next;
    const auto consider = [&](GameInstant at)
    {
        if (at > after && at <= ScheduleMaxInstant && (!next || at < *next)) { next = at; }
    };
    consider(block.SegmentEnd);
    if (decision.Work.Phase != NpcWorkPhase::None) { consider(decision.Work.SafeAt); }
    if (block.Doing == "break") { consider(block.SegmentStart + NpcMealBreakMinutes); }
    for (const auto& window : windows)
    {
        consider(window.RequestedAt);
        if (window.ReleasedAt) { consider(*window.ReleasedAt); }
    }
    if (decision.DeferredUntil) { consider(*decision.DeferredUntil); }
    if (decision.Needs.MinutesSinceMeal && *decision.Needs.MinutesSinceMeal < NpcMealPriorityAfterMinutes)
    {
        consider(after + NpcMealPriorityAfterMinutes - *decision.Needs.MinutesSinceMeal);
    }
    if (decision.Needs.MinutesSinceRest && *decision.Needs.MinutesSinceRest < NpcRestPriorityAfterMinutes)
    {
        consider(after + NpcRestPriorityAfterMinutes - *decision.Needs.MinutesSinceRest);
    }
    return next;
}

NpcWorkEvents NpcWorkEventsBetween(const PersonSchedule& person, const ScheduleTravelTable& travel, GameInstant from,
                                 GameInstant to, std::span<const NpcInterruptionWindow> windows)
{
    NpcWorkEvents result;
    if (from < 0 || to < from || to > ScheduleMaxInstant || to - from > MaxSingleAdvanceMinutes)
    {
        result.Error = NpcIntentError::InvalidResolution;
        return result;
    }
    std::vector<R5BWindow> prepared;
    result.Error = R5BPrepareWindows(person, travel, windows, prepared);
    if (!result.Ok()) { return result; }
    for (auto cursor = from; cursor < to;)
    {
        const auto block = ResolveSchedule(person, travel, cursor);
        const auto intent = ResolveNpcIntent(block, false, false);
        if (!intent.Ok() || block.SegmentEnd <= cursor)
        {
            result = {};
            result.Error = intent.Ok() ? NpcIntentError::InvalidResolution : intent.Error;
            return result;
        }
        const auto phase = R5BPhase(person, block);
        if (phase.Phase != NpcWorkPhase::None)
        {
            const auto length = block.SegmentEnd - block.SegmentStart;
            const auto count = (length + NpcWorkStepMinutes - 1) / NpcWorkStepMinutes;
            std::int64_t completed = 0;
            for (std::int64_t ordinal = 0; ordinal < count; ++ordinal)
            {
                auto boundary = block.SegmentStart + std::min(length, (ordinal + 1) * NpcWorkStepMinutes);
                for (const auto& window : prepared)
                {
                    const auto begin = std::max(block.SegmentStart, window.Accepted);
                    const auto end = std::min(block.SegmentEnd, window.Input.ReleasedAt.value_or(ScheduleMaxInstant + 1));
                    if (begin < boundary && end > begin) { boundary += end - begin; }
                }
                if (boundary > block.SegmentEnd) { break; }
                ++completed;
                if (boundary > from && boundary <= to)
                {
                    result.Events.push_back({boundary, block.SegmentStart, block.SourceId, phase.Phase, ordinal, 1, NpcWorkOutcome::Completed});
                }
            }
            if (completed < count && block.SegmentEnd > from && block.SegmentEnd <= to)
            {
                result.Events.push_back({block.SegmentEnd, block.SegmentStart, block.SourceId, phase.Phase, completed,
                                         count - completed, NpcWorkOutcome::Abandoned});
            }
        }
        cursor = block.SegmentEnd;
    }
    return result;
}
namespace
{
bool R5BNeedsPilot(std::string_view person)
{
    return person == "person.debashis_pal" || person == "person.mahua_saha" || person == "person.satyajit_dutta";
}

struct R5BNeedBlock
{
    std::optional<GameInstant> MealEnd;
    std::optional<GameInstant> RestEnd;
};

R5BNeedBlock R5BNeedsBlock(const PersonSchedule& person, const ScheduleTravelTable& travel,
                         const ScheduleResolution& block)
{
    if (block.Status != ScheduleStatus::Stay || block.Kind != ScheduleDoingKind::Role) { return {}; }
    const auto minute = (block.SegmentStart + 360) % MinutesPerDay;
    const bool midday = minute >= 720 && minute < 840;
    if (block.Doing == "break")
    {
        const auto mealEnd = std::min(block.SegmentEnd, block.SegmentStart + NpcMealBreakMinutes);
        if (midday) { return {mealEnd, mealEnd < block.SegmentEnd ? std::optional{block.SegmentEnd} : std::nullopt}; }
        return {{}, block.SegmentEnd};
    }
    if (block.Doing == "resting" || block.Doing == "sleeping" || block.Doing == "private")
    {
        return {{}, block.SegmentEnd};
    }
    if (block.Doing == "at_home" && block.Zone == person.HomeZone)
    {
        const auto previous = block.SegmentStart > 0 ? ResolveSchedule(person, travel, block.SegmentStart - 1) : ScheduleResolution{};
        const bool afterCooking = previous.Valid && previous.Status == ScheduleStatus::Stay &&
            previous.Kind == ScheduleDoingKind::Role && previous.Doing == "cooking" && previous.Zone == block.Zone;
        if (midday || afterCooking) { return {block.SegmentEnd, {}}; }
    }
    return {};
}
}

std::string_view NpcDecisionReasonKey(NpcDecisionReason reason) noexcept
{
    static constexpr std::array<std::string_view, 16> R5BReasonKeys{
        "npc.reason.available", "npc.reason.working_at_bench", "npc.reason.working_at_tools",
        "npc.reason.working_at_finishing_table", "npc.reason.serving_at_counter", "npc.reason.weighing",
        "npc.reason.restocking", "npc.reason.meal", "npc.reason.rest", "npc.reason.tea_break",
        "npc.reason.closing_up", "npc.reason.walking", "npc.reason.asleep", "npc.reason.next_safe_pause",
        "npc.reason.chat", "npc.reason.blocked"};
    const auto index = static_cast<std::size_t>(reason);
    return index < R5BReasonKeys.size() ? R5BReasonKeys[index] : std::string_view{};
}

NpcScheduleNeeds ResolveNpcScheduleNeeds(const PersonSchedule& person, const ScheduleTravelTable& travel, GameInstant at)
{
    NpcScheduleNeeds result;
    if (!R5BNeedsPilot(person.PersonId) || at < 0 || at > ScheduleMaxInstant) { return result; }
    const auto current = ResolveSchedule(person, travel, at);
    if (current.Status == ScheduleStatus::Stay)
    {
        const auto* role = current.Kind == ScheduleDoingKind::Role ? FindNpcRole(current.Doing) : nullptr;
        if (current.Kind == ScheduleDoingKind::Activity || (role != nullptr && role->State == NpcState::Work))
        {
            result.Priority = NpcNeedPriority::Work;
        }
    }
    const auto active = R5BNeedsBlock(person, travel, current);
    if (active.MealEnd && at < *active.MealEnd)
    {
        result.Priority = NpcNeedPriority::Meal;
        result.Reason = NpcDecisionReason::Meal;
        result.MinutesSinceMeal = 0;
    }
    if (active.RestEnd && (!active.MealEnd || at >= *active.MealEnd))
    {
        result.Priority = NpcNeedPriority::Rest;
        result.Reason = current.Doing == "break" ? NpcDecisionReason::TeaBreak : NpcDecisionReason::Rest;
        result.MinutesSinceRest = 0;
    }
    const auto limit = std::max<GameInstant>(0, at - ScheduleHorizonDays * MinutesPerDay);
    for (auto cursor = at; cursor >= limit;)
    {
        const auto block = ResolveSchedule(person, travel, cursor);
        if (!block.Valid) { break; }
        const auto needs = R5BNeedsBlock(person, travel, block);
        if (!result.MinutesSinceMeal && needs.MealEnd && *needs.MealEnd <= at)
        {
            result.MinutesSinceMeal = at - *needs.MealEnd;
        }
        if (!result.MinutesSinceRest && needs.RestEnd && *needs.RestEnd <= at)
        {
            result.MinutesSinceRest = at - *needs.RestEnd;
        }
        if ((result.MinutesSinceMeal && result.MinutesSinceRest) || block.SegmentStart <= limit) { break; }
        cursor = block.SegmentStart - 1;
    }
    result.NextPausePriority = !result.MinutesSinceMeal || *result.MinutesSinceMeal >= NpcMealPriorityAfterMinutes
        ? NpcNeedPriority::Meal : NpcNeedPriority::Rest;
    if (result.NextPausePriority == NpcNeedPriority::Rest && result.MinutesSinceRest &&
        *result.MinutesSinceRest < NpcRestPriorityAfterMinutes) { result.NextPausePriority = NpcNeedPriority::Routine; }
    if (current.Status == ScheduleStatus::Stay && current.Kind == ScheduleDoingKind::Role &&
        (current.Doing == "sleeping" || current.Doing == "private"))
    {
        result.Priority = NpcNeedPriority::Sleep;
        result.Reason = NpcDecisionReason::Asleep;
    }
    return result;
}
} // namespace PerfectWorld
