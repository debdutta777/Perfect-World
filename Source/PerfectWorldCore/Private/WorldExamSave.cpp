#include "WorldExam.h"

#include "OpportunityReceipts.h"
#include "WorldExamFollowup.h"
#include "WorldExamSittings.h"
#include "WorldSaveInternal.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <utility>

// P4-02: the pw.examination/1 envelope (format in WorldExam.h), the content-free rules the examination keeps inside its world,
// and its place in the world save (schema 13, after "awakening"). Decoding parses into a temporary, re-encodes it and demands
// the identical text (one canonical spelling), then applies the world rules; the reader never trusts a stored score, and the
// replay of choice provenance against runner content is ExamStateMatchesRunners at the WorldSim boundary.
// P4-09: earlier annual sittings ride in the same section as pw.examination/2 (only when there are any), so worlds with one
// sitting keep P4-02's exact bytes and hash; the per-session rules live in WorldExamSittings.cpp.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view P402Header = "pw.examination/1";
constexpr std::string_view P409Header = "pw.examination/2";
constexpr std::string_view P503Header = "pw.examination/3";

bool P402Id(std::string_view id) { return IsReceiptIdWellFormed(id); }

std::string P402Joined(const std::vector<std::string>& ids, bool& ok)
{
    std::string out;
    for (const std::string& id : ids)
    {
        ok = ok && P402Id(id);
        if (!out.empty())
            out += ';';
        out += id;
    }
    return out;
}

template <typename T>
std::string P402Num(T value) { return std::to_string(value); }

/** One session's lines, each ending "\n": participation, preparation, runner, attempts, attempt*, active. */
std::string P402Body(const TowerSessionState& s, bool& ok)
{
    ok = ok && static_cast<unsigned>(s.Participation) <= static_cast<unsigned>(TowerParticipation::Declined);
    std::string out = "participation=" + P402Num(static_cast<unsigned>(s.Participation));
    out += "\npreparation=";
    for (const std::int32_t value : s.Preparation.Preparation)
        out += P402Num(value) + ',';
    out += P402Num(s.Preparation.Composure) + ',' + P402Num(s.Preparation.RouteDelayMinutes);
    out += "\nrunner=" + P402Num(s.NextRunner) + ',' + P402Num(s.Runner);
    out += "\nattempts=" + P402Num(s.Attempts.size());
    for (const TowerSessionAttempt& a : s.Attempts)
    {
        ok = ok && P402Id(a.RouteId) && P402Id(a.ScenarioId) && !a.ChoiceIds.empty();
        out += "\nattempt=" + a.RouteId + ',' + P402Num(static_cast<unsigned>(a.Method)) + ',' + P402Num(a.DisciplineMilli) + ',' +
               P402Num(a.CommonJudgmentMilli) + ',' + P402Num(a.MissionMinutes) + ',' + (a.Defeated ? "1" : "0") + ',' +
               a.ScenarioId + ',' + P402Joined(a.ChoiceIds, ok);
    }
    if (!s.Active)
        out += "\nactive=none";
    else
    {
        const TowerRunnerRun& r = *s.Active;
        ok = ok && P402Id(r.ScenarioId) && P402Id(r.RouteId);
        out += "\nactive=" + r.ScenarioId + ',' + r.RouteId + ',' + P402Num(static_cast<unsigned>(r.Method)) + ',' + P402Num(r.Beat) +
               ',' + P402Num(r.Conditions.Reserve) + ',' + P402Num(r.Conditions.MinuteDiscount) + ',' + P402Num(r.DisciplineSum) +
               ',' + P402Num(r.JudgmentSum) + ',' + P402Num(r.MissionMinutes) + ',' + P402Joined(r.Choices, ok);
    }
    return out + '\n';
}

/**
 * P4-09: pw.examination/1 (header, the current body, end) while there is no earlier sitting, byte for byte P4-02's format;
 * otherwise pw.examination/2, "sittings=<n>", the n earlier bodies in sitting order, then the current body and end.
 */
