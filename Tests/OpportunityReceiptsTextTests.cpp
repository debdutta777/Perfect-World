// ROADMAP M2-11: opportunity receipts (text form, truncation, random sequences, determinism).
#include "OpportunityReceiptsTestSupport.h"

using namespace PerfectWorld;

namespace
{
ReceiptState BuildTextState()
{
    ReceiptState state;
    auto add = [&](int day, int count, int candidates)
    {
        for (int i = 0; i < count; ++i)
        {
            const GameInstant when = Mid(day) + 60 + i * 10;
            std::vector<ReceiptCandidate> list;
            for (int c = 0; c < candidates; ++c)
                list.push_back(Cand(("alt.t" + std::to_string(c)).c_str(), 0, when + c, 25 + c, true, 100 * c, c % 2 ? "source.friend" : "source.board"));
            Check(RecordCommitment(state, Commit("act.t", when, when, 20, 40 + i, 3), list).Ok(), "recorded");
        }
    };
    add(2, 3, 2);
    add(3, 26, 1);
    add(20, 1, 10);  // more than the cap, so the entry carries omitted counters
    add(20, 25, 1);  // the day fills and overflows
    add(21, 2, 3);
    Check(FoldReceipts(state, Mid(135)) > 0, "the older days fold");
    Check(!state.Entries.empty() && !state.Overflow.empty() && !state.Weeks.empty() && state.Entries[0].OmittedCount == 2, "the sample state has omitted lines, overflow and weeks");
    return state;
}

std::string ReplaceFirst(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "the text to mutate is present");
    text.replace(at, from.size(), to);
    return text;
}

