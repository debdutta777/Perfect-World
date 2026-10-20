#pragma once

#include "Doors.h"
#include "LaneStairLayout.h"

namespace PerfectWorld::LaneTallFlats
{
inline constexpr std::string_view UpperDoorId = "door.nabapara.tall_flats_upper_a";
inline constexpr std::string_view UpperLocationId = "location.nabapara.tall_flats_upper_a";

struct Layout
{
    LaneInterior::InteriorLayout Ground;
    LaneInterior::InteriorLayout Upper;
    LaneStair::StairLayout Stair;
    // Whole-building slab tiles, excluding the contract's openings; absolute datums 289..315.
    std::vector<LaneInterior::Rect> Slab;
    [[nodiscard]] bool Ok() const noexcept { return Ground.Ok() && Upper.Ok() && Stair.Ok(); }
};

/** Production footprint only. Ground comes directly from the frozen frontage catalogue, without edits. */
[[nodiscard]] PERFECTWORLDCORE_API Layout GenerateLayout();
/** The registered door definition of the upper dwelling's side entrance (its resident is Ishita Banerjee). This function does not register
 * or hang a leaf: GenerateLayout populates the side opening with UpperDoorId and the presenter hangs it. */
[[nodiscard]] PERFECTWORLDCORE_API DoorDef PlannedUpperDoor() noexcept;
} // namespace PerfectWorld::LaneTallFlats
