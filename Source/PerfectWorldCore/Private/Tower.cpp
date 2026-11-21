#include "Tower.h"

#include <algorithm>
#include <array>

namespace PerfectWorld
{
namespace
{
using P = AptitudePath;
using M = TowerMethod;
constexpr std::uint32_t Bit(P path) { return 1u << static_cast<unsigned>(path); }
static_assert(AptitudePathCount <= 32);
static_assert(TowerDisciplineSharePercent >= 0 && TowerDisciplineSharePercent <= 100);

// ROADMAP M4-10..12 floor pairings (floor 1 combat and endurance, 2 analysis and construction,
// 3 negotiation and logistics); route tasks are provisional runner contracts. Every route's own
// method is non-combat, so a player who never fights can complete all nine; the one combat
// alternative (the encounter route) earns the identical ceiling. Hunter evidence is reachable
// without combat through team_transit (PATHS_AND_PROGRESSION: withdrawal, rescue, team
// communication are the trainee hunter's first tests).
constexpr std::array<TowerFloorDefinition, TowerFloorCount> Floors{{
    {"tower.floor.endurance", 1}, {"tower.floor.system", 2}, {"tower.floor.alliance", 3}
}};
constexpr std::array<TowerRouteDefinition, 9> Routes{{
    {"tower.route.encounter", 1, M::Endurance, true, Bit(P::Hunter) | Bit(P::Athlete),
     "Read tells and reach shelter by evasion or engagement"},
    {"tower.route.pace", 1, M::Endurance, false, Bit(P::Athlete) | Bit(P::Ecologist),
     "Pace a sustained traverse while monitoring degradation"},
    {"tower.route.team_transit", 1, M::Endurance, false, Bit(P::Hunter) | Bit(P::Clinician) | Bit(P::Logistician),
     "Move a team to shelter with rest and care stops"},
    {"tower.route.changed_rule", 2, M::Analysis, false, Bit(P::Scholar) | Bit(P::Clinician),
     "Deduce the changed rule from observations and test a prediction"},
    {"tower.route.stabilise", 2, M::Construction, false, Bit(P::Engineer) | Bit(P::Ecologist),
     "Diagnose and repair a failing system without exhausting its reserve"},
    {"tower.route.design", 2, M::Construction, false, Bit(P::Artist) | Bit(P::Engineer) | Bit(P::Scholar),
     "Build and test a new arrangement under the changed rule"},
    {"tower.route.alliance", 3, M::Negotiation, false, Bit(P::PublicLeader) | Bit(P::Enterprise) | Bit(P::Artist),
     "Represent competing needs and secure a workable temporary alliance"},
    {"tower.route.supply", 3, M::Logistics, false, Bit(P::Logistician) | Bit(P::Enterprise) | Bit(P::Ecologist),
     "Route scarce supply and recover after a disrupted delivery"},
    {"tower.route.mediation", 3, M::Negotiation, false, Bit(P::PublicLeader) | Bit(P::Clinician),
     "Resolve a team conflict with consent and clear responsibilities"}
}};
constexpr std::array<TowerCredentialDefinition, AptitudePathCount> Credentials{{
    {"credential.tower.hunter", P::Hunter}, {"credential.tower.scholar", P::Scholar},
    {"credential.tower.engineer", P::Engineer}, {"credential.tower.enterprise", P::Enterprise},
    {"credential.tower.athlete", P::Athlete}, {"credential.tower.clinician", P::Clinician},
    {"credential.tower.public_leader", P::PublicLeader}, {"credential.tower.logistician", P::Logistician},
    {"credential.tower.artist", P::Artist}, {"credential.tower.ecologist", P::Ecologist}
}};
// Offers are qualification next steps, not automatic admission, debt or a licence.
// The first four are open at every result, including declining: the examination cannot be failed
// out of existence (AWAKENING_AND_EXAM "Failure"). The retake is annual up to age twenty.
// TODO M4-13/M5: authored institutions, terms, first weeks and eligibility rules (and the age bound).
constexpr std::array<TowerOfferDefinition, 10> Offers{{
    {"offer.tower.apprenticeship", "Request a supervised workshop apprenticeship assessment", 10, 0},
    {"offer.tower.study_bridge", "Book a foundation study adviser appointment", 10, 0},
    {"offer.tower.community_practice", "Apply for an introductory community logistics placement", 10, 0},
    {"offer.tower.retake", "Register to sit the examination again next year", 10, 0},
    {"offer.tower.equipment", "Submit a practice-equipment allowance application", 5, 500'000},
    {"offer.tower.licence", "Request a supervised hunter licence eligibility assessment", 5, 0, true, P::Hunter},
    {"offer.tower.patronage", "Submit a work portfolio for sponsorship or patronage review", 3, 0},
    {"offer.tower.public_portfolio", "Choose whether to publish a credential portfolio for public attention", 3, 0},
    {"offer.tower.scholarship", "Request a scholarship and university placement interview", 1, 2'000'000},
    {"offer.tower.enterprise", "Book a starting-capital proposal assessment", 1, 2'000'000}
}};
constexpr std::array<std::string_view, TowerMethodCount> MethodIds{
    "combat", "analysis", "endurance", "construction", "negotiation", "logistics"
};

bool HasCredential(const TowerResult& result, AptitudePath discipline)
{
    return std::any_of(result.Credentials.begin(), result.Credentials.end(),
        [discipline](const TowerCredential& credential) { return credential.Discipline == discipline; });
}

TowerResult Refused(TowerError error)
{
    TowerResult result;
    result.Error = error;
    return result;
}

/** Synthetic M4-04 fixture, NOT real exam data or the global rankings model:
 * 999 peers with scores TowerCohortMinPoints + floor(i * span / 1000), i=0..998, where the span
 * runs from a defeated first floor (-500) to a perfect three-floor climb (6000). Equal scores
 * place ahead of the player (conservative competition rank). No seed, no randomness, no identity
 * bias: the same score is always the same rank. Source: this uniform authored distribution.
 * TODO M6: calibrated national cohort distribution with published provenance.
 */
std::int32_t NationalRank(std::int32_t score)
{
    constexpr std::int32_t ceiling = TowerFloorCount * (TowerDepthPoints + TowerScoreMaxMilli);
    constexpr std::int32_t span = ceiling - TowerCohortMinPoints;
    std::int32_t rank = 1;
    for (std::int32_t i = 0; i < TowerCohortCount - 1; ++i)
        if (TowerCohortMinPoints + i * span / TowerCohortCount >= score)
            ++rank;
    return rank;
}
} // namespace

std::span<const TowerFloorDefinition> TowerFloors() noexcept { return Floors; }
std::span<const TowerRouteDefinition> TowerRoutes() noexcept { return Routes; }
std::span<const TowerCredentialDefinition> TowerCredentials() noexcept { return Credentials; }
std::span<const TowerOfferDefinition> TowerOffers() noexcept { return Offers; }

const TowerRouteDefinition* FindTowerRoute(std::string_view id) noexcept
{
    const auto found = std::find_if(Routes.begin(), Routes.end(), [id](const auto& route) { return route.Id == id; });
    return found == Routes.end() ? nullptr : &*found;
}

bool TowerRouteCovers(const TowerRouteDefinition& route, AptitudePath discipline) noexcept
{
    return static_cast<std::size_t>(discipline) < AptitudePathCount && (route.DisciplineMask & Bit(discipline)) != 0;
}

std::string_view TowerMethodId(TowerMethod method) noexcept
{
    const auto index = static_cast<std::size_t>(method);
    return index < MethodIds.size() ? MethodIds[index] : std::string_view{};
}

bool TowerMethodFromId(std::string_view id, TowerMethod& out) noexcept
{
    for (std::size_t i = 0; i < MethodIds.size(); ++i)
        if (MethodIds[i] == id)
        {
            out = static_cast<TowerMethod>(i);
            return true;
        }
    return false;
}

TowerError TowerDisciplineFromId(std::string_view id, AptitudePath& out) noexcept
{
    for (std::size_t p = 0; p < AptitudePathCount; ++p)
    {
        const auto path = static_cast<P>(p);
        if (AptitudePathId(path) == id)
        {
            out = path;
            return TowerError::None;
        }
    }
    return TowerError::UnknownDiscipline;
}

InteractionDeclaration TowerMissionDeclaration()
{
    InteractionDeclaration declaration;
    declaration.Id = "tower.exam";
    declaration.Mode = ClockMode::Mission;
    declaration.CostMinutes = TowerWorldCapMinutes;
    declaration.Interrupt = InterruptPolicy::Immediate;
    declaration.Resolve = ResolutionBoundary::OnComplete;
    declaration.Mission = {0, TowerWorldRatePermille, TowerWorldCapMinutes};
    return declaration;
}

std::string_view TowerErrorCode(TowerError error) noexcept
{
    switch (error)
    {
    case TowerError::None: return {};
    case TowerError::UnknownRoute: return "tower.unknown_route";
    case TowerError::UnknownDiscipline: return "tower.unknown_discipline";
    case TowerError::UnknownMethod: return "tower.unknown_method";
    case TowerError::MethodNotAllowed: return "tower.method_not_allowed";
    case TowerError::InvalidScore: return "tower.invalid_score";
    case TowerError::InvalidMinutes: return "tower.invalid_minutes";
    case TowerError::FloorOutOfOrder: return "tower.floor_out_of_order";
    case TowerError::AfterDefeat: return "tower.after_defeat";
    case TowerError::InvalidClock: return "tower.invalid_clock";
    }
    return "tower.unknown_error";
}

TowerResult ResolveTower(const std::vector<TowerFloorAttempt>& attempts)
{
    TowerResult result;
    std::array<bool, AptitudePathCount> observed{};
    std::array<std::int32_t, AptitudePathCount> scores{};
    std::array<std::string_view, AptitudePathCount> sources{};
    std::int32_t floorNumber = 1;
    for (const auto& attempt : attempts)
    {
        if (result.Ejected)
            return Refused(TowerError::AfterDefeat);
        const auto* route = FindTowerRoute(attempt.RouteId);
        if (!route)
            return Refused(TowerError::UnknownRoute);
        if (route->Floor != floorNumber++)
            return Refused(TowerError::FloorOutOfOrder);
        if (TowerMethodId(attempt.Method).empty())
            return Refused(TowerError::UnknownMethod);
        if (attempt.Method != route->Method && !(route->AllowsCombat && attempt.Method == M::Combat))
            return Refused(TowerError::MethodNotAllowed);
        if (attempt.DisciplineMilli < 0 || attempt.DisciplineMilli > TowerScoreMaxMilli ||
            attempt.CommonJudgmentMilli < 0 || attempt.CommonJudgmentMilli > TowerScoreMaxMilli)
            return Refused(TowerError::InvalidScore);
        if (attempt.MissionMinutes < 0 || attempt.MissionMinutes > MaxSingleAdvanceMinutes - result.MissionMinutes)
            return Refused(TowerError::InvalidMinutes);
        result.MissionMinutes += attempt.MissionMinutes;
        if (attempt.Defeated)
        {
            result.Ejected = true;
            result.ScorePoints -= TowerDefeatPenaltyPoints;
            continue;
        }
        ++result.FloorsCleared;
        const auto quality = (TowerDisciplineSharePercent * attempt.DisciplineMilli +
            (100 - TowerDisciplineSharePercent) * attempt.CommonJudgmentMilli) / 100;
        result.ScorePoints += TowerDepthPoints + quality;
        for (std::size_t p = 0; p < AptitudePathCount; ++p)
        {
            if (!TowerRouteCovers(*route, static_cast<P>(p)))
                continue;
            if (!observed[p] || quality > scores[p])
            {
                observed[p] = true;
                scores[p] = quality;
                sources[p] = route->Id;
            }
        }
    }
    // TimeContract owns the formula and exit boundary: cap once for the entire run.
    const auto time = ResolveInteraction(TowerMissionDeclaration(),
        {{TimeEventKind::Progress, result.MissionMinutes}, {TimeEventKind::Finish, 0}});
    if (!time.Ok() || !time.Resolved())
        return Refused(TowerError::InvalidClock);
    result.OutsideMinutes = time.ChargedMinutes;
    for (std::size_t p = 0; p < AptitudePathCount; ++p)
        if (observed[p])
            result.Credentials.push_back({Credentials[p].Id, Credentials[p].Discipline, scores[p], sources[p]});
    result.Completed = result.FloorsCleared == TowerFloorCount;
    result.Withdrew = !result.Ejected && !result.Completed;
    result.NationalRank = NationalRank(result.ScorePoints);
    result.Decile = (result.NationalRank * 10 + TowerCohortCount - 1) / TowerCohortCount;
    for (const auto& offer : Offers)
    {
        if (result.Decile > offer.MaxDecile || (offer.NeedsCredential && !HasCredential(result, offer.Discipline)))
            continue;
        result.OfferIds.push_back(offer.Id);
        result.SupportEnvelopePaise = std::max(result.SupportEnvelopePaise, offer.SupportPaise);
    }
    return result;
}
} // namespace PerfectWorld