std::string P402Encode(const std::vector<TowerSessionState>& earlier, const TowerSessionState& s,
    const std::vector<P503FollowupRecord>& followups)
{
    bool ok = earlier.size() <= P409MaxSitting && followups.size() <= P503MaxFollowupRecords;
    std::string out;
    if (!followups.empty())
    {
        // P5-03: /3 always names its earlier sittings, possibly none, so one header covers every follow-up world.
        out = std::string(P503Header) + "\nsittings=" + P402Num(earlier.size()) + '\n';
        for (const TowerSessionState& e : earlier)
            out += P402Body(e, ok);
    }
    else if (earlier.empty())
        out = std::string(P402Header) + '\n';
    else
    {
        out = std::string(P409Header) + "\nsittings=" + P402Num(earlier.size()) + '\n';
        for (const TowerSessionState& e : earlier)
            out += P402Body(e, ok);
    }
    out += P402Body(s, ok);
    if (!followups.empty())
    {
        out += "followups=" + P402Num(followups.size()) + '\n';
        for (const P503FollowupRecord& r : followups)
        {
            ok = ok && P402Id(r.AppointmentId) && r.Sessions.size() <= P503MaxFollowupSessions;
            out += "followup=" + P402Num(r.Sitting) + ',' + r.AppointmentId + ',' + P402Num(r.Revision) + ',' +
                   P402Num(r.Sessions.size()) + '\n';
            for (const P503FollowupSession& session : r.Sessions)
            {
                ok = ok && session.At >= 0 && static_cast<unsigned>(session.State) <= static_cast<unsigned>(P503SessionState::Released);
                out += "session=" + P402Num(session.Generation) + ',' + P402Num(session.At) + ',' +
                       P402Num(static_cast<unsigned>(session.State)) + '\n';
            }
            if (!r.Completion)
                out += "completion=none\n";
            else
            {
                const P503FollowupCompletion& c = *r.Completion;
                ok = ok && P402Id(c.TaskId) && P402Id(c.ChoiceId) && c.CompletedAt >= 0 && !c.Records.empty();
                out += "completion=" + P402Num(c.Generation) + ',' + c.TaskId + ',' + c.ChoiceId + ',' + P402Num(c.CompletedAt) +
                       ',' + P402Joined(c.Records, ok) + '\n';
            }
        }
    }
    out += "end\n";
    return ok ? out : std::string{};
}

std::vector<std::string_view> P402Split(std::string_view text, char separator)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (;;)
    {
        const std::size_t end = text.find(separator, start);
        parts.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos)
            return parts;
        start = end + 1;
    }
}

/** Canonical non-negative decimal within [0, high]; no sign, no leading zero. Re-encoding catches any other spelling too. */
template <typename T>
bool P402Read(std::string_view text, T& out, std::uint64_t high = std::numeric_limits<T>::max())
{
    if (text.empty() || text.size() > 20 || (text.size() > 1 && text.front() == '0'))
        return false;
    std::uint64_t value = 0;
    for (const char c : text)
    {
        if (c < '0' || c > '9')
            return false;
        const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
            return false;
        value = value * 10 + digit;
    }
    if (value > high)
        return false;
    out = static_cast<T>(value);
    return true;
}

