// Order F3: the quest log projection (Today, Promises, Discoveries), derived from authoritative state with no new persisted
// state. Usage: PerfectWorldQuestLogTests <path to Data/Content>
#include "QuestLog.h"

#include "Attendance.h"
#include "ContentRegistry.h"
#include "Doors.h"
#include "LaneJournal.h"
#include "SchoolCalendar.h"
#include "WorldHash.h"
#include "WorldReducer.h"
#include "WorldState.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace
{
int QwChecks = 0;

void QwExpect(const bool condition, const char* message)
{
    ++QwChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

GameInstant QwAt(const int day, const int hour, const int minute)
{
    return GameClock::FromCalendar(2042, 6, day, hour, minute); // 2 June 2042 is the Monday of day 0
}

void QwSubmit(WorldSim& sim, CommandPayload payload)
{
    const CommandResult result = sim.Submit({CommandSource::System, std::move(payload)});
    if (!result.Accepted) std::cerr << "refused: " << result.Why.Code << " " << result.Why.Message << '\n';
    QwExpect(result.Accepted, "the world command is accepted");
}

void QwTalk(WorldSim& sim, const std::string& person, const std::vector<std::string>& choices)
{
    QwSubmit(sim, ConversationOpen{person, PersonAvailability::Awake});
    for (const std::string& choice : choices)
        QwSubmit(sim, ConversationChoose{person, choice, sim.State().Conversations.at(person).SessionCount});
    QwSubmit(sim, ConversationLeave{person});
}

void QwAdvanceTo(WorldSim& sim, const GameInstant when)
{
    QwSubmit(sim, AdvanceTime{when - sim.State().Clock.Now()});
}

const QuestItem* QwFind(const QuestLogView& log, const std::string_view id)
{
    return QuestLogFind(log, id);
}

std::vector<ContentFile> QwReadContent(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        if (entry.is_regular_file() && entry.path().extension() == ".json") paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end());
    std::vector<ContentFile> files;
    for (const auto& path : paths)
    {
        std::ifstream stream(path, std::ios::binary);
        std::ostringstream text;
        text << stream.rdbuf();
        files.push_back({path.generic_string(), text.str()});
    }
    return files;
}

BellScheduleSet QwBells;
SchoolCalendarSet QwCalendars;

void QwLoadContent(const std::filesystem::path& root)
{
    const ContentLoadResult loaded = LoadContent(QwReadContent(root));
    QwExpect(loaded.Ok(), "Data/Content validates");
    const BellScheduleResult bells = LoadBellSchedules(loaded.Db);
    QwExpect(bells.Ok() && bells.Set.Schedules.size() == 3, "the three bell schedules load");
    QwBells = bells.Set;
    const SchoolCalendarResult calendars = LoadSchoolCalendars(loaded.Db);
    QwExpect(calendars.Ok() && !calendars.Set.Calendars.empty(), "the school calendar loads");
    QwCalendars = calendars.Set;
}

void QwShippedBellsMatchTheDefaults()
{
    for (const BellSchedule& schedule : QwBells.Schedules)
    {
        QwExpect(schedule.GatesOpenMinute == QuestDefaultGatesOpenMinute, "the default gate time is the content's");
        QwExpect(BellFirstBell(schedule) == QuestDefaultFirstBellMinute, "the default first bell is the content's");
        QwExpect(!schedule.Bells.empty() && schedule.Bells.back() == QuestDefaultDismissalMinute, "the default dismissal is the content's");
    }
}

void QwFreshWorld()
{
    const WorldState world = MakeWorldState(11);
    const QuestLogView log = MakeQuestLog(world, QwAt(2, 7, 30));
    QwExpect(log.Day == GameClock::DayNumber(QwAt(2, 7, 30)) && log.Now == QwAt(2, 7, 30), "the log carries its instant and day");
    QwExpect(log.Promises.empty(), "a new life has no promises");
    QwExpect(log.Today.size() == 1 && log.Today[0].Id == QuestIdSchoolBell, "a Monday morning has one obligation: school");
    const QuestItem& school = log.Today[0];
    QwExpect(school.Section == QuestSection::Today && school.Status == QuestStatus::Open, "school is open before the gates");
    QwExpect(school.TextKey == "quest.today.school_bell" && school.PlaceKey == "place.school", "school keeps its text key and place");
    QwExpect(school.DueAt == QwAt(2, 10, 0), "school is due at the first bell, 10:00");
    QwExpect(school.PersonId.empty() && !school.Tracked, "school has no person and starts untracked");
    QwExpect(log.Discoveries.size() == 9, "nine places on the lane have nobody spoken to yet");
    QwExpect(log.Count() == 10 && !log.Empty(), "the count adds the sections");
    QwExpect(log.Discoveries.front().Id == "quest.discover.caretaker_flat" && log.Discoveries.back().Id == "quest.discover.stationery",
             "discoveries keep their table order");
    for (const QuestItem& item : log.Discoveries)
    {
        QwExpect(item.Section == QuestSection::Discoveries && item.TextKey == item.Id && item.PersonId.empty() && item.WhoLabel.empty(),
                 "a discovery names its place, not a person");
        QwExpect(item.PlaceKey.starts_with("location.nabapara.") && !item.Windows.empty() && item.WindowKind != QuestWindowKind::None,
                 "a discovery has its authored hours");
    }
    QwExpect(MakeQuestLog(world, QwAt(2, 7, 30)) == log, "the same world and instant give the same log");
    QwExpect(MakeQuestLog(world, -5).Now == 0, "a negative instant reads as zero");
    std::set<std::string> ids;
    for (const auto* section : {&log.Today, &log.Promises, &log.Discoveries})
        for (const QuestItem& item : *section) ids.insert(item.Id);
    QwExpect(ids.size() == log.Count(), "ids are unique");
}

void QwDerivedOnly()
{
    WorldSim sim(12);
    QwAdvanceTo(sim, QwAt(2, 12, 0));
    QwTalk(sim, std::string(HouseholdAdultId), {"choice.household_adult.talk_rota", "choice.household_adult.propose_dishes"});
    const std::uint64_t before = HashWorldState(sim.State());
    const std::uint64_t seq = sim.State().Seq;
    const GameInstant clock = sim.State().Clock.Now();
    (void)MakeQuestLog(sim.State(), sim.State().Clock.Now());
    QwExpect(HashWorldState(sim.State()) == before && sim.State().Seq == seq && sim.State().Clock.Now() == clock, "projecting a quest log changes nothing in the world");
}

void QwSchoolStatus()
{
    const BellSchedule* science = QwBells.Find("institution.nabapara_vidyalaya", "science");
    QwExpect(science != nullptr, "the science schedule exists");
    const QuestLogContext context{science, nullptr};
    WorldState world = MakeWorldState(13);

    // The status of the school row at an instant, or nothing when there is no such row. Never keep a pointer into a temporary log.
    const auto statusAt = [&](const GameInstant at) -> std::optional<QuestStatus>
    {
        const QuestLogView projected = MakeQuestLog(world, at, context);
        const QuestItem* item = QwFind(projected, QuestIdSchoolBell);
        return item != nullptr ? std::optional<QuestStatus>(item->Status) : std::nullopt;
    };
    QwExpect(statusAt(QwAt(2, 9, 29)) == QuestStatus::Open, "open until the gates open");
    QwExpect(statusAt(QwAt(2, 9, 30)) == QuestStatus::Due, "due when the gates open");
    QwExpect(statusAt(QwAt(2, 10, 20)) == QuestStatus::Due, "still due, late, until dismissal");
    QwExpect(statusAt(QwAt(2, 15, 49)) == QuestStatus::Due, "due until the last bell");
    QwExpect(statusAt(QwAt(2, 15, 50)) == QuestStatus::Missed, "missed once school is over without an arrival");
    QwExpect(QwFind(MakeQuestLog(world, QwAt(2, 9, 0), context), QuestIdSchoolBell)->DueAt == QwAt(2, 10, 0), "the schedule's first bell is the due time");

    // Monday arrival at 09:55 is recorded: done, even before dismissal.
    QwExpect(RecordAttendanceArrival(world.DayLoop.Attendance, *science, QwAt(2, 9, 55)).Ok, "an on-time arrival records");
    QwExpect(statusAt(QwAt(2, 11, 0)) == QuestStatus::Done && statusAt(QwAt(2, 17, 0)) == QuestStatus::Done,
             "an attended day is done all day");
    QwExpect(statusAt(QwAt(3, 9, 40)) == QuestStatus::Due, "the next day is a new obligation");

    // A recorded absence is missed, even before the day is over.
    QwExpect(RecordAttendanceAbsence(world.DayLoop.Attendance, *science, QwAt(3, 9, 0)).Ok, "an absence records");
    QwExpect(statusAt(QwAt(3, 9, 40)) == QuestStatus::Missed, "a recorded absence is missed");

    // A late arrival still counts as attending.
    QwExpect(RecordAttendanceArrival(world.DayLoop.Attendance, *science, QwAt(4, 10, 20)).Ok, "a late arrival records");
    QwExpect(statusAt(QwAt(4, 12, 0)) == QuestStatus::Done, "a late arrival still attended");

    // Weekend: no school obligation at all.
    QwExpect(!statusAt(QwAt(7, 8, 0)).has_value() && !statusAt(QwAt(8, 8, 0)).has_value(), "no school on Saturday or Sunday");
    QwExpect(MakeQuestLog(world, QwAt(7, 8, 0)).Today.empty(), "a bare weekend has an empty Today");
}

void QwSchoolCalendar()
{
    const SchoolCalendar* calendar = QwCalendars.Find("institution.nabapara_vidyalaya", "stage.higher_secondary");
    QwExpect(calendar != nullptr, "the Class XI calendar exists");
    const QuestLogContext context{nullptr, calendar};
    const WorldState world = MakeWorldState(14);
    std::int64_t holiday = -1;
    std::int64_t schoolDay = -1;
    for (std::int64_t day = calendar->FirstDay; day <= calendar->LastDay; ++day)
    {
        if (AttendanceIsSchoolDay(nullptr, day) && !CalendarIsSchoolDay(*calendar, day) && holiday < 0) holiday = day;
        if (CalendarIsSchoolDay(*calendar, day) && schoolDay < 0) schoolDay = day;
    }
    QwExpect(holiday >= 0 && schoolDay >= 0, "the calendar has a weekday off and a school day");
    const auto at = [](const std::int64_t day) { return GameClock::StartOfDay(0) + day * MinutesPerDay + 8 * 60; };
    QwExpect(QwFind(MakeQuestLog(world, at(holiday), context), QuestIdSchoolBell) == nullptr, "a weekday holiday has no school obligation");
    QwExpect(QwFind(MakeQuestLog(world, at(holiday)), QuestIdSchoolBell) != nullptr, "without a calendar the Monday to Friday rule holds");
    QwExpect(QwFind(MakeQuestLog(world, at(schoolDay), context), QuestIdSchoolBell) != nullptr, "a calendar school day has one");
    const QuestLogView onSchoolDay = MakeQuestLog(world, at(schoolDay), context);
    const QuestItem* withCalendar = QwFind(onSchoolDay, QuestIdSchoolBell);
    QwExpect(withCalendar != nullptr && !withCalendar->WeekdaysOnly, "with a calendar the item is not described as weekdays only");
}

void QwPromisesAndObligations()
{
    WorldSim sim(15);
    const std::string parent(HouseholdAdultId);
    QwAdvanceTo(sim, QwAt(2, 12, 0));
    QwTalk(sim, parent, {"choice.household_adult.talk_rota", "choice.household_adult.propose_dishes"});

    QuestLogView log = MakeQuestLog(sim.State(), QwAt(2, 12, 0));
    QwExpect(log.Promises.size() == 1, "one remembered agreement is one promise");
    const QuestItem& promise = log.Promises[0];
    QwExpect(promise.Id == "quest.promise.evening_rota_dishes_weekdays", "the promise id is the journal's agreement id without 'fact.'");
    QwExpect(promise.TextKey == "journal.agreement.evening_rota_dishes_weekdays", "the promise keeps the journal's ShortTextKey");
    QwExpect(promise.PersonId == parent && promise.WhoLabel == HouseholdAdultLabel && promise.PlaceKey == "place.home", "who and where");
    QwExpect(promise.WeekdaysOnly && promise.Status == QuestStatus::Open && promise.Windows.empty(), "a household promise is open and weekday only");

    const LaneJournalView journal = LaneJournalToday(sim.State(), log.Day);
    QwExpect(journal.AgreementsRemembered.size() == 1 && journal.AgreementsRemembered[0].ShortTextKey == promise.TextKey,
             "the promise is the journal's agreement");

    const QuestItem* dishes = QwFind(log, "quest.today.evening_dishes");
    QwExpect(dishes != nullptr && dishes->Section == QuestSection::Today, "the agreed weekday dishes are also an obligation today");
    QwExpect(dishes->DueAt == QwAt(2, 18, 0) && dishes->Status == QuestStatus::Open, "due at 18:00 and open at noon");
    QwExpect(dishes->PersonId == parent && dishes->PlaceKey == "place.home", "the obligation names the parent and home");
    QwExpect(QwFind(MakeQuestLog(sim.State(), QwAt(2, 18, 0)), "quest.today.evening_dishes")->Status == QuestStatus::Due, "due when evening opens");
    QwExpect(QwFind(MakeQuestLog(sim.State(), QwAt(2, 23, 59)), "quest.today.evening_dishes")->Status == QuestStatus::Due,
             "never done or missed: no per-day evidence exists");
    QwExpect(QwFind(MakeQuestLog(sim.State(), QwAt(7, 18, 0)), "quest.today.evening_dishes") == nullptr, "weekday dishes are not an obligation on Saturday");
    QwExpect(QwFind(MakeQuestLog(sim.State(), QwAt(7, 18, 0)), promise.Id) != nullptr, "but the promise remains");
    QwExpect(log.Today.front().Id == QuestIdSchoolBell && log.Today[1].Id == "quest.today.evening_dishes", "Today is ordered by due time");

    // A replacement suppresses the old promise at once (the journal rule) and the new arrangement is every day.
    QwTalk(sim, parent, {"choice.household_adult.revisit_rota", "choice.household_adult.propose_alternate"});
    log = MakeQuestLog(sim.State(), QwAt(7, 18, 0));
    QwExpect(log.Promises.size() == 1 && log.Promises[0].TextKey == "journal.agreement.evening_rota_alternate_days", "the replacement is the one promise");
    QwExpect(QwFind(log, "quest.today.evening_alternate") != nullptr && QwFind(log, "quest.today.evening_dishes") == nullptr,
             "the new arrangement runs on Saturday and the old one is gone");

    // Two proprietors' promises carry their place and opening hours from the door table.
    QwAdvanceTo(sim, QwAt(3, 9, 0));
    QwTalk(sim, "person.mahua_saha", {"choice.mahua_saha.promise_sort"});
    QwTalk(sim, "person.debashis_pal", {"choice.debashis_pal.promise_bring"});
    log = MakeQuestLog(sim.State(), QwAt(3, 9, 30));
    const QuestItem* mahua = QwFind(log, "quest.promise.mahua_saha.sort_promised");
    const QuestItem* debashis = QwFind(log, "quest.promise.debashis_pal.bring_promised");
    QwExpect(mahua != nullptr && debashis != nullptr, "both shop promises are listed");
    QwExpect(mahua->PlaceKey == "location.nabapara.hardware_store" && mahua->WindowKind == QuestWindowKind::Opening && !mahua->Windows.empty(),
             "the hardware store's opening hours");
    QwExpect(debashis->PlaceKey == "location.nabapara.repair_workshop" && debashis->WindowKind == QuestWindowKind::Opening,
             "the workshop's opening hours");
    QwExpect(mahua->Status == QuestStatus::Due, "a promise is due while its place is open (09:30)");
    QwExpect(QwFind(MakeQuestLog(sim.State(), QwAt(3, 3, 0)), mahua->Id)->Status == QuestStatus::Open, "and open when it is shut");
    QwExpect(log.Promises.size() == 3 && log.Promises[0].PersonId == "person.debashis_pal" && log.Promises[1].PersonId == parent,
             "promises keep the journal's person order");
    QwExpect(QwFind(log, "quest.discover.repair_workshop") == nullptr && QwFind(log, "quest.discover.hardware_store") == nullptr,
             "places you have spoken at are no longer discoveries");
    QwExpect(QwFind(log, "quest.discover.tea_row_house") != nullptr, "an unmet place remains");
}

void QwCareJobs()
{
    // The care home's arrangements reach Today too. Opening the conversation at the care worker's morning hour (07:00-11:00).
    WorldSim sim(16);
    QwAdvanceTo(sim, QwAt(2, 7, 30));
    QwTalk(sim, "person.care_worker", {"choice.care_worker.talk_jobs", "choice.care_worker.propose_breakfast"});
    const QuestLogView monday = MakeQuestLog(sim.State(), QwAt(2, 7, 30));
    const QuestItem* breakfast = QwFind(monday, "quest.today.care_breakfast");
    QwExpect(breakfast != nullptr && breakfast->PlaceKey == "place.care_home" && breakfast->Status == QuestStatus::Due &&
             breakfast->DueAt == QwAt(2, 7, 0), "the care home's breakfast is due from 07:00 on a weekday");
    QwExpect(QwFind(monday, "quest.promise.care_jobs_breakfast_weekdays") != nullptr, "and it is a promise");
    QwExpect(QwFind(MakeQuestLog(sim.State(), QwAt(7, 7, 30)), "quest.today.care_breakfast") == nullptr, "not at the weekend");
}

void QwPlanAndCommitment()
{
    WorldState world = MakeWorldState(17);
    world.DayLoop.Planned = true;
    world.DayLoop.Plan = {{540, "activity.study_at_home"}, {900, "activity.study_at_home"}, {1080, "activity.eat_meal"}};
    const GameInstant dayStart = GameClock::StartOfDay(QwAt(2, 0, 0));

    QuestLogView log = MakeQuestLog(world, QwAt(2, 8, 0));
    QwExpect(QwFind(log, "quest.today.plan.study_at_home.1") != nullptr && QwFind(log, "quest.today.plan.study_at_home.2") != nullptr &&
             QwFind(log, "quest.today.plan.eat_meal.1") != nullptr, "each plan item has its own stable id");
    const QuestItem* first = QwFind(log, "quest.today.plan.study_at_home.1");
    QwExpect(first->TextKey == "quest.today.plan.study_at_home" && first->ActivityId == "activity.study_at_home" && first->DueAt == dayStart + 540,
             "a plan item keeps the activity text key, id and start");
    QwExpect(first->Status == QuestStatus::Open && QwFind(MakeQuestLog(world, QwAt(2, 9, 0)), first->Id)->Status == QuestStatus::Due,
             "a plan item is due from its start");
    QwExpect(QwFind(MakeQuestLog(world, QwAt(2, 23, 0)), "quest.today.plan.eat_meal.1")->Status == QuestStatus::Due,
             "an uncompleted plan item is never missed");

    ReceiptEntry done;
    done.Seq = 1;
    done.ActivityId = "activity.study_at_home";
    done.Start = dayStart + 545;
    done.DurationMinutes = 60;
    world.DayLoop.Receipts.Entries.push_back(done);
    log = MakeQuestLog(world, QwAt(2, 12, 0));
    QwExpect(QwFind(log, "quest.today.plan.study_at_home.1")->Status == QuestStatus::Done, "a completed activity ticks the first matching plan item");
    QwExpect(QwFind(log, "quest.today.plan.study_at_home.2")->Status == QuestStatus::Open, "and only that one");
    QwExpect(QwFind(MakeQuestLog(world, QwAt(3, 12, 0)), "quest.today.plan.study_at_home.1")->Status != QuestStatus::Done,
             "yesterday's completion does not tick today's plan");

    world.DayLoop.HasCommitment = true;
    world.DayLoop.Commitment.ActivityId = "activity.household_chores";
    world.DayLoop.Commitment.PlaceKind = "home";
    log = MakeQuestLog(world, QwAt(2, 12, 0));
    QwExpect(log.Today.front().Id == QuestIdCommitment && log.Today.front().Status == QuestStatus::Due, "the running commitment comes first");
    QwExpect(log.Today.front().ActivityId == "activity.household_chores" && log.Today.front().PlaceKey == "place.home", "with its activity and place");

    world.DayLoop.Planned = false;
    QwExpect(QwFind(MakeQuestLog(world, QwAt(2, 12, 0)), "quest.today.plan.eat_meal.1") == nullptr, "an unplanned day shows no plan");
}

void QwDiscoveryHours()
{
    const WorldState world = MakeWorldState(18);
    const QuestLogView noon = MakeQuestLog(world, QwAt(2, 12, 0));
    const QuestItem* row = QwFind(noon, "quest.discover.tea_row_house");
    QwExpect(row != nullptr && row->WindowKind == QuestWindowKind::Visiting && row->Windows.size() == 1, "the row house has visiting hours");
    const DoorDef* door = FindDoor("door.nabapara.tea_row_house_front");
    QwExpect(door != nullptr && row->Windows[0] == QuestWindow{door->VisitingWindows[0].OpenMinute, door->VisitingWindows[0].CloseMinute},
             "read straight from the authored door");
    QwExpect(row->Status == QuestStatus::Open, "open at noon, before visiting hours");
    QwExpect(QwFind(MakeQuestLog(world, QwAt(2, 17, 0)), row->Id)->Status == QuestStatus::Due, "due inside visiting hours");
    QwExpect(QwFind(MakeQuestLog(world, QwAt(2, 19, 30)), row->Id)->Status == QuestStatus::Open, "open again after them (half open window)");
    const QuestItem* shop = QwFind(noon, "quest.discover.hardware_store");
    QwExpect(shop != nullptr && shop->WindowKind == QuestWindowKind::Opening && shop->Windows.size() == 2, "a shop has its opening hours, split in two");

    // Speaking at the stationery shop to either person takes it off the list.
    WorldSim sim(19);
    QwAdvanceTo(sim, QwAt(2, 16, 0));
    QwTalk(sim, "person.farida_qadri", {"choice.farida_qadri.ask_bench"});
    QwExpect(QwFind(MakeQuestLog(sim.State(), sim.State().Clock.Now()), "quest.discover.stationery") == nullptr, "a met shop is no discovery");
    QwExpect(MakeQuestLog(sim.State(), sim.State().Clock.Now()).Discoveries.size() == 8, "the others remain");
}

void QwTracking()
{
    WorldState world = MakeWorldState(20);
    QuestLogView log = MakeQuestLog(world, QwAt(2, 8, 0));
    QwExpect(QuestLogTracked(log) == nullptr, "nothing is tracked in a fresh projection");
    QwExpect(QuestLogTrack(log, QuestIdSchoolBell) && QuestLogTracked(log) != nullptr && QuestLogTracked(log)->Id == QuestIdSchoolBell, "track one");
    QwExpect(QuestLogTrack(log, "quest.discover.tea_row_house") && QuestLogTracked(log)->Id == "quest.discover.tea_row_house", "tracking another moves it");
    QwExpect(!QwFind(log, QuestIdSchoolBell)->Tracked, "only one item is tracked");
    QwExpect(!QuestLogTrack(log, "quest.nothing") && QuestLogTracked(log) == nullptr, "an unknown id tracks nothing and clears the flag");
    QwExpect(QuestLogTrack(log, QuestIdSchoolBell) && !QuestLogTrack(log, "") && QuestLogTracked(log) == nullptr, "an empty id clears");
    QwExpect(QuestLogFind(log, "") == nullptr && QuestLogFind(log, "quest.nothing") == nullptr, "find is exact");
    const QuestLogView& readOnly = log;
    QwExpect(QuestLogFind(readOnly, QuestIdSchoolBell) != nullptr, "find works on a const log");
    QwExpect(MakeQuestLog(world, QwAt(2, 8, 0)) == MakeQuestLog(world, QwAt(2, 8, 0)), "tracking is not part of the world");
    QwExpect(QuestSectionName(QuestSection::Promises) == "promises" && QuestStatusName(QuestStatus::Missed) == "missed", "stable names");
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldQuestLogTests <path to Data/Content>\n";
        return 2;
    }
    QwLoadContent(argv[1]);
    QwShippedBellsMatchTheDefaults();
    QwFreshWorld();
    QwDerivedOnly();
    QwSchoolStatus();
    QwSchoolCalendar();
    QwPromisesAndObligations();
    QwCareJobs();
    QwPlanAndCommitment();
    QwDiscoveryHours();
    QwTracking();
    std::cout << "QuestLogTests passed (" << QwChecks << " checks)\n";
    return 0;
}
