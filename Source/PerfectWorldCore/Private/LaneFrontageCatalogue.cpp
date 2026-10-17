#include "LaneFrontageCatalogue.h"

#include <array>

namespace PerfectWorld::LaneInterior
{
namespace
{
// Batch 1 (LL-03 submission 2). The doors are open doorways: a lane door leaf needs a door-set migration (Doors.cpp, old-save
// fixtures), which is a later integrator step. TODO(doors): hang door.nabapara.<name>_front on each row (PlannedDoorId).
const std::array<FrontageEntry, 3> Entries{{
    // -Y, row 5, X 2600 to 3300: the "neighbour / caretaker flat" of LANE_LIFE_PLAN. Compact residence; the stationery footprint on the other side.
    {"building.nabapara.caretaker_flat", -1, 5, 2600, 460, 700, 700, 60, 3, 1965316759u, Variant::CompactResidence, FurnitureSet::Residence, 250, false,
     "", "door.nabapara.caretaker_flat_front"},
    // +Y, row 1, X -1300 to -200: a five-storey block with a shared stair passage and a flat to each side.
    {"building.nabapara.tall_flats", 1, 1, -1300, 460, 1100, 800, 60, 5, 2008046423u, Variant::SharedEntry, FurnitureSet::Residence, 550, false,
     "", "door.nabapara.tall_flats_front"},
    // +Y, row 4, X 1400 to 2100, east of the tea shop: a compact residence with its entrance on the east side (mirrored).
    {"building.nabapara.tea_row_house", 1, 4, 1400, 520, 700, 800, 120, 4, 2168505253u, Variant::CompactResidence, FurnitureSet::Residence, 450, true,
     "", "door.nabapara.tea_row_house_front"},
}};

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
    return in;
}
} // namespace PerfectWorld::LaneInterior
