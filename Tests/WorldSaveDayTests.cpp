// The player's day in the world save (ROADMAP M1-05, M2-12): the v7 "dayloop" member, the v6 save that
// has none, and the one clock. The scripted day with real content is in DayLoopWorldTests.cpp.
#include "WorldSave.h"

#include "CharacterCreation.h"
#include "CreationSaveSerialization.h"
#include "GateSites.h"
#include "WorldHash.h"
#include "WorldReducer.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

using namespace PerfectWorld;

#include "WorldSaveTestSupport.h"

namespace
{
constexpr std::size_t DayTestNpos = std::string::npos;

struct DayTestSpan
{
    std::size_t Begin = 0;
    std::size_t End = 0;
};

/** The current world record's "dayloop" member, ending before the lane sections. */
DayTestSpan DayLoopSpan(const std::string& text)
{
    DayTestSpan span;
    span.Begin = text.find(",\n    \"dayloop\": ");
    Check(span.Begin != DayTestNpos, "the save has a dayloop member");
    span.End = text.find(",\n    \"conversations\":", span.Begin);
    Check(span.End != DayTestNpos, "the dayloop member ends before conversations");
    return span;
}

/** The same save as a build before the day loop wrote it: schema 6, hash version 3, no "dayloop" member. */
std::string AsVersionSix(std::string text)
{
    const DayTestSpan span = DayLoopSpan(text);
    const std::size_t end = text.find("\n  }", span.End);
    text.erase(span.Begin, end - span.Begin);
    text = ReplaceOnce(text, "\"schemaVersion\": " + std::to_string(WorldSaveSchemaVersion), "\"schemaVersion\": 6");
    return ReplaceOnce(text, "\"hashVersion\": " + std::to_string(WorldHashVersion), "\"hashVersion\": 3");
}

void TestOldVersionSix()
{
    WorldSaveData rich = RichSave(51, 40);
    Check(rich.World.Clock.Now() > 0 && rich.World.DayLoop.Seq == 3, "the rich world has been running and has a busy day");
    const std::string v6 = AsVersionSix(SerializeWorldSave(rich));
    Check(v6.find("dayloop") == DayTestNpos && v6.find("\"schemaVersion\": 6") != DayTestNpos, "the v6 text has no day loop");

    WorldSaveData loaded = Sentinel();
    const WorldSaveResult result = DeserializeWorldSave(v6, loaded);
    Check(result.Succeeded() && !result.Migrated && result.SourceVersion == 6 && !result.HashVerified,
          "a v6 save loads as v6; its hash version is one this build cannot recompute, so it is unverified");
    // Everything else is as saved; the player has a fresh day beginning at the world clock's instant.
    WorldSaveData expected = rich;
    expected.World.DayLoop = NewDayLoopState(rich.World.Clock.Now());
    // R8-04: no commerce before v10; the household's starting wallet is granted once at the saved instant.
    expected.World.Commerce = InitialiseCommerce(1, rich.World.Clock.Now()).State;
    CheckSame(expected, loaded, "an old v6 save loads with a fresh day at the world clock");
    Check(loaded.World.DayLoop.Now == rich.World.Clock.Now() && loaded.World.DayLoop.Seq == 0 && loaded.World.DayLoop.Phase == DayPhase::Plan &&
              loaded.World.DayLoop.Attendance == NewDayLoopState(rich.World.Clock.Now()).Attendance && IsWorldDayInStep(loaded.World),
          "the fresh day is awake, unplanned and in step with the clock");

    // Saving it again writes the current schema, which verifies, and the world is unchanged by the round trip.
    WorldSaveData again = Sentinel();
    const WorldSaveResult second = DeserializeWorldSave(SerializeWorldSave(loaded), again);
    Check(second.Succeeded() && second.SourceVersion == WorldSaveSchemaVersion && second.HashVerified && HashWorldState(again.World) == HashWorldState(loaded.World),
          "what an old save was loaded as is saved as the current schema and verifies");

    // v6 is refused as v6 would always have been: truncated, a key renamed, an unexpected day loop key, the wrong version.
    for (std::size_t cut = 0; cut < v6.size(); ++cut)
    {
        WorldSaveData out = Sentinel();
        if (DeserializeWorldSave(std::string_view(v6).substr(0, cut), out).Succeeded() || !Untouched(out))
        {
            std::cerr << "prefix of " << cut << " of " << v6.size() << " bytes was not refused\n";
            Check(false, "every proper prefix of a v6 save is refused");
        }
    }
    Check(ErrorOf(ReplaceOnce(v6, "\"rng\"", "\"rngX\"")) == WorldSaveError::Malformed, "a renamed member of a v6 record is refused");
    Check(ErrorOf(ReplaceOnce(v6, "\"rng\":", "\"dayloop\": \"\", \"rng\":")) == WorldSaveError::Malformed, "a v6 record may not carry a day loop");
    Check(ErrorOf(ReplaceOnce(v6, "\"schemaVersion\": 6", "\"schemaVersion\": " + std::to_string(WorldSaveSchemaVersion + 1))) == WorldSaveError::UnsupportedVersion, "a version after the current one is unsupported");

    // A clock the day loop cannot hold (before the campaign, or past its hundred years) is refused rather than given an invalid day.
    for (const GameInstant clock : {GameInstant{-5}, DayLoopMaxInstant + 1})
    {
        // Today's writer refuses a world whose allowance cannot stand at such a clock (R8-04), so the old file's clock is
        // written into the text of a valid save: no pending events, as a bare clock at that instant would have.
        std::string oddText = AsVersionSix(SerializeWorldSave(RichSave(52, 3)));
        const std::size_t clockAt = oddText.find("\"clock\": {");
        Check(clockAt != DayTestNpos, "the v6 text has a clock");
        oddText.replace(clockAt, oddText.find("] }", clockAt) + 3 - clockAt,
                        "\"clock\": { \"now\": " + std::to_string(clock) + ", \"nextHandle\": 1, \"pending\": [] }");
        const WorldSaveResult refused = [&]
        {
            WorldSaveData out = Sentinel();
            const WorldSaveResult r = DeserializeWorldSave(oddText, out);
            Check(!r.Succeeded() && Untouched(out), "a v6 save with an unusable clock is refused and leaves the target untouched");
            return r;
        }();
        Check(refused.Error == WorldSaveError::InvalidWorld && !refused.Message.empty(), "and says the world is impossible");
    }
}

void TestVersionSevenDay()
{
    const WorldSaveData rich = RichSave(61, 30);
    const std::string text = SerializeWorldSave(rich);
    Check(ErrorOf(text) == WorldSaveError::None, "the rich save loads");

    // The day and the clock must agree about the time, whatever else is true of the day.
    {
        WorldSaveData ahead = rich;
        ahead.World.DayLoop.Now += 1;
        ahead.World.DayLoop.Phase = DayLoopPhaseOf(ahead.World.DayLoop);
        Check(IsDayLoopStateValid(ahead.World.DayLoop) && !IsWorldDayInStep(ahead.World), "a valid day a minute ahead of the clock is out of step");
        WorldSaveData out = Sentinel();
        const WorldSaveResult result = DeserializeWorldSave(SerializeWorldSave(ahead), out);
        Check(result.Error == WorldSaveError::InvalidWorld && Untouched(out) && result.Message.find("disagree") != DayTestNpos,
              "a save whose day and clock disagree is refused as an impossible world");
    }
    {
        WorldSaveData odd = rich;
        GameClockSave save;
        save.Now = -5;
        Check(odd.World.Clock.RestoreSave(save), "set a negative clock");
        Check(SerializeWorldSave(odd).empty(), "the writer refuses an allowance that cannot stand at the clock (R8-04)");
        std::string oddText = text;
        const std::size_t clockAt = oddText.find("\"clock\": {");
        oddText.replace(clockAt, oddText.find("] }", clockAt) + 3 - clockAt, "\"clock\": { \"now\": -5, \"nextHandle\": 1, \"pending\": [] }");
        Check(ErrorOf(oddText) == WorldSaveError::InvalidWorld, "a clock before the campaign cannot hold a day");
    }
    {
        WorldSaveData invalid = rich;
        invalid.World.DayLoop.Phase = invalid.World.DayLoop.Phase == DayPhase::Sleep ? DayPhase::Plan : DayPhase::Sleep;
        Check(!IsDayLoopStateValid(invalid.World.DayLoop) && SerialiseDayLoop(invalid.World.DayLoop).empty(), "a day whose phase is wrong is invalid");
        Check(ErrorOf(SerializeWorldSave(invalid)) == WorldSaveError::InvalidWorld, "a save carrying an invalid day is refused");
    }

    // Damage to the day itself: its envelope is strict and carries its own hash.
    Check(ErrorOf(ReplaceOnce(text, "condition energy=", "condition energy=9")) == WorldSaveError::InvalidWorld, "a changed meter in the day fails its own hash");
    Check(ErrorOf(ReplaceOnce(text, "\\nend\\n\"", "\\nend\\nx\"")) == WorldSaveError::InvalidWorld, "text after the day's end is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"dayloop\": \"pw.dayloop/1", "\"dayloop\": \"pw.dayloop/2")) == WorldSaveError::InvalidWorld, "another envelope version is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"dayloop\"", "\"dayloopX\"")) == WorldSaveError::Malformed, "a missing day loop member is refused");
    {
        std::string number = text;
        const DayTestSpan span = DayLoopSpan(number);
        number.replace(span.Begin, span.End - span.Begin, ",\n    \"dayloop\": 5");
        Check(ErrorOf(number) == WorldSaveError::InvalidWorld, "a day that is not text is refused");
    }

    // A different but perfectly valid day spliced into the file passes its own hash and is caught by the world's.
    {
        WorldSaveData other = rich;
        other.World.DayLoop.Condition.Energy -= 1;
        Check(IsWorldDayInStep(other.World), "the other day is valid and in step");
        const std::string otherText = SerializeWorldSave(other);
        const DayTestSpan mine = DayLoopSpan(text);
        const DayTestSpan theirs = DayLoopSpan(otherText);
        std::string spliced = text;
        spliced.replace(mine.Begin, mine.End - mine.Begin, otherText.substr(theirs.Begin, theirs.End - theirs.Begin));
        Check(spliced != text, "the splice changed the file");
        Check(ErrorOf(spliced) == WorldSaveError::HashMismatch, "a different valid day fails the world hash");
        Check(HashWorldState(other.World) != HashWorldState(rich.World), "the world hash covers the day");
    }
}

