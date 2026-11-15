#include "WorldReducer.h"

#include "LearningCredits.h"
#include "LegacyRecovery.h"
#include "SchoolLessonContent.h"
#include "SchoolWorkIssuance.h"

#include <utility>

// P3-14: the two compatibility commands through WorldSim. awakening.recover repairs a legacy or unrouted life once, from
// what the slot really holds (LegacyRecovery.h); school.work.reconcile issues the homework briefs whose boundary passed
// while no brief catalogue was bound, once each by stable id, with a disclosed recovery window (SchoolWorkIssuance.h).
// Both are System-only, move no time, refuse when there is nothing to do (so repetition and reload are inert) and stage
// every change on copies; Submit's snapshot restores a late refusal.
namespace PerfectWorld
{
namespace
{
Rejection P314Refusal(std::string code, std::string message)
{
    return Rejection{std::move(code), std::move(message)};
}

void P314SetFlag(WorldState& state, std::string_view id, CommandResult& result)
{
    state.Flags[std::string(id)] = true;
    result.Events.push_back({0, state.Clock.Now(), WorldFlagChanged{std::string(id), true}});
}
} // namespace

bool IsLegacyRecoveryCommand(const CommandPayload& payload) noexcept
{
    return std::holds_alternative<AwakeningRecover>(payload) || std::holds_alternative<SchoolWorkReconcile>(payload);
}

std::optional<Rejection> WorldSim::ReconcileSchoolWork(SchoolWorkIssuanceResult& out) const
{
    if (Lessons_.WorkBriefs == nullptr || School_.Calendar == nullptr || School_.School == nullptr || Lessons_.Papers == nullptr)
        return P314Refusal("school.work.issue.missing_content", "Homework needs the briefs, calendar, bell schedule and papers bound.");
    out = ReconcileMissedSchoolWork(State_.SchoolWork, *Lessons_.WorkBriefs, *School_.Calendar, *School_.School, *Lessons_.Papers,
                                    State_.Clock.Now());
    if (!out.Ok())
        return P314Refusal(out.Issue.Code, out.Issue.Detail.empty() ? std::string("The homework could not be reconciled.") : out.Issue.Detail);
    return std::nullopt;
}

std::vector<std::string> WorldSim::PendingSchoolWorkReconciliation() const
{
    SchoolWorkIssuanceResult out;
    if (ReconcileSchoolWork(out))
        return {};
    return out.IssuedIds;
}

std::optional<Rejection> WorldSim::ValidateRecoveryCommand(const Command& command) const
{
    if (command.Source != CommandSource::System)
        return P314Refusal("recovery.source", "Only the loading system runs compatibility recovery.");
    if (!IsWorldDayInStep(State_))
        return P314Refusal("recovery.invalid_state", "The world and day must be valid and share one clock.");
    if (!IsWorldQuiescent(State_))
        return P314Refusal("world.unsettled", "Events are still due; settle them first.");
    if (std::holds_alternative<SchoolWorkReconcile>(command.Payload))
    {
        SchoolWorkIssuanceResult out;
        if (std::optional<Rejection> refusal = ReconcileSchoolWork(out))
            return refusal;
        if (out.IssuedIds.empty())
            return P314Refusal("school.work.reconcile.nothing_missing", "Every homework brief already reached this life.");
        return std::nullopt;
    }
    const auto& recover = std::get<AwakeningRecover>(command.Payload);
    const AwakeningRecoveryPlan plan = PlanAwakeningRecovery(State_);
    if (!plan.Needed())
        return P314Refusal("awakening.recovery_not_needed", "This life's Awakening needs no compatibility recovery.");
    if (plan.Seed)
    {
        if (Awakening_.Content == nullptr || (!State_.SchoolSessions.Periods.empty() && Lessons_.Lessons == nullptr))
            return P314Refusal("awakening.recovery_missing_content", "Recovery needs the validated content and the lesson catalogue.");
        CharacterCreator creator;
        if (!creator.RestoreSave(recover.Creation).Succeeded() || !creator.HasStarted())
            return P314Refusal("awakening.recovery_bad_creation", "The slot's creation record does not validate.");
        // The one creation fact the world itself authenticates: commerce was funded from its Household Resources rank.
        if (creator.GetTraitRank(HouseholdResourcesTraitId) != State_.Commerce.Funding.HouseholdResourcesRank)
            return P314Refusal("awakening.recovery_wrong_creation", "The creation record does not belong to this world.");
    }
    if (plan.OpenCountdown && Awakening_.Countdown.Player.empty())
        return P314Refusal("awakening.recovery_unbound", "The countdown context must be bound to open the missing countdown.");
    return std::nullopt;
}

std::optional<Rejection> WorldSim::ApplyRecoveryCommand(const Command& command, CommandResult& result)
{
    if (std::holds_alternative<SchoolWorkReconcile>(command.Payload))
    {
        SchoolWorkIssuanceResult out;
        if (std::optional<Rejection> refusal = ReconcileSchoolWork(out))
            return refusal;
        State_.SchoolWork = std::move(out.State);
        return std::nullopt;
    }
    const auto& recover = std::get<AwakeningRecover>(command.Payload);
    const AwakeningRecoveryPlan plan = PlanAwakeningRecovery(State_);
    const GameInstant now = State_.Clock.Now();
    AwakeningState awakening = State_.Awakening;
    TutorialDungeonState tutorial = State_.Tutorial;
    if (plan.Seed)
    {
        // Exactly the creation-confirmation seed (P3-06's SeedLearningInputs), never a second formula.
        AwakeningInputsSeedResult seeded = SeedLearningInputs(*Awakening_.Content, recover.Creation, State_.WorldSeed);
        if (!seeded.Ok())
            return P314Refusal(seeded.Error, "The Awakening inputs could not be seeded from the creation record.");
        awakening = std::move(seeded.State);
        // Lessons the school slice recorded as resolved were observed: the close's own evidence rule, recorded once now.
        for (const SchoolPeriodRecord& record : State_.SchoolSessions.Periods)
        {
            const SchoolLessonBinding* binding = FindSchoolLessonBinding(*Lessons_.Lessons, record.BindingId);
            const std::optional<AptitudeEvidence> item =
                binding != nullptr ? SchoolLessonObservedEvidence(record, *binding, now) : std::nullopt;
            if (!item)
                continue;
            const AptitudeAddResult added = AptitudeAddEvidence(awakening.ProjectionEvidence, *item);
            if (!added.Ok())
                return P314Refusal(std::string(AptitudeErrorCode(added.Error)), "A recorded lesson could not be read as evidence.");
        }
    }
    if (plan.OpenMakeUp)
    {
        const TutorialDungeonResult absent = MarkTutorialAbsence(tutorial, awakening, now);
        if (!absent.Ok())
            return P314Refusal(absent.Error, "The supervised make-up could not be opened.");
    }
    State_.Awakening = std::move(awakening);
    State_.Tutorial = std::move(tutorial);
    if (plan.Seed)
        P314SetFlag(State_, AwakeningSeededOnRecoveryFlag, result);
    if (plan.MarkPostDeadline)
        P314SetFlag(State_, AwakeningPostDeadlineFlag, result);
    if (plan.MarkCountdownUnavailable)
        P314SetFlag(State_, AwakeningCountdownUnavailableFlag, result);
    if (plan.OpenCountdown)
    {
        // The R9-05 late bootstrap, at the current instant: commitments already begun are omitted, the onset still ahead.
        std::vector<DomainEvent> opened;
        CountdownOpenResult open = OpenCountdownAtCurrentTime(State_, Awakening_.Countdown, School_, &opened);
        if (!open.Ok())
            return Rejection{std::string(CountdownErrorCode(open.Issue.Error)),
                             open.Issue.Detail.empty() ? std::string("The countdown could not be opened.") : open.Issue.Detail};
        std::string text = SerialiseCountdown(open.State);
        if (text.empty() || !ParseCountdown(text))
            return P314Refusal("countdown.invalid_world", "The opened countdown cannot be stored.");
        State_ = std::move(open.World);
        State_.CountdownText = std::move(text);
        result.Events.insert(result.Events.end(), opened.begin(), opened.end());
    }
    return std::nullopt;
}
} // namespace PerfectWorld
