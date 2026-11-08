#include "WorldSaveInternal.h"

#include <string>
#include <utility>

// S04: writing the pw.school/1 envelope (format in WorldSave.h) and its place in the world save (schema 11). Reading is in
// WorldSaveSchoolDecode.cpp; the semantic rules a state must keep to be written or read are in WorldSaveSchoolRules.cpp.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view S04Header = "pw.school/1";

std::string_view S04ModeName(SchoolLessonMode mode)
{
    const auto index = static_cast<std::size_t>(mode);
    return index < std::size(WorldSaveSchoolModeNames) ? WorldSaveSchoolModeNames[index] : std::string_view{};
}

std::string_view S04StructureName(SchoolStructure structure)
{
    const auto index = static_cast<std::size_t>(structure);
    return index < AssessmentStructureNames.size() ? AssessmentStructureNames[index] : std::string_view{};
}

std::string_view S04KindName(SchoolWorkKind kind)
{
    return kind == SchoolWorkKind::Homework ? "homework" : kind == SchoolWorkKind::AcademicAssessment ? "academic_assessment" : "";
}

template <typename Grade>
std::string_view S04GradeName(Grade grade)
{
    const auto index = static_cast<std::size_t>(grade);
    return index < std::size(WorldSaveSchoolGradeNames) ? WorldSaveSchoolGradeNames[index] : std::string_view{};
}

/** Builds comma-joined fields; any unwritable text field or enum spoils the whole envelope. */
class S04Fields
{
public:
    S04Fields& Text(std::string_view value)
    {
        Ok_ = Ok_ && WorldSaveSchoolSafeText(value);
        return Raw(value);
    }
    S04Fields& Name(std::string_view value)
    {
        Ok_ = Ok_ && !value.empty();
        return Raw(value);
    }
    template <typename T>
    S04Fields& Number(T value)
    {
        return Raw(std::to_string(value));
    }
    S04Fields& Flag(bool value) { return Raw(value ? "1" : "0"); }
    S04Fields& Instant(const std::optional<GameInstant>& value) { return value ? Number(*value) : Raw(""); }
    /** A literal the writer itself chose (tags); never player or content text. */
    S04Fields& Raw(std::string_view value)
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

void S04Common(S04Fields& f, const StructureOutcome& o)
{
    f.Text(o.FamilyId).Text(o.CaseId).Name(S04GradeName(o.Grade)).Number(static_cast<unsigned>(o.Failures)).Flag(o.Foreground)
        .Number(o.LearningPermille).Number(o.UnitsSpent).Flag(o.BecameSecure);
}

S04Fields S04EncodeOutcome(const SchoolLessonOutcome& outcome)
{
    S04Fields f;
    if (const auto* m = std::get_if<ModelOutcome>(&outcome))
        f.Raw("model").Text(m->FamilyId).Text(m->CaseId).Name(S04GradeName(m->Grade)).Number(static_cast<unsigned>(m->Failures))
            .Flag(m->Foreground).Number(m->LearningPermille).Number(m->UnitsSpent).Number(m->MissingLinks).Number(m->ExtraLinks)
            .Flag(m->BecameSecure);
    else if (const auto* e = std::get_if<ExplainOutcome>(&outcome))
    {
        S04Common(f.Raw("explain"), *e);
        f.Number(e->PlacedItems).Number(e->SupportingItems).Flag(e->Bounded);
    }
    else if (const auto* i = std::get_if<InvestigateOutcome>(&outcome))
    {
        S04Common(f.Raw("investigate"), *i);
        f.Text(i->Diagnosed).Number(i->TestsRun).Number(i->RedundantTests).Number(i->RemainingCauses);
    }
    else if (const auto* c = std::get_if<CoordinateOutcome>(&outcome))
    {
        S04Common(f.Raw("coordinate"), *c);
        f.Number(c->OverloadedMembers).Number(c->MismatchedTasks).Number(c->MissedFacts).Number(c->IdleConversations);
    }
    else if (const auto* r = std::get_if<StructureOutcome>(&outcome))
        S04Common(f.Raw("routine"), *r);
    else
        f.Raw("none");
    return f;
}
} // namespace

bool WorldSaveSchoolSafeText(std::string_view text)
{
    for (const char c : text)
    {
        if (c <= ' ' || c > '~' || c == ',')
            return false;
    }
    return true;
}

