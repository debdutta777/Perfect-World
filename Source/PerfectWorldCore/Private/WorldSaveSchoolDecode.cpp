#include "WorldSaveInternal.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// S04: reading the pw.school/1 envelope (format in WorldSave.h; written by WorldSaveSchool.cpp) and the empty school slices
// every earlier world migrates to. Only the canonical spelling is accepted: a decoded state is re-encoded and must give back
// the exact text, and it must keep WorldSaveSchoolRules.cpp's rules inside the world being read.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view S04DHeader = "pw.school/1";
constexpr std::size_t S04DMaxFields = 16;

bool S04DSafe(std::string_view text)
{
    return WorldSaveSchoolSafeText(text);
}

bool S04DMode(std::string_view text, SchoolLessonMode& out)
{
    for (std::size_t index = 0; index < std::size(WorldSaveSchoolModeNames); ++index)
    {
        if (text == WorldSaveSchoolModeNames[index])
        {
            out = static_cast<SchoolLessonMode>(index);
            return true;
        }
    }
    return false;
}

bool S04DStructure(std::string_view text, SchoolStructure& out)
{
    for (std::size_t index = 0; index < AssessmentStructureNames.size(); ++index)
    {
        if (text == AssessmentStructureNames[index])
        {
            out = static_cast<SchoolStructure>(index);
            return true;
        }
    }
    return false;
}

template <typename Grade>
bool S04DGrade(std::string_view text, Grade& out)
{
    for (std::size_t index = 0; index < std::size(WorldSaveSchoolGradeNames); ++index)
    {
        if (text == WorldSaveSchoolGradeNames[index])
        {
            out = static_cast<Grade>(index);
            return true;
        }
    }
    return false;
}

bool S04DLine(std::string_view& text, std::string_view& line)
{
    const std::size_t end = text.find('\n');
    if (end == std::string_view::npos)
        return false;
    line = text.substr(0, end);
    text.remove_prefix(end + 1);
    return true;
}

