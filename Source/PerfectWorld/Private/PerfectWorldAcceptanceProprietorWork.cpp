#include "PerfectWorldAcceptanceProprietorWork.h"

#include "Engine/World.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

FStepDef R55Action(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run, double Timeout = 25.0)
{
    FStepDef Step;
    Step.Name = Name;
    Step.Run = MoveTemp(Run);
    Step.TimeoutSeconds = Timeout;
    return Step;
}
}

FStepDef R55SafeTalkStep(const TCHAR* Name, const FName Person)
{
    return R55Action(Name, [Person](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
        auto* Interaction = Sub.GetInteraction();
        auto* Actor = Presence != nullptr ? Presence->FindPerson(Person) : nullptr;
        const auto* Decision = Presence != nullptr ? Presence->FindPilotDecision(Person) : nullptr;
        if (Life == nullptr || Talk == nullptr || Interaction == nullptr || Actor == nullptr || Decision == nullptr)
            return FStepOutcome::Fail(TEXT("pilot Talk dependencies unavailable"));
        if (Rt.Phase == 0)
        {
            Sub.FaceWorldPoint(Actor->GetInteractionPoint());
            if (Interaction->GetTarget() != Actor || !Interaction->IsPromptVisible()) { return FStepOutcome::Wait(); }
            // Request inside a step, rather than at an already-safe boundary. All timing comes from the decision.
            if (Decision->Intent.State != PerfectWorld::NpcState::Work || Decision->Work.Phase == PerfectWorld::NpcWorkPhase::None ||
                Decision->Work.SafeAt - Life->GetNow() >= PerfectWorld::NpcWorkStepMinutes || Decision->Work.BlockEnd <= Life->GetNow() + 4)
                return FStepOutcome::Wait();
            Rt.Target.X = static_cast<double>(Life->GetNow());
            Rt.LastSize = Decision->Work.CompletedSteps;
            if (!Interaction->TryInteract()) { return FStepOutcome::Fail(TEXT("the real Talk interaction was refused")); }
            Decision = Presence->FindPilotDecision(Person);
            if (Talk->IsOpen() || Decision == nullptr || !Decision->DeferredUntil ||
                Decision->Intent.State != PerfectWorld::NpcState::Work || !Interaction->IsNoticeVisible() ||
                Interaction->GetNotice().IsEmpty() || Interaction->GetNotice().ToString().Contains(TEXT("npc.reason.")))
                return FStepOutcome::Fail(TEXT("Talk bypassed the safe boundary or gave no readable waiting reason"));
            Rt.Target.Y = static_cast<double>(*Decision->DeferredUntil);
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Life->GetNow() < static_cast<int64>(Rt.Target.Y))
        {
            return Talk->IsOpen() ? FStepOutcome::Fail(TEXT("panel opened before core acceptance")) : FStepOutcome::Wait();
        }
        if (!Talk->IsOpen())
        {
            return Life->GetNow() - static_cast<int64>(Rt.Target.X) > 3 ?
                FStepOutcome::Fail(TEXT("Talk exceeded three game minutes")) : FStepOutcome::Wait();
        }
        if (PerfectWorld::OpenConversationPersonId(Life->GetWorldState()) != TCHAR_TO_UTF8(*Person.ToString()) ||
            Decision->Intent.State != PerfectWorld::NpcState::Chat || Decision->AcceptedAt != static_cast<int64>(Rt.Target.Y) ||
            Decision->Work.CompletedSteps != Rt.LastSize + 1 || Life->GetNow() - static_cast<int64>(Rt.Target.X) > 3)
            return FStepOutcome::Fail(TEXT("the panel did not follow the accepted safe pause and exactly one finished step"));
        return FStepOutcome::Pass(TEXT("real Talk deferred with readable notice; clock ran to safe pause within three minutes before panel opened"));
    });
}

FStepDef R55LeaveResumeStep(const TCHAR* Name, const FName Person)
{
    return R55Action(Name, [Person](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
        const auto* Decision = Presence != nullptr ? Presence->FindPilotDecision(Person) : nullptr;
        if (Life == nullptr || Talk == nullptr || Decision == nullptr) { return FStepOutcome::Fail(TEXT("no pilot conversation owner")); }
        if (Rt.Phase == 0)
        {
            if (!Talk->IsOpen() || Decision->Intent.State != PerfectWorld::NpcState::Chat)
                return FStepOutcome::Fail(TEXT("no accepted pilot Chat to leave"));
            Rt.Target.X = static_cast<double>(Decision->Work.BlockStart);
            Rt.LastSize = Decision->Work.CompletedSteps;
            Talk->LeaveConversation();
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Talk->IsOpen() || !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty()) { return FStepOutcome::Wait(); }
        if (Decision->Intent.State == PerfectWorld::NpcState::Chat || Decision->DeferredUntil)
            return FStepOutcome::Fail(TEXT("Leave retained a Chat request"));
        if (Decision->Work.BlockStart == static_cast<int64>(Rt.Target.X) && Decision->Work.CompletedSteps < Rt.LastSize)
            return FStepOutcome::Fail(TEXT("release restarted already completed work"));
        return FStepOutcome::Pass(TEXT("Leave released the window; unfinished work or the current schedule resumed"));
    });
}

