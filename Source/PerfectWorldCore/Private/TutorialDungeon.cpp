#include "TutorialDungeon.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::array<TutorialSituationDefinition, TutorialSituationCount> R92Situations = {{
    {"tutorial.stalled_crossing", "tutorial.station.crossing", {{
        {"tutorial.crossing.steady_classmate", ConductAction::Assist, "tutorial.outcome.classmate_crossed"},
        {"tutorial.crossing.explain_cycle", ConductAction::Observe, "tutorial.outcome.route_explained"},
        {"tutorial.crossing.wait_together", ConductAction::Hold, "tutorial.outcome.assisted_crossing"}}},
        "tutorial.crossing.safe_withdrawal"},
    {"tutorial.shifting_corridor", "tutorial.station.corridor", {{
        {"tutorial.corridor.move_barrier", ConductAction::Assist, "tutorial.outcome.group_shielded"},
        {"tutorial.corridor.brace_wall", ConductAction::Improvise, "tutorial.outcome.passage_braced"},
        {"tutorial.corridor.lead_long_route", ConductAction::Evade, "tutorial.outcome.group_rerouted"}}},
        "tutorial.corridor.safe_withdrawal"},
    {"tutorial.shared_delivery", "tutorial.station.delivery", {{
        {"tutorial.delivery.coordinate_carriers", ConductAction::Hold, "tutorial.outcome.supplies_delivered"},
        {"tutorial.delivery.investigate_marker", ConductAction::Observe, "tutorial.outcome.marker_corrected"},
        {"tutorial.delivery.supervised_handover", ConductAction::Withdraw, "tutorial.outcome.handover_arranged"}}},
        "tutorial.delivery.safe_withdrawal"}
}};

bool R92RoomForSession(const AwakeningState& awakening, GameInstant at)
{
    return at >= awakening.AppointmentAt && at < awakening.ExaminationAt &&
           awakening.ExaminationAt - at > TutorialSessionMinutes;
}

bool R92TimeAllowed(const TutorialDungeonState& tutorial, const AwakeningState& awakening, GameInstant now)
{
    return now >= 0 && now < awakening.ExaminationAt &&
           (!tutorial.LastActionAt || now >= *tutorial.LastActionAt);
}

const TutorialChoiceDefinition* R92Choice(const TutorialSituationDefinition& situation, std::string_view id)
{
    for (const auto& choice : situation.Choices)
        if (choice.Id == id)
            return &choice;
    return nullptr;
}

TutorialDungeonResult R92ActiveIssue(const TutorialDungeonState& tutorial,
                                    const AwakeningState& awakening, GameInstant now)
{
    if (!IsTutorialDungeonStateValid(tutorial, awakening))
        return {"tutorial.invalid_state"};
    if (awakening.Granted)
        return {"tutorial.already_granted"};
    if (tutorial.Phase != TutorialDungeonPhase::Active)
        return {"tutorial.not_active"};
    if (!R92TimeAllowed(tutorial, awakening, now))
        return {"tutorial.invalid_time"};
    return {};
}

TutorialDungeonResult R92Record(TutorialDungeonState& tutorial, AwakeningState& awakening,
                                std::string_view choiceId, ConductAction action, GameInstant now,
                                const std::vector<std::string>& parties, bool withdrawal)
{
    auto nextAwakening = awakening;
    const auto& situation = R92Situations[tutorial.NextSituation];
    const auto recorded = RecordConduct(nextAwakening.Conduct,
        {std::string(situation.Id), std::string(choiceId), action, TutorialConductStakes, now, parties});
    if (!recorded.Ok())
        return {"tutorial.conduct_rejected"};
    auto nextTutorial = tutorial;
    ++nextTutorial.NextSituation;
    nextTutorial.LastActionAt = now;
    nextTutorial.RecoveryPending = false;
    nextTutorial.Withdrawn = withdrawal;
    if (withdrawal || nextTutorial.NextSituation == TutorialSituationCount)
        nextTutorial.Phase = TutorialDungeonPhase::ReadyToGrant;
    awakening = std::move(nextAwakening);
    tutorial = std::move(nextTutorial);
    return {};
}
} // namespace

std::span<const TutorialSituationDefinition> TutorialSituations()
{
    return R92Situations;
}