bool P402Value(std::string_view line, std::string_view key, std::string_view& value)
{
    if (line.size() <= key.size() || line.substr(0, key.size()) != key || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

bool P402Choices(std::string_view field, std::vector<std::string>& out)
{
    out.clear();
    if (field.empty())
        return true;
    for (const std::string_view id : P402Split(field, ';'))
    {
        if (!P402Id(id))
            return false;
        out.emplace_back(id);
    }
    return true;
}

bool P402Method(std::string_view field, TowerMethod& out)
{
    unsigned value = 0;
    if (!P402Read(field, value, TowerMethodCount - 1))
        return false;
    out = static_cast<TowerMethod>(value);
    return true;
}

bool P402DecodeBody(const std::vector<std::string_view>& lines, std::size_t& at, TowerSessionState& s)
{
    std::string_view v;
    const auto next = [&](std::string_view key) { return at < lines.size() && P402Value(lines[at++], key, v); };
    unsigned participation = 0;
    if (!next("participation") || !P402Read(v, participation, static_cast<unsigned>(TowerParticipation::Declined)))
        return false;
    s.Participation = static_cast<TowerParticipation>(participation);
    if (!next("preparation"))
        return false;
    const auto prep = P402Split(v, ',');
    if (prep.size() != TowerMethodCount + 2)
        return false;
    for (std::size_t i = 0; i < TowerMethodCount; ++i)
        if (!P402Read(prep[i], s.Preparation.Preparation[i]))
            return false;
    if (!P402Read(prep[TowerMethodCount], s.Preparation.Composure) || !P402Read(prep[TowerMethodCount + 1], s.Preparation.RouteDelayMinutes))
        return false;
    if (!next("runner"))
        return false;
    const auto runner = P402Split(v, ',');
    std::size_t count = 0;
    if (runner.size() != 2 || !P402Read(runner[0], s.NextRunner) || !P402Read(runner[1], s.Runner) || !next("attempts") ||
        !P402Read(v, count, static_cast<std::uint64_t>(TowerFloorCount)))
        return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!next("attempt"))
            return false;
        const auto f = P402Split(v, ',');
        TowerSessionAttempt a;
        unsigned defeated = 0;
        if (f.size() != 8 || !P402Id(f[0]) || !P402Method(f[1], a.Method) || !P402Read(f[2], a.DisciplineMilli) ||
            !P402Read(f[3], a.CommonJudgmentMilli) || !P402Read(f[4], a.MissionMinutes) || !P402Read(f[5], defeated, 1) ||
            !P402Id(f[6]) || !P402Choices(f[7], a.ChoiceIds))
            return false;
        a.RouteId = std::string(f[0]);
        a.Defeated = defeated == 1;
        a.ScenarioId = std::string(f[6]);
        s.Attempts.push_back(std::move(a));
    }
    if (at >= lines.size())
        return false;
    if (lines[at] == "active=none")
        ++at;
    else
    {
        if (!next("active"))
            return false;
        const auto f = P402Split(v, ',');
        TowerRunnerRun r;
        if (f.size() != 10 || !P402Id(f[0]) || !P402Id(f[1]) || !P402Method(f[2], r.Method) || !P402Read(f[3], r.Beat) ||
            !P402Read(f[4], r.Conditions.Reserve) || !P402Read(f[5], r.Conditions.MinuteDiscount) || !P402Read(f[6], r.DisciplineSum) ||
            !P402Read(f[7], r.JudgmentSum) || !P402Read(f[8], r.MissionMinutes) || !P402Choices(f[9], r.Choices))
            return false;
        r.ScenarioId = std::string(f[0]);
        r.RouteId = std::string(f[1]);
        s.Active = std::move(r);
    }
    return true;
}

/** Version-gated: /1 holds the current session only; /2 adds "sittings=<n>" (1..P409MaxSitting) earlier bodies first. */
/** P5-03: the follow-up records after the current body; the count, sessions and records are bounded before allocating. */
bool P503DecodeFollowups(const std::vector<std::string_view>& lines, std::size_t& at, std::vector<P503FollowupRecord>& out)
{
    std::string_view v;
    const auto next = [&](std::string_view key) { return at < lines.size() && P402Value(lines[at++], key, v); };
    std::size_t count = 0;
    if (!next("followups") || !P402Read(v, count, std::uint64_t{P503MaxFollowupRecords}) || count == 0)
        return false;
    out.resize(count);
    for (P503FollowupRecord& r : out)
    {
        if (!next("followup"))
            return false;
        const auto f = P402Split(v, ',');
        std::size_t sessions = 0;
        if (f.size() != 4 || !P402Read(f[0], r.Sitting, std::uint64_t{P409MaxSitting}) || !P402Id(f[1]) ||
            !P402Read(f[2], r.Revision) || !P402Read(f[3], sessions, std::uint64_t{P503MaxFollowupSessions}) || sessions == 0)
            return false;
        r.AppointmentId = std::string(f[1]);
        r.Sessions.resize(sessions);
        for (P503FollowupSession& session : r.Sessions)
        {
            if (!next("session"))
                return false;
            const auto g = P402Split(v, ',');
            unsigned state = 0;
            if (g.size() != 3 || !P402Read(g[0], session.Generation) || !P402Read(g[1], session.At) ||
                !P402Read(g[2], state, static_cast<unsigned>(P503SessionState::Released)))
                return false;
            session.State = static_cast<P503SessionState>(state);
        }
        if (at >= lines.size())
            return false;
        if (lines[at] == "completion=none")
        {
            ++at;
            continue;
        }
        if (!next("completion"))
            return false;
        const auto c = P402Split(v, ',');
        P503FollowupCompletion done;
        if (c.size() != 5 || !P402Read(c[0], done.Generation) || !P402Id(c[1]) || !P402Id(c[2]) || !P402Read(c[3], done.CompletedAt) ||
            c[4].empty() || P402Split(c[4], ';').size() > P503MaxCompletionRecords || !P402Choices(c[4], done.Records))
            return false;
        done.TaskId = std::string(c[1]);
        done.ChoiceId = std::string(c[2]);
        r.Completion = std::move(done);
    }
    return true;
}

