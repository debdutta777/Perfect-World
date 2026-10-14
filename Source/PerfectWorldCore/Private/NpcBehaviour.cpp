#include "NpcBehaviour.h"

#include <array>

namespace PerfectWorld
{
namespace
{
using Avail = PersonAvailability;

// Ascending by Role. Idle and Sleep roles carry no work action.
constexpr std::array<NpcRoleDef, 16> RoleTable{{
    {"at_home", NpcState::Idle, "", Avail::Awake},
    {"break", NpcState::Idle, "", Avail::Awake},
    {"caring", NpcState::Work, "work.care", Avail::Awake},
    {"cooking", NpcState::Work, "work.cook", Avail::Busy},
    {"inspecting", NpcState::Work, "work.inspect", Avail::Busy},
    {"observing", NpcState::Idle, "", Avail::Awake},
    {"private", NpcState::Sleep, "", Avail::Asleep},
    {"reading", NpcState::Idle, "", Avail::Awake},
    {"repairing", NpcState::Work, "work.repair", Avail::Busy},
    {"resting", NpcState::Idle, "", Avail::Awake},
    {"serving", NpcState::Work, "work.serve", Avail::Awake},
    {"sleeping", NpcState::Sleep, "", Avail::Asleep},
    {"stocking", NpcState::Work, "work.stock", Avail::Busy},
    {"studying", NpcState::Work, "work.study", Avail::Awake},
    {"sweeping", NpcState::Work, "work.sweep", Avail::Busy},
    {"teaching", NpcState::Work, "work.teach", Avail::Busy},
}};

constexpr std::string_view StateIds[NpcStateCount] = {"work", "travel", "idle", "chat", "sleep", "yield"};

NpcIntentResult Refuse(NpcIntentError error) noexcept
{
    NpcIntentResult result;
    result.Error = error;
    return result;
}

/** What a schedule item's Doing means for a person standing (or about to stand) in it. */
struct Classified
{
    NpcState State = NpcState::Idle;
    std::string_view WorkActionId;
    Avail Availability = Avail::Awake;
};

/** Error is None on success. */
NpcIntentError Classify(ScheduleDoingKind kind, const std::string& doing, Classified& out) noexcept
{
    if (doing.empty())
    {
        return NpcIntentError::InvalidResolution;
    }
    if (kind == ScheduleDoingKind::Activity)
    {
        out = {NpcState::Work, NpcActivityWorkActionId, Avail::Busy};
        return NpcIntentError::None;
    }
    const NpcRoleDef* role = FindNpcRole(doing);
    if (role == nullptr)
    {
        return NpcIntentError::UnknownRole;
    }
    out.State = role->State;
    out.WorkActionId = role->WorkActionId;
    out.Availability = role->State == NpcState::Work ? role->WorkAvailability : (role->State == NpcState::Sleep ? Avail::Asleep : Avail::Awake);
    return NpcIntentError::None;
}

/** Fills availability-derived fields and the resume fields of an unsuspended intent. */
void SetAvailability(NpcIntent& intent, Avail availability, std::string_view reasonKey, bool talkable) noexcept
{
    intent.Availability = availability;
    intent.DeclineReasonKey = reasonKey;
    intent.bTalkable = talkable;
}
} // namespace

std::string_view NpcStateId(NpcState state) noexcept
{
    const auto index = static_cast<std::size_t>(state);
    return index < NpcStateCount ? StateIds[index] : std::string_view{};
}

std::string_view NpcIntentErrorCode(NpcIntentError error) noexcept
{
    switch (error)
    {
    case NpcIntentError::None: return "none";
    case NpcIntentError::InvalidResolution: return "invalid_resolution";
    case NpcIntentError::UnknownRole: return "unknown_role";
    case NpcIntentError::ConversationUnavailable: return "conversation_unavailable";
    }
    return "unknown";
}

std::span<const NpcRoleDef> NpcRoles() noexcept
{
    return RoleTable;
}

const NpcRoleDef* FindNpcRole(std::string_view role) noexcept
{
    for (const NpcRoleDef& def : RoleTable)
    {
        if (def.Role == role)
        {
            return &def;
        }
    }
    return nullptr;
}

bool ValidateNpcRoleTable() noexcept
{
    for (std::size_t i = 0; i < RoleTable.size(); ++i)
    {
        const NpcRoleDef& def = RoleTable[i];
        if (def.Role.empty() || (i > 0 && !(RoleTable[i - 1].Role < def.Role)))
        {
            return false;
        }
        switch (def.State)
        {
        case NpcState::Work:
            if (def.WorkActionId.substr(0, 5) != "work." || def.WorkActionId.size() <= 5 || def.WorkAvailability == Avail::Asleep)
            {
                return false;
            }
            break;
        case NpcState::Idle:
        case NpcState::Sleep:
            if (!def.WorkActionId.empty())
            {
                return false;
            }
            break;
        default:
            return false;
        }
    }
    return true;
}

NpcIntentResult ResolveNpcIntent(const ScheduleResolution& resolution, bool conversationOpen, bool blocked) noexcept
{
    if (!resolution.Valid || resolution.Zone.empty() || resolution.ProgressPermille < 0 || resolution.ProgressPermille > 1000)
    {
        return Refuse(NpcIntentError::InvalidResolution);
    }

    NpcIntent intent;
    intent.AnchorId = resolution.Zone;
    intent.ProgressPermille = resolution.ProgressPermille;
    intent.SourceId = resolution.SourceId;

    switch (resolution.Status)
    {
    case ScheduleStatus::Stay:
    {
        Classified role;
        if (const NpcIntentError error = Classify(resolution.Kind, resolution.Doing, role); error != NpcIntentError::None)
        {
            return Refuse(error);
        }
        intent.State = role.State;
        intent.WorkActionId = role.WorkActionId;
        if (role.State == NpcState::Sleep)
        {
            SetAvailability(intent, Avail::Asleep, NpcReasonAsleep, false);
        }
        else if (role.State == NpcState::Work && role.Availability == Avail::Busy)
        {
            SetAvailability(intent, Avail::Busy, NpcReasonWorking, true);
        }
        else
        {
            SetAvailability(intent, Avail::Awake, {}, true);
        }
        break;
    }
    case ScheduleStatus::Travel:
    {
        Classified destination;
        if (resolution.FromZone.empty() || resolution.FromZone == resolution.Zone)
        {
            return Refuse(NpcIntentError::InvalidResolution);
        }
        if (const NpcIntentError error = Classify(resolution.Kind, resolution.Doing, destination); error != NpcIntentError::None)
        {
            return Refuse(error);
        }
        intent.State = NpcState::Travel;
        intent.RouteId = "route:" + resolution.FromZone + "->" + resolution.Zone;
        SetAvailability(intent, Avail::Busy, NpcReasonWalking, true);
        break;
    }
    case ScheduleStatus::Idle:
        intent.State = NpcState::Idle;
        SetAvailability(intent, Avail::Awake, {}, true);
        break;
    default:
        return Refuse(NpcIntentError::InvalidResolution);
    }

    intent.ResumeState = intent.State;
    intent.ResumeWorkActionId = intent.WorkActionId;

    if (conversationOpen)
    {
        if (intent.Availability == Avail::Asleep)
        {
            return Refuse(NpcIntentError::ConversationUnavailable);
        }
        intent.State = NpcState::Chat;
        intent.WorkActionId = {};
        intent.bTalkable = true;
    }
    else if (blocked && intent.State != NpcState::Sleep)
    {
        intent.State = NpcState::Yield;
        intent.WorkActionId = {};
        SetAvailability(intent, Avail::Busy, NpcReasonBlocked, true);
    }

    NpcIntentResult result;
    result.Intent = std::move(intent);
    return result;
}

NpcIntentResult ResolveNpcIntentAt(const PersonSchedule& person, const ScheduleTravelTable& travel, GameInstant at, bool conversationOpen,
                                   bool blocked)
{
    return ResolveNpcIntent(ResolveSchedule(person, travel, at), conversationOpen, blocked);
}

std::optional<GameInstant> NextNpcIntentBoundary(const PersonSchedule& person, const ScheduleTravelTable& travel, GameInstant after)
{
    return NextScheduleBoundary(person, travel, after);
}

std::string ValidateNpcSchedule(const PersonSchedule& person)
{
    Classified scratch;
    for (std::size_t i = 0; i < person.Routine.size(); ++i)
    {
        const ScheduleEntry& entry = person.Routine[i];
        if (Classify(entry.Kind, entry.Doing, scratch) != NpcIntentError::None)
        {
            return "routine entry " + std::to_string(i) + " has an unknown or empty role '" + entry.Doing + "'";
        }
    }
    for (const ScheduleException& exception : person.Exceptions)
    {
        if (Classify(exception.Kind, exception.Doing, scratch) != NpcIntentError::None)
        {
            return "exception " + exception.Id + " has an unknown or empty role '" + exception.Doing + "'";
        }
    }
    return {};
}
} // namespace PerfectWorld
