#include "ExamResult.h"

#include "ExamContract.h"

#include <algorithm>
#include <array>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::array<std::string_view, 19> P403Keys{
    "P403.exam.reason.discipline", "P403.exam.reason.common_judgment",
    "P403.exam.reason.quality", "P403.exam.reason.depth",
    "P403.exam.reason.defeat", "P403.exam.reason.unfinished",
    "P403.exam.reason.credential_best", "P403.exam.reason.credential_other",
    "P403.exam.reason.unobserved", "P403.exam.reason.total",
    "P403.exam.reason.cohort", "P403.exam.reason.decile",
    "P403.exam.reason.outcome", "P403.exam.reason.offer",
    "P403.exam.reason.support", "P403.exam.reason.followup",
    "P403.exam.reason.elapsed", "P403.exam.reason.route_delay",
    "P403.exam.reason.outside_time"
};
enum P403Key : std::size_t
{
    P403Discipline, P403Judgment, P403Quality, P403Depth, P403Defeat,
    P403Unfinished, P403Best, P403Other, P403Unobserved, P403Total,
    P403Cohort, P403Decile, P403Outcome, P403Offer, P403Support,
    P403Followup, P403Elapsed, P403Delay, P403Outside
};
P403ExamResult P403Refused(std::string_view code)
{
    P403ExamResult out;
    out.Issue.Code = code;
    return out;
}
void P403Reason(P403ExamResult& out, P403Key key, std::string_view dimension,
    std::size_t evidence, std::string_view target, std::int64_t value, std::int32_t weight = 0)
{
    out.Reasons.push_back({P403Keys[key], dimension, evidence, std::string(target), value, weight});
}
bool P403ReceiptMatches(const TowerSessionState& state, const TowerResult& score)
{
    switch (state.Participation)
    {
    case TowerParticipation::Completed: return score.Completed && !state.Active;
    case TowerParticipation::Ejected: return score.Ejected && !state.Active;
    case TowerParticipation::Withdrawn: return score.Withdrew;
    case TowerParticipation::Deferred:
    case TowerParticipation::Declined:
        return state.Attempts.empty() && !state.Active && state.Preparation.RouteDelayMinutes == 0;
    default: return false;
    }
}
void P403EvidenceReasons(P403ExamResult& out, const std::vector<TowerFloorAttempt>& attempts)
{
    std::vector<TowerFloorAttempt> prefix;
    auto previous = ResolveTower(prefix);
    const auto dimensions = P401ExamScoring();
    for (std::size_t i = 0; i < attempts.size(); ++i)
    {
        const auto& attempt = attempts[i];
        prefix.push_back(attempt);
        const auto current = ResolveTower(prefix);
        const auto delta = current.ScorePoints - previous.ScorePoints;
        P403Reason(out, P403Elapsed, "mission_minutes", i, {}, attempt.MissionMinutes);
        if (attempt.Defeated)
            P403Reason(out, P403Defeat, "score_points", i, {}, delta);
        else
        {
            const auto depth = (current.FloorsCleared - previous.FloorsCleared) * dimensions.DepthPerClear;
            P403Reason(out, P403Discipline, "discipline", i, {}, attempt.DisciplineMilli, dimensions.DisciplinePercent);
            P403Reason(out, P403Judgment, "common_judgment", i, {}, attempt.CommonJudgmentMilli, dimensions.JudgmentPercent);
            P403Reason(out, P403Quality, "score_points", i, {}, delta - depth);
            P403Reason(out, P403Depth, "score_points", i, {}, depth);
            const auto* route = FindTowerRoute(attempt.RouteId);
            for (const auto& credential : out.Score.Credentials)
                if (TowerRouteCovers(*route, credential.Discipline))
                    P403Reason(out, credential.RouteId == route->Id ? P403Best : P403Other,
                        "credential_milli", i, credential.Id, delta - depth);
        }
        previous = current;
    }
    if (out.Active)
    {
        P403Reason(out, P403Unfinished, "score_points", attempts.size(), {}, 0);
        P403Reason(out, P403Elapsed, "mission_minutes", attempts.size(), {}, out.Active->MissionMinutes);
    }
    for (const auto& definition : TowerCredentials())
        if (std::none_of(out.Score.Credentials.begin(), out.Score.Credentials.end(),
            [&](const auto& credential) { return credential.Id == definition.Id; }))
            P403Reason(out, P403Unobserved, "credential_milli", P403NoEvidence, definition.Id, 0);
}
} // namespace

