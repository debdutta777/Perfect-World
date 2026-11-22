#include "NpcBehaviour.h"

#include "NpcBehaviourR5Support.h"

#include <array>
#include <set>
#include <tuple>

using namespace PerfectWorld;

namespace
{
NpcDecision R5BDecision(const NpcSchedule& schedule, GameInstant at, std::span<const NpcInterruptionWindow> windows = {})
{
    const auto decision = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, at, windows);
    R5BRequire(decision.Ok(), "decision accepted");
    return decision;
}

void R5BPhasesTest(const NpcSchedule& debashis, const NpcSchedule& mahua)
{
    const std::array<std::tuple<int, NpcWorkPhase, std::string_view>, 7> phases{{
        {462, NpcWorkPhase::Inspect, "npc.pause.inspection_recorded"},
        {552, NpcWorkPhase::Repair, "npc.pause.tools_down"},
        {643, NpcWorkPhase::ToolRestock, "npc.pause.tool_stowed"},
        {662, NpcWorkPhase::Collect, "npc.pause.collection_recorded"},
        {926, NpcWorkPhase::Repair, "npc.pause.item_secured"},
        {970, NpcWorkPhase::Repair, "npc.pause.tools_down"},
        {1020, NpcWorkPhase::Test, "npc.pause.test_stopped"}}};
    for (const auto& [minute, phase, pause] : phases)
    {
        const auto result = R5BDecision(debashis, R5BAt(minute));
        R5BRequire(result.Work.Phase == phase && result.Work.SafePauseKey == pause, "workshop phase and named pause");
        R5BRequire(result.Work.Ordinal == 0 && result.Work.CompletedSteps == 0, "new block starts at its first step");
    }
    for (const auto& [minute, phase] : std::array<std::pair<int, NpcWorkPhase>, 3>{{
        {540, NpcWorkPhase::Serve}, {634, NpcWorkPhase::Weigh}, {667, NpcWorkPhase::StockRestock}}})
    {
        R5BRequire(R5BDecision(mahua, R5BAt(minute)).Work.Phase == phase, "hardware phase");
    }
    R5BRequire(R5BDecision(debashis, R5BAt(1117)).Reason == NpcDecisionReason::Closing, "workshop closing explained");
    R5BRequire(R5BDecision(mahua, R5BAt(1209)).Reason == NpcDecisionReason::Closing, "store closing explained");
    std::set<std::string_view> reasons;
    for (int i = 0; i <= static_cast<int>(NpcDecisionReason::Yield); ++i)
    {
        const auto key = NpcDecisionReasonKey(static_cast<NpcDecisionReason>(i));
        R5BRequire(!key.empty() && reasons.insert(key).second, "unique reason text keys");
    }
    R5BRequire(NpcDecisionReasonKey(static_cast<NpcDecisionReason>(255)).empty(), "invalid reason has no text key");
}