bool IsTutorialDungeonStateValid(const TutorialDungeonState& tutorial, const AwakeningState& awakening)
{
    if (awakening.AppointmentAt < 0 || !R92RoomForSession(awakening, awakening.AppointmentAt))
        return false;
    const auto& makeUp = awakening.MakeUp;
    switch (makeUp.Phase)
    {
    case AwakeningMakeUpPhase::NotRequired:
    case AwakeningMakeUpPhase::Required:
        if (makeUp.At) return false;
        break;
    case AwakeningMakeUpPhase::Scheduled:
    case AwakeningMakeUpPhase::Completed:
        if (!makeUp.At || !R92RoomForSession(awakening, *makeUp.At)) return false;
        break;
    default: return false;
    }
    if (tutorial.Phase == TutorialDungeonPhase::NotStarted)
        return !tutorial.StartedAt && !tutorial.LastActionAt && !tutorial.CompletedAt &&
               tutorial.NextSituation == 0 && !tutorial.Withdrawn && !tutorial.RecoveryPending &&
               awakening.Conduct.Entries.empty() && !awakening.Granted &&
               makeUp.Phase != AwakeningMakeUpPhase::Completed;
    if (!awakening.Seeded || !tutorial.StartedAt || !tutorial.LastActionAt ||
        !R92RoomForSession(awakening, *tutorial.StartedAt) ||
        *tutorial.LastActionAt < *tutorial.StartedAt || *tutorial.LastActionAt >= awakening.ExaminationAt ||
        tutorial.NextSituation > TutorialSituationCount || makeUp.Phase == AwakeningMakeUpPhase::Required)
        return false;
    const GameInstant slot = makeUp.At.value_or(awakening.AppointmentAt);
    if (*tutorial.StartedAt != slot || awakening.Conduct.Entries.size() != tutorial.NextSituation)
        return false;
    GameInstant previousAt = *tutorial.StartedAt;
    for (std::size_t i = 0; i < tutorial.NextSituation; ++i)
    {
        const auto& entry = awakening.Conduct.Entries[i];
        const auto& record = entry.Record;
        const auto& situation = R92Situations[i];
        const bool withdrewHere = tutorial.Withdrawn && i + 1 == tutorial.NextSituation &&
                                  record.ChoiceId == situation.WithdrawalChoiceId;
        const auto* choice = R92Choice(situation, record.ChoiceId);
        if (record.TestId != situation.Id || (!withdrewHere && !choice) ||
            (tutorial.Withdrawn && i + 1 == tutorial.NextSituation &&
             tutorial.NextSituation < TutorialSituationCount && !withdrewHere) ||
            record.Action != (withdrewHere ? ConductAction::Withdraw : choice->Action) ||
            record.Stakes != TutorialConductStakes || record.At < previousAt ||
            record.At > *tutorial.LastActionAt || entry.Points != ConductActionEvidence(record.Action, record.Stakes))
            return false;
        previousAt = record.At;
    }
    if (tutorial.Phase == TutorialDungeonPhase::Active)
        return tutorial.NextSituation < TutorialSituationCount && !tutorial.Withdrawn &&
               !tutorial.CompletedAt && !awakening.Granted && makeUp.Phase != AwakeningMakeUpPhase::Completed;
    if ((tutorial.Phase != TutorialDungeonPhase::ReadyToGrant && tutorial.Phase != TutorialDungeonPhase::Completed) ||
        tutorial.RecoveryPending || tutorial.NextSituation == 0 ||
        (!tutorial.Withdrawn && tutorial.NextSituation != TutorialSituationCount))
        return false;
    if (tutorial.Phase == TutorialDungeonPhase::ReadyToGrant)
        return !tutorial.CompletedAt && !awakening.Granted && makeUp.Phase != AwakeningMakeUpPhase::Completed;
    return tutorial.CompletedAt && awakening.Granted && *tutorial.CompletedAt == awakening.Granted->At &&
           *tutorial.CompletedAt == *tutorial.LastActionAt &&
           *tutorial.CompletedAt >= *tutorial.StartedAt + TutorialSessionMinutes &&
           (!makeUp.At || makeUp.Phase == AwakeningMakeUpPhase::Completed);
}

TutorialDungeonResult MarkTutorialAbsence(TutorialDungeonState& tutorial, AwakeningState& awakening, GameInstant now)
{
    if (!IsTutorialDungeonStateValid(tutorial, awakening) || !awakening.Seeded)
        return {"tutorial.invalid_state"};
    if (tutorial.Phase != TutorialDungeonPhase::NotStarted)
        return {"tutorial.session_started"};
    if (awakening.MakeUp.Phase == AwakeningMakeUpPhase::Required)
        return {"tutorial.make_up_required"};
    if (now < awakening.MakeUp.At.value_or(awakening.AppointmentAt))
        return {"tutorial.invalid_time"};
    awakening.MakeUp = {AwakeningMakeUpPhase::Required, std::nullopt};
    return {};
}

TutorialDungeonResult ScheduleTutorialMakeUp(TutorialDungeonState& tutorial, AwakeningState& awakening,
                                            GameInstant at, GameInstant now)
{
    if (!IsTutorialDungeonStateValid(tutorial, awakening) || !awakening.Seeded)
        return {"tutorial.invalid_state"};
    if (tutorial.Phase != TutorialDungeonPhase::NotStarted ||
        (awakening.MakeUp.Phase != AwakeningMakeUpPhase::Required &&
         awakening.MakeUp.Phase != AwakeningMakeUpPhase::Scheduled))
        return {"tutorial.make_up_not_required"};
    if (now < awakening.AppointmentAt || at < now || !R92RoomForSession(awakening, at))
        return {"tutorial.invalid_slot"};
    awakening.MakeUp = {AwakeningMakeUpPhase::Scheduled, at};
    return {};
}

