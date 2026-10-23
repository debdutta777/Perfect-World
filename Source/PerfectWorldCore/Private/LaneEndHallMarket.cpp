#include "LaneEndHallDetail.h"

#include <string>

namespace PerfectWorld::LaneEnds::Furnish
{
namespace
{
// Four bays of three stall rows. Every dimension is a multiple of 5 so the aisles (exactly 120 wide) sit on the validator's grid.
//   V 25..145 entry aisle | 145..225 row 1 | 225..345 aisle | 345..425 row 2 | 425..545 aisle | 545..625 row 3 | 625..795 back aisle |
//   795..875 wall counters.
// Cross aisles (120 wide) run front to back between the bays: U 585..705, 1240..1360, 1895..2015.
constexpr int MkBayCount = 4;
constexpr int MkTableLength = 420;
constexpr int MkTableDepth = 80;
constexpr int MkTableU0[MkBayCount] = {105, 770, 1410, 2075};
constexpr int MkRowV0[3] = {145, 345, 545};
constexpr int MkCounterV0 = 795;
constexpr int MkCounterV1 = 875;
constexpr int MkCounterHalf = 150;
constexpr int MkAisleV[3][2] = {{225, 345}, {425, 545}, {625, 795}}; // the aisle after each row; the last is the back aisle

const char* MkGoods(int index)
{
    static constexpr const char* names[4] = {"goods_green", "goods_orange", "goods_red", "goods_yellow"};
    return names[index % 4];
}

const char* MkCloth(int index)
{
    static constexpr const char* names[3] = {"cloth_magenta", "cloth_mustard", "cloth_teal"};
    return names[index % 3];
}

std::string MkName(const char* stem, int bay, int row = -1, int extra = -1)
{
    std::string name = std::string(stem) + "_b" + std::to_string(bay);
    if (row >= 0)
    {
        name += "_r" + std::to_string(row + 1);
    }
    if (extra >= 0)
    {
        name += "_" + std::to_string(extra);
    }
    return name;
}

/** One stall: the produce table (a Stand anchor in the aisle on its low-V side), three heaps on it and the awning over it. */
bool MkStall(Ld::Builder& b, const Li::Room& room, int bay, int row)
{
    const int u0 = MkTableU0[bay];
    const int v0 = MkRowV0[row];
    const Li::Rect table{u0, u0 + MkTableLength, v0, v0 + MkTableDepth};
    const std::string anchor = row == 0 ? MkName("browse", bay) : MkName("browse", bay, row);
    if (!Put(b, room, MakeItem(MkName("produce_table", bay, row), "asset.lane.furniture.display_table", "wood_light", Li::FurnitureKind::Table, 0, 75),
             table, StandUse(0, -1, Li::StandZoneCm, 120, anchor)))
    {
        return false;
    }
    for (int heap = 0; heap < 3; ++heap)
    {
        const int hu = u0 + 30 + heap * 125;
        if (!Put(b, room, MakeItem(MkName("goods", bay, row, heap), "asset.lane.furniture.goods_heap", MkGoods(bay * 3 + row + heap), Li::FurnitureKind::Display, 75, 110, false),
                 Li::Rect{hu, hu + 110, v0 + 10, v0 + 70}))
        {
            return false;
        }
    }
    // The awning is cloth well above head height (it blocks nothing): it only reads as a covered stall from the aisle.
    return Put(b, room, MakeItem(MkName("awning", bay, row), "asset.lane.furniture.awning_cloth", MkCloth(bay + row), Li::FurnitureKind::Display, 205, 215, false),
               Li::Rect{u0 - 20, u0 + MkTableLength + 20, v0 - 25, v0 + MkTableDepth + 25});
}

/** The counter against the back wall for one bay, with the customer's place in front of it. */
bool MkCounter(Ld::Builder& b, const Li::Room& room, int bay)
{
    const int centre = MkTableU0[bay] + MkTableLength / 2;
    return Put(b, room, MakeItem(MkName("counter", bay), "asset.lane.furniture.counter_shop", "wood_light", Li::FurnitureKind::Counter, 0, 95),
               Li::Rect{centre - MkCounterHalf, centre + MkCounterHalf, MkCounterV0, MkCounterV1}, StandUse(0, -1, Li::StandZoneCm, 120, MkName("counter", bay)));
}
} // namespace

bool FurnishMarketHall(Ld::Builder& b, const Li::Room& room)
{
    const std::string entrance = b.Key("opening", "entrance");
    // Free aisles first: three cross aisles, the aisle after every row, and the back aisle. The entry aisle is the shell's.
    for (int row = 0; row < 3; ++row)
    {
        b.AddZone(Li::ZoneKind::Circulation, room.Id, Li::Rect{25, 2575, MkAisleV[row][0], MkAisleV[row][1]}, entrance, "aisle_" + std::to_string(row + 1));
    }
    b.AddZone(Li::ZoneKind::Circulation, room.Id, Li::Rect{585, 705, 25, MkCounterV0}, entrance, "cross_1");
    b.AddZone(Li::ZoneKind::Circulation, room.Id, Li::Rect{1240, 1360, 25, MkCounterV0}, entrance, "cross_2");
    b.AddZone(Li::ZoneKind::Circulation, room.Id, Li::Rect{1895, 2015, 25, MkCounterV0}, entrance, "cross_3");
    for (int bay = 0; bay < MkBayCount; ++bay)
    {
        for (int row = 0; row < 3; ++row)
        {
            if (!MkStall(b, room, bay, row))
            {
                return false;
            }
        }
        if (!MkCounter(b, room, bay))
        {
            return false;
        }
    }
    const std::vector<Li::RoutePoint> toCounter{{1300, -60}, {1300, 85}, {1300, 755}, {MkTableU0[1] + MkTableLength / 2, 755}};
    const std::vector<Li::RoutePoint> toBrowse{{1300, -60}, {1300, 85}, {MkTableU0[3] + MkTableLength / 2, 85}, {MkTableU0[3] + MkTableLength / 2, 105}};
    if (!b.AddRoute("door_to_counter_b1", "threshold", "counter_b1", toCounter) || !b.AddRoute("door_to_browse_b3", "threshold", "browse_b3", toBrowse))
    {
        return false;
    }
    for (int row = 0; row < 2; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            static constexpr int lightU[3] = {645, 1300, 1955};
            static constexpr int lightV[2] = {285, 485};
            AddLight(b, room, "market." + std::to_string(row * 3 + column), lightU[column], lightV[row], 300, 800);
        }
    }
    return true;
}
} // namespace PerfectWorld::LaneEnds::Furnish
