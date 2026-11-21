#include "WorldReducer.h"

#include "KnownCommitments.h"
#include "LaneJournal.h"

#include <algorithm>
#include <charconv>

namespace PerfectWorld
{
namespace
{
std::string P407UKey(const SchoolFollowupAction& action, std::string_view kind)
{
    return std::string(P407UFollowupFlagPrefix) + action.AppointmentId + std::string(kind);
}

bool P407UFlag(const WorldState& state, const std::string& key)
{
    const auto found = state.Flags.find(key);
    return found != state.Flags.end() && found->second;
}

P407UFollowupView P407UStored(const WorldState& state, const SchoolFollowupAction& action)
{
    P407UFollowupView row;
    row.Action = action;
    const auto prefix = P407UKey(action, ".book.");
    for (auto it = state.Flags.lower_bound(prefix); it != state.Flags.end() && it->first.starts_with(prefix); ++it)
    {
        if (!it->second) continue;
        const auto digits = std::string_view(it->first).substr(prefix.size());
        GameInstant at = 0;
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), at);
        if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() || at < CountdownEndInstant() ||
            at > DayLoopMaxInstant - action.DurationMinutes || std::to_string(at) != digits) continue;
        row.At = at;
        row.Booked = true;
        row.Attended = P407UFlag(state, P407UKey(action, ".done.") + std::to_string(at));
        break;
    }
    return row;
}

bool P407UOverlap(GameInstant a, std::int64_t minutes, GameInstant b, std::int64_t other)
{
    return a < b + other && b < a + minutes;
}

bool P407UFree(const WorldState& world, const DayLoopCatalogues& school,
    const KnownCommitmentContent& known, const std::vector<P407UFollowupView>& rows,
    const SchoolFollowupAction& action, GameInstant at)
{
    const auto day = CalendarDayOfInstant(at);
    const auto midnight = GameClock::StartOfDay(at);
    const bool open = CalendarRequiresAttendance(*school.Calendar, day);
    if (action.Destination == "place.school" && !open) return false;
    // A follow-up never replaces class attendance, even for home/workshop commitments.
    if (open && !school.School->Blocks.empty() &&
        P407UOverlap(at, action.DurationMinutes, midnight + school.School->Blocks.front().StartMinute,
            school.School->Blocks.back().EndMinute - school.School->Blocks.front().StartMinute)) return false;
    for (const auto& row : rows)
        if (row.Action.Id != action.Id && row.Booked && !row.Attended &&
            P407UOverlap(at, action.DurationMinutes, row.At, row.Action.DurationMinutes)) return false;
    for (const auto& promise : KnownTimedPromises(LaneJournalToday(world, GameClock::DayNumber(at)), known))
        if (P407UOverlap(at, action.DurationMinutes, promise.Commitment.Start, promise.Commitment.Minutes)) return false;
    if (GameClock::DayNumber(at) == GameClock::DayNumber(world.Clock.Now()))
    {
        // Timed intentions have authoritative activity durations when available.
        for (const auto& plan : world.DayLoop.Plan)
        {
            const auto* activity = school.Activities != nullptr ? school.Activities->Find(plan.ActivityId) : nullptr;
            if (activity != nullptr && P407UOverlap(at, action.DurationMinutes,
                midnight + plan.StartMinute, activity->DurationMinutes)) return false;
        }
    }
    return true;
}
} // namespace