FStepDef R55PilotTimeStep(const TCHAR* Name, const int32 Hour, const int32 Minute)
{
    return R55Action(Name, [Hour, Minute](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr) { return FStepOutcome::Fail(TEXT("no live core clock")); }
        if (Life->HasMovingDoors() || !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty()) { return FStepOutcome::Wait(); }
        const auto Target = PerfectWorld::GameClock::FromCalendar(PerfectWorld::CampaignStartYear,
            PerfectWorld::CampaignStartMonth, PerfectWorld::CampaignStartDay, Hour, Minute);
        if (Life->GetNow() >= Target) { return FStepOutcome::Fail(TEXT("the declared clock target has already passed")); }
        auto Sim = MakeUnique<PerfectWorld::WorldSim>(Life->GetWorldState());
        for (int32 Round = 0; Round < 64; ++Round)
        {
            if (!PerfectWorld::IsWorldQuiescent(Sim->State()))
            {
                if (!Sim->Submit({PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}}).Accepted)
                    return FStepOutcome::Fail(TEXT("core settlement refused"));
            }
            else if (Sim->State().Clock.Now() < Target)
            {
                if (!Sim->Submit({PerfectWorld::CommandSource::System, PerfectWorld::AdvanceTime{Target - Sim->State().Clock.Now()}}).Accepted)
                    return FStepOutcome::Fail(TEXT("core time advance refused"));
            }
            else { break; }
        }
        if (Sim->State().Clock.Now() != Target || !PerfectWorld::IsWorldQuiescent(Sim->State()))
            return FStepOutcome::Fail(TEXT("core clock failed to reach the declared target"));
        Life->AdoptSavedWorld(MoveTemp(Sim));
        return FStepOutcome::Pass(TEXT("advanced through core commands; began fresh transient presentation"));
    });
}

FStepDef R55DecisionStep(const TCHAR* Name, const FName Person, const PerfectWorld::NpcDecisionReason Reason)
{
    return R55Action(Name, [Person, Reason](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        const auto* Decision = Presence != nullptr ? Presence->FindPilotDecision(Person) : nullptr;
        if (Decision == nullptr || Decision->Reason != Reason) { return FStepOutcome::Fail(TEXT("pilot decision differs from the scheduled break/closing reason")); }
        return FStepOutcome::Pass(TEXT("presence exposes the core's scheduled priority and reason"));
    });
}

FStepDef R55ObstructionStep(const TCHAR* Name, const FName Person)
{
    return R55Action(Name, [Person](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        const auto* Decision = Presence != nullptr ? Presence->FindPilotDecision(Person) : nullptr;
        const auto* Actor = Presence != nullptr ? Presence->FindPerson(Person) : nullptr;
        if (Decision == nullptr || Actor == nullptr || Sub.GetPawn() == nullptr) { return FStepOutcome::Wait(); }
        const double Apart = FVector::Dist2D(Actor->GetActorLocation(), Sub.GetPawn()->GetActorLocation());
        if (Apart < 68.0) { return FStepOutcome::Fail(TEXT("worker overlaps the player's capsule")); }
        if (Decision->Intent.State != PerfectWorld::NpcState::Yield || !Actor->GetNpcMovement()->IsYielding())
        {
            Rt.Phase = 0;
            return FStepOutcome::Wait();
        }
        if (Rt.Phase == 0)
        {
            Rt.LastPos = Sub.GetPawn()->GetActorLocation();
            Rt.NextPhase();
        }
        if (FVector::Dist2D(Rt.LastPos, Sub.GetPawn()->GetActorLocation()) > 2.0)
            return FStepOutcome::Fail(TEXT("the worker pushed the stationary player"));
        return Rt.InPhase() >= 3.0 ? FStepOutcome::Pass(TEXT("core Yield and physical collision hold; no overlap or player displacement for three seconds")) : FStepOutcome::Wait();
    }, 120.0);
}
