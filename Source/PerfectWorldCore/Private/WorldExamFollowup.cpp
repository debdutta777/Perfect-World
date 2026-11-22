#include "WorldExamFollowup.h"

#include "WorldReducer.h"

#include "KnownCommitments.h"
#include "LaneJournal.h"
#include "WorldExamSittings.h"

#include <algorithm>
#include <charconv>
#include <limits>

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

std::string P502Scope(const WorldState& state, const SchoolFollowupAction& action)
{
    return P407UKey(action, ".p502.s") + std::to_string(P409CurrentSitting(state)) + ".";
}

std::vector<P502TaskChoice> P502Choices(const WorldState& state, std::string_view task)
{
    if (task == "term_plan") return {
        {"explore_study", {"term.explore.study", "term.decline.hunter_enquiry"}},
        {"explore_civic", {"term.explore.civic", "term.decline.university_enquiry"}}};
    if (task == "stream_review") return {
        {"science", {"stream.tentative.science", "timetable.preview.science_practical"}},
        {"commerce", {"stream.tentative.commerce", "timetable.preview.accounting"}},
        {"humanities", {"stream.tentative.humanities", "timetable.preview.civics"}}};
    if (task == "practical_step") return {
        {"reseat_connector", {"practical.connector_reseated", "workshop.reference.debashis_pal"}},
        {"secure_housing", {"practical.housing_secured", "workshop.reference.debashis_pal"}}};
    if (task == "pacing_plan")
    {
        std::vector<P502TaskChoice> choices{
            {"lighter_evenings", {"pacing.lighter_evenings.two_weeks"}},
            {"usual_rhythm", {"pacing.usual_rhythm"}}};
        // List only an earned credential, including the legitimate no-credential withdrawal.
        const auto view = QueryTowerSession(state.Examination, {});
        for (auto& choice : choices)
            if (!view.Result.Credentials.empty()) choice.Records.push_back(std::string(view.Result.Credentials.front().Id));
        return choices;
    }
    if (task == "volunteer_enquiry") return {
        {"reading_table", {"volunteer.request.reading_table", "calendar.proposed.next_week.reading_table"}},
        {"patient_cards", {"volunteer.request.patient_cards", "calendar.proposed.next_week.patient_cards"}}};
    if (task == "route_sheet") return {
        {"science_reading", {"boards.science", "civic.reading_table", "timetable.note.board_subjects"}},
        {"humanities_civic", {"boards.humanities", "civic.neighbourhood_records", "timetable.note.board_subjects"}}};
    return {};
}

P407UFollowupView P407UStored(const WorldState& state, const SchoolFollowupAction& action)
{
    P407UFollowupView row;
    row.Action = action;
    row.P502Choices = P502Choices(state, action.P502TaskId);
    if (!row.P502Choices.empty()) row.P502Stage = P502TaskStage::Offered;
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
    if (!row.P502Choices.empty())
    {
        const auto scope = P502Scope(state, action);
        const auto revisionPrefix = scope + "revision.";
        for (auto it = state.Flags.lower_bound(revisionPrefix);
            it != state.Flags.end() && it->first.starts_with(revisionPrefix); ++it)
        {
            if (!it->second) continue;
            const auto digits = std::string_view(it->first).substr(revisionPrefix.size());
            std::uint64_t revision = 0;
            const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), revision);
            if (parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size() &&
                std::to_string(revision) == digits) row.P502Revision = std::max(row.P502Revision, revision);
        }
        const auto progress = scope + "at." + std::to_string(row.At) + ".";
        if (P407UFlag(state, progress + "arrived"))
        {
            row.Attended = true;
            row.P502Stage = P502TaskStage::Arrived;
            if (P407UFlag(state, progress + "paused")) row.P502Stage = P502TaskStage::Paused;
            if (P407UFlag(state, progress + "active")) row.P502Stage = P502TaskStage::Active;
        }
        else if (row.Attended) row.P502Stage = P502TaskStage::LegacyAttendance;
        for (const auto& choice : row.P502Choices)
            if (P407UFlag(state, scope + "completed." + choice.Id))
            {
                row.P502Stage = P502TaskStage::Completed;
                row.P502Record = choice;
                break;
            }
    }
    return row;
}

bool P407UOverlap(GameInstant a, std::int64_t minutes, GameInstant b, std::int64_t other)
{
    return a < b + other && b < a + minutes;
}

