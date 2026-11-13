#include "LaneInteriorDetail.h"

namespace PerfectWorld::LaneInterior::Detail
{
namespace
{
constexpr int LsPassageWidth = 130;
constexpr int LsMinUnitWidth = 200;
constexpr int LsHalfDoor = DoorOpeningWidthCm / 2;
constexpr int LsUnitDoorV = 110; // unit doorway centre, measured from the interior's front face

/** One dwelling beside the passage: its doorway wall run, circulation, furniture. Returns false if a required piece does not fit. */
bool LsBuildUnit(Builder& b, const Room& unit, const Room& passage, Run& wall, const std::string& suffix, bool leftUnit, bool furnish)
{
    const Rect in = b.Inner;
    const int vc = in.V0 + LsUnitDoorV;
    const std::string name = "unit" + suffix;
    const std::string openingId = b.Key("opening", name);
    // With a unit door id the doorway hangs a leaf (hinge on the low-V jamb, mid-partition). The builder places the sweep on the +U
    // side of the partition; the leaf swings into the dwelling, so the left unit's sweep is moved to its own side. SweepSign is the
    // sign of the swing along U for this V-axis wall (the kit presenter reads the sweep zone, which survives a mirror).
    const std::string& doorId = leftUnit ? b.In.UnitDoorIdA : b.In.UnitDoorIdB;
    const bool hung = !doorId.empty();
    b.AddOpening(wall, name, hung ? OpeningKind::Door : OpeningKind::Doorway, vc - LsHalfDoor, vc + LsHalfDoor, 0, DoorOpeningHeightCm,
                 leftUnit ? unit.Id : passage.Id, leftUnit ? passage.Id : unit.Id, doorId, true);
    if (hung)
    {
        wall.Open.back().SweepSign = leftUnit ? -1 : 1;
        if (leftUnit)
        {
            for (Zone& z : b.L.Zones)
            {
                if (z.Kind == ZoneKind::DoorSweep && z.OwnerId == openingId)
                {
                    z.Area = Rect{wall.P0 - DoorClearWidthCm, wall.P0, z.Area.V0, z.Area.V1};
                    z.RoomId = unit.Id;
                }
            }
        }
    }
    b.AddConnection(name, passage.Id, unit.Id, openingId, DoorClearWidthCm);
    const int u0 = leftUnit ? unit.Area.U1 - CirculationCm : unit.Area.U0;
    b.AddZone(ZoneKind::Circulation, unit.Id, Rect{u0, u0 + CirculationCm, vc - CirculationCm / 2, vc + CirculationCm / 2}, openingId,
              "approach" + suffix);
    if (!furnish)
    {
        return true;
    }
    const int refU = leftUnit ? unit.Area.U0 : unit.Area.U1;
    return PlaceBed(b, unit, suffix, refU, in.V1) && PlaceDiningSet(b, unit, suffix, refU, in.V0 + 260);
}
} // namespace

Refusal BuildSharedEntry(Builder& b)
{
    const InteriorInput& in = b.In;
    const Rect Inner = b.Inner;
    const int W = b.W;
    const int D = b.D;
    const int c = in.EntranceCentreU;
    const bool furnish = in.Furniture == FurnitureSet::Residence;

    // The passage is 130 wide around the entrance. A side with room for a dwelling (200 clear) gets a partition and a unit; a side
    // without folds into the passage, which then runs to the wall.
    const int ps0 = ClampInt(c - LsPassageWidth / 2, Inner.U0, Inner.U1 - LsPassageWidth);
    const int pe0 = ps0 + LsPassageWidth;
    const bool leftUnit = (ps0 - PartitionThicknessCm) - Inner.U0 >= LsMinUnitWidth;
    const bool rightUnit = Inner.U1 - (pe0 + PartitionThicknessCm) >= LsMinUnitWidth;
    if (!leftUnit && !rightUnit)
    {
        return Refusal::NoRoomForUnits;
    }
    const int ps = leftUnit ? ps0 : Inner.U0;
    const int pe = rightUnit ? pe0 : Inner.U1;

    const Room passage = b.AddRoom("passage", RoomKind::Passage, Rect{ps, pe, Inner.V0, Inner.V1}, "floor_tile", "plaster_pale");
    Room unitA;
    Room unitB;
    if (leftUnit)
    {
        unitA = b.AddRoom("unit_a", RoomKind::Dwelling, Rect{Inner.U0, ps - PartitionThicknessCm, Inner.V0, Inner.V1}, "floor_oxide", "plaster_warm");
    }
    if (rightUnit)
    {
        unitB = b.AddRoom("unit_b", RoomKind::Dwelling, Rect{pe + PartitionThicknessCm, Inner.U1, Inner.V0, Inner.V1}, "floor_oxide", "plaster_warm");
    }

    Run frontWall{"front", true, 0, WallThicknessCm, 0, W, {}};
    Run backWall{"back", true, D - WallThicknessCm, D, 0, W, {}};
    Run sideA{"side_a", false, 0, WallThicknessCm, Inner.V0, Inner.V1, {}};
    Run sideB{"side_b", false, W - WallThicknessCm, W, Inner.V0, Inner.V1, {}};
    Run partA{"partition_a", false, ps - PartitionThicknessCm, ps, Inner.V0, Inner.V1, {}};
    Run partB{"partition_b", false, pe, pe + PartitionThicknessCm, Inner.V0, Inner.V1, {}};

    const std::string entranceId = b.Key("opening", "entrance");
    b.AddOpening(frontWall, "entrance", in.EntranceDoorId.empty() ? OpeningKind::Doorway : OpeningKind::Door, c - LsHalfDoor, c + LsHalfDoor, 0,
                 DoorOpeningHeightCm, OutsideId, passage.Id, in.EntranceDoorId, true);
    b.AddConnection("entrance", OutsideId, passage.Id, entranceId, DoorClearWidthCm);
    b.AddZone(ZoneKind::Circulation, passage.Id, passage.Area, entranceId, "passage");

    if (leftUnit && !LsBuildUnit(b, unitA, passage, partA, "_a", true, furnish))
    {
        return Refusal::FurnitureDoesNotFit;
    }
    if (rightUnit && !LsBuildUnit(b, unitB, passage, partB, "_b", false, furnish))
    {
        return Refusal::FurnitureDoesNotFit;
    }

    if (leftUnit)
    {
        b.MaybeWindow(frontWall, "window_a", Inner.U0, ps - PartitionThicknessCm, 120, unitA.Id);
        b.MaybeWindow(backWall, "window_back_a", Inner.U0, ps - PartitionThicknessCm, 120, unitA.Id);
    }
    if (rightUnit)
    {
        b.MaybeWindow(frontWall, "window_b", pe + PartitionThicknessCm, Inner.U1, 120, unitB.Id);
        b.MaybeWindow(backWall, "window_back_b", pe + PartitionThicknessCm, Inner.U1, 120, unitB.Id);
    }
    b.EndRun(frontWall);
    b.EndRun(backWall);
    b.EndRun(sideA);
    b.EndRun(sideB);
    if (leftUnit)
    {
        b.EndRun(partA);
    }
    if (rightUnit)
    {
        b.EndRun(partB);
    }
    b.AddLights(passage);
    if (leftUnit)
    {
        b.AddLights(unitA);
    }
    if (rightUnit)
    {
        b.AddLights(unitB);
    }
    return Refusal::None;
}
} // namespace PerfectWorld::LaneInterior::Detail
