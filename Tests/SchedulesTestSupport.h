// Shared fixtures for the M2-06 schedule tests (SchedulesTests.cpp owns main and Check).
#pragma once

#include "IdentityRegistry.h"
#include "Schedules.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace PerfectWorld;

extern int Checks;
void Check(bool condition, const char* message);

struct Rng
{
    std::uint64_t S;
    std::uint64_t Next()
    {
        S ^= S >> 12;
        S ^= S << 25;
        S ^= S >> 27;
        return (S * 2685821657736338717ULL) >> 16;
    }
    std::int64_t Pick(std::int64_t low, std::int64_t high)
    {
        return low + static_cast<std::int64_t>(Next() % static_cast<std::uint64_t>(high - low + 1));
    }
};

const char* const Student = "p_0123456789abcdef";
const char* const Home = "loc_home";
const char* const School = "loc_nabapara_vidyalaya";
const char* const Field = "loc_para_field";
const char* const Library = "loc_library";
const char* const Class = "activity.attend_class";
const char* const Run = "activity.exercise_run";
const char* const Study = "activity.study_at_home";
constexpr auto Role = ScheduleDoingKind::Role;
constexpr auto Act = ScheduleDoingKind::Activity;
constexpr std::uint8_t Weekdays = 0x3E; // Monday..Friday
constexpr std::uint8_t Weekend = 0x41;  // Saturday and Sunday

// Day 0 is the Monday of the campaign start; instant 0 is 06:00 that day.
inline GameInstant At(std::int64_t day, int hour, int minute) { return day * MinutesPerDay + hour * 60 + minute - 6 * 60; }
inline int Hm(int hour, int minute) { return hour * 60 + minute; }

inline ScheduleTravelTable Travel()
{
    ScheduleTravelTable table;
    table.Legs = {{Home, Library, 20}, {Home, School, 30}, {Home, Field, 10}, {Library, School, 25}, {Library, Field, 20}, {School, Field, 15}};
    std::sort(table.Legs.begin(), table.Legs.end(), [](const ScheduleTravelLeg& a, const ScheduleTravelLeg& b) {
        return a.From != b.From ? a.From < b.From : a.To < b.To;
    });
    return table;
}

inline ScheduleEntry Entry(std::uint8_t mask, int start, int end, const char* zone, ScheduleDoingKind kind, const char* doing, int priority)
{
    ScheduleEntry e;
    e.WeekdayMask = mask;
    e.StartMinute = start;
    e.EndMinute = end;
    e.Zone = zone;
    e.Kind = kind;
    e.Doing = doing;
    e.Priority = priority;
    return e;
}

inline ScheduleException Exception(const char* id, GameInstant from, GameInstant to, const char* zone, const char* doing, int priority,
                            ScheduleDoingKind kind = Role)
{
    ScheduleException e;
    e.Id = id;
    e.From = from;
    e.To = to;
    e.Zone = zone;
    e.Doing = doing;
    e.Kind = kind;
    e.Priority = priority;
    return e;
}

// A Class XI student's week. Entry indices are the "routine:<n>" source IDs the tests check.
inline PersonSchedule Fixture()
{
    PersonSchedule p;
    p.PersonId = Student;
    p.HomeZone = Home;
    p.Routine = {
        Entry(ScheduleAllWeekdays, Hm(0, 0), Hm(6, 0), Home, Role, "sleeping", 50),   // 0
        Entry(ScheduleAllWeekdays, Hm(22, 0), Hm(24, 0), Home, Role, "sleeping", 50), // 1
        Entry(Weekdays, Hm(6, 0), Hm(7, 30), Home, Role, "morning", 10),              // 2
        Entry(Weekdays, Hm(8, 0), Hm(15, 0), School, Act, Class, 20),                 // 3
        Entry(Weekdays, Hm(15, 30), Hm(17, 0), Field, Act, Run, 20),                  // 4
        Entry(Weekdays, Hm(18, 0), Hm(20, 0), Home, Act, Study, 20),                  // 5
        Entry(Weekdays, Hm(20, 0), Hm(22, 0), Home, Role, "evening", 10),             // 6
        Entry(Weekend, Hm(6, 0), Hm(9, 30), Home, Role, "morning", 10),               // 7
        Entry(Weekend, Hm(10, 0), Hm(12, 0), Library, Act, Study, 20),                // 8
        Entry(Weekend, Hm(12, 30), Hm(22, 0), Home, Role, "leisure", 10),             // 9
    };
    return p;
}

