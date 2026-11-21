#include "WorldExam.h"

#include "OpportunityReceipts.h"
#include "WorldSaveInternal.h"

#include <algorithm>
#include <limits>
#include <utility>

// P4-02: the pw.examination/1 envelope (format in WorldExam.h), the content-free rules the examination keeps inside its world,
// and its place in the world save (schema 13, after "awakening"). Decoding parses into a temporary, re-encodes it and demands
// the identical text (one canonical spelling), then applies the world rules; the reader never trusts a stored score, and the
// replay of choice provenance against runner content is ExamStateMatchesRunners at the WorldSim boundary.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view P402Header = "pw.examination/1";

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

std::string P402Encode(const TowerSessionState& s)
{
    bool ok = static_cast<unsigned>(s.Participation) <= static_cast<unsigned>(TowerParticipation::Declined);
    std::string out = std::string(P402Header) + "\nparticipation=" + P402Num(static_cast<unsigned>(s.Participation));
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
    out += "\nend\n";
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

bool P402Decode(std::string_view text, TowerSessionState& s)
{
    if (text.size() < 2 || text.back() != '\n')
        return false;
    const std::vector<std::string_view> lines = P402Split(text.substr(0, text.size() - 1), '\n');
    std::size_t at = 0;
    std::string_view v;
    const auto next = [&](std::string_view key) { return at < lines.size() && P402Value(lines[at++], key, v); };
    if (lines.empty() || lines[at++] != P402Header)
        return false;
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
    return at + 1 == lines.size() && lines[at] == "end";
}

bool P402MethodAllowed(const TowerRouteDefinition& route, TowerMethod method)
{
    return method == route.Method || (method == TowerMethod::Combat && route.AllowsCombat);
}

bool P402ChoicesValid(const std::vector<std::string>& choices, std::size_t low)
{
    return choices.size() >= low && choices.size() <= TowerRunnerBeatLimit &&
           std::all_of(choices.begin(), choices.end(), [](const std::string& id) { return P402Id(id); });
}

bool P402AttemptValid(const TowerSessionAttempt& a, std::size_t floor)
{
    const TowerRouteDefinition* route = FindTowerRoute(a.RouteId);
    const auto beats = static_cast<std::int64_t>(a.ChoiceIds.size());
    return route != nullptr && route->Floor == static_cast<std::int32_t>(floor) && P402MethodAllowed(*route, a.Method) &&
           P402Id(a.ScenarioId) && P402ChoicesValid(a.ChoiceIds, 1) && a.DisciplineMilli >= 0 &&
           a.DisciplineMilli <= TowerScoreMaxMilli && a.CommonJudgmentMilli >= 0 && a.CommonJudgmentMilli <= TowerScoreMaxMilli &&
           a.MissionMinutes >= beats && a.MissionMinutes <= 60 * beats;
}

bool P402ActiveValid(const TowerRunnerRun& r, std::size_t floor)
{
    const TowerRouteDefinition* route = FindTowerRoute(r.RouteId);
    const auto beats = static_cast<std::int64_t>(r.Beat);
    return route != nullptr && route->Floor == static_cast<std::int32_t>(floor) && P402MethodAllowed(*route, r.Method) &&
           P402Id(r.ScenarioId) && r.Beat == r.Choices.size() && r.Beat < TowerRunnerBeatLimit && P402ChoicesValid(r.Choices, 0) &&
           r.Conditions.Reserve >= 0 && r.Conditions.Reserve <= 10 && r.Conditions.MinuteDiscount >= 0 &&
           r.Conditions.MinuteDiscount <= 2 && r.DisciplineSum >= 0 && r.DisciplineSum <= TowerScoreMaxMilli * beats &&
           r.JudgmentSum >= 0 && r.JudgmentSum <= TowerScoreMaxMilli * beats && r.MissionMinutes >= beats &&
           r.MissionMinutes <= 60 * beats;
}

bool P402PreparationValid(const WorldState& world, const TowerSessionState& s)
{
    const TowerPreparationSnapshot& p = s.Preparation;
    if (p.Composure < 0 || p.Composure > CountdownScoreMax || p.RouteDelayMinutes < 0 || p.RouteDelayMinutes > TowerWorldCapMinutes ||
        std::any_of(p.Preparation.begin(), p.Preparation.end(), [](std::int32_t v) { return v < 0 || v > CountdownScoreMax; }))
        return false;
    const P402ExamPreparation authority = P402FinalPreparation(world);
    if (!authority.Ok())
        return false;
    if (authority.ZeroByCompatibility)
        return p == TowerPreparationSnapshot{};
    // Composure reads today's condition, so it is exact only while the clock stands at the entry instant.
    return p.Preparation == authority.Profile.Preparation && p.RouteDelayMinutes == authority.Profile.RouteDelayMinutes &&
           (s.Participation != TowerParticipation::Entered || p.Composure == authority.Profile.Composure);
}
} // namespace

