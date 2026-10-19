#include "LaneAmbient.h"

namespace PerfectWorld
{
namespace
{
constexpr AmbientPoint AmAmbientNorth{3100, 600, 15};
constexpr AmbientPoint AmAmbientFoot{3100, 340, 15};
constexpr AmbientPoint AmAmbientRekhaPause{3100, 480, 15};
constexpr AmbientPoint AmAmbientRoad{3100, 80, 0};
constexpr AmbientPoint AmAmbientSouthRoad{4600, 80, 0};
constexpr AmbientPoint AmAmbientSouth{4600, -600, 15};
constexpr AmbientPoint AmAmbientEast{2000, 80, 0};
constexpr AmbientPoint AmAmbientMiddle{800, 80, 0};
constexpr AmbientPoint AmAmbientWest{-400, 80, 0};
constexpr std::array<AmbientRoute, 7> AmAmbientRoutes{{
    {"person.purnima_das", "route.purnima.home_east", "location.ambient.purnima.home", "anchor.ambient.purnima.east",
     {AmAmbientNorth, AmAmbientFoot, AmAmbientRoad, AmAmbientEast}, 4, 1680},
    {"person.purnima_das", "route.purnima.east_middle", "anchor.ambient.purnima.east", "anchor.ambient.purnima.middle",
     {AmAmbientEast, AmAmbientMiddle}, 2, 1260},
    {"person.purnima_das", "route.purnima.middle_west", "anchor.ambient.purnima.middle", "anchor.ambient.purnima.west",
     {AmAmbientMiddle, AmAmbientWest}, 2, 1260},
    {"person.arun_paul", "route.arun.home_pause", "location.ambient.arun.home", "anchor.ambient.arun.pause",
     {AmAmbientNorth, AmAmbientFoot}, 2, 280},
    {"person.arun_paul", "route.arun.pause_out", "anchor.ambient.arun.pause", "location.ambient.arun.out",
     {AmAmbientFoot, AmAmbientRoad, AmAmbientSouthRoad, AmAmbientSouth}, 4, 2520},
    {"person.rekha_bose", "route.rekha.home_pause", "location.ambient.rekha.home", "anchor.ambient.rekha.pause",
     {AmAmbientSouth, AmAmbientSouthRoad, AmAmbientRoad, AmAmbientFoot, AmAmbientRekhaPause}, 5, 2660},
    {"person.rekha_bose", "route.rekha.pause_out", "anchor.ambient.rekha.pause", "location.ambient.rekha.out",
     {AmAmbientRekhaPause, AmAmbientNorth}, 2, 140},
}};
}
std::span<const AmbientRoute> LaneAmbientRoutes() noexcept { return AmAmbientRoutes; }
} // namespace PerfectWorld
