// ROADMAP M3-06: every template rule, exercised by editing a real record and expecting the validator to refuse it
// for exactly that reason. Part of PerfectWorldThreadTests.
#include "ThreadsTestSupport.h"

#include <algorithm>

namespace ThrTest
{
namespace
{
struct Mutation
{
    const char* File;
    std::string From;
    std::string To;
    std::string Issue;
    std::string From2 = std::string();
    std::string To2 = std::string();
};

constexpr const char* Notes = "thread.borrowed_notes.json";

bool HasIssue(const ContentLoadResult& result, const std::string& text)
{
    return std::any_of(result.Issues.begin(), result.Issues.end(),
                       [&](const ContentIssue& issue) { return issue.Message.find(text) != std::string::npos; });
}

void Replace(std::string& text, const std::string& from, const std::string& to, const std::string& what)
{
    const std::size_t at = text.find(from);
    Expect(at != std::string::npos, "the mutation target exists: " + what + " / " + from.substr(0, 60));
    if (at != std::string::npos)
        text.replace(at, from.size(), to);
}

void RunMutation(const Mutation& mutation)
{
    std::vector<ContentFile> files = TheWorld().Files;
    bool found = false;
    for (ContentFile& file : files)
    {
        if (file.Path.find(mutation.File) == std::string::npos)
            continue;
        Replace(file.Text, mutation.From, mutation.To, mutation.Issue);
        if (!mutation.From2.empty())
            Replace(file.Text, mutation.From2, mutation.To2, mutation.Issue);
        found = true;
    }
    Expect(found, std::string("the file exists: ") + mutation.File);
    const ContentLoadResult result = LoadContent(files);
    Expect(!result.Ok(), "the mutation is refused: " + mutation.Issue);
    if (!HasIssue(result, mutation.Issue))
    {
        for (const ContentIssue& issue : result.Issues)
            std::cerr << "  issue: " << issue.Message << '\n';
    }
    Expect(HasIssue(result, mutation.Issue), "the refusal says: " + mutation.Issue);
}

std::vector<Mutation> Mutations()
{
    const std::string player = R"("parties": ["player", "classmate"])";
    return {
        // Shape and limits.
        {Notes, R"("kind": "debt")", R"("kind": "feud")", "has an unknown value"},
        {Notes, R"("opensWhen": "The player takes a classmate's compressed notes to prepare for the first unit test and does not give them back that week.")",
         R"("opensWhen": "")", "opensWhen is empty"},
        {Notes, R"("name": "The classmate who lent the notes" })", R"("name": "The classmate who lent the notes" }, { "id": "ghost", "name": "Nobody" })",
         "role ghost is never used by an expression or a condition"},
        {Notes, R"({ "id": "classmate", "name": "The classmate who lent the notes" })", R"({ "id": "player", "name": "The classmate who lent the notes" })",
         "two roles are both called player"},
        {Notes, R"("id": "classmate", "name")", R"("id": "caste_mate", "name")", "is keyed on 'caste', which no thread may depend on"},
        {Notes, R"("id": "classmate", "name")", R"("id": "Classmate", "name")", "must be lowercase letters, digits and underscores"},
        // The escalation clock.
        {Notes, R"("intensity": 450)", R"("intensity": 150)", "does not rise above 200"},
        {Notes, R"("intensity": 1000, "dwellDays": 14)", R"("intensity": 900, "dwellDays": 14)", "the last stage must reach intensity 1000, found 900"},
        {Notes, R"("dwellDays": 10)", R"("dwellDays": 0)", "dwell 0 days is outside 1..120"},
        {Notes, R"("dwellDays": 10)", R"("dwellDays": 120)", "the clock runs 218 days in total, over the limit of 200", R"("dwellDays": 14)",
         R"("dwellDays": 80)"},
        // Expressions.
        {Notes, R"({ "channel": "ambient", "by": "classmate",)", R"({ "channel": "scene", "by": "classmate", "scene": "scene_x",)",
         "the first stage must not be a scene: a thread begins quietly"},
        {Notes, R"("channel": "scene", "by": "classmate", "scene": "scene_notes_confrontation",)", R"("channel": "scene", "by": "classmate",)",
         "is a scene but names no scene token"},
        {Notes, R"("scene": "scene_notes_confrontation")", R"("scene": "Scene Notes")", "which is not a lowercase token"},
        {Notes, R"("by": "classmate",)", R"("by": "nobody",)", "expression is by 'nobody', which is not a role of the template"},
        {Notes, R"("observe": "Their notes sit in your bag, written in a shorthand you only half follow. They have not asked for them back.")",
         R"("observe": "")", "has no observable text"},
        {Notes, R"("observe": "'No hurry,' they say, in the tone people use when there is a hurry.")", "\"observe\": \"" + std::string(300, 'x') + "\"",
         "expression text is over 240 characters"},
        // Resolutions.
        {Notes, R"("defaultResolution": "faded")", R"("defaultResolution": "nothing")", "defaultResolution 'nothing' is not one of the resolutions"},
        {Notes, R"("defaultResolution": "faded")", R"("defaultResolution": "shared_understanding")", "an untouched thread does not mend itself"},
        {Notes, R"("effect": "resolve", "resolution": "returned",)", R"("effect": "resolve", "resolution": "shared_understanding",)",
         "resolution returned is never reached: no trigger resolves it and it is not the default"},
        {Notes, R"("effect": "resolve", "resolution": "returned",)", R"("effect": "resolve", "resolution": "vanished",)",
         "names the resolution vanished, which the template does not have"},
        {Notes, R"("effect": "resolve", "resolution": "returned",)", R"("effect": "resolve",)", "resolves but names no resolution"},
        {Notes, R"("effect": "escalate",)", R"("effect": "escalate", "resolution": "returned",)", "names a resolution but its effect is not resolve"},
        {Notes, R"("effect": "escalate",)", R"("effect": "express",)", "expresses but has no expression"},
        // Triggers.
        {Notes, R"("armedFrom": "lent", "armedUntil": "awkward")", R"("armedFrom": "awkward", "armedUntil": "lent")", "before it is armed from stage"},
        {Notes, R"("armedUntil": "awkward")", R"("armedUntil": "nowhere")", "armedUntil 'nowhere' is not a stage of the template"},
        {Notes, R"("id": "worked_through_together")", R"("id": "returned_in_time")", "two triggers are both called returned_in_time"},
        {Notes, R"("when": [ { "kind": "relationship", "a": "player", "b": "classmate", "dimension": "owed", "op": "at_least", "value": 3000 } ])",
         R"("when": [ ])", "needs 1 to 4 conditions, found 0"},
        {Notes, R"({ "kind": "event", "event": "notes_returned", )" + player + " }", R"({ "kind": "days_open", "days": 5 })",
         "no event trigger eases or resolves the thread, so nothing the player does can change its course",
         R"({ "kind": "event", "event": "notes_explained_together", )" + player + " }", R"({ "kind": "days_open", "days": 6 })"},
        // Conditions.
        {Notes, R"({ "kind": "days_in_stage", "days": 2 })", R"({ "kind": "days_in_stage" })", "needs days"},
        {Notes, R"({ "kind": "days_in_stage", "days": 2 })", R"({ "kind": "days_in_stage", "days": 400 })", "days 400 is outside 1..365"},
        {Notes, R"("span": "unit_test_1", "phase": "ended")", R"("span": "unit_test_1", "phase": "ended", "days": 3)", "(calendar) does not take days"},
        {Notes, R"("span": "unit_test_1")", R"("span": "unit_test_9")", "names the calendar span 'unit_test_9', which calendar.nabapara_2042_43 does not have"},
        {Notes, R"("calendar": "calendar.nabapara_2042_43",)", "", "is a calendar condition but the template names no calendar"},
        {Notes, R"("calendar": "calendar.nabapara_2042_43")", R"("calendar": "calendar.missing")", "calendar.missing, which does not exist"},
        {Notes, player, R"("parties": ["player", "teacher"])", "names the role 'teacher', which is not a role of the template"},
        {Notes, R"("parties": ["player", "classmate"] } ] },)", R"("parties": ["player", "classmate", "player"] } ] },)", "names the party player twice"},
        {Notes, R"("a": "player", "b": "classmate")", R"("a": "player", "b": "player")", "compares a role with itself"},
        {Notes, R"("value": 3000)", R"("value": 300000)", "is outside -100000..100000"},
        {Notes, R"("event": "notes_returned")", R"("event": "stream_notes_returned")", "event type 'stream_notes_returned' is keyed on 'stream'"},
        {Notes, R"("event": "notes_returned")", R"("event": "Notes Returned")", "event type 'Notes Returned' is not a lowercase token"},
        {Notes, R"("event": "notes_returned", )", R"("event": "notes_returned", "count": 30, )", "count 30 is outside 1..20"},
        // The fairness rule runs on templates like every record.
        {Notes, R"("name": "Borrowed notes",)", R"("name": "Borrowed notes", "religionBonus": 5,)", "unknown field"},
        {"thread.evening_rota.json", R"("id": "carer", "name")", R"("id": "language_carer", "name")", "is keyed on 'language'"},
    };
}
} // namespace

void TestMutations()
{
    const std::vector<Mutation> mutations = Mutations();
    Expect(mutations.size() >= 40, "the rule table is complete");
    for (const Mutation& mutation : mutations)
        RunMutation(mutation);

    // A calendar condition cannot be checked without a calendar; a template with none has no problem of that kind.
    const ThreadTemplate& notes = Template("thread.borrowed_notes");
    const std::vector<std::string> noCalendar = ValidateThreadTemplate(notes, nullptr);
    Expect(!noCalendar.empty() && noCalendar.front().find("cannot be checked") != std::string::npos, "a missing calendar is reported");
    ThreadTemplate twin = notes;
    twin.Stages[1].Id = twin.Stages[0].Id;
    const std::vector<std::string> twinProblems = ValidateThreadTemplate(twin, TheWorld().Calendar);
    Expect(!twinProblems.empty() && twinProblems.front() == "two stages are both called lent", "two stages with one ID are refused");
    ThreadTemplate hollow = notes;
    hollow.Triggers.clear();
    Expect(!ValidateThreadTemplate(hollow, TheWorld().Calendar).empty(), "a template with no triggers is refused");
}
} // namespace ThrTest
