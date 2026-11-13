#include "FaridaSchedule.h"

#include "NpcBehaviour.h"

#include <algorithm>
#include <array>
#include <set>

namespace PerfectWorld
{
namespace
{
constexpr std::string_view Home = "location.nabapara.farida_home";
constexpr std::string_view Footpath = "location.nabapara.shanti_chai_footpath";
constexpr std::string_view Counter = "anchor.shanti_chai.counter";
constexpr std::string_view Door = "anchor.shanti_chai.door";

struct ApprovedRoute
{
    std::string_view Id;
    std::string_view From;
    std::string_view To;
    std::int32_t MinCm;
};
// TS RouteTable, summing ceil(integer segment lengths). Counter-to-door also
// includes the 135 cm connector from V=90 to the door stand point V=-45.
// Full door-to-footpath polyline is conservatively budgeted, including V=90.
// Home is an explicitly authored off-lane leg, not a claimed built interior.
constexpr std::array<ApprovedRoute, 8> Approved{{
    {"route.shanti_chai.counter_to_bench_wipe", Counter, "anchor.shanti_chai.bench_wipe", 1002},
    {"route.shanti_chai.counter_to_door", Counter, Door, 1082},
    {"route.shanti_chai.counter_to_rest", Counter, "anchor.shanti_chai.rest", 1070},
    {"route.shanti_chai.counter_to_storage", Counter, "anchor.shanti_chai.storage", 861},
    {"route.shanti_chai.counter_to_stove", Counter, "anchor.shanti_chai.stove", 58},
    {"route.shanti_chai.counter_to_wash", Counter, "anchor.shanti_chai.wash", 150},
    {"route.shanti_chai.door_to_footpath", Door, Footpath, 270},
    {"route.farida_home.footpath_to_home", Footpath, Home, 1400},
}};

std::string String(const JsonValue& object, std::string_view field)
{
    return object.Find(field)->String;
}
std::int32_t Integer(const JsonValue& object, std::string_view field)
{
    return static_cast<std::int32_t>(*object.Find(field)->Integer);
}

std::string ValidateRoutes(const FaridaSchedule& schedule)
{
    if (schedule.Routes.size() != Approved.size())
        return "all eight approved routes are required";
    std::set<std::string> ids;
    for (const auto& route : schedule.Routes)
    {
        if (!ids.insert(route.Id).second)
            return "duplicate route " + route.Id;
        const auto expected = std::find_if(Approved.begin(), Approved.end(), [&](const auto& r) { return r.Id == route.Id; });
        if (expected == Approved.end() || route.From != expected->From || route.To != expected->To ||
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
} // namespace

std::string ValidateFaridaSchedule(const FaridaSchedule& schedule)
{
    const auto& person = schedule.Person;
    if (person.PersonId != FaridaPersonId || person.HomeZone != Home || !person.Exceptions.empty())
        return "Farida requires the frozen identity, home and ordinary weekly routine";
    if (const auto error = ValidatePersonSchedule(person); !error.empty())
        return error;
    if (const auto error = ValidateNpcSchedule(person); !error.empty())
        return error;
    std::set<std::string_view> zones{Home, Footpath};
    for (const auto& route : Approved)
    {
        zones.insert(route.From);
        zones.insert(route.To);
    }
    for (const auto& entry : person.Routine)
    {
        if (!zones.contains(entry.Zone) || entry.Kind != ScheduleDoingKind::Role || entry.Priority != 0)
            return "routine requires approved zones and ordinary roles";
    }
    if (const auto error = ValidateScheduleTravel(schedule.Travel); !error.empty())
        return error;
    if (const auto error = ValidateRoutes(schedule); !error.empty())
        return error;
    // Explicit all-pairs coverage is the core contract; it also prevents its
    // fallback duration from hiding an absent leg, including home and footpath.
    for (const auto& leg : schedule.Travel.Legs)
        if (!zones.contains(leg.From) || !zones.contains(leg.To))
            return "travel leg uses an unapproved zone";
    PersonSchedule coverage = person;
    for (const auto zone : zones)
        coverage.Routine.push_back({ScheduleAllWeekdays, 0, 1, std::string(zone), ScheduleDoingKind::Role, "resting", 0});
    if (const auto error = ValidateScheduleTravelCoverage(coverage, schedule.Travel); !error.empty())
        return error;
    if (const auto error = ValidateScheduleRoutineTravel(person, schedule.Travel); !error.empty())
        return error;
    // Raw routine spans deliberately leave travel gaps. Demand resolved coverage
    // instead of treating those gaps as idle or overlapping stays with travel.
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

FaridaScheduleResult LoadFaridaSchedule(const ContentDb& db)
{
    const auto* record = db.Find(FaridaScheduleId);
    if (record == nullptr || record->Type != "npc_schedule" || record->Id != FaridaScheduleId)
        return {{}, "Farida NPC schedule record is missing or has the wrong type"};
    const auto& data = record->Data;
    if (const auto error = ContentSchema::ValidateNpcScheduleShape(data); !error.empty())
        return {{}, error};
    if (String(data, "type") != "npc_schedule" || String(data, "id") != FaridaScheduleId ||
        String(data, "workplace") != "location.nabapara.shanti_chai" ||
        String(data, "door") != "door.nabapara.shanti_chai_shutter")
        return {{}, "NPC schedule metadata differs from the frozen contract"};
    FaridaSchedule out;
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
    if (const auto error = ValidateFaridaSchedule(out); !error.empty())
        return {{}, error};
    return {std::move(out), {}};
}
} // namespace PerfectWorld
