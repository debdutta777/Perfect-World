// ROADMAP M2-10: relationships and remembered events. Standalone: not wired into WorldState.
#include "IdentityRegistry.h"
#include "RandomStreams.h"
#include "RelationshipsTestSupport.h"

#include <algorithm>
#include <array>
#include <iostream>

namespace
{
struct RelScenario
{
    EventLog Log;
    std::vector<RelationshipAct> Acts;
};

// A pure function of the seed: a log of events between a few people with growing time, one act per event.
RelScenario RelBuildScenario(std::uint64_t seed, std::size_t people, std::size_t events)
{
    RelScenario scenario;
    RandomStream rng(seed);
    GameInstant now = 0;
    for (std::size_t i = 0; i < events; ++i)
    {
        if (rng.Chance(2, 3))
            now += static_cast<GameInstant>(rng.NextBelow(40 * MinutesPerDay));
        const std::size_t a = static_cast<std::size_t>(rng.NextBelow(people));
        std::size_t b = static_cast<std::size_t>(rng.NextBelow(people - 1));
        if (b >= a)
            ++b;
        std::vector<std::string> parties = {RelPerson(a), RelPerson(b)};
        const std::string type = "event_" + std::to_string(rng.NextBelow(5));
        const std::uint64_t seq = RelAppend(scenario.Log, now, type, parties);
        const RelationshipActKind kind = rng.Chance(1, 30) ? RelationshipActKind::Bereave
                                                            : static_cast<RelationshipActKind>(rng.NextBelow(RelationshipActKindCount - 1));
        scenario.Acts.push_back(RelAct(seq, parties[0], parties[1], kind));
    }
    return scenario;
}

RelationshipBook RelApplyAll(const RelScenario& scenario)
{
    RelationshipBook book;
    for (const RelationshipAct& act : scenario.Acts)
        book.Apply(scenario.Log, act);
    return book;
}

std::int32_t RelClamped(std::int64_t value)
{
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(value, RelationshipMilliMin, RelationshipMilliMax));
}

bool RelInvariantsHold(const Relationship& r)
{
    const auto inRange = [](std::int32_t v) { return v >= RelationshipMilliMin && v <= RelationshipMilliMax; };
    if (!inRange(r.Trust) || !inRange(r.Warmth) || !inRange(r.Respect) || !inRange(r.Owed) || !(r.Low < r.High))
        return false;
    if (r.Memories.size() > RelationshipMaxMemories)
        return false;
    for (std::size_t i = 0; i < r.Memories.size(); ++i)
    {
        const RelationshipMemory& m = r.Memories[i];
        if (m.Salience < 0 || m.Salience > RelationshipSalienceMax || m.EventSeq > r.LastSeq)
            return false;
        if (i > 0 && !(r.Memories[i - 1].EventSeq < m.EventSeq))
            return false;
        if ((r.FirstActs & (1u << static_cast<unsigned>(m.Kind))) == 0)
            return false;
    }
    return true;
}

void RelTestTables()
{
    for (std::size_t i = 0; i < RelationshipActKindCount; ++i)
    {
        const auto kind = static_cast<RelationshipActKind>(i);
        const RelationshipEffect e = RelationshipActEffect(kind);
        for (const std::int32_t d : {e.Trust, e.Warmth, e.Respect, e.Owed})
            Check(d >= -RelationshipMaxActDelta && d <= RelationshipMaxActDelta, "every table delta is within the bound");
        Check(e.Salience >= 0 && e.Salience <= RelationshipSalienceMax && e.Salience / RelationshipRepeatSalienceDivisor > 0, "salience is in range");
        Check(ParseRelationshipActKind(RelationshipActKindName(kind)) == kind, "kind names round-trip");
    }
    for (std::size_t i = 0; i < RelationshipStageCount; ++i)
        Check(ParseRelationshipStage(RelationshipStageName(static_cast<RelationshipStage>(i))) == static_cast<RelationshipStage>(i), "stage names round-trip");
    Check(RelationshipActKindName(static_cast<RelationshipActKind>(99)).empty() && !ParseRelationshipActKind("Help") && !ParseRelationshipStage("") &&
              RelationshipActEffect(static_cast<RelationshipActKind>(99)).Salience == 0,
          "bad values are empty or refused");
}

