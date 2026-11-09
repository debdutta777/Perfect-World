#include "SchoolWorkIssuance.h"

#include <algorithm>
#include <set>
#include <utility>

namespace PerfectWorld
{
namespace
{
SchoolIssue P34Issue(std::string_view suffix, std::string detail = {})
{
    return {"school.work.issue." + std::string(suffix), std::move(detail)};
}
bool P34Token(std::string_view value, std::string_view prefix = {})
{
    return IsReceiptIdWellFormed(value) && value.starts_with(prefix) && value.size() > prefix.size();
}
const BellBlock* P34Period(const BellSchedule& bells, std::int32_t period)
{
    std::int32_t index = 0;
    for (const auto& block : bells.Blocks)
        if (block.Kind == BellBlockKind::Period && index++ == period) return &block;
    return nullptr;
}
} // namespace

SchoolIssue ValidateSchoolWorkBriefDef(const SchoolWorkBriefDef& def, const SchoolCalendar& calendar,
    const BellSchedule& bells, const AssessmentCatalogue& papers)
{
    const auto& b = def.Brief;
    if (!P34Token(b.Id, "school.work.") || b.Kind != SchoolWorkKind::Homework ||
        !P34Token(b.TitleTextKey, "phase3.p304.") || !P34Token(b.BriefTextKey, "phase3.p304.") ||
        !P34Token(b.CriteriaTextKey, "phase3.p304.") || !P34Token(b.RecoveryTextKey, "phase3.p304.") ||
        !P34Token(def.TermId) || !P34Token(def.Subject)) return P34Issue("bad_brief", b.Id);
    if (def.CalendarId != calendar.Id || def.BellScheduleId != bells.Id ||
        calendar.InstitutionId != bells.InstitutionId || calendar.StageId != bells.StageId ||
        def.Stream != bells.Stream || (def.Stream != "science" && def.Stream != "commerce" && def.Stream != "humanities"))
        return P34Issue("school_mismatch", b.Id);
    if (def.IssueDay < 0 || def.IssueDay > DayLoopMaxInstant / MinutesPerDay ||
        def.IssueDay < calendar.FirstDay || def.IssueDay > calendar.LastDay) return P34Issue("bad_day", b.Id);
    const auto* term = CalendarTermOn(calendar, def.IssueDay);
    if (!term || term->Id != def.TermId || def.IssueDay == calendar.AwakeningDay || !CalendarIsSchoolDay(calendar, def.IssueDay) ||
        CalendarAssessmentOn(calendar, def.IssueDay) || CalendarEventOn(calendar, def.IssueDay))
        return P34Issue("no_ordinary_period", b.Id);
    const auto* block = P34Period(bells, def.Period);
    if (!block || block->Subject != def.Subject || block->StartMinute < 0 || block->StartMinute >= MinutesPerDay)
        return P34Issue("bad_period", b.Id);
    const auto scheduled = CalendarDayStartInstant(def.IssueDay) + block->StartMinute;
    if (b.AvailableAt != scheduled || scheduled < 0 || scheduled > DayLoopMaxInstant - AssessmentPaperMinutes ||
        b.DueAt < scheduled + AssessmentPaperMinutes || b.DueAt > DayLoopMaxInstant ||
        CalendarDayOfInstant(b.DueAt) > term->Last) return P34Issue("bad_deadline", b.Id);
    const auto* paper = papers.FindPaper(b.PaperId);
    if (!paper || paper->CalendarId != calendar.Id || paper->Units != AssessmentPaperUnits ||
        paper->Questions.size() != AssessmentGroupCount ||
        std::any_of(paper->Questions.begin(), paper->Questions.end(), [](const auto& q) { return q.Marks != AssessmentGroupMarks; }))
        return P34Issue("bad_paper", b.Id);
    return {};
}

SchoolWorkIssuanceResult IssueScheduledSchoolWork(const SchoolWorkState& state,
    const SchoolWorkBriefCatalogue& catalogue, const SchoolCalendar& calendar, const BellSchedule& bells,
    const AssessmentCatalogue& papers, GameInstant from, GameInstant to)
{
    SchoolWorkIssuanceResult out{{}, state, {}};
    const auto refuse = [&](SchoolIssue issue) {
        out.Issue = std::move(issue);
        out.State = state;
        out.IssuedIds.clear();
        return out;
    };
    if (from < 0 || to < from || to > DayLoopMaxInstant) return refuse(P34Issue("bad_range"));
    if (state.Records.size() > SchoolWorkLimit) return refuse(P34Issue("invalid_state"));
    std::string previous;
    for (const auto& record : state.Records)
    {
        if (!P34Token(record.Brief.Id) || record.Brief.Id <= previous) return refuse(P34Issue("invalid_state"));
        previous = record.Brief.Id;
    }
    std::set<std::string> ids;
    std::vector<const SchoolWorkBriefDef*> pending;
    for (const auto& def : catalogue.Briefs)
    {
        if (!ids.insert(def.Brief.Id).second) return refuse(P34Issue("duplicate_id", def.Brief.Id));
        if (def.CalendarId != calendar.Id || def.Stream != bells.Stream) continue;
        const auto valid = ValidateSchoolWorkBriefDef(def, calendar, bells, papers);
        if (!valid.Ok()) return refuse(valid);
        const auto pos = std::lower_bound(state.Records.begin(), state.Records.end(), def.Brief.Id,
            [](const auto& r, std::string_view id) { return r.Brief.Id < id; });
        if (pos != state.Records.end() && pos->Brief.Id == def.Brief.Id)
        {
            if (pos->Brief != def.Brief || pos->IssuedAt != def.Brief.AvailableAt)
                return refuse(P34Issue("identity_conflict", def.Brief.Id));
        }
        else if (def.Brief.AvailableAt >= from && def.Brief.AvailableAt <= to) pending.push_back(&def);
    }
    std::sort(pending.begin(), pending.end(), [](const auto* a, const auto* b) {
        return a->Brief.AvailableAt != b->Brief.AvailableAt ? a->Brief.AvailableAt < b->Brief.AvailableAt : a->Brief.Id < b->Brief.Id;
    });
    for (const auto* def : pending)
    {
        const auto& brief = def->Brief;
        const auto issued = IssueSchoolWork(out.State, brief, papers, brief.AvailableAt);
        if (!issued.Ok()) return refuse(issued);
        out.IssuedIds.push_back(brief.Id);
    }
    return out;
}
} // namespace PerfectWorld
