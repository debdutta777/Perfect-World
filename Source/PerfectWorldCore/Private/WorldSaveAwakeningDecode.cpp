#include "WorldSaveInternal.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// R9-04: reading the pw.awakening/1 envelope (format in WorldSave.h; written by WorldSaveAwakening.cpp) and the empty
// Awakening slices every earlier world migrates to. Only the canonical spelling is accepted: a decoded state is re-encoded
// and must give back the exact text, and it must keep WorldSaveAwakeningRules.cpp's rules inside the world being read.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view R94DHeader = "pw.awakening/1";
/** The longest line is the live profile: 5 + 4 * 6 + 3 * 6 * 7 + 3 fields. */
constexpr std::size_t R94DMaxFields = 160;
/** Bounds that keep a decode finite; the rules and the native codecs carry the real limits. */
constexpr std::size_t R94DMaxList = 1'000'000;

bool R94DLine(std::string_view& text, std::string_view& line)
{
    const std::size_t end = text.find('\n');
    if (end == std::string_view::npos)
        return false;
    line = text.substr(0, end);
    text.remove_prefix(end + 1);
    return true;
}

bool R94DKeyed(std::string_view& text, std::string_view key, std::string_view& value)
{
    std::string_view line;
    if (!R94DLine(text, line) || line.size() < key.size() + 1 || !line.starts_with(key) || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

bool R94DSplit(std::string_view value, std::vector<std::string_view>& fields, char separator = ',')
{
    fields.clear();
    while (fields.size() <= R94DMaxFields)
    {
        const std::size_t at = value.find(separator);
        fields.push_back(value.substr(0, at));
        if (at == std::string_view::npos)
            return true;
        value.remove_prefix(at + 1);
    }
    return false;
}

template <typename T>
bool R94DNumber(std::string_view text, T& out)
{
    if (text.empty())
        return false;
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
        return false;
    out = value;
    return true;
}

template <typename E>
bool R94DEnum(std::string_view text, unsigned count, E& out)
{
    unsigned value = 0;
    if (!R94DNumber(text, value) || value >= count)
        return false;
    out = static_cast<E>(value);
    return true;
}

bool R94DFlag(std::string_view text, bool& out)
{
    if (text != "0" && text != "1")
        return false;
    out = text == "1";
    return true;
}

bool R94DInstant(std::string_view text, std::optional<GameInstant>& out)
{
    if (text.empty())
    {
        out.reset();
        return true;
    }
    GameInstant value = 0;
    if (!R94DNumber(text, value))
        return false;
    out = value;
    return true;
}

template <typename T>
bool R94DKeyedNumber(std::string_view& text, std::string_view key, T& out)
{
    std::string_view value;
    return R94DKeyed(text, key, value) && R94DNumber(value, out);
}

bool R94DCount(std::string_view& text, std::string_view key, std::size_t& out)
{
    return R94DKeyedNumber(text, key, out) && out <= R94DMaxList && out <= text.size();
}

bool R94DBlock(std::string_view& text, std::string_view key, std::string_view& block)
{
    std::size_t length = 0;
    if (!R94DKeyedNumber(text, key, length) || length > text.size())
        return false;
    block = text.substr(0, length);
    text.remove_prefix(length);
    return true;
}

/** Fills `out` from fields[from, from + out.size()). */
template <typename Array>
bool R94DArray(const std::vector<std::string_view>& f, std::size_t from, Array& out)
{
    if (f.size() < from + out.size())
        return false;
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        if (!R94DNumber(f[from + i], out[i]))
            return false;
    }
    return true;
}

template <typename Array>
bool R94DKeyedArray(std::string_view& text, std::string_view key, Array& out)
{
    std::string_view value;
    std::vector<std::string_view> f;
    return R94DKeyed(text, key, value) && R94DSplit(value, f) && f.size() == out.size() && R94DArray(f, 0, out);
}

bool R94DStats(std::string_view& text, std::string_view prefix, LatentStats& stats)
{
    const std::string p(prefix);
    std::size_t count = 0;
    if (!R94DKeyedArray(text, p + "attributes", stats.Attributes) || !R94DCount(text, p + "skills", count))
        return false;
    stats.Skills.resize(count);
    std::string_view value;
    std::vector<std::string_view> f;
    for (SkillLevel& skill : stats.Skills)
    {
        if (!R94DKeyed(text, p + "skill", value) || !R94DSplit(value, f) || f.size() != 2 || !WorldSaveAwakeningSafeText(f[0]) ||
            !R94DNumber(f[1], skill.LevelMilli))
            return false;
        skill.SkillId = std::string(f[0]);
    }
    return true;
}

bool R94DCauses(std::string_view& text, std::vector<AwakeningCause>& causes)
{
    std::size_t count = 0;
    if (!R94DCount(text, "causes", count))
        return false;
    causes.resize(count);
    std::string_view value;
    std::vector<std::string_view> f;
    for (AwakeningCause& c : causes)
    {
        if (!R94DKeyed(text, "cause", value) || !R94DSplit(value, f) || f.size() != 6 ||
            !R94DEnum(f[0], static_cast<unsigned>(AwakeningSignalCount), c.Signal) || !R94DNumber(f[1], c.Reading) ||
            !R94DNumber(f[2], c.Contribution) || !WorldSaveAwakeningSafeText(f[3]) || !WorldSaveAwakeningSafeText(f[4]) ||
            !WorldSaveAwakeningSafeText(f[5]))
            return false;
        c.Source = std::string(f[3]);
        c.TestId = std::string(f[4]);
        c.ChoiceId = std::string(f[5]);
    }
    return true;
}

bool R94DPath(std::string_view value, AptitudePathProjection& p)
{
    std::vector<std::string_view> f;
    if (!R94DSplit(value, f) || f.size() != 6 || !R94DEnum(f[0], static_cast<unsigned>(AptitudePathCount), p.Path) ||
        !R94DEnum(f[1], 4u, p.Band) || !R94DEnum(f[2], 4u, p.Coverage))
        return false;
    std::vector<std::string_view> items;
    if (!f[3].empty())
    {
        if (!R94DSplit(f[3], items, ';'))
            return false;
        for (const std::string_view item : items)
        {
            std::vector<std::string_view> pair;
            AptitudeFacetStatement statement;
            if (!R94DSplit(item, pair, ':') || pair.size() != 2 || !R94DEnum(pair[0], static_cast<unsigned>(AptitudeFacetCount), statement.Facet) ||
                !R94DEnum(pair[1], 4u, statement.State))
                return false;
            p.Facets.push_back(statement);
        }
    }
    if (!f[4].empty())
    {
        if (!R94DSplit(f[4], items, ';'))
            return false;
        for (const std::string_view item : items)
        {
            AptitudeFacet facet = AptitudeFacet::Analysis;
            if (!R94DEnum(item, static_cast<unsigned>(AptitudeFacetCount), facet))
                return false;
            p.Unobserved.push_back(facet);
        }
    }
    if (!f[5].empty())
    {
        if (!R94DSplit(f[5], items, ';'))
            return false;
        for (const std::string_view item : items)
            p.Strongest.emplace_back(item);
    }
    return true;
}

bool R94DGrant(std::string_view& text, std::string_view grantValue, AwakeningGrant& g)
{
    if (!R94DNumber(grantValue, g.At) || !R94DStats(text, "input_", g.Input.Stats))
        return false;
    std::size_t count = 0;
    std::string_view value;
    if (!R94DCount(text, "paths", count))
        return false;
    g.Input.Projection.Paths.resize(count);
    for (AptitudePathProjection& p : g.Input.Projection.Paths)
    {
        if (!R94DKeyed(text, "path", value) || !R94DPath(value, p))
            return false;
    }
    AwakeningResult& r = g.Result;
    if (!R94DKeyed(text, "observed", value) || !R94DFlag(value, r.ConductObserved) || !R94DKeyedArray(text, "signals", r.Signals) ||
        !R94DKeyedArray(text, "tattva_milli", r.TattvaMilli) || !R94DKeyedArray(text, "tattva_percent", r.TattvaPercent))
        return false;
    for (auto& causes : r.TattvaCauses)
    {
        if (!R94DCauses(text, causes))
            return false;
    }
    if (!R94DKeyedArray(text, "rating", r.Rating) || !R94DKeyedArray(text, "latent_rating", r.LatentRating) ||
        !R94DKeyedArray(text, "conduct_rating", r.ConductRating))
        return false;
    for (auto& causes : r.RatingCauses)
    {
        if (!R94DCauses(text, causes))
            return false;
    }
    std::vector<std::string_view> f;
    AwakenedAbilityGrant& a = r.Ability;
    if (!R94DKeyed(text, "ability", value) || !R94DSplit(value, f) || f.size() != 5 || !WorldSaveAwakeningSafeText(f[0]) ||
        !WorldSaveAwakeningSafeText(f[1]) || !R94DNumber(f[2], a.Rank) || !R94DFlag(f[3], a.Fallback) || !R94DNumber(f[4], a.Score))
        return false;
    a.RecipeId = std::string(f[0]);
    a.OriginTag = std::string(f[1]);
    return R94DCauses(text, a.Causes);
}

bool R94DProfile(std::string_view value, AwakenedProfile& p)
{
    std::vector<std::string_view> f;
    constexpr std::size_t windowFields = 3u * PowerDimensionCount * PracticeDaysInWindow;
    constexpr std::size_t total = TattvaCount + 4u * PowerDimensionCount + windowFields + 3u;
    if (!R94DSplit(value, f) || f.size() != total)
        return false;
    std::size_t at = 0;
    if (!R94DArray(f, at, p.AffinityPercent) || !R94DArray(f, at += TattvaCount, p.Rating) ||
        !R94DArray(f, at += PowerDimensionCount, p.Peak) || !R94DArray(f, at += PowerDimensionCount, p.LastPracticeDay) ||
        !R94DArray(f, at += PowerDimensionCount, p.DecaySettledDay))
        return false;
    at += PowerDimensionCount;
    for (auto& window : p.Window)
    {
        for (PracticeDay& day : window)
        {
            if (!R94DNumber(f[at], day.Day) || !R94DNumber(f[at + 1], day.Minutes) || !R94DNumber(f[at + 2], day.GainCentipoints))
                return false;
            at += 3;
        }
    }
    return R94DEnum(f[at], static_cast<unsigned>(AwakenedGradeCount), p.Grade) && R94DNumber(f[at + 1], p.SupervisedPracticeMinutes) &&
           R94DNumber(f[at + 2], p.EarliestRetakeDay);
}

bool R94DSlices(std::string_view& text, WorldSaveAwakeningSlices& s)
{
    AwakeningState& a = s.Awakening;
    std::string_view line;
    std::string_view value;
    std::vector<std::string_view> f;
    std::size_t count = 0;
    if (!R94DLine(text, line) || line != R94DHeader || !R94DKeyed(text, "seeded", value) || !R94DSplit(value, f) || f.size() != 2 ||
        !R94DFlag(f[0], a.Seeded) || !R94DNumber(f[1], a.WorldSeed) || !R94DStats(text, "", a.Stats) ||
        !R94DKeyedArray(text, "evidence", a.AttributeEvidenceMilli))
        return false;
    std::string_view native;
    if (!R94DBlock(text, "projection", native))
        return false;
    AptitudeParse projection = DeserialiseAptitude(native);
    if (!projection.Ok() || !R94DCount(text, "applied", count))
        return false;
    a.ProjectionEvidence = std::move(projection.Ledger);
    a.AppliedActivityInstances.resize(count);
    for (std::uint64_t& instance : a.AppliedActivityInstances)
    {
        if (!R94DKeyedNumber(text, "instance", instance))
            return false;
    }
    if (!R94DBlock(text, "conduct", native))
        return false;
    std::optional<ConductLedger> conduct = ParseConductLedger(native);
    if (!conduct || !R94DKeyed(text, "appointment", value) || !R94DSplit(value, f) || f.size() != 2 ||
        !R94DNumber(f[0], a.AppointmentAt) || !R94DNumber(f[1], a.ExaminationAt) || !R94DKeyed(text, "makeup", value) ||
        !R94DSplit(value, f) || f.size() != 2 || !R94DEnum(f[0], 4u, a.MakeUp.Phase) || !R94DInstant(f[1], a.MakeUp.At) ||
        !R94DKeyed(text, "grant", value))
        return false;
    a.Conduct = std::move(*conduct);
    if (value != "none")
    {
        AwakeningGrant grant;
        if (!R94DGrant(text, value, grant))
            return false;
        // Derived, never stored: the frozen conduct summary, the world seed and the starting profile of the committed result.
        grant.Input.Conduct = SummariseConduct(a.Conduct);
        grant.Input.WorldSeed = a.WorldSeed;
        const std::optional<AwakenedProfile> starting = AwakenedStartingProfile(grant.Result, GameClock::DayNumber(grant.At));
        if (!starting)
            return false;
        grant.StartingProfile = *starting;
        a.Granted = std::move(grant);
    }
    TutorialDungeonState& t = s.Tutorial;
    unsigned next = 0;
    if (!R94DKeyed(text, "tutorial", value) || !R94DSplit(value, f) || f.size() != 7 || !R94DEnum(f[0], 4u, t.Phase) ||
        !R94DNumber(f[1], next) || next > TutorialSituationCount || !R94DFlag(f[2], t.RecoveryPending) || !R94DFlag(f[3], t.Withdrawn) ||
        !R94DInstant(f[4], t.StartedAt) || !R94DInstant(f[5], t.LastActionAt) || !R94DInstant(f[6], t.CompletedAt))
        return false;
    t.NextSituation = static_cast<std::uint8_t>(next);
    std::string_view probe = text;
    if (R94DKeyed(probe, "countdown", value) && value == "none")
        text = probe;
    else if (R94DBlock(text, "countdown", native) && !native.empty())
        s.Countdown = std::string(native);
    else
        return false;
    if (!R94DKeyed(text, "live", value))
        return false;
    if (value != "none")
    {
        AwakenedProfile live;
        if (!R94DProfile(value, live))
            return false;
        s.Live = std::move(live);
    }
    return true;
}
} // namespace

