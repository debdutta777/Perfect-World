#pragma once

#include "LaneEndLayouts.h"
#include "LaneInteriorDetail.h"

#include <string>
#include <utility>

namespace PerfectWorld::LaneEnds::Furnish
{
namespace Li = PerfectWorld::LaneInterior;
namespace Ld = PerfectWorld::LaneInterior::Detail;

/** A piece's physical description; sizes come from the footprint given to Put. Heights are above the finished floor. */
inline Ld::Item MakeItem(std::string name, std::string asset, std::string palette, Li::FurnitureKind kind, int h0, int h1, bool solid = true)
{
    Ld::Item item;
    item.Name = std::move(name);
    item.Asset = std::move(asset);
    item.Palette = std::move(palette);
    item.Kind = kind;
    item.H0 = h0;
    item.H1 = h1;
    item.Solid = solid;
    return item;
}

/** A standing place on one side of a piece: free square, a Standing zone and a Stand anchor. Side is the side the user approaches from. */
inline Ld::Use StandUse(int sideU, int sideV, int depth, int lateral, std::string anchor, bool hasCentre = false, int centre = 0)
{
    Ld::Use use;
    use.SideU = sideU;
    use.SideV = sideV;
    use.Depth = depth;
    use.Zone = Li::ZoneKind::Standing;
    use.Anchor = true;
    use.Kind = Li::AnchorKind::Stand;
    use.AnchorName = std::move(anchor);
    use.Lateral = lateral;
    use.HasCentre = hasCentre;
    use.Centre = centre;
    return use;
}

/** A seat: the user approaches from the open side (SideU / SideV), a SeatApproach zone is kept free, and a Seat anchor is made. */
inline Ld::Use SeatUse(int sideU, int sideV, int lateral, std::string anchor)
{
    Ld::Use use;
    use.SideU = sideU;
    use.SideV = sideV;
    use.Depth = Li::SeatApproachCm;
    use.Zone = Li::ZoneKind::SeatApproach;
    use.Anchor = true;
    use.Kind = Li::AnchorKind::Seat;
    use.AnchorName = std::move(anchor);
    use.Lateral = lateral;
    return use;
}

inline bool Put(Ld::Builder& b, const Li::Room& room, const Ld::Item& item, const Li::Rect& footprint, const Ld::Use& use = Ld::Use{})
{
    return b.TryPlace(room, item, footprint, use);
}

/** A ceiling fitting's light for the room, in the same form Builder::AddLights produces (height 17 below the ceiling). */
inline void AddLight(Ld::Builder& b, const Li::Room& room, const std::string& name, int u, int v, int radius, int lumens)
{
    Li::LightSpec light;
    light.Id = b.Key("light", name);
    light.RoomId = room.Id;
    light.U = u;
    light.V = v;
    light.HeightCm = b.L.ClearHeightCm - 17;
    light.RadiusCm = radius;
    light.Lumens = lumens;
    b.L.Lights.push_back(std::move(light));
}

/** One window span along a wall run, in U. */
struct WindowSpan
{
    int U0 = 0;
    int U1 = 0;
};

/** Furnishes the west mass's hall: stage, benches, carrom corner, notice boards, committee table. False when a piece does not fit. */
[[nodiscard]] bool FurnishCommunityHall(Ld::Builder& b, const Li::Room& room);
/** Furnishes the east mass's hall: three rows of produce stalls in four bays, wall counters, aisles. False when a piece does not fit. */
[[nodiscard]] bool FurnishMarketHall(Ld::Builder& b, const Li::Room& room);

/** The front and back windows each hall wants (the shell cuts them), in U. */
[[nodiscard]] std::pair<std::vector<WindowSpan>, std::vector<WindowSpan>> HallWindows(EndHall hall);
} // namespace PerfectWorld::LaneEnds::Furnish