/** Save, load, then keep going: the loaded world advances like the one that never stopped. */
void TestOneClock()
{
    const CharacterCreationSave creation = MakeCreation("parent_household");
    WorldSim live(5, CreatorOf(creation));
    Check(live.Submit({CommandSource::Player, AdvanceTime{90}}).Accepted, "advance before the save");
    WorldSaveData loaded = Sentinel();
    const WorldSaveResult result = DeserializeWorldSave(SerializeWorldSave(SaveOf(live, creation)), loaded);
    Check(result.Succeeded() && result.HashVerified, "the save loads");
    Check(loaded.World.Clock.Now() == 90 && loaded.World.DayLoop.Now == 90 && IsWorldDayInStep(loaded.World),
          "the world clock and the day's clock agree after a load");

    WorldSim resumed(loaded.World);
    Check(resumed.Log().empty() && HashWorldState(resumed.State()) == HashWorldState(live.State()), "a resumed world is the saved world");
    const CommandResult step = resumed.Submit({CommandSource::Player, AdvanceTime{1000}});
    Check(step.Accepted && live.Submit({CommandSource::Player, AdvanceTime{1000}}).Accepted, "both advance");
    Check(resumed.State().Clock.Now() == 1090 && resumed.State().DayLoop.Now == 1090 && IsWorldDayInStep(resumed.State()),
          "the clocks agree after advancing");
    Check(HashWorldState(resumed.State()) == HashWorldState(live.State()) && resumed.State().Seq == live.State().Seq,
          "the resumed world and the one that never stopped reach the same hash");

    // A capped advance stops short, and the day stops with the clock.
    WorldSim capped(7, 2);
    for (GameInstant t = 1; t <= 6; ++t)
        Check(capped.Submit({CommandSource::System, ScheduleWorldEvent{t * 10, "test/many/" + std::to_string(t)}}).Accepted, "schedule");
    const CommandResult partial = capped.Submit({CommandSource::Player, AdvanceTime{500}});
    Check(partial.Accepted && partial.Partial && capped.State().Clock.Now() < 500 && capped.State().DayLoop.Now == capped.State().Clock.Now(),
          "an advance cut short by the event cap leaves the day with the clock, not at the target");

    // Time that would leave the player's calendar, and time while the day holds the clock, are refused without a trace.
    {
        // A life's calendar is a hundred years: exactly ten of the longest single advances.
        WorldSim longLived(1);
        for (int i = 0; i < 10; ++i)
            Check(longLived.Submit({CommandSource::Player, AdvanceTime{MaxSingleAdvanceMinutes}}).Accepted, "a decade passes");
        Check(IsWorldDayInStep(longLived.State()) && longLived.State().Clock.Now() == DayLoopMaxInstant, "the clock is at the end of the day's calendar");
        const std::uint64_t atEnd = HashWorldState(longLived.State());
        const CommandResult over = longLived.Submit({CommandSource::Player, AdvanceTime{1}});
        Check(!over.Accepted && over.Why.Code == "time.beyond_calendar" && HashWorldState(longLived.State()) == atEnd,
              "the calendar ends after a hundred years, and a refused advance changes nothing");
    }
    {
        WorldState sleeper = MakeWorldState(11);
        BusyDay(sleeper);
        sleeper.DayLoop.Asleep = true;
        sleeper.DayLoop.SleptAt = sleeper.DayLoop.Now;
        sleeper.DayLoop.Phase = DayLoopPhaseOf(sleeper.DayLoop);
        Check(IsWorldDayInStep(sleeper), "an asleep day is valid");
        WorldSim asleep(sleeper);
        const std::uint64_t before = HashWorldState(asleep.State());
        const CommandResult refused = asleep.Submit({CommandSource::Player, AdvanceTime{10}});
        Check(!refused.Accepted && refused.Why.Code == "time.asleep" && HashWorldState(asleep.State()) == before && asleep.Log().empty(),
              "time does not pass behind a sleeping player's back");
        Check(asleep.Submit({CommandSource::Player, SetWorldFlag{"still.works", true}}).Accepted, "other commands are unaffected");
    }
}
} // namespace

void TestWorldSaveDay()
{
    TestOldVersionSix();
    TestVersionSevenDay();
    TestOneClock();
}
