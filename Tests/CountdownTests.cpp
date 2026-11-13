// ROADMAP M4-03: the ten days. This file: the driver, the shared setup, the span and the content.
#include "CountdownTestSupport.h"

#include "ContentRegistry.h"
#include "SchoolCalendar.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace PerfectWorld;

int CdChecks = 0;

namespace
{
DayLoopCatalogues Catalogues;
ActivityCatalogue ActivityCat;
BellScheduleSet BellSet;
SchoolCalendarSet CalendarSet;

std::vector<ContentFile> ReadContent(const std::filesystem::path& root)
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

struct Slot
{
    int Start;
    WeekEntryKind Kind;
    const char* Id;
    const char* Place;
    int Minutes;
    DayPassKind Pass;
    int Priority;
};

// The ordinary things of a day, at fixed hours. Priority 0 is always wanted; the rest are chosen by the style.
constexpr Slot Slots[] = {
    {390, WeekEntryKind::Activity, "activity.eat_meal", "dining", 20, DayPassKind::Wait, 0},
    {420, WeekEntryKind::Activity, "activity.study_at_home", "home", 120, DayPassKind::Wait, 1},
    {540, WeekEntryKind::Activity, "activity.study_at_home", "home", 120, DayPassKind::Wait, 2},
    {660, WeekEntryKind::Activity, "activity.exercise_run", "outdoors", 45, DayPassKind::Wait, 10},
    {750, WeekEntryKind::Activity, "activity.eat_meal", "dining", 20, DayPassKind::Wait, 0},
    {780, WeekEntryKind::Activity, "activity.study_at_home", "home", 120, DayPassKind::Wait, 3},
    {900, WeekEntryKind::Activity, "activity.household_chores", "home", 60, DayPassKind::Wait, 11},
    {960, WeekEntryKind::Activity, "activity.study_at_home", "home", 120, DayPassKind::Wait, 4},
    {1080, WeekEntryKind::Activity, "activity.eat_meal", "dining", 20, DayPassKind::Wait, 0},
    {1110, WeekEntryKind::Spend, "", "", 60, DayPassKind::Rest, 12},
    {1170, WeekEntryKind::Activity, "activity.study_at_home", "home", 120, DayPassKind::Wait, 5},
};
} // namespace

const DayLoopCatalogues& CdCats()
{
    return Catalogues;
}

WorldState CdStartWorld(std::uint64_t seed)
{
    WorldSim sim(seed);
    CdExpect(sim.Submit({CommandSource::System, AdvanceTime{CountdownStartInstant()}}).Accepted, "the clock reaches Awakening Day at 18:00");
    WorldState world = sim.State();
    world.DayLoop.Condition.Energy = MeterMax;
    world.DayLoop.Attendance = StartAttendanceRecord(CalendarDayOfInstant(CountdownStartInstant()) + 1);
    CdExpect(IsWorldDayInStep(world) && IsDayLoopStateValid(world.DayLoop), "the start world is in step and valid");
    return world;
}

std::vector<CdTie> CdDefaultTies()
{
    return {{"friend", "npc_friend", 28000, 20000, 18000}, {"teacher", "npc_teacher", 14000, 6000, 12000},
            {"neighbour", "npc_neighbour", 9000, 4000, 9000}, {"rival", "npc_rival", 2000, 0, 4000},
            {"sibling", "npc_sibling", 15000, 12000, 8000}};
}

RelationshipBook CdBook(const std::vector<CdTie>& ties)
{
    std::vector<Relationship> pairs;
    for (const CdTie& tie : ties)
    {
        Relationship pair;
        pair.Low = std::min<std::string>(tie.Person, "player");
        pair.High = std::max<std::string>(tie.Person, "player");
        pair.Trust = tie.Trust;
        pair.Warmth = tie.Warmth;
        pair.Respect = tie.Respect;
        pairs.push_back(std::move(pair));
    }
    std::sort(pairs.begin(), pairs.end(), [](const Relationship& a, const Relationship& b) { return a.Low < b.Low; });
    RelationshipBook book;
    for (Relationship& pair : pairs)
        CdExpect(book.RestorePair(std::move(pair)), "the test relationship is added");
    return book;
}

