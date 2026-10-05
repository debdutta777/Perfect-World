#include "GateSites.h"

#include <type_traits>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::string_view GateSitesIdPrefix = "gate.";
constexpr std::string_view GateSitesEventPrefix = "gates/";
constexpr std::string_view GateSitesOnsetPrefix = "gates/onset/";
constexpr std::size_t GateSitesMaxIdLength = 100;

bool GateSitesIsIdChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

/** "gate." plus at least one more stable-ID character, not ending in '.'. Safe inside a clock event ID. */
bool GateSitesIsGateId(std::string_view id)
{
    if (id.size() <= GateSitesIdPrefix.size() || id.size() > GateSitesMaxIdLength || id.substr(0, GateSitesIdPrefix.size()) != GateSitesIdPrefix
        || id.back() == '.')
        return false;
    for (const char c : id)
    {
        if (!GateSitesIsIdChar(c))
            return false;
    }
    return true;
}

bool GateSitesRankInRange(GateRank rank)
{
    return static_cast<std::uint8_t>(rank) <= static_cast<std::uint8_t>(GateRank::S);
}

bool GateSitesStatusInRange(GateStatus status)
{
    return static_cast<std::uint8_t>(status) <= static_cast<std::uint8_t>(GateStatus::Sealed);
}

Rejection GateSitesReject(std::string code, std::string message)
{
    return Rejection{std::move(code), std::move(message)};
}

const GateSiteState* GateSitesFind(const WorldState& state, const std::string& id)
{
    const auto found = state.Gates.find(id);
    return found == state.Gates.end() ? nullptr : &found->second;
}

const std::string& GateSitesCommandId(const CommandPayload& payload)
{
    static const std::string none;
    if (const auto* reg = std::get_if<GateRegister>(&payload))
        return reg->Id;
    if (const auto* onset = std::get_if<GateScheduleOnset>(&payload))
        return onset->Id;
    if (const auto* contain = std::get_if<GateContain>(&payload))
        return contain->Id;
    if (const auto* seal = std::get_if<GateSeal>(&payload))
        return seal->Id;
    return none;
}
} // namespace

std::string_view GateRankName(GateRank rank) noexcept
{
    switch (rank)
    {
    case GateRank::F: return "F";
    case GateRank::E: return "E";
    case GateRank::D: return "D";
    case GateRank::C: return "C";
    case GateRank::B: return "B";
    case GateRank::A: return "A";
    case GateRank::S: return "S";
    }
    return "?";
}

std::string_view GateStatusName(GateStatus status) noexcept
{
    switch (status)
    {
    case GateStatus::Contained: return "contained";
    case GateStatus::Active: return "active";
    case GateStatus::Sealed: return "sealed";
    }
    return "?";
}

std::optional<GateRank> ParseGateRank(std::string_view text)
{
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(GateRank::S); ++i)
    {
        if (GateRankName(static_cast<GateRank>(i)) == text)
            return static_cast<GateRank>(i);
    }
    return std::nullopt;
}

std::optional<GateStatus> ParseGateStatus(std::string_view text)
{
    if (text == "open")
        return GateStatus::Active;
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(GateStatus::Sealed); ++i)
    {
        if (GateStatusName(static_cast<GateStatus>(i)) == text)
            return static_cast<GateStatus>(i);
    }
    return std::nullopt;
}

std::string GateOnsetEventId(std::string_view gateId)
{
    std::string id(GateSitesOnsetPrefix);
    id.append(gateId);
    return id;
}

bool IsGateCommand(const CommandPayload& payload)
{
    return std::holds_alternative<GateRegister>(payload) || std::holds_alternative<GateScheduleOnset>(payload)
        || std::holds_alternative<GateContain>(payload) || std::holds_alternative<GateSeal>(payload);
}