void R5BInterruptTest(const NpcSchedule& schedule, int start)
{
    const auto begin = R5BAt(start);
    for (const auto state : {NpcState::Chat, NpcState::Yield})
    {
        for (int offset = 0; offset < 3; ++offset)
        {
            const auto request = begin + offset;
            const auto accepted = begin + (offset == 0 ? 0 : 3);
            const std::array windows{NpcInterruptionWindow{state, request, begin + 8}};
            const auto first = R5BDecision(schedule, request, windows);
            if (offset != 0)
            {
                R5BRequire(first.Intent.State == NpcState::Work && first.DeferredUntil == accepted,
                            "mid-step request waits for the next safe boundary");
                R5BRequire(first.Reason == NpcDecisionReason::PausePending && *first.DeferredUntil - request <= NpcWorkStepMinutes,
                            "wait is bounded and explained");
                R5BRequire(R5BDecision(schedule, accepted - 1, windows).Intent.State == NpcState::Work, "last unsafe minute still works");
            }
            else { R5BRequire(!first.DeferredUntil, "request on a boundary is immediate"); }
            const auto during = R5BDecision(schedule, accepted, windows);
            R5BRequire(during.Intent.State == state && during.AcceptedAt == accepted, "boundary accepts interruption");
            R5BRequire(during.Intent.WorkActionId.empty() && during.Intent.ResumeState == NpcState::Work,
                        "work stops while interruption retains its resume state");
            if (state == NpcState::Chat)
            {
                R5BRequire(during.Intent.Availability == PersonAvailability::Awake && during.Intent.DeclineReasonKey.empty(),
                            "accepted work Chat is available at the safe pause");
            }
            const auto later = R5BDecision(schedule, begin + 7, windows);
            R5BRequire(later.Work.Ordinal == during.Work.Ordinal && later.Work.CompletedSteps == during.Work.CompletedSteps,
                        "interruption never earns work steps");
            const auto resumed = R5BDecision(schedule, begin + 8, windows);
            R5BRequire(resumed.Intent.State == NpcState::Work && resumed.Work.Phase == during.Work.Phase &&
                        resumed.Work.Ordinal == during.Work.Ordinal && !resumed.ScheduleChanged,
                        "release resumes the same phase and next unfinished step");
            R5BRequire(R5BDecision(schedule, begin + 11, windows).Work.CompletedSteps == resumed.Work.CompletedSteps + 1,
                        "only a full resumed step earns one completion");
        }
        const std::array cancelled{NpcInterruptionWindow{state, begin + 1, begin + 2}};
        const auto cleared = R5BDecision(schedule, begin + 2, cancelled);
        R5BRequire(!cleared.DeferredUntil && cleared.Intent.State == NpcState::Work, "release before safe point cancels request");
        R5BRequire(cleared.Work == R5BDecision(schedule, begin + 2).Work, "cancelled request consumes no work time");
    }
    const std::array repeated{NpcInterruptionWindow{NpcState::Chat, begin + 1, begin + 8},
                              NpcInterruptionWindow{NpcState::Yield, begin + 9, begin + 15}};
    R5BRequire(R5BDecision(schedule, begin + 9, repeated).DeferredUntil == begin + 11,
                "second interruption uses the resumed step boundary, not a wall-clock grid");
}

void R5BBoundaryTest(const NpcSchedule& debashis, const NpcSchedule& mahua)
{
    for (const auto state : {NpcState::Chat, NpcState::Yield})
    {
        const std::array windows{NpcInterruptionWindow{state, R5BAt(638), R5BAt(650)}};
        R5BRequire(R5BDecision(debashis, R5BAt(638), windows).DeferredUntil == R5BAt(639), "last full repair step waits one minute");
        const auto next = R5BDecision(debashis, R5BAt(643), windows);
        R5BRequire(next.Work.Phase == NpcWorkPhase::ToolRestock && next.ScheduleChanged, "new block wins while interrupted");
        R5BRequire(next.Work.CompletedSteps == 0, "new block does not earn steps while held");
        const auto released = R5BDecision(debashis, R5BAt(650), windows);
        R5BRequire(released.Work.Phase == NpcWorkPhase::ToolRestock && released.Intent.State == NpcState::Work,
                    "release after boundary uses the current schedule");
        const std::array partial{NpcInterruptionWindow{state, R5BAt(639), R5BAt(646)}};
        R5BRequire(R5BDecision(mahua, R5BAt(639), partial).DeferredUntil == R5BAt(640), "mid weighing step waits");
        const std::array shortFinal{NpcInterruptionWindow{state, R5BAt(472), R5BAt(480)}};
        R5BRequire(R5BDecision(debashis, R5BAt(472), shortFinal).DeferredUntil == R5BAt(473),
                    "short terminal step stops at its schedule boundary");
    }
    const std::array travel{NpcInterruptionWindow{NpcState::Yield, R5BAt(666), R5BAt(667)}};
    const auto walking = R5BDecision(mahua, R5BAt(666), travel);
    R5BRequire(walking.Intent.State == NpcState::Yield && walking.Intent.ResumeState == NpcState::Travel &&
                !walking.DeferredUntil, "a route obstruction yields immediately with route retained");
    R5BRequire(R5BDecision(mahua, R5BAt(667), travel).Work.Phase == NpcWorkPhase::StockRestock, "route clear at stock-room arrival");
    const std::array night{NpcInterruptionWindow{NpcState::Yield, R5BAt(1381), R5BAt(1390)}};
    R5BRequire(R5BDecision(mahua, R5BAt(1382), night).Intent.State == NpcState::Sleep, "sleep ignores obstruction");
    const std::array asleepChat{NpcInterruptionWindow{NpcState::Chat, R5BAt(1381), {}}};
    R5BRequire(ResolveNpcDecisionAt(mahua.Person, mahua.Travel, R5BAt(1382), asleepChat).Error ==
                NpcIntentError::ConversationUnavailable, "sleep refuses Chat");
}