void RelTestBasics()
{
    EventLog log;
    const std::string a = RelPerson(1);
    const std::string b = RelPerson(2);
    const std::uint64_t s1 = RelAppend(log, 100, "lent_notes", {a, b});
    const std::uint64_t s2 = RelAppend(log, 200, "lent_notes_again", {b, a});
    const std::uint64_t s3 = RelAppend(log, 300, "walked_home", {a, b});

    RelationshipBook book;
    const RelationshipApplyResult first = book.Apply(log, RelAct(s1, a, b, RelationshipActKind::Help));
    const Relationship* pair = book.Find(b, a);
    Check(first.Ok() && first.MemoryKept && first.EvictedSeq == 0 && pair != nullptr && book.Find(a, b) == pair, "a first help creates the pair, found in either order");
    const RelationshipEffect help = RelationshipActEffect(RelationshipActKind::Help);
    Check(pair->Trust == help.Trust && pair->Warmth == help.Warmth && pair->Respect == help.Respect && pair->Stage == RelationshipStage::Acquaintance,
          "the first meaningful act changes the relationship by the table");
    Check(RelationshipOwedBy(*pair, b) == help.Owed && RelationshipOwedBy(*pair, a) == -help.Owed && RelationshipOwedBy(*pair, "npc_9") == 0, "the other person owes the actor");
    Check(pair->Memories.size() == 1 && pair->Memories[0].EventSeq == s1 && pair->Memories[0].At == 100 && pair->Memories[0].Type == "lent_notes", "the event is remembered");

    // The same act again cannot change any dimension, even by the other person.
    const Relationship before = *pair;
    const RelationshipApplyResult again = book.Apply(log, RelAct(s2, b, a, RelationshipActKind::Help));
    pair = book.Find(a, b);
    Check(again.Status == RelationshipStatus::Repeat && !again.Ok(), "a repeated help is a Repeat");
    Check(pair->Trust == before.Trust && pair->Warmth == before.Warmth && pair->Respect == before.Respect && pair->Owed == before.Owed &&
              pair->FirstActs == before.FirstActs && pair->Stage == before.Stage,
          "repeating it changes no dimension");
    Check(pair->Memories.size() == 2 && pair->Memories[1].Salience == help.Salience / RelationshipRepeatSalienceDivisor, "the repeat is remembered at reduced salience");

    // A different kind is a different first act; a defend by the other person flips the Owed direction.
    const RelationshipApplyResult defend = book.Apply(log, RelAct(s3, b, a, RelationshipActKind::Defend));
    pair = book.Find(a, b);
    const RelationshipEffect d = RelationshipActEffect(RelationshipActKind::Defend);
    Check(defend.Ok() && pair->Trust == help.Trust + d.Trust && pair->Owed == RelClamped(static_cast<std::int64_t>(help.Owed) - d.Owed), "a different kind is its own first act and Owed nets across directions");
    Check(HashRelationships(book) != HashRelationships(RelationshipBook()), "the hash reflects content");
}

