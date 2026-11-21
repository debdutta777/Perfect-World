#pragma once

#include "GameClock.h"
#include "NeighbourhoodPlan.h"

#include <cstddef>
#include <span>

namespace PerfectWorld
{
inline constexpr std::size_t NeighbourhoodTrafficActorCap = 12;
inline constexpr int NeighbourhoodTrafficGapCm = 200;
inline constexpr int NeighbourhoodTrafficStepMs = 100;
inline constexpr int NeighbourhoodBusDwellMs = 12000;

/** Campaign minute plus [0,59999] milliseconds within that minute; sampled down to 100 ms.
 * This is transient traffic time, never an additional saved clock or persistent RNG stream.
 */
struct NeighbourhoodTrafficTime
{
    GameInstant Minute = 0;
    int Millisecond = 0;
    friend bool operator==(const NeighbourhoodTrafficTime&, const NeighbourhoodTrafficTime&) = default;
};
enum class NeighbourhoodVehicleKind : std::uint8_t { YellowTaxi, Bus, SharedAuto, CycleRickshaw, FictionalTram };
enum class NeighbourhoodVehicleState : std::uint8_t
{
    Moving, StoppedAtStopLine, DwellingAtBusStop, YieldingToPedestrians, Queued, HoldingJunction
};
enum class NeighbourhoodCrossingPhase : std::uint8_t { Unknown, Walk, Clearance, Drive };
struct NeighbourhoodVehicle
{
    std::string Id, LaneId;
    NeighbourhoodVehicleKind Kind = NeighbourhoodVehicleKind::YellowTaxi;
    NeighbourhoodPoint Position;
    int AlongLaneCm = 0; // Centre, measured from the directed lane's first point.
    int SpeedCmPerSecond = 0, LengthCm = 0, WidthCm = 0;
    NeighbourhoodVehicleState State = NeighbourhoodVehicleState::Moving;
    friend bool operator==(const NeighbourhoodVehicle&, const NeighbourhoodVehicle&) = default;
};
/** XY world-cm occupancy, including the pedestrian's body radius; no identity or save data.
 * Supply every footprint, including the player, for the WHOLE interval advanced this frame.
 */
struct NeighbourhoodPedestrianOccupancy
{
    NeighbourhoodRect Bounds;
};

/** Aggregate scheduled arrival headway per directed road lane: rush 07-10/17-20,
 * day 05-07/10-17/20-21, night 21-05. Returns 0 for excluded narrow streets/invalid time.
 * Arrivals have seeded jitter; full lanes safely omit an arrival rather than overlap.
 * A 23:45-00:15 service gap drains baseline traffic before daily reconstruction.
 */
[[nodiscard]] PERFECTWORLDCORE_API int NeighbourhoodTrafficHeadwayMs(
    NeighbourhoodStreetClass street, GameInstant minute);

/** Owns a COPY of the four main/para directed lanes and their crossing/stop geometry.
 * No narrow lane/cross-street traffic, turns, collisions that push people, save fields or IO.
 * The T-junction reservation covers the nearby main crossing and stops as well, preventing
 * a bus/crossing queue from blocking another approach. Three vehicles per lane, at most 12.
 * Invalid/incomplete spatial input produces no traffic. Stable output order is spawn order.
 */
class PERFECTWORLDCORE_API NeighbourhoodTraffic
{
public:
    NeighbourhoodTraffic(const NeighbourhoodPlan& plan, std::uint64_t worldSeed);
    /** Replays today's seeded baseline from midnight. No pedestrian history is saved.
     * Use only offscreen/on load/time jumps; live occupancy history is intentionally discarded.
     */
    void Reconstruct(NeighbourhoodTrafficTime now);
    /** Fixed-step advance; same-time queries do not move. Backward/day changes reconstruct.
     * Occupancy blocks swept vehicle bodies, not just endpoints. An externally introduced
     * footprint already under a vehicle holds it still; it cannot repair the supplied overlap.
     * Persistent obstruction can hold traffic indefinitely; liveness assumes it eventually clears.
     */
    void AdvanceTo(NeighbourhoodTrafficTime now, std::span<const NeighbourhoodPedestrianOccupancy> pedestrians = {});
    [[nodiscard]] std::vector<NeighbourhoodVehicle> VehiclesAt(NeighbourhoodTrafficTime now,
        NeighbourhoodPoint centre, int radiusCm, std::span<const NeighbourhoodPedestrianOccupancy> pedestrians = {});
    /** Inclusive circle/body intersection in XY; complete bodies, not just centres. */
    [[nodiscard]] std::vector<NeighbourhoodVehicle> VehiclesNear(NeighbourhoodPoint centre, int radiusCm) const;
    [[nodiscard]] NeighbourhoodCrossingPhase CrossingPhaseAt(std::string_view crossingId,
        NeighbourhoodTrafficTime now) const;
    [[nodiscard]] static constexpr std::size_t ActorCapHint() { return NeighbourhoodTrafficActorCap; }

private:
    struct Crossing
    {
        std::string Id;
        int BeginCm = 0, EndCm = 0, StopCm = 0;
    };
    struct Lane
    {
        std::string Id;
        NeighbourhoodPoint Start;
        int Dx = 0, Dy = 0, LengthCm = 0, WidthCm = 0;
        int JunctionBeginCm = 0, JunctionEndCm = 0, BusCm = -1;
        NeighbourhoodStreetClass Street = NeighbourhoodStreetClass::None;
        std::vector<Crossing> Crossings;
    };
    struct Slot
    {
        NeighbourhoodVehicle Vehicle;
        std::size_t LaneIndex = 0;
        int CruiseCmPerSecond = 0, DwellRemainingMs = 0;
        bool ServedBus = false;
    };
    void Step(std::int64_t day, int millisecond, std::span<const NeighbourhoodPedestrianOccupancy> pedestrians);
    void Spawn(std::int64_t day, int millisecond, std::span<const NeighbourhoodPedestrianOccupancy> pedestrians);
    std::uint64_t Seed_ = 0;
    std::vector<Lane> Lanes_;
    std::vector<std::string> CrossingIds_;
    std::vector<Slot> Slots_;
    Slot PendingArrival_; // One reverse-para request waits outside the simulated boundary.
    bool HasPendingArrival_ = false;
    std::string JunctionOwner_;
    std::size_t NextLane_ = 0;
    std::int64_t Day_ = -1;
    int Millisecond_ = -NeighbourhoodTrafficStepMs;
};
} // namespace PerfectWorld
