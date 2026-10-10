// ROADMAP M2-08: the structure, problem family and problem case content rules, each shown to
// refuse a bad record, and the catalogue loader's own refusals. The edits are made to the real
// Data/Content in memory, so each test changes exactly one thing in valid data.
#include "StructureModelTestSupport.h"

using namespace PerfectWorld;

namespace
{
struct Edit
{
    std::string Path; // a fragment of the file path
    std::string From;
    std::string To;
};

ContentLoadResult LoadEdited(std::vector<ContentFile> files, const std::vector<Edit>& edits)
{
    for (const Edit& edit : edits)
    {
        bool found = false;
        for (ContentFile& file : files)
        {
            if (file.Path.find(edit.Path) == std::string::npos)
                continue;
            const std::size_t at = file.Text.find(edit.From);
            Check(at != std::string::npos, "the edit target text exists in the file");
            if (at != std::string::npos)
                file.Text.replace(at, edit.From.size(), edit.To);
            found = true;
        }
        Check(found, "the edited file exists");
    }
    return LoadContent(files);
}

bool HasIssue(const ContentLoadResult& result, const std::string& text)
{
    return std::any_of(result.Issues.begin(), result.Issues.end(), [&](const ContentIssue& issue) {
        return issue.Message.find(text) != std::string::npos && !issue.File.empty();
    });
}

void Refuses(const std::vector<ContentFile>& files, const std::vector<Edit>& edits, const char* issue)
{
    const ContentLoadResult result = LoadEdited(files, edits);
    if (!HasIssue(result, issue))
    {
        std::cerr << "expected: " << issue << '\n';
        for (const ContentIssue& found : result.Issues)
            std::cerr << "  got: " << found.Message << '\n';
    }
    Check(HasIssue(result, issue), issue);
}

const char* const Tank = "problem_case.xi_water_tank_level.json";
const char* const Family = "problem_family.xi_stock_and_flow.json";
const char* const Structure = "structure.model_a_system.json";
} // namespace

