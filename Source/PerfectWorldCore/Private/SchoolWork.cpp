#include "SchoolWork.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
SchoolIssue P2CWorkIssue(std::string_view code) { return {"school.work." + std::string(code), {}}; }
SchoolIssue P2CAssessmentIssue(AssessmentError error)
{
    return error == AssessmentError::None ? SchoolIssue{} : SchoolIssue{std::string(AssessmentErrorCode(error)), {}};
}
bool P2CPaperShape(const AssessmentPaperDef& p)
{
    return p.Units == AssessmentPaperUnits && p.Questions.size() == AssessmentGroupCount &&
        std::all_of(p.Questions.begin(), p.Questions.end(), [](const auto& q) { return q.Marks == AssessmentGroupMarks; });
}
} // namespace

SchoolIssue IssueSchoolWork(SchoolWorkState& state, const SchoolWorkBrief& b, const AssessmentCatalogue& papers, GameInstant now)
{
    if (now < 0 || now > DayLoopMaxInstant || b.AvailableAt < now || b.DueAt < b.AvailableAt || b.DueAt > DayLoopMaxInstant ||
        !IsReceiptIdWellFormed(b.Id) || !IsReceiptIdWellFormed(b.TitleTextKey) || !IsReceiptIdWellFormed(b.BriefTextKey) ||
        !IsReceiptIdWellFormed(b.CriteriaTextKey) || !IsReceiptIdWellFormed(b.RecoveryTextKey) ||
        (b.Kind != SchoolWorkKind::Homework && b.Kind != SchoolWorkKind::AcademicAssessment)) return P2CWorkIssue("bad_brief");
    const auto* paper = papers.FindPaper(b.PaperId);
    if (!paper || !P2CPaperShape(*paper)) return P2CWorkIssue("unknown_paper");
    if (state.Records.size() >= SchoolWorkLimit) return P2CWorkIssue("full");
    const auto pos = std::lower_bound(state.Records.begin(), state.Records.end(), b.Id,
        [](const auto& r, std::string_view id) { return r.Brief.Id < id; });
    if (pos != state.Records.end() && pos->Brief.Id == b.Id) return P2CWorkIssue("already_issued");
    state.Records.insert(pos, {b, now, {}, {}, {}});
    return {};
}

SchoolWorkResult ApplySchoolWork(SchoolWorkState& state, const AssessmentCatalogue& papers, const SchoolWorkCommand& command,
    const AssessmentSkills& skills, GameInstant now)
{
    SchoolWorkResult result;
    auto next = state;
    const auto pos = std::find_if(next.Records.begin(), next.Records.end(), [&](const auto& r) { return r.Brief.Id == command.WorkId; });
    if (pos == next.Records.end()) { result.Issue = P2CWorkIssue("unknown_work"); return result; }
    auto& r = *pos;
    const auto* paper = papers.FindPaper(r.Brief.PaperId);
    if (!paper || !P2CPaperShape(*paper)) { result.Issue = P2CWorkIssue("unknown_paper"); return result; }
    if (now < r.IssuedAt || now < r.Brief.AvailableAt || now > DayLoopMaxInstant - AssessmentPaperMinutes)
        { result.Issue = P2CWorkIssue("unavailable"); return result; }
    if (r.SubmittedAt) { result.Issue = P2CWorkIssue("already_submitted"); return result; }
    if (r.StartedAt && *r.StartedAt != now) { result.Issue = P2CWorkIssue("clock_changed"); return result; }
    switch (command.Kind)
    {
    case SchoolWorkActionKind::Begin:
        if (!r.Paper.PaperId.empty() || std::any_of(next.Records.begin(), next.Records.end(),
            [&](const auto& other) { return !other.SubmittedAt && !other.Paper.PaperId.empty(); }))
            { result.Issue = P2CWorkIssue("busy"); break; }
        result.Issue = P2CAssessmentIssue(BeginAssessment(r.Paper, *paper, skills));
        if (result.Issue.Ok()) r.StartedAt = now;
        break;
    case SchoolWorkActionKind::Survey:
    {
        const auto applied = SurveyAssessment(r.Paper, *paper);
        result.Issue = P2CAssessmentIssue(applied.Error);
        result.ChargedUnits = applied.Charged;
        break;
    }
    case SchoolWorkActionKind::Allocate: result.Issue = P2CAssessmentIssue(AllocateAssessmentUnits(r.Paper, command.Group, command.Units)); break;
    case SchoolWorkActionKind::Answer:
    {
        const auto applied = ApplyAssessmentAnswer(r.Paper, command.Answer);
        result.Issue = P2CAssessmentIssue(applied.Error);
        result.ChargedUnits = applied.Charged;
        break;
    }
    case SchoolWorkActionKind::Submit:
    {
        if (!r.Paper.Finished) result.Issue = P2CAssessmentIssue(FinishAssessment(r.Paper));
        if (!result.Issue.Ok()) break;
        auto marking = MarkAssessment(*paper, r.Paper);
        result.Issue = P2CAssessmentIssue(marking.Error);
        if (result.Issue.Ok())
        {
            r.SubmittedAt = now + AssessmentPaperMinutes;
            result.Marking = std::move(marking);
            result.WorldMinutes = AssessmentPaperMinutes;
        }
        break;
    }
    case SchoolWorkActionKind::Cancel:
        if (!r.StartedAt) { result.Issue = P2CWorkIssue("not_started"); break; }
        result.WorldMinutes = (AssessmentPaperUnits - r.Paper.UnitsLeft) * AssessmentUnitMinutes;
        result.Issue = P2CAssessmentIssue(CancelAssessment(r.Paper));
        if (result.Issue.Ok()) r.StartedAt.reset();
        break;
    default: result.Issue = P2CWorkIssue("bad_command"); break;
    }
    if (result.Ok()) state = std::move(next);
    return result;
}

AssessmentQuote QuoteSchoolWorkAnswer(const SchoolWorkState& state, std::string_view id, const AssessmentAnswer& answer)
{
    for (const auto& r : state.Records)
        if (r.Brief.Id == id && !r.SubmittedAt) return QuoteAssessmentAnswer(r.Paper, answer);
    AssessmentQuote q;
    q.Error = AssessmentError::NotActive;
    return q;
}

SchoolAcademicRecordView QuerySchoolAcademicRecord(const SchoolWorkState& state, const AssessmentCatalogue& papers, GameInstant now)
{
    SchoolAcademicRecordView view;
    const bool busy = std::any_of(state.Records.begin(), state.Records.end(),
        [](const auto& r) { return !r.SubmittedAt && !r.Paper.PaperId.empty(); });
    for (const auto& r : state.Records)
    {
        SchoolWorkView line;
        line.Brief = r.Brief;
        line.Available = now >= r.Brief.AvailableAt;
        line.Overdue = !r.SubmittedAt && now > r.Brief.DueAt;
        line.LateSubmission = r.SubmittedAt && *r.SubmittedAt > r.Brief.DueAt;
        line.CanBegin = line.Available && !r.SubmittedAt && !busy;
        line.CanSubmit = !r.SubmittedAt && !r.Paper.PaperId.empty();
        line.Paper = r.Paper;
        if (const auto* p = papers.FindPaper(r.Brief.PaperId))
        {
            line.Survey = AssessmentSurveyLines(r.Paper, *p);
            if (r.SubmittedAt) line.Marking = MarkAssessment(*p, r.Paper);
        }
        line.StatusTextKey = r.SubmittedAt ? "school.work.submitted" : line.Overdue ? "school.work.overdue" : "school.work.pending";
        view.Work.push_back(std::move(line));
    }
    return view;
}
} // namespace PerfectWorld