CountdownContext CdContext(const RelationshipBook* book, const std::vector<CdTie>& ties)
{
    CountdownContext context;
    context.Relationships = book;
    context.Player = "player";
    for (const CdTie& tie : ties)
        context.Contacts.push_back({tie.Role, tie.Person});
    return context;
}

CdSession CdOpen(const CountdownContext& context, std::uint64_t seed)
{
    CountdownOpenResult opened = OpenCountdown(CdStartWorld(seed), context, CdCats());
    if (!opened.Ok())
        std::cerr << "open refused: " << CountdownErrorCode(opened.Issue.Error) << ' ' << opened.Issue.Detail << '\n';
    CdExpect(opened.Ok(), "the countdown opens");
    return {std::move(opened.World), std::move(opened.State)};
}

WeekDayPlan CdDay(const CountdownState& state, int day, const CdStyle& style, GameInstant notBefore)
{
    const GameInstant dayStart = CountdownInstantOf(day, 0);
    std::vector<std::pair<GameInstant, GameInstant>> taken;
    for (const CountdownReservedBlock& block : CountdownReservedBlocks(state))
        taken.emplace_back(block.Start, block.End);
    WeekDayPlan plan;
    // Sleep first: it is the one entry every day needs, and it ends at 06:00 the next morning.
    WeekEntry sleep;
    sleep.Kind = WeekEntryKind::Sleep;
    sleep.StartMinute = style.SleepStart;
    sleep.Minutes = 1800 - style.SleepStart;
    taken.emplace_back(dayStart + sleep.StartMinute, dayStart + sleep.StartMinute + sleep.Minutes);
    plan.Entries.push_back(sleep);
    for (const Slot& slot : Slots)
    {
        const bool wanted = slot.Priority == 0 || (slot.Priority <= 5 && slot.Priority <= style.Study) || (slot.Priority == 10 && style.Run) ||
                            (slot.Priority == 11 && style.Chores) || (slot.Priority == 12 && style.Rest);
        const GameInstant start = dayStart + slot.Start;
        if (!wanted || start < notBefore)
            continue;
        const GameInstant end = start + slot.Minutes;
        const bool free = std::none_of(taken.begin(), taken.end(), [&](const auto& span) { return start < span.second && span.first < end; });
        if (!free)
            continue;
        WeekEntry entry;
        entry.Kind = slot.Kind;
        entry.StartMinute = slot.Start;
        entry.ActivityId = slot.Id;
        entry.PlaceKind = slot.Place;
        entry.Minutes = slot.Kind == WeekEntryKind::Spend ? slot.Minutes : 0;
        entry.Pass = slot.Pass;
        plan.Entries.push_back(std::move(entry));
        taken.emplace_back(start, end);
    }
    // The sleep may not begin before `notBefore` either (a late call on the last day).
    std::stable_sort(plan.Entries.begin(), plan.Entries.end(), [](const WeekEntry& a, const WeekEntry& b) { return a.StartMinute < b.StartMinute; });
    return plan;
}

std::vector<WeekDayPlan> CdDays(const CountdownState& state, int fromDay, const CdStyle& style, GameInstant notBefore)
{
    std::vector<WeekDayPlan> days;
    for (int day = fromDay; day < CountdownDays; ++day)
        days.push_back(CdDay(state, day, style, notBefore));
    return days;
}

int CdSpanDay(const WorldState& world)
{
    return static_cast<int>(GameClock::DayNumber(world.DayLoop.Now) - GameClock::DayNumber(CountdownStartInstant()));
}