void TestContentRules(const std::vector<ContentFile>& files)
{
    Check(LoadContent(files).Ok(), "the unedited content is valid");

    // Every case fits the family's lists and template.
    Refuses(files, {{Family, "\"roles\": [\"solver\", \"experimenter\", \"recorder\", \"critic\", \"presenter\"]",
                     "\"roles\": [\"solver\", \"experimenter\", \"recorder\", \"critic\"]"}},
            "its role presenter is not a role the family allows");
    Refuses(files, {{Family, "\"faults\": [\"none\", \"timing\", \"hidden_flow\", \"scale\"]", "\"faults\": [\"none\", \"hidden_flow\", \"scale\"]"}},
            "its fault timing is not a fault the family allows");
    Refuses(files, {{Tank, "\"kind\": \"context\"", "\"kind\": \"stock\""}}, "wants exactly 1 stock quantity, found 2");
    Refuses(files, {{Tank, "\"kind\": \"time\"", "\"kind\": \"context\""}}, "wants exactly 1 time quantity, found 0");
    Refuses(files, {{Tank, "\"kind\": \"inflow\"", "\"kind\": \"context\""}}, "wants 1 to 3 flow quantities, found 0");
    Refuses(files, {{Tank, "\"given\": false", "\"given\": true"}}, "exactly one quantity must be unknown (given false), found 0");
    Refuses(files, {{Tank, "\"unknown\": \"stock\"", "\"unknown\": \"inflow\""}}, "the unknown quantity is a stock, but the case says");
    Refuses(files, {{Tank, "\"method\": \"method.balance_stock\"", "\"method\": \"method.nope\""}}, "is not one of the family's methods");
    Refuses(files, {{Tank, "\"method\": \"method.balance_stock\"", "\"method\": \"method.time_to_target\""}}, "does not solve for an unknown stock");
    Refuses(files, {{Tank, "\"id\": \"l_flow_adds\", \"from\": \"q_tap_flow\", \"to\": \"q_gained\", \"effect\": \"adds\"",
                     "\"id\": \"l_flow_adds\", \"from\": \"q_tap_flow\", \"to\": \"q_gained\", \"effect\": \"subtracts\""}},
            "sound link l_flow_adds must join an inflow (adds)");
    Refuses(files, {{Tank, "\"id\": \"l_flow_adds\", \"from\": \"q_tap_flow\", \"to\": \"q_gained\", \"effect\": \"adds\", \"sound\": true",
                     "\"id\": \"l_flow_adds\", \"from\": \"q_tap_flow\", \"to\": \"q_gained\", \"effect\": \"adds\", \"sound\": false"}},
            "needs exactly one sound link from q_tap_flow, found 0");
    Refuses(files, {{Tank, "\"from\": \"q_paint\"", "\"from\": \"q_nowhere\""}}, "must join two different quantities of the case");
    Refuses(files, {{Tank, "\"id\": \"l_gain_drives_flow\"", "\"id\": \"l_flow_adds\""}}, "link IDs must be well-formed and unique");
    Refuses(files, {{Tank, "\"id\": \"q_paint\"", "\"id\": \"Q Paint\""}}, "quantity IDs must be well-formed and unique");
    Refuses(files, {{Family, "\"minDistractors\": 1", "\"minDistractors\": 3"}}, "wants 3 to 4 unsound links, found 2");
    Refuses(files, {{Family, "\"maxContexts\": 2", "\"maxContexts\": 0"}}, "allows at most 0 context quantities");
    Refuses(files, {{Family, "\"minFlows\": 1", "\"minFlows\": 4"}}, "the template's minFlows is above its maxFlows");

    // The variation must be real: no repeated triple, every listed value used.
    Refuses(files, {{"xi_stationery_notebooks", "\"role\": \"recorder\"", "\"role\": \"experimenter\""},
                    {"xi_stationery_notebooks", "\"scarce\": \"readings\"", "\"scarce\": \"help\""}},
            "repeats the unknown, role and scarce resource of problem_case.xi_roof_rainwater_tank");
    Refuses(files, {{"xi_stationery_notebooks", "\"fault\": \"none\"", "\"fault\": \"timing\""}, {"xi_roof_rainwater_tank", "\"fault\": \"none\"", "\"fault\": \"timing\""}},
            "no case uses the fault none the family lists");
    Refuses(files, {{"xi_water_tank_level", "\"subject\": \"physics\"", "\"subject\": \"geography\""}, {"xi_overhead_tank_draw", "\"subject\": \"physics\"", "\"subject\": \"geography\""}},
            "no case uses the subject physics the family lists");
    Refuses(files, {{Family, "\"knownAtMilli\": 140000 }", "\"knownAtMilli\": 140000 },\n    { \"id\": \"method.unused\", \"name\": \"Unused\", \"unknowns\": [\"stock\"], \"knownAtMilli\": 150000 }"}},
            "no case uses the method method.unused the family lists");
    Refuses(files, {{Family, "\"unknowns\": [\"stock\", \"inflow\", \"outflow\", \"time\"]", "\"unknowns\": [\"stock\", \"inflow\", \"outflow\", \"time\", \"time\"]"}},
            "the family's unknowns names time twice");
    Refuses(files, {{Family, "\"subjects\": [\"mathematics\"", "\"subjects\": [\"chemistry\""}}, "names chemistry, which is not a known value");

    // At least eight cases for a classroom model family, declared and actual.
    std::vector<ContentFile> fewer;
    for (const ContentFile& file : files)
    {
        if (file.Path.find("xi_roof_rainwater_tank") == std::string::npos)
            fewer.push_back(file);
    }
    Check(HasIssue(LoadContent(fewer), "needs at least 8 cases, found 7"), "seven cases are too few for a family that declares eight");
    Refuses(files, {{Family, "\"minCases\": 8", "\"minCases\": 3"}}, "a classroom model family declares at least 8 cases");
    Refuses(files, {{Family, "\"setting\": \"classroom\"", "\"setting\": \"playground\""}}, "has an unknown value");

    // The structure record agrees with the runner's constants.
    Refuses(files, {{Structure, "[\"build_model\", \"resolve_discrepancy\", \"present_claim\"]", "[\"resolve_discrepancy\", \"build_model\", \"present_claim\"]"}},
            "the model structure's beats must be build_model, resolve_discrepancy, present_claim, in that order");
    Refuses(files, {{Structure, "\"periodUnits\": 9", "\"periodUnits\": 8"}}, "the model structure's period is 9 units of 5 minutes");
    Refuses(files, {{Structure, "\"claims\": [\"estimate\", \"bounded_range\", \"need_more_measurement\"]", "\"claims\": [\"estimate\"]"}},
            "the model structure's claims must be");
    Refuses(files, {{Structure, "\"engine\": \"model\"", "\"engine\": \"sculpt\""}}, "has an unknown value");

    // Shape, references and fairness hold for the new types like any other.
    Refuses(files, {{Tank, "\"role\": \"solver\"", "\"role\": \"captain\""}}, "has an unknown value");
    Refuses(files, {{Tank, "\"family\": \"problem_family.xi_stock_and_flow\"", "\"family\": \"problem_family.nope\""}}, "which does not exist");
    Refuses(files, {{Family, "\"structure\": \"structure.model_a_system\"", "\"structure\": \"skill.academics\""}}, "but needs a structure");
    Refuses(files, {{Tank, "\"name\": \"The tank that rose too little\",", "\"name\": \"The tank that rose too little\", \"frobnicate\": 1,"}}, "unknown field");
    Refuses(files, {{Tank, "\"name\": \"The tank that rose too little\",", "\"name\": \"The tank that rose too little\", \"religionBonus\": 5,"}},
            "keys a modifier on religion");
    Refuses(files, {{Tank, "\"given\": false", "\"given\": \"no\""}}, "must be true or false");
    Refuses(files, {{Family, "\"knownAtMilli\": 110000", "\"knownAtMilli\": 1.5"}}, "must be a whole number");
}

