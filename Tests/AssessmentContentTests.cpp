// ROADMAP M3-07: the assessment paper content rules, each shown to refuse a bad record, and the
// catalogue loader's own refusals. The edits are made to the real Data/Content in memory, so each
// test changes exactly one thing in valid data.
#include "AssessmentTestSupport.h"

using namespace PerfectWorld;

namespace
{
const char* const PaperFile = "assessment_paper.xi_half_yearly_mixed.json";

/** What the loader says about the edited content, read against the real structure catalogues. */
std::string LoaderSays(const std::vector<Edit>& edits, const AssessmentStructures& structures)
{
    const ContentLoadResult edited = LoadEdited(TestWorld->Files, edits);
    const AssessmentCatalogueResult result = LoadAssessmentCatalogue(edited.Db, structures);
    Check(result.Ok() == result.Error.empty() && (!result.Ok() ? result.Catalogue.Papers.empty() : true),
          "a refusal leaves no half-loaded paper");
    return result.Error;
}

bool Says(const std::string& error, const char* text)
{
    return error.find(text) != std::string::npos;
}
} // namespace

void TestContentRules()
{
    const std::vector<ContentFile>& files = TestWorld->Files;
    Check(LoadContent(files).Ok(), "the unedited content is valid");

    // The shape of a paper (SCHOOL_AND_PROGRESSION "Time economy").
    Refuses(files, {{PaperFile, "\"units\": 18", "\"units\": 17"}}, "a paper is 90 minutes: 18 units of 5 minutes");
    Refuses(files, {{PaperFile, "\"minutes\": 90", "\"minutes\": 80"}}, "a paper is 90 minutes: 18 units of 5 minutes");
    Refuses(files, {{PaperFile, "\"groups\": [", "\"groups\": [ { \"id\": \"g0\", \"structure\": \"model\", \"marks\": 20, \"case\": \"problem_case.xi_bus_stand_queue\" },"}},
            "a paper has 5 question groups, found 6");
    Refuses(files, {{PaperFile, "\"id\": \"g1\", \"structure\": \"model\", \"marks\": 20", "\"id\": \"g1\", \"structure\": \"model\", \"marks\": 10"}},
            "group g1 is worth 10 marks, not 20");
    Refuses(files, {{PaperFile, "\"id\": \"g2\"", "\"id\": \"g1\""}}, "the group id g1 is used twice");
    Refuses(files, {{PaperFile, "\"structure\": \"explain\"", "\"structure\": \"essay\""}}, "unknown value");

    // Its questions are the cases of the structures.
    Refuses(files, {{PaperFile, "\"case\": \"problem_case.xi_water_tank_level\"", "\"case\": \"problem_case.xi_no_such\""}},
            "group g1 names the case problem_case.xi_no_such, which does not exist");
    Refuses(files, {{PaperFile, "\"id\": \"g2\", \"structure\": \"explain\"", "\"id\": \"g2\", \"structure\": \"model\""}},
            "group g2 names the case explain_case.xi_canal_log, a explain_case, but its structure model needs a problem_case");
    Refuses(files, {{PaperFile, "\"case\": \"problem_case.xi_roof_rainwater_tank\"", "\"case\": \"problem_case.xi_water_tank_level\""}},
            "group g5 repeats the case problem_case.xi_water_tank_level");

    // And it is sat in a real assessment window of a real calendar.
    Refuses(files, {{PaperFile, "\"window\": \"half_yearly\"", "\"window\": \"quarterly\""}}, "the calendar calendar.nabapara_2042_43 has no assessment window quarterly");
    Refuses(files, {{PaperFile, "\"calendar\": \"calendar.nabapara_2042_43\"", "\"calendar\": \"calendar.nowhere\""}}, "which does not exist");

    // The loader refuses what it cannot read, each time with no paper.
    const AssessmentStructures all = StructuresOf(TestWorld->Structures);
    Check(LoaderSays({}, all).empty(), "the unedited paper loads");
    Check(Says(LoaderSays({{PaperFile, "\"units\": 18", "\"units\": 17"}}, all), "a paper is 90 minutes: 18 units of 5 minutes"), "the loader refuses 17 units");
    Check(Says(LoaderSays({{PaperFile, "\"id\": \"g1\", \"structure\": \"model\", \"marks\": 20", "\"id\": \"g1\", \"structure\": \"model\", \"marks\": 10"}}, all),
               "group g1 is worth 10 marks, not 20"),
          "the loader refuses a group of ten marks");
    Check(Says(LoaderSays({{PaperFile, "\"id\": \"g2\"", "\"id\": \"g1\""}}, all), "the group id g1 is used twice"), "the loader refuses a repeated group ID");
    Check(Says(LoaderSays({{PaperFile, "\"groups\": [", "\"groups\": [ { \"id\": \"g0\", \"structure\": \"model\", \"marks\": 20, \"case\": \"problem_case.xi_bus_stand_queue\" },"}}, all),
               "a paper has 5 question groups, found 6"),
          "the loader refuses six groups");
    Check(Says(LoaderSays({{PaperFile, "\"case\": \"problem_case.xi_water_tank_level\"", "\"case\": \"problem_case.xi_no_such\""}}, all),
               "the case problem_case.xi_no_such is not a model case in its catalogue"),
          "the loader refuses a case its catalogue does not have");
    AssessmentStructures noExplain = all;
    noExplain.Explain = nullptr;
    Check(Says(LoaderSays({}, noExplain), "the case explain_case.xi_canal_log is not a explain case in its catalogue"), "a paper needs the catalogue of each structure it uses");
    AssessmentStructures none;
    Check(!LoaderSays({}, none).empty(), "with no catalogues no paper loads");

    // The loader reads the case's own facts.
    const AssessmentPaperDef& paper = *TestWorld->Paper;
    const ModelCaseDef* tank = TestWorld->Structures.Model.FindCase("problem_case.xi_water_tank_level");
    Check(tank != nullptr && paper.Questions[0].SkillId == "skill.academics" && paper.Questions[0].MethodKnownAtMilli == tank->MethodKnownAtMilli &&
              paper.Questions[0].FamilyId == tank->FamilyId && paper.Questions[0].Name == tank->Name,
          "a question carries its case's family, skill, method threshold and name");
    const CoordinateCaseDef* report = TestWorld->Structures.Coordinate.FindCase("coordinate_case.xi_titration_report");
    Check(report != nullptr && report->Seam && paper.Questions[3].Trap, "a coordinate question's trap is the case's seam");
    const InvestigateCaseDef* lamp = TestWorld->Structures.Investigate.FindCase("investigate_case.xi_bicycle_lamp");
    Check(lamp != nullptr && lamp->Symptom == InvestigateSymptom::Intermittent && paper.Questions[2].Trap, "an investigate question's trap is an intermittent symptom");
    const ModelCaseDef* roof = TestWorld->Structures.Model.FindCase("problem_case.xi_roof_rainwater_tank");
    Check(roof != nullptr && roof->Fault == ModelFault::None && !paper.Questions[4].Trap, "a model question's trap is a hidden fault, and this one has none");
}
