#include "SchoolWorkIssuance.h"

#include "SchoolWorkBriefContent.h"
#include "P2CTestSupport.h"

namespace
{
PerfectWorld::SchoolWorkState P34IssuanceSubmit(PerfectWorld::SchoolWorkState state, const PerfectWorld::AssessmentCatalogue& papers,
    const std::string& id, PerfectWorld::GameInstant at)
{
    using namespace PerfectWorld;
    AssessmentSkills skills;
    for (const auto& q : papers.Papers.front().Questions)
        if (std::none_of(skills.begin(), skills.end(), [&](const auto& s) { return s.SkillId == q.SkillId; })) skills.push_back({q.SkillId, 200000});
    SchoolWorkCommand command;
    command.WorkId = id;
    P2CExpect(ApplySchoolWork(state, papers, command, skills, at).Ok(), "P34 begin issued work");
    command.Kind = SchoolWorkActionKind::Survey;
    P2CExpect(ApplySchoolWork(state, papers, command, skills, at).Ok(), "P34 survey");
    for (std::size_t i = 0; i < AssessmentGroupCount; ++i)
    {
        command.Kind = SchoolWorkActionKind::Allocate; command.Group = i; command.Units = 3;
        P2CExpect(ApplySchoolWork(state, papers, command, skills, at).Ok(), "P34 allocate");
        command.Kind = SchoolWorkActionKind::Answer; command.Answer = {i, AssessmentMode::Work, true, false};
        const auto quote = QuoteSchoolWorkAnswer(state, id, command.Answer);
        const auto answer = ApplySchoolWork(state, papers, command, skills, at);
        P2CExpect(quote.Ok() && answer.Ok() && answer.ChargedUnits == quote.Units, "P34 core answer price");
    }
    command.Kind = SchoolWorkActionKind::Submit;
    const auto sent = ApplySchoolWork(state, papers, command, skills, at);
    P2CExpect(sent.Ok() && sent.WorldMinutes == AssessmentPaperMinutes && sent.Marking && sent.Marking->Total == 100,
        "P34 core submission and time");
    const auto finished = state;
    const auto duplicate = ApplySchoolWork(state, papers, command, skills, at + AssessmentPaperMinutes);
    P2CExpect(!duplicate.Ok() && duplicate.WorldMinutes == 0 && state == finished, "P34 duplicate submission inert");
    return state;
}
} // namespace