bool WorldSaveAwakeningDecode(std::string_view text, WorldSaveAwakeningSlices& out)
{
    const std::string_view original = text;
    WorldSaveAwakeningSlices slices;
    if (!R94DSlices(text, slices) || text != "end\n" || WorldSaveAwakeningEncode(slices) != original)
        return false;
    out = std::move(slices);
    return true;
}

bool DeserialiseAwakeningState(std::string_view text, WorldState& world)
{
    WorldSaveAwakeningSlices slices;
    if (!WorldSaveAwakeningDecode(text, slices))
        return false;
    WorldState staged = world;
    staged.Awakening = std::move(slices.Awakening);
    staged.Tutorial = slices.Tutorial;
    staged.CountdownText = std::move(slices.Countdown);
    staged.LiveAwakenedProfile = std::move(slices.Live);
    if (!WorldSaveAwakeningValid(staged))
        return false;
    world = std::move(staged);
    return true;
}

WorldSaveResult WorldSaveReadAwakening(const JsonValue& value, WorldState& world, std::int32_t version)
{
    if (version <= WorldSaveLastWithoutAwakeningVersion)
    {
        // No Awakening inputs, session, countdown or live profile is inferred from anything an older world kept: those lives
        // are unseeded until a creation seeds them, and no past school year, conduct or grant is invented.
        world.Awakening = {};
        world.Tutorial = {};
        world.CountdownText.clear();
        world.LiveAwakenedProfile.reset();
        return {};
    }
    const JsonValue* awakening = value.Find("awakening");
    if (awakening == nullptr || !awakening->IsString())
        return WorldSaveInvalid("the awakening section has the wrong type.");
    if (!DeserialiseAwakeningState(awakening->String, world))
        return WorldSaveInvalid("the Awakening, its session, the countdown or the live profile are damaged or do not fit this world.");
    return {};
}
} // namespace PerfectWorld