std::span<const std::string_view> P403ExamReasonKeys() noexcept { return P403Keys; }

P403ExamResult P403ResolveExamResult(const TowerSessionState& state, const SchoolFollowupContent& followups)
{
    if (!P401ExamIsExit(state.Participation)) return P403Refused("P403.exam.not_terminal");
    // Canonical route views keep TowerResult independent of the state's string lifetime.
    std::vector<TowerFloorAttempt> attempts;
    for (const auto& attempt : state.Attempts)
    {
        const auto* route = FindTowerRoute(attempt.RouteId);
        if (!route) return P403Refused(TowerErrorCode(TowerError::UnknownRoute));
        attempts.push_back({route->Id, attempt.Method, attempt.DisciplineMilli,
            attempt.CommonJudgmentMilli, attempt.MissionMinutes, attempt.Defeated});
    }
    const auto score = ResolveTower(attempts);
    if (!score.Ok()) return P403Refused(TowerErrorCode(score.Error));
    if (!P403ReceiptMatches(state, score)) return P403Refused("P403.exam.inconsistent_exit");
    auto remaining = MaxSingleAdvanceMinutes - score.MissionMinutes;
    if (state.Preparation.RouteDelayMinutes < 0 || state.Preparation.RouteDelayMinutes > TowerWorldCapMinutes)
        return P403Refused("P403.exam.invalid_time");
    remaining -= state.Preparation.RouteDelayMinutes;
    if (remaining < 0) return P403Refused("P403.exam.invalid_time");
    if (state.Active)
    {
        const auto& active = *state.Active;
        const auto* route = FindTowerRoute(active.RouteId);
        if (!route || route->Floor != static_cast<std::int32_t>(attempts.size()) + 1 ||
            (active.Method != route->Method && !(route->AllowsCombat && active.Method == TowerMethod::Combat)))
            return P403Refused("P403.exam.invalid_active");
        if (active.MissionMinutes < 0 || active.MissionMinutes > remaining)
            return P403Refused("P403.exam.invalid_time");
    }
    const auto view = QueryTowerSession(state, {});
    P403ExamResult out;
    out.Participation = state.Participation;
    out.Score = view.Result;
    out.Outcome = SchoolFollowupOutcome(view);
    out.NextSteps = QuerySchoolFollowups(followups, view);
    if (std::none_of(out.NextSteps.begin(), out.NextSteps.end(), [&](const auto& action) {
        return action.Outcome == out.Outcome && action.Kind == "appointment" && !action.AppointmentId.empty();
    })) return P403Refused("P403.exam.missing_followup");
    // Ordering is stable even if equivalent loaded content has a different insertion order.
    std::sort(out.NextSteps.begin(), out.NextSteps.end(), [](const auto& a, const auto& b) { return a.Id < b.Id; });
    out.Attempts = state.Attempts;
    out.Active = state.Active;
    P403EvidenceReasons(out, attempts);
    P403Reason(out, P403Total, "score_points", P403NoEvidence, {}, out.Score.ScorePoints);
    P403Reason(out, P403Cohort, "national_rank", P403NoEvidence, {}, out.Score.NationalRank);
    P403Reason(out, P403Decile, "decile", P403NoEvidence, {}, out.Score.Decile);
    P403Reason(out, P403Outcome, "outcome", P403NoEvidence, out.Outcome, out.Score.Decile);
    P403Reason(out, P403Delay, "mission_minutes", P403NoEvidence, {}, state.Preparation.RouteDelayMinutes);
    P403Reason(out, P403Outside, "outside_minutes", P403NoEvidence, {}, out.Score.OutsideMinutes);
    P403Reason(out, P403Support, "support_envelope_paise", P403NoEvidence, {}, out.Score.SupportEnvelopePaise);
    for (const auto id : out.Score.OfferIds) P403Reason(out, P403Offer, "offer", P403NoEvidence, id, out.Score.Decile);
    for (const auto& action : out.NextSteps) P403Reason(out, P403Followup, "followup", P403NoEvidence, action.Id, 0);
    return out;
}
} // namespace PerfectWorld
