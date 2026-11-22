#include "WorldExamSittings.h"

#include "OpportunityReceipts.h"
#include "SaveSlotSummary.h"

#include <algorithm>
#include <utility>

// P4-09: annual sittings, the age-twenty limit, the late-entry window with its reduced allowance, and the content-free save
// rules of one stored session (moved here from WorldExamSave.cpp and generalised from the single 2043 sitting).
namespace PerfectWorld
{
namespace
{
bool P409MethodAllowed(const TowerRouteDefinition& route, TowerMethod method)
{
    return method == route.Method || (method == TowerMethod::Combat && route.AllowsCombat);
}

bool P409ChoicesValid(const std::vector<std::string>& choices, std::size_t low)
{
    return choices.size() >= low && choices.size() <= TowerRunnerBeatLimit &&
           std::all_of(choices.begin(), choices.end(), [](const std::string& id) { return IsReceiptIdWellFormed(id); });
}

bool P409AttemptValid(const TowerSessionAttempt& a, std::size_t floor)
{
    const TowerRouteDefinition* route = FindTowerRoute(a.RouteId);
    const auto beats = static_cast<std::int64_t>(a.ChoiceIds.size());
    return route != nullptr && route->Floor == static_cast<std::int32_t>(floor) && P409MethodAllowed(*route, a.Method) &&
           IsReceiptIdWellFormed(a.ScenarioId) && P409ChoicesValid(a.ChoiceIds, 1) && a.DisciplineMilli >= 0 &&
           a.DisciplineMilli <= TowerScoreMaxMilli && a.CommonJudgmentMilli >= 0 && a.CommonJudgmentMilli <= TowerScoreMaxMilli &&
           a.MissionMinutes >= beats && a.MissionMinutes <= 60 * beats;
}

bool P409ActiveValid(const TowerRunnerRun& r, std::size_t floor)
{
    const TowerRouteDefinition* route = FindTowerRoute(r.RouteId);
    const auto beats = static_cast<std::int64_t>(r.Beat);
    return route != nullptr && route->Floor == static_cast<std::int32_t>(floor) && P409MethodAllowed(*route, r.Method) &&
           IsReceiptIdWellFormed(r.ScenarioId) && r.Beat == r.Choices.size() && r.Beat < TowerRunnerBeatLimit &&
           P409ChoicesValid(r.Choices, 0) && r.Conditions.Reserve >= 0 && r.Conditions.Reserve <= 10 &&
           r.Conditions.MinuteDiscount >= 0 && r.Conditions.MinuteDiscount <= 2 && r.DisciplineSum >= 0 &&
           r.DisciplineSum <= TowerScoreMaxMilli * beats && r.JudgmentSum >= 0 && r.JudgmentSum <= TowerScoreMaxMilli * beats &&
           r.MissionMinutes >= beats && r.MissionMinutes <= 60 * beats;
}

bool P409PreparationValid(const WorldState& world, const TowerSessionState& s, std::uint32_t sitting, bool live)
{
    const TowerPreparationSnapshot& p = s.Preparation;
    if (p.Composure < 0 || p.Composure > CountdownScoreMax || p.RouteDelayMinutes < 0 || p.RouteDelayMinutes > TowerWorldCapMinutes ||
        std::any_of(p.Preparation.begin(), p.Preparation.end(), [](std::int32_t v) { return v < 0 || v > CountdownScoreMax; }))
        return false;
    const P402ExamPreparation authority = P409SittingPreparation(world, sitting);
    if (!authority.Ok())
        return false;
    if (authority.ZeroByCompatibility)
        return p == TowerPreparationSnapshot{};
    // Composure reads today's condition, so it is exact only while the clock stands at the live session's entry instant.
    return p.Preparation == authority.Profile.Preparation && p.RouteDelayMinutes == authority.Profile.RouteDelayMinutes &&
           (!live || p.Composure == authority.Profile.Composure);
}
} // namespace

GameInstant P409SittingAt(std::uint32_t sitting) noexcept
{
    return GameClock::FromCalendar(P409FirstSittingYear + static_cast<std::int32_t>(std::min<std::uint32_t>(sitting, 100)), 2, 2, 6, 0);
}

std::optional<std::uint32_t> P409SittingOpenedBy(GameInstant now) noexcept
{
    if (now < P409SittingAt(0))
        return std::nullopt;
    std::uint32_t sitting = 0;
    while (sitting <= P409MaxSitting && P409SittingAt(sitting + 1) <= now)
        ++sitting;
    return sitting;
}

bool P409SittingAllowsAge(std::uint32_t sitting, const DateOfBirth* birth) noexcept
{
    if (sitting > P409MaxSitting)
        return false;
    return AgeOnInstant(birth != nullptr ? *birth : EarliestDateOfBirth, P409SittingAt(sitting)) <= P409AgeLimitYears;
}

std::uint32_t P409CurrentSitting(const WorldState& world) noexcept
{
    return static_cast<std::uint32_t>(world.ExaminationEarlier.size());
}

std::int64_t P409EntryLateness(const WorldState& world) noexcept
{
    if (world.Examination.Participation != TowerParticipation::Entered)
        return 0;
    return std::max<std::int64_t>(0, world.Clock.Now() - P409SittingAt(P409CurrentSitting(world)));
}

std::int64_t P409OutsideAllowance(const WorldState& world) noexcept
{
    return TowerWorldCapMinutes - std::min<std::int64_t>(P409EntryLateness(world), TowerWorldCapMinutes);
}

P402ExamPreparation P409SittingPreparation(const WorldState& world, std::uint32_t sitting)
{
    P402ExamPreparation out = P402FinalPreparation(world);
    if (out.Ok() && sitting > 0)
        out.Profile.RouteDelayMinutes = 0;
    return out;
}

bool P413DeferralUsed(const WorldState& world) noexcept
{
    return world.Examination.Participation == TowerParticipation::Deferred ||
        std::any_of(world.ExaminationEarlier.begin(), world.ExaminationEarlier.end(), [](const auto& session) {
            return session.Participation == TowerParticipation::Deferred;
        });
}

std::optional<std::uint32_t> P413AvailableSitting(const WorldState& world, GameInstant now) noexcept
{
    std::uint32_t available = 0;
    if (world.Awakening.Granted)
    {
        // Awakened BY the opening, not merely by today's query. An absent student retains the first annual sitting
        // whose opening follows their eventual grant. Today's make-up orchestration requires an earlier grant;
        // this rule also covers a future post-first-sitting grant path without inventing saved authority.
        while (available <= P409MaxSitting && world.Awakening.Granted->At > P409SittingAt(available))
            ++available;
    }
    else
        available = P409SittingOpenedBy(now).value_or(0);

    bool deferred = false;
    const auto consume = [&](const TowerSessionState& session, std::uint32_t sitting) {
        if (session.Participation == TowerParticipation::Unset)
            return true;
        if (session.Participation != TowerParticipation::Deferred || deferred)
            return false; // Entered, every terminal result and Declined consume the one chance, even in legacy archives.
        if (!world.Awakening.Granted)
            available = sitting;
        if (sitting != available)
            return false; // A legacy deferral after an already missed eligible sitting grants no new chance.
        deferred = true;
        available = sitting + 1;
        return true;
    };
    for (std::uint32_t sitting = 0; sitting < P409CurrentSitting(world); ++sitting)
        if (!consume(world.ExaminationEarlier[sitting], sitting))
            return std::nullopt;
    if (!consume(world.Examination, P409CurrentSitting(world)))
        return std::nullopt;
    if ((world.Awakening.Granted || deferred) && now > P409SittingAt(available) + P409LateEntryMinutes)
        return std::nullopt;
    return available;
}

std::uint32_t P409DecisionTarget(const WorldState& world, GameInstant now) noexcept
{
    const std::uint32_t current = P409CurrentSitting(world);
    const auto available = P413AvailableSitting(world, now);
    return available && *available > current && P409SittingAt(*available) <= now ? *available : current;
}

void P409ArchiveUntil(const WorldState& world, std::vector<TowerSessionState>& earlier, TowerSessionState& session, std::uint32_t target)
{
    const auto available = P413AvailableSitting(world, world.Clock.Now());
    if (!available || target != *available || target != P409DecisionTarget(world, world.Clock.Now()) || target > P409MaxSitting)
        return;
    while (earlier.size() < target)
    {
        earlier.push_back(std::move(session));
        session = {};
    }
}

bool P409SessionValidInWorld(const WorldState& world, const TowerSessionState& s, std::uint32_t sitting, bool current)
{
    if (sitting > P409MaxSitting)
        return false;
    const GameInstant at = P409SittingAt(sitting);
    const GameInstant now = world.Clock.Now();
    TowerSessionState empty;
    empty.Participation = s.Participation;
    switch (s.Participation)
    {
    case TowerParticipation::Unset: return s == empty && (!current || sitting == 0); // a later sitting opens only by a decision
    case TowerParticipation::Deferred:
    case TowerParticipation::Declined: return s == empty && now >= at;
    case TowerParticipation::Entered:
        if (!current)
            return false;
        break;
    case TowerParticipation::Completed:
    case TowerParticipation::Ejected:
    case TowerParticipation::Withdrawn: break;
    default: return false;
    }
    const bool live = current && s.Participation == TowerParticipation::Entered;
    if (!world.Awakening.Granted || now < at || !P409PreparationValid(world, s, sitting, live))
        return false;
    const std::uint64_t opened = s.Attempts.size() + (s.Active ? 1U : 0U);
    if (s.Attempts.size() > static_cast<std::size_t>(TowerFloorCount) || s.NextRunner != opened + 1 || s.Runner != opened)
        return false;
    for (std::size_t i = 0; i < s.Attempts.size(); ++i)
    {
        const bool last = i + 1 == s.Attempts.size();
        if (!P409AttemptValid(s.Attempts[i], i + 1) || (s.Attempts[i].Defeated && !last))
            return false;
    }
    const bool defeated = !s.Attempts.empty() && s.Attempts.back().Defeated;
    const bool climbed = s.Attempts.size() == static_cast<std::size_t>(TowerFloorCount);
    if (s.Active && !P409ActiveValid(*s.Active, s.Attempts.size() + 1))
        return false;
    switch (s.Participation)
    {
    case TowerParticipation::Entered:
    {
        // The clock stands still while the examination runs, somewhere inside the entry window, and time is called as soon
        // as the pending outside minutes exceed the late candidate's reduced allowance.
        if (defeated || climbed || now > at + P409LateEntryMinutes)
            return false;
        return QueryTowerSession(s, {}).Result.OutsideMinutes <= TowerWorldCapMinutes - (now - at);
    }
    case TowerParticipation::Completed:
        if (defeated || !climbed || s.Active)
            return false;
        break;
    case TowerParticipation::Ejected:
        if (!defeated || s.Active)
            return false;
        break;
    default: // Withdrawn
        if (defeated || climbed)
            return false;
        break;
    }
    const P402ExamFinish finished = P402ExamFinishSeam(s);
    // A late exit charged at most its allowance, so the clock is still at least the opening plus the derived charge.
    return finished.Ok() && finished.Score.OutsideMinutes >= 0 && finished.Score.OutsideMinutes <= TowerWorldCapMinutes &&
           (!current || now >= at + finished.Score.OutsideMinutes);
}
} // namespace PerfectWorld
