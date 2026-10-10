// ROADMAP M2-03: the activity content rules, each shown to refuse a bad record.
#include "Activities.h"
#include "ActivitiesTestSupport.h"

using namespace PerfectWorld;

namespace
{
bool HasIssue(const ContentLoadResult& result, const std::string& text)
{
    return std::any_of(result.Issues.begin(), result.Issues.end(),
                       [&](const ContentIssue& issue) { return issue.Message.find(text) != std::string::npos; });
}

/** The real content with one replacement made in one activity file. */
ContentLoadResult LoadMutated(const std::vector<ContentFile>& files, const std::string& activity, const std::string& from,
                              const std::string& to)
{
    std::vector<ContentFile> copy = files;
    bool found = false;
    for (ContentFile& file : copy)
    {
        if (file.Path.find("/activity/" + activity + ".json") == std::string::npos)
            continue;
        const std::size_t at = file.Text.find(from);
        Check(at != std::string::npos, "the mutation target text exists in the activity file");
        if (at != std::string::npos)
            file.Text.replace(at, from.size(), to);
        found = true;
    }
    Check(found, "the mutated activity file exists");
    return LoadContent(copy);
}
} // namespace

void TestContentRules(const std::vector<ContentFile>& files)
{
    const auto refuses = [&](const char* activity, const char* from, const char* to, const char* issue) {
        const ContentLoadResult result = LoadMutated(files, activity, from, to);
        Check(HasIssue(result, issue), issue);
        return result;
    };
    const ContentLoadResult split = refuses("activity.study_at_home", "\"permille\": 800", "\"permille\": 700", "sums to 900 parts per thousand");
    const ActivityCatalogueResult refused = LoadActivityCatalogue(split.Db);
    Check(!refused.Ok() && refused.Error.find("activity.study_at_home") != std::string::npos && refused.Catalogue.Defs.empty(),
          "the loader refuses a split that is not whole");
    refuses("activity.study_at_home", "\"permille\": 800", "\"permille\": 900", "sums to 1100 parts per thousand");
    refuses("activity.attend_class", "skill.communication", "skill.academics", "names skill.academics twice");
    refuses("activity.study_at_home", "skill.self_management", "skill.alchemy", "which does not exist");
    refuses("activity.study_at_home", "{ \"skill\": \"skill.self_management\", \"permille\": 200 }",
            "{ \"skill\": \"skill.fitness\", \"permille\": 50 }, { \"skill\": \"skill.computing\", \"permille\": 50 }, "
            "{ \"skill\": \"skill.creative\", \"permille\": 50 }, { \"skill\": \"skill.commerce\", \"permille\": 50 }",
            "between one and four skills");
    refuses("activity.study_at_home", "\"durationMinutes\": 120", "\"durationMinutes\": -120", "outside 1..1440");
    refuses("activity.study_at_home", "\"durationMinutes\": 120", "\"durationMinutes\": 0", "outside 1..1440");
    refuses("activity.study_at_home", "\"durationMinutes\": 120", "\"durationMinutes\": 1441", "outside 1..1440");
    refuses("activity.study_at_home", "\"durationMinutes\": 120", "\"durationMinutes\": 1.5", "must be a whole number");
    refuses("activity.study_at_home", "\"milli\": 150", "\"milli\": 151", "above one twentieth");
    refuses("activity.study_at_home", "\"milli\": 150", "\"milli\": 1001", "outside 1..1000");
    refuses("activity.eat_meal", "\"outcome\": \"ate\",", "\"outcome\": \"ate\", \"evidence\": [{ \"attribute\": \"attribute.body\", \"milli\": 1 }],",
            "needs a learning allowance");
    refuses("activity.sleep_night", "\"activity\": \"sleeping\"", "\"activity\": \"sleeping\", \"meal\": \"snack\"", "a meal is eaten exactly when");
    refuses("activity.eat_meal", ", \"meal\": \"full\"", "", "a meal is eaten exactly when");
    refuses("activity.sleep_night", "\"activity\": \"sleeping\"", "\"activity\": \"resting\"", "needs the condition activity sleeping");
    refuses("activity.study_at_home", "\"mode\": \"scene\"", "\"mode\": \"slow\"", "has an unknown value");
    refuses("activity.study_at_home", "\"resolve\": \"on_complete\"", "\"resolve\": \"per_beat\"", "has an unknown value");
    refuses("activity.study_at_home", "\"clock\": { \"mode\": \"scene\", \"interrupt\": \"at_beat\", \"resolve\": \"on_complete\" },", "",
            "missing required field \"clock\"");
    refuses("activity.study_at_home", "\"outcome\": \"studied\",", "\"outcome\": \"studied\", \"frobnicate\": 1,", "unknown field");
    refuses("activity.study_at_home", "\"outcome\": \"studied\",", "\"outcome\": \"studied\", \"religionBonus\": 5,", "keys a modifier on religion");
    refuses("activity.study_at_home", "\"allowanceMilliXp\": 3000", "\"allowanceMilliXp\": 20001", "outside 1..20000");
}
