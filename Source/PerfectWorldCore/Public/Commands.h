#pragma once

#include "GameClock.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace PerfectWorld
{
/**
 * Commands: the only way anything asks the world to change (SYSTEMS.md §2).
 * Each command's TypeId is the stable text written to replay logs; it never
 * changes once shipped. Systems add their own command headers as they arrive.
 */
enum class CommandSource : std::uint8_t
{
    Player,
    Presentation,
    Performance,
    System,
    Migration,
    Debug
};

/** Move the one clock forward. The only way time passes. */
struct AdvanceTime
{
    static constexpr std::string_view TypeId = "time.advance";
    std::int64_t Minutes = 1;
};

/** Schedule a world event on the clock. IDs follow "system/kind/key". Systems and debug only. */
struct ScheduleWorldEvent
{
    static constexpr std::string_view TypeId = "world.schedule";
    GameInstant When = 0;
    std::string EventId;
};

/** Set a world flag by stable ID (lowercase letters, digits, '_' and '.'). */
struct SetWorldFlag
{
    static constexpr std::string_view TypeId = "world.flag";
    std::string FlagId;
    bool Value = true;
};

using CommandPayload = std::variant<AdvanceTime, ScheduleWorldEvent, SetWorldFlag>;

struct Command
{
    CommandSource Source = CommandSource::Player;
    CommandPayload Payload;
};

inline std::string_view CommandTypeId(const CommandPayload& payload)
{
    return std::visit([](const auto& command) { return std::decay_t<decltype(command)>::TypeId; }, payload);
}
} // namespace PerfectWorld
