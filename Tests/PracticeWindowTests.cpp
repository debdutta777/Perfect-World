// ROADMAP M2-04: practice limits (rolling 24-hour window, per-skill budget, 3-in-7 consolidation, forecast).
#include "PracticeWindow.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>

using namespace PerfectWorld;

namespace
{
int Checks = 0;

void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

// A small deterministic generator for the tests (xorshift64*).
struct Rng
{
    std::uint64_t S;
    std::uint64_t Next()
    {
        S ^= S >> 12;
        S ^= S << 25;
        S ^= S >> 27;
        return (S * 2685821657736338717ULL) >> 16;
    }
    std::int64_t Pick(std::int64_t low, std::int64_t high)
    {
        return low + static_cast<std::int64_t>(Next() % static_cast<std::uint64_t>(high - low + 1));
    }
};

const char* const Endurance = "family.endurance";
const char* const Written = "family.written_study";

// Instant 0 is 06:00 on the campaign's first day, so `day` days later at 06:00 plus `minute` (under 480, to stay in the day).
GameInstant At(std::int64_t day, std::int64_t minute = 0)
{
    return day * MinutesPerDay + minute;
}

PracticeSession Session(const char* family, GameInstant at, std::vector<SkillCredit> base)
{
    return PracticeSession{family, std::move(base), at};
}

PracticeSession One(const char* family, GameInstant at, std::int32_t milli = 1000, const char* skill = "skill.fitness")
{
    return Session(family, at, {{skill, milli}});
}

std::int32_t Granted(const PracticeForecast& forecast, std::size_t index = 0)
{
    return forecast.Credit.size() > index ? forecast.Credit[index].MilliXp : -1;
}

void TestFourthSessionTeachesNothing()
{
    PracticeState state;
    const PracticeForecast first = RecordPractice(state, One(Endurance, At(0)));
    Check(first.Ok() && first.Tier == PracticeTier::Full && Granted(first) == 1000, "the first session is full");
    Check(first.Label == PracticeLabel::UsefulPractice && PracticeLabelText(first.Label) == "Useful practice", "full reads Useful practice");
    const PracticeForecast second = RecordPractice(state, One(Endurance, At(0, 60)));
    Check(second.Tier == PracticeTier::Half && Granted(second) == 500, "the second is half");
    Check(second.Label == PracticeLabel::Consolidation && PracticeLabelText(second.Label) == "Consolidation", "half reads Consolidation");
    const PracticeForecast third = RecordPractice(state, One(Endurance, At(0, 120)));
    Check(third.Tier == PracticeTier::Tenth && Granted(third) == 100, "the third is a tenth");
    Check(third.Label == PracticeLabel::LittleNewLearning, "a tenth reads Little new learning");
    const PracticeForecast fourth = RecordPractice(state, One(Endurance, At(0, 180)));
    Check(fourth.Ok() && fourth.Tier == PracticeTier::None && Granted(fourth) == 0, "the fourth teaches nothing");
    Check(fourth.Label == PracticeLabel::LittleNewLearning && PracticeLabelText(fourth.Label) == "Little new learning", "none reads Little new learning");
    for (int i = 0; i < 20; ++i)
        Check(Granted(RecordPractice(state, One(Endurance, At(0, 200 + i)))) == 0, "every further session in the window teaches nothing");
    Check(PracticeSessionsInWindow(state, Endurance, At(0, 300)) == 3, "only the three most recent sessions are kept, which is exact");
    Check(PracticeTierPermille(PracticeTier::None) == 0 && PracticeTierName(PracticeTier::Half) == "half", "tier helpers");
    Check(IsPracticeStateValid(state), "the state stays valid");

    // A different family is its own window.
    Check(RecordPractice(state, One(Written, At(0, 220), 1000, "skill.academics")).Tier == PracticeTier::Full, "another family starts full");
}

void TestWindowRolls()
{
    PracticeState state;
    (void)RecordPractice(state, One(Endurance, At(0)));
    Check(PracticeSessionsInWindow(state, Endurance, At(0) + 1439) == 1, "a session still counts 1439 minutes later");
    Check(ForecastPractice(state, One(Endurance, At(0) + 1439)).Tier == PracticeTier::Half, "at 23h59 the window still holds it");
    Check(PracticeSessionsInWindow(state, Endurance, At(0) + 1440) == 0, "it expires exactly 24 hours later");
    Check(ForecastPractice(state, One(Endurance, At(0) + 1440)).Tier == PracticeTier::Full, "at exactly 24h the session is full again");
    const PracticeForecast later = ForecastPractice(state, One(Endurance, At(0) + 1441));
    Check(later.Tier == PracticeTier::Full && Granted(later) == 1000, "a session 24h+1min after the first is full again");

    // The window slides with the sessions inside it: it is not a calendar day.
    PracticeState slide;
    (void)RecordPractice(slide, One(Endurance, At(0)));
    (void)RecordPractice(slide, One(Endurance, At(0, 60)));
    (void)RecordPractice(slide, One(Endurance, At(0, 120)));
    Check(ForecastPractice(slide, One(Endurance, At(0) + 1439)).Tier == PracticeTier::None, "three in the window: none");
    Check(ForecastPractice(slide, One(Endurance, At(0) + 1441)).Tier == PracticeTier::Tenth, "the first expired: a tenth");
    Check(ForecastPractice(slide, One(Endurance, At(0) + 1500)).Tier == PracticeTier::Half, "two expired: half");
    Check(ForecastPractice(slide, One(Endurance, At(0) + 1560)).Tier == PracticeTier::Full, "all expired: full");

    // A locked window is not unlocked sooner by practising in it: the zero-credit session counts.
    PracticeState locked = slide;
    (void)RecordPractice(locked, One(Endurance, At(0, 300)));
    Check(ForecastPractice(locked, One(Endurance, At(0) + 1560)).Tier == PracticeTier::Half, "the maintenance session still occupies the window");
}

void TestRouteVariantsShareTheLimit()
{
    PracticeState state;
    // Three routes of one endurance family teach different skills; they share one window.
    const PracticeForecast a = RecordPractice(state, Session(Endurance, At(0), {{"skill.fitness", 1500}}));
    const PracticeForecast b = RecordPractice(state, Session(Endurance, At(0, 30), {{"skill.fitness", 1500}, {"skill.willpower", 300}}));
    const PracticeForecast c = RecordPractice(state, Session(Endurance, At(0, 60), {{"skill.fitness", 1500}}));
    const PracticeForecast d = RecordPractice(state, Session(Endurance, At(0, 90), {{"skill.fitness", 1500}}));
    Check(a.Tier == PracticeTier::Full && Granted(a) == 1500, "route A full");
    Check(b.Tier == PracticeTier::Half && Granted(b, 0) == 750 && Granted(b, 1) == 150, "route B is half for every skill");
    Check(c.Tier == PracticeTier::Tenth && Granted(c) == 150, "route C is a tenth");
    Check(d.Tier == PracticeTier::None && Granted(d) == 0, "route D teaches nothing: changing routes does not reset the window");
    // Separate families do not share it.
    Check(RecordPractice(state, One(Written, At(0, 100), 1500, "skill.academics")).Tier == PracticeTier::Full, "a different family is independent");
}

void TestSkillBudget()
{
    PracticeState state;
    // Four families cycle one skill; the per-skill budget (8000) still caps the day.
    Check(Granted(RecordPractice(state, One("family.a", At(0), 3000, "skill.academics"))) == 3000, "3000 of 8000");
    Check(Granted(RecordPractice(state, One("family.b", At(0, 10), 3000, "skill.academics"))) == 3000, "6000 of 8000");
    const PracticeForecast capped = RecordPractice(state, One("family.c", At(0, 20), 3000, "skill.academics"));
    Check(Granted(capped) == 2000 && capped.BudgetLimited, "the third family is cut to what remains");
    Check(PracticeBudgetRemaining(state, "skill.academics", At(0, 30)) == 0, "the budget is spent");
    const PracticeForecast none = RecordPractice(state, One("family.d", At(0, 30), 3000, "skill.academics"));
    Check(Granted(none) == 0 && none.BudgetLimited && none.Tier == PracticeTier::Full, "nothing is left, though family.d is a fresh window");
    Check(none.Label == PracticeLabel::LittleNewLearning, "a spent budget reads Little new learning");
    // Another skill is not touched, and a session mixing the two is capped per skill.
    const PracticeForecast mixed = RecordPractice(state, Session("family.e", At(0, 40), {{"skill.academics", 1000}, {"skill.fitness", 1000}}));
    Check(Granted(mixed, 0) == 0 && Granted(mixed, 1) == 1000, "the budget is per skill");
    // The next game day starts fresh (day starts at midnight: 06:00 epoch plus 18 hours).
    const GameInstant midnight = 18 * MinutesPerHour;
    Check(PracticeBudgetRemaining(state, "skill.academics", midnight - 1) == 0, "still spent a minute before midnight");
    Check(PracticeBudgetRemaining(state, "skill.academics", midnight) == PracticeSkillDailyBudgetMilli, "restored at midnight");
    Check(Granted(RecordPractice(state, One("family.a", At(1, 400), 3000, "skill.academics"))) == 3000, "the next day pays again");
    Check(PracticeBudgetRemaining(state, "skill.unseen", At(1)) == PracticeSkillDailyBudgetMilli, "an unseen skill has the whole budget");
}

void TestConsolidation()
{
    // A missed day does not reset it: Monday (0), Tuesday missed, Wednesday (2), Thursday (3).
    PracticeState state;
    PracticeSession s = One(Endurance, At(0));
    Check(RecordPractice(state, s).Consolidates.empty(), "one useful day: not consolidated");
    Check(RecordPractice(state, One(Endurance, At(2))).Consolidates.empty(), "two useful days: not consolidated");
    Check(!PracticeIsConsolidated(state, "skill.fitness", At(2)), "not consolidated yet");
    const PracticeForecast would = ForecastPractice(state, One(Endurance, At(3)));
    Check(would.Consolidates.size() == 1 && would.Consolidates[0] == "skill.fitness", "the forecast says the third useful day consolidates");
    const PracticeForecast third = RecordPractice(state, One(Endurance, At(3)));
    Check(third == would && third.Consolidates.size() == 1, "the third useful day in the week consolidates, with no streak needed");
    Check(PracticeIsConsolidated(state, "skill.fitness", At(3)), "consolidated");
    Check(PracticeIsConsolidated(state, "skill.fitness", At(6)), "the missed Tuesday and further missed days do not undo it");
    Check(RecordPractice(state, One(Endurance, At(4))).Consolidates.empty(), "already consolidated: not reported again");
    // It ages out of the seven-day span only by time, never by a miss: days 0,2,3,4 -> at day 8 the days 2,3,4 remain.
    Check(PracticeIsConsolidated(state, "skill.fitness", At(8)), "three days still in the span");
    Check(!PracticeIsConsolidated(state, "skill.fitness", At(11)), "after the span the status ages out");
    Check(RecordPractice(state, One(Endurance, At(20))).Consolidates.empty(), "a gap needs three new days");
    Check(RecordPractice(state, One(Endurance, At(21))).Consolidates.empty(), "two new days");
    Check(RecordPractice(state, One(Endurance, At(24))).Consolidates.size() == 1, "three new days reconsolidate after a long gap");

    // The span is seven calendar days inclusive.
    PracticeState edge;
    (void)RecordPractice(edge, One(Endurance, At(0)));
    (void)RecordPractice(edge, One(Endurance, At(2)));
    Check(RecordPractice(edge, One(Endurance, At(6))).Consolidates.size() == 1, "days 0, 2 and 6 fall in seven days");
    PracticeState wide;
    (void)RecordPractice(wide, One(Endurance, At(0)));
    (void)RecordPractice(wide, One(Endurance, At(2)));
    Check(RecordPractice(wide, One(Endurance, At(7))).Consolidates.empty(), "days 0, 2 and 7 do not");

    // Days count once however many sessions they hold, and a tenth or a locked session is not useful.
    PracticeState same;
    for (int i = 0; i < 2; ++i)
        (void)RecordPractice(same, One(Endurance, At(0, 10 * i)));
    (void)RecordPractice(same, One(Endurance, At(0, 40)));
    (void)RecordPractice(same, One(Endurance, At(0, 50)));
    Check(!PracticeIsConsolidated(same, "skill.fitness", At(0, 60)), "four sessions on one day are one day");
    PracticeState tenth;
    (void)RecordPractice(tenth, One(Endurance, At(0)));
    (void)RecordPractice(tenth, One(Endurance, At(0, 10)));
    (void)RecordPractice(tenth, One(Endurance, At(0, 20))); // a tenth on day 0
    Check(RecordPractice(tenth, One(Endurance, At(1, 0))).Tier == PracticeTier::Tenth, "(the window still holds two sessions)");
    (void)RecordPractice(tenth, One(Endurance, At(2, 0)));
    Check(!PracticeIsConsolidated(tenth, "skill.fitness", At(2)), "days 0 and 2 are two useful days; the tenth on day 1 does not make a third");
}

// An independent oracle: it keeps every session ever and recomputes from scratch.
struct Oracle
{
    std::vector<std::pair<std::string, GameInstant>> Sessions;
    std::map<std::pair<std::string, std::int64_t>, std::int64_t> Spent;
    std::map<std::string, std::set<std::int64_t>> Useful;