bool S04DKeyed(std::string_view& text, std::string_view key, std::string_view& value)
{
    std::string_view line;
    if (!S04DLine(text, line) || line.size() <= key.size() || !line.starts_with(key) || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

bool S04DSplit(std::string_view value, std::vector<std::string_view>& fields)
{
    fields.clear();
    while (fields.size() <= S04DMaxFields)
    {
        const std::size_t comma = value.find(',');
        fields.push_back(value.substr(0, comma));
        if (comma == std::string_view::npos)
            return true;
        value.remove_prefix(comma + 1);
    }
    return false;
}

/** A whole decimal number; canonical spelling is enforced by the final re-encoding comparison. */
template <typename T>
bool S04DNumber(std::string_view text, T& out)
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

bool S04DFlag(std::string_view text, bool& out)
{
    if (text != "0" && text != "1")
        return false;
    out = text == "1";
    return true;
}

bool S04DInstant(std::string_view text, std::optional<GameInstant>& out)
{
    if (text.empty())
    {
        out.reset();
        return true;
    }
    GameInstant value = 0;
    if (!S04DNumber(text, value))
        return false;
    out = value;
    return true;
}

template <typename T>
bool S04DKeyedNumber(std::string_view& text, std::string_view key, T& out)
{
    std::string_view value;
    return S04DKeyed(text, key, value) && S04DNumber(value, out);
}

/** "<key>=<length>" then exactly that many bytes of a native envelope. */
bool S04DBlock(std::string_view& text, std::string_view key, std::string_view& block)
{
    std::size_t length = 0;
    if (!S04DKeyedNumber(text, key, length) || length > text.size())
        return false;
    block = text.substr(0, length);
    text.remove_prefix(length);
    return true;
}

bool S04DCommon(const std::vector<std::string_view>& f, StructureOutcome& o)
{
    o.FamilyId = std::string(f[1]);
    o.CaseId = std::string(f[2]);
    return S04DSafe(f[1]) && S04DSafe(f[2]) && S04DGrade(f[3], o.Grade) && S04DNumber(f[4], o.Failures) &&
           S04DFlag(f[5], o.Foreground) && S04DNumber(f[6], o.LearningPermille) && S04DNumber(f[7], o.UnitsSpent) &&
           S04DFlag(f[8], o.BecameSecure);
}

bool S04DOutcome(std::string_view value, SchoolLessonOutcome& out)
{
    std::vector<std::string_view> f;
    if (!S04DSplit(value, f))
        return false;
    if (f[0] == "none")
    {
        out = std::monostate{};
        return f.size() == 1;
    }
    if (f[0] == "model" && f.size() == 11)
    {
        ModelOutcome m;
        m.FamilyId = std::string(f[1]);
        m.CaseId = std::string(f[2]);
        const bool ok = S04DSafe(f[1]) && S04DSafe(f[2]) && S04DGrade(f[3], m.Grade) && S04DNumber(f[4], m.Failures) &&
                        S04DFlag(f[5], m.Foreground) && S04DNumber(f[6], m.LearningPermille) && S04DNumber(f[7], m.UnitsSpent) &&
                        S04DNumber(f[8], m.MissingLinks) && S04DNumber(f[9], m.ExtraLinks) && S04DFlag(f[10], m.BecameSecure);
        out = std::move(m);
        return ok;
    }
    if (f[0] == "explain" && f.size() == 12)
    {
        ExplainOutcome e;
        const bool ok = S04DCommon(f, e) && S04DNumber(f[9], e.PlacedItems) && S04DNumber(f[10], e.SupportingItems) &&
                        S04DFlag(f[11], e.Bounded);
        out = std::move(e);
        return ok;
    }
    if (f[0] == "investigate" && f.size() == 13)
    {
        InvestigateOutcome i;
        i.Diagnosed = std::string(f[9]);
        const bool ok = S04DCommon(f, i) && S04DSafe(f[9]) && S04DNumber(f[10], i.TestsRun) && S04DNumber(f[11], i.RedundantTests) &&
                        S04DNumber(f[12], i.RemainingCauses);
        out = std::move(i);
        return ok;
    }
    if (f[0] == "coordinate" && f.size() == 13)
    {
        CoordinateOutcome c;
        const bool ok = S04DCommon(f, c) && S04DNumber(f[9], c.OverloadedMembers) && S04DNumber(f[10], c.MismatchedTasks) &&
                        S04DNumber(f[11], c.MissedFacts) && S04DNumber(f[12], c.IdleConversations);
        out = std::move(c);
        return ok;
    }
    if (f[0] == "routine" && f.size() == 9)
    {
        StructureOutcome r;
        const bool ok = S04DCommon(f, r);
        out = std::move(r);
        return ok;
    }
    return false;
}

bool S04DBinding(std::string_view value, SchoolLessonBinding& b)
{
    std::vector<std::string_view> f;
    if (!S04DSplit(value, f) || f.size() != 9)
        return false;
    for (const std::size_t index : {0u, 1u, 2u, 5u, 6u, 7u, 8u})
    {
        if (!S04DSafe(f[index]))
            return false;
    }
    b.Id = std::string(f[0]);
    b.Stream = std::string(f[1]);
    b.Subject = std::string(f[2]);
    b.FamilyId = std::string(f[5]);
    b.TitleTextKey = std::string(f[6]);
    b.BriefTextKey = std::string(f[7]);
    b.RoleTextKey = std::string(f[8]);
    return S04DNumber(f[3], b.Period) && S04DStructure(f[4], b.Structure);
}

bool S04DSessions(std::string_view& text, SchoolSessionsState& s)
{
    std::string_view line;
    std::string_view value;
    std::vector<std::string_view> f;
    std::size_t count = 0;
    if (!S04DLine(text, line) || line != S04DHeader || !S04DKeyedNumber(text, "next_session", s.NextSession) ||
        !S04DKeyed(text, "active", value))
        return false;
    if (value != "none")
    {
        SchoolLessonRun run;
        if (!S04DSplit(value, f) || f.size() != 4 || !S04DNumber(f[0], run.Session) || !S04DMode(f[1], run.Mode) ||
            !S04DNumber(f[2], run.OpenedAt) || !S04DFlag(f[3], run.ReadyToClose) || !S04DKeyed(text, "binding", value) ||
            !S04DBinding(value, run.Binding) || !S04DKeyed(text, "outcome", value) || !S04DOutcome(value, run.Outcome))
            return false;
        s.Active = std::move(run);
    }
    if (!S04DKeyedNumber(text, "periods", count) || count > SchoolPeriodHistoryLimit)
        return false;
    s.Periods.resize(count);
    for (SchoolPeriodRecord& r : s.Periods)
    {
        if (!S04DKeyed(text, "period", value) || !S04DSplit(value, f) || f.size() != 7 || !S04DNumber(f[0], r.Day) ||
            !S04DNumber(f[1], r.Period) || !S04DSafe(f[2]) || !S04DMode(f[3], r.Mode) || !S04DFlag(f[4], r.Completed) ||
            !S04DFlag(f[5], r.Cancelled) || !S04DNumber(f[6], r.ActivityInstance) || !S04DKeyed(text, "outcome", value) ||
            !S04DOutcome(value, r.Outcome))
            return false;
        r.BindingId = std::string(f[2]);
    }
    std::string_view native;
    if (!S04DBlock(text, "model", native))
        return false;
    StructureModelParse model = DeserialiseStructureModel(native);
    if (!model.Ok() || !S04DBlock(text, "explain", native))
        return false;
    ExplainParse explain = DeserialiseStructureExplain(native);
    if (!explain.Ok() || !S04DBlock(text, "investigate", native))
        return false;
    InvestigateParse investigate = DeserialiseStructureInvestigate(native);
    if (!investigate.Ok() || !S04DBlock(text, "coordinate", native))
        return false;
    CoordinateParse coordinate = DeserialiseStructureCoordinate(native);
    if (!coordinate.Ok())
        return false;
    s.Model = std::move(model.State);
    s.Explain = std::move(explain.State);
    s.Investigate = std::move(investigate.State);
    s.Coordinate = std::move(coordinate.State);
    return true;
}

bool S04DWork(std::string_view& text, SchoolWorkState& w)
{
    std::string_view value;
    std::string_view native;
    std::vector<std::string_view> f;
    std::size_t count = 0;
    if (!S04DKeyedNumber(text, "work", count) || count > SchoolWorkLimit)
        return false;
    w.Records.resize(count);
    for (SchoolWorkRecord& r : w.Records)
    {
        SchoolWorkBrief& b = r.Brief;
        if (!S04DKeyed(text, "brief", value) || !S04DSplit(value, f) || f.size() != 9)
            return false;
        for (const std::size_t index : {0u, 1u, 5u, 6u, 7u, 8u})
        {
            if (!S04DSafe(f[index]))
                return false;
        }
        b.Id = std::string(f[0]);
        b.PaperId = std::string(f[1]);
        b.TitleTextKey = std::string(f[5]);
        b.BriefTextKey = std::string(f[6]);
        b.CriteriaTextKey = std::string(f[7]);
        b.RecoveryTextKey = std::string(f[8]);
        if (f[2] == "homework")
            b.Kind = SchoolWorkKind::Homework;
        else if (f[2] == "academic_assessment")
            b.Kind = SchoolWorkKind::AcademicAssessment;
        else
            return false;
        if (!S04DNumber(f[3], b.AvailableAt) || !S04DNumber(f[4], b.DueAt) || !S04DKeyed(text, "times", value) || !S04DSplit(value, f) ||
            f.size() != 3 || !S04DNumber(f[0], r.IssuedAt) || !S04DInstant(f[1], r.StartedAt) || !S04DInstant(f[2], r.SubmittedAt) ||
            !S04DBlock(text, "paper", native))
            return false;
        AssessmentParse paper = DeserialiseAssessment(native);
        if (!paper.Ok())
            return false;
        r.Paper = std::move(paper.State);
    }
    return true;
}

bool S04DWeek(std::string_view& text, SchoolWeekState& k)
{
    std::string_view value;
    std::vector<std::string_view> f;
    std::size_t count = 0;
    if (!S04DKeyedNumber(text, "orders", count) || count > 7)
        return false;
    k.Orders.resize(count);
    for (SchoolStandingOrder& o : k.Orders)
    {
        if (!S04DKeyed(text, "order", value) || !S04DSplit(value, f) || f.size() != 4 || !S04DSafe(f[0]) ||
            !S04DNumber(f[1], o.Weekdays) || !S04DFlag(f[2], o.AttendSchool) || !S04DMode(f[3], o.LessonMode))
            return false;
        o.Id = std::string(f[0]);
    }
    if (!S04DKeyedNumber(text, "scene_days", count) || count > 400)
        return false;
    k.SceneDays.resize(count);
    for (std::int64_t& day : k.SceneDays)
    {
        if (!S04DKeyedNumber(text, "scene_day", day))
            return false;
    }
    return true;
}
} // namespace

bool DeserialiseSchoolState(std::string_view text, WorldState& world)
{
    const std::string_view original = text;
    SchoolSessionsState sessions;
    SchoolWorkState work;
    SchoolWeekState week;
    if (!S04DSessions(text, sessions) || !S04DWork(text, work) || !S04DWeek(text, week) || text != "end\n" ||
        !WorldSaveSchoolValid(sessions, work, week, world) || WorldSaveSchoolEncode(sessions, work, week) != original)
        return false;
    world.SchoolSessions = std::move(sessions);
    world.SchoolWork = std::move(work);
    world.SchoolWeek = std::move(week);
    return true;
}

WorldSaveResult WorldSaveReadSchool(const JsonValue& value, WorldState& world, std::int32_t version)
{
    if (version <= WorldSaveLastWithoutSchoolVersion)
    {
        // No lesson, paper or standing order is inferred from attendance, receipts or anything else an older world kept.
        world.SchoolSessions = {};
        world.SchoolWork = {};
        world.SchoolWeek = {};
        return {};
    }
    const JsonValue* school = value.Find("school");
    if (school == nullptr || !school->IsString())
        return WorldSaveInvalid("the school section has the wrong type.");
    if (!DeserialiseSchoolState(school->String, world))
        return WorldSaveInvalid("the lessons, papers or standing orders are damaged or do not fit this world.");
    return {};
}
} // namespace PerfectWorld
