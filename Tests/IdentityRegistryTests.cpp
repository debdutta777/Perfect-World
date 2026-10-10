// ROADMAP M2-05: persistent identity registry. The event log tests are in EventLogTests.cpp;
// both are linked into PerfectWorldIdentityTests and this file owns main().
#include "IdentityTestSupport.h"

#include <cstdlib>
#include <iostream>
#include <iterator>
#include <utility>
#include <vector>

void EventLogTestsRun();

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

namespace
{
IdentityRegistry IdTestBuild(std::uint64_t seed, const std::vector<std::pair<std::size_t, std::int64_t>>& members)
{
    IdentityRegistry registry;
    registry.WorldSeed = seed;
    for (const auto& [cohort, slot] : members)
    {
        const IdentityResult result = ReferenceCohortMember(registry, IdTestOrigin(IdTestCohorts[cohort], slot), IdTestRefFor(slot));
        Check(result.Ok(), "a valid reference is accepted");
    }
    return registry;
}

std::vector<std::pair<std::size_t, std::int64_t>> IdTestAllMembers()
{
    std::vector<std::pair<std::size_t, std::int64_t>> members;
    for (std::size_t cohort = 0; cohort < IdTestCohorts.size(); ++cohort)
    {
        for (std::int64_t slot = 0; slot < 40; ++slot)
            members.emplace_back(cohort, slot);
    }
    return members;
}

void IdTestShuffle(std::vector<std::pair<std::size_t, std::int64_t>>& members, RandomStream& rng)
{
    for (std::size_t i = members.size(); i > 1; --i)
        std::swap(members[i - 1], members[static_cast<std::size_t>(rng.NextBelow(i))]);
}

IdentityRegistry IdTestMixed()
{
    IdentityRegistry registry = IdTestBuild(IdTestSeed, IdTestAllMembers());
    IdentityFirstRef ref;
    ref.Kind = IdentityReferenceKind::Dialogue;
    ref.At = 5;
    ref.Ref = "scene_first_day";
    Check(RegisterAuthoredIdentity(registry, "npc_class_teacher", ref).Status == IdentityStatus::Created, "authored cast registers");
    ref.Ref.clear();
    Check(RegisterAuthoredIdentity(registry, "npc_neighbour.aunty", ref).Status == IdentityStatus::Created, "a second authored person registers");
    return registry;
}

void IdTestIdentityBasics()
{
    // ID scheme: "p_" plus 16 lowercase hex digits, a pure function of seed and origin.
    const IdentityOrigin origin = IdTestOrigin(IdTestCohorts[0], 3);
    const std::string id = DeriveGeneratedIdentityId(IdTestSeed, origin);
    Check(id.size() == 18 && id.starts_with("p_") && IsValidIdentityId(id), "a generated ID is p_ plus 16 hex digits");
    Check(DeriveGeneratedIdentityId(IdTestSeed, origin) == id, "derivation is repeatable");
    // Pinned: a change here renumbers every generated person in every save.
    Check(id == "p_d3bb718af9160972", "the generated ID for the pinned origin is unchanged");
    Check(DeriveGeneratedIdentityId(IdTestSeed + 1, origin) != id, "another world seed gives another person");
    Check(DeriveGeneratedIdentityId(IdTestSeed, IdTestOrigin(IdTestCohorts[0], 4)) != id, "another slot gives another person");
    Check(DeriveGeneratedIdentityId(IdTestSeed, IdTestOrigin(IdTestCohorts[1], 3)) != id, "another cohort gives another person");
    Check(DeriveGeneratedIdentityId(IdTestSeed, IdTestOrigin(IdTestCohorts[0], -1)).empty(), "a negative slot has no ID");
    Check(DeriveGeneratedIdentityId(IdTestSeed, IdTestOrigin("Bad Cohort", 0)).empty(), "a cohort that is not a token has no ID");
    Check(DeriveGeneratedIdentityId(IdTestSeed, IdTestOrigin("", 0)).empty(), "an empty cohort has no ID");

    Check(IsValidIdentityId("npc_teacher") && IsValidIdentityId("p_0123456789abcdef"), "authored and generated IDs are valid");
    Check(!IsValidIdentityId("p_abc") && !IsValidIdentityId("p_0123456789ABCDEF") && !IsValidIdentityId("") &&
              !IsValidIdentityId("has space") && !IsValidIdentityId("-leading") && !IsValidIdentityId(std::string(97, 'a')),
          "malformed IDs are refused");
    for (std::size_t i = 0; i < IdentityReferenceKindCount; ++i)
    {
        const auto kind = static_cast<IdentityReferenceKind>(i);
        Check(ParseIdentityReferenceKind(IdentityReferenceKindName(kind)) == kind, "reference kind names round-trip");
    }
    Check(!ParseIdentityReferenceKind("telepathy") && IdentityReferenceKindName(static_cast<IdentityReferenceKind>(99)).empty(),
          "unknown kinds are refused");
}

void IdTestFirstReference()
{
    IdentityRegistry registry;
    registry.WorldSeed = IdTestSeed;
    const IdentityOrigin origin = IdTestOrigin(IdTestCohorts[2], 11);
    IdentityFirstRef first;
    first.Kind = IdentityReferenceKind::News;
    first.At = 480;
    first.Ref = "news_league_table";

    const IdentityResult created = ReferenceCohortMember(registry, origin, first);
    Check(created.Status == IdentityStatus::Created && registry.People.size() == 1, "the first reference creates exactly one identity");
    Check(created.Id == DeriveGeneratedIdentityId(IdTestSeed, origin), "the created ID is the derived ID");
    const Identity snapshot = *FindIdentity(registry, created.Id);
    Check(snapshot.FirstRef == first && snapshot.Origin == origin && !snapshot.Authored, "the identity records its origin and first reference");

    // A later, different reference changes nothing: first reference stands.
    IdentityFirstRef later;
    later.Kind = IdentityReferenceKind::Dialogue;
    later.At = 99999;
    later.Ref = "scene_met_in_person";
    for (int i = 0; i < 5; ++i)
    {
        const IdentityResult again = ReferenceCohortMember(registry, origin, later);
        Check(again.Status == IdentityStatus::Existing && again.Id == created.Id, "referencing again returns the same identity");
    }
    Check(registry.People.size() == 1 && *FindIdentity(registry, created.Id) == snapshot, "the identity is never changed or duplicated");
    Check(FindIdentity(registry, "p_0000000000000000") == nullptr, "an unknown ID is not found");
    Check(DeriveIdentityRollSeed(IdTestSeed, snapshot) == DeriveIdentityRollSeed(IdTestSeed, snapshot), "the roll seed is repeatable");
    Check(DeriveIdentityRollSeed(IdTestSeed + 1, snapshot) != DeriveIdentityRollSeed(IdTestSeed, snapshot), "the roll seed follows the world seed");

    // Refusals change nothing.
    const IdentityRegistry before = registry;
    Check(ReferenceCohortMember(registry, IdTestOrigin("NOT A TOKEN", 0), first).Status == IdentityStatus::BadOrigin, "a bad origin is refused");
    Check(ReferenceCohortMember(registry, IdTestOrigin(IdTestCohorts[0], -2), first).Status == IdentityStatus::BadOrigin, "a negative slot is refused");
    IdentityFirstRef bad = first;
    bad.At = -1;
    Check(ReferenceCohortMember(registry, IdTestOrigin(IdTestCohorts[0], 1), bad).Status == IdentityStatus::BadReference, "a negative time is refused");
    bad = first;
    bad.Ref = "has space";
    Check(ReferenceCohortMember(registry, IdTestOrigin(IdTestCohorts[0], 1), bad).Status == IdentityStatus::BadReference, "a bad record ID is refused");
    bad = first;
    bad.Kind = static_cast<IdentityReferenceKind>(77);
    Check(ReferenceCohortMember(registry, IdTestOrigin(IdTestCohorts[0], 1), bad).Status == IdentityStatus::BadReference, "a bad kind is refused");
    Check(RegisterAuthoredIdentity(registry, "p_0123456789abcdef", first).Status == IdentityStatus::BadOrigin, "an authored ID may not look generated");
    Check(RegisterAuthoredIdentity(registry, "Bad", first).Status == IdentityStatus::BadOrigin, "an authored ID must be a token");
    Check(registry == before, "refusals change nothing");

    // Authored: idempotent too, and never confused with a generated person.
    Check(RegisterAuthoredIdentity(registry, "npc_head_teacher", first).Status == IdentityStatus::Created, "authored cast registers");
    Check(RegisterAuthoredIdentity(registry, "npc_head_teacher", later).Status == IdentityStatus::Existing, "authored cast registers once");
    Check(FindIdentity(registry, "npc_head_teacher")->FirstRef == first && registry.People.size() == 2, "authored first reference stands");

    // A forced ID collision (a hand-made registry holding the ID for another origin) is reported, not overwritten.
    IdentityRegistry forged;
    forged.WorldSeed = IdTestSeed;
    Identity impostor;
    impostor.Id = created.Id;
    impostor.Origin = IdTestOrigin(IdTestCohorts[1], 0);
    forged.People.emplace(impostor.Id, impostor);
    Check(ReferenceCohortMember(forged, origin, first).Status == IdentityStatus::IdCollision && forged.People.at(created.Id) == impostor,
          "an ID held by another origin is a collision and is left alone");
}

void IdTestDeterminismAcrossOrderings()
{
    const auto all = IdTestAllMembers();
    const IdentityRegistry reference = IdTestBuild(IdTestSeed, all);
    Check(reference.People.size() == all.size(), "every distinct member got exactly one identity");
    const std::string referenceText = SerialiseIdentityRegistry(reference);

    // Same references, repeated twice and in many shuffled orders, give byte-identical registries.
    RandomStream rng(0xABCDEF);
    for (int round = 0; round < 25; ++round)
    {
        auto shuffled = all;
        IdTestShuffle(shuffled, rng);
        auto doubled = shuffled;
        doubled.insert(doubled.end(), all.begin(), all.end()); // every member referenced a second time
        const IdentityRegistry built = IdTestBuild(IdTestSeed, doubled);
        Check(built == reference, "ordering and repeats do not change the registry");
        Check(SerialiseIdentityRegistry(built) == referenceText, "ordering and repeats do not change the serialised bytes");
        Check(HashIdentityRegistry(built) == HashIdentityRegistry(reference), "ordering and repeats do not change the hash");
    }
    // Two runs from nothing agree; a different seed does not.
    Check(SerialiseIdentityRegistry(IdTestBuild(IdTestSeed, all)) == referenceText, "two runs agree");
    Check(HashIdentityRegistry(IdTestBuild(IdTestSeed + 1, all)) != HashIdentityRegistry(reference), "another world seed gives another registry");

    // No two members share an ID (64-bit keyed hash over 120 members).
    Check(reference.People.size() == 120, "120 members are 120 distinct identities");
}

void IdTestRoundTrip()
{
    const IdentityRegistry registry = IdTestMixed();
    const std::string text = SerialiseIdentityRegistry(registry);
    const std::optional<IdentityRegistry> loaded = ParseIdentityRegistry(text);
    Check(loaded.has_value(), "a serialised registry parses");
    Check(*loaded == registry, "identities survive a save unchanged");
    Check(SerialiseIdentityRegistry(*loaded) == text, "re-serialising gives the same bytes");
    Check(HashIdentityRegistry(*loaded) == HashIdentityRegistry(registry), "the hash survives a save");

    // An empty registry round-trips too.
    IdentityRegistry empty;
    empty.WorldSeed = 0;
    Check(ParseIdentityRegistry(SerialiseIdentityRegistry(empty)) == empty, "an empty registry round-trips");

    // Save, load, then keep playing, equals playing straight through.
    IdentityRegistry straight = IdTestMixed();
    IdentityRegistry reloaded = *ParseIdentityRegistry(SerialiseIdentityRegistry(IdTestMixed()));
    for (std::int64_t slot = 100; slot < 130; ++slot)
    {
        ReferenceCohortMember(straight, IdTestOrigin(IdTestCohorts[1], slot), IdTestRefFor(slot));
        ReferenceCohortMember(reloaded, IdTestOrigin(IdTestCohorts[1], slot), IdTestRefFor(slot));
    }
    for (const auto& [cohort, slot] : IdTestAllMembers())
    {
        Check(ReferenceCohortMember(reloaded, IdTestOrigin(IdTestCohorts[cohort], slot), IdTestRefFor(slot + 5000)).Status == IdentityStatus::Existing,
              "a member met before the save is not made again after it");
    }
    Check(straight == reloaded, "continuing after a reload equals playing straight through");

    // Extreme but valid integers survive.
    IdentityRegistry extreme;
    extreme.WorldSeed = UINT64_MAX;
    IdentityFirstRef ref;
    ref.At = INT64_MAX;
    Check(ReferenceCohortMember(extreme, IdTestOrigin("c", INT64_MAX), ref).Ok(), "the largest slot and time are accepted");
    Check(ParseIdentityRegistry(SerialiseIdentityRegistry(extreme)) == extreme, "the largest integers round-trip");
}

void IdTestRefusals()
{
    const IdentityRegistry registry = IdTestMixed();
    const std::string text = SerialiseIdentityRegistry(registry);
    const auto parse = [](std::string_view s) { return ParseIdentityRegistry(s).has_value(); };

    IdTestEveryTruncationRefused(text, parse, "every truncation of a serialised registry is refused");
    // The corruption sweep is quadratic in size; use the small registry for it.
    IdentityRegistry small;
    small.WorldSeed = 42;
    ReferenceCohortMember(small, IdTestOrigin(IdTestCohorts[0], 0), IdTestRefFor(0));
    ReferenceCohortMember(small, IdTestOrigin(IdTestCohorts[1], 1), IdTestRefFor(1));
    RegisterAuthoredIdentity(small, "npc_a", IdTestRefFor(2));
    const std::string smallText = SerialiseIdentityRegistry(small);
    Check(parse(smallText), "the small registry parses");
    IdTestEveryTruncationRefused(smallText, parse, "every truncation of the small registry is refused");
    IdTestEveryCorruptionRefused(smallText, parse, "every single-byte corruption of a registry is refused");

    Check(!parse(text + "\n") && !parse(text + "extra 1\n") && !parse(text + text), "trailing data is refused");
    Check(!parse(IdTestReplace(text, "pw_identities 1", "pw_identities 2")), "an unknown version is refused");
    Check(!parse(IdTestReplace(text, "seed ", "sead ")), "an unknown field name is refused");
    Check(!parse(IdTestReplace(text, "count 122", "count 121")) && !parse(IdTestReplace(text, "count 122", "count 123")), "a wrong count is refused");
    Check(!parse(IdTestReplace(text, "count 122", "count 0122")), "a non-canonical number is refused");
    Check(!parse(IdTestReplace(text, "person ", "persons ")), "an unknown line keyword is refused");
    Check(!parse(IdTestReplace(text, " family_history ", " telepathy ")), "an unknown reference kind is refused");
    Check(!parse(IdTestReplace(text, "person ", "person  ")), "a doubled space is refused");

    // Honest-looking edits with a recomputed hash must still fail the structural checks.
    const auto reseal = [](IdentityRegistry r) { return SerialiseIdentityRegistry(r); };
    IdentityRegistry forgedId = small;
    Identity moved = forgedId.People.rbegin()->second; // the last ID sorts after "npc_a", so it is a generated person
    forgedId.People.erase(std::prev(forgedId.People.end()));
    moved.Id = "p_ffffffffffffffff";
    forgedId.People.emplace(moved.Id, moved);
    Check(!parse(reseal(forgedId)), "a generated ID that does not match its origin and seed is refused");
    IdentityRegistry wrongSeed = small;
    wrongSeed.WorldSeed = 43;
    Check(!parse(reseal(wrongSeed)), "people from another world seed are refused");
    IdentityRegistry negativeSlot = small;
    negativeSlot.People.rbegin()->second.Origin.Slot = -3;
    Check(!parse(reseal(negativeSlot)), "a negative slot is refused");
    IdentityRegistry authoredLooksGenerated = small;
    Identity fake;
    fake.Id = "p_0123456789abcdef";
    fake.Authored = true;
    authoredLooksGenerated.People.emplace(fake.Id, fake);
    Check(!parse(reseal(authoredLooksGenerated)), "an authored person with a generated-looking ID is refused");
}

void IdTestRandomSequences()
{
    // Random registrations, authored mixes and refusals: serialising and parsing always agree, in any order.
    RandomStream rng(0xFEEDBEEF);
    for (int round = 0; round < 40; ++round)
    {
        IdentityRegistry registry;
        registry.WorldSeed = rng.NextU64();
        std::vector<std::string> expectedIds;
        const int steps = 1 + static_cast<int>(rng.NextBelow(60));
        for (int step = 0; step < steps; ++step)
        {
            const std::int64_t slot = static_cast<std::int64_t>(rng.NextBelow(25));
            IdentityFirstRef ref = IdTestRefFor(slot);
            if (rng.Chance(1, 4))
            {
                const std::string content = "npc_" + std::to_string(rng.NextBelow(8));
                const bool known = FindIdentity(registry, content) != nullptr;
                const IdentityResult result = RegisterAuthoredIdentity(registry, content, ref);
                Check(result.Status == (known ? IdentityStatus::Existing : IdentityStatus::Created), "authored registration is created once, then existing");
            }
            else
            {
                const IdentityOrigin origin = IdTestOrigin(IdTestCohorts[rng.NextBelow(3)], slot);
                const std::string id = DeriveGeneratedIdentityId(registry.WorldSeed, origin);
                const bool known = FindIdentity(registry, id) != nullptr;
                const IdentityResult result = ReferenceCohortMember(registry, origin, ref);
                Check(result.Id == id && result.Status == (known ? IdentityStatus::Existing : IdentityStatus::Created),
                      "a random reference creates once and is then existing");
            }
        }
        const std::string text = SerialiseIdentityRegistry(registry);
        const auto loaded = ParseIdentityRegistry(text);
        Check(loaded && *loaded == registry && SerialiseIdentityRegistry(*loaded) == text, "a random registry round-trips unchanged");
    }
}

} // namespace

int main()
{
    IdTestIdentityBasics();
    IdTestFirstReference();
    IdTestDeterminismAcrossOrderings();
    IdTestRoundTrip();
    IdTestRefusals();
    IdTestRandomSequences();
    EventLogTestsRun();
    std::cout << "PerfectWorldIdentityTests passed, " << Checks << " checks\n";
    return 0;
}