std::optional<Rejection> ValidateGateCommand(const WorldState& state, const Command& command)
{
    if (!IsGateCommand(command.Payload))
        return GateSitesReject("command.unknown", "Unknown command.");
    if (command.Source != CommandSource::System && command.Source != CommandSource::Debug)
        return GateSitesReject("gate.source", "Only systems change Gate sites.");
    const std::string& id = GateSitesCommandId(command.Payload);
    if (!GateSitesIsGateId(id))
        return GateSitesReject("gate.bad_id", "Gate IDs start with \"gate.\" and use lowercase letters, digits, '_', '-' and '.'.");
    const GateSiteState* gate = GateSitesFind(state, id);

    if (const auto* reg = std::get_if<GateRegister>(&command.Payload))
    {
        if (!GateSitesRankInRange(reg->Rank))
            return GateSitesReject("gate.bad_rank", "A Gate's rank is F, E, D, C, B, A or S.");
        if (!GateSitesStatusInRange(reg->Status))
            return GateSitesReject("gate.bad_status", "A Gate's status is contained, active or sealed.");
        if (gate != nullptr)
            return GateSitesReject("gate.duplicate", "That Gate is already registered.");
        return std::nullopt;
    }
    if (gate == nullptr)
        return GateSitesReject("gate.unknown", "No Gate site has that ID.");
    if (const auto* onset = std::get_if<GateScheduleOnset>(&command.Payload))
    {
        if (gate->Status == GateStatus::Sealed)
            return GateSitesReject("gate.sealed", "A sealed Gate has no onset.");
        if (onset->When < state.Clock.Now())
            return GateSitesReject("gate.in_past", "An onset cannot be scheduled in the past.");
        return std::nullopt;
    }
    if (std::holds_alternative<GateContain>(command.Payload) && gate->Status != GateStatus::Active)
        return GateSitesReject("gate.not_active", "Only an active Gate can be contained.");
    if (std::holds_alternative<GateSeal>(command.Payload) && gate->Status != GateStatus::Contained)
        return GateSitesReject("gate.not_contained", "Only a contained Gate can be sealed.");
    return std::nullopt;
}

void ApplyGateCommand(WorldState& state, const Command& command, std::vector<DomainEvent>& events)
{
    const GameInstant now = state.Clock.Now();
    if (const auto* reg = std::get_if<GateRegister>(&command.Payload))
    {
        state.Gates[reg->Id] = GateSiteState{reg->Rank, reg->Status, now, 0};
        events.push_back({0, now, GateRegistered{reg->Id, reg->Rank, reg->Status}});
    }
    else if (const auto* onset = std::get_if<GateScheduleOnset>(&command.Payload))
    {
        (void)state.Clock.Schedule(onset->When, GateOnsetEventId(onset->Id));
    }
    else if (const auto* contain = std::get_if<GateContain>(&command.Payload))
    {
        GateSiteState& gate = state.Gates[contain->Id];
        gate.Status = GateStatus::Contained;
        gate.StatusSince = now;
        events.push_back({0, now, GateContained{contain->Id}});
    }
    else if (const auto* seal = std::get_if<GateSeal>(&command.Payload))
    {
        GateSiteState& gate = state.Gates[seal->Id];
        gate.Status = GateStatus::Sealed;
        gate.StatusSince = now;
        events.push_back({0, now, GateSealed{seal->Id}});
    }
}

bool IsGateEventId(std::string_view eventId)
{
    return eventId.substr(0, GateSitesEventPrefix.size()) == GateSitesEventPrefix;
}

void RouteGateEvent(WorldState& state, const ScheduledEvent& fired, std::vector<DomainEvent>& events)
{
    const std::string_view eventId = fired.Id;
    if (eventId.substr(0, GateSitesOnsetPrefix.size()) != GateSitesOnsetPrefix)
        return;
    const auto found = state.Gates.find(eventId.substr(GateSitesOnsetPrefix.size()));
    if (found == state.Gates.end())
        return;
    GateSiteState& gate = found->second;
    if (gate.Status == GateStatus::Sealed)
    {
        events.push_back({0, fired.When, GateOnsetHeld{found->first}});
        return;
    }
    const GateStatus previous = gate.Status;
    if (previous != GateStatus::Active)
    {
        gate.Status = GateStatus::Active;
        gate.StatusSince = fired.When;
    }
    gate.OnsetCount += 1;
    events.push_back({0, fired.When, GateOnset{found->first, gate.Rank, previous, gate.OnsetCount}});
}

std::vector<Command> GateRegistrationCommands(const ContentDb& db)
{
    std::vector<Command> commands;
    for (const ContentRecord* record : db.OfType("gate_site"))
    {
        const JsonValue* rank = record->Data.Find("rank");
        const JsonValue* status = record->Data.Find("status");
        if (rank == nullptr || status == nullptr || !rank->IsString() || !status->IsString())
            continue;
        const std::optional<GateRank> parsedRank = ParseGateRank(rank->String);
        const std::optional<GateStatus> parsedStatus = ParseGateStatus(status->String);
        if (parsedRank && parsedStatus)
            commands.push_back({CommandSource::System, GateRegister{record->Id, *parsedRank, *parsedStatus}});
    }
    return commands;
}

std::size_t RegisterContentGates(WorldSim& sim, const ContentDb& db)
{
    std::size_t accepted = 0;
    for (const Command& command : GateRegistrationCommands(db))
    {
        if (sim.Submit(command).Accepted)
            ++accepted;
    }
    return accepted;
}
} // namespace PerfectWorld
