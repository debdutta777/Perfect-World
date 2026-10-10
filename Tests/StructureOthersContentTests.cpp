// ROADMAP M2-09: the explain, investigate and coordinate record rules, each shown to refuse a
// bad record, and the catalogue loaders' own refusals. The edits are made to the real
// Data/Content in memory, so each test changes exactly one thing in valid data.
#include "StructureOthersTestSupport.h"

using namespace PerfectWorld;

namespace
{
const char* const ExplainFamilyFile = "explain_family.xi_claim_evidence_limit.json";
const char* const ExplainStructureFile = "structure.construct_an_explanation.json";
const char* const Tram = "explain_case.xi_tram_poem.json";
const char* const InvestigateFamilyFile = "investigate_family.xi_fault_finding.json";
const char* const InvestigateStructureFile = "structure.investigate_a_fault.json";
const char* const Fan = "investigate_case.xi_classroom_fan.json";
const char* const CoordinateFamilyFile = "coordinate_family.xi_group_work.json";
const char* const CoordinateStructureFile = "structure.coordinate_a_group.json";
const char* const Stall = "coordinate_case.xi_science_stall.json";
const char* const Sports = "coordinate_case.xi_sports_day_stall.json";
const char* const Debate = "coordinate_case.xi_debate_hall.json";

void TestExplainRules(const std::vector<ContentFile>& files)
{
    // Every case fits the family's lists and template.
    Refuses(files, {{ExplainFamilyFile, "\"roles\": [\"author\", ", "\"roles\": ["}}, "its role author is not a role the family allows");
    Refuses(files, {{ExplainFamilyFile, "\"questions\": [\"meaning\", ", "\"questions\": ["}}, "its question meaning is not a question the family allows");
    Refuses(files, {{ExplainFamilyFile, "\"minClaims\": 3", "\"minClaims\": 5"}}, "the template's minClaims is above its maxClaims");
    Refuses(files, {{ExplainFamilyFile, "\"minEvidence\": 5", "\"minEvidence\": 6"}}, "the family template wants 6 to 7 evidence items, found 5");
    Refuses(files, {{ExplainFamilyFile, "\"maxLimitations\": 4", "\"maxLimitations\": 3"}}, "the family template wants 3 to 3 limitations, found 4");
    Refuses(files, {{Tram, "\"defensible\": true },", "\"defensible\": false },"}}, "the family template wants at least 2 defensible claims, found 1");
    Refuses(files, {{Tram, "\"defensible\": false }", "\"defensible\": true }"}}, "needs at least one indefensible claim");

    // There is no single right answer, but there is a wrong one: defensible claims need support and a fitting limit.
    Refuses(files, {{Tram, "\"supports\": [\"c_farewell\"], \"against\": [] },", "\"supports\": [], \"against\": [] },"}},
            "the defensible claim c_farewell needs at least 2 supporting evidence items, found 1");
    Refuses(files, {{Tram, "\"bounds\": [\"c_farewell\"] },", "\"bounds\": [\"c_defiance\"] },"},
                    {Tram, "\"bounds\": [\"c_farewell\", \"c_defiance\", \"c_complaint\"]", "\"bounds\": [\"c_complaint\"]"}},
            "the defensible claim c_farewell has no limitation that bounds it");
    Refuses(files, {{Tram, "\"bounds\": [\"c_defiance\"] },", "\"bounds\": [\"c_defiance\", \"c_farewell\"] },"}},
            "every limitation bounds the defensible claim c_farewell; the choice of limitation must matter");
    Refuses(files, {{Tram, "\"supports\": [], \"against\": [] }", "\"supports\": [\"c_complaint\"], \"against\": [] }"}},
            "the indefensible claim c_complaint is supported by evidence");
    Refuses(files, {{Tram, "\"against\": [\"c_complaint\"] },", "\"against\": [] },"}}, "the indefensible claim c_complaint is contradicted by no evidence");
    Refuses(files, {{Tram, "\"supports\": [], \"against\": [] }", "\"supports\": [], \"against\": [\"c_complaint\"] }"}},
            "has no evidence item beside the point; padding must be possible");
    Refuses(files, {{Tram, "\"supports\": [\"c_farewell\", \"c_defiance\"]", "\"supports\": [\"c_farewell\", \"c_nope\"]"}},
            "evidence e_bell names the claim c_nope, which the case does not have");
    Refuses(files, {{Tram, "\"supports\": [\"c_defiance\"], \"against\": [] }", "\"supports\": [\"c_defiance\"], \"against\": [\"c_defiance\"] }"}},
            "evidence e_sparks both supports and cuts against the claim c_defiance");
    Refuses(files, {{Tram, "\"bounds\": [\"c_defiance\"] },", "\"bounds\": [] },"}}, "limitation l_sorrow bounds no claim");
    Refuses(files, {{Tram, "\"bounds\": [\"c_defiance\"] },", "\"bounds\": [\"c_nope\"] },"}}, "limitation l_sorrow names the claim c_nope, which the case does not have");
    Refuses(files, {{Tram, "\"id\": \"e_year\"", "\"id\": \"e_bell\""}}, "evidence IDs must be well-formed and unique");

    // The variation must be real, and every listed value used.
    Refuses(files, {{"xi_annex_notice", "\"role\": \"recorder\"", "\"role\": \"author\""}, {"xi_annex_notice", "\"scarce\": \"sources\"", "\"scarce\": \"time\""}},
            "repeats the question, role and scarce resource of explain_case.xi_annex_notice");
    Refuses(files, {{"xi_annex_notice", "\"subject\": \"interpretation\"", "\"subject\": \"literature\""}}, "no case uses the subject interpretation the family lists");
    Refuses(files, {{"xi_tea_price", "\"method\": \"method.reading_figures\"", "\"method\": \"method.source_criticism\""}},
            "no case uses the method method.reading_figures the family lists");
    Refuses(files, {{"xi_tea_price", "\"method\": \"method.reading_figures\"", "\"method\": \"method.nope\""}}, "the method method.nope is not one of the family's methods");

    // Four cases for a classroom family that declares four; and at least three declared.
    std::vector<ContentFile> fewer;
    for (const ContentFile& file : files)
    {
        if (file.Path.find("xi_tea_price") == std::string::npos)
            fewer.push_back(file);
    }
    Check(HasIssue(LoadContent(fewer), "needs at least 4 cases, found 3"), "three cases are too few for a family that declares four");
    Refuses(files, {{ExplainFamilyFile, "\"minCases\": 4", "\"minCases\": 2"}}, "a classroom explain family declares at least 3 cases");
    Refuses(files, {{ExplainFamilyFile, "\"structure\": \"structure.construct_an_explanation\"", "\"structure\": \"structure.investigate_a_fault\""}},
            "needs a structure whose engine is explain, but its structure's engine is investigate");

    // The structure record agrees with the runner's constants.
    Refuses(files, {{ExplainStructureFile, "[\"claim\", \"evidence\", \"limitation\"]", "[\"evidence\", \"claim\", \"limitation\"]"}},
            "the explain structure's beats must be claim, evidence, limitation, in that order");
    Refuses(files, {{ExplainStructureFile, "\"goals\": [\"argue_claim\", ", "\"goals\": ["}}, "the explain structure's goals must be argue_claim, weigh_evidence, state_limits, in that order");
    Refuses(files, {{ExplainStructureFile, "\"model_paragraph\", ", ""}}, "the explain structure's decisions must be model_paragraph, choose_claim, place_evidence, place_limitation");
    Refuses(files, {{ExplainStructureFile, ", \"padding\"]", "]"}}, "the explain structure's failures must be indefensible_reading, unsupported_claim, overreach, padding");
    Refuses(files, {{ExplainStructureFile, "\"periodUnits\": 9", "\"periodUnits\": 8"}}, "the explain structure's period is 9 units of 5 minutes");
    Refuses(files, {{ExplainStructureFile, "\"engine\": \"explain\"", "\"engine\": \"sculpt\""}}, "has an unknown value");

    // Shape, references and fairness hold for the new types like any other.
    Refuses(files, {{Tram, "\"role\": \"author\"", "\"role\": \"captain\""}}, "has an unknown value");
    Refuses(files, {{Tram, "\"family\": \"explain_family.xi_claim_evidence_limit\"", "\"family\": \"explain_family.nope\""}}, "which does not exist");
    Refuses(files, {{Tram, "\"name\": \"The last tram on the poem's route\",", "\"name\": \"The last tram on the poem's route\", \"frobnicate\": 1,"}}, "unknown field");
    Refuses(files, {{Tram, "\"name\": \"The last tram on the poem's route\",", "\"name\": \"The last tram on the poem's route\", \"religionBonus\": 5,"}},
            "keys a modifier on religion");
    Refuses(files, {{Tram, "\"defensible\": true },", "\"defensible\": \"yes\" },"}}, "must be true or false");
    Refuses(files, {{ExplainFamilyFile, "\"knownAtMilli\": 108000", "\"knownAtMilli\": -1"}}, "outside 0..1000000");
}

void TestInvestigateRules(const std::vector<ContentFile>& files)
{
    Refuses(files, {{Fan, "\"fault\": false }", "\"fault\": true }"}}, "exactly one cause must be the fault, found 2");
    Refuses(files, {{Fan, "\"fault\": true }", "\"fault\": false }"}}, "exactly one cause must be the fault, found 0");
    Refuses(files, {{Fan, "\"implicates\": [\"c_bearing\"] }", "\"implicates\": [\"c_bearing\", \"c_dust\", \"c_capacitor\", \"c_regulator\"] }"}},
            "test t_spin must implicate some, but not all, of the causes");
    Refuses(files, {{Fan, "\"implicates\": [\"c_bearing\"] }", "\"implicates\": [] }"}}, "test t_spin must implicate some, but not all, of the causes");
    Refuses(files, {{Fan, "\"implicates\": [\"c_bearing\"] }", "\"implicates\": [\"c_nope\"] }"}}, "test t_spin implicates the cause c_nope, which the case does not have");
    Refuses(files, {{Fan, "\"units\": 1", "\"units\": 2"}, {Fan, "\"units\": 1", "\"units\": 2"}, {Fan, "\"units\": 3", "\"units\": 2"}},
            "the tests all cost the same; tests must have different costs");
    Refuses(files, {{Fan, "\"implicates\": [\"c_regulator\"] }", "\"implicates\": [\"c_capacitor\", \"c_regulator\"] }"},
                    {Fan, "\"implicates\": [\"c_capacitor\"] }", "\"implicates\": [\"c_capacitor\", \"c_regulator\"] }"}},
            "no test tells the cause c_capacitor from c_regulator");
    Refuses(files, {{Fan, "\"id\": \"t_blades\"", "\"id\": \"t_spin\""}}, "test IDs must be well-formed and unique");
    Refuses(files, {{InvestigateFamilyFile, "\"minTests\": 4", "\"minTests\": 6"}}, "the family template wants 6 to 6 tests, found 5");
    Refuses(files, {{InvestigateFamilyFile, "\"maxCauses\": 5", "\"maxCauses\": 4"}}, "the family template wants 4 to 4 causes, found 5");
    Refuses(files, {{InvestigateFamilyFile, "\"symptoms\": [\"wrong_output\", ", "\"symptoms\": ["}}, "its symptom wrong_output is not a symptom the family allows");
    Refuses(files, {{"xi_cash_total", "\"scarce\": \"time\"", "\"scarce\": \"access\""}}, "no case uses the scarce resource time the family lists");
    Refuses(files, {{"xi_classroom_fan", "\"method\": \"method.half_split\"", "\"method\": \"method.read_the_meter\""},
                    {"xi_cash_total", "\"method\": \"method.half_split\"", "\"method\": \"method.read_the_meter\""}},
            "no case uses the method method.half_split the family lists");
    Refuses(files, {{"xi_cash_total", "\"role\": \"critic\"", "\"role\": \"inspector\""}, {"xi_cash_total", "\"scarce\": \"time\"", "\"scarce\": \"access\""},
                    {"xi_cash_total", "\"symptom\": \"wrong_output\"", "\"symptom\": \"intermittent\""}, {"xi_bicycle_lamp", "\"role\": \"repairer\"", "\"role\": \"inspector\""}},
            "repeats the symptom, role and scarce resource of investigate_case.xi_bicycle_lamp");
    Refuses(files, {{InvestigateStructureFile, "[\"isolate\", \"conclude\"]", "[\"conclude\", \"isolate\"]"}},
            "the investigate structure's beats must be isolate, conclude, in that order");
    Refuses(files, {{InvestigateStructureFile, "\"report_inconclusive\"", "\"give_up\""}},
            "the investigate structure's decisions must be run_test, diagnose, report_inconclusive");
    Refuses(files, {{InvestigateStructureFile, "\"abandoned_search\"", "\"nothing\""}}, "the investigate structure's failures must be wrong_cause");
    Refuses(files, {{InvestigateFamilyFile, "\"structure\": \"structure.investigate_a_fault\"", "\"structure\": \"structure.coordinate_a_group\""}},
            "needs a structure whose engine is investigate, but its structure's engine is coordinate");
    Refuses(files, {{Fan, "\"units\": 3", "\"units\": 4"}}, "outside 1..3");
    Refuses(files, {{Fan, "\"kind\": \"swap\"", "\"kind\": \"smell\""}}, "has an unknown value");
}

void TestCoordinateRules(const std::vector<ContentFile>& files)
{
    Refuses(files, {{Stall, "\"capacity\": 2, \"self\": true", "\"capacity\": 2, \"self\": false"}}, "exactly one member must be the player's own (self), found 0");
    Refuses(files, {{Stall, "\"holder\": \"m_tara\", \"text\": \"Tara runs", "\"holder\": \"m_you\", \"text\": \"Tara runs"}},
            "the critical fact f_tara_meet is held by the player; a critical fact must be a classmate's");
    Refuses(files, {{Stall, "\"critical\": true", "\"critical\": false"}}, "needs at least one critical fact");
    Refuses(files, {{CoordinateFamilyFile, "\"maxCriticalHolders\": 2", "\"maxCriticalHolders\": 1"}, {Debate, "\"critical\": false }", "\"critical\": true }"}},
            "the family template wants at most 1 members holding a critical fact, found 2");
    Refuses(files, {{Stall, "\"critical\": false }", "\"critical\": true }"}}, "every classmate holds a critical fact; a conversation must be able to find nothing");
    Refuses(files, {{Stall, "\"capacity\": 2, \"self\": true", "\"capacity\": 1, \"self\": true"}, {Stall, "\"capacity\": 2, \"self\": false", "\"capacity\": 1, \"self\": false"}},
            "no assignment keeps every member within capacity and every task with a member who suits it");
    Refuses(files, {{Stall, "\"fits\": [\"m_you\", \"m_tara\"]", "\"fits\": []"}}, "task t_poster is suited to nobody");
    Refuses(files, {{Stall, "\"fits\": [\"m_you\", \"m_tara\"]", "\"fits\": [\"m_you\", \"m_nope\"]"}}, "task t_poster is suited to m_nope, who is not a member of the case");
    Refuses(files, {{Stall, "\"holder\": \"m_ishan\", \"text\": \"Ishan takes", "\"holder\": \"m_nope\", \"text\": \"Ishan takes"}},
            "fact f_ishan_bus is held by m_nope, who is not a member of the case");
    Refuses(files, {{Stall, "\"id\": \"m_tara\"", "\"id\": \"m_ishan\""}}, "member IDs must be well-formed and unique");
    Refuses(files, {{CoordinateFamilyFile, "\"minMembers\": 3", "\"minMembers\": 4"}}, "the family template wants 4 to 4 members, found 3");
    Refuses(files, {{CoordinateFamilyFile, "\"maxTasks\": 4", "\"maxTasks\": 2"}}, "the template's minTasks is above its maxTasks");
    Refuses(files, {{CoordinateFamilyFile, "\"constraints\": [\"capacity\", ", "\"constraints\": ["}}, "its constraint capacity is not a constraint the family allows");
    Refuses(files, {{"xi_debate_hall", "\"scarce\": \"space\"", "\"scarce\": \"time\""}}, "no case uses the scarce resource space the family lists");
    Refuses(files, {{"xi_titration_report", "\"method\": \"method.run_a_check_in\"", "\"method\": \"method.split_the_list\""},
                    {"xi_debate_hall", "\"method\": \"method.run_a_check_in\"", "\"method\": \"method.split_the_list\""}},
            "no case uses the method method.run_a_check_in the family lists");
    Refuses(files, {{CoordinateStructureFile, "[\"assign\", \"exchange\", \"settle\", \"submit\"]", "[\"assign\", \"settle\", \"exchange\", \"submit\"]"}},
            "the coordinate structure's beats must be assign, exchange, settle, submit, in that order");
    Refuses(files, {{CoordinateStructureFile, "\"end_exchange\", ", ""}}, "the coordinate structure's decisions must be assign_work, exchange, end_exchange, settle, submit");
    Refuses(files, {{CoordinateStructureFile, "\"organise_work\", ", ""}}, "the coordinate structure's goals must be organise_work");
    Refuses(files, {{CoordinateFamilyFile, "\"structure\": \"structure.coordinate_a_group\"", "\"structure\": \"structure.construct_an_explanation\""}},
            "needs a structure whose engine is coordinate, but its structure's engine is explain");
    Refuses(files, {{Stall, "\"seam\": false", "\"seam\": 0"}}, "must be true or false");
    Refuses(files, {{Stall, "\"load\": 2", "\"load\": 3"}}, "outside 1..2");
}

/** The loaders refuse (never coerce) records the validator would also have refused. */
template <class LoadFn>
void LoaderRefuses(const std::vector<ContentFile>& files, const std::vector<Edit>& edits, LoadFn load, const char* text)
{
    const ContentLoadResult broken = LoadEdited(files, edits);
    const auto result = load(broken.Db);
    Check(!result.Ok() && result.Error.find(text) != std::string::npos && result.Catalogue.Families.empty(), text);
}
} // namespace

