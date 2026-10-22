#pragma once

#include "GameClock.h"
#include "NeighbourhoodPlan.h"
#include "SchoolCalendar.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PerfectWorld
{
enum class CrowdCohort : std::uint8_t { Student, OfficeCommuter, Vendor, Child, Elder, PasserBy };
enum class CrowdOutfit : std::uint8_t
{
    WhiteShirtBlueTrousers, WhiteShirtBlueSkirt, OfficeWear, VendorWear, PlayWear, EverydayWear
};
enum class CrowdActivity : std::uint8_t
{
    Walking, WaitingAtBusStop, ChattingAtTeaStall, SittingOnBench, WaitingForSchool, TendingPitch, Playing, Standing
};
enum class CrowdLod : std::uint8_t { FullActor, InstancedFigure, Hidden };
enum class CrowdTimeBand : std::uint8_t { Night, EarlyMorning, SchoolArrival, SchoolDeparture, Daytime, Evening };
struct CrowdDensityTarget
{
    CrowdTimeBand Band = CrowdTimeBand::Night;
    int Minimum = 6, Maximum = 12;
    int NearSchoolMinimum = 0, NearSchoolMaximum = 0;
    /** Profiling catchment, not a promise of this count around every possible query centre. */
    NeighbourhoodPoint ReferenceCentre{6500, 4700, NeighbourhoodFootpathZCm};
    NeighbourhoodPoint NearSchoolCentre{6500, 4850, NeighbourhoodFootpathZCm};
    int ReferenceRadiusCm = 10000, NearSchoolRadiusCm = 3000;
    friend bool operator==(const CrowdDensityTarget&, const CrowdDensityTarget&) = default;
};
struct CrowdPresentationBudget
{
    std::size_t AmbientActorLimit = 36, NamedActorCount = 0, FarFigureLimit = 80;
};
struct CrowdRouteSegment
{
    std::string LinkId;
    NeighbourhoodPoint From, To;
    std::int64_t LengthCm = 0;
};
/** Immutable daily template. No identity, RNG cursor, actor state or save data lives here. */
struct CrowdRouteSlot
{
    std::uint32_t Slot = 0;
    CrowdCohort Cohort = CrowdCohort::PasserBy;
    int BeginMinute = 0, EndMinute = 0;
    bool SchoolOnly = false, Loop = false;
    CrowdActivity RestActivity = CrowdActivity::Walking;
    std::string EnterEntranceId, ExitEntranceId;
    NeighbourhoodRoute Route;
    std::vector<CrowdRouteSegment> Segments;
};
struct CrowdPerson
{
    std::string Id;
    std::uint32_t Slot = 0;
    CrowdCohort Cohort = CrowdCohort::PasserBy;
    std::uint64_t AppearanceSeed = 0;
    CrowdOutfit Outfit = CrowdOutfit::EverydayWear;
    NeighbourhoodPoint Position;
    /** Unit direction times 1000; Z is a vector component, not the ground datum. */
    NeighbourhoodPoint Heading{0, 0, 0};
    std::string LinkId;
    std::int64_t RouteDistanceCm = 0;
    /** At exploration's one game minute per real second, this is also cm per real second.
     * Other clock modes/time jumps must reconcile offscreen, not accelerate visible walkers.
     */
    int SpeedCmPerGameMinute = 0;
    CrowdActivity Activity = CrowdActivity::Walking;
    std::string ShortExchangeTextKey;
    CrowdLod Lod = CrowdLod::InstancedFigure;
    bool ReceivesAvoidanceAndInteraction = false;
    /** A consequential reference requires crystallisation outside this anonymous system. */
    static constexpr bool RecordsJournalFacts = false;
    friend bool operator==(const CrowdPerson&, const CrowdPerson&) = default;
};
struct CrowdSnapshot
{
    CrowdDensityTarget Target;
    /** All people in the XY radius, nearest first; ties use slot order. */
    std::vector<CrowdPerson> People;
    friend bool operator==(const CrowdSnapshot&, const CrowdSnapshot&) = default;
};
enum class CrowdEntranceEventKind : std::uint8_t { Enter, Exit };
struct CrowdEntranceEvent
{
    GameInstant At = 0;
    std::string PersonId, EntranceId;
    NeighbourhoodPoint Position;
    CrowdEntranceEventKind Kind = CrowdEntranceEventKind::Enter;
    friend bool operator==(const CrowdEntranceEvent&, const CrowdEntranceEvent&) = default;
};
struct CrowdEntranceBatch
{
    std::vector<CrowdEntranceEvent> Events;
    /** Resume with (Through, originalTo]. A timestamp group is never split. */
    GameInstant Through = 0;
    bool Complete = false;
    std::string Error;
};

class PERFECTWORLDCORE_API AmbientCrowd
{
public:
    /** Copy immutable inputs once. Calendar institution must match the spatial school contract. */
    AmbientCrowd(std::uint64_t worldSeed, std::string district, const NeighbourhoodPlan& plan,
                 const SchoolCalendar& calendar);
    [[nodiscard]] CrowdSnapshot CrowdAt(GameInstant now, NeighbourhoodPoint centre, int radiusCm,
        CrowdPresentationBudget budget = {}, int minuteFractionMilli = 0) const;
    [[nodiscard]] CrowdDensityTarget DensityAt(GameInstant now) const;
    /** (fromExclusive, toInclusive], bounded pages (one timestamp group may exceed maxEvents);
     * no spawn implied by crossing a query radius.
     * Presenter must defer entrance materialisation/removal until occluded; reconcile time jumps
     * offscreen. Interpolate the graph with minuteFractionMilli for continuous visible movement.
     */
    [[nodiscard]] CrowdEntranceBatch EntranceEvents(GameInstant fromExclusive, GameInstant toInclusive,
                                                   std::size_t maxEvents = 4096) const;
    [[nodiscard]] const std::vector<CrowdRouteSlot>& RouteSlots() const { return Slots_; }
    [[nodiscard]] const std::vector<std::string>& Problems() const { return Problems_; }
private:
    [[nodiscard]] std::uint64_t DailySeed(CalendarDay day, std::uint32_t slot) const;
    [[nodiscard]] std::string PersonId(CalendarDay day, std::uint32_t slot) const;
    std::uint64_t Seed_ = 0;
    std::string District_;
    NeighbourhoodPlan Plan_;
    SchoolCalendar Calendar_;
    std::vector<CrowdRouteSlot> Slots_;
    std::vector<std::string> Problems_;
};
} // namespace PerfectWorld
