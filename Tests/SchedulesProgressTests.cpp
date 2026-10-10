// ROADMAP M2-06: off-screen progress, text forms and 10,000 random schedules.
#include "SchedulesTestSupport.h"

void TestAdvance()
{
    const PersonSchedule p = Fixture();
    const PersonSchedule busy = BusyFixture();
    const ScheduleTravelTable travel = Travel();

    // 16 hours in one jump equals stepping boundary by boundary, equals stepping every minute.
    for (const PersonSchedule* person : {&p, &busy})
    {
        SchedulePersonState jump = StartSchedulePerson(Student, 0);
        const ScheduleAdvanceResult jumped = Step(*person, jump, 960);
        Check(jumped.Ok && !jumped.Events.empty(), "a 16 hour advance succeeds");
        SchedulePersonState stepped = StartSchedulePerson(Student, 0);
        std::vector<ScheduleEvent> events;
        for (GameInstant at = 0; at < 960;)
        {
            const std::optional<GameInstant> next = NextScheduleBoundary(*person, travel, at);
            const GameInstant to = std::min<GameInstant>(next ? *next : 960, 960);
            Append(events, Step(*person, stepped, to));
            at = to;
        }
        SchedulePersonState fine = StartSchedulePerson(Student, 0);
        const std::vector<ScheduleEvent> fineEvents = FineStep(*person, travel, 0, 960, fine);
        Check(events == jumped.Events && fineEvents == jumped.Events, "boundary stepping and fine stepping give the jump's events");
        Check(stepped == jump && fine == jump && HashSchedulePersonState(stepped) == HashSchedulePersonState(jump), "and the same state and hash");
    }
    // Events are ordered, inside (from, to], and the activity end comes before the next start.
    SchedulePersonState day = StartSchedulePerson(Student, 0);
    const ScheduleAdvanceResult one = Step(p, day, 1440);
    for (std::size_t i = 0; i < one.Events.size(); ++i)
    {
        Check(one.Events[i].At > 0 && one.Events[i].At <= 1440 && (i == 0 || one.Events[i - 1].At <= one.Events[i].At), "events are ordered inside the window");
        if (one.Events[i].Kind == ScheduleEventKind::ActivityEnded)
            Check(i + 1 < one.Events.size() && one.Events[i + 1].At == one.Events[i].At && one.Events[i + 1].Kind != ScheduleEventKind::ActivityEnded, "an activity end is followed by what begins");
    }
    Check(one.Events.front().At == 90 && one.Events.front().Kind == ScheduleEventKind::TravelBegan, "first boundary: 07:30 departure");
    Check(NextScheduleBoundary(p, travel, 0) == std::optional<GameInstant>(90) && NextScheduleBoundary(p, travel, 90) == std::optional<GameInstant>(120) &&
              NextScheduleBoundary(p, travel, 119) == std::optional<GameInstant>(120),
          "next boundary");

    // A whole week: the unseen student still gets the scheduled practice, and the assembly shortens one class.
    PersonSchedule assemblyEdit = p;
    assemblyEdit.Exceptions = {Assembly()};
    const PersonSchedule withAssembly = assemblyEdit;
    for (const PersonSchedule* person : {&p, &withAssembly})
    {
        SchedulePersonState week = StartSchedulePerson(Student, 0);
        Check(Step(*person, week, At(7, 6, 0)).Ok, "a week advances");
        const bool assembled = !person->Exceptions.empty();
        Check(week.Tally.at(Class) == ScheduleActivityTally{5, assembled ? 2010 : 2100}, "class sessions and minutes");
        Check(week.Tally.at(Run) == ScheduleActivityTally{5, 450} && week.Tally.at(Study) == ScheduleActivityTally{7, 840}, "practice and study tally");
        Check(week.Tally.size() == 3 && week.At == At(7, 6, 0), "nothing else is tallied");
    }
    // Starting inside a stay does not change what the stay is worth.
    SchedulePersonState mid = StartSchedulePerson(Student, 300);
    const ScheduleAdvanceResult midResult = Step(p, mid, 700);
    bool sawEnd = false;
    for (const ScheduleEvent& e : midResult.Events)
    {
        if (e.Kind == ScheduleEventKind::ActivityEnded && e.Doing == Class)
            sawEnd = e.At == 540 && e.Minutes == 420 && e.NominalMinutes == 420 && e.Counted;
    }
    Check(sawEnd && mid.Tally.at(Class).Minutes == 420, "a stay entered before the advance is credited in full");

    // The busy fixture against the minute-by-minute reference over a week and a half, tally included.
    SchedulePersonState jumped = StartSchedulePerson(Student, 50);
    SchedulePersonState fine = StartSchedulePerson(Student, 50);
    const ScheduleAdvanceResult big = Step(busy, jumped, At(9, 0, 0));
    Check(big.Ok && FineStep(busy, travel, 50, At(9, 0, 0), fine) == big.Events && fine == jumped, "busy week equals the minute-by-minute reference");
    // Additivity at awkward points: mid-travel, on a boundary, across midnight and on the last minute.
    for (const GameInstant mid2 : {GameInstant(60), GameInstant(90), GameInstant(100), GameInstant(120), At(1, 10, 0), At(2, 23, 59), At(3, 0, 0), GameInstant(At(9, 0, 0))})
    {
        SchedulePersonState a = StartSchedulePerson(Student, 50);
        std::vector<ScheduleEvent> events;
        Append(events, Step(busy, a, mid2));
        Append(events, Step(busy, a, At(9, 0, 0)));
        Check(events == big.Events && a == jumped, "T1->T2->T3 equals T1->T3");
    }
    // Determinism and refusals.
    SchedulePersonState again = StartSchedulePerson(Student, 50);
    Check(Step(busy, again, At(9, 0, 0)).Events == big.Events && SerialiseSchedulePersonState(again) == SerialiseSchedulePersonState(jumped), "repeat runs are byte-identical");
    SchedulePersonState s = StartSchedulePerson(Student, 100);
    Check(!Step(p, s, 99).Ok && s == StartSchedulePerson(Student, 100), "time cannot run backwards");
    Check(Step(p, s, 100).Ok && Step(p, s, 100).Events.empty(), "a zero advance is fine");
    SchedulePersonState other = StartSchedulePerson("p_fedcba9876543210", 0);
    Check(!Step(p, other, 10).Ok && other.At == 0, "a state for another person is refused");
    Check(!Step(p, s, 100 + MaxSingleAdvanceMinutes + 1).Ok && !Step(p, s, ScheduleMaxInstant + 1).Ok, "absurd advances are refused");
    Check(Step(p, s, 100 + MaxSingleAdvanceMinutes).Ok, "a ten year advance is allowed");
}

