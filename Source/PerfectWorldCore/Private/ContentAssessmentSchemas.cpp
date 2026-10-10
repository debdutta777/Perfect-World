#include "Assessment.h"
#include "ContentSchemaBuilders.h"
#include "ContentStructureChecks.h"

#include <set>

// The assessment paper record (ROADMAP M3-07; SCHOOL_AND_PROGRESSION "Time economy": "a
// 90-minute paper is eighteen five-minute units. Five groups worth twenty marks each."). One
// file per paper under Data/Content/assessment_paper. A paper names the calendar assessment
// window it is sat in and five question groups, each pointing at one existing case of the
// structure the group names, so the examination re-asks questions the classroom has taught.
// The numbers are the runner's constants (Assessment.h), so the data and the code cannot
// drift apart.
namespace PerfectWorld::ContentSchema
{
using namespace Build;
using namespace StructureChecks;

namespace
{
std::vector<std::string> AssessmentSchemaStructures()
{
    return std::vector<std::string>(AssessmentStructureNames.begin(), AssessmentStructureNames.end());
}

const TypeSchema& AssessmentSchemaGroup()
{
    static const TypeSchema s{"assessment_group", "", {
        Str("id", true), OneOf("structure", true, AssessmentSchemaStructures()), Int("marks", true, 1, 100), Str("case", true),
        Str("note", false)}};
    return s;
}

void AssessmentSchemaCheckGroup(const ContentDb& db, const ContentRecord& paper, const JsonValue& group, std::set<std::string>& groupIds,
                                std::set<std::string>& caseIds, std::vector<ContentIssue>& issues)
{
    const std::string id = StrOf(&group, "id");
    const std::string structure = StrOf(&group, "structure");
    const std::string caseId = StrOf(&group, "case");
    if (IntOf(&group, "marks") != AssessmentGroupMarks)
        Report(issues, paper, "group " + id + " is worth " + std::to_string(IntOf(&group, "marks")) + " marks, not " +
                                  std::to_string(AssessmentGroupMarks));
    if (!groupIds.insert(id).second)
        Report(issues, paper, "the group id " + id + " is used twice");
    if (!caseIds.insert(caseId).second)
        Report(issues, paper, "group " + id + " repeats the case " + caseId);

    const ContentRecord* target = db.Find(caseId);
    if (target == nullptr)
    {
        Report(issues, paper, "group " + id + " names the case " + caseId + ", which does not exist");
        return;
    }
    for (std::size_t i = 0; i < AssessmentStructureNames.size(); ++i)
    {
        if (AssessmentStructureNames[i] == structure && target->Type != AssessmentCaseTypes[i])
            Report(issues, paper, "group " + id + " names the case " + caseId + ", a " + target->Type + ", but its structure " + structure +
                                      " needs a " + std::string(AssessmentCaseTypes[i]));
    }
}

void AssessmentSchemaCheckPaper(const ContentDb& db, const ContentRecord& paper, std::vector<ContentIssue>& issues)
{
    if (IntOf(&paper.Data, "units") != AssessmentPaperUnits || IntOf(&paper.Data, "minutes") != AssessmentPaperMinutes)
        Report(issues, paper, "a paper is " + std::to_string(AssessmentPaperMinutes) + " minutes: " + std::to_string(AssessmentPaperUnits) +
                                  " units of " + std::to_string(AssessmentUnitMinutes) + " minutes");
    const std::vector<JsonValue>& groups = ItemsOf(&paper.Data, "groups");
    if (groups.size() != AssessmentGroupCount)
        Report(issues, paper, "a paper has " + std::to_string(AssessmentGroupCount) + " question groups, found " + std::to_string(groups.size()));
    std::set<std::string> groupIds;
    std::set<std::string> caseIds;
    for (const JsonValue& group : groups)
        AssessmentSchemaCheckGroup(db, paper, group, groupIds, caseIds, issues);

    // The window is one of the calendar's own assessment windows.
    const std::string calendarId = StrOf(&paper.Data, "calendar");
    const std::string window = StrOf(&paper.Data, "window");
    const ContentRecord* calendar = db.Find(calendarId);
    if (calendar == nullptr)
        return;
    bool found = false;
    for (const JsonValue& entry : ItemsOf(&calendar->Data, "assessments"))
        found = found || StrOf(&entry, "id") == window;
    if (!found)
        Report(issues, paper, "the calendar " + calendarId + " has no assessment window " + window);
}
} // namespace

const TypeSchema& AssessmentPaperSchema()
{
    static const TypeSchema s{"assessment_paper", "assessment_paper.", Base({
        Str("name", true), Ref("calendar", true, "calendar"), Str("window", true), Int("minutes", true, 1, 600),
        Int("units", true, 1, 120), Obj("groups", true, &AssessmentSchemaGroup(), true)})};
    return s;
}

void CheckAssessmentSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    for (const ContentRecord* paper : db.OfType("assessment_paper"))
        AssessmentSchemaCheckPaper(db, *paper, issues);
}
} // namespace PerfectWorld::ContentSchema