void R5BNeedsTest(const NpcSchedule& debashis, const NpcSchedule& mahua)
{
    const auto morning = R5BDecision(mahua, R5BAt(476));
    R5BRequire(morning.Needs.MinutesSinceMeal == 0, "just completed breakfast has age zero");
    const auto due = R5BDecision(mahua, R5BAt(780));
    R5BRequire(due.Needs.NextPausePriority == NpcNeedPriority::Meal && due.Needs.MinutesSinceMeal == 304,
                "meal due is derived from the end of the last realised meal block");
    const auto meal = R5BDecision(mahua, R5BAt(789));
    R5BRequire(meal.Needs.Priority == NpcNeedPriority::Meal && meal.Reason == NpcDecisionReason::Meal &&
                meal.Work.Phase == NpcWorkPhase::None, "scheduled midday meal precedes work and rest");
    const auto tea = R5BDecision(mahua, R5BAt(809));
    R5BRequire(tea.Needs.Priority == NpcNeedPriority::Rest && tea.Reason == NpcDecisionReason::TeaBreak &&
                tea.Needs.MinutesSinceMeal == 0 && tea.Needs.MinutesSinceRest == 0, "meal transitions to tea/rest at twenty minutes");
    R5BRequire(R5BDecision(debashis, R5BAt(744)).Needs.Priority == NpcNeedPriority::Meal, "home midday block is a meal");
    R5BRequire(R5BDecision(debashis, R5BAt(790)).Needs.Priority == NpcNeedPriority::Rest, "authored rest follows meal");
    const auto after = R5BDecision(debashis, R5BAt(840));
    R5BRequire(after.Needs.Priority == NpcNeedPriority::Work && after.Needs.MinutesSinceRest == 24 &&
                after.Needs.MinutesSinceMeal == 50, "return to work keeps exact schedule-derived ages");
    const std::array acrossLunch{NpcInterruptionWindow{NpcState::Chat, R5BAt(778), R5BAt(810)}};
    const auto resumed = R5BDecision(mahua, R5BAt(810), acrossLunch);
    R5BRequire(resumed.Intent.State == NpcState::Idle && resumed.Needs.Priority == NpcNeedPriority::Rest &&
                resumed.ScheduleChanged, "leaving conversation after meal resumes rest, never old work");
    auto changed = mahua;
    changed.Person.Exceptions.push_back({"exception.no_meal", R5BAt(789), R5BAt(860),
        "anchor.hardware_store.stock_room", ScheduleDoingKind::Role, "stocking", 100});
    const auto overridden = R5BDecision(changed, R5BAt(810));
    R5BRequire(overridden.Needs.Priority == NpcNeedPriority::Work && overridden.Needs.MinutesSinceMeal == 334,
                "an overridden meal never resets need age");
    R5BRequire(R5BDecision(mahua, R5BAt(800, 6)).Needs.Priority == NpcNeedPriority::Meal, "Sunday uses its own longer break");
    const auto restDue = R5BDecision(mahua, R5BAt(500));
    R5BRequire(restDue.Needs.NextPausePriority == NpcNeedPriority::Routine, "recent meal and rest need no extra pause");
    R5BRequire(R5BDecision(mahua, R5BAt(510)).Needs.NextPausePriority == NpcNeedPriority::Rest,
                "rest becomes due at its deterministic age threshold before the next meal");
}