void CdFinish(CdSession& session, const CdStyle& style, int maxDaysPerCall)
{
    for (int guard = 0; !session.State.Closed && guard < 40; ++guard)
    {
        std::vector<WeekDayPlan> days = CdDays(session.State, CdSpanDay(session.World), style, session.World.DayLoop.Now);
        if (days.size() > static_cast<std::size_t>(maxDaysPerCall))
            days.resize(static_cast<std::size_t>(maxDaysPerCall));
        CountdownResolveResult resolved = ResolveCountdownDays(session.World, session.State, days, CdCats());
        if (!resolved.Ok())
            std::cerr << "resolve refused: " << CountdownErrorCode(resolved.Issue.Error) << ' ' << resolved.Issue.Detail << '\n';
        CdExpect(resolved.Ok(), "the days resolve");
        session.World = std::move(resolved.World);
        session.State = std::move(resolved.State);
    }
    CdExpect(session.State.Closed, "the countdown closes");
}

void CdTake(CdSession& session, const CountdownContext& context, const char* id)
{
    CountdownTakeResult taken = TakeOpportunity(session.World, session.State, context, id);
    if (!taken.Ok())
        std::cerr << id << " refused: " << CountdownErrorCode(taken.Issue.Error) << ' ' << taken.Issue.Detail << '\n';
    CdExpect(taken.Ok(), "the opportunity is taken");
    session.State = std::move(taken.State);
}