void RelTestRefusals()
{
    EventLog log;
    const std::string a = RelPerson(1);
    const std::string b = RelPerson(2);
    const std::string c = RelPerson(3);
    const std::uint64_t ok = RelAppend(log, 10, "met_in_class", {a, b});
    const std::uint64_t bad = RelAppend(log, 20, "religion_festival", {a, b});
    const std::uint64_t bg = RelAppend(log, 20, "family_background_check", {a, b});
    const std::uint64_t later = RelAppend(log, 30, "met_again", {a, b, c});

    RelationshipBook book;
    Check(book.Apply(log, RelAct(ok, a, b, RelationshipActKind::Gift)).Ok(), "setup act applies");
    const RelationshipBook snapshot = book;
    const auto refused = [&](const RelationshipAct& act, RelationshipStatus want, const char* message) {
        Check(book.Apply(log, act).Status == want && book == snapshot, message);
    };
    refused(RelAct(99, a, b, RelationshipActKind::Help), RelationshipStatus::BadEvent, "a missing log entry is refused");
    refused(RelAct(0, a, b, RelationshipActKind::Help), RelationshipStatus::BadEvent, "Seq 0 is refused");
    refused(RelAct(later, a, "npc_9", RelationshipActKind::Help), RelationshipStatus::BadParties, "a person who is not in the event is refused");
    refused(RelAct(later, a, a, RelationshipActKind::Help), RelationshipStatus::BadParties, "a pair of the same person is refused");
    refused(RelAct(later, "Not An Id", b, RelationshipActKind::Help), RelationshipStatus::BadParties, "a malformed ID is refused");
    refused(RelAct(later, a, b, static_cast<RelationshipActKind>(40)), RelationshipStatus::BadKind, "a bad kind is refused");
    refused(RelAct(bad, a, b, RelationshipActKind::Help), RelationshipStatus::BadEventType, "an event type naming religion is refused");
    refused(RelAct(bg, a, b, RelationshipActKind::Help), RelationshipStatus::BadEventType, "an event type naming a background is refused");
    refused(RelAct(ok, a, b, RelationshipActKind::Help), RelationshipStatus::OutOfOrder, "an act at or before the pair's last Seq is refused");
    Check(book.Apply(log, RelAct(later, a, c, RelationshipActKind::Help)).Ok() && book.Size() == 2, "another pair is independent");
    Check(IsRelationshipEventTypeFair("streaming_night") && IsRelationshipEventTypeFair("met_in_class") && !IsRelationshipEventTypeFair("Caste_x") &&
              !IsRelationshipEventTypeFair("a.stream.b") && !IsRelationshipEventTypeFair("language-gap") && !IsRelationshipEventTypeFair(""),
          "the event type rule matches words, not substrings");
}

void RelTestIdentityAgnostic()
{
    // The same acts between two authored IDs and two generated IDs give identical numbers: nothing keys on who they are.
    IdentityOrigin o1;
    o1.Cohort = "nation_bra.chess.age_16_20.y2026";
    IdentityOrigin o2 = o1;
    o2.Slot = 5;
    const std::string g1 = DeriveGeneratedIdentityId(77, o1);
    const std::string g2 = DeriveGeneratedIdentityId(77, o2);
    const auto run = [](const std::string& x, const std::string& y) {
        EventLog log;
        RelationshipBook book;
        GameInstant at = 0;
        for (std::size_t k = 0; k < RelationshipActKindCount - 1; ++k)
        {
            const std::uint64_t seq = RelAppend(log, at += 50, "scene", {x, y});
            book.Apply(log, RelAct(seq, x, y, static_cast<RelationshipActKind>(k)));
        }
        const Relationship* r = book.Find(x, y);
        return std::array<std::int32_t, 3>{r->Trust, r->Warmth, r->Respect};
    };
    Check(run(RelPerson(1), RelPerson(2)) == run(g1, g2), "dimensions depend on the acts, never on the identities");
}

void RelTestMemorial()
{
    EventLog log;
    const std::string a = RelPerson(1);
    const std::string b = RelPerson(2);
    RelationshipBook book;
    book.Apply(log, RelAct(RelAppend(log, 1, "met", {a, b}), a, b, RelationshipActKind::ShareTime));
    Check(book.Apply(log, RelAct(RelAppend(log, 2, "passed_away", {a, b}), a, b, RelationshipActKind::Bereave)).Ok(), "a bereavement applies");
    const Relationship frozen = *book.Find(a, b);
    Check(frozen.Stage == RelationshipStage::Memorial && book.Size() == 1, "the pair becomes a memorial and is not deleted");
    Check(book.Apply(log, RelAct(RelAppend(log, 3, "visit", {a, b}), b, a, RelationshipActKind::Gift)).Status == RelationshipStatus::Frozen && *book.Find(a, b) == frozen,
          "a memorial no longer changes");
}

