#include "Tower.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>

using namespace PerfectWorld;

namespace
{
int Assertions = 0;
void Expect(bool condition, const char* message)
{
    ++Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

TowerFloorAttempt Attempt(const TowerRouteDefinition& route, std::int32_t quality = 1000)
{
    return {route.Id, route.Method, quality, quality, 60, false};
}

std::vector<TowerFloorAttempt> FullRoute(std::int32_t quality = 1000)
{
    return {Attempt(*FindTowerRoute("tower.route.pace"), quality),
            Attempt(*FindTowerRoute("tower.route.stabilise"), quality),
            Attempt(*FindTowerRoute("tower.route.supply"), quality)};
}

const TowerOfferDefinition* Offer(std::string_view id)
{
    for (const auto& offer : TowerOffers())
        if (offer.Id == id)
            return &offer;
    return nullptr;
}

bool HasCredential(const TowerResult& result, AptitudePath discipline)
{
    return std::any_of(result.Credentials.begin(), result.Credentials.end(),
        [discipline](const TowerCredential& item) { return item.Discipline == discipline; });
}

bool Offered(const TowerResult& result, std::string_view id)
{
    return std::find(result.OfferIds.begin(), result.OfferIds.end(), id) != result.OfferIds.end();
}

void ExpectOffers(const TowerResult& result)
{
    Expect(result.Ok() && result.OfferIds.size() >= 3, "every valid result has at least three offers");
    std::set<std::string_view> unique;
    std::int64_t support = 0;
    for (const auto id : result.OfferIds)
    {
        const auto* offer = Offer(id);
        Expect(offer != nullptr, "returned offer has an authored definition");
        Expect(!offer->NextAction.empty(), "offer has an actionable next step");
        Expect(unique.insert(id).second, "offer is not duplicated");
        Expect(result.Decile <= offer->MaxDecile, "offer qualifies at the result's decile");
        if (offer->NeedsCredential)
            Expect(HasCredential(result, offer->Discipline), "credential-gated offer needs its credential in the portfolio");
        support = std::max(support, offer->SupportPaise);
    }
    Expect(result.SupportEnvelopePaise == support, "support is the best envelope, never summed");
}

void TestMatrix()
{
    Expect(TowerFloors().size() == 3 && TowerRoutes().size() == 9, "three floors each have three routes");
    std::set<std::string_view> ids;
    std::array<int, TowerFloorCount> perFloor{};
    std::array<bool, TowerMethodCount> methods{};
    std::array<bool, AptitudePathCount> covered{};
    for (const auto& floor : TowerFloors())
        Expect(!floor.Id.empty() && ids.insert(floor.Id).second, "floor IDs are stable and unique");
    for (const auto& route : TowerRoutes())
    {
        Expect(!route.Id.empty() && ids.insert(route.Id).second, "route IDs are stable and unique");
        Expect(FindTowerRoute(route.Id) == &route && !route.Task.empty(), "route has a lookup and a task");
        Expect(route.Floor >= 1 && route.Floor <= TowerFloorCount, "route references an existing floor");
        ++perFloor[route.Floor - 1];
        Expect(route.Method != TowerMethod::Combat, "every route has a non-combat completion method");
        methods[static_cast<std::size_t>(route.Method)] = true;
        methods[0] = methods[0] || route.AllowsCombat;
        bool any = false;
        for (std::size_t p = 0; p < AptitudePathCount; ++p)
        {
            const auto path = static_cast<AptitudePath>(p);
            if (!TowerRouteCovers(route, path))
                continue;
            any = covered[p] = true;
            auto receipt = FullRoute();
            receipt[route.Floor - 1] = Attempt(route);
            const auto result = ResolveTower(receipt);
            Expect(result.Ok() && result.Decile == 1, "each discipline's non-combat route can reach the top decile");
            const auto credential = std::find_if(result.Credentials.begin(), result.Credentials.end(),
                [path](const auto& item) { return item.Discipline == path; });
            Expect(credential != result.Credentials.end() && credential->ScoreMilli == 1000,
                   "covered discipline receives substantive evidence at the shared ceiling");
        }
        Expect(any, "every route evidences at least one registered discipline");
    }
    for (const auto count : perFloor)
        Expect(count == 3, "each future playable floor has exactly three routes");
    for (const auto present : methods)
        Expect(present, "matrix covers every documented floor method including combat");
    for (std::size_t p = 0; p < AptitudePathCount; ++p)
    {
        const auto path = static_cast<AptitudePath>(p);
        Expect(covered[p], "every existing path has non-combat coverage");
        auto parsed = AptitudePath::Hunter;
        Expect(TowerDisciplineFromId(AptitudePathId(path), parsed) == TowerError::None && parsed == path,
               "discipline IDs come from the existing registry");
    }
    // Every discipline has at least two routes, so no discipline hangs on one task; and each of the
    // five non-combat methods has a route on which a player never fights.
    for (std::size_t p = 0; p < AptitudePathCount; ++p)
    {
        int routes = 0;
        for (const auto& route : TowerRoutes())
            routes += TowerRouteCovers(route, static_cast<AptitudePath>(p)) ? 1 : 0;
        Expect(routes >= 2, "every discipline is evidenced by at least two non-combat-completable routes");
    }
    for (std::size_t m = 1; m < TowerMethodCount; ++m)
        Expect(methods[m], "every non-combat floor method has a route");
    // The six methods are the countdown's six preparation dimensions, in order and by name.
    constexpr std::array<std::string_view, TowerMethodCount> names{
        "combat", "analysis", "endurance", "construction", "negotiation", "logistics"};
    for (std::size_t m = 0; m < TowerMethodCount; ++m)
    {
        auto parsed = TowerMethod::Logistics;
        Expect(TowerMethodId(static_cast<TowerMethod>(m)) == names[m], "method ids match the countdown dimension names");
        Expect(TowerMethodFromId(names[m], parsed) && static_cast<std::size_t>(parsed) == m, "method ids round trip");
    }
    auto method = TowerMethod::Analysis;
    Expect(!TowerMethodFromId("magic", method) && !TowerMethodFromId("", method) && method == TowerMethod::Analysis,
           "unknown method id is refused without modifying output");
    Expect(TowerCredentials().size() == AptitudePathCount, "credential table covers the registry");
    std::set<AptitudePath> disciplines;
    for (const auto& credential : TowerCredentials())
    {
        Expect(ids.insert(credential.Id).second, "credential IDs are unique");
        Expect(disciplines.insert(credential.Discipline).second, "one credential definition per discipline");
    }
    for (const auto& offer : TowerOffers())
        Expect(ids.insert(offer.Id).second && !offer.NextAction.empty(), "offer IDs are unique and actionable");
}

void TestRoutesAndRanking()
{
    // All 27 complete non-combat route combinations have the same ceiling.
    for (const auto& a : TowerRoutes())
        for (const auto& b : TowerRoutes())
            for (const auto& c : TowerRoutes())
            {
                if (a.Floor != 1 || b.Floor != 2 || c.Floor != 3)
                    continue;
                const std::vector<TowerFloorAttempt> receipt{Attempt(a), Attempt(b), Attempt(c)};
                const auto result = ResolveTower(receipt);
                Expect(result.Ok() && result.NationalRank == 1 && result.Decile == 1,
                       "all non-combat combinations reach the top decile");
                Expect(result == ResolveTower(receipt), "identical inputs produce identical results twice");
                ExpectOffers(result);
            }
    auto combat = FullRoute();
    combat[0] = Attempt(*FindTowerRoute("tower.route.encounter"));
    const auto endurance = ResolveTower(combat);
    combat[0].Method = TowerMethod::Combat;
    Expect(ResolveTower(combat) == endurance, "combat and endurance earn identical rank and credentials");

    const auto empty = ResolveTower({});
    Expect(empty.Ok() && empty.NationalRank == 923 && empty.Decile == 10,
           "declining has a bottom-decile result");
    Expect(empty.Credentials.empty() && empty.OutsideMinutes == 0, "declining invents no evidence or time");
    ExpectOffers(empty);
    Expect(empty.ScorePoints == 0 && empty.NationalRank == 923 && empty.Withdrew && !empty.Completed && !empty.Ejected,
           "declining scores nothing and withdraws");
    auto early = FullRoute(0);
    early.resize(1);
    early[0].Defeated = true;
    const auto bottom = ResolveTower(early);
    Expect(bottom.Ejected && !bottom.Withdrew && bottom.Decile == 10 && bottom.ScorePoints == -TowerDefeatPenaltyPoints,
           "first-floor defeat reaches the bottom decile safely");
    Expect(bottom.NationalRank == TowerCohortCount && bottom.NationalRank > empty.NationalRank,
           "a first-floor defeat costs rank against not attempting");
    ExpectOffers(bottom);

    auto previousRank = TowerCohortCount;
    for (int quality = 0; quality <= 1000; ++quality)
    {
        const auto receipt = FullRoute(quality);
        const auto result = ResolveTower(receipt);
        Expect(result.Ok() && result.NationalRank <= previousRank, "improving performance never worsens rank");
        Expect(result == ResolveTower(receipt), "ranking and portfolios are deterministic across score range");
        Expect(result.Decile >= 1 && result.Decile <= 10, "decile stays in range");
        previousRank = result.NationalRank;
    }
    const auto tied = ResolveTower(FullRoute(0));
    Expect(tied.ScorePoints == 3000 && tied.NationalRank == 461 && tied.Decile == 5 && tied.Completed,
           "synthetic cohort counts equal-score peers ahead of the player");
    ExpectOffers(tied);
    // The top-decile boundary of the synthetic cohort, for every all-non-combat combination: a
    // uniform quality of 800 (score 5400) is rank 92; 780 (5340) is rank 101, just outside.
    for (const auto& a : TowerRoutes())
        for (const auto& b : TowerRoutes())
            for (const auto& c : TowerRoutes())
            {
                if (a.Floor != 1 || b.Floor != 2 || c.Floor != 3)
                    continue;
                const auto in = ResolveTower({Attempt(a, 800), Attempt(b, 800), Attempt(c, 800)});
                const auto out = ResolveTower({Attempt(a, 780), Attempt(b, 780), Attempt(c, 780)});
                Expect(in.NationalRank == 92 && in.Decile == 1 && out.NationalRank == 101 && out.Decile == 2,
                       "top-decile boundary is the same for every non-combat route combination");
            }
    // A partial climb is a withdrawal that keeps what it earned.
    for (std::size_t size = 1; size < 3; ++size)
    {
        auto partial = FullRoute(900);
        partial.resize(size);
        const auto result = ResolveTower(partial);
        Expect(result.Withdrew && !result.Completed && !result.Ejected && result.FloorsCleared == static_cast<int>(size),
               "a partial receipt is a withdrawal");
        Expect(result.ScorePoints == static_cast<int>(size) * 1900, "a withdrawal keeps its earned points");
        ExpectOffers(result);
    }

    // The bottom decile, found by sweeping every floor-one route at every quality in steps of 25, cleared
    // and defeated, always has at least three actionable offers, and they depend on neither a credential
    // nor on having tried.
    int bottomResults = 0;
    for (const auto& route : TowerRoutes())
        for (int quality = 0; quality <= 1000 && route.Floor == 1; quality += 25)
            for (const bool defeat : {false, true})
            {
                auto receipt = std::vector<TowerFloorAttempt>{Attempt(route, quality)};
                receipt[0].Defeated = defeat;
                const auto result = ResolveTower(receipt);
                Expect(result.Ok(), "one-floor receipt resolves");
                ExpectOffers(result);
                if (result.Decile != 10)
                    continue;
                ++bottomResults;
                int open = 0;
                for (const auto id : result.OfferIds)
                    open += Offer(id)->MaxDecile == 10 && !Offer(id)->NeedsCredential && !Offer(id)->NextAction.empty() ? 1 : 0;
                Expect(open >= 3, "bottom decile gets at least three actionable offers open to anyone");
                Expect(!Offered(result, "offer.tower.scholarship") && !Offered(result, "offer.tower.licence"),
                       "top-rank offers are not given to the bottom decile");
            }
    Expect(bottomResults > 10, "the sweep really exercised the bottom decile");
    Expect(Offered(empty, "offer.tower.apprenticeship") && Offered(empty, "offer.tower.retake") && empty.OfferIds.size() >= 3,
           "declining the examination still has offers, including trying again");
    Expect(empty.SupportEnvelopePaise == 0, "no rank, no allowance");
}

void TestDefeatAndPortfolio()
{
    for (std::size_t size = 1; size <= 3; ++size)
    {
        auto receipt = FullRoute();
        receipt.resize(size);
        auto prefix = receipt;
        prefix.pop_back();
        const auto withdrawn = ResolveTower(prefix);
        receipt.back().Defeated = true;
        const auto defeated = ResolveTower(receipt);
        Expect(defeated.Ok() && defeated.Ejected && defeated.FloorsCleared == static_cast<int>(size - 1),
               "defeat ejects without clearing the failed floor");
        Expect(defeated.Credentials == withdrawn.Credentials, "defeat preserves completed portfolio only");
        Expect(defeated.ScorePoints == withdrawn.ScorePoints - TowerDefeatPenaltyPoints, "defeat deducts rank points");
        Expect(defeated.NationalRank > withdrawn.NationalRank, "defeat actually worsens national rank");
        ExpectOffers(defeated);
    }
    auto receipt = FullRoute();
    receipt[0].DisciplineMilli = 501;
    receipt[0].CommonJudgmentMilli = 502;
    receipt[1].DisciplineMilli = receipt[1].CommonJudgmentMilli = 900;
    receipt[2].DisciplineMilli = receipt[2].CommonJudgmentMilli = 700;
    const auto result = ResolveTower(receipt);
    Expect(result.ScorePoints == 5101, "depth plus weighted quality uses integer floor arithmetic");
    std::set<AptitudePath> unique;
    for (const auto& item : result.Credentials)
    {
        Expect(unique.insert(item.Discipline).second, "portfolio keeps one best credential per discipline");
        Expect(FindTowerRoute(item.RouteId) != nullptr, "portfolio evidence names an authored route");
        if (item.Discipline == AptitudePath::Ecologist)
            Expect(item.ScoreMilli == 900 && item.RouteId == receipt[1].RouteId, "best evidence wins across floors");
    }
    // The hunter licence offer follows a Hunter credential, and the credential is reachable with no combat:
    // team transit (withdrawal, rescue, team communication) is a non-combat route that evidences Hunter.
    auto fighter = FullRoute();
    const auto pace = ResolveTower(fighter);
    fighter[0] = Attempt(*FindTowerRoute("tower.route.team_transit"));
    const auto transit = ResolveTower(fighter);
    Expect(!HasCredential(pace, AptitudePath::Hunter) && !Offered(pace, "offer.tower.licence"),
           "no hunter credential, no licence offer");
    Expect(HasCredential(transit, AptitudePath::Hunter) && Offered(transit, "offer.tower.licence") &&
           transit.Decile == 1 && transit.SupportEnvelopePaise == pace.SupportEnvelopePaise,
           "a hunter credential earned without combat opens the licence offer without a second envelope");
    ExpectOffers(pace);
    ExpectOffers(transit);

    const auto zero = ResolveTower(FullRoute(0));
    Expect(!zero.Credentials.empty() && zero.Credentials[0].ScoreMilli == 0, "zero-quality completion differs from unobserved");
}

void TestClock()
{
    const auto declaration = TowerMissionDeclaration();
    Expect(declaration.Mode == ClockMode::Mission && ValidateDeclaration(declaration) == TimeContractError::None,
           "tower declares the existing mission clock contract");
    TimeModeState state;
    Expect(EnterBaseMode(state, ClockMode::Mission) && !WorldClockRuns(state), "mission pauses the outside world");
    Expect(AdvanceRealTime(state, 60'000) == 0, "real-time waiting in the mission charges no outside minutes");
    for (const auto minutes : {0LL, 1LL, 4LL, 719LL, 720LL, 721LL, 4000LL, MaxSingleAdvanceMinutes})
    {
        auto receipt = FullRoute();
        receipt.resize(1);
        receipt[0].MissionMinutes = minutes;
        const auto result = ResolveTower(receipt);
        const auto time = ResolveInteraction(declaration, {{TimeEventKind::Progress, minutes}, {TimeEventKind::Finish, 0}});
        Expect(result.Ok() && time.Ok() && result.OutsideMinutes == time.ChargedMinutes,
               "tower delegates outside advance to TimeContract at every boundary");
        Expect(result.OutsideMinutes <= TowerWorldCapMinutes && result.OutsideMinutes <= minutes,
               "mission world advance obeys rate and cap");
        receipt[0].Defeated = true;
        Expect(ResolveTower(receipt).OutsideMinutes == result.OutsideMinutes, "defeat still charges mission time at exit");
    }
    auto longRun = FullRoute();
    for (auto& item : longRun)
        item.MissionMinutes = 1000;
    Expect(ResolveTower(longRun).OutsideMinutes == TowerWorldCapMinutes, "cap applies once to the whole exam");
    const auto running = ResolveInteraction(declaration, {{TimeEventKind::Progress, 4000}});
    Expect(running.Ok() && !running.Resolved() && running.ChargedMinutes == 0, "outside time is charged only at mission exit");
}

void TestRefusals()
{
    auto discipline = AptitudePath::Engineer;
    Expect(TowerDisciplineFromId("path.no_such", discipline) == TowerError::UnknownDiscipline &&
           discipline == AptitudePath::Engineer, "unknown discipline ID is refused without modifying output");
    Expect(FindTowerRoute("tower.route.no_such") == nullptr, "unknown route lookup fails");
    Expect(!TowerRouteCovers(TowerRoutes()[0], static_cast<AptitudePath>(255)), "invalid discipline enum cannot shift the mask");
    const auto refuse = [](const auto& receipt, TowerError error)
    {
        const auto result = ResolveTower(receipt);
        TowerResult expected;
        expected.Error = error;
        Expect(result == expected && !TowerErrorCode(error).empty(), "refusal is atomic and has a stable error code");
    };
    auto receipt = FullRoute();
    receipt[1].RouteId = "tower.route.no_such";
    refuse(receipt, TowerError::UnknownRoute);
    receipt = FullRoute();
    receipt[1].Method = static_cast<TowerMethod>(255);
    refuse(receipt, TowerError::UnknownMethod);
    receipt[1].Method = TowerMethod::Combat;
    refuse(receipt, TowerError::MethodNotAllowed);
    for (const auto score : {-1, 1001, std::numeric_limits<int>::max()})
    {
        receipt = FullRoute();
        receipt[1].DisciplineMilli = score;
        refuse(receipt, TowerError::InvalidScore);
        receipt = FullRoute();
        receipt[1].CommonJudgmentMilli = score;
        refuse(receipt, TowerError::InvalidScore);
    }
    receipt = FullRoute();
    receipt[0].MissionMinutes = -1;
    refuse(receipt, TowerError::InvalidMinutes);
    receipt[0].MissionMinutes = std::numeric_limits<std::int64_t>::max();
    refuse(receipt, TowerError::InvalidMinutes);
    receipt[0].MissionMinutes = MaxSingleAdvanceMinutes;
    receipt[1].MissionMinutes = 1;
    refuse(receipt, TowerError::InvalidMinutes);
    receipt = FullRoute();
    std::swap(receipt[0], receipt[1]);
    refuse(receipt, TowerError::FloorOutOfOrder);
    receipt = FullRoute();
    receipt[1] = receipt[0];
    refuse(receipt, TowerError::FloorOutOfOrder);
    receipt = FullRoute();
    receipt.push_back(receipt.back());
    refuse(receipt, TowerError::FloorOutOfOrder);
    receipt = FullRoute();
    receipt[0].Defeated = true;
    refuse(receipt, TowerError::AfterDefeat);
    Expect(TowerErrorCode(TowerError::None).empty(), "success has no refusal code");
}
} // namespace

int main()
{
    TestMatrix();
    TestRoutesAndRanking();
    TestDefeatAndPortfolio();
    TestClock();
    TestRefusals();
    std::cout << "PerfectWorld tower: " << Assertions << " checks passed\n";
    return 0;
}