bool P402Decode(std::string_view text, std::vector<TowerSessionState>& earlier, TowerSessionState& s,
    std::vector<P503FollowupRecord>& followups)
{
    if (text.size() < 2 || text.back() != '\n')
        return false;
    const std::vector<std::string_view> lines = P402Split(text.substr(0, text.size() - 1), '\n');
    std::size_t at = 0;
    if (lines.empty())
        return false;
    const std::string_view header = lines[at++];
    const bool withFollowups = header == P503Header;
    if (header == P409Header || withFollowups)
    {
        std::string_view v;
        std::size_t count = 0;
        if (at >= lines.size() || !P402Value(lines[at++], "sittings", v) || !P402Read(v, count, std::uint64_t{P409MaxSitting}) ||
            (count == 0 && !withFollowups))
            return false;
        earlier.resize(count);
        for (TowerSessionState& e : earlier)
            if (!P402DecodeBody(lines, at, e))
                return false;
    }
    else if (header != P402Header)
        return false;
    if (!P402DecodeBody(lines, at, s))
        return false;
    if (withFollowups && !P503DecodeFollowups(lines, at, followups))
        return false;
    return at + 1 == lines.size() && lines[at] == "end";
}

bool P503Open(P503SessionState state)
{
    return state == P503SessionState::Booked || state == P503SessionState::Arrived || state == P503SessionState::Active ||
           state == P503SessionState::Paused;
}

/** The session of `sitting` (an earlier one or the current one); null past the current sitting. */
const TowerSessionState* P503SittingSession(const WorldState& world, std::uint32_t sitting)
{
    const std::uint32_t current = P409CurrentSitting(world);
    if (sitting > current)
        return nullptr;
    return sitting == current ? &world.Examination : &world.ExaminationEarlier[sitting];
}

bool P503Decided(const WorldState& world, std::uint32_t sitting)
{
    const TowerSessionState* session = P503SittingSession(world, sitting);
    return session != nullptr && session->Participation != TowerParticipation::Unset &&
           session->Participation != TowerParticipation::Entered;
}

bool P503RecordValid(const WorldState& world, const P503FollowupRecord& r)
{
    const GameInstant now = world.Clock.Now();
    if (!P503Decided(world, r.Sitting) || !r.AppointmentId.starts_with("appointment.") || !P402Id(r.AppointmentId) ||
        r.Sessions.empty() || r.Sessions.size() > P503MaxFollowupSessions)
        return false;
    const GameInstant opened = P409SittingAt(r.Sitting);
    std::uint64_t previous = 0;
    std::size_t paid = 0;
    std::size_t completed = 0;
    for (std::size_t i = 0; i < r.Sessions.size(); ++i)
    {
        const P503FollowupSession& s = r.Sessions[i];
        const bool last = i + 1 == r.Sessions.size();
        if (s.Generation <= previous || s.Generation > r.Revision || s.At < opened || s.At > DayLoopMaxInstant ||
            static_cast<unsigned>(s.State) > static_cast<unsigned>(P503SessionState::Released))
            return false;
        previous = s.Generation;
        if (P503Open(s.State) && !last)
            return false; // At most one open session, and it is the newest.
        if (s.State != P503SessionState::Booked && P503Open(s.State) && s.At > now)
            return false; // Arrival happens at the booked instant.
        if (s.State == P503SessionState::Paid && (s.At > now || ++paid > 1))
            return false;
        if (s.State == P503SessionState::Completed && (!last || ++completed > 1))
            return false;
    }
    if (completed == 0)
        return !r.Completion;
    if (!r.Completion)
        return false;
    const P503FollowupCompletion& c = *r.Completion;
    const P503FollowupSession& done = r.Sessions.back();
    return c.Generation == done.Generation && P503IsTaskChoice(c.TaskId, c.ChoiceId) && !c.Records.empty() &&
           c.Records.size() <= P503MaxCompletionRecords &&
           std::all_of(c.Records.begin(), c.Records.end(), [](const std::string& id) { return P402Id(id); }) &&
           c.CompletedAt >= done.At && c.CompletedAt <= now;
}

} // namespace

bool P503IsReservedFollowupFlag(std::string_view id) noexcept
{
    return id.starts_with(P407UFollowupFlagPrefix);
}

