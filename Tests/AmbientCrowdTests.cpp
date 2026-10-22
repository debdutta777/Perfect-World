#include "AmbientCrowd.h"

#include "Json.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>

using namespace PerfectWorld;
namespace
{
int R75Assertions = 0;
void R75Expect(bool value, const std::string& message)
{
    ++R75Assertions;
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
GameInstant R75At(int minute, CalendarDay day = 0)
{
    return CalendarDayStartInstant(day)+minute;
}
SchoolCalendar R75Calendar(const char* path)
{
    std::ifstream input(path);
    R75Expect(input.good(), "authoritative calendar file opens");
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    const auto json = ParseJson(text);
    R75Expect(json.Value.has_value(), "calendar JSON parses");
    ContentRecord record;
    record.Type = "calendar"; record.Id = "calendar.nabapara_2042_43"; record.Data = *json.Value;
    std::vector<std::string> problems;
    const auto calendar = ParseSchoolCalendarRecord(record, problems);
    R75Expect(problems.empty() && ValidateSchoolCalendar(calendar).empty(), "real calendar passes its rules");
    return calendar;
}
std::int64_t R75Distance(NeighbourhoodPoint a, NeighbourhoodPoint b)
{
    const auto abs = [](std::int64_t v) { return v < 0 ? -v : v; };
    return abs(std::int64_t(a.X)-b.X)+abs(std::int64_t(a.Y)-b.Y)+abs(std::int64_t(a.Z)-b.Z);
}
bool R75Inside(NeighbourhoodPoint p, NeighbourhoodRect r)
{
    return p.X > r.X0 && p.X < r.X1 && p.Y > r.Y0 && p.Y < r.Y1;
}
bool R75OnSegment(NeighbourhoodPoint p, const CrowdRouteSegment& segment)
{
    return R75Distance(segment.From, p)+R75Distance(p, segment.To) == segment.LengthCm;
}
const CrowdPerson* R75Person(const CrowdSnapshot& snapshot, const std::string& id)
{
    for (const auto& person : snapshot.People) if (person.Id == id) return &person;
    return nullptr;
}
void R75Routes(const AmbientCrowd& crowd, const NeighbourhoodPlan& plan)
{
    std::set<CrowdCohort> cohorts;
    for (const auto& slot : crowd.RouteSlots())
    {
        cohorts.insert(slot.Cohort);
        const auto& route = slot.Route;
        R75Expect(route.Found && route.NodeIds.size() == route.LinkIds.size()+1, "route topology");
        std::int64_t length = 0;
        for (std::size_t i = 0; i < route.LinkIds.size(); ++i)
        {
            const auto link = std::find_if(plan.Links.begin(), plan.Links.end(), [&](const auto& edge) {
                return edge.Id == route.LinkIds[i]; });
            R75Expect(link != plan.Links.end(), "route references authored link");
            R75Expect((link->From == route.NodeIds[i] && link->To == route.NodeIds[i+1]) ||
                (link->To == route.NodeIds[i] && link->From == route.NodeIds[i+1]), "every route connected");
            R75Expect(link->Kind != NeighbourhoodLinkKind::Interior && link->Kind != NeighbourhoodLinkKind::Stair,
                      "no anonymous access through private interiors");
            length += link->LengthCm;
        }
        R75Expect(length == route.LengthCm, "route length");
        std::int64_t expanded = 0;
        for (std::size_t i = 0; i < slot.Segments.size(); ++i)
        {
            const auto& segment = slot.Segments[i];
            expanded += segment.LengthCm;
            R75Expect(segment.LengthCm == R75Distance(segment.From, segment.To), "polyline metric");
            if (i > 0) R75Expect(slot.Segments[i-1].To == segment.From, "polyline continuity");
        }
        R75Expect(expanded == length, "expanded route retains all bends");
        if (slot.Cohort == CrowdCohort::Student)
            R75Expect(slot.SchoolOnly && ((slot.BeginMinute == 405 && slot.EndMinute == 450) ||
                (slot.BeginMinute == 810 && slot.EndMinute == 870)), "morning school shift");
        if (slot.Cohort == CrowdCohort::OfficeCommuter)
            R75Expect(slot.EnterEntranceId == "stop.nabapara.bus_west" &&
                route.NodeIds.back() == "walk.nabapara.main_cross_west", "commuters use main-road bus stop");
    }
    R75Expect(cohorts.size() == 6, "all six cohorts authored");
}
void R75Day(const AmbientCrowd& crowd, const NeighbourhoodPlan& plan, CalendarDay day, bool school)
{
    std::set<CrowdTimeBand> bands;
    std::set<CrowdActivity> activities;
    const NeighbourhoodPoint centre{6500, 4700};
    for (int minute = 0; minute < 1440; ++minute)
    {
        const auto at = CalendarDayStartInstant(day)+minute;
        const auto snapshot = crowd.CrowdAt(at, centre, 10000);
        const auto& target = snapshot.Target;
        bands.insert(target.Band);
        R75Expect(snapshot.People.size() >= static_cast<std::size_t>(target.Minimum) &&
            snapshot.People.size() <= static_cast<std::size_t>(target.Maximum), "density at minute " + std::to_string(minute));
        const bool peak = school && ((minute >= 405 && minute < 450) || (minute >= 810 && minute < 870));
        R75Expect((target.Band == CrowdTimeBand::SchoolArrival || target.Band == CrowdTimeBand::SchoolDeparture) == peak,
                  "calendar controls target band");
        std::set<std::string> ids;
        std::size_t full = 0, instanced = 0, nearSchool = 0, students = 0;
        std::int64_t previousDistance = -1;
        for (const auto& person : snapshot.People)
        {
            R75Expect(ids.insert(person.Id).second && person.Id.starts_with("ambient."), "anonymous unique namespace");
            activities.insert(person.Activity);
            full += person.Lod == CrowdLod::FullActor;
            instanced += person.Lod == CrowdLod::InstancedFigure;
            const auto dx = std::int64_t(person.Position.X)-target.NearSchoolCentre.X;
            const auto dy = std::int64_t(person.Position.Y)-target.NearSchoolCentre.Y;
            nearSchool += dx*dx+dy*dy <= 3000LL*3000;
            const auto sortY = std::int64_t(person.Position.Y)-centre.Y;
            const auto distance = dx*dx+sortY*sortY;
            R75Expect(distance >= previousDistance, "nearest first"); previousDistance = distance;
            if (person.Cohort == CrowdCohort::Student)
            {
                ++students;
                R75Expect(person.Outfit == CrowdOutfit::WhiteShirtBlueTrousers ||
                    person.Outfit == CrowdOutfit::WhiteShirtBlueSkirt, "white and blue student uniforms");
                R75Expect(person.ShortExchangeTextKey == (minute < 450 ? "ambient.exchange.heading_to_school" :
                    "ambient.exchange.greeting"), "student exchange key matches arrival/departure");
            }
            R75Expect(!person.RecordsJournalFacts && !person.ShortExchangeTextKey.empty(), "exchanges cannot record facts");
            const auto slot = std::find_if(crowd.RouteSlots().begin(), crowd.RouteSlots().end(), [&](const auto& candidate) {
                return candidate.Slot == person.Slot; });
            R75Expect(slot != crowd.RouteSlots().end(), "person has route slot");
            R75Expect(std::any_of(slot->Segments.begin(), slot->Segments.end(), [&](const auto& segment) {
                return segment.LinkId == person.LinkId && R75OnSegment(person.Position, segment); }), "person on graph polyline");
            for (const auto& feature : plan.Features)
            {
                if (feature.Kind == NeighbourhoodFeatureKind::Footprint &&
                    (feature.FootprintKind == NeighbourhoodFootprintKind::ExistingBuilding ||
                     feature.FootprintKind == NeighbourhoodFootprintKind::TeachingBlock ||
                     feature.FootprintKind == NeighbourhoodFootprintKind::Club ||
                     feature.FootprintKind == NeighbourhoodFootprintKind::SweetShop ||
                     feature.FootprintKind == NeighbourhoodFootprintKind::Pharmacy ||
                     feature.FootprintKind == NeighbourhoodFootprintKind::TeaPitch ||
                     feature.FootprintKind == NeighbourhoodFootprintKind::VendorPitch ||
                     feature.FootprintKind == NeighbourhoodFootprintKind::TransportStand))
                    R75Expect(!R75Inside(person.Position, feature.Bounds), "no person in a solid/pitch footprint");
                if (feature.Kind == NeighbourhoodFeatureKind::Carriageway && R75Inside(person.Position, feature.Bounds))
                    R75Expect(std::any_of(plan.Features.begin(), plan.Features.end(), [&](const auto& crossing) {
                        return crossing.Kind == NeighbourhoodFeatureKind::Crossing &&
                            person.Position.X >= crossing.Bounds.X0 && person.Position.X <= crossing.Bounds.X1 &&
                            person.Position.Y >= crossing.Bounds.Y0 && person.Position.Y <= crossing.Bounds.Y1;
                    }), "carriageway traversal only at crossings");
            }
        }
        R75Expect(students == (peak ? 27u : 0u), "students absent outside school commute and on holidays");
        R75Expect(full <= 36 && instanced <= 80, "default actor and far caps");
        if (peak)
            R75Expect(nearSchool >= 25 && nearSchool <= 35, "25..35 concentrated near school");
    }
    R75Expect(bands.size() == (school ? 6u : 4u), "every time band covered");
    for (const auto activity : {CrowdActivity::Walking, CrowdActivity::WaitingAtBusStop,
         CrowdActivity::ChattingAtTeaStall, CrowdActivity::SittingOnBench, CrowdActivity::Playing, CrowdActivity::TendingPitch})
        R75Expect(activities.contains(activity), "cohort activity represented");
}
void R75Reconstruction(const AmbientCrowd& crowd, const NeighbourhoodPlan& plan, const SchoolCalendar& calendar)
{
    const AmbientCrowd rebuilt(7505, std::string(NeighbourhoodId), plan, calendar);
    const AmbientCrowd anotherSeed(7506, std::string(NeighbourhoodId), plan, calendar);
    const AmbientCrowd anotherDistrict(7505, "district.other", plan, calendar);
    const NeighbourhoodPoint centre{6500, 4700};
    CrowdSnapshot stepped;
    for (int minute = 0; minute <= 1100; ++minute)
    {
        const auto at = R75At(minute);
        stepped = crowd.CrowdAt(at, centre, 10000);
        if (minute % 113 == 0) R75Expect(stepped == rebuilt.CrowdAt(at, centre, 10000), "rebuild determinism");
    }
    R75Expect(stepped == rebuilt.CrowdAt(R75At(1100), centre, 10000), "direct evaluation equals stepping to T");
    R75Expect(stepped != anotherSeed.CrowdAt(R75At(1100), centre, 10000), "seed affects generation");
    R75Expect(stepped != anotherDistrict.CrowdAt(R75At(1100), centre, 10000), "district affects generation");
    R75Expect(stepped != crowd.CrowdAt(R75At(1100, 1), centre, 10000), "date affects identity and appearance");
    const auto before = crowd.CrowdAt(R75At(420), centre, 10000);
    const auto after = crowd.CrowdAt(R75At(421), centre, 10000);
    for (const auto& person : before.People)
    {
        const auto* next = R75Person(after, person.Id);
        R75Expect(next && next->AppearanceSeed == person.AppearanceSeed && next->Outfit == person.Outfit,
                  "daily slot appearance remains stable across query order and time");
    }
    // Subminute positions stay continuous across movement, waiting and cycle boundaries.
    for (int minute : {0, 301, 404, 405, 406, 407, 420, 448, 449, 809, 810, 868, 869, 1020, 1199, 1439})
    {
        auto previous = crowd.CrowdAt(R75At(minute), centre, 100000, {}, 0);
        for (int fraction = 1; fraction <= 1000; ++fraction)
        {
            const auto current = crowd.CrowdAt(R75At(minute+fraction/1000), centre, 100000, {}, fraction%1000);
            for (const auto& person : current.People)
                if (const auto* prior = R75Person(previous, person.Id))
                    R75Expect(R75Distance(person.Position, prior->Position) <= 3, "continuous fractional movement without teleports");
            previous = current;
        }
    }
}
void R75Caps(const AmbientCrowd& crowd)
{
    const NeighbourhoodPoint centre{6500, 4700};
    for (const auto budget : {CrowdPresentationBudget{}, CrowdPresentationBudget{100, 20, 100},
                              CrowdPresentationBudget{0, 0, 5}, CrowdPresentationBudget{100, 99, 100}})
    {
        const auto snapshot = crowd.CrowdAt(R75At(420), centre, 10000, budget);
        std::size_t full = 0, far = 0;
        for (const auto& person : snapshot.People)
        {
            full += person.Lod == CrowdLod::FullActor; far += person.Lod == CrowdLod::InstancedFigure;
            const auto dx = std::int64_t(person.Position.X)-centre.X, dy = std::int64_t(person.Position.Y)-centre.Y;
            R75Expect(person.ReceivesAvoidanceAndInteraction == (person.Lod == CrowdLod::FullActor &&
                dx*dx+dy*dy <= 2500LL*2500), "only actors within 25 m get avoidance/interaction");
        }
        R75Expect(full == std::min({snapshot.People.size(), budget.AmbientActorLimit,
            48-std::min(budget.NamedActorCount, std::size_t(48))}), "named actor room reserved");
        R75Expect(far <= std::min(budget.FarFigureLimit, std::size_t(80)), "far cap clamped");
    }
    R75Expect(crowd.CrowdAt(R75At(420), centre, -1).People.empty(), "negative radius refused");
    R75Expect(crowd.CrowdAt(R75At(420), centre, 10000, {}, 1000).People.empty(), "invalid fractional minute refused");
    R75Expect(crowd.CrowdAt(R75At(420), {std::numeric_limits<int>::min(), std::numeric_limits<int>::min()},
        std::numeric_limits<int>::max()).People.empty(), "radius arithmetic does not overflow");
    const auto all = crowd.CrowdAt(R75At(420), centre, 10000);
    const auto point = crowd.CrowdAt(R75At(420), all.People.front().Position, 0);
    R75Expect(!point.People.empty(), "inclusive radius zero");
}
void R75Events(const AmbientCrowd& crowd, const NeighbourhoodPlan& plan)
{
    const auto all = crowd.EntranceEvents(R75At(-1), R75At(2880));
    R75Expect(all.Complete && all.Error.empty() && all.Through == R75At(2880), "multi-day event enumeration");
    auto cursor = R75At(-1);
    std::vector<CrowdEntranceEvent> paged;
    while (cursor < R75At(2880))
    {
        const auto page = crowd.EntranceEvents(cursor, R75At(2880), 17);
        R75Expect(page.Error.empty() && page.Through > cursor, "event pagination makes progress");
        paged.insert(paged.end(), page.Events.begin(), page.Events.end());
        cursor = page.Through;
    }
    R75Expect(paged == all.Events, "event pagination never drops or repeats a boundary");
    GameInstant previous = R75At(-1);
    std::set<std::string> unique;
    for (const auto& event : all.Events)
    {
        R75Expect(event.At >= previous, "events chronological"); previous = event.At;
        R75Expect(unique.insert(event.PersonId+":"+std::to_string(static_cast<int>(event.Kind))).second,
                  "one enter and exit per daily slot");
        const auto* entrance = FindNeighbourhoodFeature(plan, event.EntranceId);
        R75Expect(entrance && (entrance->Kind == NeighbourhoodFeatureKind::Entrance ||
            entrance->Kind == NeighbourhoodFeatureKind::PopulationEntrance || entrance->Kind == NeighbourhoodFeatureKind::BusStop),
            "events only at credible entrances");
        R75Expect(event.Position == FindNeighbourhoodNode(plan, entrance->LinkNodeId)->Position, "event anchor position");
        const auto snapshot = crowd.CrowdAt(event.At, event.Position, 100000);
        const auto* person = R75Person(snapshot, event.PersonId);
        if (event.Kind == CrowdEntranceEventKind::Enter)
            R75Expect(person && person->Position == event.Position, "enter begins at entrance");
        else
        {
            R75Expect(person == nullptr, "exit removes slot at boundary");
            const auto last = crowd.CrowdAt(event.At-1, event.Position, 100000, {}, 999);
            const auto* prior = R75Person(last, event.PersonId);
            R75Expect(prior && R75Distance(prior->Position, event.Position) <= 3, "exit ends continuously at entrance");
        }
    }
    R75Expect(crowd.EntranceEvents(10, 9).Error.size() > 0, "reverse interval refused");
    R75Expect(crowd.EntranceEvents(10, 10).Complete, "empty interval complete");
    R75Expect(!crowd.EntranceEvents(0, 1440, 0).Complete, "zero budget defers enumeration");
    std::set<std::string> active;
    std::size_t eventIndex = 0;
    for (int minute = 0; minute <= 2880; ++minute)
    {
        const auto at = R75At(minute);
        while (eventIndex < all.Events.size() && all.Events[eventIndex].At <= at)
        {
            const auto& event = all.Events[eventIndex++];
            if (event.Kind == CrowdEntranceEventKind::Enter) active.insert(event.PersonId);
            else active.erase(event.PersonId);
        }
        std::set<std::string> direct;
        for (const auto& person : crowd.CrowdAt(at, {6500,4700}, 100000).People) direct.insert(person.Id);
        R75Expect(active == direct, "event-stepped roster equals direct reconstruction at " + std::to_string(at));
    }
    const auto holidayStart = CalendarDayStartInstant(*CalendarDayFromDate(2042, 8, 15));
    const auto holiday = crowd.EntranceEvents(holidayStart-1, holidayStart+1439);
    R75Expect(holiday.Complete && holiday.Events.size() < all.Events.size()/2, "holiday emits fewer entrances");
}
} // namespace

int main(int argc, char** argv)
{
    R75Expect(argc == 2, "calendar path argument");
    const auto calendar = R75Calendar(argv[1]);
    const auto plan = BuildNeighbourhoodPlan(7505);
    const AmbientCrowd crowd(7505, std::string(NeighbourhoodId), plan, calendar);
    for (const auto& problem : crowd.Problems()) std::cerr << problem << '\n';
    R75Expect(crowd.Problems().empty(), "crowd inputs and public routes valid");
    R75Routes(crowd, plan);
    R75Day(crowd, plan, 0, true);
    const auto holiday = CalendarDayFromDate(2042, 8, 15);
    R75Expect(holiday && !CalendarIsSchoolDay(calendar, *holiday), "authoritative Independence Day closure");
    R75Day(crowd, plan, *holiday, false);
    for (const auto date : {CalendarDay(5), CalendarDay(6), *CalendarDayFromDate(2042, 12, 26), CalendarDay(1000)})
        for (int minute : {420, 840})
        {
            const auto snapshot = crowd.CrowdAt(CalendarDayStartInstant(date)+minute, {6500,4700}, 10000);
            R75Expect(std::none_of(snapshot.People.begin(), snapshot.People.end(), [](const auto& person) {
                return person.Cohort == CrowdCohort::Student; }), "weekend/break/outside-year suppress students");
        }
    R75Reconstruction(crowd, plan, calendar);
    R75Caps(crowd);
    R75Events(crowd, plan);
    for (auto seed : {std::uint64_t(0), std::uint64_t(71), std::numeric_limits<std::uint64_t>::max()})
    {
        const AmbientCrowd other(seed, std::string(NeighbourhoodId), plan, calendar);
        R75Expect(other.Problems().empty(), "seed extremes have valid routes");
        for (int minute : {360, 405, 420, 449, 450, 480, 810, 840, 869, 900, 1020, 1199, 1230, 1260})
        {
            const auto snapshot = other.CrowdAt(R75At(minute), {6500,4700}, 10000);
            R75Expect(snapshot.People.size() >= static_cast<std::size_t>(snapshot.Target.Minimum) &&
                snapshot.People.size() <= static_cast<std::size_t>(snapshot.Target.Maximum), "density survives extreme seeds");
            for (const auto& person : snapshot.People)
                R75Expect(person.SpeedCmPerGameMinute <= 130 && person.SpeedCmPerGameMinute >= 0,
                          "accelerated-clock walking speed remains plausible");
        }
    }
    auto broken = plan; broken.Links.clear();
    const AmbientCrowd disconnected(0, std::string(NeighbourhoodId), broken, calendar);
    R75Expect(!disconnected.Problems().empty() && disconnected.CrowdAt(420, {6500,4700}, 10000).People.empty(),
              "invalid graph cannot emit crowds");
    auto wrongCalendar = calendar; wrongCalendar.InstitutionId = "institution.other";
    const AmbientCrowd mismatched(0, std::string(NeighbourhoodId), plan, wrongCalendar);
    R75Expect(!mismatched.Problems().empty(), "wrong institution calendar refused");
    std::cout << "R75: " << R75Assertions << " assertions; " << crowd.RouteSlots().size() << " seeded route slots\n";
    return 0;
}
