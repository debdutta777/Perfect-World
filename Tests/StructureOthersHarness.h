#pragma once

// The properties every structure's runner shares (ROADMAP M2-09, "The M2-08 properties
// hold for each structure"), written once over a traits type so the explain,
// investigate and coordinate runners are held to exactly the same rules the model
// structure's tests check: every case can be completed, quote equals charge, selection
// is deterministic and never repeats, mastery and routine abbreviation, the save text
// and random sequences. A traits type supplies the catalogue, the state and the
// runner's functions; the structure's own rules are in its own test file.
#include "StructureOthersTestSupport.h"

#include <map>
#include <set>

namespace Harness
{
using namespace PerfectWorld;

/** Quotes, applies and checks the charge equals the quote shown before. */
template <class T>
StructureStepResult<typename T::Outcome> Do(typename T::State& state, const typename T::Action& action,
                                            const typename T::Catalogue& cat = T::Cat())
{
    const typename T::State before = state;
    const StructureQuote quote = T::Quote(state, cat, action);
    Check(state == before, "quoting changes nothing");
    Check(quote.Ok(), "the action is legal");
    const std::int32_t left = state.Exposure.UnitsLeft;
    const StructureStepResult<typename T::Outcome> step = T::Apply(state, cat, action);
    Check(step.Ok() && step.Charged == quote.Units, "the charge equals the quote shown before");
    Check(step.Finished || state.Exposure.UnitsLeft == left - quote.Units, "the units fall by exactly the quote");
    Check(step.Finished || state.Exposure.UnitsLeft == quote.UnitsAfter, "UnitsAfter is what remains");
    Check(T::Valid(state), "the state stays valid");
    return step;
}

/** A fresh state with `def` begun for a character of `skill`. */
template <class T>
typename T::State Started(const typename T::CaseDef& def, int goal, std::int32_t skill, const typename T::Catalogue& cat = T::Cat())
{
    typename T::State state;
    const StructureBeginResult begin = T::Begin(state, cat, def.Id, goal, skill);
    Check(begin.Ok() && begin.Units == T::Budget(def) && begin.MethodKnown == (skill >= def.MethodKnownAtMilli), "an exposure begins with its budget");
    return state;
}

/** Plays the standard policy to the end of the running exposure. */
template <class T>
typename T::Outcome PlayStandard(typename T::State& state, const typename T::Catalogue& cat = T::Cat())
{
    for (int i = 0; i < 12; ++i)
    {
        const typename T::CaseDef* def = cat.FindCase(state.Exposure.CaseId);
        Check(def != nullptr, "the running case exists");
        const StructureStepResult<typename T::Outcome> step = Do<T>(state, T::Standard(*def, state.Exposure), cat);
        if (step.Finished)
        {
            Check(!state.Exposure.Active(), "a finished exposure leaves the runner idle");
            return step.Outcome;
        }
    }
    Check(false, "the standard policy finishes within twelve steps");
    return {};
}

template <class T>
const std::vector<typename T::CaseDef>& Cases()
{
    return T::Cat().Families.front().Cases;
}

/** Every case can be completed Sound by the standard policy, whatever the goal and the skill. */
template <class T>
void EveryCaseCompletable()
{
    for (const typename T::CaseDef& def : Cases<T>())
    {
        for (int goal = 0; goal < T::Goals; ++goal)
        {
            for (const std::int32_t skill : {NoSkill, def.MethodKnownAtMilli - 1 < 0 ? 0 : def.MethodKnownAtMilli - 1, def.MethodKnownAtMilli, FullSkill})
            {
                typename T::State state = Started<T>(def, goal, skill);
                const typename T::Outcome outcome = PlayStandard<T>(state);
                Check(outcome.Grade == StructureGrade::Sound && outcome.Failures == 0, "every case can be completed soundly");
                Check(outcome.Foreground && outcome.LearningPermille == StructureLearningPermille, "foreground learning is the normal value");
                Check(outcome.UnitsSpent >= 0 && outcome.UnitsSpent <= T::Budget(def), "within the budget");
                Check(outcome.CaseId == def.Id && outcome.FamilyId == T::Cat().Families.front().Id, "the outcome names its case");
                const StructureFamilyRecord* record = state.FindFamily(outcome.FamilyId);
                Check(record != nullptr && record->Exposures == 1 && record->SoundExposures == 1, "one sound exposure is recorded");
            }
        }
    }
}

/** Presents the next selected case, plays it with the standard policy and returns its ID. */
template <class T>
std::string PlayNext(typename T::State& state, const typename T::Catalogue& cat, std::uint64_t seed, int index)
{
    const std::string family = cat.Families.front().Id;
    const StructureSelection selection = T::Select(cat, state, family, seed, static_cast<GameInstant>(index) * 60);
    Check(selection.Ok() && selection.Candidates >= 1, "a case is selected");
    if (const StructureFamilyRecord* record = state.FindFamily(family))
    {
        const std::size_t window = std::min(StructureRecentKept, cat.Families.front().Cases.size() - 1);
        for (std::size_t i = 0; i < std::min(window, record->Recent.size()); ++i)
            Check(selection.CaseId != record->Recent[record->Recent.size() - 1 - i], "a recent case is never offered again");
    }
    Check(T::Begin(state, cat, selection.CaseId, index % T::Goals, index % 2 == 0 ? NoSkill : FullSkill).Ok(), "the case begins");
    (void)PlayStandard<T>(state, cat);
    return selection.CaseId;
}

template <class T>
std::vector<std::string> PlaySequence(std::uint64_t seed, int count)
{
    typename T::State state;
    std::vector<std::string> order;
    for (int i = 0; i < count; ++i)
        order.push_back(PlayNext<T>(state, T::Cat(), seed, i));
    return order;
}

template <class T>
void Selection()
{
    const std::string family = T::Cat().Families.front().Id;
    const std::size_t size = Cases<T>().size();
    const typename T::State fresh;
    const StructureSelection a = T::Select(T::Cat(), fresh, family, 42, 100);
    const StructureSelection b = T::Select(T::Cat(), fresh, family, 42, 100);
    Check(a.Ok() && a.CaseId == b.CaseId && a.FromUnseen && a.Candidates == size, "selection is a pure function of its inputs");
    Check(fresh == typename T::State{}, "selecting changes nothing");
    Check(T::Select(T::Cat(), fresh, "nope.family", 42, 100).Error == StructureError::UnknownFamily, "unknown family");

    const std::vector<std::string> first = PlaySequence<T>(42, 40);
    Check(first == PlaySequence<T>(42, 40), "the same seed gives the same sequence");
    const std::set<std::string> opening(first.begin(), first.begin() + static_cast<std::ptrdiff_t>(size));
    Check(opening.size() == size, "the first exposures meet every case once before any repeats");

    typename T::State state;
    std::map<std::string, int> counts;
    for (int i = 0; i < 400; ++i)
        ++counts[PlayNext<T>(state, T::Cat(), 7, i)];
    Check(counts.size() == size, "every case is offered");
    for (const auto& [id, count] : counts)
        Check(count >= 400 / static_cast<int>(size) / 2 && count <= 400 / static_cast<int>(size) * 3 / 2, "no case is starved or dominates");
    Check(!T::Select(T::Cat(), state, family, 7, 99999).FromUnseen, "once every case is seen the pool is the eligible ones");

    // A small family leaves at least one choice: two cases alternate, one case repeats.
    typename T::Catalogue small = T::Cat();
    small.Families.front().Cases.resize(2);
    typename T::State pair;
    std::string previous;
    std::set<std::string> seen;
    for (int i = 0; i < 12; ++i)
    {
        const std::string id = PlayNext<T>(pair, small, 5, i);
        Check(id != previous, "two cases alternate");
        seen.insert(id);
        previous = id;
    }
    Check(seen.size() == 2, "both cases of the pair appear");
    small.Families.front().Cases.resize(1);
    typename T::State single;
    for (int i = 0; i < 3; ++i)
        Check(PlayNext<T>(single, small, 5, i) == small.Families.front().Cases.front().Id, "a family of one case repeats it");
}

/** Mastery needs sound play across different cases; routine then resolves at full value and equals the standard policy. */
template <class T>
void MasteryAndRoutine()
{
    const std::string family = T::Cat().Families.front().Id;
    const typename T::CaseDef& a = Cases<T>()[0];
    const typename T::CaseDef& b = Cases<T>()[1];
    const typename T::CaseDef& c = Cases<T>()[2];
    typename T::State state;
    const auto play = [&](const typename T::CaseDef& def) {
        Check(T::Begin(state, T::Cat(), def.Id, 0, FullSkill).Ok(), "begins");
        return PlayStandard<T>(state);
    };

    for (int i = 0; i < 4; ++i)
        Check(!play(a).BecameSecure, "not secure yet");
    const StructureFamilyRecord* record = state.FindFamily(family);
    Check(record->SoundExposures == 4 && record->Seen.size() == 1 && !record->Secure, "four sound exposures on one case are not mastery");
    const typename T::State unsecure = state;
    Check(T::Routine(state, T::Cat(), family).Error == StructureError::NotSecure && state == unsecure, "routine is refused until the family is secure");
    Check(!play(b).BecameSecure && !state.FindFamily(family)->Secure, "two cases are not enough");
    Check(play(c).BecameSecure && state.FindFamily(family)->Secure, "the third case met with four sound exposures makes it secure");

    // A failed foreground exposure counts as an exposure but not a sound one, and mastery stays.
    const typename T::Outcome failed = T::FailOnce(state, a);
    Check(failed.Grade != StructureGrade::Sound && !failed.BecameSecure, "a failed play is not sound");
    record = state.FindFamily(family);
    Check(record->Exposures == 7 && record->SoundExposures == 6 && record->Secure, "a failure adds an exposure, no sound one, and mastery stays");

    // Routine: normal learning, no manual play, no new case, equal to the standard policy's result.
    const typename T::State before = state;
    const StructureRoutineResult routine = T::Routine(state, T::Cat(), family);
    Check(routine.Ok() && routine.Outcome.Grade == StructureGrade::Sound && routine.Outcome.Failures == 0, "routine resolves soundly");
    Check(!routine.Outcome.Foreground && routine.Outcome.CaseId.empty() && routine.Outcome.UnitsSpent == 0 && !routine.Outcome.BecameSecure,
          "routine plays nothing and presents no case");
    Check(routine.Outcome.LearningPermille == StructureLearningPermille, "routine earns the full normal value");
    const StructureFamilyRecord& after = *state.FindFamily(family);
    const StructureFamilyRecord& was = *before.FindFamily(family);
    Check(after.Seen == was.Seen && after.Recent == was.Recent, "routine leaves the cases seen and recent alone");
    Check(after.Exposures == was.Exposures + 1 && after.SoundExposures == was.SoundExposures && after.Secure, "it is an exposure but not a sound one");
    for (const typename T::CaseDef& def : Cases<T>())
    {
        for (int goal = 0; goal < T::Goals; ++goal)
        {
            typename T::State foreground = Started<T>(def, goal, FullSkill);
            const typename T::Outcome played = PlayStandard<T>(foreground);
            Check(played.Grade == routine.Outcome.Grade && played.Failures == routine.Outcome.Failures &&
                      played.LearningPermille == routine.Outcome.LearningPermille && played.Foreground && !routine.Outcome.Foreground,
                  "routine equals foreground play under the standard policy in everything but having been played");
        }
    }
    for (int i = 0; i < 100; ++i)
        Check(T::Routine(state, T::Cat(), family).Ok(), "routine can be used every period");
    Check(state.FindFamily(family)->SoundExposures == was.SoundExposures && state.FindFamily(family)->Exposures == was.Exposures + 101,
          "routine never counts as sound practice and never pays more than normal");
    Check(T::Valid(state), "the state stays valid");

    // Not while an exposure runs; an unknown family is refused.
    Check(T::Begin(state, T::Cat(), b.Id, 0, FullSkill).Ok(), "begins");
    const typename T::State running = state;
    Check(T::Routine(state, T::Cat(), family).Error == StructureError::Busy && state == running, "no routine during an exposure");
    Check(T::Cancel(state) == StructureError::None, "abandoned");
    Check(T::Routine(state, T::Cat(), "nope.family").Error == StructureError::UnknownFamily, "routine: unknown family");

    // Mastery arrives with the fourth sound exposure when four different cases were met by selection.
    typename T::State chosen;
    for (int i = 0; i < 4; ++i)
    {
        (void)PlayNext<T>(chosen, T::Cat(), 11, i);
        Check(chosen.FindFamily(family)->Secure == (i == 3), "secure exactly at the fourth sound exposure");
    }
}

inline std::string Mutated(const std::string& text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "the mutation target exists in the text");
    std::string out = text;
    if (at != std::string::npos)
        out.replace(at, from.size(), to);
    return out;
}