void RelTestSalience()
{
    RelationshipMemory m;
    m.At = 1000;
    m.Salience = 800;
    const std::int64_t half = RelationshipSalienceHalfLifeMinutes;
    Check(RelationshipEffectiveSalience(m, 1000) == 800 && RelationshipEffectiveSalience(m, 500) == 800, "no decay at age 0, and a future memory counts as new");
    Check(RelationshipEffectiveSalience(m, 1000 + half) == 400 && RelationshipEffectiveSalience(m, 1000 + 2 * half) == 200, "halves per half-life");
    Check(RelationshipEffectiveSalience(m, 1000 + half / 2) == 600, "linear between halvings");
    Check(RelationshipEffectiveSalience(m, 1000 + 100 * half) == 0 && RelationshipEffectiveSalience(m, INT64_MAX) == 0, "very old memories are 0");
    for (GameInstant t = 1000; t < 1000 + 12 * half; t += half / 7 + 1)
        Check(RelationshipEffectiveSalience(m, t + half / 7 + 1) <= RelationshipEffectiveSalience(m, t), "decay is monotone and never negative");
}

std::uint64_t RelFillPair(EventLog& log, RelationshipBook& book, GameInstant at, RelationshipActKind kind, RelationshipApplyResult* out = nullptr)
{
    const std::string a = RelPerson(1);
    const std::string b = RelPerson(2);
    const std::uint64_t seq = RelAppend(log, at, "scene", {a, b});
    const RelationshipApplyResult result = book.Apply(log, RelAct(seq, a, b, kind));
    if (out)
        *out = result;
    return seq;
}

void RelTestEviction()
{
    const auto build = [](RelationshipBook& book, EventLog& log) {
        RelationshipApplyResult r;
        RelFillPair(log, book, 100, RelationshipActKind::Gift);                     // seq 1, 400
        RelFillPair(log, book, 100, RelationshipActKind::Confide);                  // seq 2, 800
        for (int i = 0; i < 30; ++i)                                                // seq 3..32, repeats at 100
            RelFillPair(log, book, 100, RelationshipActKind::Gift, &r);
        Check(book.Find(RelPerson(1), RelPerson(2))->Memories.size() == RelationshipMaxMemories, "the pair holds exactly the maximum");
        RelFillPair(log, book, 100, RelationshipActKind::Gift, &r);
        Check(r.Status == RelationshipStatus::Repeat && r.MemoryKept && r.EvictedSeq == 3, "the oldest of the weakest memories is evicted");
        RelFillPair(log, book, 100, RelationshipActKind::ShareTime, &r);
        Check(r.Ok() && r.EvictedSeq == 4, "a stronger new memory displaces the next weakest");
        RelFillPair(log, book, 100, RelationshipActKind::ShareTime, &r); // 75 < every held salience
        Check(r.Status == RelationshipStatus::Repeat && !r.MemoryKept && r.EvictedSeq == 0, "a weaker new memory loses at once");
        const Relationship* pair = book.Find(RelPerson(1), RelPerson(2));
        Check(pair->Memories.size() == RelationshipMaxMemories && pair->LastSeq == 35, "bounded, and the order is still tracked");
        Check(pair->Memories.front().EventSeq == 1, "the strong early memories are still there");
    };
    EventLog logA;
    EventLog logB;
    RelationshipBook a;
    RelationshipBook b;
    build(a, logA);
    build(b, logB);
    Check(a == b && SerialiseRelationships(a) == SerialiseRelationships(b), "eviction is deterministic");

    // Farming cannot push the early strong memories out.
    for (int i = 0; i < 300; ++i)
        RelFillPair(logA, a, 100, RelationshipActKind::Gift);
    const Relationship& farmed = *a.Find(RelPerson(1), RelPerson(2));
    const auto has = [&](std::uint64_t seq) {
        return std::any_of(farmed.Memories.begin(), farmed.Memories.end(), [seq](const RelationshipMemory& m) { return m.EventSeq == seq; });
    };
    Check(farmed.Memories.size() == RelationshipMaxMemories && has(1) && has(2) && has(34), "300 repeated gifts never displace first-act memories");
    Check(RelationshipsMatchLog(a, logA), "every kept memory references a real log entry");

    // Much later every old memory has decayed; the eviction rule agrees with a brute-force reading of it.
    const GameInstant far = 100 + 12 * RelationshipSalienceHalfLifeMinutes;
    const Relationship before = *a.Find(RelPerson(1), RelPerson(2));
    std::size_t weakest = 0;
    for (std::size_t i = 1; i < before.Memories.size(); ++i)
    {
        if (RelationshipEffectiveSalience(before.Memories[i], far) < RelationshipEffectiveSalience(before.Memories[weakest], far))
            weakest = i;
    }
    RelationshipApplyResult r;
    RelFillPair(logA, a, far, RelationshipActKind::Gift, &r);
    Check(r.MemoryKept && r.EvictedSeq == before.Memories[weakest].EventSeq, "decay changes who is weakest");
    const std::vector<RelationshipMemory> ranked = RelationshipMemoriesBySalience(*a.Find(RelPerson(1), RelPerson(2)), far);
    Check(ranked.size() == RelationshipMaxMemories && ranked.front().EventSeq == a.Find(RelPerson(1), RelPerson(2))->LastSeq,
          "the freshest memory ranks first once the rest have decayed");
    for (std::size_t i = 1; i < ranked.size(); ++i)
        Check(RelationshipEffectiveSalience(ranked[i - 1], far) >= RelationshipEffectiveSalience(ranked[i], far), "ranked order is by effective salience");
}

