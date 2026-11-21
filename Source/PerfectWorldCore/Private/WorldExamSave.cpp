#include "WorldExam.h"

#include "OpportunityReceipts.h"
#include "WorldExamSittings.h"
#include "WorldSaveInternal.h"

#include <algorithm>
#include <limits>
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
std::string P402Encode(const std::vector<TowerSessionState>& earlier, const TowerSessionState& s)
{
    bool ok = earlier.size() <= P409MaxSitting;
    std::string out;
    if (earlier.empty())
        out = std::string(P402Header) + '\n';
    else
    {
        out = std::string(P409Header) + "\nsittings=" + P402Num(earlier.size()) + '\n';
        for (const TowerSessionState& e : earlier)
            out += P402Body(e, ok);
    }
    out += P402Body(s, ok) + "end\n";
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
bool P402Decode(std::string_view text, std::vector<TowerSessionState>& earlier, TowerSessionState& s)
{
    if (text.size() < 2 || text.back() != '\n')
        return false;
    const std::vector<std::string_view> lines = P402Split(text.substr(0, text.size() - 1), '\n');
    std::size_t at = 0;
    if (lines.empty())
        return false;
    const std::string_view header = lines[at++];
    if (header == P409Header)
    {
        std::string_view v;
        std::size_t count = 0;
        if (at >= lines.size() || !P402Value(lines[at++], "sittings", v) || !P402Read(v, count, std::uint64_t{P409MaxSitting}) ||
            count == 0)
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
    return at + 1 == lines.size() && lines[at] == "end";
}

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
    return P409SessionValidInWorld(world, world.Examination, current, true);
}

std::string SerialiseExamState(const WorldState& world)
{
    if (!IsExamStateValidInWorld(world))
        return {};
    return P402Encode(world.ExaminationEarlier, world.Examination);
}

bool DeserialiseExamState(std::string_view text, WorldState& world)
{
    std::vector<TowerSessionState> earlier;
    TowerSessionState decoded;
    if (!P402Decode(text, earlier, decoded) || P402Encode(earlier, decoded) != text)
        return false;
    WorldState staged = world;
    staged.Examination = std::move(decoded);
    staged.ExaminationEarlier = std::move(earlier);
    if (!IsExamStateValidInWorld(staged))
        return false;
    world.Examination = std::move(staged.Examination);
    world.ExaminationEarlier = std::move(staged.ExaminationEarlier);
    return true;
}

std::string WorldSaveWriteExam(const WorldState& world)
{
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
        return {};
    }
    const JsonValue* exam = value.Find("examination");
    if (exam == nullptr || !exam->IsString())
        return WorldSaveInvalid("the examination section has the wrong type.");
    if (!DeserialiseExamState(exam->String, world))
        return WorldSaveInvalid("the examination is damaged or does not fit this world.");
    return {};
}
} // namespace PerfectWorld
