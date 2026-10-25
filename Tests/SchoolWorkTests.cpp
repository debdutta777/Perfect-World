#include "SchoolWork.h"

#include "P2CTestSupport.h"

using namespace PerfectWorld;
int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root"); const P2CFixture f(argv[1]);
    const auto& paper = f.Papers.Papers.front();
    SchoolWorkState state;
    SchoolWorkBrief brief{"school.work.first", paper.Id, SchoolWorkKind::Homework, 0, 89,
        "phase2.test.title", "phase2.test.brief", "phase2.test.criteria", "phase2.test.recovery"};
    P2CExpect(IssueSchoolWork(state, brief, f.Papers, 0).Ok(), "homework brief issued");
    P2CExpect(!IssueSchoolWork(state, brief, f.Papers, 0).Ok(), "cannot duplicate brief");
    AssessmentSkills skills;
    for (const auto& q : paper.Questions)
        if (std::none_of(skills.begin(), skills.end(), [&](const auto& skill) { return skill.SkillId == q.SkillId; })) skills.push_back({q.SkillId, 200000});
    SchoolWorkCommand command; command.WorkId = brief.Id;
    P2CExpect(ApplySchoolWork(state, f.Papers, command, skills, 0).Ok(), "paper begins");
    command.Kind = SchoolWorkActionKind::Survey;
    P2CExpect(ApplySchoolWork(state, f.Papers, command, skills, 0).ChargedUnits == 1, "survey uses Assessment price");
    for (std::size_t i = 0; i < AssessmentGroupCount; ++i)
    {
        command.Kind = SchoolWorkActionKind::Allocate; command.Group = i; command.Units = 3;
        P2CExpect(ApplySchoolWork(state, f.Papers, command, skills, 0).Ok(), "allocation");
        command.Kind = SchoolWorkActionKind::Answer; command.Answer = {i, AssessmentMode::Work, true, false};
        const auto quote = QuoteSchoolWorkAnswer(state, brief.Id, command.Answer);
        const auto answered = ApplySchoolWork(state, f.Papers, command, skills, 0);
        if (!answered.Ok() || answered.ChargedUnits != quote.Units)
            std::cerr << "group " << i << " quote=" << quote.Units << " answer=" << answered.ChargedUnits << " code=" << answered.Issue.Code << '\n';
        P2CExpect(answered.Ok() && quote.Ok() && answered.ChargedUnits == quote.Units, "answer equals quote");
    }
    command.Kind = SchoolWorkActionKind::Submit;
    const auto sent = ApplySchoolWork(state, f.Papers, command, skills, 0);
    P2CExpect(sent.Ok() && sent.WorldMinutes == 90 && sent.Marking && sent.Marking->Total == 100, "only core awards diagnostic marks");
    const auto saved = state;
    P2CExpect(!ApplySchoolWork(state, f.Papers, command, skills, 90).Ok() && state == saved, "submission exactly once");
    const auto view = QuerySchoolAcademicRecord(state, f.Papers, 100);
    P2CExpect(view.Work.front().LateSubmission && !view.Work.front().Overdue && view.Work.front().Marking->Total == 100, "late mark disclosure never changes diagnostic achievement");
    brief.Id = "school.work.recovery"; brief.AvailableAt = 100; brief.DueAt = 110;
    P2CExpect(IssueSchoolWork(state, brief, f.Papers, 100).Ok(), "recovery brief");
    command.WorkId = brief.Id; command.Kind = SchoolWorkActionKind::Begin;
    P2CExpect(ApplySchoolWork(state, f.Papers, command, {}, 200).Ok(), "missed deadline allows recovery");
    command.Kind = SchoolWorkActionKind::Survey;
    P2CExpect(!ApplySchoolWork(state, f.Papers, command, {}, 201).Ok(), "active paper clock pinned");
    P2CExpect(ApplySchoolWork(state, f.Papers, command, {}, 200).Ok(), "recovery survey");
    command.Kind = SchoolWorkActionKind::Cancel;
    const auto cancelled = ApplySchoolWork(state, f.Papers, command, {}, 200);
    P2CExpect(cancelled.Ok() && cancelled.WorldMinutes == AssessmentUnitMinutes, "cancellation pays actual spent paper units");
    command.Kind = SchoolWorkActionKind::Begin;
    P2CExpect(ApplySchoolWork(state, f.Papers, command, {}, 205).Ok(), "restart after cancellation");
    command.Kind = SchoolWorkActionKind::Submit;
    const auto unfinished = ApplySchoolWork(state, f.Papers, command, {}, 205);
    P2CExpect(unfinished.Ok() && unfinished.Marking->Total == 0 && unfinished.Marking->Groups.front().Grade == AssessmentGrade::Unattempted,
        "unfinished work keeps diagnostic distinction");
    std::cout << "P2C SchoolWork passed\n";
}
