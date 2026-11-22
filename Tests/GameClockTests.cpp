#include "GameClock.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>

using namespace PerfectWorld;

int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main()
{
    // The campaign opens on the first Monday of Class XI, at 06:00. If the
    // constant is ever changed to a date that is not a Monday, this fails.
    GameClock clock;
    Check(clock.Now() == 0, "a new clock starts at the epoch");
    const CalendarTime start = clock.Calendar();
    Check(start.Year == CampaignStartYear && start.Month == CampaignStartMonth &&
              start.Day == CampaignStartDay,
          "the epoch is the campaign start date");
    Check(start.Hour == CampaignStartHour && start.Minute == 0, "the campaign opens at 06:00");
    Check(start.DayOfWeek == Weekday::Monday, "the campaign opens on a Monday");
    Check(WeekdayName(start.DayOfWeek) == "Monday", "weekday names match");

    // Calendar conversion must round trip exactly, including across leap days
    // and month boundaries. A calendar bug would corrupt every schedule.
    Check(IsValidCivilDate(2044, 2, 29), "2044 is a leap year");
    Check(!IsValidCivilDate(2043, 2, 29), "2043 is not");
    Check(!IsValidCivilDate(2100, 2, 29), "2100 is a century non-leap year");
    Check(IsValidCivilDate(2400, 2, 29), "2400 is a leap year");
    Check(!IsValidCivilDate(2042, 13, 1) && !IsValidCivilDate(2042, 0, 1), "month range");
    Check(!IsValidCivilDate(2042, 4, 31) && !IsValidCivilDate(2042, 6, 31), "short months");

    for (std::int32_t year = 2040; year <= 2100; ++year)
    {
        for (std::int32_t month = 1; month <= 12; ++month)
        {
            for (const std::int32_t day : {1, 15, 28})
            {
                const GameInstant when = GameClock::FromCalendar(year, month, day, 13, 37);
                const CalendarTime back = GameClock::ToCalendar(when);
                Check(back.Year == year && back.Month == month && back.Day == day,
                      "calendar date round trips");
                Check(back.Hour == 13 && back.Minute == 37, "calendar time round trips");
            }
        }
    }
    // Leap day itself, and the day either side of it.
    const GameInstant leapDay = GameClock::FromCalendar(2044, 2, 29, 0, 0);
    Check(GameClock::ToCalendar(leapDay).Day == 29, "leap day round trips");
    Check(GameClock::ToCalendar(leapDay - 1).Day == 28, "the minute before leap day is the 28th");
    Check(GameClock::ToCalendar(leapDay + MinutesPerDay).Month == 3, "the day after leap day is March");

    // Weekdays must advance by exactly one per day and never repeat out of order.
    Weekday previous = GameClock::ToCalendar(0).DayOfWeek;
    for (std::int64_t day = 1; day < 400; ++day)
    {
        const Weekday current = GameClock::ToCalendar(day * MinutesPerDay).DayOfWeek;
        const int expected = (static_cast<int>(previous) + 1) % 7;
        Check(static_cast<int>(current) == expected, "weekday advances by one each day");
        previous = current;
    }

    // Instants before the epoch still land on the correct day. Floor division,
    // not truncation toward zero.
    Check(GameClock::ToCalendar(-1).Hour == 5 && GameClock::ToCalendar(-1).Minute == 59,
          "the minute before the epoch is 05:59");
    Check(GameClock::ToCalendar(-MinutesPerDay).Day == CampaignStartDay - 1,
          "a day before the epoch is the previous day");
    Check(GameClock::ToCalendar(-MinutesPerDay).DayOfWeek == Weekday::Sunday,
          "the day before the opening Monday is a Sunday");

    // StartOfDay and DayNumber agree with the calendar on both sides of the epoch.
    Check(GameClock::StartOfDay(0) == -CampaignStartHour * MinutesPerHour,
          "midnight precedes the 06:00 epoch by six hours");
    Check(GameClock::ToCalendar(GameClock::StartOfDay(0)).Hour == 0, "start of day is midnight");
    Check(GameClock::DayNumber(GameClock::StartOfDay(0)) == GameClock::DayNumber(0),
          "midnight is the same day as the epoch");
    // The epoch sits at 06:00, so one minute earlier is 05:59 on the same day.
    // Crossing into the previous day takes the full six hours and one minute.
    Check(GameClock::DayNumber(-1) == GameClock::DayNumber(0), "05:59 is still the opening day");
    Check(GameClock::DayNumber(-CampaignStartHour * MinutesPerHour - 1) == GameClock::DayNumber(0) - 1,
          "one minute before midnight is the previous day");

    // Time never runs backwards and never moves absurdly far in one call.
    Check(clock.Advance(-1).Rejected, "a negative advance is rejected");
    Check(clock.Now() == 0, "a rejected advance does not move the clock");
    Check(clock.Advance(MaxSingleAdvanceMinutes + 1).Rejected, "an absurd advance is rejected");
    Check(clock.Now() == 0, "a rejected absurd advance does not move the clock");
    Check(!clock.Advance(0).Rejected && clock.Now() == 0, "advancing zero is allowed and does nothing");
    Check(clock.AdvanceTo(-1).Rejected, "advancing to an earlier instant is rejected");

    // Ordinary advance, and the day-crossing count used for daily rollover.
    {
        GameClock day;
        const AdvanceResult toSchool = day.Advance(2 * MinutesPerHour);
        Check(!toSchool.Rejected && day.Calendar().Hour == 8, "two hours reaches 08:00");
        Check(toSchool.DaysCrossed == 0, "no midnight crossed before bedtime");
        const AdvanceResult overnight = day.Advance(20 * MinutesPerHour);
        Check(overnight.DaysCrossed == 1, "sleeping past midnight crosses exactly one day");
        Check(day.Calendar().DayOfWeek == Weekday::Tuesday, "the next school day is Tuesday");
        const AdvanceResult week = day.Advance(7 * MinutesPerDay);
        Check(week.DaysCrossed == 7, "a week crosses seven days");
        Check(day.Calendar().DayOfWeek == Weekday::Tuesday, "a week later is the same weekday");
    }

    // Scheduling: events fire in chronological order, exactly once, and an
    // event landing exactly on the target instant fires.
    {
        GameClock sched;
        const GameInstant inOneHour = sched.Now() + MinutesPerHour;
        const std::int64_t bell = sched.Schedule(inOneHour, "school_bell");
        const std::int64_t bus = sched.Schedule(sched.Now() + 30, "bus_departs");
        const std::int64_t later = sched.Schedule(sched.Now() + MinutesPerDay, "tomorrow");
        Check(bell != 0 && bus != 0 && later != 0, "scheduling returns handles");
        Check(sched.Schedule(sched.Now() - 1, "the past") == 0, "scheduling into the past is refused");
        Check(sched.Pending().size() == 3, "three events are pending");
        Check(sched.NextEventInstant() == sched.Now() + 30, "the soonest event is first");

        const AdvanceResult hour = sched.Advance(MinutesPerHour);
        Check(hour.Fired.size() == 2, "both events within the hour fired");
        Check(hour.Fired[0].Id == "bus_departs" && hour.Fired[1].Id == "school_bell",
              "events fire in chronological order");
        Check(sched.Pending().size() == 1, "the later event is still pending");

        const AdvanceResult again = sched.Advance(MinutesPerHour);
        Check(again.Fired.empty(), "an event never fires twice");
        Check(sched.Cancel(later) && sched.Pending().empty(), "an event can be cancelled");
        Check(!sched.Cancel(later), "cancelling twice reports failure");
        Check(sched.NextEventInstant() == -1, "no pending events reports no next instant");
        Check(sched.Advance(MinutesPerDay * 30).Fired.empty(), "a cancelled event never fires");
    }

    // Two events at the same instant fire in the order they were scheduled.
    {
        GameClock ties;
        const GameInstant when = ties.Now() + 10;
        for (int i = 0; i < 5; ++i)
        {
            Check(ties.Schedule(when, "tie_" + std::to_string(i)) != 0, "scheduled a tied event");
        }
        const AdvanceResult fired = ties.Advance(10);
        Check(fired.Fired.size() == 5, "all tied events fired together");
        for (int i = 0; i < 5; ++i)
        {
            Check(fired.Fired[static_cast<std::size_t>(i)].Id == "tie_" + std::to_string(i),
                  "tied events fire in the order scheduled");
        }
    }

    // Save round trip, including pending events and handle continuity.
    {
        GameClock before;
        Check(!before.Advance(3 * MinutesPerDay + 17).Rejected, "advanced before saving");
        const std::int64_t kept = before.Schedule(before.Now() + 500, "exam");
        Check(kept != 0, "scheduled an event to persist");
        const GameClockSave save = before.CaptureSave();

        GameClock after;
        Check(after.RestoreSave(save), "a valid clock record restores");
        Check(after.Now() == before.Now(), "the instant round trips");
        Check(after.Pending().size() == 1 && after.Pending()[0].Id == "exam",
              "pending events round trip");
        const std::int64_t fresh = after.Schedule(after.Now() + 10, "later");
        Check(fresh != kept && fresh != 0, "handles do not collide after restore");

        // A record whose pending event is already in the past is inconsistent.
        GameClockSave broken = save;
        broken.Pending[0].When = broken.Now - 1;
        GameClock target;
        Check(!target.RestoreSave(broken), "an event before the clock is refused");
        Check(target.Now() == 0, "a refused restore leaves the clock untouched");

        // So is a handle that a future Schedule would reissue.
        broken = save;
        broken.NextHandle = 1;
        Check(!target.RestoreSave(broken), "a handle at or above the next handle is refused");

        broken = save;
        broken.NextHandle = 0;
        Check(!target.RestoreSave(broken), "a next handle below one is refused");

        // And so are duplicate handles.
        broken = save;
        broken.Pending.push_back(broken.Pending[0]);
        Check(!target.RestoreSave(broken), "duplicate handles are refused");
    }

    // Determinism: the same sequence of advances and schedules produces the
    // same firing order every time. Reloading a save must not reroll history.
    {
        const auto Run = [](std::uint32_t seed)
        {
            std::mt19937 rng(seed);
            GameClock clock2;
            std::vector<std::string> order;
            for (int step = 0; step < 500; ++step)
            {
                if (rng() % 3 == 0)
                {
                    const std::int64_t when = clock2.Now() + static_cast<std::int64_t>(rng() % 5000);
                    (void)clock2.Schedule(when, "e" + std::to_string(step));
                }
                const AdvanceResult advanced = clock2.Advance(static_cast<std::int64_t>(rng() % 900));
                for (const ScheduledEvent& fired : advanced.Fired)
                {
                    order.push_back(fired.Id);
                }
            }
            return order;
        };
        Check(Run(20260920) == Run(20260920), "the same seed produces the same firing order");
        Check(!Run(20260920).empty(), "the determinism run actually fired events");
    }

    // An event never fires before its instant, and the clock is monotonic
    // across a long random walk.
    {
        std::mt19937 rng(7);
        GameClock walk;
        GameInstant previousInstant = walk.Now();
        std::set<std::int64_t> seen;
        for (int step = 0; step < 4000; ++step)
        {
            const GameInstant when = walk.Now() + static_cast<std::int64_t>(rng() % 2000);
            const std::int64_t handle = walk.Schedule(when, "w");
            const AdvanceResult advanced = walk.Advance(static_cast<std::int64_t>(rng() % 700));
            Check(walk.Now() >= previousInstant, "the clock never moves backwards");
            previousInstant = walk.Now();
            for (const ScheduledEvent& fired : advanced.Fired)
            {
                Check(fired.When <= walk.Now(), "an event never fires before its instant");
                Check(seen.insert(fired.Handle).second, "an event never fires twice");
            }
            (void)handle;
        }
    }

    // Due-now draining (LANE_LIFE_IMPLEMENTATION section A item 1): an event scheduled for the current instant fires
    // at that instant, without a minute passing.
    {
        GameClock due;
        Check(due.Advance(100).Fired.empty(), "nothing to fire on the way to 100");
        Check(!due.HasDueEvents(), "nothing is due on a clock with nothing pending");
        const std::int64_t now = due.Schedule(100, "test/now/a");
        const std::int64_t later = due.Schedule(101, "test/later/b");
        Check(now > 0 && later > 0 && due.HasDueEvents(), "an event for the current instant is due");
        const AdvanceResult drained = due.DrainDue(10);
        Check(!drained.Rejected && !drained.Capped && drained.From == 100 && drained.To == 100 && due.Now() == 100,
              "draining does not move time");
        Check(drained.Fired.size() == 1 && drained.Fired[0].Handle == now && drained.Fired[0].When == 100, "only the due event fires");
        Check(!due.HasDueEvents() && due.Pending().size() == 1, "the later event stays pending");
        Check(due.DrainDue(10).Fired.empty(), "a second drain fires nothing");

        (void)due.Schedule(101, "test/later/c");
        const std::int64_t again = due.Schedule(due.Now(), "test/now/d");
        const AdvanceResult zero = due.Advance(0);
        Check(!zero.Rejected && zero.Fired.size() == 1 && zero.Fired[0].Handle == again && due.Now() == 100,
              "Advance(0) fires what is due now and nothing later");
        const AdvanceResult one = due.Advance(1);
        Check(one.Fired.size() == 2 && one.Fired[0].Id == "test/later/b" && one.Fired[1].Id == "test/later/c" && due.Now() == 101,
              "same-instant events fire in scheduling order");
    }

    // The capped advance keeps every unfired event and stops where the last one fired.
    {
        GameClock capped;
        for (GameInstant t = 1; t <= 6; ++t)
            (void)capped.Schedule(t * 10, "test/many/" + std::to_string(t));
        (void)capped.Schedule(30, "test/many/tie");
        const AdvanceResult first = capped.AdvanceToCapped(1'000, 3);
        Check(first.Capped && first.Fired.size() == 3 && first.To == 30 && capped.Now() == 30,
              "a capped advance stops at the instant of the last fired event");
        Check(capped.Pending().size() == 4 && capped.HasDueEvents() && capped.Pending().front().Id == "test/many/tie",
              "the unfired event at that same instant is still pending, now due");
        const AdvanceResult none = capped.AdvanceToCapped(1'000, 0);
        Check(none.Capped && none.Fired.empty() && capped.Now() == 30 && capped.Pending().size() == 4,
              "a zero cap with something due moves nothing and loses nothing");
        const AdvanceResult rest = capped.AdvanceToCapped(1'000, 100);
        Check(!rest.Capped && rest.Fired.size() == 4 && rest.Fired[0].Id == "test/many/tie" && capped.Now() == 1'000 && capped.Pending().empty(),
              "the continuation fires the due event first, then the rest, and reaches the target");
        Check(rest.DaysCrossed == static_cast<std::int32_t>(GameClock::DayNumber(1'000) - GameClock::DayNumber(30)), "days are counted from where it stopped");
        const AdvanceResult quiet = capped.AdvanceToCapped(1'200, 0);
        Check(!quiet.Capped && capped.Now() == 1'200, "a zero cap still moves time when nothing is due on the way");
        Check(capped.AdvanceToCapped(1'100, 5).Rejected && capped.Now() == 1'200, "a capped advance never runs backwards");
    }

    // Splitting an advance anywhere fires the same events at the same instants as one advance.
    {
        std::mt19937 rng(11);
        for (int trial = 0; trial < 200; ++trial)
        {
            GameClock whole;
            GameClock split;
            for (int i = 0; i < 20; ++i)
            {
                const GameInstant when = static_cast<GameInstant>(rng() % 3000);
                (void)whole.Schedule(when, "e" + std::to_string(i));
                (void)split.Schedule(when, "e" + std::to_string(i));
            }
            const AdvanceResult once = whole.Advance(3000);
            std::vector<ScheduledEvent> pieces;
            while (split.Now() < 3000)
            {
                const AdvanceResult part = split.AdvanceToCapped(std::min<GameInstant>(3000, split.Now() + static_cast<GameInstant>(rng() % 400)),
                                                                 rng() % 4);
                pieces.insert(pieces.end(), part.Fired.begin(), part.Fired.end());
            }
            const AdvanceResult tail = split.DrainDue(100);
            pieces.insert(pieces.end(), tail.Fired.begin(), tail.Fired.end());
            bool same = once.Fired.size() == pieces.size();
            for (std::size_t i = 0; same && i < pieces.size(); ++i)
                same = once.Fired[i].Handle == pieces[i].Handle && once.Fired[i].When == pieces[i].When;
            Check(same && whole.Now() == split.Now() && split.Pending().empty(), "a split, capped advance matches one advance");
        }
    }

    std::cout << "PASS: " << Checks << " clock checks\n";
    return 0;
}