bool P503FollowupsValidInWorld(const WorldState& world)
{
    const auto& records = world.ExaminationFollowups;
    if (records.size() > P503MaxFollowupRecords)
        return false;
    for (std::size_t i = 0; i < records.size(); ++i)
    {
        if (!P503RecordValid(world, records[i]))
            return false;
        if (i > 0 && std::tie(records[i - 1].Sitting, records[i - 1].AppointmentId) >= std::tie(records[i].Sitting, records[i].AppointmentId))
            return false;
        // No repeat reward: one task completion per appointment in the whole life, across sittings.
        if (records[i].Completion)
            for (std::size_t j = 0; j < i; ++j)
                if (records[j].Completion && records[j].AppointmentId == records[i].AppointmentId)
                    return false;
    }
    return true;
}

namespace
{

} // namespace

bool IsExamStateValidInWorld(const WorldState& world)
{
    // P4-09: every earlier sitting closed or absent, then the current one under the live-session rules.
    const std::uint32_t current = P409CurrentSitting(world);
    if (current > P409MaxSitting)
        return false;
    for (std::uint32_t sitting = 0; sitting < current; ++sitting)
        if (!P409SessionValidInWorld(world, world.ExaminationEarlier[sitting], sitting, false))
            return false;
    return P409SessionValidInWorld(world, world.Examination, current, true) && P503FollowupsValidInWorld(world);
}

std::string SerialiseExamState(const WorldState& world)
{
    if (!IsExamStateValidInWorld(world))
        return {};
    return P402Encode(world.ExaminationEarlier, world.Examination, world.ExaminationFollowups);
}

bool DeserialiseExamState(std::string_view text, WorldState& world)
{
    std::vector<TowerSessionState> earlier;
    TowerSessionState decoded;
    std::vector<P503FollowupRecord> followups;
    if (!P402Decode(text, earlier, decoded, followups) || P402Encode(earlier, decoded, followups) != text)
        return false;
    WorldState staged = world;
    staged.Examination = std::move(decoded);
    staged.ExaminationEarlier = std::move(earlier);
    staged.ExaminationFollowups = std::move(followups);
    if (!IsExamStateValidInWorld(staged))
        return false;
    world.Examination = std::move(staged.Examination);
    world.ExaminationEarlier = std::move(staged.ExaminationEarlier);
    world.ExaminationFollowups = std::move(staged.ExaminationFollowups);
    return true;
}

std::string WorldSaveWriteExam(const WorldState& world)
{
    // P5-03: the reserved follow-up flags are a pre-v14 spelling only; a world still holding them is not saved.
    if (const auto it = world.Flags.lower_bound(P407UFollowupFlagPrefix); it != world.Flags.end() && P503IsReservedFollowupFlag(it->first))
        return {};
    const std::string text = SerialiseExamState(world);
    return text.empty() ? std::string{} : ",\n    \"examination\": " + WorldSaveQuoted(text);
}