void TestContentRules(const std::vector<ContentFile>& files)
{
    Check(LoadContent(files).Ok(), "the unedited content is valid");
    TestExplainRules(files);
    TestInvestigateRules(files);
    TestCoordinateRules(files);
}

void TestCatalogueRules(const std::vector<ContentFile>& files)
{
    const ContentLoadResult real = LoadContent(files);
    const ExplainCatalogueResult explain = LoadExplainCatalogue(real.Db);
    Check(explain.Ok() && explain.Catalogue.Families.size() == 1 && explain.Catalogue.Families.front().Cases.size() == 4, "the real content loads as one explain family of four");
    const ExplainFamilyDef& eFamily = explain.Catalogue.Families.front();
    Check(eFamily.SkillId == "skill.academics" && eFamily.MinCases == 4 && eFamily.Methods.size() == 3, "the family names its skill, minimum and methods");
    const ExplainCaseDef* tram = explain.Catalogue.FindCase("explain_case.xi_tram_poem");
    Check(tram != nullptr && tram->FamilyId == eFamily.Id && tram->Question == ExplainQuestion::Meaning && tram->Role == ExplainRole::Author &&
              tram->Scarce == ExplainScarce::Time && tram->Subject == "literature" && tram->MethodId == "method.close_reading" && tram->MethodKnownAtMilli == 108000,
          "the poem case is read");
    Check(tram->Claims.size() == 3 && tram->Evidence.size() == 5 && tram->Limitations.size() == 3 && tram->Claims.front().Id == "c_farewell" &&
              tram->Claims.front().Defensible && !tram->Claims.back().Defensible && tram->Evidence[1].Supports.size() == 2 && !tram->Brief.empty(),
          "its claims, evidence and limitations are read in authored order");
    Check(explain.Catalogue.FindCase("explain_case.xi_nope") == nullptr && explain.Catalogue.FindFamily("explain_family.nope") == nullptr, "unknown IDs are not found");

    const InvestigateCatalogueResult investigate = LoadInvestigateCatalogue(real.Db);
    Check(investigate.Ok() && investigate.Catalogue.Families.size() == 1 && investigate.Catalogue.Families.front().Cases.size() == 4, "the real content loads as one investigate family of four");
    const InvestigateCaseDef* fan = investigate.Catalogue.FindCase("investigate_case.xi_classroom_fan");
    Check(fan != nullptr && fan->Symptom == InvestigateSymptom::WrongOutput && fan->Role == InvestigateRole::Inspector && fan->Scarce == InvestigateScarce::Equipment &&
              fan->Causes.size() == 4 && fan->Tests.size() == 5 && fan->Tests[3].Kind == InvestigateTestKind::Measure && fan->Tests[3].Units == 2 &&
              fan->Tests[2].Implicates.size() == 2 && fan->MethodKnownAtMilli == 8000 && investigate.Catalogue.Families.front().SkillId == "skill.practical_repair",
          "the fan case is read");

    const CoordinateCatalogueResult coordinate = LoadCoordinateCatalogue(real.Db);
    Check(coordinate.Ok() && coordinate.Catalogue.Families.size() == 1 && coordinate.Catalogue.Families.front().Cases.size() == 4, "the real content loads as one coordinate family of four");
    const CoordinateCaseDef* stall = coordinate.Catalogue.FindCase("coordinate_case.xi_science_stall");
    Check(stall != nullptr && stall->Constraint == CoordinateConstraint::Capacity && stall->Role == CoordinateRole::Chair && stall->Scarce == CoordinateScarce::Time && !stall->Seam &&
              stall->Members.size() == 3 && stall->Members.front().Self && stall->Tasks.size() == 3 && stall->Facts.size() == 2 && stall->Facts.front().Critical &&
              stall->Tasks[1].Load == 2 && stall->MethodKnownAtMilli == 6000 && coordinate.Catalogue.Families.front().SkillId == "skill.communication",
          "the stall case is read");

    // The other structures' records do not leak into the model catalogue, which still reads its own.
    Check(LoadModelCatalogue(real.Db).Ok() && LoadModelCatalogue(real.Db).Catalogue.Families.size() == 1, "the model catalogue is untouched by the new record types");

    const auto loadExplain = [](const ContentDb& db) { return LoadExplainCatalogue(db); };
    LoaderRefuses(files, {{"xi_tram_poem", "\"method\": \"method.close_reading\"", "\"method\": \"method.nope\""}}, loadExplain,
                  "explain_case.xi_tram_poem: the method method.nope is not one of the family's methods");
    LoaderRefuses(files, {{"xi_tram_poem", "\"family\": \"explain_family.xi_claim_evidence_limit\"", "\"family\": \"explain_family.nope\""}}, loadExplain,
                  "its family explain_family.nope does not exist");
    LoaderRefuses(files, {{"xi_tram_poem", "\"supports\": [\"c_farewell\"], \"against\": [] },", "\"supports\": [], \"against\": [] },"},
                          {"xi_tram_poem", "\"supports\": [\"c_farewell\", \"c_defiance\"]", "\"supports\": [\"c_defiance\"]"}},
                  loadExplain, "the defensible claim c_farewell has no supporting evidence or no bounding limitation");
    LoaderRefuses(files, {{"xi_tram_poem", "\"defensible\": true },", "\"defensible\": false },"}, {"xi_tram_poem", "\"defensible\": true },", "\"defensible\": false },"}},
                  loadExplain, "at least one defensible and one indefensible claim");
    LoaderRefuses(files, {{"xi_tram_poem", "\"supports\": [\"c_defiance\"], \"against\": []", "\"supports\": [\"c_nope\"], \"against\": []"}}, loadExplain,
                  "evidence e_sparks names a claim the case does not have");
    LoaderRefuses(files, {{ExplainFamilyFile, "\"minCases\": 4", "\"minCases\": 0"}}, loadExplain, "minCases must be a positive whole number");

    const auto loadInvestigate = [](const ContentDb& db) { return LoadInvestigateCatalogue(db); };
    LoaderRefuses(files, {{"xi_classroom_fan", "\"fault\": true }", "\"fault\": false }"}}, loadInvestigate, "exactly one cause must be the fault");
    LoaderRefuses(files, {{"xi_classroom_fan", "\"implicates\": [\"c_bearing\"] }", "\"implicates\": [] }"}}, loadInvestigate, "test t_spin must implicate some, but not all");
    LoaderRefuses(files, {{"xi_classroom_fan", "\"method\": \"method.half_split\"", "\"method\": \"method.nope\""}}, loadInvestigate,
                  "investigate_case.xi_classroom_fan: the method method.nope is not one of the family's methods");
    LoaderRefuses(files, {{"xi_classroom_fan", "\"units\": 3", "\"units\": 9"}}, loadInvestigate, "test t_swap_capacitor needs a known kind and units from 1 to 3");
    // With no test that tells the range from the text, the fault can never be isolated.
    LoaderRefuses(files, {{"xi_cash_total", "\"implicates\": [\"c_range\"] }", "\"implicates\": [\"c_range\", \"c_text\"] }"},
                          {"xi_cash_total", "\"implicates\": [\"c_text\"] }", "\"implicates\": [\"c_range\", \"c_text\"] }"},
                          {"xi_cash_total", "\"implicates\": [\"c_range\", \"c_hidden\"] }", "\"implicates\": [\"c_range\", \"c_hidden\", \"c_text\"] }"}},
                  loadInvestigate, "cannot be completed within its 5 units");

    const auto loadCoordinate = [](const ContentDb& db) { return LoadCoordinateCatalogue(db); };
    LoaderRefuses(files, {{"xi_science_stall", "\"capacity\": 2, \"self\": true", "\"capacity\": 2, \"self\": false"}}, loadCoordinate, "exactly one member must be the player's own");
    LoaderRefuses(files, {{"xi_science_stall", "\"critical\": true", "\"critical\": false"}}, loadCoordinate, "a case needs at least one critical fact");
    LoaderRefuses(files, {{"xi_science_stall", "\"capacity\": 2, \"self\": true", "\"capacity\": 1, \"self\": true"}, {"xi_science_stall", "\"capacity\": 2, \"self\": false", "\"capacity\": 1, \"self\": false"}},
                  loadCoordinate, "no assignment keeps every member within capacity");
    LoaderRefuses(files, {{Sports, "\"holder\": \"m_tara\", \"text\": \"Tara is down to run the 400-metre heat at noon\", \"critical\": false",
                                    "\"holder\": \"m_tara\", \"text\": \"Tara is down to run the 400-metre heat at noon\", \"critical\": true"}},
                  loadCoordinate, "cannot be completed within its 7 units");
    LoaderRefuses(files, {{"xi_science_stall", "\"constraint\": \"capacity\"", "\"constraint\": \"gravity\""}}, loadCoordinate, "role, scarce and constraint must each be a known name");
}
