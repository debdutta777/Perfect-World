// ROADMAP M2-05: the append-only event log. Linked into PerfectWorldIdentityTests; main() is in IdentityRegistryTests.cpp.
#include "IdentityTestSupport.h"

#include <algorithm>
#include <vector>

namespace
{
void IdTestEventBasics()
{
    EventLog log;
    const std::string alice = DeriveGeneratedIdentityId(IdTestSeed, IdTestOrigin(IdTestCohorts[0], 0));

    EventLogDraft flood;
    flood.At = 100;
    flood.Type = "district_flooded";
    EventLogAppendResult r = log.Append(flood);
    Check(r.Ok() && r.Seq == 1 && log.Size() == 1, "a root event is appended as Seq 1");

    EventLogDraft moved;
    moved.At = 100;
    moved.Type = "person_moved";
    moved.CauseSeq = 1;
    moved.Parties = {alice, "npc_class_teacher"};
    r = log.Append(moved);
    Check(r.Ok() && r.Seq == 2, "a caused event with parties is appended as Seq 2");
    Check(log.Find(2)->CauseSeq == 1 && log.Find(2)->Parties.size() == 2 && log.Find(2)->Parties[0] == alice, "the cause and parties are recorded in order");
    Check(log.Find(0) == nullptr && log.Find(3) == nullptr && log.Find(1)->Type == "district_flooded", "Find is exact");

    const EventLog before = log;
    EventLogDraft bad;
    bad.At = 200;
    bad.Type = "Bad Type";
    Check(log.Append(bad).Status == EventLogStatus::BadType, "a bad type is refused");
    bad.Type = "";
    Check(log.Append(bad).Status == EventLogStatus::BadType, "an empty type is refused");
    bad.Type = "ok";
    bad.At = 99;
    Check(log.Append(bad).Status == EventLogStatus::BadTime, "time may not go backwards");
    bad.At = -1;
    Check(log.Append(bad).Status == EventLogStatus::BadTime, "time may not be negative");
    bad.At = 200;
    bad.CauseSeq = 3;
    Check(log.Append(bad).Status == EventLogStatus::BadCause, "a cause must already exist");
    bad.CauseSeq = 2;
    bad.Parties = {alice, alice};
    Check(log.Append(bad).Status == EventLogStatus::BadParties, "a repeated party is refused");
    bad.Parties = {"Not An Id"};
    Check(log.Append(bad).Status == EventLogStatus::BadParties, "a malformed party is refused");
    bad.Parties.assign(EventLogMaxParties + 1, std::string());
    for (std::size_t i = 0; i < bad.Parties.size(); ++i)
        bad.Parties[i] = "npc_" + std::to_string(i);
    Check(log.Append(bad).Status == EventLogStatus::BadParties, "too many parties are refused");
    bad.Parties.pop_back();
    Check(log.Append(bad).Ok(), "the maximum number of parties is accepted");
    Check(log.Size() == 3 && std::equal(before.Entries().begin(), before.Entries().end(), log.Entries().begin()),
          "refusals add nothing and appends never alter earlier entries");
}

// A scripted history: the same script always builds the same log, so the hash at every step is repeatable.
EventLog IdTestScriptedLog(std::vector<std::uint64_t>* hashAfterEach)
{
    EventLog log;
    RandomStream rng(0x1234);
    std::vector<std::string> people;
    for (std::int64_t slot = 0; slot < 12; ++slot)
        people.push_back(DeriveGeneratedIdentityId(IdTestSeed, IdTestOrigin(IdTestCohorts[0], slot)));
    GameInstant now = 0;
    for (int i = 0; i < 300; ++i)
    {
        EventLogDraft draft;
        now += static_cast<GameInstant>(rng.NextBelow(5));
        draft.At = now;
        draft.Type = "event_kind_" + std::to_string(rng.NextBelow(6));
        draft.CauseSeq = rng.Chance(1, 2) ? rng.NextBelow(log.Size() + 1) : 0;
        const std::uint64_t partyCount = rng.NextBelow(4);
        for (std::uint64_t p = 0; p < partyCount; ++p)
        {
            const std::string& pick = people[static_cast<std::size_t>(rng.NextBelow(people.size()))];
            if (std::find(draft.Parties.begin(), draft.Parties.end(), pick) == draft.Parties.end())
                draft.Parties.push_back(pick);
        }
        Check(log.Append(draft).Ok(), "the scripted draft is valid");
        if (hashAfterEach)
            hashAfterEach->push_back(HashEventLog(log));
    }
    return log;
}

void IdTestEventReplayAndSaves()
{
    std::vector<std::uint64_t> first;
    std::vector<std::uint64_t> second;
    const EventLog logA = IdTestScriptedLog(&first);
    const EventLog logB = IdTestScriptedLog(&second);
    Check(logA == logB && first == second, "replaying the same script gives the same log and the same hash at every step");
    Check(HashEventLog(logA) == first.back(), "the log hash is the last running hash");
    for (std::size_t k = 0; k < first.size(); ++k)
        Check(HashEventLogPrefix(logA, k + 1) == first[k], "a prefix hash equals the hash the log had at that step");
    Check(HashEventLogPrefix(logA, 100000) == HashEventLog(logA), "a prefix beyond the end is the whole log");
    Check(HashEventLogPrefix(logA, 0) != HashEventLog(logA), "the empty prefix differs from the full log");
    for (std::size_t k = 1; k < first.size(); ++k)
        Check(first[k] != first[k - 1], "each append changes the hash");

    const std::string text = SerialiseEventLog(logA);
    const std::optional<EventLog> loaded = ParseEventLog(text);
    Check(loaded.has_value() && *loaded == logA, "the log survives a save unchanged");
    Check(SerialiseEventLog(*loaded) == text && HashEventLog(*loaded) == HashEventLog(logA), "re-serialising and re-hashing agree");

    // Save mid-history, load, continue: equals a straight run.
    EventLog straight;
    EventLog resumed;
    EventLogDraft a;
    a.At = 1;
    a.Type = "begin";
    straight.Append(a);
    resumed.Append(a);
    resumed = *ParseEventLog(SerialiseEventLog(resumed));
    EventLogDraft b;
    b.At = 2;
    b.Type = "next";
    b.CauseSeq = 1;
    straight.Append(b);
    resumed.Append(b);
    Check(straight == resumed && HashEventLog(straight) == HashEventLog(resumed), "continuing after a reload equals a straight run");

    EventLog empty;
    Check(ParseEventLog(SerialiseEventLog(empty)).has_value() && ParseEventLog(SerialiseEventLog(empty))->Size() == 0, "an empty log round-trips");
}

void IdTestEventRefusals()
{
    EventLog log;
    const std::string alice = DeriveGeneratedIdentityId(IdTestSeed, IdTestOrigin(IdTestCohorts[0], 0));
    EventLogDraft draft;
    draft.At = 10;
    draft.Type = "person_registered";
    draft.Parties = {alice};
    log.Append(draft);
    draft.At = 20;
    draft.Type = "person_met";
    draft.CauseSeq = 1;
    draft.Parties = {alice, "npc_teacher"};
    log.Append(draft);
    draft.Parties.clear();
    draft.At = 30;
    draft.Type = "year_ended";
    draft.CauseSeq = 2;
    log.Append(draft);

    const std::string text = SerialiseEventLog(log);
    const auto parse = [](std::string_view s) { return ParseEventLog(s).has_value(); };
    Check(parse(text), "the small log parses");
    IdTestEveryTruncationRefused(text, parse, "every truncation of a serialised log is refused");
    IdTestEveryCorruptionRefused(text, parse, "every single-byte corruption of a log is refused");
    Check(!parse(text + "\n") && !parse(text + "x\n"), "trailing data is refused");
    Check(!parse(IdTestReplace(text, "pw_events 1", "pw_events 9")), "an unknown version is refused");
    Check(!parse(IdTestReplace(text, "count 3", "count 2")) && !parse(IdTestReplace(text, "count 3", "count 4")), "a wrong count is refused");
    Check(!parse(IdTestReplace(text, "event 2 ", "event 3 ")), "out-of-order Seq is refused");
    Check(!parse(IdTestReplace(text, "event ", "evnt ")), "an unknown line keyword is refused");

    // Edits that break the structure (the hash no longer matches either, but each is also invalid on its own).
    const auto parseAfterEditing = [&](const std::string& from, const std::string& to) { return parse(IdTestReplace(text, from, to)); };
    Check(!parseAfterEditing("person_met 1 2", "person_met 1 3"), "a party count that disagrees with the line is refused");
    Check(!parseAfterEditing("year_ended 2", "year_ended 5"), "a cause that does not precede its effect is refused");
    Check(!parseAfterEditing(" 20 person_met", " 5 person_met"), "time going backwards is refused");
}

// Random good and bad drafts: whatever was accepted replays to the same hash and survives a save.
void IdTestEventRandomSequences()
{
    RandomStream rng(0xFEEDBEEF);
    for (int round = 0; round < 40; ++round)
    {
        EventLog log;
        std::vector<EventLogDraft> accepted;
        GameInstant now = 0;
        const int steps = 1 + static_cast<int>(rng.NextBelow(80));
        for (int step = 0; step < steps; ++step)
        {
            EventLogDraft draft;
            now += static_cast<GameInstant>(rng.NextBelow(4));
            draft.At = rng.Chance(1, 8) ? now - 5 : now; // sometimes backwards
            draft.Type = rng.Chance(1, 10) ? "BAD" : "kind_" + std::to_string(rng.NextBelow(5));
            draft.CauseSeq = rng.NextBelow(log.Size() + 3);
            if (rng.Chance(1, 2))
                draft.Parties.push_back("npc_" + std::to_string(rng.NextBelow(4)));
            if (rng.Chance(1, 6))
                draft.Parties.push_back(draft.Parties.empty() ? "npc_0" : draft.Parties[0]); // sometimes a repeat
            const std::size_t sizeBefore = log.Size();
            const EventLogAppendResult result = log.Append(draft);
            Check(result.Ok() == (log.Size() == sizeBefore + 1), "a draft is appended exactly when it is accepted");
            if (result.Ok())
                accepted.push_back(draft);
        }
        EventLog replay;
        for (const EventLogDraft& draft : accepted)
            Check(replay.Append(draft).Ok(), "an accepted draft is accepted again on replay");
        Check(replay == log && HashEventLog(replay) == HashEventLog(log), "the accepted drafts replay to the same hash");
        const auto loaded = ParseEventLog(SerialiseEventLog(log));
        Check(loaded && *loaded == log, "a random log round-trips unchanged");
    }
}
} // namespace

void EventLogTestsRun()
{
    IdTestEventBasics();
    IdTestEventReplayAndSaves();
    IdTestEventRefusals();
    IdTestEventRandomSequences();
}
