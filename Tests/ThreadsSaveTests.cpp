// ROADMAP M3-06: the text form of the thread set. Part of PerfectWorldThreadTests.
#include "RandomStreams.h"
#include "ThreadsTestSupport.h"

#include <array>
#include <string_view>

namespace ThrTest
{
namespace
{
/** Several live threads mid-story and a few resolved ones, built through the real operations. */
Rig RichRig()
{
    Rig rig;
    rig.Calendar = TheWorld().Calendar;
    const GameInstant t0 = DayStart("2042-07-20") + 400;
    rig.Open("thread.borrowed_notes", 0, t0);
    rig.Open("thread.refused_invitations", 3, t0);
    rig.Open("thread.hidden_pickup", 6, t0);
    rig.Open("thread.evening_rota", 10, t0);
    rig.Open("thread.sports_day_rival", 14, t0);
    rig.Act(t0 + 10, 1, 0, RelationshipActKind::Help);
    rig.Log1(t0 + Days(2), "invitation_refused", {Person(3), Person(4)});
    rig.Log1(t0 + Days(3), "invitation_refused", {Person(3), Person(4)});
    rig.Log1(t0 + Days(4), "notes_returned", {Person(0), Person(1)});
    rig.Log1(t0 + Days(6), "obligation_missed", {Person(10), Person(11)});
    rig.Advance(t0 + Days(30));
    rig.Open("thread.canal_side_stall", 20, t0 + Days(30));
    rig.Advance(t0 + Days(45));
    return rig;
}
} // namespace

void TestSave()
{
    const Rig rig = RichRig();
    const ThreadSet& set = rig.Set;
    Expect(!set.Active.empty() && !set.Resolved.empty(), "the sample has live and resolved threads");
    const std::string text = SerialiseThreads(set);
    Expect(text.size() > 300 && text == SerialiseThreads(set), "serialising is deterministic");
    const std::optional<ThreadSet> loaded = ParseThreads(text);
    Expect(loaded.has_value() && *loaded == set && HashThreads(*loaded) == HashThreads(set), "the set survives a save");
    Expect(loaded && SerialiseThreads(*loaded) == text, "and re-serialises to the same bytes");
    Expect(loaded && ThreadSetMatchesTemplates(*loaded, TheWorld().Templates) && ThreadSetMatchesLog(*loaded, rig.Log),
           "and still matches the templates and the log");

    // A reloaded set carries on exactly like the original.
    ThreadSet original = set;
    ThreadSet restored = *loaded;
    EventLog log = rig.Log;
    const GameInstant later = set.Active.front().Now + Days(120);
    const ThreadFacts facts = Facts(&log, &rig.Book, rig.Calendar);
    const ThreadAdvanceResult a = ThreadAdvance(original, TheWorld().Templates, facts, later);
    const ThreadAdvanceResult b = ThreadAdvance(restored, TheWorld().Templates, facts, later);
    Expect(a.Ok() && b.Ok() && original == restored && a.Events == b.Events && !a.Events.empty(), "evolution after a reload is identical");

    const ThreadSet empty;
    Expect(SerialiseThreads(empty) == "pw_threads 1\nnext 1\nactive 0\nresolved 0\nend " + std::to_string(HashThreads(empty)) + "\n",
           "the empty set has a fixed form");
    const std::optional<ThreadSet> reloadedEmpty = ParseThreads(SerialiseThreads(empty));
    Expect(reloadedEmpty && *reloadedEmpty == empty, "and round-trips");

    for (std::size_t length = 0; length < text.size(); ++length)
        Expect(!ParseThreads(std::string_view(text).substr(0, length)), "every truncation is refused");

    const std::array<char, 4> replacements = {'0', '7', 'z', ' '};
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        for (const char replacement : replacements)
        {
            if (text[i] == replacement || text[i] == '\n')
                continue;
            std::string corrupt = text;
            corrupt[i] = replacement;
            Expect(!ParseThreads(corrupt), "every single-byte corruption is refused");
        }
    }
    Expect(!ParseThreads(text + "x") && !ParseThreads(text + "\n") && !ParseThreads("\n" + text), "extra bytes are refused");
    std::string crlf = text;
    crlf.replace(crlf.find('\n'), 1, "\r\n");
    Expect(!ParseThreads(crlf), "carriage returns are refused");
    std::string wrongVersion = text;
    wrongVersion.replace(text.find(" 1\n"), 3, " 2\n");
    Expect(!ParseThreads(wrongVersion), "a wrong version is refused");
    std::string padded = text;
    padded.replace(padded.find("active "), 7, "active 0");
    Expect(!ParseThreads(padded), "a wrong or padded count is refused");

    // Structural refusals that a matching hash would not catch: rebuild the text with a new, correct hash.
    const auto rehash = [](ThreadSet changed) { return SerialiseThreads(changed); };
    ThreadSet bad = set;
    bad.Active.push_back(bad.Active.front());
    Expect(!ParseThreads(rehash(bad)), "a repeated instance is refused");
    bad = set;
    std::swap(bad.Active.front(), bad.Active.back());
    Expect(bad.Active.size() < 2 || !ParseThreads(rehash(bad)), "instances out of order are refused");
    bad = set;
    bad.Active.front().People[1] = bad.Active.front().People[0];
    Expect(!ParseThreads(rehash(bad)), "one person in two roles is refused");
    bad = set;
    bad.Active.front().Stage = static_cast<std::int32_t>(ThreadMaxStages);
    Expect(!ParseThreads(rehash(bad)), "a stage out of range is refused");
    bad = set;
    bad.Active.front().Now = bad.Active.front().StageEnteredAt - 1;
    Expect(!ParseThreads(rehash(bad)), "time running backwards inside a thread is refused");
    bad = set;
    bad.Active.front().Counts.push_back(ThreadMaxEventCount + 1);
    Expect(!ParseThreads(rehash(bad)), "a counter above its cap is refused");
    bad = set;
    bad.Active.front().Fired = 1u << ThreadMaxTriggers;
    Expect(!ParseThreads(rehash(bad)), "a trigger bit that cannot exist is refused");
    bad = set;
    bad.Active.front().People[0] = "NPC 0";
    Expect(!ParseThreads(rehash(bad)), "a person who is not a valid identity is refused");
    bad = set;
    bad.NextInstance = 2;
    Expect(!ParseThreads(rehash(bad)), "an instance number at or above the next number is refused");
    bad = set;
    bad.Resolved.front().Id = bad.Active.front().Id;
    Expect(!ParseThreads(rehash(bad)), "an ID that is both live and resolved is refused");

    // A set that parses but does not fit the templates is caught by the second check.
    bad = set;
    bad.Active.front().Template = "thread.nothing";
    const std::optional<ThreadSet> foreign = ParseThreads(rehash(bad));
    Expect(foreign && !ThreadSetMatchesTemplates(*foreign, TheWorld().Templates), "an unknown template parses but does not match");
    bad = set;
    bad.Active.front().Counts.clear();
    const std::optional<ThreadSet> shortCounts = ParseThreads(rehash(bad));
    Expect(shortCounts && !ThreadSetMatchesTemplates(*shortCounts, TheWorld().Templates), "wrong counters parse but do not match");
}
} // namespace ThrTest