void RelTestStepInvariants(const RelScenario& scenario, bool wholeBookChecks)
{
    RelationshipBook book;
    for (const RelationshipAct& act : scenario.Acts)
    {
        const RelationshipBook before = book;
        const Relationship* priorPtr = before.Find(act.Actor, act.Other);
        const Relationship prior = priorPtr ? *priorPtr : Relationship();
        const RelationshipApplyResult result = book.Apply(scenario.Log, act);
        const Relationship* after = book.Find(act.Actor, act.Other);
        if (result.Ok() || result.Status == RelationshipStatus::Repeat)
        {
            Check(after != nullptr && RelInvariantsHold(*after), "the touched pair keeps its invariants");
            const bool low = act.Actor < act.Other;
            const RelationshipEffect e = RelationshipActEffect(act.Kind);
            const auto moved = [](std::int32_t from, std::int32_t to) { return std::abs(static_cast<std::int64_t>(to) - from); };
            Check(moved(prior.Trust, after->Trust) <= RelationshipMaxActDelta && moved(prior.Warmth, after->Warmth) <= RelationshipMaxActDelta &&
                      moved(prior.Respect, after->Respect) <= RelationshipMaxActDelta && moved(prior.Owed, after->Owed) <= RelationshipMaxActDelta,
                  "no dimension moves by more than the bound");
            if (result.Ok())
            {
                Check(after->Trust == RelClamped(static_cast<std::int64_t>(prior.Trust) + e.Trust) &&
                          after->Owed == RelClamped(static_cast<std::int64_t>(prior.Owed) + (low ? e.Owed : -e.Owed)) &&
                          after->FirstActs == (prior.FirstActs | (1u << static_cast<unsigned>(act.Kind))) && (prior.FirstActs & (1u << static_cast<unsigned>(act.Kind))) == 0,
                      "an applied act is exactly the table delta, clamped, and a new first act");
            }
            else
            {
                Check(after->Trust == prior.Trust && after->Warmth == prior.Warmth && after->Respect == prior.Respect && after->Owed == prior.Owed &&
                          after->FirstActs == prior.FirstActs,
                      "a repeat moves nothing");
            }
        }
        else if (wholeBookChecks)
        {
            Check(book == before, "a refused act changes nothing");
        }
    }
    for (const auto& entry : book.Pairs())
        Check(RelInvariantsHold(entry.second), "every pair keeps its invariants");
    Check(RelationshipsMatchLog(book, scenario.Log), "every memory references a real log entry");
    const std::string text = SerialiseRelationships(book);
    const std::optional<RelationshipBook> loaded = ParseRelationships(text);
    Check(loaded && *loaded == book && SerialiseRelationships(*loaded) == text && HashRelationships(*loaded) == HashRelationships(book),
          "the book survives a save unchanged");
}

