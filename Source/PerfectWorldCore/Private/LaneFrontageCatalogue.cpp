#include "LaneFrontageCatalogue.h"

#include <array>

namespace PerfectWorld::LaneInterior
{
namespace
{
// Batch 1 (LL-03 submission 2). NL-01 authored the front doors and their v8 migration; NL-04 hangs them: EntranceDoorId == PlannedDoorId.
// The tall flats also hang a leaf on each flat (UnitDoors below, applied by MakeFrontageInput).
const std::array<FrontageEntry, 3> Entries{{
    // -Y, row 5, X 2600 to 3300: the "neighbour / caretaker flat" of LANE_LIFE_PLAN. Compact residence; the stationery footprint on the other side.
    {"building.nabapara.caretaker_flat", -1, 5, 2600, 460, 700, 700, 60, 3, 1965316759u, Variant::CompactResidence, FurnitureSet::Residence, 250, false,
     "door.nabapara.caretaker_flat_front", "door.nabapara.caretaker_flat_front"},
    // +Y, row 1, X -1300 to -200: a five-storey block with a shared stair passage and a flat to each side.
    {"building.nabapara.tall_flats", 1, 1, -1300, 460, 1100, 800, 60, 5, 2008046423u, Variant::SharedEntry, FurnitureSet::Residence, 550, false,
     "door.nabapara.tall_flats_front", "door.nabapara.tall_flats_front"},
    // +Y, row 4, X 1400 to 2100, east of the tea shop: a compact residence with its entrance on the east side (mirrored).
    {"building.nabapara.tea_row_house", 1, 4, 1400, 520, 700, 800, 120, 4, 2168505253u, Variant::CompactResidence, FurnitureSet::Residence, 450, true,
     "door.nabapara.tea_row_house_front", "door.nabapara.tea_row_house_front"},
}};

/** The flat doors of a shared-entry block (NL-01 roster); other kit rows have none. */
struct FcUnitDoors
{
    std::string_view BuildingId;
    std::string_view DoorA;
    std::string_view DoorB;
};
const std::array<FcUnitDoors, 1> UnitDoors{{{"building.nabapara.tall_flats", "door.nabapara.tall_flats_unit_a", "door.nabapara.tall_flats_unit_b"}}};

const std::array<BuiltFootprint, 4> Built{{
    {"building.nabapara.care_home", -1, 1, -1400, -300},
    {"building.nabapara.opening_home", -1, 2, -300, 700},
    {"building.nabapara.stationery", 1, 2, -100, 600},
    {"building.nabapara.shanti_chai", 1, 3, 600, 1300},
}};
} // namespace

std::span<const FrontageEntry> KitFrontageEntries() noexcept
{
    return Entries;
}

std::span<const BuiltFootprint> BuiltFootprints() noexcept
{
    return Built;
}

const FrontageEntry* FindFrontageByFootprint(int side, int originXCm, int frontAbsYCm, int widthCm, int depthCm, int setbackCm, int storeys,
                                             std::uint32_t seed) noexcept
{
    for (const FrontageEntry& row : Entries)
    {
        if (row.Side == side && row.OriginXCm == originXCm && row.FrontAbsYCm == frontAbsYCm && row.WidthCm == widthCm && row.DepthCm == depthCm &&
            row.SetbackCm == setbackCm && row.Storeys == storeys && row.Seed == seed)
        {
            return &row;
        }
    }
    return nullptr;
}

const FrontageEntry* FindFrontageById(std::string_view buildingId) noexcept
{
    for (const FrontageEntry& row : Entries)
    {
        if (row.BuildingId == buildingId)
        {
            return &row;
        }
    }
    return nullptr;
}

InteriorInput MakeFrontageInput(const FrontageEntry& entry, int finishedFloorCm, int storeyHeightCm)
{
    InteriorInput in;
    in.BuildingId = std::string(entry.BuildingId);
    in.Kind = entry.Kind;
    in.Furniture = entry.Furniture;
    in.WidthCm = entry.WidthCm;
    in.DepthCm = entry.DepthCm;
    in.MirrorU = entry.MirrorU;
    in.FinishedFloorCm = finishedFloorCm;
    in.StoreyHeightCm = storeyHeightCm;
    in.EntranceCentreU = entry.EntranceCentreU;
    in.EntranceDoorId = std::string(entry.EntranceDoorId);
    for (const FcUnitDoors& row : UnitDoors)
    {
        if (row.BuildingId == entry.BuildingId)
        {
            in.UnitDoorIdA = std::string(row.DoorA);
            in.UnitDoorIdB = std::string(row.DoorB);
        }
    }
    return in;
}
} // namespace PerfectWorld::LaneInterior