void R5BEquivalenceTest(const NpcSchedule& schedule, int start)
{
    const auto from = R5BAt(start), to = R5BAt(1440, 1);
    const std::array windows{NpcInterruptionWindow{NpcState::Chat, from + 1, from + 9},
        NpcInterruptionWindow{NpcState::Yield, from + 10, from + 21},
        NpcInterruptionWindow{NpcState::Chat, R5BAt(1069), R5BAt(1130)}};
    const auto whole = NpcWorkEventsBetween(schedule.Person, schedule.Travel, from, to, windows);
    R5BRequire(whole.Ok() && !whole.Events.empty(), "whole advance has step observations");
    std::set<std::tuple<GameInstant, std::string, std::int64_t>> identities;
    bool abandoned = false;
    for (const auto& event : whole.Events)
    {
        abandoned = abandoned || event.Outcome == NpcWorkOutcome::Abandoned;
        if (event.Outcome == NpcWorkOutcome::Completed)
        {
            for (const auto& window : windows)
            {
                const auto requested = R5BDecision(schedule, window.RequestedAt, windows);
                const auto accepted = requested.DeferredUntil.value_or(window.RequestedAt);
                if (window.ReleasedAt && *window.ReleasedAt > accepted)
                {
                    R5BRequire(event.At <= accepted || event.At > *window.ReleasedAt,
                                "no completion is earned during an accepted interruption");
                }
            }
        }
        for (std::int64_t i = 0; i < event.Count; ++i)
        {
            R5BRequire(identities.emplace(event.BlockStart, event.SourceId, event.FirstStep + i).second,
                        "no step is completed twice or both completed and abandoned");
        }
    }
    R5BRequire(abandoned, "block expiration explicitly reports unfinished steps");
    for (auto at = from; at < from + 30; ++at)
    {
        const auto decision = R5BDecision(schedule, at, windows);
        if (decision.Work.Phase == NpcWorkPhase::None) { continue; }
        const auto observations = NpcWorkEventsBetween(schedule.Person, schedule.Travel, decision.Work.BlockStart, at, windows);
        std::int64_t completed = 0;
        for (const auto& event : observations.Events)
        {
            if (event.Outcome == NpcWorkOutcome::Completed) { completed += event.Count; }
        }
        R5BRequire(observations.Ok() && completed == decision.Work.CompletedSteps,
                    "point decision progress agrees with interval observations throughout repeated interruptions");
    }
    // Every planned ordinal is accounted for, including the interrupted block's abandoned suffix.
    for (auto cursor = from; cursor < to;)
    {
        const auto block = ResolveSchedule(schedule.Person, schedule.Travel, cursor);
        const auto decision = R5BDecision(schedule, cursor, windows);
        if (decision.Work.Phase != NpcWorkPhase::None && block.SegmentStart >= from && block.SegmentEnd <= to)
        {
            const auto count = (block.SegmentEnd - block.SegmentStart + 2) / 3;
            for (std::int64_t ordinal = 0; ordinal < count; ++ordinal)
            {
                R5BRequire(identities.contains({block.SegmentStart, block.SourceId, ordinal}), "no step silently skipped");
            }
        }
        cursor = block.SegmentEnd;
    }
    for (const GameInstant chunk : {GameInstant{1}, GameInstant{17}, GameInstant{333}})
    {
        std::vector<NpcWorkEvent> pieces;
        for (auto cursor = from; cursor < to;)
        {
            const auto next = std::min(to, cursor + chunk);
            const auto part = NpcWorkEventsBetween(schedule.Person, schedule.Travel, cursor, next, windows);
            R5BRequire(part.Ok(), "split interval accepted");
            pieces.insert(pieces.end(), part.Events.begin(), part.Events.end());
            R5BRequire(R5BDecision(schedule, next, windows) == R5BDecision(schedule, next, windows), "repeat query deterministic");
            cursor = next;
        }
        R5BRequire(pieces == whole.Events, "minute/split observations equal one long skip");
    }
    std::vector<NpcWorkEvent> boundaries;
    auto cursor = from;
    while (cursor < to)
    {
        const auto boundary = NextNpcDecisionBoundary(schedule.Person, schedule.Travel, cursor, windows);
        R5BRequire(boundary && *boundary > cursor, "next decision boundary moves forward");
        const auto next = std::min(to, *boundary);
        const auto part = NpcWorkEventsBetween(schedule.Person, schedule.Travel, cursor, next, windows);
        R5BRequire(part.Ok(), "boundary interval accepted");
        boundaries.insert(boundaries.end(), part.Events.begin(), part.Events.end());
        cursor = next;
    }
    R5BRequire(boundaries == whole.Events, "boundary hopping equals direct long skip");
    R5BRequire(R5BDecision(schedule, cursor, windows) == R5BDecision(schedule, to, windows), "direct final decision equals stepped final decision");
    R5BRequire(NpcWorkEventsBetween(schedule.Person, schedule.Travel, to, to, windows).Events.empty(), "same instant emits no completions");
}