inline ScheduleException Assembly() { return Exception("ex_assembly", At(1, 10, 0), At(1, 11, 0), Field, "assembly", 30); }

// Exceptions that exercise every tie-break and the multi-day case.
inline PersonSchedule BusyFixture()
{
    PersonSchedule p = Fixture();
    p.Exceptions = {Assembly(),
                    Exception("ex_a", At(2, 10, 0), At(2, 12, 0), Library, "alpha", 60),
                    Exception("ex_b", At(2, 11, 0), At(2, 13, 0), Field, "beta", 60),
                    Exception("ex_watch", At(2, 22, 30), At(2, 23, 30), Home, "watching", 50),
                    Exception("ex_sick", At(3, 6, 0), At(5, 6, 0), Home, "sick", 40)};
    return p;
}

inline ScheduleResolution Where(const PersonSchedule& p, GameInstant t) { return ResolveSchedule(p, Travel(), t); }

inline bool SameId(const ScheduleResolution& a, const ScheduleResolution& b)
{
    return a.Status == b.Status && a.Zone == b.Zone && a.FromZone == b.FromZone && a.Kind == b.Kind && a.Doing == b.Doing &&
           a.SourceId == b.SourceId && a.Squeezed == b.Squeezed;
}

inline void ExpectStay(const PersonSchedule& p, GameInstant t, const char* zone, const char* doing, const char* message)
{
    const ScheduleResolution r = Where(p, t);
    Check(r.Valid && r.Status == ScheduleStatus::Stay && r.Zone == zone && r.Doing == doing, message);
}

inline void ExpectIdle(const PersonSchedule& p, GameInstant t, const char* zone, const char* message)
{
    const ScheduleResolution r = Where(p, t);
    Check(r.Valid && r.Status == ScheduleStatus::Idle && r.Zone == zone && r.Doing.empty(), message);
}

inline void ExpectTravel(const PersonSchedule& p, GameInstant t, const char* from, const char* to, std::int32_t progress, const char* message)
{
    const ScheduleResolution r = Where(p, t);
    Check(r.Valid && r.Status == ScheduleStatus::Travel && r.FromZone == from && r.Zone == to && r.ProgressPermille == progress, message);
}

inline ScheduleEvent BeganFrom(GameInstant at, const ScheduleResolution& r)
{
    ScheduleEvent e;
    e.At = at;
    e.Kind = r.Status == ScheduleStatus::Stay     ? ScheduleEventKind::StayBegan
             : r.Status == ScheduleStatus::Travel ? ScheduleEventKind::TravelBegan
                                                  : ScheduleEventKind::IdleBegan;
    e.Zone = r.Zone;
    e.FromZone = r.FromZone;
    e.DoingKind = r.Kind;
    e.Doing = r.Doing;
    e.SourceId = r.SourceId;
    e.Squeezed = r.Squeezed;
    return e;
}

// The independent reference: resolve every single minute and note each change of identity.
inline std::vector<ScheduleEvent> FineStep(const PersonSchedule& p, const ScheduleTravelTable& travel, GameInstant from, GameInstant to,
                                    SchedulePersonState& state)
{
    std::vector<ScheduleEvent> events;
    ScheduleResolution previous = ResolveSchedule(p, travel, from);
    for (GameInstant m = from + 1; m <= to; ++m)
    {
        const ScheduleResolution current = ResolveSchedule(p, travel, m);
        if (!SameId(previous, current))
        {
            if (previous.Status == ScheduleStatus::Stay && previous.Kind == Act)
            {
                ScheduleEvent ended = BeganFrom(m, previous);
                ended.Kind = ScheduleEventKind::ActivityEnded;
                ended.Minutes = m - previous.SegmentStart;
                ended.NominalMinutes = previous.NominalMinutes;
                ended.Counted = ended.Minutes * 1000 >= ended.NominalMinutes * ScheduleSessionMinPartsPerThousand;
                state.Tally[previous.Doing].Minutes += ended.Minutes;
                if (ended.Counted)
                    ++state.Tally[previous.Doing].Sessions;
                events.push_back(ended);
            }
            events.push_back(BeganFrom(m, current));
        }
        previous = current;
    }
    state.At = to;
    return events;
}

inline ScheduleAdvanceResult Step(const PersonSchedule& p, SchedulePersonState& state, GameInstant to)
{
    return AdvanceSchedulePerson(p, Travel(), state, to);
}

inline void Append(std::vector<ScheduleEvent>& all, const ScheduleAdvanceResult& r)
{
    all.insert(all.end(), r.Events.begin(), r.Events.end());
}
