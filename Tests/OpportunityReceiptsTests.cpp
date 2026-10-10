// ROADMAP M2-11: opportunity receipts (rules, cap, append-only, day query, refusals, fold).
#include "OpportunityReceiptsTestSupport.h"

using namespace PerfectWorld;

// Defined in OpportunityReceiptsTextTests.cpp.
void TestText();
void TestRandomSequences();
void TestDeterminism();

namespace
{
void TestOverlapRules()
{
    const ReceiptCommitment commitment = Commit("act.study", 1000, 1100, 60, 800, 50);  // window [1100, 1160)
    const std::vector<ReceiptCandidate> candidates = {
        Cand("alt.inside", 900, 1110, 20),
        Cand("alt.before", 900, 1050, 100),        // [1050, 1150): overlap 50
        Cand("alt.after", 900, 1150, 60),          // [1150, 1210): overlap 10
        Cand("alt.touch_end", 900, 1160, 30),      // starts the minute the commitment ends
        Cand("alt.touch_start", 900, 1000, 100),   // ends the minute it starts
        Cand("alt.far", 900, 5000, 60),
        Cand("alt.closed", 900, 1100, 60, false),  // not available at decision time
        Cand("alt.late", 1001, 1100, 60),          // learned one minute after the decision
        Cand("alt.exact", 1000, 1100, 60),         // learned at the decision: known
        Cand("act.study", 900, 1100, 60),          // the commitment itself is not its own alternative
    };
    const auto lines = DecideDisplaced(commitment, candidates);
    Check(Ids(lines) == std::vector<std::string>({"alt.exact", "alt.before", "alt.inside", "alt.after"}), "only overlapping, known, available alternatives, ranked by overlap");
    Check(lines[0].OverlapMinutes == 60 && lines[1].OverlapMinutes == 50 && lines[2].OverlapMinutes == 20 && lines[3].OverlapMinutes == 10, "overlap minutes are recorded");

    ReceiptState state;
    const ReceiptRecordResult result = RecordCommitment(state, commitment, candidates);
    Check(result.Ok() && result.Seq == 1 && !result.Summarised && result.DisplacedCount == 4, "recording succeeds");
    Check(Ids(state.Entries[0].Displaced) == Ids(lines), "the entry records exactly the displaced alternatives");
    Check(state.Entries[0].Displaced[0].KnownAt == 1000 && state.Entries[0].Displaced[0].Source == "source.board", "the entry keeps when and where it was learned");
    const ReceiptTotals totals = ReceiptEntryTotals(state.Entries[0]);
    Check(totals.Commitments == 1 && totals.CommittedMinutes == 60 && totals.LearningMilliXp == 800 && totals.ConditionCostMilli == 50, "commitment totals");
    Check(totals.Displaced == 4 && totals.DisplacedOverlapMinutes == 140, "displacement totals");
    Check(IsReceiptStateValid(state), "the state is valid");

    Check(DecideDisplaced(commitment, {}).empty(), "no candidates displace nothing");
    ReceiptState none;
    Check(RecordCommitment(none, commitment, {Cand("alt.far", 0, 9000, 30)}).Ok() && none.Entries[0].Displaced.empty(), "a commitment that displaced nothing still has a receipt");
}

void TestOrderIndependenceAndDuplicates()
{
    const ReceiptCommitment commitment = Commit("act.a", 100, 200, 100);
    std::vector<ReceiptCandidate> candidates;
    for (int i = 0; i < 9; ++i)
        candidates.push_back(Cand(("alt.n" + std::to_string(i % 5)).c_str(), 50 + i, 150 + i * 10, 60 + i, true, (i * 37) % 4 * 100));
    const auto reference = DecideDisplaced(commitment, candidates);
    Check(Ids(reference).size() == 5, "candidates sharing an ID are one alternative");
    Rng rng{99};
    for (int round = 0; round < 50; ++round)
    {
        for (std::size_t i = candidates.size() - 1; i > 0; --i)
            std::swap(candidates[i], candidates[static_cast<std::size_t>(rng.Next() % (i + 1))]);
        Check(DecideDisplaced(commitment, candidates) == reference, "the decision does not depend on the order passed in");
    }
    for (std::size_t i = 1; i < reference.size(); ++i)
        Check(reference[i - 1].Forecast.LearningMilliXp >= reference[i].Forecast.LearningMilliXp, "ranked by forecast learning, highest first");
}

void TestCapAndSummary()
{
    std::vector<ReceiptCandidate> candidates;
    for (int i = 0; i < 12; ++i)
        candidates.push_back(Cand(("alt.c" + std::to_string(i)).c_str(), 0, 100, 60, true, 1000 - i * 10));
    ReceiptState state;
    const ReceiptRecordResult result = RecordCommitment(state, Commit("act.b", 100, 100, 60), candidates);
    const ReceiptEntry& entry = state.Entries[0];
    Check(result.DisplacedCount == 12 && entry.Displaced.size() == static_cast<std::size_t>(ReceiptMaxDisplacedPerCommitment), "an entry stores at most the cap");
    Check(entry.OmittedCount == 4 && entry.OmittedOverlapMinutes == 240 && entry.OmittedLearningMilliXp == 920 + 910 + 900 + 890, "the rest are summarised");
    Check(entry.Displaced.front().Id == "alt.c0" && entry.Displaced.back().Id == "alt.c7", "the best ranked are kept");
    const ReceiptTotals totals = ReceiptEntryTotals(entry);
    Check(totals.Displaced == 12 && totals.DisplacedOverlapMinutes == 720, "totals include the omitted alternatives");
    Check(IsReceiptStateValid(state), "an entry with omitted alternatives is valid");
}

void TestAppendOnly()
{
    ReceiptState state;
    std::vector<ReceiptEntry> before;
    GameInstant decided = Mid(0);
    for (int i = 0; i < 10; ++i)
    {
        const std::vector<ReceiptCandidate> candidates = {Cand("alt.x", 0, decided + 10, 60, true, 100 * i), Cand("alt.y", Mid(0) + 150, decided + 20, 30)};
        Check(RecordCommitment(state, Commit("act.c", decided, decided + 10, 60, 10 * i, i), candidates).Ok(), "recorded");
        Check(std::equal(before.begin(), before.end(), state.Entries.begin()), "earlier receipts never change when later ones are added");
        before = state.Entries;
        decided += 30;
    }
    Check(state.Entries[0].Displaced.size() == 1 && state.Entries[0].Displaced[0].Id == "alt.x", "alt.y was not yet known at the first decision, so it is absent from that receipt");
    Check(state.Entries[9].Displaced.size() == 2, "and listed once it was known");

    // A save taken now, loaded, and extended: the old receipts are byte-for-byte unchanged.
    const std::string text = SerialiseReceipts(state);
    const ReceiptParse loaded = DeserialiseReceipts(text);
    Check(loaded.Ok() && loaded.State == state && SerialiseReceipts(loaded.State) == text, "save then load gives the same state and the same text");
    ReceiptState resumed = loaded.State;
    Check(RecordCommitment(resumed, Commit("act.c", decided, decided, 45), {Cand("alt.x", 0, decided, 45)}).Ok(), "recording continues after a load");
    Check(std::equal(before.begin(), before.end(), resumed.Entries.begin()) && resumed.Entries.back().Seq == 11, "loaded receipts are unchanged and numbering continues");
}

void TestDayQueryAndBound()
{
    ReceiptState state;
    ReceiptTotals expected;
    const GameInstant day = Mid(3);
    for (int i = 0; i < 30; ++i)
    {
        const GameInstant when = day + i * 20;
        const ReceiptRecordResult result = RecordCommitment(state, Commit("act.d", when, when, 15, 100, 7), {Cand("alt.z", 0, when, 30, true, 50)});
        Check(result.Ok() && result.DisplacedCount == 1, "recorded");
        Check(result.Summarised == (i >= ReceiptMaxEntriesPerDay), "commitments past the cap are summarised");
        Check((result.Seq != 0) == (i < ReceiptMaxEntriesPerDay), "a summarised commitment has no entry");
        expected.Commitments += 1;
        expected.CommittedMinutes += 15;
        expected.LearningMilliXp += 100;
        expected.ConditionCostMilli += 7;
        expected.Displaced += 1;
        expected.DisplacedOverlapMinutes += 15;
        expected.DisplacedLearningMilliXp += 50;
    }
    const ReceiptDay query = ReceiptDayQuery(state, ReceiptDayOf(day));
    Check(query.Entries.size() == static_cast<std::size_t>(ReceiptMaxEntriesPerDay) && !query.Folded, "a day keeps at most the cap in full");
    Check(query.Totals == expected, "the day's totals are exact however many commitments it held");
    Check(query.OverflowTotals.Commitments == 30 - ReceiptMaxEntriesPerDay && state.Overflow.size() == 1, "the overflow is a running sum");
    for (std::size_t i = 0; i < query.Entries.size(); ++i)
        Check(query.Entries[i].Seq == i + 1, "Seq is dense from 1 in recording order");
    Check(IsReceiptStateValid(state), "the state is valid");

    // A commitment crossing midnight belongs to the day it starts on.
    const GameInstant late = Mid(5) - 30;
    Check(RecordCommitment(state, Commit("act.night", late, late, 120), {}).Ok(), "recorded");
    Check(ReceiptDayQuery(state, ReceiptDayOf(late)).Entries.size() == 1 && ReceiptDayQuery(state, ReceiptDayOf(Mid(5))).Entries.empty(), "a midnight-crossing commitment is filed on its start day");
    const ReceiptDay empty = ReceiptDayQuery(state, ReceiptDayOf(Mid(9)));
    Check(empty.Entries.empty() && empty.Totals == ReceiptTotals{} && !empty.Folded, "an empty day costs nothing");

    // A plan made ahead files under the day it will happen, and its receipt carries only what was known when it was made.
    ReceiptState ahead;
    Check(RecordCommitment(ahead, Commit("act.ahead", Mid(10), Mid(12), 60), {Cand("alt.now", Mid(10), Mid(12), 60), Cand("alt.later", Mid(11), Mid(12), 60)}).Ok(), "recorded");
    Check(ReceiptDayQuery(ahead, ReceiptDayOf(Mid(12))).Entries[0].Displaced.size() == 1, "an alternative learned after the decision is not listed");
}

void TestRefusals()
{
    ReceiptState state;
    Check(RecordCommitment(state, Commit("act.e", Mid(1), Mid(1), 30), {Cand("alt.k", 0, Mid(1), 30)}).Ok(), "recorded");
    const ReceiptState snapshot = state;
    const std::vector<ReceiptCandidate> good = {Cand("alt.k", 0, Mid(1), 30)};
    auto refused = [&](const ReceiptCommitment& c, const std::vector<ReceiptCandidate>& cs, ReceiptError expected, const char* message)
    {
        const ReceiptRecordResult result = RecordCommitment(state, c, cs);
        Check(result.Error == expected && !result.Ok() && state == snapshot && !ReceiptErrorCode(expected).empty(), message);
    };
    refused(Commit("Act.Bad", Mid(2), Mid(2), 30), good, ReceiptError::InvalidCommitment, "a malformed activity ID is refused");
    refused(Commit("act.e", Mid(2), Mid(2) - 1, 30), good, ReceiptError::InvalidCommitment, "starting before the decision is refused");
    refused(Commit("act.e", Mid(2), Mid(2), 0), good, ReceiptError::InvalidCommitment, "a zero duration is refused");
    refused(Commit("act.e", Mid(2), Mid(2), ReceiptMaxDurationMinutes + 1), good, ReceiptError::InvalidCommitment, "an over-long duration is refused");
    refused(Commit("act.e", Mid(2), Mid(2), 30, -1), good, ReceiptError::InvalidCommitment, "a negative forecast is refused");
    refused(Commit("act.e", -5, 0, 30), good, ReceiptError::InvalidCommitment, "a negative decision instant is refused");
    ReceiptCandidate bad = Cand("alt.k", 0, Mid(2), 30);
    bad.Source = "has space";
    refused(Commit("act.e", Mid(2), Mid(2), 30), {good[0], bad}, ReceiptError::InvalidCandidate, "a malformed candidate refuses the whole record");
    bad = Cand("alt.k", 0, Mid(2), 0);
    refused(Commit("act.e", Mid(2), Mid(2), 30), {bad}, ReceiptError::InvalidCandidate, "a zero-length candidate is refused");
    refused(Commit("act.e", Mid(2), Mid(2), 30), std::vector<ReceiptCandidate>(ReceiptMaxCandidates + 1, good[0]), ReceiptError::TooManyCandidates, "too many candidates are refused");
    refused(Commit("act.e", Mid(1) - 1, Mid(1) - 1, 30), good, ReceiptError::OutOfOrder, "a decision earlier than the last is refused");
    Check(DecideDisplaced(Commit("Bad", 0, 0, 5), good).empty(), "an invalid commitment displaces nothing");
}

void TestFold()
{
    ReceiptState state;
    ReceiptTotals all;
    for (int d = 0; d < 200; ++d)
    {
        const int count = d == 10 || d == 150 ? 28 : 1 + d % 3;
        for (int i = 0; i < count; ++i)
        {
            const GameInstant when = Mid(d) + 60 + i * 10;
            Check(RecordCommitment(state, Commit("act.f", when, when, 20, 30, 4), {Cand("alt.w", 0, when, 20, true, 10), Cand("alt.v", 0, when + 5, 40)}).Ok(), "recorded");
        }
    }
    all = ReceiptGrandTotals(state);
    Check(all.Commitments > 0, "grand totals count the commitments");
    const ReceiptState unfolded = state;
    Check(FoldReceipts(state, Mid(50)) == 0 && state == unfolded, "nothing is older than 120 days yet");
    const std::size_t folded = FoldReceipts(state, Mid(199));
    Check(folded > 0 && state.Entries.size() + folded == unfolded.Entries.size(), "old entries are folded");
    Check(ReceiptGrandTotals(state) == all, "folding preserves every total");
    Check(FoldReceipts(state, Mid(199)) == 0, "folding again changes nothing");
    Check(IsReceiptStateValid(state) && IsMondayDay(state.FoldedBeforeDay), "the folded state is valid and folds on a Monday");
    Check(ReceiptDayQuery(state, ReceiptDayOf(Mid(10))).Folded && ReceiptDayQuery(state, ReceiptDayOf(Mid(10))).Entries.empty(), "a folded day has no detail");
    Check(ReceiptDayQuery(state, ReceiptDayOf(Mid(150))).Entries.size() == static_cast<std::size_t>(ReceiptMaxEntriesPerDay), "recent days keep their detail");
    for (const ReceiptEntry& kept : state.Entries)
    {
        const auto original = std::find_if(unfolded.Entries.begin(), unfolded.Entries.end(), [&](const ReceiptEntry& e) { return e.Seq == kept.Seq; });
        Check(original != unfolded.Entries.end() && *original == kept, "kept receipts are byte-for-byte what was recorded");
    }
    Check(FindReceiptWeek(state, ReceiptWeekStartDay(Mid(10))) != nullptr, "the week holding day 10 is kept");
    Check(FoldReceipts(state, Mid(400)) > 0 && ReceiptGrandTotals(state) == all, "a later fold keeps every total too");
    Check(RecordCommitment(state, Commit("act.f", Mid(250), Mid(250), 20), {}).Error == ReceiptError::AlreadyFolded, "recording on a folded day is refused");
    const ReceiptParse back = DeserialiseReceipts(SerialiseReceipts(state));
    Check(back.Ok() && back.State == state, "a folded state saves and loads");
}

} // namespace

int main()
{
    TestOverlapRules();
    TestOrderIndependenceAndDuplicates();
    TestCapAndSummary();
    TestAppendOnly();
    TestDayQueryAndBound();
    TestRefusals();
    TestFold();
    TestText();
    TestRandomSequences();
    TestDeterminism();
    std::cout << "OpportunityReceiptsTests passed (" << Checks << " checks)\n";
    return 0;
}
