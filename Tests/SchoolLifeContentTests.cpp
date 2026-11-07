#include "SchoolLifeContent.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

using namespace PerfectWorld;
namespace fs = std::filesystem;

namespace
{
int Assertions = 0;

void Expect(bool condition, std::string_view message)
{
    ++Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::string ReadTextFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    Expect(in.good(), "file opens: " + path.generic_string());
    return {std::istreambuf_iterator<char>(in), {}};
}

std::vector<ContentFile> ReadContentTree(const fs::path& root)
{
    std::vector<fs::path> paths;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root, ec))
    {
        Expect(!ec, "directory iteration succeeds");
        if (entry.is_regular_file(ec) && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    std::vector<ContentFile> files;
    for (const fs::path& p : paths)
        files.push_back({"/" + fs::relative(p, root).generic_string(), ReadTextFile(p)});
    return files;
}

std::vector<ContentFile> MutateFile(std::vector<ContentFile> files, std::string_view arcId,
                                    std::string_view from, std::string_view to)
{
    bool found = false;
    const std::string suffix = "/school_scene/" + std::string(arcId) + ".json";
    for (ContentFile& f : files)
    {
        if (!f.Path.ends_with(suffix)) continue;
        const std::size_t pos = f.Text.find(from);
        Expect(pos != std::string::npos, "mutation target exists in " + std::string(arcId) + ": " + std::string(from));
        f.Text.replace(pos, from.size(), to);
        found = true;
    }
    Expect(found, "target school_scene file exists: " + std::string(arcId));
    return files;
}

void ExpectRefused(const std::vector<ContentFile>& files, std::string_view arcId,
                   std::string_view from, std::string_view to, std::string_view expectedReason)
{
    const ContentLoadResult mutated = LoadContentWithSchoolScenes(MutateFile(files, arcId, from, to));
    Expect(!mutated.Ok(), "mutated school_scene is refused by content load: " + std::string(expectedReason));
    const bool matched = std::any_of(mutated.Issues.begin(), mutated.Issues.end(), [&](const ContentIssue& issue) {
        return issue.Message.find(expectedReason) != std::string::npos;
    });
    if (!matched)
    {
        for (const ContentIssue& i : mutated.Issues)
            std::cerr << "  issue: " << i.Id << ": " << i.Message << '\n';
    }
    Expect(matched, "expected refusal reason reported: " + std::string(expectedReason));
    const SchoolSceneLoadResult cat = LoadSchoolSceneCatalogue(mutated.Db);
    Expect(!cat.Ok(), "typed catalogue loader also refuses invalid db");
}

void AuditCatalogueAndSimulation(const SchoolSceneCatalogue& catalogue, const SchoolTextTable& textTable)
{
    Expect(catalogue.Arcs.size() == 3, "exactly three Phase 2 A4 school_scene arcs load");
    Expect(catalogue.FindArc(SchoolArcChangingFriendshipId) != nullptr, "p2.changing_friendship exists");
    Expect(catalogue.FindArc(SchoolArcGroupProjectId) != nullptr, "p2.group_project exists");
    Expect(catalogue.FindArc(SchoolArcHouseholdResponsibilityId) != nullptr, "p2.household_responsibility exists");

    // Proposed new classmate Deven Chakraborty is explicitly flagged; existing cast are not.
    const SchoolCastMember* deven = FindSchoolLifeCast("person.deven_chakraborty");
    Expect(deven != nullptr && deven->bProposedNewCast && deven->Role == "classmate",
           "person.deven_chakraborty is registered as a proposed new classmate");
    for (std::string_view existingId : {"person.amrita_basu", "person.mira_sen", "person.ishan_sheikh",
                                        "person.tara_das", "person.household_adult", "person.care_worker"})
    {
        const SchoolCastMember* member = FindSchoolLifeCast(existingId);
        Expect(member != nullptr && !member->bProposedNewCast, "existing cast member is not marked proposedNewCast");
    }

    for (const SchoolSceneArc& arc : catalogue.Arcs)
    {
        Expect(!arc.Scenes.empty() && !arc.Resolutions.empty(), "arc has scenes and resolutions: " + arc.Id);
        bool sawFamilyVariant = false;
        bool sawCareVariant = false;

        for (const SchoolSceneNode& scene : arc.Scenes)
        {
            if (scene.Origin == SchoolOriginVariant::Family) sawFamilyVariant = true;
            if (scene.Origin == SchoolOriginVariant::CareHome) sawCareVariant = true;

            bool hasRefusalChoice = false;
            for (const SchoolSceneNodeChoice& ch : scene.Choices)
            {
                if (ch.bRefusal) hasRefusalChoice = true;
                Expect(!ch.bHumiliating, "no choice is humiliating: " + ch.Id);
                Expect(textTable.Find(ch.LabelKey) != nullptr, "choice label key exists: " + ch.LabelKey);
                Expect(textTable.Find(ch.OutcomeKey) != nullptr, "choice outcome key exists: " + ch.OutcomeKey);
                if (!ch.Consequence.JournalAgreementFact.empty())
                {
                    const std::string jKey = "journal.agreement." + ch.Consequence.JournalAgreementFact;
                    Expect(textTable.Find(jKey) != nullptr, "journal agreement key exists: " + jKey);
                }
            }
            Expect(hasRefusalChoice, "every scene offers a non-humiliating refusal choice: " + scene.Id);
        }
        Expect(sawFamilyVariant && sawCareVariant, "arc has both family and care_home scenes: " + arc.Id);

        // Simulate full traversal for both family (bGrewUpInCare = false) and care_home (bGrewUpInCare = true).
        for (const bool careOrigin : {false, true})
        {
            const SchoolSceneNode* entry = arc.FindScene(arc.EntrySceneId);
            Expect(entry != nullptr, "entry scene exists");

            SchoolLifeTrigger trigger;
            trigger.bGrewUpInCare = careOrigin;
            trigger.CurrentTermId = arc.TermId;
            trigger.CurrentZone = entry->Schedule.Zone;
            trigger.MinuteOfDay = entry->Schedule.StartMinute;
            const auto available = QueryAvailableSchoolScenes(catalogue, trigger);
            Expect(std::find(available.begin(), available.end(), entry) != available.end(),
                   "entry scene triggers at its scheduled zone and minute");

            // Refusal path from entry resolves cleanly without humiliation.
            const auto refIt = std::find_if(entry->Choices.begin(), entry->Choices.end(),
                                            [](const SchoolSceneNodeChoice& c) { return c.bRefusal; });
            Expect(refIt != entry->Choices.end(), "entry scene has refusal option");
            const SchoolLifeOutcome refOut = ResolveSchoolSceneChoice(catalogue, arc.Id, entry->Id, refIt->Id, careOrigin);
            Expect(refOut.bOk && !refOut.ResolutionId.empty(), "entry refusal resolves cleanly");

            // Cross-origin scene choice is refused.
            for (const SchoolSceneNodeChoice& ch : entry->Choices)
            {
                if (ch.NextSceneId.empty()) continue;
                const SchoolSceneNode* next = arc.FindScene(ch.NextSceneId);
                Expect(next != nullptr, "nextScene exists");
                const SchoolLifeOutcome step = ResolveSchoolSceneChoice(catalogue, arc.Id, entry->Id, ch.Id, careOrigin);
                if (SchoolOriginMatches(next->Origin, careOrigin))
                    Expect(step.bOk && step.NextSceneId == next->Id, "matching origin transition succeeds");
                else
                    Expect(!step.bOk, "cross-origin transition is refused");
            }
        }
    }
}
} // namespace