void TestText()
{
    const PersonSchedule p = BusyFixture();
    const ScheduleTravelTable travel = Travel();
    SchedulePersonState state = StartSchedulePerson(Student, 0);
    Check(Step(p, state, At(8, 0, 0)).Ok, "state for the text tests");
    const std::string schedule = SerialisePersonSchedule(p);
    const std::string table = SerialiseScheduleTravel(travel);
    const std::string stateText = SerialiseSchedulePersonState(state);
    Check(schedule.rfind("pw.schedule 1\nperson p_0123456789abcdef\nhome loc_home\nroutine 10\nentry 127 0 360 50 role loc_home sleeping\n", 0) == 0, "schedule layout");
    Check(stateText.rfind("pw.schedule_state 1\nperson p_0123456789abcdef\nat " + std::to_string(At(8, 0, 0)) + "\ntally " +
                              std::to_string(state.Tally.size()) + "\nactivity activity.attend_class ", 0) == 0,
          "state layout");
    Check(ParsePersonSchedule(schedule) == p && ParseScheduleTravel(table) == travel && ParseSchedulePersonState(stateText) == state, "round trips");
    Check(SerialisePersonSchedule(*ParsePersonSchedule(schedule)) == schedule, "serialising is canonical");
    Check(HashPersonSchedule(p) != HashPersonSchedule(Fixture()) && HashScheduleTravel(travel) != 0, "hashes see the exceptions");
    for (std::size_t length = 0; length < schedule.size(); ++length)
        Check(!ParsePersonSchedule(std::string_view(schedule).substr(0, length)).has_value(), "every truncated schedule is refused");
    for (std::size_t length = 0; length < table.size(); ++length)
        Check(!ParseScheduleTravel(std::string_view(table).substr(0, length)).has_value(), "every truncated travel table is refused");
    for (std::size_t length = 0; length < stateText.size(); ++length)
        Check(!ParseSchedulePersonState(std::string_view(stateText).substr(0, length)).has_value(), "every truncated state is refused");
    // Any single changed byte is refused (the hash, the counts and the validation all guard the content).
    for (std::size_t i = 0; i < schedule.size(); ++i)
    {
        std::string mutated = schedule;
        mutated[i] = schedule[i] == 'x' ? 'y' : 'x';
        Check(!ParsePersonSchedule(mutated).has_value(), "a changed byte in a schedule is refused");
    }
    for (std::size_t i = 0; i < stateText.size(); ++i)
    {
        std::string mutated = stateText;
        mutated[i] = stateText[i] == 'x' ? 'y' : 'x';
        Check(!ParseSchedulePersonState(mutated).has_value(), "a changed byte in a state is refused");
    }
    for (std::size_t i = 0; i < table.size(); ++i)
    {
        std::string mutated = table;
        mutated[i] = table[i] == 'x' ? 'y' : 'x';
        Check(!ParseScheduleTravel(mutated).has_value(), "a changed byte in a travel table is refused");
    }
    Check(!ParsePersonSchedule(schedule + "\n").has_value() && !ParsePersonSchedule(schedule + "end 1\n").has_value(), "trailing text is refused");
    std::string crlf = schedule;
    crlf.insert(crlf.find('\n'), "\r");
    Check(!ParsePersonSchedule(crlf).has_value() && !ParsePersonSchedule("").has_value(), "carriage returns and empty text are refused");
    // Data that serialises but is invalid does not parse.
    PersonSchedule overlap = Fixture();
    overlap.Routine.push_back(overlap.Routine[3]);
    Check(!ParsePersonSchedule(SerialisePersonSchedule(overlap)).has_value(), "an equal-priority overlap does not parse");
    ScheduleTravelTable unsorted;
    unsorted.Legs = {{"loc_b", "loc_a", 5}, {"loc_a", "loc_b", 5}};
    Check(!ParseScheduleTravel(SerialiseScheduleTravel(unsorted)).has_value(), "an unsorted table does not parse");
    SchedulePersonState badState = state;
    badState.At = ScheduleMaxInstant + 1;
    Check(!ParseSchedulePersonState(SerialiseSchedulePersonState(badState)).has_value(), "an out-of-range state does not parse");
}