std::string WorldSaveSchoolEncode(const SchoolSessionsState& s, const SchoolWorkState& w, const SchoolWeekState& k)
{
    bool ok = true;
    std::string out = std::string(S04Header) + "\n";
    // Every line ends in a newline; a block is its length line followed by exactly that many bytes of native text.
    const auto line = [&](std::string_view key, const S04Fields& fields) {
        ok = ok && fields.Ok();
        out += std::string(key) + "=" + fields.Value() + "\n";
    };
    const auto block = [&](std::string_view key, const std::string& native) {
        ok = ok && !native.empty();
        out += std::string(key) + "=" + std::to_string(native.size()) + "\n" + native;
    };
    line("next_session", S04Fields().Number(s.NextSession));
    if (!s.Active)
        line("active", S04Fields().Raw("none"));
    else
    {
        const SchoolLessonRun& run = *s.Active;
        const SchoolLessonBinding& b = run.Binding;
        line("active", S04Fields().Number(run.Session).Name(S04ModeName(run.Mode)).Number(run.OpenedAt).Flag(run.ReadyToClose));
        line("binding", S04Fields().Text(b.Id).Text(b.Stream).Text(b.Subject).Number(b.Period).Name(S04StructureName(b.Structure))
                            .Text(b.FamilyId).Text(b.TitleTextKey).Text(b.BriefTextKey).Text(b.RoleTextKey));
        line("outcome", S04EncodeOutcome(run.Outcome));
    }
    line("periods", S04Fields().Number(s.Periods.size()));
    for (const SchoolPeriodRecord& r : s.Periods)
    {
        line("period", S04Fields().Number(r.Day).Number(r.Period).Text(r.BindingId).Name(S04ModeName(r.Mode)).Flag(r.Completed)
                           .Flag(r.Cancelled).Number(r.ActivityInstance));
        line("outcome", S04EncodeOutcome(r.Outcome));
    }
    block("model", SerialiseStructureModel(s.Model));
    block("explain", SerialiseStructureExplain(s.Explain));
    block("investigate", SerialiseStructureInvestigate(s.Investigate));
    block("coordinate", SerialiseStructureCoordinate(s.Coordinate));
    line("work", S04Fields().Number(w.Records.size()));
    for (const SchoolWorkRecord& r : w.Records)
    {
        const SchoolWorkBrief& b = r.Brief;
        line("brief", S04Fields().Text(b.Id).Text(b.PaperId).Name(S04KindName(b.Kind)).Number(b.AvailableAt).Number(b.DueAt)
                          .Text(b.TitleTextKey).Text(b.BriefTextKey).Text(b.CriteriaTextKey).Text(b.RecoveryTextKey));
        line("times", S04Fields().Number(r.IssuedAt).Instant(r.StartedAt).Instant(r.SubmittedAt));
        block("paper", SerialiseAssessment(r.Paper));
    }
    line("orders", S04Fields().Number(k.Orders.size()));
    for (const SchoolStandingOrder& o : k.Orders)
        line("order", S04Fields().Text(o.Id).Number(static_cast<unsigned>(o.Weekdays)).Flag(o.AttendSchool).Name(S04ModeName(o.LessonMode)));
    line("scene_days", S04Fields().Number(k.SceneDays.size()));
    for (const std::int64_t day : k.SceneDays)
        line("scene_day", S04Fields().Number(day));
    out += "end\n";
    return ok ? out : std::string{};
}

std::string SerialiseSchoolState(const WorldState& world)
{
    if (!WorldSaveSchoolValid(world.SchoolSessions, world.SchoolWork, world.SchoolWeek, world))
        return {};
    return WorldSaveSchoolEncode(world.SchoolSessions, world.SchoolWork, world.SchoolWeek);
}

bool IsSchoolStateValidInWorld(const WorldState& world)
{
    return WorldSaveSchoolValid(world.SchoolSessions, world.SchoolWork, world.SchoolWeek, world);
}

std::string WorldSaveWriteSchool(const WorldState& world)
{
    const std::string text = SerialiseSchoolState(world);
    return text.empty() ? std::string{} : ",\n    \"school\": " + WorldSaveQuoted(text);
}
} // namespace PerfectWorld