std::vector<P407UFollowupView> WorldSim::P407UFollowups() const
{
    std::vector<P407UFollowupView> rows;
    const auto report = ExaminationResult();
    if (!report.Ok()) return rows;
    for (const auto& action : report.NextSteps) rows.push_back(P407UStored(State_, action));
    if (School_.School == nullptr || School_.Calendar == nullptr) return rows;
    KnownCommitmentContent known;
    if (Awakening_.Content != nullptr)
    {
        const auto loaded = LoadKnownCommitmentContent(*Awakening_.Content);
        if (!loaded.Ok()) return rows;
        known = loaded.Content;
    }
    const auto now = State_.Clock.Now();
    for (auto& row : rows)
    {
        if (row.Attended) continue;
        if (row.Booked && row.At >= now)
        {
            row.CanAttend = row.At == now && P407UFree(State_, School_, known, rows, row.Action, row.At);
            continue;
        }
        row.Booked = false; // Expired reservation; the next Book replaces its saved key.
        // Suggestions are relative to exam day. Reopening later never proposes a past slot.
        const auto preferred = GameClock::StartOfDay(CountdownEndInstant()) + row.Action.DayOffset * MinutesPerDay;
        const auto first = std::max(preferred, GameClock::StartOfDay(now));
        for (int offset = 0; offset < 32; ++offset)
        {
            const auto midnight = first + offset * MinutesPerDay;
            auto minute = row.Action.Minute;
            if (CalendarRequiresAttendance(*School_.Calendar, CalendarDayOfInstant(midnight)) && !School_.School->Blocks.empty())
                minute = std::max(minute, School_.School->Blocks.back().EndMinute);
            const auto at = midnight + minute;
            if (at < now || at > DayLoopMaxInstant - row.Action.DurationMinutes ||
                minute + row.Action.DurationMinutes > MinutesPerDay) continue;
            if (!P407UFree(State_, School_, known, rows, row.Action, at)) continue;
            row.At = at;
            row.CanBook = true;
            break;
        }
    }
    return rows;
}

std::optional<Rejection> WorldSim::ApplyP407UFollowup(const P407UFollowupCommand& command, CommandResult& result)
{
    const auto refuse = [](std::string code, std::string message) {
        return std::optional<Rejection>{{std::move(code), std::move(message)}};
    };
    if (!IsWorldDayInStep(State_)) return refuse("p407u.invalid_state", "The world and day must share one valid clock.");
    if (!IsWorldQuiescent(State_)) return refuse("world.unsettled", "Settle due events first.");
    if (!OpenConversationPersonId(State_).empty()) return refuse("time.conversation_open", "Finish the conversation first.");
    if (auto issue = SchoolPlayOpenRefusal(State_)) return issue;
    if (!ServiceShop_.empty() || State_.DayLoop.HasCommitment || State_.DayLoop.Asleep)
        return refuse("p407u.busy", "Finish the current activity, sleep or counter service first.");
    const auto rows = P407UFollowups();
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.Action.Id == command.ActionId; });
    if (found == rows.end()) return refuse("p407u.unavailable", "That next step is not available for the recorded result.");
    const auto& row = *found;
    if (row.Attended) return refuse("p407u.attended", "This session is already recorded.");
    if (row.At != command.ExpectedAt) return refuse("p407u.stale", "The appointment changed; read its current time again.");
    const auto bookPrefix = P407UKey(row.Action, ".book.");
    const auto publish = [&](const std::string& key, bool value) {
        State_.Flags[key] = value;
        result.Events.push_back({0, State_.Clock.Now(), WorldFlagChanged{key, value}});
    };
    if (command.Kind == P407UFollowupAction::Book)
    {
        if (!row.CanBook) return refuse("p407u.booked", "This appointment is already booked or has no available slot.");
        // Release an expired booking; attendance history is never removed.
        for (auto& [key, value] : State_.Flags)
            if (key.starts_with(bookPrefix) && value) publish(key, false);
        publish(bookPrefix + std::to_string(row.At), true);
    }
    else if (command.Kind == P407UFollowupAction::Cancel)
    {
        if (!row.Booked) return refuse("p407u.not_booked", "Book a session first.");
        for (auto& [key, value] : State_.Flags)
            if (key.starts_with(bookPrefix) && value) publish(key, false);
    }
    else if (command.Kind == P407UFollowupAction::Attend)
    {
        if (!row.Booked || !row.CanAttend) return refuse("p407u.not_due", "Attend at the booked time, or book another slot.");
        if (auto issue = ValidateTimeCharge(row.Action.DurationMinutes)) return issue;
        const auto target = State_.Clock.Now() + row.Action.DurationMinutes;
        ChargeTime(row.Action.DurationMinutes, result);
        if (result.Partial || State_.Clock.Now() != target)
            return refuse("p407u.charge_interrupted", "The session could not finish; no attendance was recorded.");
        publish(P407UKey(row.Action, ".done.") + std::to_string(row.At), true);
    }
    else return refuse("p407u.bad_action", "Unknown follow-up action.");
    return std::nullopt;
}
} // namespace PerfectWorld