inline std::string Joined(const std::vector<std::string>& parts)
{
    std::string text;
    for (std::size_t i = 0; i < parts.size(); ++i)
        text += (i > 0 ? "," : "") + parts[i];
    return text;
}

template <class T>
void CheckSaveContinues(const typename T::State& state, const std::vector<typename T::Action>& rest)
{
    const std::string text = T::Serialise(state);
    Check(!text.empty(), "a valid state serialises");
    const auto parsed = T::Parse(text);
    Check(parsed.Ok() && parsed.State == state && T::Serialise(parsed.State) == text, "the text restores the same state");
    typename T::State original = state;
    typename T::State restored = parsed.State;
    for (const typename T::Action& action : rest)
    {
        const auto x = T::Apply(original, T::Cat(), action);
        const auto y = T::Apply(restored, T::Cat(), action);
        Check(x.Ok() && y.Ok() && x.Charged == y.Charged && x.Finished == y.Finished && x.Outcome == y.Outcome, "a restored exposure plays on identically");
        Check(original == restored, "and ends in the same state");
    }
    Check(!original.Exposure.Active() && T::Serialise(original) == T::Serialise(restored), "the exposure finished once");
    Check(T::Apply(restored, T::Cat(), rest.back()).Error == StructureError::NotActive, "it cannot finish a second time");
}

