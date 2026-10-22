#include "AmbientCrowd.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace PerfectWorld
{
namespace
{
std::uint64_t R75Mix(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
std::int64_t R75Length(NeighbourhoodPoint a, NeighbourhoodPoint b)
{
    const auto abs = [](std::int64_t v) { return v < 0 ? -v : v; };
    return abs(std::int64_t(a.X)-b.X)+abs(std::int64_t(a.Y)-b.Y)+abs(std::int64_t(a.Z)-b.Z);
}
std::uint64_t R75DistanceSquared(NeighbourhoodPoint a, NeighbourhoodPoint b)
{
    // Saturation preserves comparison even for hostile int-coordinate inputs.
    const auto square = [](std::int64_t v) { return std::uint64_t(v*v); };
    const auto dx = std::int64_t(a.X)-b.X, dy = std::int64_t(a.Y)-b.Y;
    if (dx > 2147483647 || dx < -2147483647 || dy > 2147483647 || dy < -2147483647)
        return std::numeric_limits<std::uint64_t>::max();
    return square(dx)+square(dy);
}
const NeighbourhoodLink* R75Link(const NeighbourhoodPlan& plan, const std::string& id)
{
    for (const auto& link : plan.Links) if (link.Id == id) return &link;
    return nullptr;
}
bool R75Entrance(const NeighbourhoodFeature* f)
{
    return f && (f->Kind == NeighbourhoodFeatureKind::PopulationEntrance ||
        f->Kind == NeighbourhoodFeatureKind::Entrance || f->Kind == NeighbourhoodFeatureKind::BusStop);
}
bool R75Expand(CrowdRouteSlot& slot, const NeighbourhoodPlan& plan)
{
    if (!slot.Route.Found || slot.Route.LengthCm <= 0) return false;
    std::int64_t total = 0;
    for (std::size_t i = 0; i < slot.Route.LinkIds.size(); ++i)
    {
        const auto* link = R75Link(plan, slot.Route.LinkIds[i]);
        if (!link || link->Kind == NeighbourhoodLinkKind::Interior || link->Kind == NeighbourhoodLinkKind::Stair)
            return false;
        auto points = link->Points;
        if (link->From != slot.Route.NodeIds[i]) std::reverse(points.begin(), points.end());
        const auto* from = FindNeighbourhoodNode(plan, slot.Route.NodeIds[i]);
        const auto* to = FindNeighbourhoodNode(plan, slot.Route.NodeIds[i+1]);
        if (!from || !to || points.empty() || points.front() != from->Position || points.back() != to->Position)
            return false;
        std::int64_t expanded = 0;
        for (std::size_t j = 1; j < points.size(); ++j)
        {
            const auto length = R75Length(points[j-1], points[j]);
            if (length > 0) slot.Segments.push_back({link->Id, points[j-1], points[j], length});
            expanded += length;
        }
        if (expanded != link->LengthCm) return false;
        total += expanded;
    }
    return !slot.Segments.empty() && total == slot.Route.LengthCm;
}
CrowdOutfit R75Outfit(CrowdCohort cohort, std::uint64_t seed)
{
    switch (cohort)
    {
    case CrowdCohort::Student: return (seed & 1) ? CrowdOutfit::WhiteShirtBlueSkirt : CrowdOutfit::WhiteShirtBlueTrousers;
    case CrowdCohort::OfficeCommuter: return CrowdOutfit::OfficeWear;
    case CrowdCohort::Vendor: return CrowdOutfit::VendorWear;
    case CrowdCohort::Child: return CrowdOutfit::PlayWear;
    default: return CrowdOutfit::EverydayWear;
    }
}
std::string R75Text(const CrowdRouteSlot& slot, std::uint64_t seed)
{
    if (slot.Cohort == CrowdCohort::Student)
        return slot.Arrival ? "ambient.exchange.heading_to_school" : "ambient.exchange.greeting";
    return seed % 3 == 0 ? "ambient.exchange.directions_school" :
        seed % 3 == 1 ? "ambient.exchange.directions_bus_stop" : "ambient.exchange.greeting";
}
int R75TravelMinutes(std::int64_t distance, int speed)
{
    return static_cast<int>((distance + speed - 1) / speed);
}
void R75Place(CrowdPerson& person, const CrowdRouteSlot& slot, std::int64_t distance, int direction)
{
    person.RouteDistanceCm = distance;
    for (std::size_t i = 0; i < slot.Segments.size(); ++i)
    {
        const auto& segment = slot.Segments[i];
        if (distance > segment.LengthCm && i+1 < slot.Segments.size())
        {
            distance -= segment.LengthCm;
            continue;
        }
        const auto interpolate = [&](int a, int b) {
            return static_cast<int>(a + (std::int64_t(b)-a)*distance/segment.LengthCm); };
        person.Position = {interpolate(segment.From.X, segment.To.X), interpolate(segment.From.Y, segment.To.Y),
                           interpolate(segment.From.Z, segment.To.Z)};
        person.Heading = {static_cast<int>((std::int64_t(segment.To.X)-segment.From.X)*1000/segment.LengthCm)*direction,
                          static_cast<int>((std::int64_t(segment.To.Y)-segment.From.Y)*1000/segment.LengthCm)*direction,
                          static_cast<int>((std::int64_t(segment.To.Z)-segment.From.Z)*1000/segment.LengthCm)*direction};
        person.LinkId = segment.LinkId;
        return;
    }
}
void R75Evaluate(CrowdPerson& person, const CrowdRouteSlot& slot, std::uint64_t seed, int elapsedMilli)
{
    // 90..130 cm per exploration second (one game minute). Route durations are whole game minutes;
    // speed returned below is the actual rounded-duration speed, not the requested speed.
    const int speed = 90 + static_cast<int>(seed % 41);
    const auto length = slot.Route.LengthCm;
    const int duration = (slot.EndMinute-slot.BeginMinute)*1000;
    std::int64_t distance = 0;
    int direction = 1;
    person.Activity = slot.RestActivity;
    if (slot.Loop)
    {
        const int leg = R75TravelMinutes(length, speed)*1000;
        const int pause = static_cast<int>(seed % 3000);
        const int movingUntil = (duration-pause)/(2*leg)*(2*leg)+pause;
        if (elapsedMilli >= pause && elapsedMilli < movingUntil)
        {
            const int phase = (elapsedMilli-pause) % (2*leg);
            direction = phase < leg ? 1 : -1;
            distance = direction == 1 ? length*phase/leg : length*(2*leg-phase)/leg;
            person.SpeedCmPerGameMinute = static_cast<int>(length*1000/leg);
            person.Activity = CrowdActivity::Walking;
        }
        else person.Activity = CrowdActivity::Standing;
    }
    else
    {
        // Seeded waiting spots remain on the authored graph. Students spread over
        // the forecourt/classroom path instead of stacking at one gate point.
        std::int64_t hold = length;
        if (slot.Cohort == CrowdCohort::Student)
        {
            const auto spread = std::min<std::int64_t>(length-1, 150 + static_cast<int>(seed % 2251));
            hold = slot.Arrival ? length-spread : spread;
        }
        else hold -= std::min<std::int64_t>(length-1, static_cast<int>(seed % (slot.Cohort == CrowdCohort::Elder ? 601 : 201)));
        const int incoming = R75TravelMinutes(hold, speed)*1000;
        const auto remaining = slot.EnterEntranceId == slot.ExitEntranceId ? hold : length-hold;
        const int outgoing = R75TravelMinutes(remaining, speed)*1000;
        distance = hold;
        if (elapsedMilli < incoming)
        {
            distance = hold*elapsedMilli/incoming;
            person.SpeedCmPerGameMinute = static_cast<int>(hold*1000/incoming);
            person.Activity = CrowdActivity::Walking;
        }
        else if (outgoing > 0 && elapsedMilli >= duration-outgoing)
        {
            direction = slot.EnterEntranceId == slot.ExitEntranceId ? -1 : 1;
            distance = hold + direction*remaining*(elapsedMilli-(duration-outgoing))/outgoing;
            person.SpeedCmPerGameMinute = static_cast<int>(remaining*1000/outgoing);
            person.Activity = CrowdActivity::Walking;
        }
    }
    R75Place(person, slot, distance, direction);
}
} // namespace

CrowdSchoolTiming CrowdSchoolTimingFromBell(const BellSchedule& schedule)
{
    CrowdSchoolTiming timing;
    if (schedule.Bells.empty()) return timing;
    timing.ArrivalEndMinute = schedule.Bells.front();
    timing.ArrivalBeginMinute = schedule.GatesOpenMinute - 15;
    timing.DepartureBeginMinute = schedule.Bells.back();
    timing.DepartureEndMinute = schedule.Bells.back() + 40;
    return timing;
}

AmbientCrowd::AmbientCrowd(std::uint64_t worldSeed, std::string district, const NeighbourhoodPlan& plan,
                         const SchoolCalendar& calendar, CrowdSchoolTiming timing)
    : Seed_(worldSeed), Timing_(timing), District_(std::move(district)), Plan_(plan), Calendar_(calendar)
{
    // Validate the consumed routes below. Whole-plan content diagnostics belong to
    // its owner and must not suppress crowds because of an unrelated frontage record.
    Problems_ = ValidateSchoolCalendar(Calendar_);
    if (District_.empty()) Problems_.push_back("R75: district id is empty");
    if (Calendar_.InstitutionId != NeighbourhoodSchoolInstitutionId)
        Problems_.push_back("R75: calendar institution does not match school anchors");
    if (Timing_.ArrivalBeginMinute < 405 || Timing_.ArrivalBeginMinute >= Timing_.ArrivalEndMinute ||
        Timing_.ArrivalEndMinute > Timing_.DepartureBeginMinute ||
        Timing_.DepartureBeginMinute >= Timing_.DepartureEndMinute || Timing_.DepartureEndMinute > 1020)
        Problems_.push_back("R75: school commute windows must satisfy 06:45 <= arrival < departure <= 17:00");
    if (!Problems_.empty()) return;
    const std::string lane = "population.nabapara.lane_north_3340";
    const std::string tea = "entrance.nabapara.tea";
    const std::string vendor = "entrance.nabapara.vendor";
    const std::string bus = "stop.nabapara.bus_west";
    const std::string club = "entrance.nabapara.club_entrance";
    const std::string gate(NeighbourhoodSchoolGateId);
    const std::string classroom = "entrance.nabapara.school_classroom";
    const auto add = [&](int count, CrowdCohort cohort, int begin, int end, const std::string& enter,
                         const std::string& destination, const std::string& exit, CrowdActivity rest,
                         bool loop = false, bool school = false, bool arrival = false) {
        const auto* source = FindNeighbourhoodFeature(Plan_, enter);
        const auto* finish = FindNeighbourhoodFeature(Plan_, exit);
        if (!R75Entrance(source) || !R75Entrance(finish))
        {
            Problems_.push_back("R75: route entrance missing: " + enter + " -> " + exit);
            return;
        }
        CrowdRouteSlot slot;
        slot.Cohort = cohort; slot.BeginMinute = begin; slot.EndMinute = end;
        slot.SchoolOnly = school; slot.Arrival = arrival; slot.Loop = loop; slot.RestActivity = rest;
        slot.EnterEntranceId = enter; slot.ExitEntranceId = exit;
        slot.Route = RouteNeighbourhood(Plan_, source->LinkNodeId, destination);
        if (!R75Expand(slot, Plan_) || (enter != exit && slot.Route.NodeIds.back() != finish->LinkNodeId))
        {
            Problems_.push_back("R75: connected public route missing: " + enter + " -> " + destination);
            return;
        }
        for (int i = 0; i < count; ++i)
        {
            slot.Slot = static_cast<std::uint32_t>(Slots_.size());
            Slots_.push_back(slot);
        }
    };
    using C = CrowdCohort;
    using A = CrowdActivity;
    add(8, C::PasserBy, 0, 1440, lane, "walk.nabapara.tea_path", lane, A::Walking, true);
    add(8, C::Elder, 300, 405, club, "walk.nabapara.club_benches", club, A::Walking, true);
    add(4, C::Vendor, 405, 1260, vendor, "walk.nabapara.vendor_path", vendor, A::TendingPitch);
    add(12, C::PasserBy, 405, 1020, tea, "walk.nabapara.tea_path", tea, A::ChattingAtTeaStall);
    add(8, C::OfficeCommuter, 405, 480, bus, "walk.nabapara.main_cross_west", bus, A::WaitingAtBusStop);
    add(8, C::PasserBy, 480, 870, tea, "walk.nabapara.rickshaw_auto_path", tea, A::Walking, true);
    // Student commute follows the authored bell schedule (default: gates 09:30, bell 10:00, dismissal 15:50).
    add(27, C::Student, Timing_.ArrivalBeginMinute, Timing_.ArrivalEndMinute, gate, "walk.nabapara.school_classroom",
        classroom, A::WaitingForSchool, false, true, true);
    add(27, C::Student, Timing_.DepartureBeginMinute, Timing_.DepartureEndMinute, classroom, std::string(NeighbourhoodSchoolGateNodeId), gate, A::WaitingForSchool, false, true);
    add(8, C::Child, 870, 1020, club, "walk.nabapara.pandal", club, A::Playing);
    add(24, C::OfficeCommuter, 1020, 1200, bus, "walk.nabapara.main_cross_west", bus, A::WaitingAtBusStop);
    add(12, C::Elder, 1020, 1200, club, "walk.nabapara.club_benches", club, A::SittingOnBench);
    add(20, C::PasserBy, 1200, 1260, tea, "walk.nabapara.tea_path", tea, A::ChattingAtTeaStall);
}

std::uint64_t AmbientCrowd::DailySeed(CalendarDay day, std::uint32_t slot) const
{
    auto value = R75Mix(Seed_);
    for (unsigned char c : District_) value = R75Mix(value ^ c);
    return R75Mix(R75Mix(value ^ static_cast<std::uint64_t>(day)) ^ slot);
}
std::string AmbientCrowd::PersonId(CalendarDay day, std::uint32_t slot) const
{
    return "ambient." + District_ + "." + std::to_string(Seed_) + "." + std::to_string(day) + "." + std::to_string(slot);
}
CrowdDensityTarget AmbientCrowd::DensityAt(GameInstant now) const
{
    CrowdDensityTarget result;
    const auto civil = GameClock::ToCalendar(now);
    const int minute = civil.Hour*60+civil.Minute;
    const bool school = CalendarIsSchoolDayAt(Calendar_, now);
    if (minute < 300 || minute >= 1260) return result;
    if (minute < 405) { result.Band = CrowdTimeBand::EarlyMorning; result.Minimum = 12; result.Maximum = 20; }
    else if (school && ((minute >= Timing_.ArrivalBeginMinute && minute < Timing_.ArrivalEndMinute) ||
                        (minute >= Timing_.DepartureBeginMinute && minute < Timing_.DepartureEndMinute)))
    {
        result.Band = minute < Timing_.ArrivalEndMinute ? CrowdTimeBand::SchoolArrival : CrowdTimeBand::SchoolDeparture;
        result.Minimum = 55; result.Maximum = 75; result.NearSchoolMinimum = 25; result.NearSchoolMaximum = 35;
    }
    else if (minute >= 1020 && minute < 1200)
    { result.Band = CrowdTimeBand::Evening; result.Minimum = 40; result.Maximum = 55; }
    else { result.Band = CrowdTimeBand::Daytime; result.Minimum = 25; result.Maximum = 40; }
    return result;
}

CrowdSnapshot AmbientCrowd::CrowdAt(GameInstant now, NeighbourhoodPoint centre, int radiusCm,
                                 CrowdPresentationBudget budget, int minuteFractionMilli) const
{
    CrowdSnapshot result;
    result.Target = DensityAt(now);
    if (!Problems_.empty() || radiusCm < 0 || minuteFractionMilli < 0 || minuteFractionMilli >= 1000) return result;
    const auto day = CalendarDayOfInstant(now);
    const auto civil = GameClock::ToCalendar(now);
    const int minute = civil.Hour*60+civil.Minute;
    const bool school = CalendarIsSchoolDay(Calendar_, day);
    const auto radiusSquared = std::uint64_t(radiusCm)*radiusCm;
    for (const auto& slot : Slots_)
    {
        if (minute < slot.BeginMinute || minute >= slot.EndMinute || (slot.SchoolOnly && !school)) continue;
        CrowdPerson person;
        person.Id = PersonId(day, slot.Slot); person.Slot = slot.Slot; person.Cohort = slot.Cohort;
        person.AppearanceSeed = DailySeed(day, slot.Slot);
        person.Outfit = R75Outfit(slot.Cohort, person.AppearanceSeed);
        person.ShortExchangeTextKey = R75Text(slot, person.AppearanceSeed);
        R75Evaluate(person, slot, person.AppearanceSeed, (minute-slot.BeginMinute)*1000+minuteFractionMilli);
        if (R75DistanceSquared(centre, person.Position) <= radiusSquared) result.People.push_back(std::move(person));
    }
    std::stable_sort(result.People.begin(), result.People.end(), [&](const auto& a, const auto& b) {
        return R75DistanceSquared(centre, a.Position) < R75DistanceSquared(centre, b.Position); });
    const auto actors = std::min({budget.AmbientActorLimit, std::size_t(48), 48-std::min(budget.NamedActorCount, std::size_t(48))});
    const auto far = std::min(budget.FarFigureLimit, std::size_t(80));
    for (std::size_t i = 0; i < result.People.size(); ++i)
    {
        auto& person = result.People[i];
        person.Lod = i < actors ? CrowdLod::FullActor : i-actors < far ? CrowdLod::InstancedFigure : CrowdLod::Hidden;
        person.ReceivesAvoidanceAndInteraction = person.Lod == CrowdLod::FullActor &&
            R75DistanceSquared(centre, person.Position) <= 2500ULL*2500;
    }
    return result;
}

CrowdEntranceBatch AmbientCrowd::EntranceEvents(GameInstant from, GameInstant to, std::size_t maxEvents) const
{
    CrowdEntranceBatch result;
    result.Through = from;
    if (to < from || !Problems_.empty()) { result.Error = "R75: invalid interval or crowd inputs"; return result; }
    if (to == from) { result.Complete = true; return result; }
    const auto first = CalendarDayOfInstant(from), last = CalendarDayOfInstant(to);
    if (last-first > MaxSingleAdvanceMinutes/MinutesPerDay)
    { result.Error = "R75: event interval exceeds ten years"; return result; }
    if (maxEvents == 0) return result;
    maxEvents = std::min(maxEvents, std::size_t(16384));
    for (auto day = first; day <= last; ++day)
    {
        std::vector<CrowdEntranceEvent> events;
        const bool school = CalendarIsSchoolDay(Calendar_, day);
        for (const auto& slot : Slots_)
        {
            if (slot.SchoolOnly && !school) continue;
            for (int edge = 0; edge < 2; ++edge)
            {
                const auto at = CalendarDayStartInstant(day) + (edge == 0 ? slot.BeginMinute : slot.EndMinute);
                if (at <= from || at > to || (edge == 1 && slot.EndMinute == 1440)) continue;
                const auto& id = edge == 0 ? slot.EnterEntranceId : slot.ExitEntranceId;
                const auto* entrance = FindNeighbourhoodFeature(Plan_, id);
                const auto* node = FindNeighbourhoodNode(Plan_, entrance->LinkNodeId);
                events.push_back({at, PersonId(day, slot.Slot), id, node->Position,
                                  edge == 0 ? CrowdEntranceEventKind::Enter : CrowdEntranceEventKind::Exit});
            }
        }
        // Midnight exits from the preceding day belong to this timestamp group too.
        for (const auto& slot : Slots_) if (slot.EndMinute == 1440)
        {
            const auto at = CalendarDayStartInstant(day);
            if (at > from && at <= to)
            {
                const auto* entrance = FindNeighbourhoodFeature(Plan_, slot.ExitEntranceId);
                events.push_back({at, PersonId(day-1, slot.Slot), slot.ExitEntranceId,
                    FindNeighbourhoodNode(Plan_, entrance->LinkNodeId)->Position, CrowdEntranceEventKind::Exit});
            }
        }
        std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
            if (a.At != b.At) return a.At < b.At;
            if (a.Kind != b.Kind) return a.Kind == CrowdEntranceEventKind::Exit;
            return a.PersonId < b.PersonId;
        });
        for (std::size_t i = 0; i < events.size();)
        {
            auto end = i+1;
            while (end < events.size() && events[end].At == events[i].At) ++end;
            if (!result.Events.empty() && result.Events.size()+end-i > maxEvents)
            { result.Through = events[i].At-1; return result; }
            result.Events.insert(result.Events.end(), events.begin()+i, events.begin()+end);
            i = end;
        }
    }
    result.Through = to; result.Complete = true;
    return result;
}
} // namespace PerfectWorld