void RelTestRandomSequences()
{
    for (std::uint64_t seed = 1; seed <= 10000; ++seed)
    {
        const std::size_t people = 2 + static_cast<std::size_t>(seed % 5);
        const std::size_t events = 20 + static_cast<std::size_t>(seed % 90);
        RelTestStepInvariants(RelBuildScenario(seed * 2654435761ULL, people, events), seed % 50 == 0);
    }
}

void RelTestReplayAndOrder()
{
    const RelScenario scenario = RelBuildScenario(4242, 5, 200);
    const RelationshipBook one = RelApplyAll(scenario);
    const RelationshipBook two = RelApplyAll(RelBuildScenario(4242, 5, 200));
    Check(one == two && HashRelationships(one) == HashRelationships(two) && SerialiseRelationships(one) == SerialiseRelationships(two),
          "applying the same log twice from scratch gives identical state");

    // Save halfway, load, continue: equals a straight run.
    RelationshipBook half;
    for (std::size_t i = 0; i < 100; ++i)
        half.Apply(scenario.Log, scenario.Acts[i]);
    std::optional<RelationshipBook> resumed = ParseRelationships(SerialiseRelationships(half));
    Check(resumed.has_value(), "a mid-history save loads");
    for (std::size_t i = 100; i < scenario.Acts.size(); ++i)
        resumed->Apply(scenario.Log, scenario.Acts[i]);
    Check(*resumed == one, "save, load and continue equals a straight run");

    // Acts on different pairs are independent: any interleaving that keeps each pair's order gives the same book.
    std::vector<RelationshipAct> byPair = scenario.Acts;
    std::stable_sort(byPair.begin(), byPair.end(), [](const RelationshipAct& x, const RelationshipAct& y) {
        return std::minmax(x.Actor, x.Other) < std::minmax(y.Actor, y.Other);
    });
    RelationshipBook grouped;
    for (const RelationshipAct& act : byPair)
        grouped.Apply(scenario.Log, act);
    Check(grouped == one, "acts on different pairs commute");

    // Within one pair order matters, so a reversed pair of acts is refused rather than silently reordered.
    EventLog log;
    const std::uint64_t s1 = RelAppend(log, 1, "e1", {RelPerson(1), RelPerson(2)});
    const std::uint64_t s2 = RelAppend(log, 2, "e2", {RelPerson(1), RelPerson(2)});
    RelationshipBook book;
    Check(book.Apply(log, RelAct(s2, RelPerson(1), RelPerson(2), RelationshipActKind::Help)).Ok() &&
              book.Apply(log, RelAct(s1, RelPerson(1), RelPerson(2), RelationshipActKind::Gift)).Status == RelationshipStatus::OutOfOrder,
          "within a pair, acts must arrive in log order");
}

void RelTestLimits()
{
    EventLog log;
    RelationshipBook book;
    std::uint64_t applied = 0;
    RelationshipApplyResult last;
    for (std::size_t i = 0; i < 200 && last.Status != RelationshipStatus::Full; ++i)
    {
        for (std::size_t j = i + 1; j < 200 && last.Status != RelationshipStatus::Full; ++j)
        {
            const std::uint64_t seq = RelAppend(log, 1, "met", {RelPerson(i), RelPerson(j)});
            last = book.Apply(log, RelAct(seq, RelPerson(i), RelPerson(j), RelationshipActKind::Gift));
            applied += last.Ok() ? 1 : 0;
        }
    }
    Check(last.Status == RelationshipStatus::Full && applied == RelationshipMaxPairs && book.Size() == RelationshipMaxPairs, "the pair limit refuses the next new pair");
    const auto loaded = ParseRelationships(SerialiseRelationships(book));
    Check(loaded && *loaded == book, "a book at the limit round-trips");
}

} // namespace

int main()
{
    RelTestTables();
    RelTestBasics();
    RelTestRefusals();
    RelTestIdentityAgnostic();
    RelTestMemorial();
    RelTestSalience();
    RelTestEviction();
    RelTestReplayAndOrder();
    RelTestLimits();
    RelTestRandomSequences();
    RelTestSaveRefusals();
    std::cout << "PerfectWorldRelationshipTests passed, " << RelTestSupport::Checks << " checks\n";
    return 0;
}