void TextCases()
{
    const ReceiptState state = BuildTextState();
    const std::string text = SerialiseReceipts(state);
    Check(text.rfind(std::string(ReceiptTextHeader) + "\nstate next_seq=", 0) == 0 && text.substr(text.size() - 4) == "end\n", "the text opens with the header and ends with end");
    const ReceiptParse parsed = DeserialiseReceipts(text);
    Check(parsed.Ok() && parsed.State == state && SerialiseReceipts(parsed.State) == text, "round trip is exact and canonical");
    Check(SerialiseReceipts(ReceiptState{}) == std::string(ReceiptTextHeader) + "\nstate next_seq=1 last_decided=0 folded_before=0\nentries=0\noverflow=0\nweeks=0\nend\n", "the empty state's text is fixed");
    Check(DeserialiseReceipts(SerialiseReceipts(ReceiptState{})).Ok(), "the empty state loads");

    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!DeserialiseReceipts(std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");

    const std::string folded = "folded_before=" + std::to_string(state.FoldedBeforeDay);
    const std::vector<std::pair<std::string, std::string>> mutations = {
        {"state next_seq=", "state  next_seq="},
        {"\noverflow=", "\noverflow=0"},
        {"\nweeks=", "\nweeks=+"},
        {"\nentries=", "\nentries=-0"},
        {" last_decided=", " last_decided_=1"},
        {" last_decided=", " folded_before=1 last_decided="},
        {"\ne seq=", "\nE seq="},
        {" source=", " src="},
        {"\nend\n", "\nend \n"},
        {folded, "folded_before=" + std::to_string(state.FoldedBeforeDay + 1)},
        {folded, "folded_before=" + std::to_string(state.FoldedBeforeDay + 7 * 400)},
        {"pw.receipts/1", "pw.receipts/2"},
        {"\nentries=", "\nentries=99999999999999999999 "},
        {"\nentries=", "\nentries=1000000\nentries="},
    };
    for (const auto& [from, to] : mutations)
        Check(!DeserialiseReceipts(ReplaceFirst(text, from, to)).Ok(), "a non-canonical or edited text is refused");
    Check(!DeserialiseReceipts(text + "extra\n").Ok() && !DeserialiseReceipts(text + "\n").Ok() && !DeserialiseReceipts(text + text).Ok(), "data after the end line is refused");
    std::string crlf;
    for (const char c : text)
        crlf += c == '\n' ? std::string("\r\n") : std::string(1, c);
    Check(!DeserialiseReceipts(crlf).Ok() && !DeserialiseReceipts("").Ok(), "CRLF and empty text are refused");
    const std::size_t displacedLine = text.find("\nd ");
    const std::size_t lineEnd = text.find('\n', displacedLine + 1);
    Check(!DeserialiseReceipts(text.substr(0, displacedLine) + text.substr(lineEnd)).Ok(), "a missing displaced line is refused");

    // States that no recording could have produced do not serialise.
    auto breaks = [&](auto&& mutate, const char* message)
    {
        ReceiptState bad = state;
        mutate(bad);
        Check(!IsReceiptStateValid(bad) && SerialiseReceipts(bad).empty(), message);
    };
    breaks([](ReceiptState& s) { std::swap(s.Entries[0], s.Entries[1]); }, "entries out of recording order");
    breaks([](ReceiptState& s) { s.Entries[1].Seq = s.Entries[0].Seq; }, "a repeated Seq");
    breaks([](ReceiptState& s) { s.Entries[0].OmittedCount = 0; }, "omitted totals without a count");
    breaks([](ReceiptState& s) { s.Entries[0].Displaced[0].OverlapMinutes += 1; }, "an overlap that is not the arithmetic overlap");
    breaks([](ReceiptState& s) { s.Entries[0].Displaced[0].KnownAt = s.Entries[0].DecidedAt + 1; }, "an alternative known after the decision");
    breaks([](ReceiptState& s) { s.Entries[0].Displaced[1] = s.Entries[0].Displaced[0]; }, "a repeated alternative");
    breaks([](ReceiptState& s) { std::swap(s.Entries[0].Displaced[0], s.Entries[0].Displaced[1]); }, "alternatives out of rank order");
    breaks([](ReceiptState& s) { s.Entries[0].Displaced[0].Id = s.Entries[0].ActivityId; }, "an activity listed as its own alternative");
    breaks([](ReceiptState& s) { s.Overflow[0].Day += 1; }, "an overflow day that never filled");
    breaks([](ReceiptState& s) { s.Weeks[0].StartDay += 1; }, "a week not opening on Monday");
    breaks([](ReceiptState& s) { s.Weeks[0].Totals.Commitments = 0; }, "an empty week");
    breaks([](ReceiptState& s) { s.NextSeq = 1; }, "a Seq at or beyond the next");
    breaks([](ReceiptState& s) { s.LastDecidedAt = 0; }, "a last decision earlier than a receipt");
}

const char* const ActPool[] = {"act.study", "act.rest", "act.work", "act.walk", "act.help", "act.read"};
const char* const AltPool[] = {"alt.a", "alt.b", "alt.c", "alt.d", "alt.e", "alt.f", "alt.g", "alt.h", "alt.i", "alt.j", "alt.k", "alt.l", "act.study", "act.walk"};
const char* const SourcePool[] = {"source.board", "source.friend", "source.phone"};

// The expected totals of one commitment, worked out independently of the module: one alternative per ID (the one with
// the most forecast learning, then the most overlap), only if known, available, overlapping, and not the activity itself.
ReceiptTotals OracleTotals(const ReceiptCommitment& c, const std::vector<ReceiptCandidate>& candidates)
{
    std::map<std::string, std::pair<std::int32_t, std::int64_t>> best;
    for (const ReceiptCandidate& candidate : candidates)
    {
        if (!candidate.Available || candidate.KnownAt > c.DecidedAt || candidate.Id == c.ActivityId)
            continue;
        const std::int64_t overlap = std::min<std::int64_t>(c.Start + c.DurationMinutes, candidate.Start + candidate.DurationMinutes) - std::max(c.Start, candidate.Start);
        if (overlap <= 0)
            continue;
        const std::pair<std::int32_t, std::int64_t> key{candidate.Forecast.LearningMilliXp, overlap};
        const auto it = best.find(candidate.Id);
        if (it == best.end() || key > it->second)
            best[candidate.Id] = key;
    }
    ReceiptTotals totals;
    totals.Commitments = 1;
    totals.CommittedMinutes = c.DurationMinutes;
    totals.LearningMilliXp = c.Forecast.LearningMilliXp;
    totals.ConditionCostMilli = c.Forecast.ConditionCostMilli;
    for (const auto& [id, key] : best)
    {
        ++totals.Displaced;
        totals.DisplacedLearningMilliXp += key.first;
        totals.DisplacedOverlapMinutes += key.second;
    }
    return totals;
}

void RunRandomSequence(std::uint64_t seed)
{
    Rng rng{seed * 7919 + 13};
    const bool dense = seed % 10 == 0;
    ReceiptState state;
    ReceiptTotals grand;
    std::map<std::int64_t, ReceiptTotals> perDay;
    std::map<std::int64_t, ReceiptTotals> perWeek;
    std::map<std::uint64_t, ReceiptEntry> expected;
    GameInstant decided = Mid(0);
    const int steps = static_cast<int>(rng.Pick(1, dense ? 60 : 40));
    for (int step = 0; step < steps; ++step)
    {
        decided += dense ? rng.Pick(0, 10) : rng.Next() % 200 == 0 ? rng.Pick(100, 300) * MinutesPerDay : rng.Next() % 8 == 0 ? rng.Pick(0, 3 * MinutesPerDay) : rng.Pick(0, 120);
        ReceiptCommitment commitment = Commit(ActPool[rng.Next() % 6], decided, decided + rng.Pick(0, 600), static_cast<std::int32_t>(rng.Pick(1, 240)), static_cast<std::int32_t>(rng.Pick(0, 2000)), static_cast<std::int32_t>(rng.Pick(0, 300)));
        std::vector<ReceiptCandidate> candidates;
        for (std::int64_t i = rng.Pick(0, 14); i > 0; --i)
        {
            ReceiptCandidate candidate = Cand(AltPool[rng.Next() % 14], std::max<GameInstant>(0, decided - rng.Pick(-30, 3000)),
                                              std::max<GameInstant>(0, commitment.Start + rng.Pick(-200, 300)), static_cast<std::int32_t>(rng.Pick(1, 200)),
                                              rng.Next() % 100 < 85, static_cast<std::int32_t>(rng.Pick(0, 5) * 100), SourcePool[rng.Next() % 3]);
            candidates.push_back(candidate);
        }
        const ReceiptTotals oracle = OracleTotals(commitment, candidates);
        const ReceiptRecordResult result = RecordCommitment(state, commitment, candidates);
        Check(result.Ok() && result.DisplacedCount == oracle.Displaced, "random commitment records the oracle's alternatives");
        AddReceiptTotals(grand, oracle);
        AddReceiptTotals(perDay[ReceiptDayOf(commitment.Start)], oracle);
        AddReceiptTotals(perWeek[ReceiptWeekStartDay(commitment.Start)], oracle);
        if (result.Seq != 0)
        {
            const ReceiptEntry& entry = state.Entries.back();
            Check(entry.Seq == result.Seq && ReceiptEntryTotals(entry) == oracle, "the entry's totals equal the oracle's");
            for (const ReceiptDisplaced& line : entry.Displaced)
            {
                const bool shown = std::any_of(candidates.begin(), candidates.end(), [&](const ReceiptCandidate& c)
                                               { return c.Id == line.Id && c.Source == line.Source && c.KnownAt == line.KnownAt && c.Start == line.Start && c.DurationMinutes == line.DurationMinutes && c.Forecast == line.Forecast; });
                Check(shown && line.KnownAt <= entry.DecidedAt && line.Id != entry.ActivityId, "no line names an alternative that was not shown and known at the decision");
            }
            expected[entry.Seq] = entry;
        }
        if (rng.Next() % 9 == 0)
            FoldReceipts(state, decided);
        if (step == steps / 2)
        {
            const ReceiptParse reloaded = DeserialiseReceipts(SerialiseReceipts(state));
            Check(reloaded.Ok() && reloaded.State == state, "a mid-sequence save and load");
            state = reloaded.State;
        }
        Check(IsReceiptStateValid(state), "the state stays valid");
        Check(ReceiptGrandTotals(state) == grand, "the grand totals match the command log");
        for (const ReceiptEntry& kept : state.Entries)
            Check(expected.at(kept.Seq) == kept, "a recorded receipt is never changed");
    }
    for (const auto& [day, totals] : perDay)
    {
        if (day >= state.FoldedBeforeDay)
            Check(ReceiptDayQuery(state, day).Totals == totals, "a day's totals match the command log of that day");
    }
    for (const ReceiptWeek& week : state.Weeks)
        Check(perWeek.at(week.StartDay) == week.Totals, "a folded week matches the command log of that week");
    std::map<std::int64_t, int> entriesPerDay;
    for (const ReceiptEntry& entry : state.Entries)
        Check(++entriesPerDay[ReceiptDayOf(entry.Start)] <= ReceiptMaxEntriesPerDay, "a day never holds more than the cap");
    const std::string text = SerialiseReceipts(state);
    const ReceiptParse back = DeserialiseReceipts(text);
    Check(!text.empty() && back.Ok() && back.State == state && SerialiseReceipts(back.State) == text, "the final state round-trips");
}

void RandomCases()
{
    for (std::uint64_t seed = 1; seed <= 10000; ++seed)
        RunRandomSequence(seed);
}

void DeterminismCases()
{
    auto run = [](std::uint64_t seed)
    {
        Rng rng{seed};
        ReceiptState state;
        GameInstant decided = Mid(0);
        for (int i = 0; i < 100; ++i)
        {
            decided += rng.Pick(0, 90);
            std::vector<ReceiptCandidate> candidates;
            for (int c = 0; c < 6; ++c)
                candidates.push_back(Cand(AltPool[rng.Next() % 14], 0, decided + rng.Pick(0, 100), static_cast<std::int32_t>(rng.Pick(1, 120)), true, static_cast<std::int32_t>(rng.Pick(0, 3) * 100)));
            Check(RecordCommitment(state, Commit(ActPool[rng.Next() % 6], decided, decided + 10, 60), candidates).Ok(), "recorded");
        }
        return SerialiseReceipts(state);
    };
    Check(!run(5).empty() && run(5) == run(5) && run(5) != run(6), "the same sequence gives identical text");
}
} // namespace

void TestText() { TextCases(); }
void TestRandomSequences() { RandomCases(); }
void TestDeterminism() { DeterminismCases(); }