void R5BInvalidTest(const NpcSchedule& schedule)
{
    const auto begin = R5BAt(552);
    for (const auto& invalid : std::vector<std::vector<NpcInterruptionWindow>>{
        {{NpcState::Work, begin, {}}}, {{NpcState::Chat, -1, {}}}, {{NpcState::Yield, begin, begin - 1}},
        {{NpcState::Chat, begin, {}}, {NpcState::Yield, begin + 3, {}}},
        {{NpcState::Chat, begin, begin + 9}, {NpcState::Yield, begin + 8, begin + 10}},
        {{NpcState::Yield, begin, ScheduleMaxInstant + 1}},
        std::vector<NpcInterruptionWindow>(NpcMaxInterruptionWindows + 1, {NpcState::Yield, begin, begin})})
    {
        const auto decision = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, begin, invalid);
        R5BRequire(decision.Error == NpcIntentError::InvalidInterruption && decision.Intent == NpcIntent{}, "bad history refused atomically");
        R5BRequire(!NpcWorkEventsBetween(schedule.Person, schedule.Travel, begin, begin + 10, invalid).Ok(), "event query also refuses bad history");
    }
    R5BRequire(!ResolveNpcDecisionAt(schedule.Person, schedule.Travel, -1).Ok(), "negative instant refused");
    R5BRequire(!ResolveNpcDecisionAt(schedule.Person, schedule.Travel, ScheduleMaxInstant + 1).Ok(), "past maximum instant refused");
    R5BRequire(!NpcWorkEventsBetween(schedule.Person, schedule.Travel, begin, begin - 1).Ok(), "backwards interval refused");
    R5BRequire(!NpcWorkEventsBetween(schedule.Person, schedule.Travel, 0, MaxSingleAdvanceMinutes + 1).Ok(), "excessive interval refused");
}
}

int main(int argc, char** argv)
{
    R5BRequire(argc == 3, "supply Debashis and Mahua schedule paths");
    const auto debashis = R5BLoadSchedule(argv[1]), mahua = R5BLoadSchedule(argv[2]);
    R5BPhasesTest(debashis, mahua);
    R5BInterruptTest(debashis, 552);
    R5BInterruptTest(mahua, 634);
    R5BBoundaryTest(debashis, mahua);
    R5BNeedsTest(debashis, mahua);
    R5BEquivalenceTest(debashis, 552);
    R5BEquivalenceTest(mahua, 634);
    R5BInvalidTest(debashis);
    std::cout << "NpcBehaviourWorkTests passed\n";
}
