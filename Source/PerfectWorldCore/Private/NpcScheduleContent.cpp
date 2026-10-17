#include "NpcScheduleContent.h"

#include "NpcBehaviour.h"

#include <algorithm>
#include <array>
#include <set>
#include <span>

namespace PerfectWorld
{
namespace
{
struct ApprovedRoute
{
    std::string_view Id;
    std::string_view From;
    std::string_view To;
    std::int32_t MinCm;
};

/** The frozen contract of one authored schedule. */
struct Spec
{
    std::string_view ScheduleId;
    std::string_view PersonId;
    std::string_view Home;
    std::string_view Workplace;
    std::string_view Door;
    std::span<const ApprovedRoute> Routes;
    /** Zones with this prefix are the workplace; a stay in one must lie inside [DutyStart, DutyEnd). */
    std::string_view WorkZonePrefix;
    std::int32_t DutyStart;
    std::int32_t DutyEnd;
};

constexpr std::string_view CareDesk = "anchor.care_home.staff_desk";
constexpr std::string_view CareCommon = "anchor.care_home.common_room";
constexpr std::string_view CareDoor = "anchor.care_home.front_door";
constexpr std::string_view CareFootpath = "location.nabapara.care_home_footpath";
// Conservative integer ceilings over the care home's building-local layout (PerfectWorldLaneCareInterior.h): the staff
// desk chair (U 114, V 314) to the common room staff spot (U 600, V 360) through the doorway at U 506, V 435; the desk to
// the front door at U 235; the door to the pavement. Home is an explicitly authored off-lane leg, not a built interior.
constexpr std::array<ApprovedRoute, 4> CareRoutes{{
    {"route.care_home.desk_to_common", CareDesk, CareCommon, 600},
    {"route.care_home.desk_to_door", CareDesk, CareDoor, 500},
    {"route.care_home.door_to_footpath", CareDoor, CareFootpath, 300},
    {"route.care_worker_home.footpath_to_home", CareFootpath, CareWorkerHomeZone, 3000},
}};
constexpr Spec CareSpec{CareWorkerScheduleId, CareWorkerPersonId, CareWorkerHomeZone, "location.nabapara.care_home",
                        "door.nabapara.care_front", CareRoutes, "anchor.care_home.", CareWorkerDutyStartMinute,
                        CareWorkerDutyEndMinute};

// Anindita Sen, stationery proprietor. The identity constants stay in this file so the public header is unchanged.
constexpr std::string_view AnCounter = "anchor.stationery.counter";
constexpr std::string_view AnStockRoom = "anchor.stationery.stock_room";
constexpr std::string_view AnPackingDesk = "anchor.stationery.packing_desk";
constexpr std::string_view AnFrontDoor = "anchor.stationery.front_door";
constexpr std::string_view AnHome = "location.nabapara.sen_home";
// Built shop polylines. The front-door anchor is already outside on the footpath (U 250, V -45),
// so there is no additional door-to-footpath connector. Home retains its authored off-lane length.
constexpr std::array<ApprovedRoute, 4> AninditaRoutes{{
    {"route.stationery.counter_to_stock_room", AnCounter, AnStockRoom, 635},
    {"route.stationery.counter_to_packing_desk", AnCounter, AnPackingDesk, 727},
    {"route.stationery.counter_to_door", AnCounter, AnFrontDoor, 1100},
    {"route.sen_home.footpath_to_home", AnFrontDoor, AnHome, 2000},
}};
// At the shop from the 07:30 stock check to the end of the reconciliation (minutes of the day, end exclusive).
constexpr Spec AninditaSpec{"npc_schedule.anindita_sen", "person.anindita_sen", AnHome, "location.nabapara.stationery",
                            "door.nabapara.stationery_front", AninditaRoutes, "anchor.stationery.", 7 * 60 + 30, 21 * 60};

constexpr std::array<const Spec*, 2> AllSpecs{&CareSpec, &AninditaSpec};

const Spec* FindSpec(std::string_view personId)
{
    for (const Spec* spec : AllSpecs)
        if (personId == spec->PersonId)
            return spec;
    return nullptr;
}
const Spec* FindSpecBySchedule(std::string_view scheduleId)
{
    for (const Spec* spec : AllSpecs)
        if (scheduleId == spec->ScheduleId)
            return spec;
    return nullptr;
}

std::string String(const JsonValue& object, std::string_view field)
{
    return object.Find(field)->String;
}
std::int32_t Integer(const JsonValue& object, std::string_view field)
{
    return static_cast<std::int32_t>(*object.Find(field)->Integer);
}

std::string ValidateRoutes(const NpcSchedule& schedule, const Spec& spec)
{
    if (schedule.Routes.size() != spec.Routes.size())
        return "all approved routes are required";
    std::set<std::string> ids;
    for (const auto& route : schedule.Routes)
    {
        if (!ids.insert(route.Id).second)
            return "duplicate route " + route.Id;
        const auto expected = std::find_if(spec.Routes.begin(), spec.Routes.end(), [&](const auto& r) { return r.Id == route.Id; });
        if (expected == spec.Routes.end() || route.From != expected->From || route.To != expected->To ||
            route.LengthCm < expected->MinCm || route.LengthCm > 100000)
            return "invalid approved route " + route.Id;
    }
    if (schedule.Paths.size() != schedule.Travel.Legs.size())
        return "every travel leg requires an explicit route path";
    for (std::size_t i = 0; i < schedule.Paths.size(); ++i)
    {
        const auto& path = schedule.Paths[i];
        const auto& leg = schedule.Travel.Legs[i];
        if (path.From != leg.From || path.To != leg.To || path.Minutes != leg.Minutes || path.Routes.empty())
            return "travel path differs from its leg";
        std::string current = path.From;
        std::set<std::string> visited{current};
        std::int64_t length = 0;
        for (const auto& id : path.Routes)
        {
            const auto route = std::find_if(schedule.Routes.begin(), schedule.Routes.end(),
                                            [&](const auto& r) { return r.Id == id; });
            if (route == schedule.Routes.end())
                return "unknown route " + id;
            if (route->From == current)
                current = route->To;
            else if (route->To == current)
                current = route->From;
            else
                return "disconnected route path";
            if (!visited.insert(current).second)
                return "route path repeats a zone";
            length += route->LengthCm;
        }
        if (current != path.To || length > static_cast<std::int64_t>(path.Minutes) * FaridaTravelCmPerMinute)
            return "route destination or travel duration is invalid";
    }
    return {};
}

std::string ValidateWithSpec(const NpcSchedule& schedule, const Spec& spec)
{
    const auto& person = schedule.Person;
    if (person.PersonId != spec.PersonId || person.HomeZone != spec.Home || !person.Exceptions.empty())
        return std::string(spec.PersonId) + " requires the frozen identity, home and ordinary weekly routine";
    if (const auto error = ValidatePersonSchedule(person); !error.empty())
        return error;
    if (const auto error = ValidateNpcSchedule(person); !error.empty())
        return error;
    std::set<std::string_view> zones{spec.Home};
    for (const auto& route : spec.Routes)
    {
        zones.insert(route.From);
        zones.insert(route.To);
    }
    for (const auto& entry : person.Routine)
    {
        if (!zones.contains(entry.Zone) || entry.Kind != ScheduleDoingKind::Role || entry.Priority != 0)
            return "routine requires approved zones and ordinary roles";
        // The hours at work are bounded: a stay in a workplace zone lies wholly inside the duty window.
        if (entry.Zone.starts_with(spec.WorkZonePrefix) && (entry.StartMinute < spec.DutyStart || entry.EndMinute > spec.DutyEnd))
            return "routine places the person at work outside the duty hours";
    }
    if (const auto error = ValidateScheduleTravel(schedule.Travel); !error.empty())
        return error;
    if (const auto error = ValidateRoutes(schedule, spec); !error.empty())
        return error;
    for (const auto& leg : schedule.Travel.Legs)
        if (!zones.contains(leg.From) || !zones.contains(leg.To))
            return "travel leg uses an unapproved zone";
    // Explicit all-pairs coverage is the core contract; it keeps the fallback duration from hiding an absent leg.
    PersonSchedule coverage = person;
    for (const auto zone : zones)
        coverage.Routine.push_back({ScheduleAllWeekdays, 0, 1, std::string(zone), ScheduleDoingKind::Role, "resting", 0});
    if (const auto error = ValidateScheduleTravelCoverage(coverage, schedule.Travel); !error.empty())
        return error;
    if (const auto error = ValidateScheduleRoutineTravel(person, schedule.Travel); !error.empty())
        return error;
    // Raw spans deliberately leave travel gaps. Demand resolved coverage: no idle gap, nothing squeezed, every weekday.
    constexpr GameInstant start = 7 * MinutesPerDay - CampaignStartHour * MinutesPerHour;
    constexpr GameInstant end = start + 7 * MinutesPerDay;
    for (GameInstant at = start; at < end;)
    {
        const auto resolved = ResolveSchedule(person, schedule.Travel, at);
        if (!resolved.Valid || resolved.Squeezed || resolved.Status == ScheduleStatus::Idle || resolved.SegmentEnd <= at)
            return "routine lacks complete unsqueezed resolved weekly coverage";
        at = resolved.SegmentEnd;
    }
    return {};
}

NpcScheduleResult LoadWithSpec(const ContentDb& db, const Spec& spec)
{
    const auto* record = db.Find(spec.ScheduleId);
    if (record == nullptr || record->Type != "npc_schedule" || record->Id != spec.ScheduleId)
        return {{}, std::string(spec.ScheduleId) + " NPC schedule record is missing or has the wrong type"};
    const auto& data = record->Data;
    if (const auto error = ContentSchema::ValidateNpcScheduleShape(data); !error.empty())
        return {{}, error};
    if (String(data, "type") != "npc_schedule" || String(data, "id") != spec.ScheduleId || String(data, "person") != spec.PersonId ||
        String(data, "workplace") != spec.Workplace || String(data, "door") != spec.Door)
        return {{}, "NPC schedule metadata differs from the frozen contract"};
    NpcSchedule out;
    out.Person.PersonId = String(data, "person");
    out.Person.HomeZone = String(data, "home");
    for (const auto& item : data.Find("routine")->Array)
        out.Person.Routine.push_back({static_cast<std::uint8_t>(Integer(item, "days")), Integer(item, "start"),
            Integer(item, "end"), String(item, "zone"), ScheduleDoingKind::Role, String(item, "role"), 0});
    for (const auto& item : data.Find("routes")->Array)
        out.Routes.push_back({String(item, "id"), String(item, "from"), String(item, "to"), Integer(item, "lengthCm")});
    for (const auto& item : data.Find("travel")->Array)
    {
        FaridaTravelPath path{String(item, "from"), String(item, "to"), Integer(item, "minutes"), {}};
        for (const auto& route : item.Find("routes")->Array)
            path.Routes.push_back(route.String);
        out.Travel.Legs.push_back({path.From, path.To, path.Minutes});
        out.Paths.push_back(std::move(path));
    }
    if (const auto error = ValidateWithSpec(out, spec); !error.empty())
        return {{}, error};
    return {std::move(out), {}};
}
} // namespace

NpcScheduleResult LoadNpcSchedule(const ContentDb& db, std::string_view scheduleId)
{
    if (scheduleId == FaridaScheduleId)
        // Preserve her complete frozen route/identity contract and resolved weekly coverage without duplicating it.
        return LoadFaridaSchedule(db);
    if (const auto* spec = FindSpecBySchedule(scheduleId))
        return LoadWithSpec(db, *spec);
    return {{}, "unsupported NPC schedule identity"};
}

std::string ValidateNpcScheduleContent(const NpcSchedule& schedule)
{
    if (schedule.Person.PersonId == FaridaPersonId)
        return ValidateFaridaSchedule(schedule);
    if (const auto* spec = FindSpec(schedule.Person.PersonId))
        return ValidateWithSpec(schedule, *spec);
    return "unsupported NPC schedule identity";
}
} // namespace PerfectWorld