    static bool Consolidated(const std::set<std::int64_t>& days, std::int64_t day)
    {
        int n = 0;
        for (const std::int64_t d : days)
            n += (d <= day && d > day - 7) ? 1 : 0;
        return n >= 3;
    }

    PracticeForecast Step(const PracticeSession& session)
    {
        const std::int64_t day = GameClock::DayNumber(session.At);
        int inWindow = 0;
        for (const auto& [family, at] : Sessions)
            inWindow += (family == session.FamilyId && session.At - at < 1440) ? 1 : 0;
        const int permille = inWindow == 0 ? 1000 : inWindow == 1 ? 500 : inWindow == 2 ? 100 : 0;
        PracticeForecast out;
        out.Tier = inWindow == 0 ? PracticeTier::Full : inWindow == 1 ? PracticeTier::Half : inWindow == 2 ? PracticeTier::Tenth : PracticeTier::None;
        bool any = false;
        for (const SkillCredit& base : session.Base)
        {
            const std::int64_t byTier = static_cast<std::int64_t>(base.MilliXp) * permille / 1000;
            std::int64_t& spent = Spent[{base.SkillId, day}];
            const std::int64_t credit = std::min<std::int64_t>(byTier, PracticeSkillDailyBudgetMilli - spent);
            spent += credit;
            out.Credit.push_back({base.SkillId, static_cast<std::int32_t>(credit)});
            out.BudgetLimited = out.BudgetLimited || credit < byTier;
            any = any || credit > 0;
            if (inWindow <= 1 && credit > 0)
            {
                std::set<std::int64_t>& days = Useful[base.SkillId];
                const bool before = Consolidated(days, day);
                days.insert(day);
                if (!before && Consolidated(days, day))
                    out.Consolidates.push_back(base.SkillId);
            }
        }
        out.Label = !any || inWindow >= 2 ? PracticeLabel::LittleNewLearning
                    : inWindow == 0       ? PracticeLabel::UsefulPractice
                                          : PracticeLabel::Consolidation;
        Sessions.emplace_back(session.FamilyId, session.At);
        return out;
    }
};

PracticeSession RandomSession(Rng& rng, GameInstant& now)
{
    static const char* const families[] = {"family.a", "family.b", "family.c"};
    static const char* const skills[] = {"skill.a", "skill.b", "skill.c", "skill.d"};
    now += rng.Pick(0, 5) == 0 ? rng.Pick(0, 3 * MinutesPerDay) : rng.Pick(0, 400);
    PracticeSession session;
    session.FamilyId = families[rng.Pick(0, 2)];
    session.At = now;
    const std::int64_t count = rng.Pick(1, 3);
    std::set<std::string> used;
    for (std::int64_t i = 0; i < count; ++i)
    {
        const std::string skill = skills[rng.Pick(0, 3)];
        if (used.insert(skill).second)
            session.Base.push_back({skill, static_cast<std::int32_t>(rng.Pick(1, 3000))});
    }
    return session;
}

void TestRandomSequences()
{
    for (std::uint64_t seed = 1; seed <= 10000; ++seed)
    {
        Rng rng{seed * 0x9E3779B97F4A7C15ULL | 1};
        PracticeState state;
        PracticeState reloaded;
        Oracle oracle;
        bool split = false;
        const std::int64_t length = rng.Pick(4, 16);
        const std::int64_t splitAt = rng.Pick(1, length - 1);
        GameInstant now = rng.Pick(0, 3 * MinutesPerDay);
        for (std::int64_t i = 0; i < length; ++i)
        {
            const PracticeSession session = RandomSession(rng, now);
            const PracticeState untouched = state;
            const PracticeForecast forecast = ForecastPractice(state, session);
            Check(state == untouched, "a forecast changes nothing");
            const PracticeForecast actual = RecordPractice(state, session);
            Check(forecast.Ok() && actual == forecast, "the forecast equals what is recorded");
            Check(actual == oracle.Step(session), "the recorded credit matches the independent oracle");
            Check(IsPracticeStateValid(state), "the state stays valid");
            if (i + 1 == splitAt)
            {
                const PracticeParse parsed = DeserialisePractice(SerialisePractice(state));
                Check(parsed.Ok() && parsed.State == state, "a mid-sequence save loads back equal");
                reloaded = parsed.State;
                split = true;
            }
            else if (split)
            {
                const PracticeForecast again = RecordPractice(reloaded, session);
                Check(again == actual && reloaded == state, "the reloaded run gives identical results");
            }
        }
    }
}

void TestRefusals()
{
    PracticeState state;
    (void)RecordPractice(state, One(Endurance, At(1)));
    const PracticeState before = state;
    auto refused = [&](PracticeSession s, PracticeError expected, const char* what)
    {
        const PracticeForecast f = RecordPractice(state, s);
        Check(!f.Ok() && f.Error == expected && f.Credit.empty() && state == before, what);
        Check(ForecastPractice(state, s) == f, "a forecast refuses the same way");
    };
    refused(One(Endurance, At(1) - 1), PracticeError::InvalidTime, "time never runs backwards");
    refused(One(Written, At(0), 1000, "skill.academics"), PracticeError::InvalidTime, "nor for another family");
    refused(One("", At(2)), PracticeError::InvalidSession, "an empty family");
    refused(One("Family.Bad", At(2)), PracticeError::InvalidSession, "a malformed family");
    refused(One(Endurance, At(2), 1000, "skill bad"), PracticeError::InvalidSession, "a malformed skill");
    refused(Session(Endurance, At(2), {}), PracticeError::InvalidSession, "no learning");
    refused(One(Endurance, At(2), 0), PracticeError::InvalidSession, "a zero base");
    refused(One(Endurance, At(2), -5), PracticeError::InvalidSession, "a negative base");
    refused(One(Endurance, At(2), PracticeMaxBaseMilli + 1), PracticeError::InvalidSession, "an enormous base");
    refused(Session(Endurance, At(2), {{"skill.a", 5}, {"skill.a", 5}}), PracticeError::InvalidSession, "a skill twice");
    Check(PracticeErrorCode(PracticeError::InvalidTime) == "practice.invalid_time" && PracticeErrorCode(PracticeError::None).empty(), "error codes");

    PracticeState broken = state;
    broken.Skills[0].BudgetUsedMilli = PracticeSkillDailyBudgetMilli + 1;
    Check(RecordPractice(broken, One(Endurance, At(2))).Error == PracticeError::InvalidState, "an invalid state is refused");

    PracticeState crowded;
    for (std::size_t i = 0; i < PracticeMaxFamilies; ++i)
        Check(RecordPractice(crowded, One(("family." + std::to_string(1000 + i)).c_str(), At(0))).Ok(), "families up to the cap");
    Check(RecordPractice(crowded, One("family.extra", At(0))).Error == PracticeError::TooMany, "too many families");
    Check(RecordPractice(crowded, One("family.1000", At(0))).Ok(), "an existing family is still accepted");
}

void TestText()
{
    PracticeState state;
    Check(SerialisePractice(state) == "pw.practice/1\nlast_at=0\nfamilies=0\nskills=0\nend\n", "the empty state text");
    (void)RecordPractice(state, Session(Endurance, At(0), {{"skill.fitness", 1500}, {"skill.willpower", 200}}));
    (void)RecordPractice(state, One(Endurance, At(2, 30)));
    const std::string d0 = std::to_string(GameClock::DayNumber(At(0)));
    const std::string d2 = std::to_string(GameClock::DayNumber(At(2, 30)));
    const std::string fitness = "skill=skill.fitness," + d2 + ",1000," + d0 + "," + d2;
    const std::string text = SerialisePractice(state);
    Check(text == "pw.practice/1\nlast_at=2910\nfamilies=1\nfamily=family.endurance,0,2910\nskills=2\n" + fitness +
                      "\nskill=skill.willpower," + d0 + ",200," + d0 + "\nend\n",
          "the text lists records in order");
    Check(SerialisePractice(state) == text, "the same state always gives the same text");
    const PracticeParse parsed = DeserialisePractice(text);
    Check(parsed.Ok() && parsed.State == state, "the text round-trips");
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!DeserialisePractice(std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");
    Check(!DeserialisePractice(text + "x").Ok(), "extra text is refused");
    Check(!DeserialisePractice(text + "\n").Ok(), "an extra blank line is refused");

    auto replaced = [&](const std::string& from, const std::string& to)
    {
        std::string copy = text;
        const std::size_t at = copy.find(from);
        Check(at != std::string::npos, "the fixture text is present");
        copy.replace(at, from.size(), to);
        return DeserialisePractice(copy).Ok();
    };
    const std::string family = "family=family.endurance,0,2910";
    Check(!replaced("pw.practice/1", "pw.practice/2"), "an unknown header is refused");
    Check(!replaced("last_at=2910", "last_at=02910"), "a leading zero is refused");
    Check(!replaced("last_at=2910", "last_at=+2910"), "a plus sign is refused");
    Check(!replaced("last_at=2910", "last_at=-1"), "a negative last_at is refused");
    Check(!replaced("families=1", "families=2"), "a wrong count is refused");
    Check(!replaced(family, "family=family.endurance,2910,0"), "descending instants are refused");
    Check(!replaced(family, "family=family.endurance,0,2911"), "an instant after last_at is refused");
    Check(!replaced(family, "family=Family.endurance,0,2910"), "a malformed family ID is refused");
    Check(!replaced(family, "family=family.endurance"), "a family without instants is refused");
    Check(!replaced(family, "family=family.endurance,0,0,0,0"), "too many instants are refused");
    Check(!replaced(fitness, "skill=skill.fitness," + d2 + ",8001," + d0 + "," + d2), "a budget over the cap is refused");
    Check(!replaced(fitness, "skill=skill.fitness," + d2 + ",1000," + d2 + "," + d0), "descending days are refused");
    Check(!replaced(fitness, "skill=skill.fitness," + d2 + ",1000," + d0 + "," + d0), "a repeated day is refused");
    Check(!replaced(fitness, "skill=skill.fitness," + d2 + ",1000," + d0 + "," + std::to_string(GameClock::DayNumber(At(2, 30)) + 1)),
          "a day after last_at is refused");
    Check(!replaced("skill=skill.willpower", "skill=skill.fitness"), "a duplicate skill is refused");
    Check(!replaced("skill=skill.willpower", "skill=skill.aaa"), "unsorted skills are refused");
    PracticeState invalid = state;
    invalid.Skills.pop_back();
    invalid.Skills[0].BudgetUsedMilli = -1;
    Check(SerialisePractice(invalid).empty(), "an inconsistent state serialises to nothing");
}

void TestDeterminism()
{
    auto run = []
    {
        Rng rng{12345};
        PracticeState state;
        GameInstant now = 0;
        std::vector<PracticeForecast> results;
        for (int i = 0; i < 300; ++i)
            results.push_back(RecordPractice(state, RandomSession(rng, now)));
        return std::make_pair(SerialisePractice(state), results);
    };
    const auto first = run();
    const auto second = run();
    Check(!first.first.empty() && first.first == second.first && first.second == second.second, "the same sequence gives identical state and results");
}
} // namespace

int main()
{
    TestFourthSessionTeachesNothing();
    TestWindowRolls();
    TestRouteVariantsShareTheLimit();
    TestSkillBudget();
    TestConsolidation();
    TestRandomSequences();
    TestRefusals();
    TestText();
    TestDeterminism();
    std::cout << "PracticeWindowTests passed (" << Checks << " checks)\n";
    return 0;
}
