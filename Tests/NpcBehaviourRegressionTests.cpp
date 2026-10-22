#include "NpcBehaviour.h"

#include "NpcBehaviourR5Support.h"
#include "ShopsR7.h"

#include <algorithm>
#include <array>
#include <filesystem>

using namespace PerfectWorld;

namespace
{
// Frozen pre-R5-04 role contract. Independent of NpcRoles()/FindNpcRole(), including action and availability.
constexpr std::array<NpcRoleDef, 16> R5BBaselineRoles{{
    {"at_home", NpcState::Idle, "", PersonAvailability::Awake},
    {"break", NpcState::Idle, "", PersonAvailability::Awake},
    {"caring", NpcState::Work, "work.care", PersonAvailability::Awake},
    {"cooking", NpcState::Work, "work.cook", PersonAvailability::Busy},
    {"inspecting", NpcState::Work, "work.inspect", PersonAvailability::Busy},
    {"observing", NpcState::Idle, "", PersonAvailability::Awake},
    {"private", NpcState::Sleep, "", PersonAvailability::Asleep},
    {"reading", NpcState::Idle, "", PersonAvailability::Awake},
    {"repairing", NpcState::Work, "work.repair", PersonAvailability::Busy},
    {"resting", NpcState::Idle, "", PersonAvailability::Awake},
    {"serving", NpcState::Work, "work.serve", PersonAvailability::Awake},
    {"sleeping", NpcState::Sleep, "", PersonAvailability::Asleep},
    {"stocking", NpcState::Work, "work.stock", PersonAvailability::Busy},
    {"studying", NpcState::Work, "work.study", PersonAvailability::Awake},
    {"sweeping", NpcState::Work, "work.sweep", PersonAvailability::Busy},
    {"teaching", NpcState::Work, "work.teach", PersonAvailability::Busy},
}};

NpcIntentResult R5BBaseline(const ScheduleResolution& resolution, bool chat, bool blocked)
{
    NpcIntent intent;
    intent.AnchorId = resolution.Zone;
    intent.ProgressPermille = resolution.ProgressPermille;
    intent.SourceId = resolution.SourceId;
    if (resolution.Status == ScheduleStatus::Stay)
    {
        if (resolution.Kind == ScheduleDoingKind::Activity)
        {
            intent.State = NpcState::Work;
            intent.WorkActionId = "work.activity";
            intent.Availability = PersonAvailability::Busy;
        }
        else
        {
            const auto role = std::find_if(R5BBaselineRoles.begin(), R5BBaselineRoles.end(),
                [&](const auto& entry) { return entry.Role == resolution.Doing; });
            R5BRequire(role != R5BBaselineRoles.end(), "baseline recognises authored role");
            intent.State = role->State;
            intent.WorkActionId = role->WorkActionId;
            intent.Availability = role->WorkAvailability;
        }
        if (intent.State == NpcState::Sleep)
        {
            intent.bTalkable = false;
            intent.DeclineReasonKey = "npc.reason.asleep";
        }
        else if (intent.Availability == PersonAvailability::Busy) { intent.DeclineReasonKey = "npc.reason.working"; }
    }
    else if (resolution.Status == ScheduleStatus::Travel)
    {
        intent.State = NpcState::Travel;
        intent.RouteId = "route:" + resolution.FromZone + "->" + resolution.Zone;
        intent.Availability = PersonAvailability::Busy;
        intent.DeclineReasonKey = "npc.reason.walking";
    }
    intent.ResumeState = intent.State;
    intent.ResumeWorkActionId = intent.WorkActionId;
    if (chat)
    {
        if (intent.Availability == PersonAvailability::Asleep) { return {NpcIntentError::ConversationUnavailable, {}}; }
        intent.State = NpcState::Chat;
        intent.WorkActionId = {};
    }
    else if (blocked && intent.State != NpcState::Sleep)
    {
        intent.State = NpcState::Yield;
        intent.WorkActionId = {};
        intent.Availability = PersonAvailability::Busy;
        intent.DeclineReasonKey = "npc.reason.blocked";
    }
    return {NpcIntentError::None, intent};
}

void R5BPersonRegression(const NpcSchedule& schedule)
{
    const auto from = R5BAt(0), to = R5BAt(0, 7);
    const bool pilot = schedule.Person.PersonId == "person.debashis_pal" || schedule.Person.PersonId == "person.mahua_saha" ||
        schedule.Person.PersonId == "person.satyajit_dutta" || IsShopStaffR7(schedule.Person.PersonId);
    for (auto at = from; at < to; ++at)
    {
        const auto resolution = ResolveSchedule(schedule.Person, schedule.Travel, at);
        R5BRequire(resolution.Valid, "every minute of the authored week resolves");
        for (const bool chat : {false, true})
        {
            for (const bool blocked : {false, true})
            {
                const auto expected = R5BBaseline(resolution, chat, blocked);
                const auto actual = ResolveNpcIntent(resolution, chat, blocked);
                R5BRequire(expected.Error == actual.Error && expected.Intent == actual.Intent, "legacy output unchanged for every person and flag pair");
                // Exercise the engine's convenience entry point at boundaries and periodic samples.
                if (at == resolution.SegmentStart || at % 61 == 0)
                {
                    const auto convenience = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, chat, blocked);
                    R5BRequire(convenience.Error == expected.Error && convenience.Intent == expected.Intent, "legacy time entry point unchanged");
                }
            }
        }
        if (!pilot && (at == resolution.SegmentStart || at % 61 == 0))
        {
            const auto decision = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, at);
            R5BRequire(decision.Ok() && decision.Intent == R5BBaseline(resolution, false, false).Intent &&
                decision.Work.Phase == NpcWorkPhase::None && decision.Needs == NpcScheduleNeeds{}, "opt-in query leaves every nonpilot unchanged");
            if (decision.Intent.State != NpcState::Sleep)
            {
                for (const auto state : {NpcState::Chat, NpcState::Yield})
                {
                    const std::array window{NpcInterruptionWindow{state, at, at + 1}};
                    const auto interrupted = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, at, window);
                    R5BRequire(interrupted.Ok() && interrupted.Intent == R5BBaseline(resolution, state == NpcState::Chat,
                        state == NpcState::Yield).Intent, "nonpilot interruptions remain immediate and unchanged");
                }
            }
        }
    }
}
}

int main(int argc, char** argv)
{
    R5BRequire(argc == 2, "supply npc_schedule directory");
    std::error_code error;
    std::vector<std::filesystem::path> paths;
    for (std::filesystem::directory_iterator it(argv[1], error), end; !error && it != end; it.increment(error))
    {
        if (it->path().extension() == ".json") { paths.push_back(it->path()); }
    }
    R5BRequire(!error && paths.size() >= 13, "all thirteen baseline people supplied");
    std::sort(paths.begin(), paths.end());
    for (const auto& path : paths) { R5BPersonRegression(R5BLoadSchedule(path.string())); }
    std::cout << "NpcBehaviourRegressionTests passed (" << paths.size() << " people, every minute of seven days)\n";
}