template <class T>
void CheckEveryTruncationRefused(const std::string& text)
{
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!T::Parse(std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");
    Check(!T::Parse(text + "x").Ok() && !T::Parse(text + "\n").Ok() && !T::Parse(text + text).Ok(), "trailing text is refused");
}

/** The save text: stable, restores mid-exposure, refuses every truncation and every edit. */
template <class T>
void Text()
{
    const typename T::State idle;
    const std::string idleText = T::Serialise(idle);
    Check(idleText == std::string(T::Header) + "\nfamilies=0\nexposure=-\n" + T::IdleTail + "end\n", "the idle text is stable");
    Check(T::Parse(idleText).Ok() && T::Parse(idleText).State == idle, "the idle text restores");

    // One finished exposure, then another begun unfamiliar: a save at every step of it.
    const typename T::CaseDef& first = Cases<T>()[0];
    const typename T::CaseDef& second = Cases<T>()[1];
    typename T::State state = Started<T>(first, 0, FullSkill);
    (void)PlayStandard<T>(state);
    Check(T::Begin(state, T::Cat(), second.Id, 1 % T::Goals, NoSkill).Ok(), "the second case begins");
    std::vector<typename T::State> states;
    std::vector<typename T::Action> script;
    std::vector<std::string> texts;
    typename T::State walk = state;
    while (walk.Exposure.Active())
    {
        states.push_back(walk);
        texts.push_back(T::Serialise(walk));
        script.push_back(T::Standard(*T::Cat().FindCase(walk.Exposure.CaseId), walk.Exposure));
        (void)Do<T>(walk, script.back());
    }
    for (std::size_t i = 0; i < states.size(); ++i)
        CheckSaveContinues<T>(states[i], std::vector<typename T::Action>(script.begin() + static_cast<std::ptrdiff_t>(i), script.end()));

    // A state with two families, and every truncation of every text.
    typename T::State two = state;
    StructureFamilyRecord other;
    other.FamilyId = "a_other.family";
    other.Seen = {"a_other.case"};
    other.Recent = {"a_other.case"};
    other.Exposures = 1;
    two.Families.insert(two.Families.begin(), other);
    Check(T::Valid(two) && T::Parse(T::Serialise(two)).State == two, "two families round-trip");
    texts.push_back(T::Serialise(two));
    texts.push_back(idleText);
    for (const std::string& text : texts)
        CheckEveryTruncationRefused<T>(text);

    // Malformed and inconsistent texts are refused, never half-read.
    const std::string base = T::Serialise(state);
    const std::string family = state.Families.front().FamilyId;
    const StructureFamilyRecord& record = state.Families.front();
    const std::string head = "family=" + family + ",";
    const std::string counts = std::to_string(record.Exposures) + "," + std::to_string(record.SoundExposures) + "," + (record.Secure ? "1" : "0");
    const std::string exposureLine = "exposure=";
    const std::size_t at = base.find(exposureLine);
    const std::string value = base.substr(at + exposureLine.size(), base.find('\n', at) - at - exposureLine.size());
    std::vector<std::string> fields;
    for (std::size_t start = 0;;)
    {
        const std::size_t comma = value.find(',', start);
        fields.push_back(value.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
    std::vector<std::pair<std::string, std::string>> bad{
        {T::Header, std::string(T::Header).substr(0, std::string(T::Header).size() - 1) + "2"}, {"families=1", "families=2"}, {"families=1", "families=01"},
        {"families=1", "families=65"}, {head + counts, head + "0" + counts}, {head + counts, head + "1,2,0"}, {head + counts, head + "1,1,1"},
        {head + counts, head + "1,1,2"}, {head + counts, "family=Bad_Family,1,1,0"}, {head + counts, head + "1,1"}, {head + counts, head + "-1,1,0"},
        {"recent=" + Joined(record.Recent), "recent=" + Joined(record.Recent) + ",b_other.case"}, {"recent=" + Joined(record.Recent), "recent=-"},
        {"recent=" + Joined(record.Recent), "recent=" + record.Recent.front() + ",," + record.Recent.back()},
        {"seen=" + Joined(record.Seen), "seen=" + record.Seen.back() + "," + record.Seen.front()}, {"seen=" + Joined(record.Seen), "seen=" + record.Seen.front() + "," + record.Seen.front()},
        {"end\n", "fin\n"}, {"end\n", "end\nend\n"}, {"\n", "\r\n"}};
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        std::vector<std::string> changed = fields;
        changed[i] = i < 2 ? "Bad Id" : i == 2 ? "bogus" : i == T::UnitsField ? "0" : i == T::UnitsField + 1 ? "99" : "7";
        bad.push_back({exposureLine + value, exposureLine + Joined(changed)});
        std::vector<std::string> shorter = fields;
        shorter.pop_back();
        bad.push_back({exposureLine + value, exposureLine + Joined(shorter)});
    }
    std::vector<std::string> padded = fields;
    padded.push_back("1");
    bad.push_back({exposureLine + value, exposureLine + Joined(padded)});
    std::vector<std::string> zero = fields;
    zero[T::UnitsField] = "0" + zero[T::UnitsField];
    bad.push_back({exposureLine + value, exposureLine + Joined(zero)});
    for (const auto& [from, to] : bad)
        Check(!T::Parse(Mutated(base, from, to)).Ok(), "a malformed or inconsistent text is refused");
    Check(T::Parse(base).Ok(), "the unmutated text parses");

    // An inconsistent state does not serialise at all.
    typename T::State broken = state;
    broken.Families.front().Secure = true;
    Check(!T::Valid(broken) && T::Serialise(broken).empty(), "an inconsistent state serialises to nothing");
}

/** Random play: quote and application agree, a refusal changes nothing, every state round-trips. */
template <class T>
std::string RandomRun(std::uint64_t seed, int steps, int& finished, int& refused, int& becameSecure)
{
    OthersRng rng{seed};
    const std::string family = T::Cat().Families.front().Id;
    const std::vector<typename T::CaseDef>& cases = Cases<T>();
    typename T::State state;
    std::string transcript;
    std::int32_t charged = 0;
    bool wasSecure = false;
    std::uint32_t lastExposures = 0;
    for (int step = 0; step < steps; ++step)
    {
        const typename T::State before = state;
        StructureError error = StructureError::None;
        if (!state.Exposure.Active())
        {
            charged = 0;
            if (rng.Below(100) < 15)
                error = T::Routine(state, T::Cat(), rng.Below(10) == 0 ? "nope.family" : family).Error;
            else
            {
                std::string id = T::Select(T::Cat(), state, family, 99, step).CaseId;
                if (rng.Below(10) < 3)
                    id = cases[rng.Below(cases.size())].Id;
                const std::int32_t skills[] = {-5, 0, 7000, 20000, 110000, 125000, 140000, FullSkill};
                error = T::Begin(state, T::Cat(), id, static_cast<int>(rng.Below(static_cast<std::size_t>(T::Goals) + 1)), skills[rng.Below(8)]).Error;
            }
        }
        else if (rng.Below(100) < 8)
            error = rng.Below(2) == 0 ? T::Cancel(state) : T::Begin(state, T::Cat(), cases[0].Id, 0, 0).Error;
        else
        {
            const typename T::CaseDef& def = *T::Cat().FindCase(state.Exposure.CaseId);
            const typename T::Action action = rng.Below(100) < 40 ? T::Standard(def, state.Exposure) : T::Random(rng, def, state);
            const StructureQuote quote = T::Quote(state, T::Cat(), action);
            const StructureStepResult<typename T::Outcome> result = T::Apply(state, T::Cat(), action);
            Check(result.Error == (T::PriceOnly(action) && quote.Ok() ? StructureError::BadChoice : quote.Error), "the quote and the application agree on legality");
            error = result.Error;
            if (result.Ok())
            {
                Check(result.Charged == quote.Units, "the charge is the quote");
                charged += result.Charged;
                if (result.Finished)
                {
                    ++finished;
                    const typename T::Outcome& o = result.Outcome;
                    Check(o.Grade == StructureGrade::Sound ? o.Failures == 0 : o.Failures != 0, "the grade follows the failures");
                    Check(o.LearningPermille == StructureLearningPermille && o.Foreground && o.UnitsSpent == charged && o.UnitsSpent <= StructureWorkUnits,
                          "learning is normal and the units add up");
                    becameSecure += o.BecameSecure ? 1 : 0;
                }
                else
                    Check(state.Exposure.UnitsTotal - state.Exposure.UnitsLeft == charged, "units spent equal the charges");
            }
        }
        transcript += std::to_string(static_cast<int>(error)) + ",";
        if (error != StructureError::None)
        {
            ++refused;
            Check(state == before, "a refusal changes nothing");
        }
        Check(T::Valid(state), "the state stays valid through random play");
        const std::string text = T::Serialise(state);
        const auto parsed = T::Parse(text);
        Check(!text.empty() && parsed.Ok() && parsed.State == state, "every state round-trips through its text");
        if (const StructureFamilyRecord* record = state.FindFamily(family))
        {
            Check(!wasSecure || record->Secure, "mastery is never lost");
            Check(record->Exposures >= lastExposures, "exposures never fall");
            wasSecure = record->Secure;
            lastExposures = record->Exposures;
        }
    }
    return transcript + T::Serialise(state);
}

template <class T>
void RandomSequences()
{
    int finished = 0, refused = 0, secure = 0;
    const std::string first = RandomRun<T>(1234, 6000, finished, refused, secure);
    Check(finished > 150 && refused > 500 && secure == 1, "the random run finishes, refuses and masters the family once");
    int f2 = 0, r2 = 0, s2 = 0;
    Check(RandomRun<T>(1234, 6000, f2, r2, s2) == first && f2 == finished, "the same seed gives the same run");
    Check(RandomRun<T>(4321, 6000, f2, r2, s2) != first, "another seed gives another run");
}

/** Every generic property of one structure. */
template <class T>
void AllGeneric()
{
    Check(T::Cat().Families.size() == 1, "one family is authored");
    Check(Cases<T>().size() >= 3 && static_cast<std::int32_t>(Cases<T>().size()) >= T::Cat().Families.front().MinCases, "the family has at least three cases");
    for (std::size_t i = 1; i < Cases<T>().size(); ++i)
        Check(Cases<T>()[i - 1].Id < Cases<T>()[i].Id, "cases are sorted by ID");
    EveryCaseCompletable<T>();
    Selection<T>();
    MasteryAndRoutine<T>();
    Text<T>();
    RandomSequences<T>();
}
} // namespace Harness