bool IsExamStateValidInWorld(const WorldState& world)
{
    const TowerSessionState& s = world.Examination;
    const GameInstant at = CountdownEndInstant();
    const GameInstant now = world.Clock.Now();
    TowerSessionState empty;
    empty.Participation = s.Participation;
    switch (s.Participation)
    {
    case TowerParticipation::Unset: return s == empty;
    case TowerParticipation::Deferred:
    case TowerParticipation::Declined: return s == empty && now >= at;
    case TowerParticipation::Entered:
    case TowerParticipation::Completed:
    case TowerParticipation::Ejected:
    case TowerParticipation::Withdrawn: break;
    default: return false;
    }
    if (!world.Awakening.Granted || now < at || !P402PreparationValid(world, s))
        return false;
    const std::uint64_t opened = s.Attempts.size() + (s.Active ? 1U : 0U);
    if (s.Attempts.size() > static_cast<std::size_t>(TowerFloorCount) || s.NextRunner != opened + 1 || s.Runner != opened)
        return false;
    for (std::size_t i = 0; i < s.Attempts.size(); ++i)
    {
        const bool last = i + 1 == s.Attempts.size();
        if (!P402AttemptValid(s.Attempts[i], i + 1) || (s.Attempts[i].Defeated && !last))
            return false;
    }
    const bool defeated = !s.Attempts.empty() && s.Attempts.back().Defeated;
    const bool climbed = s.Attempts.size() == static_cast<std::size_t>(TowerFloorCount);
    if (s.Active && !P402ActiveValid(*s.Active, s.Attempts.size() + 1))
        return false;
    switch (s.Participation)
    {
    case TowerParticipation::Entered:
        // The clock stands still while the examination runs: it moves only with the terminal charge.
        if (defeated || climbed || now != at)
            return false;
        return true;
    case TowerParticipation::Completed:
        if (defeated || !climbed || s.Active)
            return false;
        break;
    case TowerParticipation::Ejected:
        if (!defeated || s.Active)
            return false;
        break;
    default: // Withdrawn
        if (defeated || climbed)
            return false;
        break;
    }
    const P402ExamFinish finished = P402ExamFinishSeam(s);
    return finished.Ok() && finished.Score.OutsideMinutes >= 0 && finished.Score.OutsideMinutes <= TowerWorldCapMinutes &&
           now >= at + finished.Score.OutsideMinutes;
}

std::string SerialiseExamState(const WorldState& world)
{
    if (!IsExamStateValidInWorld(world))
        return {};
    return P402Encode(world.Examination);
}

bool DeserialiseExamState(std::string_view text, WorldState& world)
{
    TowerSessionState decoded;
    if (!P402Decode(text, decoded) || P402Encode(decoded) != text)
        return false;
    WorldState staged = world;
    staged.Examination = std::move(decoded);
    if (!IsExamStateValidInWorld(staged))
        return false;
    world.Examination = std::move(staged.Examination);
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