TutorialDungeonResult BeginTutorialDungeon(TutorialDungeonState& tutorial, const AwakeningState& awakening, GameInstant now)
{
    if (!IsTutorialDungeonStateValid(tutorial, awakening) || !awakening.Seeded)
        return {"tutorial.invalid_state"};
    if (tutorial.Phase != TutorialDungeonPhase::NotStarted)
        return {"tutorial.session_started"};
    if (awakening.MakeUp.Phase == AwakeningMakeUpPhase::Required)
        return {"tutorial.make_up_required"};
    if (now != awakening.MakeUp.At.value_or(awakening.AppointmentAt))
        return {"tutorial.appointment_required"};
    tutorial.Phase = TutorialDungeonPhase::Active;
    tutorial.StartedAt = now;
    tutorial.LastActionAt = now;
    return {};
}

TutorialDungeonResult CompleteTutorialChoice(TutorialDungeonState& tutorial, AwakeningState& awakening,
    std::string_view stationId, std::string_view choiceId, GameInstant now, const std::vector<std::string>& parties)
{
    const auto issue = R92ActiveIssue(tutorial, awakening, now);
    if (!issue.Ok()) return issue;
    if (tutorial.RecoveryPending) return {"tutorial.assistance_pending"};
    const auto& situation = R92Situations[tutorial.NextSituation];
    const auto* choice = R92Choice(situation, choiceId);
    if (stationId != situation.StationId || !choice) return {"tutorial.wrong_choice_or_station"};
    return R92Record(tutorial, awakening, choiceId, choice->Action, now, parties, false);
}

TutorialDungeonResult RequestTutorialAssistance(TutorialDungeonState& tutorial, const AwakeningState& awakening, GameInstant now)
{
    const auto issue = R92ActiveIssue(tutorial, awakening, now);
    if (!issue.Ok()) return issue;
    if (tutorial.RecoveryPending) return {"tutorial.assistance_pending"};
    tutorial.RecoveryPending = true;
    tutorial.LastActionAt = now;
    return {};
}

TutorialDungeonResult RecoverTutorialDungeon(TutorialDungeonState& tutorial, const AwakeningState& awakening, GameInstant now)
{
    const auto issue = R92ActiveIssue(tutorial, awakening, now);
    if (!issue.Ok()) return issue;
    if (!tutorial.RecoveryPending) return {"tutorial.assistance_not_pending"};
    tutorial.RecoveryPending = false;
    tutorial.LastActionAt = now;
    return {};
}

TutorialDungeonResult WithdrawTutorialDungeon(TutorialDungeonState& tutorial, AwakeningState& awakening, GameInstant now)
{
    if (!IsTutorialDungeonStateValid(tutorial, awakening)) return {"tutorial.invalid_state"};
    if (awakening.Granted) return {"tutorial.already_granted"};
    if (!R92TimeAllowed(tutorial, awakening, now)) return {"tutorial.invalid_time"};
    if (tutorial.Phase == TutorialDungeonPhase::ReadyToGrant && !tutorial.Withdrawn)
    {
        tutorial.Withdrawn = true;
        tutorial.LastActionAt = now;
        return {};
    }
    const auto issue = R92ActiveIssue(tutorial, awakening, now);
    if (!issue.Ok()) return issue;
    const auto& situation = R92Situations[tutorial.NextSituation];
    return R92Record(tutorial, awakening, situation.WithdrawalChoiceId, ConductAction::Withdraw, now, {}, true);
}

TutorialDungeonResult GrantTutorialAwakening(TutorialDungeonState& tutorial, AwakeningState& awakening,
                                            const AbilityRecipeSet& recipes, GameInstant now)
{
    if (awakening.Granted) return {"tutorial.already_granted"};
    if (!IsTutorialDungeonStateValid(tutorial, awakening)) return {"tutorial.invalid_state"};
    if (tutorial.Phase != TutorialDungeonPhase::ReadyToGrant) return {"tutorial.not_ready"};
    if (!R92TimeAllowed(tutorial, awakening, now) || now < *tutorial.StartedAt + TutorialSessionMinutes)
        return {"tutorial.invalid_time"};
    const auto input = BuildAwakeningInput(awakening);
    const auto resolved = ResolveAwakening(input, recipes);
    if (!resolved.Ok()) return {"tutorial.resolve_failed"};
    const auto starting = AwakenedStartingProfile(resolved, GameClock::DayNumber(now));
    if (!starting) return {"tutorial.profile_failed"};
    auto next = awakening;
    next.Granted = AwakeningGrant{now, input, resolved, *starting};
    if (next.MakeUp.Phase == AwakeningMakeUpPhase::Scheduled)
        next.MakeUp.Phase = AwakeningMakeUpPhase::Completed;
    awakening = std::move(next);
    tutorial.Phase = TutorialDungeonPhase::Completed;
    tutorial.CompletedAt = now;
    tutorial.LastActionAt = now;
    return {};
}
} // namespace PerfectWorld