int main(int argc, char** argv)
{
    const fs::path contentDir = argc >= 2 ? fs::path(argv[1]) : fs::path("Data/Content");
    const fs::path textPath = argc >= 3 ? fs::path(argv[2]) : (contentDir.parent_path() / "Text/en/phase2_a4.json");

    const std::vector<ContentFile> files = ReadContentTree(contentDir);
    const ContentLoadResult loaded = LoadContentWithSchoolScenes(files);
    if (!loaded.Ok())
    {
        for (const ContentIssue& issue : loaded.Issues)
            std::cerr << issue.File << " (" << issue.Id << "): " << issue.Message << '\n';
    }
    Expect(loaded.Ok(), "Data/Content including Data/Content/school_scene/p2.*.json loads without issues");

    const SchoolSceneLoadResult catRes = LoadSchoolSceneCatalogue(loaded.Db);
    Expect(catRes.Ok(), "LoadSchoolSceneCatalogue succeeds: " + catRes.Error);

    const std::string rawText = ReadTextFile(textPath);
    const SchoolTextLoadResult textRes = LoadSchoolTextTable(rawText);
    Expect(textRes.Ok(), "Data/Text/en/phase2_a4.json loads cleanly: " + textRes.Error);

    const std::vector<std::string> missingKeys = ValidateSchoolSceneTextCoverage(catRes.Catalogue, textRes.Table);
    if (!missingKeys.empty())
    {
        for (const std::string& m : missingKeys)
            std::cerr << "  missing key: " << m << '\n';
    }
    Expect(missingKeys.empty(), "every text key and journal.agreement.* key is present in phase2_a4.json");

    AuditCatalogueAndSimulation(catRes.Catalogue, textRes.Table);

    // Negative tests: prove every schema and semantic rule refuses bad content.
    ExpectRefused(files, "p2.changing_friendship",
                  R"("calendar": "calendar.nabapara_2042_43")",
                  R"("calendar": "calendar.missing")",
                  "which does not exist");
    ExpectRefused(files, "p2.changing_friendship",
                  R"("term": "term_1")",
                  R"("term": "term_9")",
                  "has no term 'term_9'");
    ExpectRefused(files, "p2.changing_friendship",
                  R"("calendarSpan": "unit_test_1")",
                  R"("calendarSpan": "no_such_span")",
                  "has no span 'no_such_span'");
    ExpectRefused(files, "p2.changing_friendship",
                  R"("subject": "bengali")",
                  R"("subject": "physics")",
                  "timetable_period does not match bellSchedule");
    ExpectRefused(files, "p2.changing_friendship",
                  R"("startMinute": 840)",
                  R"("startMinute": 360)",
                  "care_handover must be care_home at location.nabapara.care_home inside 07:00..15:00");
    ExpectRefused(files, "p2.changing_friendship",
                  R"("startMinute": 1110)",
                  R"("startMinute": 1000)",
                  "home_evening must lie inside 18:00..22:00");
    ExpectRefused(files, "p2.group_project",
                  R"("proposedNewCast": true)",
                  R"("proposedNewCast": false)",
                  "cast metadata mismatch for 'person.deven_chakraborty'");
    ExpectRefused(files, "p2.group_project",
                  R"("humiliating": false)",
                  R"("humiliating": true)",
                  "is marked humiliating");
    ExpectRefused(files, "p2.group_project",
                  R"("relationshipEvent": "school.project.bench_roles_divided")",
                  R"("relationshipEvent": "school.project.caste_preference")",
                  "is not fair");
    ExpectRefused(files, "p2.group_project",
                  R"("threadEvent": "notes_returned")",
                  R"("threadEvent": "unknown_thread_event")",
                  "is not in threadTemplate");
    ExpectRefused(files, "p2.household_responsibility",
                  R"("resolution": "redesigned")",
                  R"("resolution": "unknown_resolution")",
                  "names unknown resolution");
    ExpectRefused(files, "p2.household_responsibility",
                  R"("nextScene": "scene_family_rota_talk")",
                  R"("nextScene": "scene_evening_settlement")",
                  "scene 'scene_family_rota_talk' is unreachable for origin 'family'");

    // Missing text key detection.
    SchoolTextTable incompleteText = textRes.Table;
    incompleteText.Entries.erase("journal.agreement.a4_tara_pickup_shared");
    Expect(!ValidateSchoolSceneTextCoverage(catRes.Catalogue, incompleteText).empty(),
           "missing journal.agreement key is caught by ValidateSchoolSceneTextCoverage");

    std::cout << "PASS: " << Assertions << " school life content assertions\n";
    return 0;
}