int main(int argc, char** argv)
{
    using namespace PerfectWorld;
    P2CExpect(argc == 2, "P34 content path");
    const P2CFixture f(argv[1]);
    const auto decoded = DecodeSchoolWorkBriefContent(f.Content);
    P2CExpect(decoded.Ok(), "P34 catalogue decoded");
    const auto& catalogue = decoded.Catalogue;
    for (const auto stream : {"science", "commerce", "humanities"})
    {
        const auto cats = f.Cats(stream);
        const auto& calendar = *cats.Day.Calendar;
        const auto& bells = *cats.Day.School;
        std::vector<SchoolWorkBriefDef> defs;
        for (const auto& def : catalogue.Briefs) if (def.Stream == stream) defs.push_back(def);
        std::sort(defs.begin(), defs.end(), [](const auto& a, const auto& b) { return a.Brief.AvailableAt < b.Brief.AvailableAt; });
        const auto at = defs.front().Brief.AvailableAt;
        const auto end = defs.back().Brief.DueAt + 1;
        const SchoolWorkState empty;
        const auto advance = [&](const SchoolWorkState& state, GameInstant from, GameInstant to) {
            return IssueScheduledSchoolWork(state, catalogue, calendar, bells, f.Papers, from, to);
        };
        P2CExpect(advance(empty, 0, at - 1).State.Records.empty(), "no issue before exact boundary");
        const auto first = advance(empty, at, at);
        P2CExpect(first.Ok() && first.IssuedIds == std::vector<std::string>{defs.front().Brief.Id} && first.State.Records.size() == 1,
            "exact boundary issues once for this stream");
        P2CExpect(empty.Records.empty(), "pure input unchanged");
        P2CExpect(first.State.Records.front().IssuedAt == at && first.State.Records.front().Brief.AvailableAt == at,
            "issuance is stamped at scheduled instant");
        const auto again = advance(first.State, at, at);
        P2CExpect(again.Ok() && again.State == first.State && again.IssuedIds.empty(), "due-now replay inert");
        const auto compressed = advance(empty, 0, end);
        P2CExpect(compressed.Ok() && compressed.State.Records.size() == defs.size() && compressed.IssuedIds.size() == defs.size(), "year compression crosses every trigger of the stream");
        SchoolWorkState stepped;
        std::vector<std::string> issued;
        GameInstant from = 0;
        for (const auto& def : defs)
        {
            const auto before = advance(stepped, from, def.Brief.AvailableAt - 1);
            P2CExpect(before.Ok() && before.IssuedIds.empty(), "gaps have no triggers");
            const auto boundary = advance(before.State, def.Brief.AvailableAt - 1, def.Brief.AvailableAt);
            P2CExpect(boundary.Ok(), "manual boundary succeeds");
            stepped = boundary.State;
            issued.insert(issued.end(), boundary.IssuedIds.begin(), boundary.IssuedIds.end());
            from = def.Brief.AvailableAt;
        }
        P2CExpect(stepped == compressed.State && issued == compressed.IssuedIds, "manual and compression give identical records and order");
        const auto overlap = advance(stepped, 0, end);
        P2CExpect(overlap.Ok() && overlap.State == stepped && overlap.IssuedIds.empty(), "overlap after reload preserves exactly-once identity");
        auto reversed = catalogue;
        std::reverse(reversed.Briefs.begin(), reversed.Briefs.end());
        const auto reordered = IssueScheduledSchoolWork(empty, reversed, calendar, bells, f.Papers, 0, end);
        P2CExpect(reordered.Ok() && reordered.State == compressed.State && reordered.IssuedIds == compressed.IssuedIds,
            "catalogue traversal order does not affect issuance");
        P2CExpect(advance(empty, at + 1, at + 1).State.Records.empty(), "no invented pre-range migration history");
        {
            auto simultaneous = catalogue;
            auto second = defs.front();
            second.Brief.Id += ".second";
            simultaneous.Briefs.insert(simultaneous.Briefs.begin(), second);
            const auto together = IssueScheduledSchoolWork(empty, simultaneous, calendar, bells, f.Papers, at, at);
            P2CExpect(together.Ok() && together.IssuedIds == std::vector<std::string>{defs.front().Brief.Id, second.Brief.Id},
                "simultaneous triggers have stable ID order");
        }
        for (const auto date : {"2042-12-05", "2043-01-23"})
        {
            auto special = defs.front();
            special.IssueDay = *ParseCalendarDate(date);
            special.TermId = CalendarTermOn(calendar, special.IssueDay)->Id;
            special.Brief.AvailableAt = CalendarDayStartInstant(special.IssueDay) + P2CBellMinute(bells, special.Period);
            special.Brief.DueAt = special.Brief.AvailableAt + MinutesPerDay;
            const auto invalid = ValidateSchoolWorkBriefDef(special, calendar, bells, f.Papers);
            P2CExpect(!invalid.Ok() && invalid.Code == "school.work.issue.no_ordinary_period", "special school days have no period issue trigger");
        }
        {
            auto shortDeadline = defs.front();
            shortDeadline.Brief.DueAt = at + AssessmentPaperMinutes - 1;
            P2CExpect(!ValidateSchoolWorkBriefDef(shortDeadline, calendar, bells, f.Papers).Ok(), "deadline allows the disclosed paper block");
            auto wrongPaper = f.Papers;
            wrongPaper.Papers.front().Units = 1;
            P2CExpect(!ValidateSchoolWorkBriefDef(defs.front(), calendar, bells, wrongPaper).Ok(), "wrong paper shape refused");
        }
        const auto due = defs.front().Brief.DueAt;
        const auto saved = first.State;
        const auto absentView = QuerySchoolAcademicRecord(saved, f.Papers, due + 1);
        P2CExpect(absentView.Work.front().CanBegin && absentView.Work.front().Overdue && saved == first.State,
            "absent student retrieves overdue work without mutation or attendance requirement");
        for (int i = 0; i < 3; ++i) (void)QuerySchoolAcademicRecord(empty, f.Papers, end);
        P2CExpect(empty.Records.empty(), "opening a panel never issues work");
        const auto onTime = P34IssuanceSubmit(saved, f.Papers, defs.front().Brief.Id, at);
        const auto late = P34IssuanceSubmit(saved, f.Papers, defs.front().Brief.Id, due + 1);
        const auto normalView = QuerySchoolAcademicRecord(onTime, f.Papers, end);
        const auto lateView = QuerySchoolAcademicRecord(late, f.Papers, end);
        P2CExpect(!normalView.Work.front().LateSubmission && lateView.Work.front().LateSubmission &&
            normalView.Work.front().Marking == lateView.Work.front().Marking, "late recovery labels lateness without changing diagnostic marks");
        P2CExpect(advance(late, at, at).State == late, "issuance replay never resets completed work");
        auto recovering = saved;
        SchoolWorkCommand cmd; cmd.WorkId = defs.front().Brief.Id;
        P2CExpect(ApplySchoolWork(recovering, f.Papers, cmd, {}, due + 1).Ok(), "late draft begins");
        cmd.Kind = SchoolWorkActionKind::Survey;
        P2CExpect(ApplySchoolWork(recovering, f.Papers, cmd, {}, due + 1).Ok(), "late draft survey");
        cmd.Kind = SchoolWorkActionKind::Cancel;
        const auto cancelled = ApplySchoolWork(recovering, f.Papers, cmd, {}, due + 1);
        P2CExpect(cancelled.Ok() && cancelled.WorldMinutes == AssessmentUnitMinutes, "late cancellation uses existing time contract");
        const auto restarted = P34IssuanceSubmit(recovering, f.Papers, cmd.WorkId, due + 1 + AssessmentUnitMinutes);
        P2CExpect(restarted.Records.size() == 1 && restarted.Records.front().Brief.DueAt == due, "recovery keeps original assignment and deadline");
        auto conflict = saved;
        ++conflict.Records.front().Brief.DueAt;
        const auto refused = advance(conflict, at, end);
        P2CExpect(!refused.Ok() && refused.State == conflict && refused.IssuedIds.empty(), "identity conflict refuses atomically");
        const auto laterConflict = advance(conflict, at + 1, at + 1);
        P2CExpect(!laterConflict.Ok() && laterConflict.Issue.Code == "school.work.issue.identity_conflict" &&
            laterConflict.State == conflict && laterConflict.IssuedIds.empty(), "issued identity stays immutable outside its trigger range");
        auto wrongTimestamp = saved;
        ++wrongTimestamp.Records.front().IssuedAt;
        const auto laterTimestamp = advance(wrongTimestamp, at + 1, end);
        P2CExpect(!laterTimestamp.Ok() && laterTimestamp.Issue.Code == "school.work.issue.identity_conflict" &&
            laterTimestamp.State == wrongTimestamp && laterTimestamp.IssuedIds.empty(), "issued timestamp conflict refuses on later advances");
        for (const auto range : {std::pair<GameInstant, GameInstant>{-1, at}, {at, at - 1}, {0, DayLoopMaxInstant + 1}})
        {
            const auto bad = advance(saved, range.first, range.second);
            P2CExpect(!bad.Ok() && bad.State == saved && bad.IssuedIds.empty(), "invalid range leaves candidate unchanged");
        }
        auto duplicates = catalogue;
        duplicates.Briefs.push_back(defs.front());
        P2CExpect(!IssueScheduledSchoolWork(saved, duplicates, calendar, bells, f.Papers, at, end).Ok(), "duplicate catalogue identity refused");
        auto badDef = catalogue;
        for (auto& def : badDef.Briefs) if (def.Brief.Id == defs.back().Brief.Id) ++def.Brief.AvailableAt;
        const auto badBoundary = IssueScheduledSchoolWork(saved, badDef, calendar, bells, f.Papers, at, end);
        P2CExpect(!badBoundary.Ok() && badBoundary.State == saved, "malformed later trigger does not partially issue");
        auto nearlyFull = saved;
        nearlyFull.Records.clear();
        for (std::size_t i = 0; i < SchoolWorkLimit - 1; ++i)
        {
            auto record = saved.Records.front();
            record.Brief.Id = "school.work.capacity_" + std::to_string(i);
            nearlyFull.Records.push_back(record);
        }
        std::sort(nearlyFull.Records.begin(), nearlyFull.Records.end(), [](const auto& a, const auto& b) { return a.Brief.Id < b.Brief.Id; });
        const auto full = advance(nearlyFull, 0, end);
        P2CExpect(!full.Ok() && full.Issue.Code == "school.work.full" && full.State == nearlyFull && full.IssuedIds.empty(),
            "capacity failure after first issuance rolls whole candidate back");
        auto malformedState = saved;
        malformedState.Records.push_back(malformedState.Records.front());
        P2CExpect(!advance(malformedState, at, end).Ok(), "noncanonical ledger refused");
    }
    std::cout << "P34 SchoolWorkIssuance passed\n";
}
