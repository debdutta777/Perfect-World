#include "SaveSlotSummary.h"

#include "CharacterCreation.h"
#include "WorldSave.h"

namespace PerfectWorld
{
std::int64_t AgeOnInstant(const DateOfBirth& birth, const GameInstant instant) noexcept
{
    const CalendarTime calendar = GameClock::ToCalendar(instant);
    const std::int64_t years = static_cast<std::int64_t>(calendar.Year) - birth.Year;
    const bool birthdayStillToCome =
        birth.Month > calendar.Month || (birth.Month == calendar.Month && birth.Day > calendar.Day);
    return birthdayStillToCome ? years - 1 : years;
}

std::string_view ZoneDisplayName(std::string_view zoneId) noexcept
{
    if (zoneId == "zone.street.nabapara_lane")
    {
        return "Nabapara Lane";
    }
    constexpr std::string_view R62HomePrefix = "zone.home.";
    if (!zoneId.starts_with(R62HomePrefix))
    {
        return {};
    }
    zoneId.remove_prefix(R62HomePrefix.size());
    if (zoneId == InstitutionalHomeKitId)
    {
        return InstitutionalHomeKitName;
    }
    for (const HouseholdDefinition& household : CharacterCreator::HouseholdDefinitions())
    {
        if (zoneId == household.HomeKitId)
        {
            return household.HomeKitName;
        }
    }
    return {};
}

WorldSlotSummary MakeWorldSlotSummary(const WorldSaveData& data)
{
    WorldSlotSummary summary;
    summary.Name = data.Creation.Identity.GivenName;
    if (!data.Creation.Identity.FamilyName.empty())
    {
        if (!summary.Name.empty()) summary.Name += ' ';
        summary.Name += data.Creation.Identity.FamilyName;
    }
    const GameInstant instant = data.World.Clock.Now();
    summary.CurrentAge = AgeOnInstant(data.Creation.Identity.Birth, instant);
    summary.Calendar = GameClock::ToCalendar(instant);
    summary.ZoneDisplayName = ZoneDisplayName(data.Player.ZoneId);
    summary.HomeCity = data.Creation.Identity.HomeCity;
    summary.Draft = !data.Creation.HasStarted;
    return summary;
}
} // namespace PerfectWorld