std::vector<std::string> P502Conflicts(const WorldState& world, const DayLoopCatalogues& school,
    const KnownCommitmentContent& known, const std::vector<P407UFollowupView>& rows,
    const SchoolFollowupAction& action, GameInstant at)
{
    std::vector<std::string> conflicts;
    const auto day = CalendarDayOfInstant(at);
    const auto midnight = GameClock::StartOfDay(at);
    const bool open = CalendarRequiresAttendance(*school.Calendar, day);
    if (action.Destination == "place.school" && !open) conflicts.push_back("school.closed");
    // A follow-up never replaces class attendance, even for home/workshop commitments.
    if (open && !school.School->Blocks.empty() &&
        P407UOverlap(at, action.DurationMinutes, midnight + school.School->Blocks.front().StartMinute,
            school.School->Blocks.back().EndMinute - school.School->Blocks.front().StartMinute)) conflicts.push_back("school.lessons");
    for (const auto& row : rows)
        if (row.Action.Id != action.Id && row.Booked &&
            (!row.Attended || (!row.P502Choices.empty() && row.P502Stage != P502TaskStage::Completed &&
                row.P502Stage != P502TaskStage::LegacyAttendance)) &&
            P407UOverlap(at, action.DurationMinutes, row.At, row.Action.DurationMinutes)) conflicts.push_back(row.Action.Id);
    for (const auto& promise : KnownTimedPromises(LaneJournalToday(world, GameClock::DayNumber(at)), known))
        if (P407UOverlap(at, action.DurationMinutes, promise.Commitment.Start, promise.Commitment.Minutes))
            conflicts.push_back(promise.AgreementId);
    if (GameClock::DayNumber(at) == GameClock::DayNumber(world.Clock.Now()))
    {
        if (at == world.Clock.Now() && world.DayLoop.HasCommitment)
            conflicts.push_back(world.DayLoop.Commitment.ActivityId);
        // Timed intentions have authoritative activity durations when available.
        for (const auto& plan : world.DayLoop.Plan)
        {
            const auto* activity = school.Activities != nullptr ? school.Activities->Find(plan.ActivityId) : nullptr;
            if (activity != nullptr && P407UOverlap(at, action.DurationMinutes,
                midnight + plan.StartMinute, activity->DurationMinutes)) conflicts.push_back(plan.ActivityId);
        }
    }
    return conflicts;
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
    const auto reservations = rows; // Suggestions must not depend on iteration order.
    const bool idle = ServiceShop_.empty() && !State_.DayLoop.HasCommitment && !State_.DayLoop.Asleep &&
        OpenConversationPersonId(State_).empty() && !SchoolPlayOpenRefusal(State_) && IsWorldQuiescent(State_);
    for (auto& row : rows)
    {
        if (row.P502Stage == P502TaskStage::Completed || row.P502Stage == P502TaskStage::LegacyAttendance ||
            (row.Attended && row.P502Choices.empty())) continue;
        const bool reserved = row.Booked && row.At >= now;
        const auto preferred = GameClock::StartOfDay(P409SittingAt(P409CurrentSitting(State_))) +
            row.Action.DayOffset * MinutesPerDay;
        row.P502Conflicts = P502Conflicts(State_, School_, known, reservations, row.Action,
            reserved ? row.At : std::max(preferred, GameClock::StartOfDay(now)) + row.Action.Minute);
        if (reserved && row.At == now)
        {
            if (!OpenConversationPersonId(State_).empty()) row.P502Conflicts.push_back("session.conversation");
            if (SchoolPlayOpenRefusal(State_)) row.P502Conflicts.push_back("session.school");
            if (!ServiceShop_.empty()) row.P502Conflicts.push_back("session.service");
            if (State_.DayLoop.Asleep) row.P502Conflicts.push_back("session.sleep");
        }
        if (reserved)
        {
            const bool ready = row.At == now && row.P502Conflicts.empty() && idle;
            row.CanAttend = ready && !row.Attended;
            row.P502CanStart = ready && row.P502Stage == P502TaskStage::Arrived;
            row.P502CanResume = ready && row.P502Stage == P502TaskStage::Paused;
            row.P502CanResolve = ready && row.P502Stage == P502TaskStage::Active;
        }
        else
        {
            row.Booked = false; // A missed task, including arrival without resolution, can be rebooked.
            row.Attended = false;
            if (!row.P502Choices.empty()) row.P502Stage = P502TaskStage::Offered;
        }
        const auto first = std::max(preferred, GameClock::StartOfDay(now));
        for (int offset = 0; offset < 32; ++offset)
        {
            const auto midnight = first + offset * MinutesPerDay;
            if (row.Action.Destination == "place.school" &&
                !CalendarRequiresAttendance(*School_.Calendar, CalendarDayOfInstant(midnight))) continue;
            auto minute = row.Action.Minute;
            if (midnight == GameClock::StartOfDay(now))
                minute = std::max(minute, static_cast<std::int32_t>(now - midnight));
            if (CalendarRequiresAttendance(*School_.Calendar, CalendarDayOfInstant(midnight)) && !School_.School->Blocks.empty())
                minute = std::max(minute, School_.School->Blocks.back().EndMinute);
            // Search within the day too: a standing daily promise must not hide every later free hour.
            for (; minute + row.Action.DurationMinutes <= MinutesPerDay; ++minute)
            {
                const auto at = midnight + minute;
                if (at < now || at > DayLoopMaxInstant - row.Action.DurationMinutes) continue;
                // Old paid attendance remains history. Rebooking it must use a fresh instant.
                if (!row.P502Choices.empty() && P407UFlag(State_, P407UKey(row.Action, ".done.") + std::to_string(at))) continue;
                if (at == now && !idle) continue;
                if (!P502Conflicts(State_, School_, known, reservations, row.Action, at).empty()) continue;
                row.P502AlternativeAt = at;
                if (!reserved)
                {
                    row.At = at;
                    row.CanBook = true;
                }
                break;
            }
            if (row.P502AlternativeAt != 0) break;
        }
    }
    // Only disclose alternatives that are actually offered at this instant. Resolve uses this same view.
    for (auto& row : rows)
    {
        if (!row.P502Choices.empty() && row.Booked && row.At == now)
        {
            auto withoutOwnReservation = reservations;
            for (auto& candidate : withoutOwnReservation)
                if (candidate.Action.Id == row.Action.Id) candidate.Booked = false;
            for (const auto& other : rows)
            {
                if (other.Action.Id == row.Action.Id || other.Booked || other.Attended || !other.CanBook) continue;
                // Recompute without this task's reservation, which may have moved the suggestion.
                const auto preferred = GameClock::StartOfDay(P409SittingAt(P409CurrentSitting(State_))) +
                    other.Action.DayOffset * MinutesPerDay + other.Action.Minute;
                const auto candidateAt = std::max(preferred, GameClock::StartOfDay(now) + other.Action.Minute);
                if (candidateAt < now || candidateAt > DayLoopMaxInstant - other.Action.DurationMinutes ||
                    !P407UOverlap(row.At, row.Action.DurationMinutes, candidateAt, other.Action.DurationMinutes) ||
                    !P502Conflicts(State_, School_, known, withoutOwnReservation, other.Action, candidateAt).empty()) continue;
                row.P502Alternatives.push_back({other.Action.Id, "source.school_followup", now, candidateAt,
                    other.Action.DurationMinutes, true, {}});
            }
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
    if (row.P502Stage == P502TaskStage::Completed) return refuse("p502.completed", "This task already has its record.");
    if (row.P502Stage == P502TaskStage::LegacyAttendance && command.Kind != P407UFollowupAction::Cancel)
        return refuse("p407u.attended", "Historical attendance is already paid; cancel and book a new task session if wanted.");
    if (row.Attended && row.P502Choices.empty()) return refuse("p407u.attended", "This session is already recorded.");
    if (row.At != command.ExpectedAt) return refuse("p407u.stale", "The appointment changed; read its current time again.");
    if (!row.P502Choices.empty() && row.P502Revision != command.P502ExpectedRevision)
        return refuse("p502.stale", "The task changed; read its current revision again.");
    if (row.P502Revision == std::numeric_limits<std::uint64_t>::max())
        return refuse("p502.revision_limit", "The task revision cannot advance.");
    const auto bookPrefix = P407UKey(row.Action, ".book.");
    const auto scope = P502Scope(State_, row.Action);
    const auto progress = scope + "at." + std::to_string(row.At) + ".";
    const auto publish = [&](const std::string& key, bool value) {
        State_.Flags[key] = value;
        result.Events.push_back({0, State_.Clock.Now(), WorldFlagChanged{key, value}});
    };
    if (command.Kind != P407UFollowupAction::P502Resolve && !command.P502ChoiceId.empty())
        return refuse("p502.choice_unexpected", "Only task resolution takes a choice.");
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
        if (!row.P502Choices.empty())
        {
            publish(progress + "active", false);
            publish(progress + "paused", false);
            // Arrival is historical. A new booking must have a different generation (P5-03).
            // Prevent a same-instant cancellation from resurrecting it.
            publish(progress + "arrived", false);
        }
    }
    else if (command.Kind == P407UFollowupAction::Attend)
    {
        if (row.Attended) return refuse("p407u.attended", "This session is already recorded.");
        if (!row.Booked || !row.CanAttend) return refuse("p407u.not_due", "Attend at the booked time, or book another slot.");
        if (!row.P502Choices.empty())
        {
            // Arrival and task work are distinct. Reading/choosing is free; Resolve pays once.
            publish(progress + "arrived", true);
            publish(scope + "revision." + std::to_string(row.P502Revision + 1), true);
            return std::nullopt;
        }
        if (auto issue = ValidateTimeCharge(row.Action.DurationMinutes)) return issue;
        const auto target = State_.Clock.Now() + row.Action.DurationMinutes;
        ChargeTime(row.Action.DurationMinutes, result);
        if (result.Partial || State_.Clock.Now() != target)
            return refuse("p407u.charge_interrupted", "The session could not finish; no attendance was recorded.");
        publish(P407UKey(row.Action, ".done.") + std::to_string(row.At), true);
    }
    else if (command.Kind == P407UFollowupAction::P502Start || command.Kind == P407UFollowupAction::P502Resume)
    {
        if (row.P502Choices.empty()) return refuse("p502.no_task", "This is an attendance-only continuation.");
        const bool start = command.Kind == P407UFollowupAction::P502Start;
        if (!(start ? row.P502CanStart : row.P502CanResume))
            return refuse("p502.not_ready", "Use the current booked slot after arrival, or cancel and rebook.");
        publish(progress + "paused", false);
        publish(progress + "active", true);
    }
    else if (command.Kind == P407UFollowupAction::P502Leave)
    {
        if (row.P502Stage != P502TaskStage::Active) return refuse("p502.not_active", "No task is open to leave.");
        publish(progress + "active", false);
        publish(progress + "paused", true);
    }
    else if (command.Kind == P407UFollowupAction::P502Resolve)
    {
        if (row.P502Stage != P502TaskStage::Active) return refuse("p502.not_active", "Start or resume this task first.");
        if (!row.P502CanResolve)
            return refuse("p502.conflict", "The task window is unavailable; cancel and use the disclosed alternative slot.");
        const auto choice = std::find_if(row.P502Choices.begin(), row.P502Choices.end(),
            [&](const auto& item) { return item.Id == command.P502ChoiceId; });
        if (choice == row.P502Choices.end()) return refuse("p502.choice", "Choose one of this task's current actions.");
        if (auto issue = ValidateTimeCharge(row.Action.DurationMinutes)) return issue;
        // The authored appointment is the activity. Reuse the receipt owner; no learning allowance is invented.
        auto receipts = State_.DayLoop.Receipts;
        const auto recorded = RecordCommitment(receipts,
            {row.Action.Id, row.At, row.At, row.Action.DurationMinutes, {}}, row.P502Alternatives);
        if (!recorded.Ok()) return refuse("p502.receipt", std::string(ReceiptErrorCode(recorded.Error)));
        auto condition = State_.DayLoop.Condition;
        if (!AdvanceCondition(condition, ConditionActivity::Awake, row.Action.DurationMinutes))
            return refuse("p502.condition", "The task condition cost could not be applied.");
        const auto target = row.At + row.Action.DurationMinutes;
        ChargeTime(row.Action.DurationMinutes, result);
        if (result.Partial || State_.Clock.Now() != target)
            return refuse("p407u.charge_interrupted", "The task could not finish; no record was applied.");
        State_.DayLoop.Receipts = std::move(receipts);
        State_.DayLoop.Condition = condition;
        publish(progress + "active", false);
        publish(scope + "completed." + choice->Id, true);
    }
    else return refuse("p407u.bad_action", "Unknown follow-up action.");
    if (!row.P502Choices.empty()) publish(scope + "revision." + std::to_string(row.P502Revision + 1), true);
    return std::nullopt;
}
} // namespace PerfectWorld
