#include "TowerScenarioContent.h"

#include "Tower.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace
{
int Assertions = 0;

void Require(bool condition, const char* message)
{
    ++Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::string ReadFileText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    Require(input.good(), "authored file exists and is readable");
    return std::string{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

JsonValue& Field(JsonValue& object, std::string_view name)
{
    for (auto& [key, value] : object.Object)
    {
        if (key == name)
        {
            return value;
        }
    }
    std::cerr << "FAIL: missing expected JSON field " << name << '\n';
    std::exit(2);
}

void ResolveDataPaths(
    int argc,
    char** argv,
    std::filesystem::path& outScenarioDir,
    std::filesystem::path& outTextFile)
{
    namespace fs = std::filesystem;
    if (argc >= 3)
    {
        outScenarioDir = fs::path(argv[1]);
        outTextFile = fs::path(argv[2]);
        return;
    }
    if (argc == 2)
    {
        const fs::path arg(argv[1]);
        if (fs::exists(arg / "p2.encounter.json"))
        {
            outScenarioDir = arg;
            outTextFile = arg.parent_path().parent_path() / "Text" / "en" / "phase2_a7.json";
            return;
        }
        if (fs::exists(arg / "Content" / "tower_scenario" / "p2.encounter.json"))
        {
            outScenarioDir = arg / "Content" / "tower_scenario";
            outTextFile = arg / "Text" / "en" / "phase2_a7.json";
            return;
        }
    }
    for (fs::path cur = fs::current_path(); !cur.empty(); cur = cur.parent_path())
    {
        if (fs::exists(cur / "Data" / "Content" / "tower_scenario" / "p2.encounter.json"))
        {
            outScenarioDir = cur / "Data" / "Content" / "tower_scenario";
            outTextFile = cur / "Data" / "Text" / "en" / "phase2_a7.json";
            return;
        }
        if (cur == cur.root_path())
        {
            break;
        }
    }
    outScenarioDir = fs::path("Data/Content/tower_scenario");
    outTextFile = fs::path("Data/Text/en/phase2_a7.json");
}

std::vector<ContentFile> LoadAuthoredScenarioFiles(const std::filesystem::path& dir)
{
    static constexpr std::array<std::string_view, 9> expectedStems = {
        "p2.encounter",
        "p2.pace",
        "p2.team_transit",
        "p2.changed_rule",
        "p2.stabilise",
        "p2.design",
        "p2.alliance",
        "p2.supply",
        "p2.mediation"};

    std::vector<ContentFile> files;
    for (const auto stem : expectedStems)
    {
        const std::string filename = std::string(stem) + ".json";
        const std::filesystem::path fullPath = dir / filename;
        files.push_back({"Data/Content/tower_scenario/" + filename, ReadFileText(fullPath)});
    }
    return files;
}

void TestCatalogueCoverageAndMetrics(const TowerScenarioCatalogue& catalogue)
{
    Require(catalogue.Scenarios.size() == 9, "all nine route scenarios are present");

    std::array<bool, TowerMethodCount> methodSeen{};
    std::set<std::string> classroomModels;

    for (const auto& route : TowerRoutes())
    {
        const TowerScenario* scenario = catalogue.FindByRoute(route.Id);
        Require(scenario != nullptr, "every TowerRoute has a bound tower_scenario");
        Require(scenario->Floor == route.Floor, "scenario floor matches TowerRoute floor");
        Require(scenario->PrimaryMethod == route.Method, "scenario primaryMethod matches TowerRoute method");
        Require(scenario->AllowsCombat == route.AllowsCombat, "scenario allowsCombat matches TowerRoute contract");
        Require(scenario->Beats.size() >= 3, "scenario has at least three playable beats");

        methodSeen[static_cast<std::size_t>(scenario->PrimaryMethod)] = true;
        if (scenario->Floor == 2)
        {
            Require(scenario->ClassroomModelId.rfind("model.xi.", 0) == 0,
                    "Floor 2 scenario binds a Class XI classroom model id");
            Require(!scenario->ChangedRuleKey.empty(),
                    "Floor 2 scenario defines a changedRuleKey for its single changed rule");
            classroomModels.insert(scenario->ClassroomModelId);
        }

        bool feedsDiscipline = false;
        bool feedsCommonJudgment = false;
        for (std::size_t bIdx = 0; bIdx < scenario->Beats.size(); ++bIdx)
        {
            const auto& beat = scenario->Beats[bIdx];
            feedsDiscipline = feedsDiscipline || (beat.Metric == TowerScenarioMetric::Discipline);
            feedsCommonJudgment = feedsCommonJudgment || (beat.Metric == TowerScenarioMetric::CommonJudgment);
            methodSeen[static_cast<std::size_t>(beat.Recovery.Method)] = true;
            Require(beat.Recovery.Method != TowerMethod::Combat, "recovery never requires combat");

            for (const auto& clue : beat.Clues)
            {
                methodSeen[static_cast<std::size_t>(clue.Method)] = true;
            }
            for (const auto& dec : beat.Decisions)
            {
                methodSeen[static_cast<std::size_t>(dec.Method)] = true;
                if (beat.Metric == TowerScenarioMetric::CommonJudgment)
                {
                    Require(!dec.IsCombat && dec.Method != TowerMethod::Combat,
                            "common_judgment beats never use combat decisions");
                }
            }

            const TowerScenarioRunResult recovered = BuildRecoveredMistakeAttempt(*scenario, bIdx);
            Require(recovered.Valid, "mistake on any beat recovers cleanly to completion");
            Require(recovered.RecoveriesUsed == 1, "exactly one recovery triggered for single-beat mistake");
            Require(!recovered.Attempt.Defeated, "recovered mistake does not eject the candidate");
            Require(recovered.VisitedBeats.size() == scenario->Beats.size(),
                    "recovered run still visits every beat in the scenario");
            Require(recovered.Attempt.DisciplineMilli >= 750 && recovered.Attempt.CommonJudgmentMilli >= 750,
                    "recovered run retains strong partial credit");
        }
        Require(feedsDiscipline && feedsCommonJudgment,
                "every scenario feeds both real Tower metrics (discipline and common_judgment)");

        const TowerScenarioRunResult optimalNonCombat = BuildOptimalNonCombatAttempt(*scenario);
        Require(optimalNonCombat.Valid, "optimal non-combat run is valid");
        Require(!optimalNonCombat.UsedCombat, "optimal non-combat run uses zero combat");
        Require(!optimalNonCombat.CombatFedCommonJudgment, "no combat fed common judgment");
        Require(optimalNonCombat.Attempt.DisciplineMilli == TowerScoreMaxMilli,
                "non-combat run reaches 1000 DisciplineMilli ceiling");
        Require(optimalNonCombat.Attempt.CommonJudgmentMilli == TowerScoreMaxMilli,
                "non-combat run reaches 1000 CommonJudgmentMilli ceiling");
        Require(optimalNonCombat.Attempt.MissionMinutes > 0, "run records positive MissionMinutes");
        Require(optimalNonCombat.VisitedBeats.size() == scenario->Beats.size(),
                "every beat in scenario is reachable and visited");
    }

    Require(classroomModels.size() == 3, "Floor 2 scenarios cover three distinct classroom models");
    for (std::size_t mIdx = 0; mIdx < TowerMethodCount; ++mIdx)
    {
        Require(methodSeen[mIdx], "all six TowerMethod values are covered across scenarios");
    }
}

void TestFullNonCombatExaminationAndParity(const TowerScenarioCatalogue& catalogue)
{
    const TowerScenario* f1Encounter = catalogue.FindByRoute("tower.route.encounter");
    const TowerScenario* f1Pace = catalogue.FindByRoute("tower.route.pace");
    const TowerScenario* f1Transit = catalogue.FindByRoute("tower.route.team_transit");
    const TowerScenario* f2Rule = catalogue.FindByRoute("tower.route.changed_rule");
    const TowerScenario* f2Stabilise = catalogue.FindByRoute("tower.route.stabilise");
    const TowerScenario* f2Design = catalogue.FindByRoute("tower.route.design");
    const TowerScenario* f3Alliance = catalogue.FindByRoute("tower.route.alliance");
    const TowerScenario* f3Supply = catalogue.FindByRoute("tower.route.supply");
    const TowerScenario* f3Mediation = catalogue.FindByRoute("tower.route.mediation");

    Require(f1Encounter && f1Pace && f1Transit && f2Rule && f2Stabilise && f2Design &&
                f3Alliance && f3Supply && f3Mediation,
            "all nine route scenarios resolved by route ID");

    // 1. Combat vs non-combat parity on tower.route.encounter.
    const TowerScenarioRunResult encNonCombat = BuildOptimalNonCombatAttempt(*f1Encounter);
    const TowerScenarioRunResult encCombat = BuildOptimalCombatAttempt(*f1Encounter);
    Require(encNonCombat.Valid && encCombat.Valid, "encounter supports both non-combat and combat paths");
    Require(!encNonCombat.UsedCombat && encNonCombat.Attempt.Method == TowerMethod::Endurance,
            "encounter non-combat path uses Endurance");
    Require(encCombat.UsedCombat && encCombat.Attempt.Method == TowerMethod::Combat,
            "encounter combat path uses Combat");
    Require(!encCombat.CombatFedCommonJudgment,
            "encounter combat path never feeds CommonJudgment");

    const std::vector<TowerFloorAttempt> nonCombatExam = {
        encNonCombat.Attempt,
        BuildOptimalNonCombatAttempt(*f2Rule).Attempt,
        BuildOptimalNonCombatAttempt(*f3Alliance).Attempt};
    std::vector<TowerFloorAttempt> combatExam = nonCombatExam;
    combatExam[0] = encCombat.Attempt;

    const TowerResult nonCombatOut = ResolveTower(nonCombatExam);
    const TowerResult combatOut = ResolveTower(combatExam);
    Require(nonCombatOut.Ok() && combatOut.Ok(), "both exam attempts resolve cleanly in ResolveTower");
    Require(nonCombatOut.ScorePoints == 6000 && combatOut.ScorePoints == 6000,
            "non-combat and combat paths reach identical 6000 score ceiling");
    Require(nonCombatOut.Decile == 1 && combatOut.Decile == 1, "both paths reach the top national decile");
    Require(nonCombatOut.Credentials == combatOut.Credentials,
            "combat grants no exclusive credential advantage");

    // 2. Every credential (all 10) is reachable through purely non-combat scenario combinations.
    std::set<AptitudePath> unlocked;
    const std::array<const TowerScenario*, 3> floor1Routes = {f1Encounter, f1Pace, f1Transit};
    const std::array<const TowerScenario*, 3> floor2Routes = {f2Rule, f2Stabilise, f2Design};
    const std::array<const TowerScenario*, 3> floor3Routes = {f3Alliance, f3Supply, f3Mediation};

    for (const auto* r1 : floor1Routes)
    {
        for (const auto* r2 : floor2Routes)
        {
            for (const auto* r3 : floor3Routes)
            {
                const TowerScenarioRunResult run1 = BuildOptimalNonCombatAttempt(*r1);
                const TowerScenarioRunResult run2 = BuildOptimalNonCombatAttempt(*r2);
                const TowerScenarioRunResult run3 = BuildOptimalNonCombatAttempt(*r3);
                Require(!run1.UsedCombat && !run2.UsedCombat && !run3.UsedCombat, "non-combat runs use no combat");
                const TowerResult outcome = ResolveTower({run1.Attempt, run2.Attempt, run3.Attempt});
                Require(outcome.Ok() && outcome.Completed && outcome.Decile == 1,
                        "every 3-floor non-combat route combination is valid and reaches the top decile");
                for (const auto& credential : outcome.Credentials) unlocked.insert(credential.Discipline);
            }
        }
    }

    for (const auto& cred : TowerCredentials())
    {
        Require(unlocked.contains(cred.Discipline),
                "every Tower credential is achievable through non-combat scenario runs");
    }

    // 3. Mistake + recovery exam still completes and resolves validly.
    const TowerResult recoveredOut = ResolveTower({
        BuildRecoveredMistakeAttempt(*f1Pace, 0).Attempt,
        BuildRecoveredMistakeAttempt(*f2Stabilise, 1).Attempt,
        BuildRecoveredMistakeAttempt(*f3Supply, 2).Attempt});
    Require(recoveredOut.Ok() && recoveredOut.Completed && recoveredOut.FloorsCleared == 3,
            "candidate who makes mistakes and recovers still clears all three floors without combat");
}

void TestLocalizationTextKeys(
    const TowerScenarioCatalogue& catalogue,
    const std::filesystem::path& textFilePath)
{
    const std::string jsonText = ReadFileText(textFilePath);
    std::map<std::string, std::string> textTable;
    std::string err;
    Require(LoadTowerScenarioTextTable(jsonText, textTable, err), "phase2_a7.json parses as a valid text table");
    Require(textTable.size() == 345, "phase2_a7.json contains all 345 authored A7 strings");

    std::vector<ContentIssue> issues;
    ValidateTowerScenarioTextKeys(catalogue, textTable, issues);
    Require(issues.empty(), "zero missing or empty localization keys across all nine scenarios");

    std::map<std::string, std::string> missingOne = textTable;
    missingOne.erase("p2.a7.changed_rule.changed_rule");
    ValidateTowerScenarioTextKeys(catalogue, missingOne, issues);
    Require(!issues.empty(), "missing text key is detected by ValidateTowerScenarioTextKeys");
}

void RefuseRecord(const JsonValue& badRoot, const char* reason)
{
    ContentRecord rec{"tower_scenario", "p2.changed_rule", "Data/Content/tower_scenario/p2.changed_rule.json", badRoot, {}};
    Require(!LoadTowerScenario(rec).Ok(), reason);
    ContentDb db;
    db.Records.emplace(rec.Id, std::move(rec));
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateTowerScenarioSemantics(db, issues);
    Require(!issues.empty(), "ValidateTowerScenarioSemantics reports rejected record");
}

void TestMalformedRefusals(const std::vector<ContentFile>& files)
{
    const JsonParseResult parsed = ParseJson(files[3].Text); // p2.changed_rule.json
    Require(parsed.Value.has_value(), "reference scenario JSON parses");
    const JsonValue& good = *parsed.Value;

    JsonValue bad = good;
    Field(bad, "route").String = "tower.route.nonexistent";
    RefuseRecord(bad, "unknown route refused");

    bad = good;
    Field(bad, "floor").Integer = 1;
    RefuseRecord(bad, "floor mismatch with route refused");

    bad = good;
    Field(bad, "allowsCombat").Bool = true;
    RefuseRecord(bad, "allowsCombat mismatch with route refused");

    bad = good;
    Field(bad, "classroomModel").String = "none";
    RefuseRecord(bad, "missing classroomModel on Floor 2 refused");

    bad = good;
    Field(bad, "changedRuleKey").String = "";
    RefuseRecord(bad, "empty changedRuleKey on Floor 2 refused");

    bad = good;
    Field(Field(bad, "beats").Array[2], "decisions").Array[0].Object[3].second.String = "combat";
    Field(Field(bad, "beats").Array[2], "decisions").Array[0].Object[4].second.Bool = true;
    RefuseRecord(bad, "combat feeding common_judgment refused");

    bad = good;
    Field(Field(Field(bad, "beats").Array[0], "decisions").Array[2], "nextBeat").String = "beat.2";
    RefuseRecord(bad, "mistake bypassing recovery refused");

    bad = good;
    Field(Field(Field(bad, "beats").Array[0], "recovery"), "nextBeat").String = "complete";
    Field(Field(Field(bad, "beats").Array[0], "decisions").Array[0], "nextBeat").String = "complete";
    Field(Field(Field(bad, "beats").Array[0], "decisions").Array[1], "nextBeat").String = "complete";
    RefuseRecord(bad, "unreachable beats refused");

    std::vector<ContentFile> incompleteFiles(files.begin(), files.end() - 1);
    Require(!LoadTowerScenarioFiles(incompleteFiles, true).Ok(),
            "catalogue missing one of the nine routes is refused when full coverage is required");
}
} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path scenarioDir;
    std::filesystem::path textFile;
    ResolveDataPaths(argc, argv, scenarioDir, textFile);

    const std::vector<ContentFile> files = LoadAuthoredScenarioFiles(scenarioDir);
    const TowerScenarioCatalogueResult loaded = LoadTowerScenarioFiles(files, true);
    for (const auto& issue : loaded.Issues)
    {
        std::cerr << issue.File << " [" << issue.Id << "]: " << issue.Message << '\n';
    }
    Require(loaded.Ok(), "all nine tower_scenario files load and pass fairness and semantic validation");
    const std::vector<std::string> knownTypes = KnownContentTypes();
    Require(std::find(knownTypes.begin(), knownTypes.end(), "tower_scenario") != knownTypes.end(),
            "tower_scenario is a registered content type");

    TestCatalogueCoverageAndMetrics(loaded.Catalogue);
    TestFullNonCombatExaminationAndParity(loaded.Catalogue);
    TestLocalizationTextKeys(loaded.Catalogue, textFile);
    TestMalformedRefusals(files);

    std::cout << "PASS: " << Assertions << " assertions\n";
    return 0;
}
