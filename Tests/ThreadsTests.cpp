// ROADMAP M3-06: thread templates and instances. Usage: PerfectWorldThreadTests <path to Data/Content>
// This file: loading, the six real templates, instantiation and its refusals. The mutation rules are in
// ThreadsContentTests.cpp, the clock and triggers in ThreadsRunTests.cpp, equivalence and random runs in
// ThreadsEquivalenceTests.cpp, the text form in ThreadsSaveTests.cpp.
#include "ThreadsTestSupport.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace ThrTest
{
void TestMutations(); // ThreadsContentTests.cpp

World& TheWorld()
{
    static World world;
    return world;
}

namespace
{
std::vector<ContentFile> ReadDirectory(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> paths;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    std::vector<ContentFile> files;
    for (const fs::path& path : paths)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        files.push_back({"/" + fs::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

constexpr const char* SixIds[] = {"thread.borrowed_notes", "thread.canal_side_stall", "thread.evening_rota",
                                  "thread.hidden_pickup", "thread.refused_invitations", "thread.sports_day_rival"};

void TestSixTemplates()
{
    World& world = TheWorld();
    Expect(world.Templates.Templates.size() == 6, "the year has six thread templates");
    for (std::size_t i = 0; i < 6; ++i)
        Expect(world.Templates.Templates[i].Id == SixIds[i], std::string("the templates are sorted by ID: ") + SixIds[i]);

    std::set<ThreadSituation> situations;
    std::set<std::string> scenes;
    for (const ThreadTemplate& templ : world.Templates.Templates)
    {
        const std::vector<std::string> problems = ValidateThreadTemplate(templ, world.Calendar);
        Expect(problems.empty(), templ.Id + " is valid: " + (problems.empty() ? std::string() : problems.front()));
        situations.insert(templ.Situation);
        Expect(templ.CalendarId == CalendarId, templ.Id + " names the school calendar");
        Expect(templ.Stages.back().Intensity == ThreadMaxIntensity, templ.Id + " ends at maximum intensity");
        Expect(templ.Stages.front().Expression.Channel != ThreadChannel::Scene, templ.Id + " begins quietly");
        Expect(templ.Stages.size() >= 4 && templ.Resolutions.size() >= 3 && templ.Triggers.size() >= 4, templ.Id + " is a full template");
        std::int32_t clock = 0;
        for (const ThreadStage& stage : templ.Stages)
        {
            clock += stage.DwellDays;
            Expect(!stage.Expression.Observe.empty(), templ.Id + " has something to observe at every stage");
            if (!stage.Expression.Scene.empty())
                Expect(scenes.insert(stage.Expression.Scene).second, "scene tokens are unique: " + stage.Expression.Scene);
        }
        Expect(clock >= 30 && clock <= ThreadMaxClockDays, templ.Id + " has a clock of a month or more that fits the year");
        Expect(ThreadEventSlotCount(templ) >= 1 && ThreadEventSlotCount(templ) <= ThreadMaxEventSlots, templ.Id + " counts events");
        const ThreadResolution* fallback = ThreadFindResolution(templ, templ.DefaultResolution);
        Expect(fallback != nullptr && fallback->Outcome != ThreadOutcome::Repaired, templ.Id + " does not mend itself");
        // The spine the documents ask for: something the player does can change the course.
        bool mends = false;
        for (const ThreadTrigger& trigger : templ.Triggers)
            mends = mends || (trigger.Effect == ThreadEffectKind::Resolve && trigger.Resolution != templ.DefaultResolution);
        Expect(mends, templ.Id + " has a way out other than waiting");
    }
    Expect(situations.size() == 4, "school, home, neighbourhood and friendship are all covered");
    Expect(world.Templates.Find("thread.nothing") == nullptr, "an unknown template is not found");
}

void TestInstantiate()
{
    World& world = TheWorld();
    EventLog log;
    for (const ThreadTemplate& templ : world.Templates.Templates)
    {
        ThreadSet set;
        const ThreadOpenResult opened = ThreadOpen(set, world.Templates, templ.Id, Bind(templ, 0), world.Registry, Facts(&log, nullptr, nullptr), 360);
        Expect(opened.Ok() && opened.Instance == "thr_1", templ.Id + " opens");
        Expect(opened.Events.size() == 2 && opened.Events[0].Kind == ThreadEventKind::Opened &&
                   opened.Events[1].Kind == ThreadEventKind::Expressed,
               templ.Id + " opens with Opened and Expressed");
        Expect(set.Active.size() == 1 && set.NextInstance == 2, "one live thread");
        const ThreadInstance& live = set.Active.front();
        Expect(live.Stage == 0 && live.OpenedAt == 360 && live.Now == 360 && live.Counts.size() == ThreadEventSlotCount(templ),
               "the instance starts at stage 0");
        Expect(ThreadIntensity(templ, live) == templ.Stages.front().Intensity, "its intensity is the first stage's");
        const ThreadExpression* expression = ThreadCurrentExpression(templ, live);
        Expect(expression != nullptr && expression->Observe == templ.Stages.front().Expression.Observe, "it shows the first expression");
        for (std::size_t i = 0; i < templ.Roles.size(); ++i)
            Expect(ThreadPersonInRole(templ, live, templ.Roles[i].Id) == Person(static_cast<std::int64_t>(i)), "each role holds its person");
        Expect(ThreadPersonInRole(templ, live, "nobody").empty(), "an unknown role holds nobody");
        Expect(ThreadSetMatchesTemplates(set, world.Templates) && ThreadFind(set, "thr_1") == &live && ThreadFind(set, "thr_2") == nullptr,
               "the set fits its templates and is findable");
    }
}

void TestOpenRefusals()
{
    World& world = TheWorld();
    const ThreadTemplate& templ = Template("thread.hidden_pickup");
    EventLog log;
    const ThreadFacts facts = Facts(&log, nullptr, nullptr);
    ThreadSet set;
    const auto open = [&](const std::string& id, const std::vector<ThreadRoleBinding>& bindings, GameInstant at) {
        const ThreadSet before = set;
        const ThreadOpenResult result = ThreadOpen(set, world.Templates, id, bindings, world.Registry, facts, at);
        Expect(result.Ok() || set == before, "a refused open changes nothing");
        return result.Status;
    };
    Expect(open("thread.nothing", Bind(templ, 0), 0) == ThreadStatus::BadTemplate, "an unknown template is refused");
    Expect(open(templ.Id, Bind(templ, 0), -1) == ThreadStatus::BadTime, "a negative instant is refused");
    std::vector<ThreadRoleBinding> bindings = Bind(templ, 0);
    bindings.pop_back();
    Expect(open(templ.Id, bindings, 0) == ThreadStatus::BadRoles, "a missing role is refused");
    bindings = Bind(templ, 0);
    bindings.push_back({"extra", Person(9)});
    Expect(open(templ.Id, bindings, 0) == ThreadStatus::BadRoles, "an extra role is refused");
    bindings = Bind(templ, 0);
    bindings[1].Role = "player";
    Expect(open(templ.Id, bindings, 0) == ThreadStatus::BadRoles, "a role bound twice is refused");
    bindings = Bind(templ, 0);
    bindings[2].Role = "teacher";
    Expect(open(templ.Id, bindings, 0) == ThreadStatus::BadRoles, "a role the template lacks is refused");
    bindings = Bind(templ, 0);
    bindings[1].Person = "npc_unmet";
    Expect(open(templ.Id, bindings, 0) == ThreadStatus::BadPeople, "an unregistered person is refused");
    bindings = Bind(templ, 0);
    bindings[2].Person = bindings[0].Person;
    Expect(open(templ.Id, bindings, 0) == ThreadStatus::BadPeople, "one person in two roles is refused");

    Expect(open(templ.Id, Bind(templ, 0), 1000) == ThreadStatus::Ok, "a valid open succeeds");
    Expect(open(templ.Id, Bind(templ, 0), 1000) == ThreadStatus::Duplicate, "the same template for the same people is refused");
    Expect(open(templ.Id, Bind(templ, 3), 1000) == ThreadStatus::Ok, "other people may have the same template");
    Expect(open("thread.sports_day_rival", Bind(Template("thread.sports_day_rival"), 0), 999) == ThreadStatus::BadTime,
           "an open earlier than a live thread's instant is refused");
    Expect(open("thread.sports_day_rival", Bind(Template("thread.sports_day_rival"), 0), 1000) == ThreadStatus::Ok,
           "another template for the same people is fine");

    ThreadSet crowded;
    for (std::int64_t n = 0; n < static_cast<std::int64_t>(ThreadMaxActive); ++n)
    {
        const ThreadOpenResult result =
            ThreadOpen(crowded, world.Templates, templ.Id, Bind(templ, n), world.Registry, facts, 5);
        Expect(result.Ok(), "forty live threads fit");
    }
    Expect(ThreadOpen(crowded, world.Templates, templ.Id, Bind(templ, 50), world.Registry, facts, 5).Status == ThreadStatus::Full,
           "the forty-first is refused");
    Expect(crowded.Active.size() == ThreadMaxActive, "and nothing was added");
}

void TestClose()
{
    World& world = TheWorld();
    const ThreadTemplate& templ = Template("thread.evening_rota");
    EventLog log;
    ThreadSet set;
    Expect(ThreadOpen(set, world.Templates, templ.Id, Bind(templ, 0), world.Registry, Facts(&log, nullptr, nullptr), 100).Ok(), "opens");
    const ThreadSet before = set;
    Expect(ThreadClose(set, world.Templates, "thr_9", "redesigned", 200).Status == ThreadStatus::NotFound, "an unknown thread is refused");
    Expect(ThreadClose(set, world.Templates, "thr_1", "elsewhere", 200).Status == ThreadStatus::BadResolution, "an unknown resolution is refused");
    Expect(ThreadClose(set, world.Templates, "thr_1", "redesigned", 99).Status == ThreadStatus::BadTime, "closing in the past is refused");
    Expect(set == before, "refusals change nothing");
    const ThreadAdvanceResult closed = ThreadClose(set, world.Templates, "thr_1", "redesigned", 5000);
    Expect(closed.Ok() && closed.Events.size() == 1 && closed.Events[0].Kind == ThreadEventKind::Resolved &&
               closed.Events[0].Detail == "redesigned" && closed.Events[0].At == 5000,
           "closing records the resolution");
    Expect(set.Active.empty() && set.Resolved.size() == 1 && set.Resolved[0].Resolution == "redesigned" && set.Resolved[0].At == 5000 &&
               set.Resolved[0].People.size() == 2,
           "the thread moves to the resolved history");
    Expect(ThreadClose(set, world.Templates, "thr_1", "redesigned", 6000).Status == ThreadStatus::NotFound, "it cannot be closed twice");
    Expect(ThreadSetMatchesTemplates(set, world.Templates), "the resolved record fits its template");
    // Instance numbers are never reused.
    const ThreadOpenResult again = ThreadOpen(set, world.Templates, templ.Id, Bind(templ, 0), world.Registry, Facts(&log, nullptr, nullptr), 6000);
    Expect(again.Ok() && again.Instance == "thr_2", "a new thread takes a new number");
}
} // namespace

bool LoadWorld(const std::string& root)
{
    World& world = TheWorld();
    world.Files = ReadDirectory(root);
    world.Loaded = LoadContent(world.Files);
    for (const ContentIssue& issue : world.Loaded.Issues)
        std::cerr << issue.File << ' ' << issue.Id << ' ' << issue.Message << '\n';
    if (!world.Loaded.Ok())
        return false;
    const ThreadTemplateResult templates = LoadThreadTemplates(world.Loaded.Db);
    if (!templates.Ok())
    {
        std::cerr << templates.Error << '\n';
        return false;
    }
    world.Templates = templates.Set;
    const SchoolCalendarResult calendars = LoadSchoolCalendars(world.Loaded.Db);
    if (!calendars.Ok())
        return false;
    world.Calendars = calendars.Set;
    world.Calendar = world.Calendars.FindById(CalendarId);
    for (std::int64_t n = 0; n < People; ++n)
    {
        if (!RegisterAuthoredIdentity(world.Registry, Person(n), IdentityFirstRef{}).Ok())
            return false;
    }
    return world.Calendar != nullptr;
}
} // namespace ThrTest

int main(int argc, char** argv)
{
    using namespace ThrTest;
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldThreadTests <Data/Content>\n";
        return 2;
    }
    Expect(LoadWorld(argv[1]), "Data/Content validates with the thread templates");
    TestSixTemplates();
    TestInstantiate();
    TestOpenRefusals();
    TestClose();
    TestMutations();
    TestSchedule();
    TestTriggers();
    TestCalendarTriggers();
    TestEquivalence();
    TestRandomOperations();
    TestSave();
    std::cout << "PASS: " << Checks << " checks\n";
    return 0;
}
