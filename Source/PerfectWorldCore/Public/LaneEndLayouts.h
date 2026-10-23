#pragma once

#include "LaneInteriorLayout.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld::LaneEnds
{
/**
 * R6-05: the two masses that close the lane (west_end_block, east_end_block) opened as public halls. The west mass is a furnished
 * community hall, the east mass a covered market hall. Portable and deterministic: each hall is a LaneInterior::InteriorLayout in
 * the same building frame and units as the kit's frontages (U along the lane-facing wall, V from that wall into the building, heights
 * above the 15 cm finished floor), so the engine's interior-kit presenter (BuildInteriorKit) builds it unchanged.
 *
 * Frames. Both masses are 2600 wide (along Y) and 900 deep (along X). U runs +Y from Y = -1300 for both. The lane-facing wall is
 * the wall at X = -2400 (west, V runs toward -X) and X = 6400 (east, V runs toward +X). Neither frame is mirrored through a flag:
 * HallWorldPoint gives the world point.
 *
 * Entrance. One public archway in the lane-facing wall, centred on the lane axis, inside the 800 cm of that wall (|Y| < 400) that the
 * first-row buildings leave exposed. It is a DOORLESS opening: no leaf, no sweep, no door id, no door-roster entry and no save change.
 * A roller shutter is drawn raised above it. The hours below are POSTED, and reported by IsOpenAt and MinutesUntilOpening for the map
 * and the sign; nothing here refuses a visitor outside them (that needs a real door and a roster edition, which this order avoids).
 *
 * Contracts, checked by ValidateEndLayout on top of LaneInterior::ValidateInteriorLayout: exactly one public archway, on the
 * lane-facing wall, at least 240 wide and 212 high, hanging no door and creating no door sweep; the full-width aisle behind it is a
 * 120 cm circulation zone; every stand and seat anchor can be reached on foot (34 cm capsule) from the archway; and every circulation
 * zone can be reached by a 60 cm radius body, which proves every aisle is a true 120 cm clear channel.
 */
inline constexpr int HallWidthCm = 2600;
inline constexpr int HallDepthCm = 900;
inline constexpr int OriginYCm = -1300;
inline constexpr int WestLaneFaceXCm = -2400;
inline constexpr int EastLaneFaceXCm = 6400;
inline constexpr int FinishedFloorCm = 15;
inline constexpr int StoreyHeightCm = 300;
inline constexpr int ExposedFrontHalfWidthCm = 400;  // the first rows leave |Y| < 400 of the lane-facing wall visible
inline constexpr int MinArchwayWidthCm = 240;
inline constexpr int AisleRadiusCm = 60;             // a 120 cm aisle admits a body of this radius
inline constexpr int CapsuleRadiusCm = 34;

enum class EndHall : std::uint8_t { CommunityHall, MarketHall };
inline constexpr std::size_t EndHallCount = 2;

struct HoursWindow
{
    int OpenMinute = 0;   // inclusive, minute of day
    int CloseMinute = 0;  // exclusive
};

struct HallInfo
{
    EndHall Hall = EndHall::CommunityHall;
    std::string_view BuildingId;   // "building.nabapara.west_end_block"
    std::string_view Label;        // "Community hall"
    std::string_view LabelKey;     // "map.building.community_hall"
    std::string_view EntranceId;   // the layout's opening id (stable); not a door id
    bool East = false;
    int Storeys = 0;
    int LaneFaceXCm = 0;           // world X of the lane-facing wall
    int FaceXSign = 1;             // direction of V in world X: +1 east hall, -1 west hall
    int EntranceCentreU = 0;
    int EntranceWidthCm = 0;
    int EntranceHeightCm = 0;
    std::array<HoursWindow, 2> Hours{};
    int HoursCount = 0;
};

struct WorldPoint
{
    int X = 0;
    int Y = 0;
    friend bool operator==(const WorldPoint&, const WorldPoint&) = default;
};

[[nodiscard]] PERFECTWORLDCORE_API const HallInfo& GetHallInfo(EndHall hall) noexcept;
/** The hall of this building id ("building.nabapara.west_end_block" / "...east_end_block"), or nullptr. */
[[nodiscard]] PERFECTWORLDCORE_API const HallInfo* FindHallInfo(std::string_view buildingId) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::span<const HoursWindow> HallHours(EndHall hall) noexcept;

/** Layout (U, V) to world centimetres. The lane-facing wall is at V 0. */
[[nodiscard]] PERFECTWORLDCORE_API WorldPoint HallWorldPoint(EndHall hall, int u, int v) noexcept;
/** The archway's centre on the lane-facing wall, in world centimetres. */
[[nodiscard]] PERFECTWORLDCORE_API WorldPoint HallEntrancePoint(EndHall hall) noexcept;

/** "08:00-21:00" or "06:00-13:00 and 16:00-21:00": the text posted at the archway. */
[[nodiscard]] PERFECTWORLDCORE_API std::string PostedHoursText(EndHall hall);
/** True inside a posted window. `minuteOfDay` is 0..1439; anything else is read as midnight. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsOpenAt(EndHall hall, int minuteOfDay) noexcept;
/** Minutes until the next opening (wrapping to tomorrow, always at least 1), or -1 when open now. */
[[nodiscard]] PERFECTWORLDCORE_API int MinutesUntilOpening(EndHall hall, int minuteOfDay) noexcept;

/** The validated layout of a hall. Status is never None for an invalid result; the layout is empty on refusal. */
[[nodiscard]] PERFECTWORLDCORE_API LaneInterior::InteriorLayout GenerateEndLayout(EndHall hall);
/** The generic interior contracts plus the end-hall contracts above. Works on any layout; never repairs. */
[[nodiscard]] PERFECTWORLDCORE_API LaneInterior::Refusal ValidateEndLayout(EndHall hall, const LaneInterior::InteriorLayout& layout);

/** The archway of a hall layout (the opening from "outside" that is not a window), or nullptr. */
[[nodiscard]] PERFECTWORLDCORE_API const LaneInterior::Opening* FindEndEntrance(const LaneInterior::InteriorLayout& layout);
} // namespace PerfectWorld::LaneEnds