void TestCountdownSpanAndOpen()
{
    // The span is exactly the evening of Awakening Day to 06:00 on examination morning.
    const CalendarTime start = GameClock::ToCalendar(CountdownStartInstant());
    const CalendarTime end = GameClock::ToCalendar(CountdownEndInstant());
    CdExpect(start.Year == 2043 && start.Month == 1 && start.Day == 23 && start.Hour == 18 && start.Minute == 0 && start.DayOfWeek == Weekday::Friday,
             "the countdown starts at 18:00 on Friday 23 January 2043");
    CdExpect(end.Year == 2043 && end.Month == 2 && end.Day == 2 && end.Hour == 6 && end.Minute == 0 && end.DayOfWeek == Weekday::Monday,
             "the countdown ends at 06:00 on Monday 2 February 2043");
    CdExpect(start.Hour * 60 + start.Minute == DayLoopEveningStartMinute && end.Hour * 60 + end.Minute == DayLoopMorningStartMinute,
             "it starts when the day loop's evening begins and ends when its morning does");
    CdExpect(CountdownEndInstant() - CountdownStartInstant() == 9 * MinutesPerDay + 12 * MinutesPerHour, "the span is nine days and twelve hours");
    CdExpect(CalendarDayOfInstant(CountdownEndInstant()) - CalendarDayOfInstant(CountdownStartInstant()) == CalendarAwakeningToExamDays &&
                 CountdownDays == CalendarAwakeningToExamDays,
             "ten calendar days from Awakening Day to the examination");
    const SchoolCalendar* calendar = CdCats().Calendar;
    CdExpect(calendar->AwakeningDay == CalendarDayOfInstant(CountdownStartInstant()) && calendar->ExaminationDay == CalendarDayOfInstant(CountdownEndInstant()),
             "the real calendar agrees about both days");
    for (CalendarDay day = calendar->AwakeningDay + 1; day <= calendar->ExaminationDay; ++day)
        CdExpect(!CalendarIsSchoolDay(*calendar, day), "no day of the ten days is a school day");

    // It opens only at that minute, with a calendar and a sound context.
    const RelationshipBook book = CdBook(CdDefaultTies());
    const CountdownContext context = CdContext(&book, CdDefaultTies());
    const WorldState world = CdStartWorld();
    WorldState early = world;
    WorldSim sim(early);
    CdExpect(sim.Submit({CommandSource::System, AdvanceTime{1}}).Accepted, "a minute passes");
    CdExpect(OpenCountdown(sim.State(), context, CdCats()).Issue.Error == CountdownError::NotAtStart, "it does not open a minute late");
    CdExpect(OpenCountdown(MakeWorldState(1), context, CdCats()).Issue.Error == CountdownError::NotAtStart, "nor on the first morning of the campaign");
    DayLoopCatalogues noCalendar = CdCats();
    noCalendar.Calendar = nullptr;
    CdExpect(OpenCountdown(world, context, noCalendar).Issue.Error == CountdownError::MissingCalendar, "it needs the calendar, or the break would be marked absent");
    DayLoopCatalogues none;
    CdExpect(OpenCountdown(world, context, none).Issue.Error == CountdownError::MissingCatalogue, "it needs the activity catalogue");
    CountdownContext stranger = context;
    stranger.Player = "NOT AN ID";
    CdExpect(OpenCountdown(world, stranger, CdCats()).Issue.Error == CountdownError::BadContext, "the player's id must be valid");
    CountdownContext self = context;
    self.Contacts.push_back({"coach", "player"});
    CdExpect(OpenCountdown(world, self, CdCats()).Issue.Error == CountdownError::BadContext, "the player is not their own contact");
    WorldState asleep = world;
    asleep.DayLoop.Asleep = true;
    CdExpect(!OpenCountdown(asleep, context, CdCats()).Ok(), "it does not open while the player sleeps");

    const CountdownOpenResult opened = OpenCountdown(world, context, CdCats());
    CdExpect(opened.Ok() && opened.State.Start == CountdownStartInstant() && opened.State.End == CountdownEndInstant(), "it opens at 18:00 on Awakening Day");
    CdExpect(OpenCountdown(opened.World, context, CdCats()).Issue.Error == CountdownError::EventExists, "it cannot open twice on one world");
    CdExpect(opened.State.Obligations.size() == 3 && opened.State.Commitments.empty() && !opened.State.EventFired && !opened.State.Closed,
             "three obligations stand, nothing is committed, nothing has happened");
    CdExpect(opened.State.Seed == OpenCountdown(world, context, CdCats()).State.Seed && opened.State.Seed != 0, "the seed is stored and does not depend on anything but the world");
    CdExpect(OpenCountdown(CdStartWorld(99), context, CdCats()).State.Seed != opened.State.Seed, "another life draws another seed");

    // The countdown is the only way into the span, and it does not move the clock.
    CdExpect(opened.World.Clock.Now() == CountdownStartInstant() && IsWorldDayInStep(opened.World), "opening spends no time");
    CdExpect(ResolveCountdownDays(opened.World, opened.State, {}, CdCats()).Issue.Error == CountdownError::NoDays, "resolving no days is refused");
    CdExpect(ResolveCountdownDays(opened.World, opened.State, std::vector<WeekDayPlan>(8), CdCats()).Issue.Error == CountdownError::TooManyDays,
             "more than a week at a time is refused");
    CdExpect(ResolveCountdownDays(opened.World, opened.State, std::vector<WeekDayPlan>(1), CdCats()).Issue.Error == CountdownError::NoDays,
             "an empty day with nothing committed has nothing to resolve");
}

