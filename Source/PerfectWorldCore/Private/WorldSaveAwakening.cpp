#include "WorldSaveInternal.h"

#include <string>
#include <utility>

// R9-04: writing the pw.awakening/1 envelope (format in WorldSave.h) and its place in the world save (schema 12). Reading is
// in WorldSaveAwakeningDecode.cpp; the semantic rules a state must keep to be written or read are in
// WorldSaveAwakeningRules.cpp. Every enum is written as its declared ordinal (the frozen R9-01/R9-02 contracts fix them).
namespace PerfectWorld
{
namespace
{
constexpr std::string_view R94Header = "pw.awakening/1";

/** Builds comma-joined fields; any unwritable text field spoils the whole envelope. */
class R94Fields
{
public:
    R94Fields& Text(std::string_view value)
    {
        Ok_ = Ok_ && WorldSaveAwakeningSafeText(value);
        return Raw(value);
    }
    template <typename T>
    R94Fields& Number(T value)
    {
        return Raw(std::to_string(value));
    }
    template <typename E>
    R94Fields& Enum(E value)
    {
        return Number(static_cast<unsigned>(value));
    }
    R94Fields& Flag(bool value) { return Raw(value ? "1" : "0"); }
    R94Fields& Instant(const std::optional<GameInstant>& value) { return value ? Number(*value) : Raw(""); }
    template <typename Array>
    R94Fields& Numbers(const Array& values)
    {
        for (const auto value : values)
            Number(value);
        return *this;
    }
    /** A ';'-joined list of safe tokens (possibly empty) as one field. */
    template <typename List, typename Token>
    R94Fields& Joined(const List& list, Token token)
    {
        std::string joined;
        for (const auto& item : list)
        {
            const std::string part = token(item);
            Ok_ = Ok_ && !part.empty() && part.find(';') == std::string::npos && WorldSaveAwakeningSafeText(part);
            if (!joined.empty())
                joined += ';';
            joined += part;
        }
        return Raw(joined);
    }
    R94Fields& Raw(std::string_view value)
    {
        if (!First_)
            Out_ += ',';
        First_ = false;
        Out_ += value;
        return *this;
    }
    [[nodiscard]] bool Ok() const noexcept { return Ok_; }
    [[nodiscard]] const std::string& Value() const noexcept { return Out_; }

private:
    std::string Out_;
    bool First_ = true;
    bool Ok_ = true;
};

class R94Writer
{
public:
    void Line(std::string_view key, const R94Fields& fields)
    {
        Ok_ = Ok_ && fields.Ok();
        Out_ += std::string(key) + "=" + fields.Value() + "\n";
    }
    void Block(std::string_view key, const std::string& native)
    {
        Ok_ = Ok_ && !native.empty();
        Out_ += std::string(key) + "=" + std::to_string(native.size()) + "\n" + native;
    }
    void Literal(std::string_view line) { Out_ += std::string(line) + "\n"; }
    void Stats(std::string_view prefix, const LatentStats& stats)
    {
        Line(std::string(prefix) + "attributes", R94Fields().Numbers(stats.Attributes));
        Line(std::string(prefix) + "skills", R94Fields().Number(stats.Skills.size()));
        for (const SkillLevel& skill : stats.Skills)
            Line(std::string(prefix) + "skill", R94Fields().Text(skill.SkillId).Number(skill.LevelMilli));
    }
    void Causes(const std::vector<AwakeningCause>& causes)
    {
        Line("causes", R94Fields().Number(causes.size()));
        for (const AwakeningCause& c : causes)
            Line("cause", R94Fields().Enum(c.Signal).Number(c.Reading).Number(c.Contribution).Text(c.Source).Text(c.TestId).Text(c.ChoiceId));
    }
    [[nodiscard]] std::string Take() { return Ok_ ? std::move(Out_) : std::string{}; }
    std::string Out_;
    bool Ok_ = true;
};

void R94WriteProjection(R94Writer& w, const AptitudeProjection& projection)
{
    w.Line("paths", R94Fields().Number(projection.Paths.size()));
    for (const AptitudePathProjection& p : projection.Paths)
    {
        w.Line("path", R94Fields()
                           .Enum(p.Path)
                           .Enum(p.Band)
                           .Enum(p.Coverage)
                           .Joined(p.Facets, [](const AptitudeFacetStatement& f) {
                               return std::to_string(static_cast<unsigned>(f.Facet)) + ":" + std::to_string(static_cast<unsigned>(f.State));
                           })
                           .Joined(p.Unobserved, [](AptitudeFacet f) { return std::to_string(static_cast<unsigned>(f)); })
                           .Joined(p.Strongest, [](const std::string& id) { return id; }));
    }
}

void R94WriteGrant(R94Writer& w, const AwakeningGrant& g)
{
    // Input.Conduct, Input.WorldSeed and StartingProfile are derived on read (WorldSaveAwakeningDecode.cpp) and checked here
    // by the rules: the conduct ledger is frozen once the supervised session ends, and the profile is AwakenedStartingProfile.
    w.Line("grant", R94Fields().Number(g.At));
    w.Stats("input_", g.Input.Stats);
    R94WriteProjection(w, g.Input.Projection);
    const AwakeningResult& r = g.Result;
    w.Line("observed", R94Fields().Flag(r.ConductObserved));
    w.Line("signals", R94Fields().Numbers(r.Signals));
    w.Line("tattva_milli", R94Fields().Numbers(r.TattvaMilli));
    w.Line("tattva_percent", R94Fields().Numbers(r.TattvaPercent));
    for (const auto& causes : r.TattvaCauses)
        w.Causes(causes);
    w.Line("rating", R94Fields().Numbers(r.Rating));
    w.Line("latent_rating", R94Fields().Numbers(r.LatentRating));
    w.Line("conduct_rating", R94Fields().Numbers(r.ConductRating));
    for (const auto& causes : r.RatingCauses)
        w.Causes(causes);
    const AwakenedAbilityGrant& a = r.Ability;
    w.Line("ability", R94Fields().Text(a.RecipeId).Text(a.OriginTag).Number(a.Rank).Flag(a.Fallback).Number(a.Score));
    w.Causes(a.Causes);
}
} // namespace

bool WorldSaveAwakeningSafeText(std::string_view text)
{
    for (const char c : text)
    {
        if (c <= ' ' || c > '~' || c == ',')
            return false;
    }
    return true;
}

std::string WorldSaveAwakeningProfileText(const AwakenedProfile& p)
{
    R94Fields f;
    f.Numbers(p.AffinityPercent).Numbers(p.Rating).Numbers(p.Peak).Numbers(p.LastPracticeDay).Numbers(p.DecaySettledDay);
    for (const auto& window : p.Window)
    {
        for (const PracticeDay& day : window)
            f.Number(day.Day).Number(day.Minutes).Number(day.GainCentipoints);
    }
    f.Enum(p.Grade).Number(p.SupervisedPracticeMinutes).Number(p.EarliestRetakeDay);
    return f.Value();
}

std::string WorldSaveAwakeningEncode(const WorldSaveAwakeningSlices& s)
{
    const AwakeningState& a = s.Awakening;
    R94Writer w;
    w.Literal(R94Header);
    w.Line("seeded", R94Fields().Flag(a.Seeded).Number(a.WorldSeed));
    w.Stats("", a.Stats);
    w.Line("evidence", R94Fields().Numbers(a.AttributeEvidenceMilli));
    w.Block("projection", SerialiseAptitude(a.ProjectionEvidence));
    w.Line("applied", R94Fields().Number(a.AppliedActivityInstances.size()));
    for (const std::uint64_t instance : a.AppliedActivityInstances)
        w.Line("instance", R94Fields().Number(instance));
    w.Block("conduct", SerialiseConductLedger(a.Conduct));
    w.Line("appointment", R94Fields().Number(a.AppointmentAt).Number(a.ExaminationAt));
    w.Line("makeup", R94Fields().Enum(a.MakeUp.Phase).Instant(a.MakeUp.At));
    if (a.Granted)
        R94WriteGrant(w, *a.Granted);
    else
        w.Literal("grant=none");
    const TutorialDungeonState& t = s.Tutorial;
    w.Line("tutorial", R94Fields().Enum(t.Phase).Number(static_cast<unsigned>(t.NextSituation)).Flag(t.RecoveryPending).Flag(t.Withdrawn)
                           .Instant(t.StartedAt).Instant(t.LastActionAt).Instant(t.CompletedAt));
    if (s.Countdown.empty())
        w.Literal("countdown=none");
    else
        w.Block("countdown", s.Countdown);
    if (s.Live)
        w.Line("live", R94Fields().Raw(WorldSaveAwakeningProfileText(*s.Live)));
    else
        w.Literal("live=none");
    w.Literal("end");
    return w.Take();
}

WorldSaveAwakeningSlices WorldSaveAwakeningOf(const WorldState& world)
{
    return {world.Awakening, world.Tutorial, world.CountdownText, world.LiveAwakenedProfile};
}

std::string SerialiseAwakeningState(const WorldState& world)
{
    if (!WorldSaveAwakeningValid(world))
        return {};
    return WorldSaveAwakeningEncode(WorldSaveAwakeningOf(world));
}

bool IsAwakeningStateValidInWorld(const WorldState& world)
{
    return WorldSaveAwakeningValid(world);
}

std::string WorldSaveWriteAwakening(const WorldState& world)
{
    const std::string text = SerialiseAwakeningState(world);
    return text.empty() ? std::string{} : ",\n    \"awakening\": " + WorldSaveQuoted(text);
}
} // namespace PerfectWorld