WorldSaveResult WorldSaveReadExam(const JsonValue& value, WorldState& world, std::int32_t version)
{
    if (version <= WorldSaveLastWithoutExamVersion)
    {
        // No sitting, deferral or result is inferred from anything an older world kept: those lives are undecided.
        world.Examination = {};
        world.ExaminationEarlier.clear();
        world.ExaminationFollowups.clear();
        return {};
    }
    const JsonValue* exam = value.Find("examination");
    if (exam == nullptr || !exam->IsString())
        return WorldSaveInvalid("the examination section has the wrong type.");
    // P5-03: v13 never wrote follow-up records; its reserved flags migrate after the hash (P503MigrateLegacyFollowupFlags).
    if (version <= WorldSaveLastFollowupFlagsVersion && exam->String.starts_with(P503Header))
        return WorldSaveInvalid("the examination holds follow-up records this version never wrote.");
    if (!DeserialiseExamState(exam->String, world))
        return WorldSaveInvalid("the examination is damaged or does not fit this world.");
    if (version > WorldSaveLastFollowupFlagsVersion)
    {
        const auto reserved = world.Flags.lower_bound(P407UFollowupFlagPrefix);
        if (reserved != world.Flags.end() && P503IsReservedFollowupFlag(reserved->first))
            return WorldSaveInvalid("a world flag claims follow-up authority that only the examination section holds.");
    }
    return {};
}
bool P503MigrateLegacyFollowupFlags(WorldState& world, std::string& why)
{
    struct Legacy
    {
        std::map<GameInstant, bool> Book;
        std::set<GameInstant> Done;
    };
    const auto refuse = [&](std::string reason) {
        why = std::move(reason);
        return false;
    };
    std::map<std::string, Legacy> byAppointment;
    std::vector<std::string> keys;
    for (auto it = world.Flags.lower_bound(P407UFollowupFlagPrefix); it != world.Flags.end() && P503IsReservedFollowupFlag(it->first); ++it)
    {
        keys.push_back(it->first);
        const std::string_view rest = std::string_view(it->first).substr(P407UFollowupFlagPrefix.size());
        if (rest.find(".p502.") != std::string_view::npos)
            return refuse("a provisional P5-02 task flag (" + it->first + ") was never a released save format.");
        const std::size_t book = rest.rfind(".book.");
        const std::size_t done = rest.rfind(".done.");
        if (book == std::string_view::npos && done == std::string_view::npos)
            return refuse("the follow-up flag " + it->first + " is not a reservation or an attendance.");
        const bool isBook = done == std::string_view::npos || (book != std::string_view::npos && book > done);
        const std::size_t at = isBook ? book : done;
        const std::string_view appointment = rest.substr(0, at);
        GameInstant instant = 0;
        if (!appointment.starts_with("appointment.") || !P402Id(appointment) || !P402Read(rest.substr(at + 6), instant) ||
            instant > DayLoopMaxInstant)
            return refuse("the follow-up flag " + it->first + " is not in the canonical P4-07 spelling.");
        Legacy& legacy = byAppointment[std::string(appointment)];
        if (isBook)
            legacy.Book[instant] = it->second;
        else if (!it->second)
            return refuse("an attendance flag is false, which P4-07 never wrote (" + it->first + ").");
        else
            legacy.Done.insert(instant);
    }
    if (keys.empty())
        return true;
    if (!world.ExaminationFollowups.empty())
        return refuse("a world holds both follow-up flags and follow-up records.");
    const std::uint32_t current = P409CurrentSitting(world);
    const auto attribute = [&](GameInstant at, std::uint32_t& sitting) {
        std::size_t candidates = 0;
        for (std::uint32_t s = 0; s <= current; ++s)
            if (P503Decided(world, s) && P409SittingAt(s) <= at)
            {
                sitting = s;
                ++candidates;
            }
        return candidates == 1;
    };
    WorldState staged = world;
    for (const std::string& key : keys)
        staged.Flags.erase(key);
    std::map<std::pair<std::uint32_t, std::string>, P503FollowupRecord> records;
    for (const auto& [appointment, legacy] : byAppointment)
    {
        std::vector<GameInstant> live;
        for (const auto& [at, value] : legacy.Book)
            if (value)
                live.push_back(at);
        if (live.size() > 1)
            return refuse(appointment + " holds two live reservations, which P4-07 never wrote.");
        if (legacy.Done.size() > 1)
            return refuse(appointment + " holds two attendances, which P4-07 never wrote.");
        if (!legacy.Done.empty() && (live.size() != 1 || live.front() != *legacy.Done.begin()))
            return refuse(appointment + " holds an attendance without its own reservation, which P4-07 never wrote.");
        if (!legacy.Done.empty() && *legacy.Done.begin() > world.Clock.Now())
            return refuse(appointment + " holds an attendance after the saved clock.");
        // Terminal history first, in instant order, then the one live reservation: generations record order of creation only.
        std::vector<P503FollowupSession> sessions;
        for (const auto& [at, value] : legacy.Book)
            if (!value)
                sessions.push_back({0, at, P503SessionState::Released});
        if (!legacy.Done.empty())
            sessions.push_back({0, *legacy.Done.begin(), P503SessionState::Paid});
        std::stable_sort(sessions.begin(), sessions.end(), [](const auto& a, const auto& b) { return a.At < b.At; });
        if (legacy.Done.empty() && !live.empty())
            sessions.push_back({0, live.front(), P503SessionState::Booked});
        for (P503FollowupSession& session : sessions)
        {
            std::uint32_t sitting = 0;
            if (!attribute(session.At, sitting))
                return refuse(appointment + " at " + std::to_string(session.At) +
                              " cannot be attributed to exactly one decided sitting; the original save is kept.");
            P503FollowupRecord& record = records[{sitting, appointment}];
            record.Sitting = sitting;
            record.AppointmentId = appointment;
            session.Generation = ++record.Revision;
            record.Sessions.push_back(session);
        }
    }
    for (auto& [key, record] : records)
        staged.ExaminationFollowups.push_back(std::move(record));
    if (!P503FollowupsValidInWorld(staged))
        return refuse("the migrated follow-up records break a rule of this world.");
    world = std::move(staged);
    return true;
}
} // namespace PerfectWorld