void TestCountdownContent()
{
    CdExpect(ValidateCountdownContent(ActivityCat).empty(), "the authored tables are sound against the real activity catalogue");
    CdExpect(!ValidateCountdownContent(ActivityCatalogue{}).empty(), "against a catalogue without the activities they are not");
    CdExpect(CountdownOpportunities().size() == 12, "the tables hold the opportunities");
    std::size_t responses = 0;
    for (const CountdownOpportunityDef& def : CountdownOpportunities())
    {
        responses += def.Group == "response" ? 1 : 0;
        CdExpect(CountdownFindOpportunity(def.Id) == &def, "an opportunity is found by its id");
        CdExpect(def.BaseOdds[0] + def.BaseOdds[1] + def.BaseOdds[2] == 1000, "the odds sum to one thousand");
    }
    CdExpect(responses == 3, "the news has three responses");
    CdExpect(CountdownFindOpportunity("opp.nothing") == nullptr && CountdownFindObligation("oblig.nothing") == nullptr, "unknown ids find nothing");
    CdExpect(CountdownBandOf(0) == CountdownBand::Unlikely && CountdownBandOf(249) == CountdownBand::Unlikely && CountdownBandOf(250) == CountdownBand::Possible &&
                 CountdownBandOf(499) == CountdownBand::Possible && CountdownBandOf(500) == CountdownBand::Likely &&
                 CountdownBandOf(799) == CountdownBand::Likely && CountdownBandOf(800) == CountdownBand::VeryLikely && CountdownBandOf(1000) == CountdownBand::VeryLikely,
             "the odds bands");
    // Several kinds of life have somewhere to put their time: every dimension is fed by at least one opportunity.
    for (std::size_t dimension = 0; dimension < CountdownDimensionCount; ++dimension)
    {
        bool fed = false;
        for (const CountdownOpportunityDef& def : CountdownOpportunities())
            fed = fed || def.Effects[0].Preparation[dimension] > 0;
        CdExpect(fed, "every preparation dimension has an opportunity that feeds it");
        CdExpect(!CountdownDimensionName(static_cast<CountdownDimension>(dimension)).empty(), "every dimension has a name");
    }
    // No outcome is a loss of preparation, and the good outcome is never worth less than the poor one.
    for (const CountdownOpportunityDef& def : CountdownOpportunities())
    {
        std::int32_t good = 0;
        std::int32_t poor = 0;
        for (std::size_t i = 0; i < CountdownDimensionCount; ++i)
        {
            good += def.Effects[0].Preparation[i];
            poor += def.Effects[2].Preparation[i];
        }
        CdExpect(good >= poor && (good + def.Effects[0].Composure + def.Effects[0].Goodwill + def.Effects[0].RouteMinutes) >=
                                    (poor + def.Effects[2].Composure + def.Effects[2].Goodwill + def.Effects[2].RouteMinutes),
                 "a good outcome is worth at least a poor one");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldCountdownTests <path to Data/Content>\n";
        return 2;
    }
    const std::vector<ContentFile> files = ReadContent(argv[1]);
    static const ContentLoadResult content = LoadContent(files);
    CdExpect(content.Ok(), "the real content validates");
    static const ActivityCatalogueResult activities = LoadActivityCatalogue(content.Db);
    CdExpect(activities.Ok(), "the activity catalogue loads");
    ActivityCat = activities.Catalogue;
    static const BellScheduleResult bells = LoadBellSchedules(content.Db);
    CdExpect(bells.Ok(), "the bell schedules load");
    BellSet = bells.Set;
    const BellSchedule* science = BellSet.FindById("bell_schedule.nabapara_xi_science");
    CdExpect(science != nullptr, "the science stream's day exists");
    static const SchoolCalendarResult calendars = LoadSchoolCalendars(content.Db);
    CdExpect(calendars.Ok(), "the school calendars load");
    CalendarSet = calendars.Set;
    const SchoolCalendar* calendar = CalendarSet.Find("institution.nabapara_vidyalaya", "stage.higher_secondary");
    CdExpect(calendar != nullptr, "the Nabapara Class XI calendar exists");
    Catalogues.Activities = &ActivityCat;
    Catalogues.School = science;
    Catalogues.Calendar = calendar;

    TestCountdownSpanAndOpen();
    TestCountdownContent();
    TestCountdownExclusion();
    TestCountdownDisclosure();
    TestCountdownObligations();
    TestCountdownEvent();
    TestCountdownReload();
    TestCountdownSave();
    TestCountdownStrategies();
    TestCountdownClose();
    std::cout << "PerfectWorld countdown: " << CdChecks << " checks passed\n";
    return 0;
}
