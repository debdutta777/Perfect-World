#include "SaveSlotSummary.h"

#include "CharacterCreation.h"
#include "DayLoop.h"
#include "WorldSave.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
using namespace PerfectWorld;

int R62Assertions = 0;

void R62Expect(const bool condition, const char* message)
{
    ++R62Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void R62Birthdays()
{
    const DateOfBirth birth{2025, 6, 3};
    for (const int year : {2042, 2043, 2044, 2100})
    {
        const GameInstant midnight = GameClock::FromCalendar(year, 6, 3, 0, 0);
        R62Expect(AgeOnInstant(birth, midnight - 1) == year - 2026, "minute before birthday retains previous age");
        R62Expect(AgeOnInstant(birth, midnight) == year - 2025, "birthday starts at midnight");
        R62Expect(AgeOnInstant(birth, midnight + MinutesPerDay - 1) == year - 2025, "age remains through birthday");
    }
    const DateOfBirth newYear{2026, 1, 1};
    const GameInstant january = GameClock::FromCalendar(2043, 1, 1, 0, 0);
    R62Expect(AgeOnInstant(newYear, january - 1) == 16, "31 December precedes January birthday");
    R62Expect(AgeOnInstant(newYear, january) == 17, "year boundary birthday increments age");
    const GameInstant lastDay = GameClock::FromCalendar(2042, 12, 31, 0, 0);
    R62Expect(AgeOnInstant({2025, 12, 31}, lastDay - 1) == 16, "December birthday not reached early");
    R62Expect(AgeOnInstant({2025, 12, 31}, lastDay) == 17, "December birthday reached");
    R62Expect(AgeOnInstant(birth, GameClock::FromCalendar(2025, 6, 3, 0, 0)) == 0, "age is zero on birth date");
    R62Expect(AgeOnInstant(birth, GameClock::FromCalendar(2025, 6, 2, 23, 59)) == -1, "pre-birth age is signed");

    // Every date accepted by creation agrees with its established age-at-opening contract.
    const GameInstant start = GameClock::FromCalendar(CampaignStartYear, CampaignStartMonth, CampaignStartDay,
                                                     CampaignStartHour, 0);
    for (int year = 2025; year <= 2026; ++year)
    {
        for (int month = 1; month <= 12; ++month)
        {
            for (int day = 1; day <= 31; ++day)
            {
                if (IsValidCivilDate(year, month, day))
                {
                    const DateOfBirth candidate{year, month, day};
                    R62Expect(AgeOnInstant(candidate, start) == AgeOnCampaignStart(candidate),
                              "instant age agrees with creation age on campaign start");
                }
            }
        }
    }
}

void R62LeapBirthdays()
{
    const DateOfBirth birth{2024, 2, 29};
    for (const int year : {2043, 2044, 2100, 2400})
    {
        // 2100 is not a leap year, 2400 is: let the clock validate the civil anniversary.
        const bool leapYear = IsValidCivilDate(year, 2, 29);
        const GameInstant anniversary = GameClock::FromCalendar(year, leapYear ? 2 : 3, leapYear ? 29 : 1, 0, 0);
        R62Expect(AgeOnInstant(birth, anniversary - 1) == year - 2025, "leap birth age before anniversary");
        R62Expect(AgeOnInstant(birth, anniversary) == year - 2024, "leap birth age on anniversary");
        R62Expect(AgeOnInstant(birth, GameClock::FromCalendar(year, 2, 28, 23, 59)) == year - 2025,
                  "28 February never advances a leap-day birth age");
    }
    R62Expect(AgeOnInstant({2023, 3, 1}, GameClock::FromCalendar(2044, 2, 29, 23, 59)) == 20,
              "leap day does not advance a March birthday");
}

void R62Zones()
{
    R62Expect(ZoneDisplayName("zone.street.nabapara_lane") == "Nabapara Lane", "lane has its authored name");
    R62Expect(ZoneDisplayName(OriginHomeZoneId(InstitutionalHomeKitId)) == InstitutionalHomeKitName,
              "institutional home has its kit name");
    for (const HouseholdDefinition& household : CharacterCreator::HouseholdDefinitions())
    {
        const std::string zone = OriginHomeZoneId(household.HomeKitId);
        R62Expect(!ZoneDisplayName(zone).empty() && ZoneDisplayName(zone) == household.HomeKitName,
                  "every household home resolves from the definitions");
        R62Expect(ZoneDisplayName(zone + ".unknown").empty(), "known kit prefix does not guess a subzone name");
    }
    for (const std::string_view unknown : {"", "zone.home.", "zone.home.unknown", "zone.street.unknown",
                                          "zone.street.nabapara_lane.extra", "zone.home.childrens_home_dormitory.extra",
                                          "zone.home.CHILDRENS_HOME_DORMITORY", "childrens_home_dormitory"})
    {
        R62Expect(ZoneDisplayName(unknown).empty(), "empty and unknown zones have no invented name");
    }
    const std::string wrapped = "!zone.street.nabapara_lane?";
    R62Expect(ZoneDisplayName(std::string_view(wrapped).substr(1, wrapped.size() - 2)) == "Nabapara Lane",
              "lookup respects string_view length without a null terminator");
}

bool R62SameCalendar(const CalendarTime& a, const CalendarTime& b)
{
    return a.Year == b.Year && a.Month == b.Month && a.Day == b.Day && a.Hour == b.Hour &&
           a.Minute == b.Minute && a.DayOfWeek == b.DayOfWeek;
}

bool R62SameSummary(const WorldSlotSummary& a, const WorldSlotSummary& b)
{
    return a.Name == b.Name && a.CurrentAge == b.CurrentAge && R62SameCalendar(a.Calendar, b.Calendar) &&
           a.ZoneDisplayName == b.ZoneDisplayName && a.HomeCity == b.HomeCity && a.Draft == b.Draft;
}

void R62Projection()
{
    CharacterCreator creator;
    R62Expect(creator.ApplyPreset(DefaultPresetId).Succeeded(), "projection fixture applies preset");
    R62Expect(creator.SetDateOfBirth({2025, 6, 3}).Succeeded(), "projection fixture chooses birthday");
    R62Expect(creator.StartLife().Succeeded(), "projection fixture starts life");
    WorldSaveData data;
    R62Expect(BeginWorldFromCreation(creator.CaptureSave(), 62, data).Succeeded(), "projection fixture begins world");
    data.Creation.Identity.GivenName = "Anika";
    data.Creation.Identity.FamilyName = "Sen";
    data.Creation.Identity.HomeCity = "Kolkata";
    data.Player.ZoneId = "zone.street.nabapara_lane";
    // Age in the immutable creation record is 16; a later slot must say 18.
    const GameInstant savedAt = GameClock::FromCalendar(2043, 6, 3, 14, 30);
    R62Expect(!data.World.Clock.AdvanceTo(savedAt).Rejected, "projection fixture advances clock");
    data.World.DayLoop = NewDayLoopState(savedAt);
    // R8-04: the allowance settles with the clock, as the reducer settles it on every time path.
    data.World.Commerce = SettleAllowance(data.World.Commerce, data.World.Commerce.AllowanceSettledThrough, savedAt).State;
    const WorldSlotSummary summary = MakeWorldSlotSummary(data);
    R62Expect(summary.Name == "Anika Sen" && summary.HomeCity == "Kolkata", "saved name and home city projected");
    R62Expect(summary.CurrentAge == 18 && data.Creation.Age == 16, "current age is derived from birth and clock");
    R62Expect(!summary.Draft && summary.ZoneDisplayName == "Nabapara Lane", "started life has lane facts");
    R62Expect(R62SameCalendar(summary.Calendar, GameClock::ToCalendar(savedAt)), "all date/time and weekday fields project clock");

    for (const HouseholdDefinition& household : CharacterCreator::HouseholdDefinitions())
    {
        data.Player.ZoneId = OriginHomeZoneId(household.HomeKitId);
        R62Expect(MakeWorldSlotSummary(data).ZoneDisplayName == household.HomeKitName, "summary uses each household kit");
    }
    data.Player.ZoneId = OriginHomeZoneId(InstitutionalHomeKitId);
    R62Expect(MakeWorldSlotSummary(data).ZoneDisplayName == InstitutionalHomeKitName, "summary uses institutional kit");
    data.Player.ZoneId = "zone.unknown";
    R62Expect(MakeWorldSlotSummary(data).ZoneDisplayName.empty(), "summary preserves unknown location as unnamed");
    R62Expect(MakeWorldSlotSummary(data).HomeCity == "Kolkata", "unknown location does not change home city");

    const std::string before = SerializeWorldSave(data);
    R62Expect(!before.empty(), "determinism fixture serializes");
    const WorldSlotSummary expected = MakeWorldSlotSummary(data);
    for (int repetition = 0; repetition < 20; ++repetition)
    {
        R62Expect(R62SameSummary(expected, MakeWorldSlotSummary(data)), "projection is deterministic in every field");
    }
    R62Expect(SerializeWorldSave(data) == before, "projection leaves save bytes and world hash unchanged");
    WorldSaveData restored;
    R62Expect(DeserializeWorldSave(before, restored).Succeeded(), "determinism fixture round trips");
    R62Expect(R62SameSummary(expected, MakeWorldSlotSummary(restored)), "reload projects identical summary");

    data.Creation.HasStarted = false;
    R62Expect(MakeWorldSlotSummary(data).Draft, "unfinished creation is explicitly a draft");
    R62Expect(MakeWorldSlotSummary(data).Name == "Anika Sen", "draft retains identity");
    data.Creation.Identity.GivenName.clear();
    R62Expect(MakeWorldSlotSummary(data).Name == "Sen", "family-only name has no leading separator");
    data.Creation.Identity.FamilyName.clear();
    R62Expect(MakeWorldSlotSummary(data).Name.empty(), "unnamed draft has no presentation placeholder");
    data.Creation.Identity.GivenName = "Anika";
    R62Expect(MakeWorldSlotSummary(data).Name == "Anika", "given-only name has no trailing separator");
    data.Creation.Identity.HomeCity.clear();
    R62Expect(MakeWorldSlotSummary(data).HomeCity.empty(), "missing home city is never guessed");
    const WorldSaveData freshDraft;
    const WorldSlotSummary draft = MakeWorldSlotSummary(freshDraft);
    R62Expect(draft.Draft && draft.CurrentAge == CreationAge && draft.ZoneDisplayName.empty(), "fresh draft projects defaults and draft flag");
}
} // namespace

int main()
{
    R62Birthdays();
    R62LeapBirthdays();
    R62Zones();
    R62Projection();
    std::cout << R62Assertions << " save-slot summary assertions passed\n";
    return 0;
}
