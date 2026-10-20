#include "NpcBehaviour.h"

#include <array>

namespace PerfectWorld
{
namespace
{
using R5BAvail = PersonAvailability;

// Ascending by Role. Idle and Sleep roles carry no work action.
constexpr std::array<NpcRoleDef, 16> R5BRoleTable{{
    {"at_home", NpcState::Idle, "", R5BAvail::Awake},
    {"break", NpcState::Idle, "", R5BAvail::Awake},
    {"caring", NpcState::Work, "work.care", R5BAvail::Awake},
    {"cooking", NpcState::Work, "work.cook", R5BAvail::Busy},
    {"inspecting", NpcState::Work, "work.inspect", R5BAvail::Busy},
    {"observing", NpcState::Idle, "", R5BAvail::Awake},
    {"private", NpcState::Sleep, "", R5BAvail::Asleep},
    {"reading", NpcState::Idle, "", R5BAvail::Awake},
    {"repairing", NpcState::Work, "work.repair", R5BAvail::Busy},
    {"resting", NpcState::Idle, "", R5BAvail::Awake},
    {"serving", NpcState::Work, "work.serve", R5BAvail::Awake},
    {"sleeping", NpcState::Sleep, "", R5BAvail::Asleep},
    {"stocking", NpcState::Work, "work.stock", R5BAvail::Busy},
    {"studying", NpcState::Work, "work.study", R5BAvail::Awake},
    {"sweeping", NpcState::Work, "work.sweep", R5BAvail::Busy},
    {"teaching", NpcState::Work, "work.teach", R5BAvail::Busy},
}};

constexpr std::string_view R5BStateIds[NpcStateCount] = {"work", "travel", "idle", "chat", "sleep", "yield"};

NpcIntentResult R5BRefuse(NpcIntentError error) noexcept
{
    NpcIntentResult result;
    result.Error = error;
    return result;
}

/** What a schedule item's Doing means for a person standing (or about to stand) in it. */
struct R5BClassified
{
    NpcState State = NpcState::Idle;
    std::string_view WorkActionId;
    R5BAvail Availability = R5BAvail::Awake;
};

/** Error is None on success. */
NpcIntentError R5BClassify(ScheduleDoingKind kind, const std::string& doing, R5BClassified& out) noexcept
{
    if (doing.empty())
    {
        return NpcIntentError::InvalidResolution;
    }
    if (kind == ScheduleDoingKind::Activity)
    {
        out = {NpcState::Work, NpcActivityWorkActionId, R5BAvail::Busy};
        return NpcIntentError::None;
    }
    const NpcRoleDef* role = FindNpcRole(doing);
    if (role == nullptr)
    {
        return NpcIntentError::UnknownRole;
    }
    out.State = role->State;
    out.WorkActionId = role->WorkActionId;
    out.Availability = role->State == NpcState::Work ? role->WorkAvailability : (role->State == NpcState::Sleep ? R5BAvail::Asleep : R5BAvail::Awake);
    return NpcIntentError::None;
}

/** Fills availability-derived fields and the resume fields of an unsuspended intent. */
void R5BSetAvailability(NpcIntent& intent, R5BAvail availability, std::string_view reasonKey, bool talkable) noexcept
{
    intent.Availability = availability;
    intent.DeclineReasonKey = reasonKey;
    intent.bTalkable = talkable;
}
} // namespace

std::string_view NpcStateId(NpcState state) noexcept
{
    const auto index = static_cast<std::size_t>(state);
    return index < NpcStateCount ? R5BStateIds[index] : std::string_view{};
}

std::string_view NpcIntentErrorCode(NpcIntentError error) noexcept
{
    switch (error)
    {
    case NpcIntentError::None: return "none";
    case NpcIntentError::InvalidResolution: return "invalid_resolution";
    case NpcIntentError::UnknownRole: return "unknown_role";
    case NpcIntentError::ConversationUnavailable: return "conversation_unavailable";
    case NpcIntentError::InvalidInterruption: return "invalid_interruption";
    }
    return "unknown";
}

std::span<const NpcRoleDef> NpcRoles() noexcept
{
    return R5BRoleTable;
}

const NpcRoleDef* FindNpcRole(std::string_view role) noexcept
{
    for (const NpcRoleDef& def : R5BRoleTable)
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
    for (std::size_t i = 0; i < R5BRoleTable.size(); ++i)
    {
        const NpcRoleDef& def = R5BRoleTable[i];
        if (def.Role.empty() || (i > 0 && !(R5BRoleTable[i - 1].Role < def.Role)))
        {
            return false;
        }
        switch (def.State)
        {
        case NpcState::Work:
            if (def.WorkActionId.substr(0, 5) != "work." || def.WorkActionId.size() <= 5 || def.WorkAvailability == R5BAvail::Asleep)
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
        return R5BRefuse(NpcIntentError::InvalidResolution);
    }

    NpcIntent intent;
    intent.AnchorId = resolution.Zone;
    intent.ProgressPermille = resolution.ProgressPermille;
    intent.SourceId = resolution.SourceId;

    switch (resolution.Status)
    {
    case ScheduleStatus::Stay:
    {
        R5BClassified role;
        if (const NpcIntentError error = R5BClassify(resolution.Kind, resolution.Doing, role); error != NpcIntentError::None)
        {
            return R5BRefuse(error);
        }
        intent.State = role.State;
        intent.WorkActionId = role.WorkActionId;
        if (role.State == NpcState::Sleep)
        {
            R5BSetAvailability(intent, R5BAvail::Asleep, NpcReasonAsleep, false);
        }
        else if (role.State == NpcState::Work && role.Availability == R5BAvail::Busy)
        {
            R5BSetAvailability(intent, R5BAvail::Busy, NpcReasonWorking, true);
        }
        else
        {
            R5BSetAvailability(intent, R5BAvail::Awake, {}, true);
        }
        break;
    }
    case ScheduleStatus::Travel:
    {
        R5BClassified destination;
        if (resolution.FromZone.empty() || resolution.FromZone == resolution.Zone)
        {
            return R5BRefuse(NpcIntentError::InvalidResolution);
        }
        if (const NpcIntentError error = R5BClassify(resolution.Kind, resolution.Doing, destination); error != NpcIntentError::None)
        {
            return R5BRefuse(error);
        }
        intent.State = NpcState::Travel;
        intent.RouteId = "route:" + resolution.FromZone + "->" + resolution.Zone;
        R5BSetAvailability(intent, R5BAvail::Busy, NpcReasonWalking, true);
        break;
    }
    case ScheduleStatus::Idle:
        intent.State = NpcState::Idle;
        R5BSetAvailability(intent, R5BAvail::Awake, {}, true);
        break;
    default:
        return R5BRefuse(NpcIntentError::InvalidResolution);
    }

    intent.ResumeState = intent.State;
    intent.ResumeWorkActionId = intent.WorkActionId;

    if (conversationOpen)
    {
        if (intent.Availability == R5BAvail::Asleep)
        {
            return R5BRefuse(NpcIntentError::ConversationUnavailable);
        }
        intent.State = NpcState::Chat;
        intent.WorkActionId = {};
        intent.bTalkable = true;
    }
    else if (blocked && intent.State != NpcState::Sleep)
    {
        intent.State = NpcState::Yield;
        intent.WorkActionId = {};
        R5BSetAvailability(intent, R5BAvail::Busy, NpcReasonBlocked, true);
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
    R5BClassified scratch;
    for (std::size_t i = 0; i < person.Routine.size(); ++i)
    {
        const ScheduleEntry& entry = person.Routine[i];
        if (R5BClassify(entry.Kind, entry.Doing, scratch) != NpcIntentError::None)
        {
            return "routine entry " + std::to_string(i) + " has an unknown or empty role '" + entry.Doing + "'";
        }
    }
    for (const ScheduleException& exception : person.Exceptions)
    {
        if (R5BClassify(exception.Kind, exception.Doing, scratch) != NpcIntentError::None)
        {
            return "exception " + exception.Id + " has an unknown or empty role '" + exception.Doing + "'";
        }
    }
    return {};
}
} // namespace PerfectWorld
