#include "WorldState.h"

// The rule that ties the player's day to the world clock (ROADMAP M1-03, M1-05, M2-12).
namespace PerfectWorld
{
bool IsWorldDayInStep(const WorldState& state)
{
    return state.DayLoop.Now == state.Clock.Now() && IsDayLoopStateValid(state.DayLoop);
}
} // namespace PerfectWorld
