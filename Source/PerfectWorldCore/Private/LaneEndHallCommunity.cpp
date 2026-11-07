#include "LaneEndHallDetail.h"

#include <string>

namespace PerfectWorld::LaneEnds::Furnish
{
namespace
{
constexpr int ChBenchLength = 240;
constexpr int ChBenchDepth = 45;
constexpr int ChBenchHeight = 45;
constexpr int ChStoolSide = 36;

// Benches face the stage (+V). Rows are 130 apart: the seat approach (60 cm) sits behind each row. Columns leave a 120 centre aisle
// (U 1240 to 1360) on the hall's axis. Two blocks of two benches per row.
constexpr int ChRowV[3] = {215, 345, 475};
struct ChColumn
{
    const char* Name;
    int U0;
};
constexpr ChColumn ChColumns[4] = {{"l2", 700}, {"l1", 1000}, {"r1", 1360}, {"r2", 1660}};

bool ChBenches(Ld::Builder& b, const Li::Room& room)
{
    for (int row = 0; row < 3; ++row)
    {
        for (const ChColumn& column : ChColumns)
        {
            const std::string name = std::string("bench_r") + std::to_string(row + 1) + "_" + column.Name;
            const Li::Rect fp{column.U0, column.U0 + ChBenchLength, ChRowV[row], ChRowV[row] + ChBenchDepth};
            if (!Put(b, room, MakeItem(name, "asset.lane.furniture.bench_hall", "door_wood", Li::FurnitureKind::Chair, 0, ChBenchHeight), fp,
                     SeatUse(0, -1, ChBenchLength, "seat_" + name.substr(6))))
            {
                return false;
            }
        }
    }
    return true;
}

Ld::Item ChStool(const std::string& name)
{
    return MakeItem(name, "asset.lane.furniture.stool_wood", "door_wood", Li::FurnitureKind::Chair, 0, ChBenchHeight);
}

/** The carrom corner: a 90 x 90 table with the 74 cm board on it and four stools; two of them are sitting places with anchors. */
bool ChCarrom(Ld::Builder& b, const Li::Room& room)
{
    const Li::Rect table{160, 250, 520, 610};
    if (!Put(b, room, MakeItem("carrom_table", "asset.lane.furniture.table_wood", "wood_light", Li::FurnitureKind::Table, 0, 75), table) ||
        !Put(b, room, MakeItem("carrom_board", "asset.lane.furniture.carrom_board", "cloth_teal", Li::FurnitureKind::Display, 75, 78, false),
             Li::Rect{168, 242, 528, 602}))
    {
        return false;
    }
    return Put(b, room, ChStool("carrom_stool_w"), Li::Rect{100, 100 + ChStoolSide, 547, 547 + ChStoolSide}, SeatUse(-1, 0, ChStoolSide, "seat_carrom_w")) &&
           Put(b, room, ChStool("carrom_stool_e"), Li::Rect{274, 274 + ChStoolSide, 547, 547 + ChStoolSide}, SeatUse(1, 0, ChStoolSide, "seat_carrom_e")) &&
           Put(b, room, ChStool("carrom_stool_s"), Li::Rect{187, 187 + ChStoolSide, 460, 460 + ChStoolSide}) &&
           Put(b, room, ChStool("carrom_stool_n"), Li::Rect{187, 187 + ChStoolSide, 634, 634 + ChStoolSide});
}

/** The committee corner: a table with two chairs, a steel cupboard against the back wall and the water pot. */
bool ChCommittee(Ld::Builder& b, const Li::Room& room)
{
    const auto chair = [](const std::string& name) {
        return MakeItem(name, "asset.lane.furniture.chair_wood", "door_wood", Li::FurnitureKind::Chair, 0, 46);
    };
    return Put(b, room, MakeItem("committee_table", "asset.lane.furniture.table_wood", "wood_light", Li::FurnitureKind::Table, 0, 75),
               Li::Rect{2150, 2350, 690, 770}) &&
           Put(b, room, MakeItem("committee_papers", "asset.lane.furniture.papers", "trim_light", Li::FurnitureKind::Display, 75, 78, false),
               Li::Rect{2190, 2310, 705, 755}) &&
           Put(b, room, chair("committee_chair_a"), Li::Rect{2180, 2220, 640, 680}, SeatUse(0, -1, 40, "seat_committee_a")) &&
           Put(b, room, chair("committee_chair_b"), Li::Rect{2280, 2320, 640, 680}, SeatUse(0, -1, 40, "seat_committee_b")) &&
           Put(b, room, MakeItem("cupboard", "asset.lane.furniture.cupboard_steel", "shutter_steel", Li::FurnitureKind::Shelf, 0, 180),
               Li::Rect{2450, 2540, 830, 875}, StandUse(0, -1, Li::StandZoneCm, 90, "cupboard")) &&
           Put(b, room, MakeItem("water_pot", "asset.lane.furniture.water_pot", "pot_terracotta", Li::FurnitureKind::Trunk, 0, 85),
               Li::Rect{35, 85, 760, 810});
}

/** The dais, its curtain and lectern, and the two boards: the programme board hangs on the east wall, the notice board on the west. */
bool ChStageAndBoards(Ld::Builder& b, const Li::Room& room)
{
    return Put(b, room, MakeItem("stage", "asset.lane.furniture.stage_dais", "wood_light", Li::FurnitureKind::Display, 0, 40),
               Li::Rect{950, 1650, 655, 875}, StandUse(0, -1, 120, 240, "stage_front", true, 1300)) &&
           Put(b, room, MakeItem("stage_curtain", "asset.lane.furniture.curtain_stage", "cloth_magenta", Li::FurnitureKind::Display, 40, 235, false),
               Li::Rect{1000, 1600, 860, 875}) &&
           Put(b, room, MakeItem("lectern", "asset.lane.furniture.lectern", "door_wood", Li::FurnitureKind::Display, 40, 125, false),
               Li::Rect{1260, 1340, 700, 740}) &&
           Put(b, room, MakeItem("notice_board", "asset.lane.furniture.notice_board", "door_wood", Li::FurnitureKind::Display, 110, 200),
               Li::Rect{25, 37, 250, 430}, StandUse(1, 0, Li::StandZoneCm, 100, "notice_board", true, 340)) &&
           Put(b, room, MakeItem("programme_board", "asset.lane.furniture.notice_board", "door_wood", Li::FurnitureKind::Display, 110, 200),
               Li::Rect{2563, 2575, 250, 430}, StandUse(-1, 0, Li::StandZoneCm, 100, "programme_board", true, 340));
}
} // namespace

bool FurnishCommunityHall(Ld::Builder& b, const Li::Room& room)
{
    // Free aisles first, so no piece can be placed across them: the 120 centre aisle from the archway to the stage, and the cross
    // aisle in front of the stage (the stage's own use zone sits inside it).
    b.AddZone(Li::ZoneKind::Circulation, room.Id, Li::Rect{1240, 1360, 145, 655}, b.Key("opening", "entrance"), "centre_aisle");
    b.AddZone(Li::ZoneKind::Circulation, room.Id, Li::Rect{700, 1900, 520, 655}, b.Key("opening", "entrance"), "stage_cross");
    if (!ChStageAndBoards(b, room) || !ChBenches(b, room) || !ChCarrom(b, room) || !ChCommittee(b, room))
    {
        return false;
    }
    const std::vector<Li::RoutePoint> toStage{{1300, -60}, {1300, 595}};
    const std::vector<Li::RoutePoint> toBoard{{1300, -60}, {1300, 85}, {77, 85}, {77, 340}};
    const std::vector<Li::RoutePoint> toCupboard{{1300, -60}, {1300, 85}, {2495, 85}, {2495, 790}};
    if (!b.AddRoute("door_to_stage_front", "threshold", "stage_front", toStage) || !b.AddRoute("door_to_notice_board", "threshold", "notice_board", toBoard) ||
        !b.AddRoute("door_to_cupboard", "threshold", "cupboard", toCupboard))
    {
        return false;
    }
    for (int row = 0; row < 2; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            static constexpr int lightU[3] = {600, 1300, 2000};
            static constexpr int lightV[2] = {300, 650};
            AddLight(b, room, "hall." + std::to_string(row * 3 + column), lightU[column], lightV[row], 300, 800);
        }
    }
    return true;
}
} // namespace PerfectWorld::LaneEnds::Furnish