void TestCatalogueRules(const std::vector<ContentFile>& files)
{
    const ContentLoadResult real = LoadContent(files);
    const ModelCatalogueResult loaded = LoadModelCatalogue(real.Db);
    Check(loaded.Ok() && loaded.Catalogue.Families.size() == 1 && loaded.Catalogue.Families.front().Cases.size() == 8, "the real content loads as one family of eight");
    const ModelFamilyDef& family = loaded.Catalogue.Families.front();
    Check(family.SkillId == "skill.academics" && family.MinCases == 8 && family.Methods.size() == 3, "the family names its skill, minimum and methods");
    const ModelCaseDef* tank = loaded.Catalogue.FindCase("problem_case.xi_water_tank_level");
    Check(tank != nullptr && tank->FamilyId == family.Id && tank->Role == ModelRole::Solver && tank->Unknown == ModelUnknown::Stock &&
              tank->Scarce == ModelScarce::Time && tank->Fault == ModelFault::Timing && tank->Subject == "physics",
          "the tank case is read");
    Check(tank->MethodId == "method.balance_stock" && tank->MethodKnownAtMilli == 110000 && tank->Quantities.size() == 4 && tank->Links.size() == 4 &&
              tank->Quantities.front().Id == "q_gained" && !tank->Quantities.front().Given && !tank->Brief.empty(),
          "its method, quantities and links are read in authored order");
    Check(loaded.Catalogue.FindCase("problem_case.xi_nope") == nullptr && loaded.Catalogue.FindFamily("problem_family.nope") == nullptr, "unknown IDs are not found");
    Check(loaded.Catalogue.FindFamily(family.Id) == &family, "the family is found by ID");

    // The loader refuses (never coerces) records the validator would also have refused.
    const auto refused = [&](const std::vector<Edit>& edits, const char* text) {
        const ContentLoadResult broken = LoadEdited(files, edits);
        const ModelCatalogueResult result = LoadModelCatalogue(broken.Db);
        Check(!result.Ok() && result.Error.find(text) != std::string::npos && result.Catalogue.Families.empty(), text);
    };
    refused({{Tank, "\"method\": \"method.balance_stock\"", "\"method\": \"method.nope\""}}, "problem_case.xi_water_tank_level: the method method.nope is not one of the family's methods");
    refused({{Tank, "\"family\": \"problem_family.xi_stock_and_flow\"", "\"family\": \"problem_family.nope\""}}, "its family problem_family.nope does not exist");
    refused({{Tank, "\"sound\": true", "\"sound\": false"}, {Tank, "\"sound\": true", "\"sound\": false"}}, "at least one sound and one unsound link");
    refused({{Tank, "\"sound\": false", "\"sound\": true"}, {Tank, "\"sound\": false", "\"sound\": true"}}, "at least one sound and one unsound link");
    refused({{Family, "\"minCases\": 8", "\"minCases\": 0"}}, "minCases must be a positive whole number");
    refused({{Family, "\"id\": \"method.balance_stock\"", "\"id\": \"Method Balance\""}}, "a method needs a well-formed id");
}