namespace
{
PersonSchedule RandomSchedule(Rng& rng, ScheduleTravelTable& travel)
{
    const char* const zones[4] = {"zone_a", "zone_b", "zone_c", "zone_d"};
    travel.Legs.clear();
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
            travel.Legs.push_back({zones[i], zones[j], static_cast<std::int32_t>(rng.Pick(1, 90))});
    PersonSchedule p;
    p.PersonId = Student;
    p.HomeZone = zones[rng.Pick(0, 3)];
    const std::int64_t entries = rng.Pick(0, 8);
    for (std::int64_t i = 0; i < entries; ++i)
    {
        const int start = static_cast<int>(15 * rng.Pick(0, 94));
        const int end = static_cast<int>(std::min<std::int64_t>(1440, start + 15 * rng.Pick(1, 40)));
        p.Routine.push_back(Entry(static_cast<std::uint8_t>(rng.Pick(1, 127)), start, end, zones[rng.Pick(0, 3)], rng.Pick(0, 1) ? Act : Role,
                                  rng.Pick(0, 1) ? "act_x" : "act_y", static_cast<int>(i)));
    }
    const std::int64_t exceptions = rng.Pick(0, 4);
    for (std::int64_t i = 0; i < exceptions; ++i)
    {
        const GameInstant from = 15 * rng.Pick(0, 960);
        p.Exceptions.push_back(Exception(("ex_" + std::to_string(i)).c_str(), from, from + 15 * rng.Pick(1, 130), zones[rng.Pick(0, 3)], "ex_do",
                                         static_cast<int>(rng.Pick(0, 9)), rng.Pick(0, 1) ? Act : Role));
    }
    return p;
}

} // namespace

