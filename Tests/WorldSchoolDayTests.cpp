#include "WorldReducer.h"

#include "WorldHash.h"
#include "WorldSave.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace PerfectWorld;

namespace
{
int R67Assertions = 0;

void R67Expect(bool condition, const char* message)
{
    ++R67Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::vector<ContentFile> R67ReadContent(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end());
    std::vector<ContentFile> files;
    for (const auto& path : paths)
    {
        std::ifstream input(path, std::ios::binary);
        std::ostringstream text;
        text << input.rdbuf();
        // The registry names files as /relative/paths, as pwcontent does.
        files.push_back({"/" + std::filesystem::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

void R67Clocks(const WorldSim& sim)
{
    R67Expect(sim.State().Clock.Now() == sim.State().DayLoop.Now, "world and day clocks agree");
    R67Expect(IsWorldDayInStep(sim.State()), "the world holds a valid day");
}

CommandResult R67Submit(WorldSim& sim, CommandPayload payload)
{
    CommandResult result = sim.Submit({CommandSource::Player, std::move(payload)});
    if (!result.Accepted)
        std::cerr << result.Why.Code << ' ' << result.Why.Message << '\n';
    R67Expect(result.Accepted && !result.Partial, "the command is fully accepted");
    R67Clocks(sim);
    return result;
}

void R67At(WorldSim& sim, GameInstant when)
{
    R67Expect(when >= sim.State().Clock.Now(), "test time never runs backwards");
    if (when > sim.State().Clock.Now())
        R67Submit(sim, AdvanceTime{when - sim.State().Clock.Now()});
}

void R67Refused(WorldSim& sim, CommandPayload payload, std::string_view code)
{
    const auto hash = HashWorldState(sim.State());
    const DayLoopState day = sim.State().DayLoop;
    const auto seq = sim.State().Seq;
    const auto logSize = sim.Log().size();
    const CommandResult result = sim.Submit({CommandSource::Player, std::move(payload)});
    R67Expect(!result.Accepted && result.Why.Code == code, "refusal uses the expected stable code");
    R67Expect(result.Events.empty() && !result.Partial && result.From == result.To && result.Seq == seq,
              "refusals emit nothing and spend no time or sequence");
    R67Expect(HashWorldState(sim.State()) == hash && sim.State().DayLoop == day && sim.Log().size() == logSize,
              "refusals preserve the world hash, full day and log");
    R67Clocks(sim);
}

WorldSaveData R67Save(const WorldState& state)
{
    CharacterCreator creator;
    R67Expect(creator.ApplyPreset("balanced_scholar").Succeeded(), "creation preset applies");
    R67Expect(creator.StartLife().Succeeded(), "the life starts");
    WorldSaveData data;
    R67Expect(BeginWorldFromCreation(creator.CaptureSave(), state.WorldSeed, data).Succeeded(), "world save begins");
    data.World = state;
    return data;
}

void R67ArrivalCases(const DayLoopCatalogues& cats)
{
    const BellSchedule& bell = *cats.School;
    const GameInstant start = GameClock::StartOfDay(0);
    struct R67Case { std::int32_t Minute; AttendanceOutcome Outcome; const char* Key; };
    const R67Case cases[] = {
        {bell.GatesOpenMinute, AttendanceOutcome::OnTime, "school.arrival.on_time"},
        {BellFirstBell(bell), AttendanceOutcome::OnTime, "school.arrival.on_time"},
        {BellFirstBell(bell) + 1, AttendanceOutcome::Late, "school.arrival.late"},
        {BellFirstBell(bell) + AttendanceBellGraceMinutes, AttendanceOutcome::Late, "school.arrival.late"},
        {BellFirstBell(bell) + AttendanceBellGraceMinutes + 1, AttendanceOutcome::VeryLate, "school.arrival.very_late"},
        {bell.Bells.back(), AttendanceOutcome::Absent, "school.arrival.absent"}
    };
    for (const R67Case& item : cases)
    {
        WorldSim sim(67, MaxEventsPerAdvance, cats);
        const auto plan = R67Submit(sim, SchoolPlanDay{});
        R67Expect(plan.From == plan.To && plan.Events.size() == 1, "planning is free and emits one event");
        const auto* planned = std::get_if<DayLoopEvent>(&plan.Events.front().Payload);
        R67Expect(planned != nullptr && planned->Kind == DayLoopEventKind::Planned, "plan event uses the day-loop contract");
        R67At(sim, start + item.Minute);
        const auto before = HashWorldState(sim.State());
        const SchoolArrivalView view = sim.SchoolArrivalToday();
        R67Expect(view.CanArrive && view.HasSchool && view.DayKind == SchoolDayKind::SchoolDay && !view.Recorded,
                  "the gate forecasts a new school arrival");
        R67Expect(view.GatesOpenMinute == bell.GatesOpenMinute && view.FirstBellMinute == BellFirstBell(bell) &&
                      view.DismissalMinute == bell.Bells.back(), "the window comes from bell content");
        R67Expect(view.Forecast && view.Forecast->Outcome == item.Outcome && view.ReasonTextKey == item.Key,
                  "the forecast and text key agree with attendance rules");
        R67Expect(HashWorldState(sim.State()) == before, "querying changes nothing");
        const auto arrived = R67Submit(sim, SchoolArrive{});
        R67Expect(arrived.From == arrived.To && arrived.Events.size() == 1, "arrival is free and emits one event");
        const auto* event = std::get_if<DayLoopEvent>(&arrived.Events.front().Payload);
        R67Expect(event && event->Kind == DayLoopEventKind::ArrivedAtSchool && event->Attendance == item.Outcome &&
                      arrived.Events.front().Seq == sim.State().Seq && arrived.Events.front().At == sim.State().Clock.Now(),
                  "arrival event has outcome, world sequence and instant");
        const auto after = sim.SchoolArrivalToday();
        const auto recordedMinute = item.Outcome == AttendanceOutcome::Absent ? MinutesPerDay : item.Minute;
        R67Expect(after.Recorded && after.Recorded->Outcome == item.Outcome && after.ReasonTextKey == item.Key &&
                      after.Recorded->ArrivalMinute == recordedMinute, "the query projects the existing attendance entry");
        R67Expect(sim.State().DayLoop.Attendance.SchoolDays == 1 && sim.State().DayLoop.Attendance.Days.size() == 1,
                  "one arrival creates one attendance day");
        R67Expect(sim.State().DayLoop.Receipts.Entries.empty(), "arrival does not create an activity receipt");
        R67Refused(sim, SchoolArrive{}, "school.already_recorded");

        WorldSaveData data = R67Save(sim.State());
        const std::string bytes = SerializeWorldSave(data);
        WorldSaveData loaded;
        const auto load = DeserializeWorldSave(bytes, loaded);
        R67Expect(!bytes.empty() && load.Succeeded() && load.HashVerified && load.SourceVersion == WorldSaveSchemaVersion,
                  "existing schema reloads and authenticates an arrival");
        R67Expect(loaded.World.DayLoop == sim.State().DayLoop && HashWorldState(loaded.World) == HashWorldState(sim.State()),
                  "save reload preserves attendance, phase, plan and hash");
        WorldSim resumed(loaded.World, MaxEventsPerAdvance, cats);
        R67Refused(resumed, SchoolArrive{}, "school.already_recorded");
        R67Expect(resumed.SchoolArrivalToday().Recorded == after.Recorded, "gate result survives restart");
    }
}

void R67CalendarCases(const DayLoopCatalogues& cats)
{
    for (const CalendarHoliday& holiday : cats.Calendar->Holidays)
    {
        WorldSim sim(67, MaxEventsPerAdvance, cats);
        R67Submit(sim, SchoolPlanDay{});
        R67At(sim, CalendarDayStartInstant(holiday.First) + cats.School->GatesOpenMinute);
        const auto view = sim.SchoolArrivalToday();
        R67Expect(!view.CanArrive && view.DayKind != SchoolDayKind::SchoolDay && view.ClosureId == holiday.Id,
                  "authored holidays do not admit attendance");
        R67Refused(sim, SchoolArrive{}, "school.not_school_day");
        R67Expect(sim.State().DayLoop.Attendance.SchoolDays == 0, "refused holidays do not close earlier absences");
    }
    WorldSim weekend(67, MaxEventsPerAdvance, cats);
    R67Submit(weekend, SchoolPlanDay{});
    R67At(weekend, GameClock::StartOfDay(0) + 5 * MinutesPerDay + cats.School->GatesOpenMinute);
    R67Expect(weekend.SchoolArrivalToday().DayKind == SchoolDayKind::NonSchoolWeekday, "Saturday has no school");
    R67Refused(weekend, SchoolArrive{}, "school.not_school_day");

    WorldSim skipped(67, MaxEventsPerAdvance, cats);
    R67Submit(skipped, SchoolPlanDay{});
    const CalendarDay target = *CalendarNextSchoolDay(*cats.Calendar, 7);
    R67At(skipped, CalendarDayStartInstant(target) + cats.School->GatesOpenMinute);
    R67Submit(skipped, SchoolArrive{});
    R67Expect(skipped.State().DayLoop.Attendance.AbsentDays == CalendarCountSchoolDays(*cats.Calendar, 0, target - 1),
              "arrival catches up only unrecorded calendar school days as absences");
    R67Expect(skipped.State().DayLoop.Attendance.OnTimeDays == 1, "catch-up records today's on-time arrival once");

    WorldSim outside(67, MaxEventsPerAdvance, cats);
    R67Submit(outside, SchoolPlanDay{});
    R67At(outside, CalendarDayStartInstant(cats.Calendar->LastDay + 1) + cats.School->GatesOpenMinute);
    R67Expect(outside.SchoolArrivalToday().DayKind == SchoolDayKind::OutsideYear, "no weekday fallback beyond the calendar");
    R67Refused(outside, SchoolArrive{}, "school.not_school_day");
}

void R67AtomicCases(const DayLoopCatalogues& cats)
{
    WorldSim sim(67, MaxEventsPerAdvance, cats);
    R67At(sim, GameClock::StartOfDay(0) + cats.School->GatesOpenMinute);
    R67Refused(sim, SchoolArrive{}, "dayloop.plan_first");
    R67Refused(sim, SchoolPlanDay{{{600, "activity.unknown_r67"}}}, "dayloop.unknown_activity");
    R67Submit(sim, SchoolPlanDay{});
    WorldState asleep = sim.State();
    asleep.DayLoop.Asleep = true;
    asleep.DayLoop.SleptAt = asleep.Clock.Now();
    asleep.DayLoop.Phase = DayLoopPhaseOf(asleep.DayLoop);
    WorldSim sleeping(asleep, MaxEventsPerAdvance, cats);
    R67Refused(sleeping, SchoolArrive{}, "dayloop.asleep");

    R67Submit(sim, ConversationOpen{"person.farida_qadri"});
    R67Refused(sim, SchoolArrive{}, "school.conversation_open");
    R67Refused(sim, SchoolPlanDay{}, "school.conversation_open");
    R67Submit(sim, ConversationLeave{"person.farida_qadri"});
    const auto scheduled = sim.Submit({CommandSource::System, ScheduleWorldEvent{sim.State().Clock.Now(), "school/test/r67"}});
    R67Expect(scheduled.Accepted, "a due-now event is scheduled");
    R67Refused(sim, SchoolArrive{}, "world.unsettled");
    R67Refused(sim, SchoolPlanDay{}, "world.unsettled");
    R67Submit(sim, SettleDueEvents{});
    R67Submit(sim, SchoolArrive{});

    WorldSim early(67, MaxEventsPerAdvance, cats);
    R67Submit(early, SchoolPlanDay{});
    R67At(early, GameClock::StartOfDay(0) + cats.School->GatesOpenMinute - 1);
    R67Refused(early, SchoolArrive{}, "school.gate_closed");
    WorldSim missing(67);
    R67Refused(missing, SchoolArrive{}, "school.missing_content");
    R67Refused(missing, SchoolPlanDay{}, "dayloop.missing_catalogue");
    SchoolCalendar wrong = *cats.Calendar;
    wrong.InstitutionId = "institution.other";
    DayLoopCatalogues mismatch = cats;
    mismatch.Calendar = &wrong;
    WorldSim wrongSchool(67, MaxEventsPerAdvance, mismatch);
    R67Refused(wrongSchool, SchoolArrive{}, "school.invalid_content");

    WorldState exhausted = early.State();
    exhausted.DayLoop.Seq = ~std::uint64_t{0};
    WorldSim limit(exhausted, MaxEventsPerAdvance, cats);
    R67At(limit, GameClock::StartOfDay(0) + cats.School->GatesOpenMinute);
    R67Refused(limit, SchoolArrive{}, "dayloop.invalid_state");
    R67Refused(limit, SchoolPlanDay{}, "dayloop.invalid_state");
}

void R67ReplayAndObligations(const DayLoopCatalogues& cats)
{
    WorldSim sim(67, MaxEventsPerAdvance, cats);
    WorldSim replay(67, MaxEventsPerAdvance, cats);
    const std::vector<CommandPayload> script = {
        SchoolPlanDay{{{cats.School->Blocks[1].StartMinute, std::string(AttendanceClassActivityId)}}},
        AdvanceTime{GameClock::StartOfDay(0) + BellFirstBell(*cats.School)}, SchoolArrive{},
        AdvanceTime{MinutesPerDay}, SchoolArrive{}
    };
    for (const auto& payload : script)
    {
        // AdvanceTime is a duration from the epoch (06:00) for the first day, then a full day to Tuesday.
        const auto result = R67Submit(sim, payload);
        const auto repeated = R67Submit(replay, sim.Log().back().Payload);
        R67Expect(HashWorldState(sim.State()) == HashWorldState(replay.State()) && result.Events.size() == repeated.Events.size(),
                  "every accepted logged command replays deterministically with the same catalogues");
    }
    R67Expect(sim.State().DayLoop.Plan.size() == 1 && sim.State().DayLoop.Plan.front().ActivityId == AttendanceClassActivityId,
              "arrival preserves the player's plan");
    R67Expect(sim.State().DayLoop.Attendance.SchoolDays == 2 && sim.State().DayLoop.Attendance.LateDays == 0,
              "duplicate refusal is per day, not permanent");
    ReplayOptions options;
    options.School = cats;
    options.KeepResults = true;
    const ReplayResult replayed = ReplayCommandLog(67, sim.Log(), options);
    R67Expect(replayed.Complete && replayed.Applied == sim.Log().size() && replayed.Hash == HashWorldState(sim.State()),
              "the shared replay helper reconstructs school commands with their content binding");

    WorldState committed = sim.State();
    DayCommand commit;
    commit.Kind = DayCommandKind::Commit;
    commit.ActivityId = std::string(AttendanceClassActivityId);
    commit.PlaceKind = "classroom";
    R67Expect(ApplyDayCommand(committed.DayLoop, cats, commit).Ok(), "an existing commitment can be prepared");
    WorldSim pending(committed, MaxEventsPerAdvance, cats);
    R67At(pending, pending.State().Clock.Now() + MinutesPerDay);
    const auto obligation = pending.State().DayLoop.Commitment;
    R67Submit(pending, SchoolArrive{});
    R67Expect(pending.State().DayLoop.HasCommitment && pending.State().DayLoop.Commitment == obligation,
              "arrival preserves an unstarted obligation");

    DayCommand begin;
    begin.Kind = DayCommandKind::Start;
    WorldState running = committed;
    R67Expect(ApplyDayCommand(running.DayLoop, cats, begin).Ok(), "an activity can be started for the busy refusal");
    // Use Thursday's fresh day so the busy refusal is not hidden by duplicate attendance.
    running.Clock = GameClock{};
    R67Expect(!running.Clock.AdvanceTo(GameClock::StartOfDay(0) + 3 * MinutesPerDay + cats.School->GatesOpenMinute).Rejected,
              "the test world clock advances");
    running.DayLoop.Now = running.Clock.Now();
    running.DayLoop.Runner.StartedAt = running.Clock.Now();
    running.DayLoop.Phase = DayLoopPhaseOf(running.DayLoop);
    WorldSim busy(running, MaxEventsPerAdvance, cats);
    R67Refused(busy, SchoolArrive{}, "dayloop.busy");
}
} // namespace

int main(int argc, char** argv)
{
    R67Expect(argc == 2, "pass Data/Content");
    const auto content = LoadContent(R67ReadContent(argv[1]));
    R67Expect(content.Ok(), "authoritative content validates");
    const auto bells = LoadBellSchedules(content.Db);
    const auto calendars = LoadSchoolCalendars(content.Db);
    const auto activities = LoadActivityCatalogue(content.Db);
    R67Expect(bells.Ok() && calendars.Ok() && activities.Ok(), "school catalogues load");
    const BellSchedule* school = bells.Set.FindById("bell_schedule.nabapara_xi_science");
    R67Expect(school != nullptr, "the authoritative XI science timetable exists");
    const SchoolCalendar* calendar = calendars.Set.Find(school->InstitutionId, school->StageId);
    R67Expect(calendar != nullptr, "the matching authoritative calendar exists");
    const DayLoopCatalogues cats{&activities.Catalogue, school, calendar};
    R67ArrivalCases(cats);
    R67CalendarCases(cats);
    R67AtomicCases(cats);
    R67ReplayAndObligations(cats);
    std::cout << "School arrival: " << R67Assertions << " assertions passed\n";
}
