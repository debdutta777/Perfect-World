// ROADMAP M4-01: the conduct ledger. Standalone; not wired into WorldState, the reducer or the save.
#include "ConductLedger.h"

#include "IdentityRegistry.h"
#include "RandomStreams.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace PerfectWorld;

namespace
{
int ConductChecks = 0;

void Expect(bool condition, const char* message)
{
    ++ConductChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

ConductRecord ConductMake(const std::string& test, ConductAction action, std::int32_t stakes = 1, GameInstant at = 0,
                          std::vector<std::string> parties = {})
{
    ConductRecord record;
    record.TestId = test;
    record.ChoiceId = "choice_" + std::string(ConductActionName(action));
    record.Action = action;
    record.Stakes = stakes;
    record.At = at;
    record.Parties = std::move(parties);
    return record;
}

std::int32_t ConductSumOf(const ConductPoints& points)
{
    std::int32_t total = 0;
    for (const std::int32_t value : points)
        total += value;
    return total;
}

void ConductTestTables()
{
    for (std::size_t i = 0; i < ConductDimensionCount; ++i)
    {
        const auto dimension = static_cast<ConductDimension>(i);
        Expect(ParseConductDimension(ConductDimensionName(dimension)) == dimension, "dimension names round-trip");
    }
    for (std::size_t i = 0; i < ConductActionCount; ++i)
    {
        const auto action = static_cast<ConductAction>(i);
        Expect(ParseConductAction(ConductActionName(action)) == action, "action names round-trip");
    }
    Expect(ConductDimensionName(static_cast<ConductDimension>(ConductDimensionCount)).empty() &&
               ConductActionName(static_cast<ConductAction>(ConductActionCount)).empty() && !ParseConductAction("shout") &&
               !ParseConductDimension("honesty") && !ParseConductAction("Observe"),
           "bad names are empty or refused");

    for (std::int32_t stakes = ConductMinStakes; stakes <= ConductMaxStakes; ++stakes)
    {
        const std::int32_t expected = ConductActionTotal(stakes);
        Expect(expected > 0 && expected <= ConductTestTotalCap, "a test always adds something, within the test total cap");
        for (std::size_t i = 0; i < ConductActionCount; ++i)
        {
            const ConductPoints points = ConductActionEvidence(static_cast<ConductAction>(i), stakes);
            Expect(ConductSumOf(points) == expected, "every action carries the same total: no action is the lesser one");
            std::int32_t nonZero = 0;
            for (const std::int32_t value : points)
            {
                Expect(value >= 0 && value <= ConductTestDimensionCap, "evidence is never negative and within the per-test dimension cap");
                nonZero += value > 0 ? 1 : 0;
            }
            Expect(nonZero == 2, "an action maps to exactly a primary and a secondary dimension");
        }
    }
    // Withdrawing is as weighty as assisting, and both are recorded as conduct.
    Expect(ConductSumOf(ConductActionEvidence(ConductAction::Withdraw, 2)) == ConductSumOf(ConductActionEvidence(ConductAction::Assist, 2)),
           "withdrawal is exactly as weighty as a rescue");
    Expect(ConductSumOf(ConductActionEvidence(static_cast<ConductAction>(ConductActionCount), 1)) == 0 &&
               ConductSumOf(ConductActionEvidence(ConductAction::Assist, 0)) == 0 && ConductSumOf(ConductActionEvidence(ConductAction::Assist, 4)) == 0,
           "an invalid action or stakes gives no evidence");
    // Every dimension is reachable as a primary by some action, so no dimension is dead.
    for (std::size_t d = 0; d < ConductDimensionCount; ++d)
    {
        bool reachable = false;
        for (std::size_t a = 0; a < ConductActionCount; ++a)
            reachable = reachable || ConductActionEvidence(static_cast<ConductAction>(a), 1)[d] == ConductPrimaryPoints;
        Expect(reachable, "every dimension has an action that makes it primary");
    }
    // Stakes never lower evidence.
    for (std::size_t a = 0; a < ConductActionCount; ++a)
        Expect(ConductActionTotal(1) <= ConductActionTotal(2) && ConductActionTotal(2) <= ConductActionTotal(3), "stakes never lower the total");
    Expect(ConductBandFor(0) == ConductBand::NotObserved && ConductBandFor(1) == ConductBand::Slight &&
               ConductBandFor(ConductClearThreshold - 1) == ConductBand::Slight && ConductBandFor(ConductClearThreshold) == ConductBand::Clear &&
               ConductBandFor(ConductStrongThreshold - 1) == ConductBand::Clear && ConductBandFor(ConductStrongThreshold) == ConductBand::Strong &&
               ConductBandFor(ConductDimensionCap) == ConductBand::Strong,
           "band thresholds");
    for (std::size_t b = 0; b < ConductBandCount; ++b)
        Expect(!ConductBandName(static_cast<ConductBand>(b)).empty(), "band names exist");
    Expect(ConductBandName(static_cast<ConductBand>(ConductBandCount)).empty(), "a bad band has no name");
}

void ConductTestOnce()
{
    ConductLedger ledger;
    const ConductResult first = RecordConduct(ledger, ConductMake("dungeon_freeze", ConductAction::Assist, 2, 100, {"npc_tara_das"}));
    Expect(first.Ok() && first.Status == ConductStatus::Recorded && ledger.Entries.size() == 1, "the first resolution is recorded");
    const ConductLedger before = ledger;
    const ConductResult replay = RecordConduct(ledger, ConductMake("dungeon_freeze", ConductAction::Evade, 3, 200));
    Expect(replay.Status == ConductStatus::Replay && !replay.Ok() && replay.Points == first.Points, "a replay is refused and names the standing evidence");
    Expect(ledger == before, "a replay changes nothing: the first resolution stands");
    Expect(RecordConduct(ledger, ConductMake("dungeon_freeze", ConductAction::Assist, 2, 100, {"npc_tara_das"})).Status == ConductStatus::Replay &&
               ledger == before,
           "even an identical replay does not count twice");
    const ConductEntry* found = FindConductEntry(ledger, "dungeon_freeze");
    Expect(found != nullptr && found->Record.Action == ConductAction::Assist && FindConductEntry(ledger, "dungeon_other") == nullptr, "lookup by test id");
    Expect(ConductTotals(ledger) == first.Points, "the total is the one test's evidence");
}

void ConductTestCaps()
{
    // One test, any action, any stakes: never above the per-test caps, never Strong on its own.
    for (std::size_t a = 0; a < ConductActionCount; ++a)
    {
        for (std::int32_t stakes = ConductMinStakes; stakes <= ConductMaxStakes; ++stakes)
        {
            ConductLedger ledger;
            Expect(RecordConduct(ledger, ConductMake("t_only", static_cast<ConductAction>(a), stakes)).Ok(), "a single test records");
            const ConductSummary summary = SummariseConduct(ledger);
            for (const ConductDimensionSummary& dimension : summary.Dimensions)
                Expect(dimension.Band != ConductBand::Strong, "no single test can make a dimension Strong");
            Expect(ConductSumOf(ConductTotals(ledger)) <= ConductTestTotalCap, "a single test stays within the test total cap");
        }
    }
    Expect(ConductStrongThreshold > ConductTestDimensionCap, "the Strong threshold exceeds what one test can add to a dimension");
    Expect(ConductTestDimensionCap * 2 >= ConductStrongThreshold, "two tests can reach Strong, so Strong needs at least two tests");

    // Repeating the same choice saturates at the dimension cap and never passes it.
    ConductLedger ledger;
    for (std::size_t i = 0; i < ConductMaxTests; ++i)
        Expect(RecordConduct(ledger, ConductMake("t_" + std::to_string(i), ConductAction::Observe, 3, static_cast<GameInstant>(i))).Ok(), "tests record");
    const ConductPoints totals = ConductTotals(ledger);
    Expect(totals[static_cast<std::size_t>(ConductDimension::Attentiveness)] == ConductDimensionCap, "the dimension saturates at its cap");
    const ConductSummary summary = SummariseConduct(ledger);
    const ConductDimensionSummary& attentiveness = summary.Dimensions[static_cast<std::size_t>(ConductDimension::Attentiveness)];
    Expect(attentiveness.Band == ConductBand::Strong && attentiveness.Milli == 1000 && attentiveness.Evidence.size() == ConductTopEvidenceCount,
           "a saturated dimension is Strong at 1000 and names its top evidence");
    Expect(summary.TestsRecorded == ConductMaxTests, "the summary counts its tests");
    Expect(RecordConduct(ledger, ConductMake("t_extra", ConductAction::Observe, 1, 100)).Status == ConductStatus::LedgerFull,
           "the ledger holds at most ConductMaxTests tests");
    Expect(ConductTotals(ledger) == totals, "a full ledger refusal changes nothing");

    // The three-test dungeon: three different choices give a spread, with named evidence.
    ConductLedger dungeon;
    Expect(RecordConduct(dungeon, ConductMake("d_one", ConductAction::Assist, 2, 10)).Ok() &&
               RecordConduct(dungeon, ConductMake("d_two", ConductAction::Improvise, 2, 20)).Ok() &&
               RecordConduct(dungeon, ConductMake("d_three", ConductAction::Withdraw, 2, 30)).Ok(),
           "the three dungeon tests record");
    const ConductSummary spread = SummariseConduct(dungeon);
    const ConductDimensionSummary& care = spread.Dimensions[static_cast<std::size_t>(ConductDimension::Care)];
    Expect(care.Band == ConductBand::Clear && care.Evidence.size() == 1 && care.Evidence[0].TestId == "d_one" && care.Evidence[0].ChoiceId == "choice_assist" &&
               care.Evidence[0].Points == 8,
           "a rescue reads as Care, naming the test and choice");
    Expect(spread.Dimensions[static_cast<std::size_t>(ConductDimension::Restraint)].Band == ConductBand::Clear,
           "withdrawing reads as Restraint, a Clear reading like any other");
}

void ConductTestRefusals()
{
    ConductLedger ledger;
    Expect(RecordConduct(ledger, ConductMake("base_test", ConductAction::Hold, 1, 50, {"npc_a"})).Ok(), "a base entry");
    const ConductLedger base = ledger;
    IdentityRegistry registry;
    Expect(RegisterAuthoredIdentity(registry, "npc_known", IdentityFirstRef{}).Ok(), "a known identity");

    auto refused = [&](const ConductRecord& record, ConductStatus status, const char* message, const IdentityRegistry* reg = nullptr) {
        const ConductResult result = RecordConduct(ledger, record, reg);
        Expect(result.Status == status && !result.Ok() && ledger == base, message);
    };
    refused(ConductMake("", ConductAction::Hold), ConductStatus::BadTest, "an empty test id is refused");
    refused(ConductMake("Upper", ConductAction::Hold), ConductStatus::BadTest, "an uppercase test id is refused");
    refused(ConductMake("has space", ConductAction::Hold), ConductStatus::BadTest, "a spaced test id is refused");
    refused(ConductMake(std::string(97, 'a'), ConductAction::Hold), ConductStatus::BadTest, "a too long test id is refused");
    ConductRecord badChoice = ConductMake("t_x", ConductAction::Hold);
    badChoice.ChoiceId = "Free text verdict";
    refused(badChoice, ConductStatus::BadChoice, "a free-text choice is refused");
    badChoice.ChoiceId.clear();
    refused(badChoice, ConductStatus::BadChoice, "an empty choice is refused");
    refused(ConductMake("t_x", static_cast<ConductAction>(ConductActionCount)), ConductStatus::BadAction, "an out-of-range action is refused");
    refused(ConductMake("t_x", ConductAction::Hold, 0), ConductStatus::BadStakes, "stakes 0 is refused");
    refused(ConductMake("t_x", ConductAction::Hold, 4), ConductStatus::BadStakes, "stakes 4 is refused");
    refused(ConductMake("t_x", ConductAction::Hold, -1), ConductStatus::BadStakes, "negative stakes are refused");
    refused(ConductMake("t_x", ConductAction::Hold, 1, -1), ConductStatus::BadTime, "a negative time is refused");
    refused(ConductMake("t_x", ConductAction::Hold, 1, 49), ConductStatus::BadTime, "a time before the last test is refused");
    refused(ConductMake("t_x", ConductAction::Hold, 1, 60, {"npc_a", "npc_a"}), ConductStatus::BadParties, "a repeated party is refused");
    refused(ConductMake("t_x", ConductAction::Hold, 1, 60, {"npc_a", "npc_b", "npc_c", "npc_d", "npc_e"}), ConductStatus::BadParties, "too many parties are refused");
    refused(ConductMake("t_x", ConductAction::Hold, 1, 60, {"Not Valid"}), ConductStatus::BadParties, "an invalid party ID is refused");
    refused(ConductMake("t_x", ConductAction::Hold, 1, 60, {"p_nothex"}), ConductStatus::BadParties, "a malformed generated ID is refused");
    refused(ConductMake("t_x", ConductAction::Hold, 1, 60, {"npc_a", "npc_known"}), ConductStatus::UnknownIdentity, "a party outside the registry is refused", &registry);
    Expect(RecordConduct(ledger, ConductMake("t_known", ConductAction::Hold, 1, 60, {"npc_known"}), &registry).Ok(), "a registered party is accepted");
}

void ConductTestFairness()
{
    const std::array<std::string_view, 16> words = {"religion", "religions", "religious", "faith",  "caste",  "language", "languages", "tongue",
                                                    "stream",   "gender",    "sex",       "male",   "female", "family",   "household", "background"};
    for (const std::string_view word : words)
    {
        const std::string plain(word);
        for (const std::string& token : {plain, "a_" + plain, plain + "_b", "x." + plain + ":y", "x-" + plain})
        {
            Expect(!IsConductTokenFair(token), "a forbidden word makes a token unfair");
            ConductLedger ledger;
            Expect(RecordConduct(ledger, ConductMake(token, ConductAction::Assist)).Status == ConductStatus::BadTest && ledger.Entries.empty(),
                   "an unfair test id is refused");
            ConductRecord choice = ConductMake("t_fair", ConductAction::Assist);
            choice.ChoiceId = token;
            Expect(RecordConduct(ledger, choice).Status == ConductStatus::BadChoice && ledger.Entries.empty(), "an unfair choice id is refused");
            Expect(RecordConduct(ledger, ConductMake("t_fair", ConductAction::Assist, 1, 0, {token})).Status == ConductStatus::UnfairToken &&
                       ledger.Entries.empty(),
                   "an unfair party is refused");
        }
    }
    Expect(IsConductTokenFair("dungeon_freezing_classmate") && IsConductTokenFair("npc_tara_das") && IsConductTokenFair("streamline") &&
               IsConductTokenFair("p_0123456789abcdef"),
           "ordinary tokens are fair (only whole words are forbidden)");

    // Evidence depends on the action and stakes alone: choice, parties and time change nothing.
    ConductLedger a;
    ConductLedger b;
    ConductRecord x = ConductMake("t_one", ConductAction::Improvise, 2, 10, {});
    ConductRecord y = ConductMake("t_one", ConductAction::Improvise, 2, 9000, {"npc_a", "npc_b"});
    y.ChoiceId = "another_choice";
    Expect(RecordConduct(a, x).Points == RecordConduct(b, y).Points, "choice id, parties and time never change the evidence");
}

void ConductTestSave()
{
    ConductLedger ledger;
    Expect(RecordConduct(ledger, ConductMake("d_one", ConductAction::Assist, 2, 10, {"npc_b", "npc_a"})).Ok() &&
               RecordConduct(ledger, ConductMake("d_two", ConductAction::Evade, 3, 10)).Ok() &&
               RecordConduct(ledger, ConductMake("d_three", ConductAction::Withdraw, 1, 1440, {"p_0123456789abcdef"})).Ok(),
           "the sample ledger records");
    Expect(ledger.Entries[0].Record.Parties == std::vector<std::string>({"npc_a", "npc_b"}), "parties are stored in ascending order");
    const std::string text = SerialiseConductLedger(ledger);
    Expect(SerialiseConductLedger(ledger) == text, "serialising is deterministic");
    const auto loaded = ParseConductLedger(text);
    Expect(loaded && *loaded == ledger && SerialiseConductLedger(*loaded) == text && HashConductLedger(*loaded) == HashConductLedger(ledger),
           "a save and load round-trips byte for byte");
    Expect(text.rfind("pw_conduct 1\ncount 3\ntest d_one choice_assist assist 2 10 npc_a,npc_b ", 0) == 0, "the documented layout");

    const ConductLedger empty;
    const std::string emptyText = SerialiseConductLedger(empty);
    Expect(emptyText == "pw_conduct 1\ncount 0\nend " + std::to_string(HashConductLedger(empty)) + "\n", "the empty form");
    const auto emptyLoaded = ParseConductLedger(emptyText);
    Expect(emptyLoaded && *emptyLoaded == empty, "an empty ledger round-trips");

    for (std::size_t length = 0; length < text.size(); ++length)
        Expect(!ParseConductLedger(std::string_view(text).substr(0, length)), "every truncation is refused");
    for (std::size_t length = 0; length < emptyText.size(); ++length)
        Expect(!ParseConductLedger(std::string_view(emptyText).substr(0, length)), "every truncation of the empty form is refused");

    const std::array<char, 4> replacements = {'0', '7', 'z', ' '};
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        for (const char replacement : replacements)
        {
            if (text[i] == replacement || text[i] == '\n')
                continue;
            std::string corrupt = text;
            corrupt[i] = replacement;
            Expect(!ParseConductLedger(corrupt), "every single-byte corruption is refused");
        }
    }
    Expect(!ParseConductLedger(text + "x") && !ParseConductLedger(text + "\n") && !ParseConductLedger("\n" + text), "extra bytes are refused");
    std::string crlf = text;
    crlf.replace(crlf.find('\n'), 1, "\r\n");
    Expect(!ParseConductLedger(crlf), "carriage returns are refused");
    std::string wrongVersion = text;
    wrongVersion.replace(wrongVersion.find(" 1\n"), 3, " 2\n");
    Expect(!ParseConductLedger(wrongVersion), "a wrong version is refused");

    // Structurally valid text with a correct hash, but content the rules forbid: serialise a hand-built bad ledger.
    auto refusedLedger = [](const ConductLedger& bad, const char* message) {
        Expect(!ParseConductLedger(SerialiseConductLedger(bad)), message);
    };
    ConductLedger tampered = ledger;
    tampered.Entries[0].Points[0] += 1;
    refusedLedger(tampered, "evidence that differs from the table is refused");
    tampered = ledger;
    tampered.Entries[1].Record.TestId = "d_one";
    refusedLedger(tampered, "a repeated test is refused");
    tampered = ledger;
    tampered.Entries[2].Record.At = 5;
    refusedLedger(tampered, "a time going backwards is refused");
    tampered = ledger;
    tampered.Entries[0].Record.Parties = {"npc_b", "npc_a"};
    refusedLedger(tampered, "parties out of ascending order are refused");
    tampered = ledger;
    tampered.Entries[0].Record.ChoiceId = "religion_choice";
    refusedLedger(tampered, "an unfair token is refused");
    tampered = ledger;
    tampered.Entries[0].Record.Stakes = 3;
    refusedLedger(tampered, "stakes that disagree with the stored evidence are refused");
    tampered = ledger;
    for (std::size_t i = 0; i < ConductMaxTests; ++i)
        tampered.Entries.push_back(tampered.Entries[2]);
    refusedLedger(tampered, "more than the maximum number of tests is refused");
    tampered = ledger;
    tampered.Entries[0].Record.Action = static_cast<ConductAction>(ConductActionCount);
    refusedLedger(tampered, "a bad action is refused");

    // A correct hash on the right content passes; a wrong hash on the same content does not.
    std::string badHash = text;
    badHash.back() = '\n';
    badHash[badHash.size() - 2] = badHash[badHash.size() - 2] == '1' ? '2' : '1';
    Expect(!ParseConductLedger(badHash), "a wrong hash is refused");
}

void ConductTestRandom()
{
    RandomStream rng(0xC0DEC0DE);
    const std::array<std::string, 6> parties = {"npc_a", "npc_b", "npc_c", "p_0123456789abcdef", "npc_d", "bad party"};
    for (int round = 0; round < 10000; ++round)
    {
        ConductLedger ledger;
        const std::size_t attempts = static_cast<std::size_t>(rng.NextBelow(12));
        GameInstant at = 0;
        for (std::size_t i = 0; i < attempts; ++i)
        {
            ConductRecord record;
            record.TestId = "test_" + std::to_string(rng.NextBelow(8));
            record.ChoiceId = rng.Chance(1, 20) ? "religion_x" : "choice_" + std::to_string(rng.NextBelow(5));
            record.Action = static_cast<ConductAction>(rng.NextBelow(ConductActionCount + 1));
            record.Stakes = static_cast<std::int32_t>(rng.NextInRange(-1, 4));
            at += static_cast<GameInstant>(rng.NextInRange(-20, 400));
            record.At = at;
            const std::size_t partyCount = static_cast<std::size_t>(rng.NextBelow(5));
            for (std::size_t p = 0; p < partyCount; ++p)
                record.Parties.push_back(parties[rng.NextBelow(parties.size())]);

            const ConductLedger before = ledger;
            const ConductResult result = RecordConduct(ledger, record);
            if (!result.Ok())
                Expect(ledger == before, "a refused input changes nothing");
            else
                Expect(ledger.Entries.size() == before.Entries.size() + 1, "an accepted input adds exactly one entry");
            if (at < 0)
                at = 0;
        }

        Expect(ledger.Entries.size() <= ConductMaxTests, "the ledger stays bounded");
        const ConductPoints totals = ConductTotals(ledger);
        for (std::size_t d = 0; d < ConductDimensionCount; ++d)
            Expect(totals[d] >= 0 && totals[d] <= ConductDimensionCap, "a dimension total stays within its cap");
        GameInstant last = 0;
        std::vector<std::string> seen;
        for (const ConductEntry& entry : ledger.Entries)
        {
            Expect(ConductSumOf(entry.Points) <= ConductTestTotalCap, "no entry exceeds the test total cap");
            Expect(entry.Points == ConductActionEvidence(entry.Record.Action, entry.Record.Stakes), "every entry's evidence is its action's table row");
            for (const std::int32_t value : entry.Points)
                Expect(value >= 0 && value <= ConductTestDimensionCap, "no entry exceeds the per-dimension test cap");
            Expect(entry.Record.At >= last, "times never decrease");
            last = entry.Record.At;
            for (const std::string& id : seen)
                Expect(id != entry.Record.TestId, "each test counts once");
            seen.push_back(entry.Record.TestId);
            Expect(IsConductTokenFair(entry.Record.ChoiceId), "no unfair choice was recorded");
        }

        const ConductSummary summary = SummariseConduct(ledger);
        Expect(summary.TestsRecorded == ledger.Entries.size(), "the summary counts the tests");
        for (std::size_t d = 0; d < ConductDimensionCount; ++d)
        {
            const ConductDimensionSummary& dimension = summary.Dimensions[d];
            Expect(dimension.Milli >= 0 && dimension.Milli <= 1000 && dimension.Band == ConductBandFor(totals[d]) &&
                       dimension.Evidence.size() <= ConductTopEvidenceCount,
                   "the summary is bounded and matches the totals");
            for (std::size_t i = 1; i < dimension.Evidence.size(); ++i)
                Expect(dimension.Evidence[i - 1].Points >= dimension.Evidence[i].Points, "evidence is listed most to least");
            if (totals[d] == 0)
                Expect(dimension.Band == ConductBand::NotObserved && dimension.Evidence.empty(), "an unobserved dimension names nothing");
        }

        const std::string text = SerialiseConductLedger(ledger);
        const auto loaded = ParseConductLedger(text);
        Expect(loaded && *loaded == ledger && SerialiseConductLedger(*loaded) == text, "every random ledger round-trips");
    }
}

void ConductTestDeterministic()
{
    auto build = []() {
        ConductLedger ledger;
        RandomStream rng(77);
        GameInstant at = 0;
        for (int i = 0; i < 20; ++i)
        {
            at += static_cast<GameInstant>(rng.NextBelow(100));
            RecordConduct(ledger, ConductMake("t_" + std::to_string(rng.NextBelow(10)), static_cast<ConductAction>(rng.NextBelow(ConductActionCount)),
                                              static_cast<std::int32_t>(rng.NextInRange(1, 3)), at));
        }
        return ledger;
    };
    const ConductLedger first = build();
    const ConductLedger second = build();
    Expect(first == second && SerialiseConductLedger(first) == SerialiseConductLedger(second) && HashConductLedger(first) == HashConductLedger(second) &&
               SummariseConduct(first) == SummariseConduct(second),
           "the same sequence gives the same ledger, text, hash and summary");
    ConductLedger different = first;
    different.Entries[0].Record.ChoiceId = "another_choice";
    Expect(HashConductLedger(different) != HashConductLedger(first), "the hash covers the named choice");
    Expect(SummariseConduct(ConductLedger()).TestsRecorded == 0 && SummariseConduct(ConductLedger()).Dimensions[0].Band == ConductBand::NotObserved,
           "an empty ledger summarises as nothing observed");
}
} // namespace

int main()
{
    ConductTestTables();
    ConductTestOnce();
    ConductTestCaps();
    ConductTestRefusals();
    ConductTestFairness();
    ConductTestSave();
    ConductTestRandom();
    ConductTestDeterministic();
    std::cout << "PerfectWorldConductTests: " << ConductChecks << " checks passed\n";
    return 0;
}
