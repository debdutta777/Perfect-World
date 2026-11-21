#pragma once

#include "LaneInteriorLayout.h"

#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld::LaneStair
{
/**
 * Stair contract for the lane's multi-storey houses (UP-05). Portable and deterministic: from a footprint, a pair of finished-floor
 * datums, the upper slab and a body/scale record it produces solid treads, landings, slab openings, protective guards and headroom
 * clearance volumes, or a refusal. It never repairs and never places a stair that cannot fit safely. Unreal only emits block
 * visuals and collision from the result; a stair is never produced by hand.
 *
 * Frame. The building frame of LaneInteriorLayout.h: whole centimetres, U along the frontage, V from the front face into the
 * building. HEIGHTS are millimetres above the LOWER finished floor (0 = lower floor, RiseMm = upper floor), so risers that cannot
 * be whole centimetres still land exactly. The caller adds LowerFloorCm and its own world frame.
 *
 * Risers. N equal risers (differing by at most 1 mm) climb the rise exactly; the last one lands on the upper floor. A flight of n
 * risers has n - 1 treads, the n-th "tread" being the landing (the upper floor, or the mid landing of a switchback).
 *
 * Scale. Every body figure is an INPUT (StairScale), so the measured values of LL-05 can replace these defaults without code change.
 */
inline constexpr int GuardThicknessCm = 6;     // a stringer or rim guard is this thick
inline constexpr int SpineCm = 2 * GuardThicknessCm; // the gap between the two flights of a switchback holds one guard on each flight
inline constexpr int MinRiseCm = 250;          // supported floor-to-floor extremes
inline constexpr int MaxRiseCm = 450;
inline constexpr int StepUpMarginCm = 5;       // a riser must stay this far under the character's step-up height

/** The body and comfort figures the stair is derived from. Defaults are the fixed scale contract of SCALE_AUDIT.md. */
struct StairScale
{
    int BodyHeightCm = 165;       // PWPlayer::CapsuleHalfHeight * 2
    int EyeHeightCm = 155;        // PWPlayer::EyeHeightAboveFloor
    int CapsuleRadiusCm = 34;     // PWPlayer::CapsuleRadius
    int MaxStepUpCm = 45;         // PWPlayer::MaxStepHeight
    int WalkableSlopePermille = 965; // tan 44 degrees (UE default walkable floor angle 44.765, left unchanged)
    int TargetRiserMm = 175;      // reference riser 17.5 cm
    int MinRiserMm = 150;
    int MaxRiserMm = 190;
    int TreadCm = 25;             // reference tread 25 cm
    int HeadroomCm = 220;         // clear height above the nosing line
    int BarrierHeightCm = 100;    // railing / parapet reference
    int MinFlightWidthCm = 100;   // clear width between guards or walls
    int MaxFlightWidthCm = 160;
    int MinLandingCm = 100;       // approach, top landing and the depth of a turning landing
};

enum class Arrangement : std::uint8_t { Straight, Switchback };
/** The plan direction the stair climbs in. A switchback climbs this way first, then back the opposite way. */
enum class Climb : std::uint8_t { PlusV, MinusV, PlusU, MinusU };

/** Which footprint edges are walls built by the caller. A flight or opening edge on a wall edge needs no guard; elsewhere it gets one. */
struct Enclosure
{
    bool U0 = true;
    bool U1 = true;
    bool V0 = true;
    bool V1 = true;
};

enum class Refusal : std::uint8_t
{
    None,
    InvalidStairId, ScaleInvalid, FootprintInvalid, FloorDatumInvalid, RiseOutOfRange, HeadroomImpossible, NoValidRiser,
    FootprintTooNarrow, FootprintTooWide, FootprintTooShort,
    // Found by ValidateStairLayout on any layout.
    RiserUneven, RiserTooSteep, RisersDoNotLand, TreadTooShallow, FlightTooNarrow, GeometryOutsideFootprint, MassOverlap,
    ClearanceIntruded, SlabNotOpen, GuardMissing, GuardTooLow, DuplicateId, LayoutInconsistent
};

/** Stable machine-readable code ("footprint_too_short", ...). "ok" for None. */
[[nodiscard]] inline std::string_view RefusalCode(Refusal refusal) noexcept
{
    constexpr std::string_view codes[] = {
        "ok", "invalid_stair_id", "scale_invalid", "footprint_invalid", "floor_datum_invalid", "rise_out_of_range", "headroom_impossible",
        "no_valid_riser", "footprint_too_narrow", "footprint_too_wide", "footprint_too_short", "riser_uneven", "riser_too_steep",
        "risers_do_not_land", "tread_too_shallow", "flight_too_narrow", "geometry_outside_footprint", "mass_overlap", "clearance_intruded",
        "slab_not_open", "guard_missing", "guard_too_low", "duplicate_id", "layout_inconsistent"};
    const auto index = static_cast<size_t>(refusal);
    return index < std::size(codes) ? codes[index] : std::string_view("unknown");
}

struct StairInput
{
    std::string StairId;           // stable identity, e.g. "stair.nabapara.tall_flats.a"
    Arrangement Kind = Arrangement::Straight;
    Climb Direction = Climb::PlusV;
    bool MirrorU = false;          // reflect the whole arrangement across the footprint's U midline
    LaneInterior::Rect Footprint;  // everything the stair needs, approach and top landing included
    Enclosure Walls;
    int LowerFloorCm = 15;         // finished floors above the ground datum (the lane footpath is 15)
    int UpperFloorCm = 315;
    int UpperSlabCm = LaneInterior::SlabAndPlasterCm; // slab plus plaster under the upper finished floor
    StairScale Scale;
};

/** A solid tread: a mass from the lower floor (0) up to TopMm. Riser is its 1-based riser number in the whole climb. */
struct Step
{
    std::string Id;
    int Flight = 1;
    int Riser = 0;
    LaneInterior::Rect Plan;
    int TopMm = 0;
};

enum class LandingKind : std::uint8_t { Bottom, Mid, Top };
/** Bottom: the floor in front of the first riser. Top: the upper floor beside the last riser. Both are not solid. Mid is a solid mass. */
struct Landing
{
    std::string Id;
    LandingKind Kind = LandingKind::Bottom;
    LaneInterior::Rect Plan;
    int TopMm = 0;
    bool Solid = false;
};

struct Flight
{
    std::string Id;
    int Index = 1;
    LaneInterior::Rect Plan;       // the walked surface, treads only
    int Risers = 0;
    int DirU = 0;                  // unit climb direction in plan
    int DirV = 0;
    int FromMm = 0;                // surface height at the foot and at the head
    int ToMm = 0;
    int WidthCm = 0;
};

/** A hole in the upper slab (plan only; the slab is the footprint minus these). The edge facing the arrival floor is left unguarded. */
struct SlabOpening
{
    std::string Id;
    LaneInterior::Rect Plan;
    int ArriveU = 0;               // unit direction from the opening to the floor people step onto
    int ArriveV = 0;
};

enum class GuardKind : std::uint8_t { Stringer, Rim };
/** A protective barrier box: Stringer along a flight or landing edge, Rim on the upper slab around an opening. */
struct Guard
{
    std::string Id;
    GuardKind Kind = GuardKind::Stringer;
    LaneInterior::Rect Plan;
    int BaseMm = 0;
    int TopMm = 0;
};

enum class ClearKind : std::uint8_t { Approach, Strip, MidLanding, TopLanding };
/** A volume that must stay free of every solid and, above the slab underside, lie inside a slab opening. */
struct Clearance
{
    std::string Id;
    ClearKind Kind = ClearKind::Strip;
    LaneInterior::Rect Plan;
    int BottomMm = 0;
    int TopMm = 0;
};

struct StairLayout
{
    Refusal Status = Refusal::None;
    std::string StairId;
    Arrangement Kind = Arrangement::Straight;
    Climb Direction = Climb::PlusV;
    bool Mirrored = false;
    LaneInterior::Rect Footprint;
    Enclosure Walls;
    StairScale Scale;
    int LowerFloorCm = 0;
    int UpperFloorCm = 0;
    int RiseMm = 0;
    int SlabUndersideMm = 0;       // the upper slab's underside above the lower floor
    int RiserCount = 0;
    int RiserMmMin = 0;
    int RiserMmMax = 0;
    int FlightWidthCm = 0;
    std::vector<int> Levels;       // Levels[k] = surface height after k risers, k = 0..RiserCount; Levels.back() == RiseMm
    std::vector<Flight> Flights;
    std::vector<Step> Steps;       // in climbing order
    std::vector<Landing> Landings;
    std::vector<SlabOpening> Openings;
    std::vector<Guard> Guards;
    std::vector<Clearance> Clearances;
    [[nodiscard]] bool Ok() const noexcept { return Status == Refusal::None; }
};

/** Refuses invalid inputs with a code and an otherwise empty layout; never repairs. The result is a pure function of the input. */
[[nodiscard]] PERFECTWORLDCORE_API StairLayout GenerateStairLayout(const StairInput& input);

/** Re-checks every contract on any layout (risers, treads, landing exactness, guards, clearance, slab openings). */
[[nodiscard]] PERFECTWORLDCORE_API Refusal ValidateStairLayout(const StairLayout& layout);

/** Reflects every U coordinate (U -> sumU - U, so the footprint midline for sumU = U0 + U1, or the frontage for sumU = WidthCm). Ids and heights are unchanged. */
inline void ReflectStairU(StairLayout& layout, int sumU)
{
    const auto flip = [sumU](LaneInterior::Rect& r) { r = LaneInterior::Rect{sumU - r.U1, sumU - r.U0, r.V0, r.V1}; };
    flip(layout.Footprint);
    for (Flight& f : layout.Flights) { flip(f.Plan); f.DirU = -f.DirU; }
    for (Step& s : layout.Steps) { flip(s.Plan); }
    for (Landing& l : layout.Landings) { flip(l.Plan); }
    for (SlabOpening& o : layout.Openings) { flip(o.Plan); o.ArriveU = -o.ArriveU; }
    for (Guard& g : layout.Guards) { flip(g.Plan); }
    for (Clearance& c : layout.Clearances) { flip(c.Plan); }
    const bool u0 = layout.Walls.U0;
    layout.Walls.U0 = layout.Walls.U1;
    layout.Walls.U1 = u0;
    layout.Direction = layout.Direction == Climb::PlusU ? Climb::MinusU : layout.Direction == Climb::MinusU ? Climb::PlusU : layout.Direction;
    layout.Mirrored = !layout.Mirrored;
}

/** A canonical text dump of the whole layout, for determinism and stable-id checks. */
[[nodiscard]] PERFECTWORLDCORE_API std::string DescribeStairLayout(const StairLayout& layout);
} // namespace PerfectWorld::LaneStair