void TestRandomSchedules()
{
    std::int64_t eventCount = 0, squeezedCount = 0, endedCount = 0, countedCount = 0, tripCount = 0;
    for (std::uint64_t iteration = 0; iteration < 10000; ++iteration)
    {
        Rng rng{0x5C4ED0000ULL + iteration * 977};
        ScheduleTravelTable travel;
        const PersonSchedule p = RandomSchedule(rng, travel);
        Check(ValidatePersonSchedule(p).empty() && ValidateScheduleTravel(travel).empty() && ValidateScheduleTravelCoverage(p, travel).empty(), "random data validates");
        if (iteration % 4 == 0)
            Check(ParsePersonSchedule(SerialisePersonSchedule(p)) == p && ParseScheduleTravel(SerialiseScheduleTravel(travel)) == travel, "random data round trips");
        for (int sample = 0; sample < 3; ++sample)
        {
            const GameInstant t = rng.Pick(0, 12 * 1440);
            const ScheduleResolution r = ResolveSchedule(p, travel, t);
            Check(r.Valid && !r.Zone.empty() && r.SegmentStart <= t && t < r.SegmentEnd && r.ProgressPermille >= 0 && r.ProgressPermille <= 1000, "random resolution is sane");
            Check(r.Status != ScheduleStatus::Travel || (!r.FromZone.empty() && r.FromZone != r.Zone), "a trip goes between two zones");
            Check(r.Status != ScheduleStatus::Idle || r.Doing.empty(), "idle does nothing");
            Check(SameId(r, ResolveSchedule(p, travel, t)), "resolution is deterministic");
        }
        const GameInstant t1 = rng.Pick(0, 4 * 1440);
        const GameInstant t2 = t1 + rng.Pick(0, 2 * 1440);
        const GameInstant t3 = t2 + rng.Pick(0, 2 * 1440);
        SchedulePersonState whole = StartSchedulePerson(Student, t1);
        SchedulePersonState parts = StartSchedulePerson(Student, t1);
        const ScheduleAdvanceResult jump = AdvanceSchedulePerson(p, travel, whole, t3);
        std::vector<ScheduleEvent> events;
        Append(events, AdvanceSchedulePerson(p, travel, parts, t2));
        Append(events, AdvanceSchedulePerson(p, travel, parts, t3));
        Check(jump.Ok && events == jump.Events && whole == parts && HashSchedulePersonState(whole) == HashSchedulePersonState(parts), "random advances are additive");
        for (std::size_t i = 0; i < jump.Events.size(); ++i)
        {
            Check(jump.Events[i].At > t1 && jump.Events[i].At <= t3 && (i == 0 || jump.Events[i - 1].At <= jump.Events[i].At), "random events are ordered inside the window");
            if (i % 5 == 0 && jump.Events[i].Kind != ScheduleEventKind::ActivityEnded)
            {
                const ScheduleResolution after = ResolveSchedule(p, travel, jump.Events[i].At);
                Check(BeganFrom(jump.Events[i].At, after) == jump.Events[i] && !SameId(after, ResolveSchedule(p, travel, jump.Events[i].At - 1)), "an event is exactly a change of resolved identity");
            }
        }
        for (const ScheduleEvent& e : jump.Events)
        {
            ++eventCount;
            squeezedCount += e.Squeezed ? 1 : 0;
            endedCount += e.Kind == ScheduleEventKind::ActivityEnded ? 1 : 0;
            countedCount += e.Counted ? 1 : 0;
            tripCount += e.Kind == ScheduleEventKind::TravelBegan ? 1 : 0;
        }
        if (iteration % 200 == 0)
        {
            SchedulePersonState fine = StartSchedulePerson(Student, t1);
            const GameInstant to = t1 + 1440;
            SchedulePersonState fast = StartSchedulePerson(Student, t1);
            const ScheduleAdvanceResult fastResult = AdvanceSchedulePerson(p, travel, fast, to);
            Check(FineStep(p, travel, t1, to, fine) == fastResult.Events && fine == fast, "random day equals the minute-by-minute reference");
        }
    }
    // The random population really exercises trips, squeezes and finished activities.
    Check(eventCount > 100000 && tripCount > 10000 && squeezedCount > 1000 && endedCount > 10000 && countedCount > 5000, "random schedules cover trips, squeezes and activity ends");
}
